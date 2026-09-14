#pragma once
#include "engine/tick/Tick.hpp"
#include <set>
#include "engine/event/Event.hpp"
#include "engine/resource/Resources.hpp"
#include "engine/data/VarianTmap.hpp"

namespace gui {
	namespace Events{
		struct UIEventBase : public EventBase {};
		struct UIobjectEvent : public UIEventBase {
			std::string path;
			std::string name;
			const std::string wholePath() const {
				return path + '_' + name;
			}
		};
		struct ButtonPressed : public UIobjectEvent {};
		struct OptionDeselected : public UIobjectEvent {};
		struct OptionSelected : public UIobjectEvent {};
		struct InputSelected : public UIobjectEvent {};
		struct InputDeselected : public UIobjectEvent {};
		//Forward Events
		struct ForwardEvent : public UIEventBase {
			bool isFocusUI;
		};
		struct KeyPressed : public ForwardEvent {
			std::string focusWindow;
			std::string focusAreaPath;
			sf::Keyboard::Key code;
		};
		struct KeyReleased : public ForwardEvent {
			std::string focusWindow;
			std::string focusAreaPath;
			sf::Keyboard::Key code;
		};
		struct MousePressed : public ForwardEvent {
			sf::Mouse::Button button;
			sf::Vector2i position;
		};
		struct MouseReleased : public ForwardEvent {
			sf::Mouse::Button button;
			sf::Vector2i position;
		};
		struct MouseMoved : public ForwardEvent {
			sf::Vector2i position;
		};
		struct MouseWheelScrolled : public ForwardEvent {
			sf::Mouse::Wheel wheel;
			float delta;
			sf::Vector2i position;
		};
	}
	//快速绘制简图
	namespace _builtinGUIdraw {
		inline void Rect(sf::RenderTarget& r, const sf::Vector2f& point1, const sf::Vector2f& point2, sf::Color fillcolor, sf::Color linecolor = sf::Color::Transparent, float thickness = 0) {
			sf::RectangleShape rect;
			rect.setOrigin((point2 - point1) / 2.0f);
			rect.setPosition((point1 + point2) / 2.f);
			rect.setSize(point2 - point1);
			rect.setFillColor(fillcolor);
			rect.setOutlineColor(linecolor);
			rect.setOutlineThickness(thickness);
			r.draw(rect);
		}
		inline void Line(sf::RenderTarget& r, const sf::Vector2f& point1, const sf::Vector2f& point2, sf::Color color, float thickness) {
			sf::RectangleShape rect;
			sf::CircleShape circle;
			float len = (point2 - point1).length();
			rect.setPosition((point1 + point2) / 2.f);
			rect.setOrigin({ len / 2.0f, thickness / 2.0f });
			rect.setSize({ len, thickness });
			rect.setFillColor(color);
			rect.setRotation((point2 - point1).angle());
			r.draw(rect);
			circle.setOrigin({ thickness / 2.0f, thickness / 2.0f });
			circle.setRadius(thickness / 2.0f);
			circle.setFillColor(color);
			circle.setPosition(point1);
			r.draw(circle);
			circle.setPosition(point2);
			r.draw(circle);
		}
	}
	class UIwindowManager;
	class Style {
	public:
		sf::Color backgroundColor = sf::Color(0, 0, 0, 0), outlineColor = sf::Color(0, 0, 0, 0);
		float outlineThickness = 1;
		void set(sf::Color _backgroundColor, sf::Color _outlineColor, float _outlineThickness) {
			backgroundColor = _backgroundColor;
			outlineColor = _outlineColor;
			outlineThickness = _outlineThickness;
		}
	};
	class AreaObject;
	class UIBase {
		friend class AreaObject;
		friend class UIwindowManager;
	public:
		enum class Anchor {
			Left = 0, Right = 2, Width = 3,
			Top = 0, Bottom = 2, Height = 3,
			Mid = 1, Size = 3
		};
		enum class Relative { LeftEdge = 0, RightEdge = 2, TopEdge = 0, BottomEdge = 2 ,MidLine = 1 };
		enum class Align { Left = 0, Right = 2, Top = 0, Bottom = 2, Mid = 1 };
		class DynamicPosition{
			friend class UIBase;
			float calcRelative(float fatherSize) const{
				return (type & 0b000011) * fatherSize / 2.0f + value;
			}
			char type = 0;
			float value = 0;
		public:
			DynamicPosition() = default;
			DynamicPosition(Anchor _anchor, float _value = 0){
				type = static_cast<int>(_anchor) << 2;
				value = _value;
			}
			DynamicPosition(Anchor _anchor, Relative _relative, float _value = 0){
				type = (static_cast<int>(_anchor) << 2) | static_cast<int>(_relative);
				value = _value;
			}
			bool isSize () const{
				return getAnchor() == static_cast<int>(Anchor::Size);
			}
			bool isNormal () const{
				return type == ((static_cast<int>(Anchor::Top) << 2) | static_cast<int>(Relative::TopEdge));
			}
			DynamicPosition& setRelative (Relative _relative){
				type = (type & 0b111100) | static_cast<int>(_relative);
				return *this;
			}
			DynamicPosition& setAnchor (Anchor _anchor){
				type = (type & 0b000011) | (static_cast<int>(_anchor) << 2);
				return *this;
			}
			DynamicPosition& setValue (float _value){
				value = _value;
				return *this;
			}
			int getRelative () const{
				return type & 0b000011;
			}
			int getAnchor () const{
				return type >> 2;
			}
			float getValue () const{
				return value;
			}
			char getType () const{
				return type;
			}
		};
	protected:
		sf::FloatRect posRect = sf::FloatRect(sf::Vector2f(), sf::Vector2f());
		sf::Vector2<std::pair<DynamicPosition, DynamicPosition>> relativePosition={
			{{UIBase::Anchor::Left, UIBase::Relative::LeftEdge, 0}, {UIBase::Anchor::Width, 0}},
			{{UIBase::Anchor::Top, UIBase::Relative::TopEdge, 0}, {UIBase::Anchor::Height, 0}}
		};
		Style styles[3];
		int currentStatu = gui::UIBase::Normal;
		bool isShow = true;
		virtual void draw(sf::RenderTarget& r, sf::FloatRect displayArea, UIwindowManager& windowManager);
		//std::set<std::string> linkList;
	public:
		enum Statu { Normal = 0, Over = 1, Focus = 2 };
		UIBase& setPosition(sf::Vector2f _position) {
			relativePosition.x.first = {UIBase::Anchor::Left, UIBase::Relative::LeftEdge, _position.x};
			relativePosition.y.first = {UIBase::Anchor::Top, UIBase::Relative::TopEdge, _position.y};
			return *this;
		}
		UIBase& setPosition(sf::Vector2f _position, sf::Vector2<Anchor> _anchor) {
			relativePosition.x.first = {_anchor.x, UIBase::Relative::LeftEdge, _position.x};
			relativePosition.y.first = {_anchor.y, UIBase::Relative::TopEdge, _position.y};
			return *this;
		}
		UIBase& setPosition(sf::Vector2f _position, sf::Vector2<Anchor> _anchor, sf::Vector2<Relative> _relative) {
			relativePosition.x.first = {_anchor.x, _relative.x, _position.x};
			relativePosition.y.first = {_anchor.y, _relative.y, _position.y};
			return *this;
		}
		UIBase& setSize(sf::Vector2f _size) {
			if (relativePosition.x.second.isSize()) relativePosition.x.second = {UIBase::Anchor::Size, _size.x};
			if (relativePosition.y.second.isSize()) relativePosition.y.second = {UIBase::Anchor::Size, _size.y};
			return *this;
		}
		UIBase& setPositionRelative(std::initializer_list<UIBase::DynamicPosition> xRelative, std::initializer_list<UIBase::DynamicPosition> yRelative) {
			if (xRelative.size() == 1){
				auto& first = *xRelative.begin();
				if (first.isSize()) goto setRelativeIllegal;
				relativePosition.x.first = first;
			}
			else{
				auto & first = *xRelative.begin(),&second = *std::next(xRelative.begin());
				if (first.getAnchor() == second.getAnchor()) goto setRelativeIllegal;
				if (first.getAnchor() < second.getAnchor()){
					relativePosition.x.first = first;relativePosition.x.second = second;
				}
				else{
					relativePosition.x.first = second;relativePosition.x.second = first;
				}
			}
			if (yRelative.size() == 1){
				auto& first = *yRelative.begin();
				if (first.isSize()) goto setRelativeIllegal;
				relativePosition.y.first = first;
			}
			else{
				auto & first = *yRelative.begin(),&second = *std::next(yRelative.begin());
				if (first.getAnchor() == second.getAnchor()) goto setRelativeIllegal;
				if (first.getAnchor() < second.getAnchor()){
					relativePosition.y.first = first;relativePosition.y.second = second;
				}
				else{
					relativePosition.y.first = second;relativePosition.y.second = first;
				}
			}
			return *this;
		setRelativeIllegal:;
			throw std::runtime_error("[UIBase::setRelative] Illegal relative position\n");
		}
		//static getter
		sf::Vector2<std::pair<DynamicPosition, DynamicPosition>> getDynamicPosition() const {
			return relativePosition;
		}
		sf::Vector2<std::optional<float>> getPosition() const {
			return {
				relativePosition.x.first.isNormal() ? std::optional<float>(relativePosition.x.first.value) : std::nullopt,
				relativePosition.y.first.isNormal() ? std::optional<float>(relativePosition.y.first.value) : std::nullopt
			};
		}
		sf::Vector2<std::optional<float>> getSize() const {
			return {
				relativePosition.x.second.isSize() ? std::optional<float>(relativePosition.x.second.value) : std::nullopt,
				relativePosition.y.second.isSize() ? std::optional<float>(relativePosition.y.second.value) : std::nullopt
			};
		}
		//dynamic getter
		sf::FloatRect getPosRect() const {
			return posRect;
		}
		UIBase& setStyle(const Style& _normalStyle, const Style& _overStyle, const Style& _focusStyle) {
			styles[gui::UIBase::Normal] = _normalStyle;
			styles[gui::UIBase::Over] = _overStyle;
			styles[gui::UIBase::Focus] = _focusStyle;
			return *this;
		}
		Style& style(int id) {
			return styles[id];
		}
		Style& getStyle(int id) {
			return styles[id];
		}
		const Style& getStyle(int id) const {
			return styles[id];
		}
		int getStatu() const {
			return currentStatu;
		}
		bool getShow() const {
			return isShow;
		}
		UIBase& setShow(bool _isShow){
			isShow = _isShow;
			return *this;
		}
		UIBase& toggleShow() {
			isShow = !isShow;
			return *this;
		}
	protected:
		void updatePosRect(sf::Vector2f fatherSize) {
			auto nonNegative = [](float value) {
				return value < 0.f ? 0.f : value;
			};
			if (relativePosition.x.second.isSize())
				posRect.size.x = nonNegative(relativePosition.x.second.value);
			else posRect.size.x = nonNegative((relativePosition.x.second.calcRelative(fatherSize.x)-relativePosition.x.first.calcRelative(fatherSize.x)) * 2 / (relativePosition.x.second.getAnchor() - relativePosition.x.first.getAnchor()));
			if (relativePosition.y.second.isSize())
				posRect.size.y = nonNegative(relativePosition.y.second.value);
			else posRect.size.y = nonNegative((relativePosition.y.second.calcRelative(fatherSize.y)-relativePosition.y.first.calcRelative(fatherSize.y)) * 2 / (relativePosition.y.second.getAnchor() - relativePosition.y.first.getAnchor()));
			posRect.position.x = relativePosition.x.first.calcRelative(fatherSize.x) - (relativePosition.x.first.getAnchor()) / 2.f * posRect.size.x;
			posRect.position.y = relativePosition.y.first.calcRelative(fatherSize.y) - (relativePosition.y.first.getAnchor()) / 2.f * posRect.size.y;
		}
		void setStatu(int statu, bool force = false) {
			if (force || currentStatu != gui::UIBase::Focus) {
				currentStatu = statu;
			}
		}
	public:
		virtual void onFocusLose(EventQueue&, const std::string&, const std::string&, AreaObject&, bool) { setStatu(Normal, true); }
		virtual void onFocusGain(EventQueue&, const std::string&, const std::string&, const sf::Vector2f&, AreaObject&, bool) { setStatu(Focus); }
		virtual void onRelease(bool, bool, EventQueue&, const std::string&, const std::string&, AreaObject&) {}
		virtual void onDragUpdate(bool, bool) {}
		virtual void onTick(UIwindowManager& wm);
		virtual void onTextEntered(char32_t) {}
		virtual void onKeyPressed(sf::Keyboard::Key) {}
		virtual bool isDragScrollImmediate() { return true; }
		virtual bool isTextEnterable() { return false; }
		virtual bool shouldForwardKey(sf::Keyboard::Key) { return false; }
		virtual void onInertialScrollStart(sf::Vector2f) {}
		virtual bool isInteractive() { return false; }
	};
	class ImageObject :public UIBase {
		friend class UIwindowManager;
	public:
		ImageObject() {}
		//use this setter
		//after : setImage() , setScale() , setScaleTo()
		ImageObject& setSizeAuto() {
			setSize(static_cast<sf::Vector2f>(imageManager[imageId].getSize()).componentWiseMul(scale));
			return *this;
		}
	protected:
		std::string imageId;
		sf::Vector2i align = { static_cast<int>(gui::UIBase::Align::Mid), static_cast<int>(gui::UIBase::Align::Mid) };
		sf::Vector2f scale = sf::Vector2f(1, 1);
		sf::Color imageColors[3] = { sf::Color::White,sf::Color::White ,sf::Color::White };
		void draw(sf::RenderTarget& r, sf::FloatRect displayArea, UIwindowManager& windowManager);
	public:
		ImageObject& setImageColor(const sf::Color& _normalColor, const sf::Color& _overColor, const sf::Color& _focusColor) {
			imageColors[gui::UIBase::Normal] = _normalColor;
			imageColors[gui::UIBase::Over] = _overColor;
			imageColors[gui::UIBase::Focus] = _focusColor;
			return *this;
		}
		sf::Color& imageColor(int id) {
			return imageColors[id];
		}
		ImageObject& setAlign(gui::UIBase::Align xAlign, gui::UIBase::Align yAlign) {
			align.x = static_cast<int>(xAlign);
			align.y = static_cast<int>(yAlign);
			return *this;
		}
		ImageObject& setScale(sf::Vector2f _scale) {
			scale = _scale;
			return *this;
		}
		//use this setter
		//after : setImage()
		//设置自动缩放，autoAxes的x/y为true时对应轴实时根据posRect自动缩放
		ImageObject& setScaleAuto(sf::Vector2i _autoScalable = { 1, 1 }) {
			if (_autoScalable.x)
				scale.x = -1.f;
			if (_autoScalable.y)
				scale.y = -1.f;
			return *this;
		}
		ImageObject& setImageId(const std::string& _imageId) {
			imageId = _imageId;
			return *this;
		}
		const std::string& getImageId() const { return imageId; }
		sf::Vector2f getScale() const { return scale; }
		sf::Vector2<Align> getAlign() const {
			return static_cast<sf::Vector2<Align>>(align);
		}
		const sf::Color& getImageColor(int id) const { return imageColors[id]; }
	};
	class TextStyle{
	public:
		sf::Color fillColor;
		sf::Color outlineColor;
		void set(sf::Color _fillColor, sf::Color _outlineColor) {
			fillColor = _fillColor;
			outlineColor = _outlineColor;
		}
	};
	class TextObject :public UIBase {
		friend class UIwindowManager;
	public:
		TextObject() {
			textStyles[gui::UIBase::Normal].set(sf::Color::Black, sf::Color::Black);
			textStyles[gui::UIBase::Over].set(sf::Color::Black, sf::Color::Black);
			textStyles[gui::UIBase::Focus].set(sf::Color::Black, sf::Color::Black);
		}
		TextObject& setTextStyle(const TextStyle& _normalStyle, const TextStyle& _overStyle, const TextStyle& _focusStyle) {
			textStyles[gui::UIBase::Normal] = _normalStyle;
			textStyles[gui::UIBase::Over] = _overStyle;
			textStyles[gui::UIBase::Focus] = _focusStyle;
			return *this;
		}
		TextStyle& textStyle(int id) {
			return textStyles[id];
		}
		TextStyle& getTextStyle(int id) {
			return textStyles[id];
		}
		const TextStyle& getTextStyle(int id) const {
			return textStyles[id];
		}
		//use this setter
		//after : setFont() , setText() , setCharacterSize()
		TextObject& setSizeAuto() {
			textRender.setCharacterSize(characterSize);
			textRender.setLineSpacing(lineSpacing);
			textRender.setLetterSpacing(letterSpacing);
			textRender.setString(text);
			textRender.setPosition({ 0, 0 });
			sf::Vector2f size;
			size.x = 0;
			for (int i = 0; i <= text.getSize(); i++) {
				if (textRender.findCharacterPos(i).x > size.x)
					size.x = textRender.findCharacterPos(i).x;
			}
			size.y = textRender.findCharacterPos(textRender.getString().getSize()).y + characterSize;
			setSize(size);
			return *this;
		}
	protected:
		std::string font = "";
		unsigned int characterSize = 30;
		float letterSpacing = 1, lineSpacing = 1;
		sf::String text = "";
		sf::Text textRender{ fontManager[font] };
		sf::FloatRect textRect;
		sf::Vector2f textRenderOffsetFix;
		sf::Vector2i align = { static_cast<int>(gui::UIBase::Align::Mid), static_cast<int>(gui::UIBase::Align::Mid) };
		TextStyle textStyles[3];
		void draw(sf::RenderTarget& r, sf::FloatRect displayArea, UIwindowManager& windowManager);
	public:
		TextObject& setFont(const std::string& _font) {
			font = _font;
			textRender.setFont(fontManager[font]);
			return *this;
		}
		TextObject& setCharacterSize(int _characterSize) {
			characterSize = _characterSize;
			return *this;
		}
		TextObject& setSpacing(float _letterSpacing, float _lineSpacing) {
			letterSpacing = _letterSpacing;
			lineSpacing = _lineSpacing;
			return *this;
		}
		TextObject& setAlign(gui::UIBase::Align xAlign, gui::UIBase::Align yAlign) {
			align.x = static_cast<int>(xAlign);
			align.y = static_cast<int>(yAlign);
			return *this;
		}
		TextObject& setText(sf::String _text) {
			text = _text;
			return *this;
		}
		sf::String& getText() {
			return text;
		}
		const sf::String& getText() const {
			return text;
		}
		const std::string& getFont() const { return font; }
		unsigned int getCharacterSize() const { return characterSize; }
		float getLetterSpacing() const { return letterSpacing; }
		float getLineSpacing() const { return lineSpacing; }
		sf::Vector2i getAlign() const { return align; }
	};
	class ButtonObject :public TextObject {
		friend class UIwindowManager;
	public:
		ButtonObject() {
			styles[gui::UIBase::Normal].set(sf::Color(250, 250, 250), sf::Color(200, 200, 200), 2);
			styles[gui::UIBase::Over].set(sf::Color(220, 220, 220), sf::Color(200, 200, 200), 2);
			styles[gui::UIBase::Focus].set(sf::Color(200, 200, 200), sf::Color(150, 150, 150), 2);
		}
		void onRelease(bool isOver, bool isDragScrolling, EventQueue& event, const std::string& path, const std::string& name, AreaObject&) override {
			if (isOver && !isDragScrolling)
				event.push(gui::Events::ButtonPressed{ {.path = path,.name = name} });
			setStatu(isOver ? gui::UIBase::Over : gui::UIBase::Normal, true);
		}
		bool isDragScrollImmediate() override { return false; }
		bool isInteractive() override { return true; }
		void onDragUpdate(bool isOver, bool isDragScrolling) override {
			if (isDragScrolling)
				setStatu(isOver ? gui::UIBase::Over : gui::UIBase::Normal, true);
			else
				setStatu(isOver ? gui::UIBase::Focus : gui::UIBase::Over, true);
		}
	};
	class OptionObject :public ButtonObject {
		friend class UIwindowManager;
		bool isInteractive() override { return true; }
		void onRelease(bool isOver, bool isDragScrolling, EventQueue& event, const std::string& path, const std::string& name, AreaObject& parent) override;
		void onDragUpdate(bool isOver, bool isDragScrolling) override;
	};
	class InputObject :public TextObject {
		friend class UIwindowManager;
	public:
		InputObject() {
			styles[gui::UIBase::Normal].set(sf::Color(250, 250, 250), sf::Color(200, 200, 200), 2);
			styles[gui::UIBase::Over].set(sf::Color(220, 220, 220), sf::Color(200, 200, 200), 2);
			styles[gui::UIBase::Focus].set(sf::Color(200, 200, 200), sf::Color(150, 150, 150), 2);
		}
		class InputLimit {
			bool isAllowList = false;
			std::vector<char32_t>single;
			std::vector<std::pair<char32_t, char32_t>>range;
		public:
			InputLimit() {}
			InputLimit(bool _isAllowList, std::initializer_list<char32_t> _single, std::initializer_list<std::pair<char32_t, char32_t>> _range) {
				isAllowList = _isAllowList;
				single = _single;
				range = _range;
				for (auto& elem : range)
					if (elem.first > elem.second)
						std::swap(elem.first, elem.second);
			}
			InputLimit& setIsAllowList(bool value) {
				isAllowList = value;
				return *this;
			}
			bool getIsAllowList() const { return isAllowList; }
			InputLimit& setSingle(const std::vector<char32_t>& value) {
				single = value;
				return *this;
			}
			const std::vector<char32_t>& getSingle() const { return single; }
			InputLimit& setRange(const std::vector<std::pair<char32_t, char32_t>>& value) {
				range = value;
				for (auto& elem : range)
					if (elem.first > elem.second)
						std::swap(elem.first, elem.second);
				return *this;
			}
			const std::vector<std::pair<char32_t, char32_t>>& getRange() const { return range; }
			bool isLegal(char32_t ch) {
				for (auto& elem : single)
					if (ch == elem)return isAllowList;
				for (auto& elem : range)
					if (ch >= elem.first && ch <= elem.second)return isAllowList;
				return !isAllowList;
			}
			size_t legalize(sf::String& st) {
				size_t pos = 0;
				for (size_t i = 0; i < st.getSize(); i++) {
					if (isLegal(st[i])) {
						if (pos != i)
							st[pos] = st[i];
						pos++;
					}
				}
				return pos;
			}
		};
	protected:
		InputLimit inputLimit;
		int typeLimit = gui::InputObject::String;
		size_t sizeLimit = INT_MAX;
		
		size_t cursor = 0;
		sf::Vector2f scroll;
		sf::Vector2f updateTextLayout();
		void ensureCursorVisible();
		void draw(sf::RenderTarget& r, sf::FloatRect displayArea, UIwindowManager& windowManager);
		inline void insert(char32_t ch) {
			if (text.getSize() >= sizeLimit)return;
			if (typeLimit == gui::InputObject::Int) {
				if ((ch == '-' && cursor == 0 && (text.getSize() == 0 || text[0] != '-')) ||
					(ch >= '0' && ch <= '9' && (text.getSize() == 0 || text[0] != '-' || cursor != 0))) {
					text.insert(cursor, ch);
					cursor++;
				}
			}
			else if (typeLimit == gui::InputObject::Float) {
				if ((ch == '-' && cursor == 0 && (text.getSize() == 0 || text[0] != '-')) ||
					(ch >= '0' && ch <= '9' && (text.getSize() == 0 || text[0] != '-' || cursor != 0)) ||
					(ch == '.' && (text.getSize() == 0 || text[0] != '-' || cursor != 0) && text.find('.') == sf::String::InvalidPos)) {
					text.insert(cursor, ch);
					cursor++;
				}
			}
			else {
				if (inputLimit.isLegal(ch)) {
					text.insert(cursor, ch);
					cursor++;
				}
			}
		}
		inline void insert(sf::String st) {
			if (typeLimit == gui::InputObject::Int || typeLimit == gui::InputObject::Float) {
				for (auto& elem : st) {
					if (text.getSize() >= sizeLimit)return;
					insert(elem);
				}
			}
			else {
				size_t remainSize = sizeLimit - text.getSize();
				st.replace('\r', "");
				size_t realSize = inputLimit.legalize(st);
				if (sizeLimit - text.getSize() > 0) {
					text.insert(cursor, st.substring(0, std::min(remainSize, realSize)));
					cursor += std::min(remainSize, realSize);
				}
			}
		}
		inline void erase(bool isForward) {
			if (isForward) {
				if (text.getSize() > 0 && cursor > 0) {
					text.erase(cursor - 1);
					cursor--;
				}
			}
			else {
				if (text.getSize() > 0 && cursor < text.getSize()) {
					text.erase(cursor);
				}
			}
		}
		inline void moveCursor(bool isForward) {
			if (isForward) {
				if (cursor > 0) {
					cursor--;
				}
			}
			else {
				if (cursor < text.getSize()) {
					cursor++;
				}
			}
		}
		void updateCursorByMousePos(sf::Vector2f mousePos) {
			size_t bestCursor = 0;
			float minDist = std::numeric_limits<float>::max();
			for (size_t i = 0; i <= text.getSize(); i++) {
				sf::Vector2f charPos = textRender.findCharacterPos(i);
				charPos += textRenderOffsetFix;
				float distX = std::abs(charPos.x - mousePos.x);
				float distY = std::abs((charPos.y + characterSize / 2.f) - mousePos.y);
				float dist = distX * distX + distY * distY;
				if (dist < minDist) {
					minDist = dist;
					bestCursor = i;
				}
			}
			cursor = bestCursor;
		}
	public:
		enum InputType { String = 0, Int = 1, Float = 2 };
		InputObject& setSizeLimit(int _sizeLimit) {
			sizeLimit = _sizeLimit;
			return *this;
		}
		size_t getSizeLimit() const { return sizeLimit; }
		InputObject& setTypeLimit(gui::InputObject::InputType _typeLimit) {
			typeLimit = _typeLimit;
			return *this;
		}
		int getTypeLimit() const { return typeLimit; }
		InputObject& setStringTypeLimit(bool _isAllowList, const std::vector<char32_t>& _single, const std::vector<std::pair<char32_t, char32_t>>& _range) {
			typeLimit = gui::InputObject::String;
			inputLimit.setIsAllowList(_isAllowList).setSingle(_single).setRange(_range);
			return *this;
		}
		std::optional<InputLimit> getStringTypeLimit() const {
			if (typeLimit != gui::InputObject::String)return {};
			return inputLimit;
		}
		std::optional<InputLimit> getStringTypeLimit() {
			if (typeLimit != gui::InputObject::String)return {};
			return inputLimit;
		}
		InputObject& setText(sf::String _text) {
			text = _text;
			cursor = text.getSize();
			return *this;
		}
		void onFocusLose(EventQueue& event, const std::string& path, const std::string& name, AreaObject&, bool focusChanged) override;
		void onFocusGain(EventQueue& event, const std::string& path, const std::string& name,
						 const sf::Vector2f& mouseInLocal, AreaObject& parent, bool focusChanged) override;
		void onTextEntered(char32_t ch) override {
			if (ch == 8) erase(true);
			else if (ch == 9) insert('\t');
			else if (ch == 13) insert('\n');
			else if (ch == 22) insert(sf::Clipboard::getString());
			else if (ch >= 32) insert(ch);
		}
		void onKeyPressed(sf::Keyboard::Key key) override {
			if (key == sf::Keyboard::Key::Left) moveCursor(true);
			else if (key == sf::Keyboard::Key::Right) moveCursor(false);
			else if (key == sf::Keyboard::Key::Delete) erase(false);
			else if (key == sf::Keyboard::Key::Home) {
				auto pos = sf::String::InvalidPos;
				for (auto i = cursor; i > 0; i--)
					if (text[i - 1] == L'\n') { pos = i; break; }
				cursor = (pos == sf::String::InvalidPos) ? 0 : pos;
			}
			else if (key == sf::Keyboard::Key::End) {
				auto pos = text.find(L'\n', cursor);
				cursor = (pos == sf::String::InvalidPos) ? text.getSize() : pos;
			}
			else if (key == sf::Keyboard::Key::Up) {
				if (cursor == 0) return;
				float currentY = textRender.findCharacterPos(cursor).y;
				// find the previous line's y
				float prevLineY = currentY;
				for (auto i = cursor; i > 0; i--) {
					float y = textRender.findCharacterPos(i - 1).y;
					if (y < currentY) { prevLineY = y; break; }
				}
				if (prevLineY == currentY) { cursor = 0; return; }
				// find closest x on the previous line
				float targetX = textRender.findCharacterPos(cursor).x + textRenderOffsetFix.x;
				float minDist = std::numeric_limits<float>::max();
				size_t bestCursor = 0;
				for (auto i = 0u; i <= text.getSize(); i++) {
					if (textRender.findCharacterPos(i).y == prevLineY) {
						float dist = std::abs((textRender.findCharacterPos(i).x + textRenderOffsetFix.x) - targetX);
						if (dist < minDist) {
							minDist = dist;
							bestCursor = i;
						}
					}
				}
				cursor = bestCursor;
			}
			else if (key == sf::Keyboard::Key::Down) {
				if (cursor == text.getSize()) return;
				float currentY = textRender.findCharacterPos(cursor).y;
				// find the next line's y
				float nextLineY = currentY;
				for (auto i = cursor; i <= text.getSize(); i++) {
					float y = textRender.findCharacterPos(i).y;
					if (y > currentY) { nextLineY = y; break; }
				}
				if (nextLineY == currentY) { cursor = text.getSize(); return; }
				// find closest x on the next line
				float targetX = textRender.findCharacterPos(cursor).x + textRenderOffsetFix.x;
				float minDist = std::numeric_limits<float>::max();
				size_t bestCursor = text.getSize();
				for (auto i = 0u; i <= text.getSize(); i++) {
					if (textRender.findCharacterPos(i).y == nextLineY) {
						float dist = std::abs((textRender.findCharacterPos(i).x + textRenderOffsetFix.x) - targetX);
						if (dist < minDist) {
							minDist = dist;
							bestCursor = i;
						}
					}
				}
				cursor = bestCursor;
			}
		}
		void onTick(UIwindowManager& wm) override;
		bool isTextEnterable() override { return true; }
		bool isInteractive() override { return true; }
		bool shouldForwardKey(sf::Keyboard::Key key) override;
	};
	class AreaObject :public UIBase {
		friend class UIwindowManager;
		friend class OptionObject;
		friend class InputObject;
	protected:
		sf::Vector2f scroll;
		sf::Vector2i mouseDragScrollable, mouseWheelScrollable;
		sf::Vector2f scrollVelocity;
		sf::FloatRect scrollLimit;
		std::string option;
	public:
		AreaObject() {
			styles[gui::UIBase::Normal].set(sf::Color::White, sf::Color(200, 200, 200), 2);
			styles[gui::UIBase::Over].set(sf::Color::White, sf::Color(200, 200, 200), 2);
			styles[gui::UIBase::Focus].set(sf::Color::White, sf::Color(200, 200, 200), 2);
		}
		VarianTmap<UIBase>sub;
		AreaObject& setScrollable(sf::Vector2i _mouseDragScrollable, sf::Vector2i _mouseWheelScrollable) {
			mouseDragScrollable = _mouseDragScrollable;
			mouseWheelScrollable = _mouseWheelScrollable;
			return *this;
		}
	protected:
		inline static void addPoint(sf::FloatRect& rect, sf::Vector2f point) {
			if (point.x < rect.position.x) {
				rect.size.x += rect.position.x - point.x;
				rect.position.x = point.x;
			}
			else if (point.x > rect.position.x + rect.size.x) {
				rect.size.x = point.x - rect.position.x;
			}
			if (point.y < rect.position.y) {
				rect.size.y += rect.position.y - point.y;
				rect.position.y = point.y;
			}
			else if (point.y > rect.position.y + rect.size.y) {
				rect.size.y = point.y - rect.position.y;
			}
		}
		AreaObject& setScrollLimitAuto() {
			scrollLimit = sf::FloatRect(sf::Vector2f(), posRect.size);
			for (auto& elem : sub) {
				addPoint(scrollLimit, elem->posRect.position);
				addPoint(scrollLimit, elem->posRect.position + elem->posRect.size);
			}
			scrollLimit.size -= posRect.size;
			return *this;
		}
	public:
		AreaObject& setOption(const std::string& key) {
			if (option!="") {
				if (auto ptr = sub.find<OptionObject>(option))
					ptr->setStatu(gui::UIBase::Normal, true);
				else option = "";
			}
			option = key;
			if (auto ptr = sub.find<OptionObject>(key)) {
				ptr->setStatu(gui::UIBase::Focus);
			}
			return *this;
		}
		AreaObject& setOption() {
			if (option != "") {
				if (auto ptr = sub.find<OptionObject>(option))
					ptr->setStatu(gui::UIBase::Normal, true);
				else option = "";
			}
			option = "";
			return *this;
		}
		std::string getOption() {
			return option;
		}
		const std::string getOption() const {
			return option;
		}
		sf::Vector2i getMouseDragScrollable() const { return mouseDragScrollable; }
		sf::Vector2i getMouseWheelScrollable() const { return mouseWheelScrollable; }
		sf::Vector2f getScroll() const { return scroll; }
		AreaObject& setScroll(sf::Vector2f value) {
			scroll = value;
			scrollVelocity = sf::Vector2f();
			return *this;
		}
	protected:
		void updateOption() {
			if (option!="") {
				if (auto ptr = sub.find<OptionObject>(option))
					ptr->setStatu(gui::UIBase::Focus);
				else option = "";
			}
		}
		void ensureScrollLimit() {
			setScrollLimitAuto();
			if (-scroll.x < scrollLimit.position.x) {
				scroll.x = -(scrollLimit.position.x);
			}
			if (-scroll.x > scrollLimit.position.x + scrollLimit.size.x) {
				scroll.x = -(scrollLimit.position.x + scrollLimit.size.x);
			}
			if (-scroll.y < scrollLimit.position.y) {
				scroll.y = -(scrollLimit.position.y);
			}
			if (-scroll.y > scrollLimit.position.y + scrollLimit.size.y) {
				scroll.y = -(scrollLimit.position.y + scrollLimit.size.y);
			}
		}
		void updateLayout(sf::Vector2f fatherSize) {
			updatePosRect(fatherSize);
			for (auto& elem : sub.iterate()) {
				if (auto* area = sub.find<AreaObject>(elem))
					area->updateLayout(posRect.size);
				else
					elem->updatePosRect(posRect.size);
			}
		}
		void updateScroll(UIwindowManager& windowManager);
		void draw(sf::RenderTarget& r, sf::FloatRect displayArea, UIwindowManager& windowManager);
	public:
		void onDragScroll(sf::Vector2f delta) { scroll += delta; }
		void onInertialScrollStart(sf::Vector2f velocity) override { scrollVelocity = velocity; }
		void onInertialScrollStop() { scrollVelocity = sf::Vector2f(); }
		template<typename T>
		T& path_get(const std::string& path) {
			AreaObject* areaPtr = this;
			std::string temp = "";
			for (int i = 0; i < path.size(); i++) {
				if (path[i] == '_' || path[i] == '.') {
					areaPtr = &(areaPtr->sub.get<AreaObject>(temp));
					temp = "";
				}
				else temp.push_back(path[i]);
			}
			return areaPtr->sub.get<T>(temp);
		}
		template<typename T>
		const T& path_at(const std::string& path) const {
			const AreaObject* areaPtr = this;
			std::string temp = "";
			for (int i = 0; i < path.size(); i++) {
				if (path[i] == '_' || path[i] == '.') {
					areaPtr = &(areaPtr->sub.at<AreaObject>(temp));
					temp = "";
				}
				else temp.push_back(path[i]);
			}
			return areaPtr->sub.at<T>(temp);
		}
		template<typename T>
		T& path_at(const std::string& path) {
			return const_cast<T&>(std::as_const(*this).path_at<T>(path));
		}
		template<typename T>
		T* path_find(const std::string& path) const {
			const AreaObject* areaPtr = this;
			std::string temp = "";
			for (int i = 0; i < path.size(); i++) {
				if (path[i] == '_' || path[i] == '.') {
					areaPtr = areaPtr->sub.find<AreaObject>(temp);
					temp = "";
					if (areaPtr == nullptr)return nullptr;
				}
				else temp.push_back(path[i]);
			}
			return areaPtr->sub.find<T>(temp);
		}
		template<typename T>
		T* path_find(const std::string& path) {
			return const_cast<T*>(std::as_const(*this).path_find<T>(path));
		}
	};
	inline void OptionObject::onRelease(bool isOver, bool isDragScrolling, EventQueue& event, const std::string& path, const std::string& name, AreaObject& parent) {
		if (isOver && !isDragScrolling) {
			if (name != parent.option) {
				if (parent.option != "") {
					parent.sub.at<OptionObject>(parent.option).setStatu(gui::UIBase::Normal, true);
					event.push(gui::Events::OptionDeselected{ {.path = path,.name = parent.option} });
				}
				parent.option = name;
				event.push(gui::Events::OptionSelected{ {.path = path,.name = name} });
			}
		}
		setStatu(isOver ? gui::UIBase::Over : gui::UIBase::Normal, true);
		parent.updateOption();
	}
	inline void OptionObject::onDragUpdate(bool isOver, bool isDragScrolling) {
		if (isDragScrolling)
			setStatu(isOver ? gui::UIBase::Over : gui::UIBase::Normal, true);
		else
			setStatu(isOver ? gui::UIBase::Focus : gui::UIBase::Over, true);
	}
	inline void InputObject::onFocusLose(EventQueue& event, const std::string& path, const std::string& name, AreaObject&, bool focusChanged) {
		setStatu(gui::UIBase::Normal, true);
		if (focusChanged)
			event.push(gui::Events::InputDeselected{ {.path = path,.name = name} });
	}
	inline void InputObject::onFocusGain(EventQueue& event, const std::string& path, const std::string& name,
										 const sf::Vector2f& mouseInLocal, AreaObject& parent, bool focusChanged) {
		setStatu(gui::UIBase::Focus);
		if (focusChanged)
			event.push(gui::Events::InputSelected{ {.path = path,.name = name} });
		updateCursorByMousePos(mouseInLocal);
	}
	class UIwindowManager {
		friend class UIBase;
		friend class InputObject;
		friend class AreaObject;
	public:
		UIwindowManager() {}
		//noncopyable
		UIwindowManager(const UIwindowManager&) = delete;
		UIwindowManager& operator=(const UIwindowManager&) = delete;
		size_t getCursorBlinkRate() const { return cursorBlinkRate; }
		UIwindowManager& setCursorBlinkRate(size_t rate) { cursorBlinkRate = rate; return *this; }
		float getScrollResistance() const { return scrollResistance; }
		UIwindowManager& setScrollResistance(float resistance) { scrollResistance = resistance; return *this; }
		float getMouseWheelScrollRate() const { return mouseWheelScrollRate; }
		UIwindowManager& setMouseWheelScrollRate(float rate) { mouseWheelScrollRate = rate; return *this; }
		void simulatePress(const std::string& path) {
			size_t lastSep = path.find_last_of("._");
			if (lastSep == std::string::npos) return;
			std::string areaPath = path.substr(0, lastSep);
			std::string objectName = path.substr(lastSep + 1);
			AreaObject* areaPtr = path_find<AreaObject>(areaPath);
			if (!areaPtr) return;
			// 查找目标对象并确定类型
			UIBase* targetObj = areaPtr->sub.find(objectName);
			if (!targetObj || !targetObj->isInteractive()) return;
			ObjectPath target;
			target.path = areaPath;
			target.name = objectName;
			target.setType<UIBase>();
			// 处理焦点变更（复用 update 的 Press 逻辑）
			auto* curAreaPtr = focus.type.has_value() ? path_find<AreaObject>(focus.path) : nullptr;
			if (focus.type.has_value() && focus.path == target.path && focus.name == target.name) {
				if (auto* obj = objectPathVisit(focus, curAreaPtr)) {
					obj->onFocusLose(event, focus.path, focus.name, *curAreaPtr, false);
					curAreaPtr->updateOption();
					obj->onFocusGain(event, focus.path, focus.name, sf::Vector2f(),
					                 *curAreaPtr, false);
				}
			}
			else {
				if (auto* oldFocus = objectPathVisit(focus, curAreaPtr)) {
					oldFocus->onFocusLose(event, focus.path, focus.name, *curAreaPtr, true);
					if (curAreaPtr) curAreaPtr->updateOption();
				}
				targetObj->onFocusGain(event, areaPath, objectName,
				                       sf::Vector2f(), *areaPtr, true);
				areaPtr->updateOption();
			}
			focus = target;
		}
		void simulateRelease(const std::string&) {
			over = focus;
			auto* areaFocusPtr = focus.type.has_value() ? path_find<AreaObject>(focus.path) : nullptr;
			if (areaFocusPtr) {
				if (auto* obj = objectPathVisit(focus, areaFocusPtr))
					obj->onRelease(true, false, event, focus.path, focus.name, *areaFocusPtr);
			}
		}
		void simulateSetPrevOption(const std::string& areaPath) {
			auto& area = path_at<AreaObject>(areaPath);
			std::string curOption = area.getOption();
			std::vector<std::pair<sf::Vector2f, std::string>> entries;
			for (auto& elem : area.sub.iterate()) {
				if (area.sub.find_type_index(elem) == std::type_index(typeid(OptionObject))) {
					auto* obj = static_cast<OptionObject*>(elem);
					entries.push_back({ obj->posRect.position, area.sub.find_key(elem) });
				}
			}
			if (entries.empty()) return;
			std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
				return a.first.x < b.first.x || (a.first.x == b.first.x && a.first.y < b.first.y);
			});
			for (size_t i = 0; i < entries.size(); i++) {
				if (entries[i].second == curOption) {
					if (i > 0) {
						std::string btnPath = areaPath + "." + entries[i - 1].second;
						simulatePress(btnPath);
						simulateRelease(btnPath);
					}
					return;
				}
			}
			std::string btnPath = areaPath + "." + entries[0].second;
			simulatePress(btnPath);
			simulateRelease(btnPath);
		}
		void simulateSetNextOption(const std::string& areaPath) {
			auto& area = path_at<AreaObject>(areaPath);
			std::string curOption = area.getOption();
			std::vector<std::pair<sf::Vector2f, std::string>> entries;
			for (auto& elem : area.sub.iterate()) {
				if (area.sub.find_type_index(elem) == std::type_index(typeid(OptionObject))) {
					auto* obj = static_cast<OptionObject*>(elem);
					entries.push_back({ obj->posRect.position, area.sub.find_key(elem) });
				}
			}
			if (entries.empty()) return;
			std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
				return a.first.x < b.first.x || (a.first.x == b.first.x && a.first.y < b.first.y);
			});
			for (size_t i = 0; i < entries.size(); i++) {
				if (entries[i].second == curOption) {
					if (i + 1 < entries.size()) {
						std::string btnPath = areaPath + "." + entries[i + 1].second;
						simulatePress(btnPath);
						simulateRelease(btnPath);
					}
					return;
				}
			}
			std::string btnPath = areaPath + "." + entries[0].second;
			simulatePress(btnPath);
			simulateRelease(btnPath);
		}
		std::string getTopWindowId() const {
			return layer.find_key(std::prev(layer.end()));
		}
	private:
		//窗口管理

		// One full cursor blink cycle (0.5 seconds).
		size_t cursorBlinkRate = TickManager::ticksPerSecond / 2;
		float scrollResistance = 3.f;
		// Retain 0.1 seconds of mouse history.
		static constexpr int scrollSensitivity = static_cast<int>(TickManager::ticksPerSecond / 10);
		float mouseWheelScrollRate = 50.f;
		float scrollThreshold = 20;

		EventQueue event;
		struct ObjectPath {
			std::optional<std::type_index> type;
			std::string path;
			std::string name;
			template<typename T>
			void setType() {
				type = std::type_index(typeid(T));
			}
			void setType(std::type_index type) {
				this->type = type;
			}
			template<typename T>
			bool is() const {
				return std::type_index(typeid(T)) == type;
			}
			void clear() {
				type = std::nullopt;
				path.clear();
				name.clear();
			}
			bool operator==(const ObjectPath& other) const = default;
		}focus, over;
		sf::Vector2f mouseInOverObject;
		bool isDragScrolling = false;
		bool mousePressed = false;
		bool isPressInsideUI = false;
		bool isFocusUI = false;
		unsigned int cursorBlinkTick = 0;
		template<typename T, int capacity>
		class RollArray {
		private:
			T array[capacity]{};
			size_t realSize = 0;
			size_t backPosition = 0;
		public:
			inline void emplace_back() {
				if (realSize < capacity) {
					realSize++;
					backPosition = realSize - 1;
				}
				else {
					backPosition = (backPosition + 1) % capacity;
				}
			}
			inline T& front() {
				if (realSize < capacity)
					return array[0];
				else return array[(backPosition + 1) % capacity];
			}
			inline T& back() {
				return array[backPosition];
			}
			inline T& operator[](size_t pos) {
				return array[(((realSize < capacity ? 0 : (backPosition + 1) % capacity) + pos) % capacity + capacity) % capacity];
			}
			inline bool copy_back() {
				emplace_back();
				array[backPosition] = array[(backPosition - 1 + capacity) % capacity];
				return true;
			}
			inline size_t size() {
				return realSize;
			}
		};
		RollArray<sf::Vector2f, scrollSensitivity>mousePos;
		sf::Vector2f mouseLastPressPos;
		sf::Vector2f mouseVelocity() {
			return mousePos.back() - mousePos.front();
		}
		VarianTmap<AreaObject> layer;
	public:
		void clear() {
			event.clear();
			focus.clear(); over.clear();
			layer.clear();
		}
		void open(const std::string& id) {
			if (layer.find(id)) {
				throw std::runtime_error("[UIwindowManager::open] Window ID already exists 窗口ID重复\n  id: " + id + "\n");
			}
			layer.emplace<AreaObject>(layer.end(),id);
		}
		void open(const std::string& id,const AreaObject& Window) {
			if (layer.find(id)) {
				throw std::runtime_error("[UIwindowManager::open] Window ID already exists 窗口ID重复\n  id: " + id + "\n");
			}
			auto ptr=layer.push_back<AreaObject>(id, Window);
			ptr->updateOption();
		}
		void open(const std::string& id, AreaObject&& Window) {
			if (layer.find(id)) {
				throw std::runtime_error("[UIwindowManager::open] Window ID already exists 窗口ID重复\n  id: " + id + "\n");
			}
			auto ptr = layer.push_back<AreaObject>(id, Window);
			ptr->updateOption();
		}
		size_t close(const std::string& id) {
			if (!layer.empty() && layer.find_key(std::prev(layer.end())) == id) {
				layer.erase(std::prev(layer.end()));
				return 1;
			}
			return 0;
		}
		size_t closeUntil(const std::string& id, bool include = false) {
			size_t count = 0;
			while (!layer.empty() && layer.find_key(std::prev(layer.end())) != id) {
				layer.erase(std::prev(layer.end()));
				count++;
			}
			if (include && !layer.empty() && layer.find_key(std::prev(layer.end())) == id) {
				layer.erase(std::prev(layer.end()));
				count++;
			}
			return count;
		}
		AreaObject& window(const std::string& id) {
			if (auto ptr = layer.find(id))
				return *ptr;
			else {
				throw std::runtime_error("[UIwindowManager::window] Window ID not found 未找到指定的窗口ID\n  id: " + id + "\n");
			}
		}
		template<typename T>
		T& path_get(const std::string& path){
			AreaObject* areaPtr = nullptr;
			std::string temp = "";
			for (int i = 0; i < path.size(); i++) {
				if (path[i] == '_' || path[i] == '.') {
					if (areaPtr == nullptr)
						areaPtr = &window(temp);
					else areaPtr = &(areaPtr->sub.get<AreaObject>(temp));
					temp = "";
				}
				else temp.push_back(path[i]);
			}
			if constexpr (std::is_same_v<T, AreaObject>) {
				if (areaPtr == nullptr)
					return window(temp);
				else return areaPtr->sub.get<T>(temp);
			}
			else {
				if (areaPtr != nullptr)
					return areaPtr->sub.get<T>(temp);
				else {
					throw std::runtime_error("[UIwindowManager::path_get] Path error. 路径错误\n  path: " + path + "\n");
				}
			}
		}
		template<typename T>
		T& path_at(const std::string& path) {
			AreaObject* areaPtr = nullptr;
			std::string temp = "";
			for (int i = 0; i < path.size(); i++) {
				if (path[i] == '_'|| path[i] == '.') {
					if (areaPtr == nullptr)
						areaPtr = &window(temp);
					else areaPtr = &(areaPtr->sub.at<AreaObject>(temp));
					temp = "";
				}
				else temp.push_back(path[i]);
			}
			if constexpr (std::is_same_v<T, AreaObject>) {
				if (areaPtr == nullptr)
					return window(temp);
				else return areaPtr->sub.at<T>(temp);
			}
			else {
				if (areaPtr != nullptr)
					return areaPtr->sub.at<T>(temp);
				else {
					throw std::runtime_error("[UIwindowManager::path_at] Path error. 路径错误\n  path: " + path + "\n");
				}
			}
		}
		template<typename T>
		T* path_find(const std::string& path) {
			AreaObject* areaPtr = nullptr;
			std::string temp = "";
			for (int i = 0; i < path.size(); i++) {
				if (path[i] == '_' || path[i] == '.') {
					if (areaPtr == nullptr)
						areaPtr = layer.find<AreaObject>(temp);
					else areaPtr = areaPtr->sub.find<AreaObject>(temp);
					temp = "";
					if (areaPtr == nullptr)return nullptr;
				}
				else temp.push_back(path[i]);
			}
			if constexpr (std::is_same_v<T, AreaObject>) {
				if (areaPtr == nullptr)
					return layer.find(temp);
				else return areaPtr->sub.find<T>(temp);
			}
			else return areaPtr->sub.find<T>(temp);
		}
		const std::optional<Event> pollEvent() {
			return event.pollEvent();
		}
	private:
		inline bool isOutsideUIwindow(sf::Vector2i pos) const {
			return !(*std::prev(layer.end()))->posRect.contains(static_cast<sf::Vector2f>(pos));
		}
		inline UIBase* objectPathVisit(ObjectPath& obj, AreaObject* areaPtr = nullptr) {
			if (!obj.type.has_value())return nullptr;
			if (areaPtr == nullptr) {
				areaPtr = path_find<gui::AreaObject>(obj.path);
				if (areaPtr == nullptr) return nullptr;
			}
			if (auto* interactive = areaPtr->sub.find(obj.name);
				interactive && interactive->isInteractive())
				return interactive;
			if (obj.is<gui::AreaObject>())
				return areaPtr;
			return nullptr;
		}
		inline AreaObject* updateOverInArea(AreaObject* areaPtr,
		                                      sf::Vector2f mouseInLocal,
		                                      const std::string& path,
		                                      bool stopScroll) {
			if (stopScroll)
				areaPtr->scrollVelocity = sf::Vector2f();

			// Each AreaObject converts its own local position into content-space
			// coordinates, then passes coordinates local to the child downwards.
			const sf::Vector2f mouseInContent = mouseInLocal - areaPtr->scroll;
			for (auto& elem : areaPtr->sub.riterate()) {
				if (!elem->isShow || !elem->posRect.contains(mouseInContent))
					continue;

				const sf::Vector2f mouseInChild = mouseInContent - elem->posRect.position;
				if (auto* childArea = areaPtr->sub.find<AreaObject>(elem)) {
					return updateOverInArea(childArea, mouseInChild,
					                        path + '_' + areaPtr->sub.find_key(elem),
					                        stopScroll);
				}

				if (elem->isInteractive()) {
					over.path = path;
					over.setType<UIBase>();
					over.name = areaPtr->sub.find_key(elem);
					mouseInOverObject = mouseInChild;
					return areaPtr;
				}
			}

			over.setType<AreaObject>();
			over.path = path;
			mouseInOverObject = mouseInLocal;
			return areaPtr;
		}

		inline AreaObject* updateOver(bool stopScroll = false) {
			if (objectPathVisit(over) != nullptr)
				objectPathVisit(over)->setStatu(gui::UIBase::Normal);
			AreaObject* root = *std::prev(layer.end());
			over.clear();
			mouseInOverObject = sf::Vector2f();
			return updateOverInArea(root, mousePos.back() - root->posRect.position,
			                        layer.find_key(root), stopScroll);
		}
		inline void updateSimpleMove(AreaObject* areaFocusPtr, AreaObject* areaOverPtr) {
			if (areaFocusPtr != nullptr && mousePressed) {
				if (auto* obj = objectPathVisit(focus, areaFocusPtr)) {
					if (areaFocusPtr->mouseDragScrollable != sf::Vector2i()) {
						if (obj->isDragScrollImmediate() || (mouseLastPressPos - mousePos.back()).lengthSquared() >= scrollThreshold * scrollThreshold)
							isDragScrolling = true;
						if (isDragScrolling) {
							obj->onDragUpdate(over == focus, true);
							areaFocusPtr->updateOption();
						}
					}
					else {
						obj->onDragUpdate(over == focus, false);
						areaFocusPtr->updateOption();
					}
				}
			}
			else {
				if (auto* obj = objectPathVisit(over, areaOverPtr))
					obj->setStatu(gui::UIBase::Over);
			}
		}
	public:
		bool update(const std::optional<sf::Event>& sfEvent) {
			if (layer.size() == 0)
				return false;
			//update mouse press statu
			if (auto ptr = sfEvent->getIf<sf::Event::MouseButtonPressed>()) {
				mousePressed = true;
				mouseLastPressPos = static_cast<sf::Vector2f>(ptr->position);
			}
			if (sfEvent->is<sf::Event::MouseButtonReleased>())
				mousePressed = false;
			if (objectPathVisit(focus) == nullptr)
				focus.clear();
			AreaObject* areaOverPtr = nullptr, * areaFocusPtr = nullptr;
			if (focus.type.has_value())
				areaFocusPtr = path_find<AreaObject>(focus.path);
			//sf::Event::MouseButtonPressed
			//update focus
			//after updating focus ,varible 'focus' & 'over' must have a value
			if (auto ptr = sfEvent->getIf<sf::Event::MouseButtonPressed>()) {
				mousePos.back() = sf::Vector2f(ptr->position);//update mousePos
				if (isOutsideUIwindow(ptr->position)) {
					isPressInsideUI = false;
					isFocusUI = false;
					event.push(gui::Events::MousePressed{ {.isFocusUI = false}, ptr->button, ptr->position });
					return true;
				}

				//update inertial scroll stop
				isPressInsideUI = true;
				isFocusUI = true;
				areaOverPtr = updateOver(true);

				if (over != focus) {
					if (auto* oldFocus = objectPathVisit(focus, areaFocusPtr)) {
						oldFocus->onFocusLose(event, focus.path, focus.name, *areaFocusPtr, true);
						areaFocusPtr->updateOption();
					}
					if (auto* newFocus = objectPathVisit(over, areaOverPtr))
						newFocus->onFocusGain(event, over.path, over.name, mouseInOverObject,
						                      *areaOverPtr, true);
				}
				else if (auto* sameFocus = objectPathVisit(focus, areaFocusPtr)) {
					sameFocus->onFocusLose(event, focus.path, focus.name, *areaFocusPtr, false);
					areaFocusPtr->updateOption();
					sameFocus->onFocusGain(event, focus.path, focus.name, mouseInOverObject,
					                       *areaFocusPtr, false);
				}

				focus = over;
				return true;
			}
			//sf::Event::MouseMoved
			if (auto ptr = sfEvent->getIf<sf::Event::MouseMoved>()) {
				sf::Vector2f mousePosDelta = sf::Vector2f(ptr->position) - mousePos.back();
				mousePos.back() = sf::Vector2f(ptr->position);//update mousePos
				if (mousePressed && !isPressInsideUI) {
					event.push(gui::Events::MouseMoved{ {.isFocusUI = false}, ptr->position });
					return true;
				}
				areaOverPtr = updateOver();
				if (areaFocusPtr != nullptr && mousePressed && areaFocusPtr->mouseDragScrollable != sf::Vector2i() && isDragScrolling)
					areaFocusPtr->onDragScroll(mousePosDelta.componentWiseMul(static_cast<sf::Vector2f>(areaFocusPtr->mouseDragScrollable)));
				updateSimpleMove(areaFocusPtr, areaOverPtr);
				return true;
			}
			//sf::Event::MouseButtonReleased 
			//update release
			//after updating release ,varible 'focus' will not be changed
			if (auto ptr = sfEvent->getIf<sf::Event::MouseButtonReleased>()) {
				mousePos.back() = sf::Vector2f(ptr->position);//update mousePos
				if (!isPressInsideUI) {
					event.push(gui::Events::MouseReleased{ {.isFocusUI = false}, ptr->button, ptr->position });
					return true;
				}
				areaOverPtr = updateOver();

				if (areaFocusPtr != nullptr)
					objectPathVisit(focus, areaFocusPtr)->onRelease(over == focus, isDragScrolling, event, focus.path, focus.name, *areaFocusPtr);
				isDragScrolling = false;
				isPressInsideUI = false;

				//update inertial scroll start
				if (areaFocusPtr != nullptr && areaFocusPtr->mouseDragScrollable != sf::Vector2i())
					objectPathVisit(focus, areaFocusPtr)->onInertialScrollStart(mouseVelocity()
						.componentWiseMul(static_cast<sf::Vector2f>(areaFocusPtr->mouseDragScrollable))
						.componentWiseDiv(sf::Vector2f(scrollSensitivity, scrollSensitivity)));
				return true;
			}
			if (auto ptr = sfEvent->getIf<sf::Event::TextEntered>()) {
				if (auto* obj = objectPathVisit(focus, areaFocusPtr)) {
					obj->onTextEntered(ptr->unicode);
					cursorBlinkTick = 0;
				}
				return true;
			}
			if (auto ptr = sfEvent->getIf<sf::Event::KeyPressed>()) {
				if (auto* obj = objectPathVisit(focus, areaFocusPtr)) {
					obj->onKeyPressed(ptr->code);
					cursorBlinkTick = 0;
				}
				if (!focus.type.has_value() || !objectPathVisit(focus)->isTextEnterable() || objectPathVisit(focus)->shouldForwardKey(ptr->code))
					event.push(gui::Events::KeyPressed{ {.isFocusUI = isFocusUI}, getTopWindowId(), focus.path, ptr->code });
				return true;
			}
			if (auto ptr = sfEvent->getIf<sf::Event::KeyReleased>()) {
				if (!focus.type.has_value() || !objectPathVisit(focus)->isTextEnterable() || objectPathVisit(focus)->shouldForwardKey(ptr->code))
					event.push(gui::Events::KeyReleased{ {.isFocusUI = isFocusUI}, getTopWindowId(), focus.path, ptr->code });
				return true;
			}
			if (auto ptr = sfEvent->getIf<sf::Event::MouseWheelScrolled>()) {
				if (isOutsideUIwindow(ptr->position)) {
					event.push(gui::Events::MouseWheelScrolled{ {.isFocusUI = false}, ptr->wheel, ptr->delta, ptr->position });
					return true;
				}
				areaOverPtr = updateOver();
				float delta = ptr->delta;
				if (areaOverPtr->mouseWheelScrollable == sf::Vector2i(1, 0))
					areaOverPtr->onInertialScrollStart(sf::Vector2f(delta * mouseWheelScrollRate, 0));
				if (areaOverPtr->mouseWheelScrollable == sf::Vector2i(0, 1))
					areaOverPtr->onInertialScrollStart(sf::Vector2f(0, delta * mouseWheelScrollRate));
				if (areaOverPtr->mouseWheelScrollable == sf::Vector2i(1, 1)) {
					if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::RShift))
						areaOverPtr->onInertialScrollStart(sf::Vector2f(delta * mouseWheelScrollRate, 0));
					else areaOverPtr->onInertialScrollStart(sf::Vector2f(0, delta * mouseWheelScrollRate));
				}
				updateSimpleMove(areaFocusPtr, areaOverPtr);
				return true;
			}
			return false;
		}
		// Recomputes layout, then advances all time-based UI state by one game tick.
		void update(sf::Vector2f viewportSize) {
			for (auto& elem : layer.iterate())
				elem->updateLayout(viewportSize);
			if (layer.size() >= 1) {
				if (objectPathVisit(focus) == nullptr)
					focus.clear();
				if (!(mousePressed && !isPressInsideUI))
					updateSimpleMove(focus.type.has_value() ? path_find<AreaObject>(focus.path) : nullptr, updateOver());
			}
			if (auto* obj = objectPathVisit(focus))
				obj->onTick(*this);
			mousePos.copy_back();
			for (auto& elem : layer)
				elem->updateScroll(*this);
		}
		// Draws the layout produced by update() without mutating it.
		void draw(sf::RenderTarget& drawTarget) {
			for (auto& elem : layer) {
				elem->draw(drawTarget, sf::FloatRect(sf::Vector2f(), static_cast<sf::Vector2f>(drawTarget.getSize())), *this);
			}
		}
	};
	inline void UIBase::onTick(UIwindowManager& wm) {
		wm.cursorBlinkTick = 0;
	}
	inline void InputObject::onTick(UIwindowManager& wm) {
		wm.cursorBlinkTick++;
		wm.cursorBlinkTick %= wm.cursorBlinkRate;
		ensureCursorVisible();
	}
	inline bool InputObject::shouldForwardKey(sf::Keyboard::Key key) {
		switch (key) {
			case sf::Keyboard::Key::Escape:
			case sf::Keyboard::Key::LControl: case sf::Keyboard::Key::LShift:
			case sf::Keyboard::Key::LAlt: case sf::Keyboard::Key::LSystem:
			case sf::Keyboard::Key::RControl: case sf::Keyboard::Key::RShift:
			case sf::Keyboard::Key::RAlt: case sf::Keyboard::Key::RSystem:
			case sf::Keyboard::Key::Menu:
			case sf::Keyboard::Key::F1: case sf::Keyboard::Key::F2: case sf::Keyboard::Key::F3:
			case sf::Keyboard::Key::F4: case sf::Keyboard::Key::F5: case sf::Keyboard::Key::F6:
			case sf::Keyboard::Key::F7: case sf::Keyboard::Key::F8: case sf::Keyboard::Key::F9:
			case sf::Keyboard::Key::F10: case sf::Keyboard::Key::F11: case sf::Keyboard::Key::F12:
			case sf::Keyboard::Key::F13: case sf::Keyboard::Key::F14: case sf::Keyboard::Key::F15:
			case sf::Keyboard::Key::Pause:
				return true;
			case sf::Keyboard::Key::Tab:
				if (typeLimit == Int || typeLimit == Float)
					return true;
				if (typeLimit == String && !inputLimit.isLegal('\t'))
					return true;
				return false;
			case sf::Keyboard::Key::Enter:
				if (typeLimit == Int || typeLimit == Float)
					return true;
				if (typeLimit == String && !inputLimit.isLegal('\n'))
					return true;
				return false;
			default:
				return false;
		}
	}
}
