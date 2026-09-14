#include "engine/gui/MyGUI.hpp"
namespace gui {
	void UIBase::draw(sf::RenderTarget& r, sf::FloatRect displayArea, UIwindowManager& windowManager) {
		if (!isShow)
			return;
		if (posRect.findIntersection(displayArea)) {
			_builtinGUIdraw::Rect(
				r,
				posRect.position - displayArea.position,
				posRect.position - displayArea.position + posRect.size,
				styles[currentStatu].backgroundColor,
				styles[currentStatu].outlineColor,
				styles[currentStatu].outlineThickness
			);
		}
	}
	void ImageObject::draw(sf::RenderTarget& r, sf::FloatRect displayArea, UIwindowManager& windowManager) {
		if (!isShow)
			return;
		UIBase::draw(r, displayArea, windowManager);
		if (posRect.findIntersection(displayArea)) {
			sf::Vector2f imgSize = static_cast<sf::Vector2f>(imageManager[imageId].getSize());
			sf::Vector2f realScale = scale;
			if (realScale.x < 0) realScale.x = posRect.size.x / imgSize.x;
			if (realScale.y < 0) realScale.y = posRect.size.y / imgSize.y;
			sf::Sprite imageRender(imageManager[imageId]);
			imageRender.setPosition(posRect.position + ((posRect.size - imgSize.componentWiseMul(realScale)) / 2.f).componentWiseMul(static_cast<sf::Vector2f>(align)) - displayArea.position);
			imageRender.setScale(realScale);
			imageRender.setColor(imageColors[currentStatu]);
			r.draw(imageRender);
		}
	}
	void TextObject::draw(sf::RenderTarget& r, sf::FloatRect displayArea, UIwindowManager& windowManager) {
		if (!isShow)
			return;
		UIBase::draw(r, displayArea, windowManager);

		//render text
		textRender.setString("_");
		textRender.setCharacterSize(characterSize);
		textRender.setLineSpacing(lineSpacing);
		textRender.setLetterSpacing(letterSpacing);
		textRender.setFillColor(textStyles[currentStatu].fillColor);
		textRender.setOutlineColor(textStyles[currentStatu].outlineColor);
		textRender.setPosition({ 0,0 });
		//fix position offset
		sf::Vector2f offsetFix;
		offsetFix.x = textRender.getGlobalBounds().position.x;
		offsetFix.y = textRender.getGlobalBounds().position.y + textRender.getGlobalBounds().size.y - characterSize;
		textRender.setString(text);
		textRect.size.x = 0;
		for (int i = 0; i <= text.getSize(); i++) {
			if (textRender.findCharacterPos(i).x > textRect.size.x)
				textRect.size.x = textRender.findCharacterPos(i).x;
		}
		textRect.size.y = textRender.findCharacterPos(textRender.getString().getSize()).y + characterSize;
		textRender.setPosition(posRect.position - offsetFix + ((posRect.size - textRect.size) / 2.f).componentWiseMul(static_cast<sf::Vector2f>(align)) - displayArea.position);
		textRect.position = posRect.position + ((posRect.size - textRect.size) / 2.f).componentWiseMul(static_cast<sf::Vector2f>(align));

				if (textRect.findIntersection(displayArea)) {
			//debug
			/*_builtinGUIdraw::Rect(
				r,
				textRect.position - displayArea.position,
				textRect.position - displayArea.position + textRect.size,
				sf::Color::Red
				//textStyles[currentStatu].fillColor
			);*/
			r.draw(textRender);
		}
	}
	sf::Vector2f InputObject::updateTextLayout() {
			textRender.setString("_");
			textRender.setCharacterSize(characterSize);
			textRender.setLineSpacing(lineSpacing);
			textRender.setLetterSpacing(letterSpacing);
			textRender.setFillColor(textStyles[currentStatu].fillColor);
			textRender.setOutlineColor(textStyles[currentStatu].outlineColor);
			textRender.setPosition({ 0,0 });
			//fix position offset
			textRenderOffsetFix.x = textRender.getGlobalBounds().position.x;
			textRenderOffsetFix.y = textRender.getGlobalBounds().position.y + textRender.getGlobalBounds().size.y - characterSize;
			textRender.setString(text);
			textRect.size.x = 0;
			for (int i = 0; i <= text.getSize(); i++) {
				if (textRender.findCharacterPos(i).x > textRect.size.x)
					textRect.size.x = textRender.findCharacterPos(i).x;
			}
			textRect.size.y = textRender.findCharacterPos(textRender.getString().getSize()).y + characterSize;
			const sf::FloatRect displayAreaCur(-scroll, posRect.size);
			textRender.setPosition(-textRenderOffsetFix + ((posRect.size - textRect.size) / 2.f).componentWiseMul(static_cast<sf::Vector2f>(align)) - displayAreaCur.position);
			textRect.position = ((posRect.size - textRect.size) / 2.f).componentWiseMul(static_cast<sf::Vector2f>(align));
			sf::Vector2f cursorPos = textRender.findCharacterPos(cursor);
			cursorPos += textRenderOffsetFix;
			return cursorPos;
	}

	void InputObject::ensureCursorVisible() {
			sf::Vector2f cursorPos = updateTextLayout();
			if (textRect.size.x > posRect.size.x) {
				if (cursorPos.x < 0)
					scroll.x += 0 - cursorPos.x;
				else if (cursorPos.x > posRect.size.x)
					scroll.x += posRect.size.x - cursorPos.x;
				if (textRect.position.x + scroll.x + textRect.size.x < posRect.size.x)
					scroll.x += posRect.size.x - (textRect.position.x + scroll.x + textRect.size.x);
				if (textRect.position.x + scroll.x > 0)
					scroll.x += 0 - (textRect.position.x + scroll.x);
			}
			else scroll.x = 0;
			if (textRect.size.y > posRect.size.y) {
				if (posRect.size.y < characterSize){
					scroll.y += 0 - cursorPos.y;
				}
				else {
					if (cursorPos.y < 0)
						scroll.y += 0 - cursorPos.y;
					else if (cursorPos.y + characterSize > posRect.size.y)
						scroll.y += posRect.size.y - (cursorPos.y + characterSize);
				}
				if (textRect.position.y + scroll.y + textRect.size.y < posRect.size.y)
					scroll.y += posRect.size.y - (textRect.position.y + scroll.y + textRect.size.y);
				if (textRect.position.y + scroll.y > 0)
					scroll.y += 0 - (textRect.position.y + scroll.y);
			}
			else scroll.y = 0;
	}

	void InputObject::draw(sf::RenderTarget& r, sf::FloatRect displayArea, UIwindowManager& windowManager) {
		if (!isShow)
			return;
		if (posRect.findIntersection(displayArea)) {
			UIBase::draw(r, displayArea, windowManager);
			const sf::FloatRect displayAreaCur(-scroll, posRect.size);
			sf::RenderTexture rCur(static_cast<sf::Vector2u>(posRect.size));
			rCur.clear(sf::Color::Transparent);
			const sf::Vector2f cursorPos = updateTextLayout();
			//debug
			/*_builtinGUIdraw::Rect(
				rCur,
				textRect.position - displayAreaCur.position,
				textRect.position - displayAreaCur.position + textRect.size,
				sf::Color::Red
				//textStyles[currentStatu].fillColor
			);*/
			rCur.draw(textRender);

			//draw cursor
			if (currentStatu == gui::UIBase::Focus && (windowManager.cursorBlinkTick >= 0 && windowManager.cursorBlinkTick < windowManager.cursorBlinkRate / 2)) {
				_builtinGUIdraw::Line(
					rCur,
					cursorPos,
					cursorPos + sf::Vector2f(0, (float)characterSize),
					textStyles[currentStatu].fillColor,
					2.0f
				);
			}
			rCur.display();
			sf::Sprite s(rCur.getTexture());
			s.setPosition(posRect.position - displayArea.position);
			r.draw(s);
		}
	}
	void AreaObject::draw(sf::RenderTarget& r, sf::FloatRect displayArea, UIwindowManager& windowManager) {
		if (!isShow)
			return;
		if (!posRect.findIntersection(displayArea))
			return;
		UIBase::draw(r, displayArea, windowManager);
		sf::FloatRect displayAreaCur(-scroll, posRect.size);
		sf::RenderTexture rCur(static_cast<sf::Vector2u>(posRect.size));
		rCur.clear(sf::Color::Transparent);
		//clear
		for (auto& elem : sub.riterate()) {
			elem->draw(rCur, displayAreaCur, windowManager);
		}
		rCur.display();
		sf::Sprite s(rCur.getTexture());
		s.setPosition(posRect.position - displayArea.position);
		r.draw(s);
	}
	void AreaObject::updateScroll(UIwindowManager& windowManager) {
		if (scrollVelocity != sf::Vector2f()) {
			if (scrollVelocity.lengthSquared() < windowManager.scrollResistance * windowManager.scrollResistance) {
				scrollVelocity = sf::Vector2f();
			}
			else {
				scrollVelocity -= scrollVelocity
					.componentWiseDiv(sf::Vector2f(scrollVelocity.length(), scrollVelocity.length()))
					.componentWiseMul(sf::Vector2f(windowManager.scrollResistance, windowManager.scrollResistance));
			}
			scroll += scrollVelocity;
		}
		if (mouseDragScrollable != sf::Vector2i() || mouseWheelScrollable != sf::Vector2i())
			ensureScrollLimit();
		for (auto& elem : sub.iterate<AreaObject>()) {
			elem.updateScroll(windowManager);
		}
	}
}
