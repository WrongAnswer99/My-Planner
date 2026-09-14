#pragma once
#include <fstream>
#include <type_traits>
#include <cstdint>
#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include <utility>
#include <iostream>
#include <filesystem>

class BinaryFileStream;

// Helper functions to dispatch to free functions from member functions
// 辅助函数，用于从成员函数转发调用到自由函数
// Uses dependent name lookup - read/write are resolved at instantiation time via ADL
// 使用依赖名查找机制 —— read/write 函数在实例化阶段通过ADL实参依赖查找完成解析
template<typename T>
inline void dispatch_read_reference(BinaryFileStream& bf, T& x) {
	read(bf, x);
}
template<typename T>
inline void dispatch_read_wrapper(BinaryFileStream& bf, T x) {
	read(bf, x);
}
template<typename T>
inline void dispatch_write(BinaryFileStream& bf, const T& x) {
	write(bf, x);
}

//二进制文件输入输出流
//主流平台上大小均一致的类型才能正常序列化
//支持struct二进制序列化
class BinaryFileStream {
private:
	std::ifstream fileIn;
	std::ofstream fileOut;
	std::filesystem::path filename;
	char mode;
	enum modeEnum{
		None,
		InMode,
		OutMode
	};
	bool isFileInGood = true;

	template<typename T>
	static constexpr bool isSafeType =
		std::is_same_v<T, bool> ||
		std::is_same_v<T, char> || std::is_same_v<T, unsigned char> ||
		std::is_same_v<T, short> || std::is_same_v<T, unsigned short> ||
		std::is_same_v<T, int> || std::is_same_v<T, unsigned int> ||
		std::is_same_v<T, long long> || std::is_same_v<T, unsigned long long> ||
		std::is_same_v<T, float> || std::is_same_v<T, double> ||
		std::is_same_v<T, char8_t> || std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t> ||
		std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t> ||
		std::is_same_v<T, int8_t> || std::is_same_v<T, int16_t> || std::is_same_v<T, int32_t> || std::is_same_v<T, int64_t>;

	inline void ensureMode(modeEnum _mode){
		if (_mode==mode)return;
		if (_mode==InMode){
			if (fileOut.is_open()) {
				fileOut.close();
			}
			if (!fileIn.is_open()) {
				fileIn.open(filename, std::ios::binary);
				isFileInGood = true;
			}
		}
		if (_mode==OutMode){
			if (fileIn.is_open()) {
				fileIn.close();
				isFileInGood = true;
			}
			if (!fileOut.is_open()) {
				fileOut.open(filename, std::ios::binary | std::ios::app);
			}
		}
		this->mode=_mode;
	}

	template<typename T, typename...Args>
	[[deprecated("Use read() instead")]] inline void readStructHelper(int index, int indexRead, T&& x, Args&& ...args) {
		if (indexRead == 0)
			this->read(indexRead);
		if (indexRead == 0)
			return;
		//indexRead!=0
		if (index == indexRead) {
			this->read(std::forward<T>(x));
			readStructHelper(index + 1, 0, std::forward<Args>(args)...);
		}
		else {
			readStructHelper(index + 1, indexRead, std::forward<Args>(args)...);
		}
	}

	template<typename T>
	[[deprecated("Use read() instead")]] inline void readStructHelper(int index, int indexRead, T&& x) {
		if (indexRead == 0)
			this->read(indexRead);
		if (indexRead == 0)
			return;
		//indexRead!=0
		if (index == indexRead)
			this->read(std::forward<T>(x));
		this->read(indexRead);
		if (indexRead == 0)
			return;
		else {
			isFileInGood = false;// 旧程序遇到新字段时标记错误
		}
	}

	template<typename T, typename...Args>
	[[deprecated("Use write() instead")]] inline void writeStructHelper(int index, T&& x, Args&& ...args) {
		this->write(index,std::forward<T>(x));
		writeStructHelper(index + 1, std::forward<Args>(args)...);
	}

	template<typename T>
	[[deprecated("Use write() instead")]] inline void writeStructHelper(int index, T&& x) {
		this->write(index,std::forward<T>(x),0);
		//0是占位符,表示当前struct结束,用于嵌套struct
	}

public:
	BinaryFileStream(const std::filesystem::path& filename) :filename(filename) {}
	void setFile(const std::filesystem::path& filename){
		close();
		this->filename = filename;
	}
	//外部定义函数读取
	//使用 if constexpr 延迟检测，避免特化顺序问题
	//左值走 reference 路径，右值（wrapper临时对象）走 wrapper 路径
	template<typename T>
	inline void read(T&& x) {
		if constexpr(isSafeType<std::decay_t<T>>) {
			ensureMode(InMode);
			if (fileIn.good() && isFileInGood)
				fileIn.read(reinterpret_cast<char*>(&x), sizeof(x));
		}
		else if constexpr(std::is_same_v<std::decay_t<T>, size_t>) {
			ensureMode(InMode);
			if (fileIn.good() && isFileInGood){
				unsigned long long ullx;
				fileIn.read(reinterpret_cast<char*>(&ullx), sizeof(ullx));
				x = static_cast<size_t>(ullx);
			}
		}
		else if constexpr(std::is_same_v<std::decay_t<T>, std::string>) {
			ensureMode(InMode);
			if (fileIn.good() && isFileInGood){
				size_t size;
				this->read(size);
				x.resize(size);
				fileIn.read(&x[0], size);
			}
		}
		else if constexpr(std::is_lvalue_reference_v<T&&>) {
			dispatch_read_reference(*this, x);
		}
		else {
			dispatch_read_wrapper(*this, std::forward<T>(x));
		}
	}

	//普通批量读取
	//若读取大型结构体，建议使用readStruct
	//readStruct会自动处理struct嵌套
	template<typename T,typename U,typename ...Args>
	inline void read(T&& t,U&& u,Args&&...args){
		this->read(std::forward<T>(t));
		this->read(std::forward<U>(u));
		if constexpr(sizeof...(Args)>0)
			this->read(std::forward<Args>(args)...);
	}

	//外部定义函数写入
	//只需要类型在外部有void write(BinaryFileStream&,const T&)函数，即可自动调用
	//使用 if constexpr 延迟检测，避免特化顺序问题
	template<typename T>
	inline void write(T&& x) {
		if constexpr(isSafeType<std::decay_t<T>>) {
			ensureMode(OutMode);
			fileOut.write(reinterpret_cast<const char*>(&x), sizeof(x));
		}
		else if constexpr(std::is_same_v<std::decay_t<T>, size_t>) {
			ensureMode(OutMode);
			const unsigned long long ullx = static_cast<const unsigned long long>(x);
			fileOut.write(reinterpret_cast<const char*>(&ullx), sizeof(ullx));
		}
		else if constexpr(std::is_same_v<std::decay_t<T>, std::string>) {
			ensureMode(OutMode);
			this->write(x.size());
			fileOut.write(x.data(), x.size());
		}
		else {
			dispatch_write(*this, x);
		}
	}

	//普通批量写入
	//若写入大型结构体，建议使用writeStruct
	//writeStruct会自动处理struct嵌套
	template<typename T,typename U,typename ...Args>
	inline void write(T&& t, U&& u, Args&&...args){
		this->write(std::forward<T>(t));
		this->write(std::forward<U>(u));
		if constexpr(sizeof...(Args)>0)
			this->write(std::forward<Args>(args)...);
	}

	//关闭文件输入与输出
	inline void close() {
		ensureMode(None);
		if (fileIn.is_open())
			fileIn.close();
		if (fileOut.is_open())
			fileOut.close();
	}

	//清空文件内容
	//这个函数不会关闭文件输出,需使用close()关闭
	inline void clear() {
		close();
		fileOut.open(filename, std::ios::binary | std::ios::trunc);
		mode = OutMode;
	}

	//文件输入或输出是否良好
	//当未打开文件时返回false
	inline bool good() {
		if (fileIn.is_open())
			return fileIn.good() && isFileInGood;
		if (fileOut.is_open())
			return fileOut.good();
		return false;
	}

	template <typename ...Args>
	[[deprecated("Use read() instead")]] inline void readStruct(Args&&...args) {
		readStructHelper(1, 0, std::forward<Args>(args)...);
	}

	template <typename ...Args>
	[[deprecated("Use write() instead")]] inline void writeStruct(Args&&...args) {
		writeStructHelper(1, std::forward<Args>(args)...);
	}

	//仅作为语法兼容，外部仍需要写friend read()和friend write()
	template<typename T>
	inline BinaryFileStream& operator>>(T& x) {
		read(x);
		return *this;
	}

	//仅作为语法兼容，外部仍需要写friend read()和friend write()
	template<typename T>
	inline BinaryFileStream& operator<<(const T& x) {
		write(x);
		return *this;
	}
};

// Standard container serialization
template<typename T, typename U>
inline void read(BinaryFileStream& bf, std::pair<T, U>& x) {
	bf.read(x.first,x.second);
}
template<typename T, typename U>
inline void write(BinaryFileStream& bf, const std::pair<T, U>& x) {
	bf.write(x.first,x.second);
}
template<typename T, typename U>
inline void read(BinaryFileStream& bf, std::unordered_map<T, U>& x) {
	size_t size;
	bf.read(size);
	x.clear();
	x.reserve(size);
	T t{};
	U u{};
	for (size_t i = 0; i < size; ++i) {
		bf.read(t,u);
		x.emplace(std::move(t), std::move(u));
		t = T{};
		u = U{};
	}
}
template<typename T, typename U>
inline void write(BinaryFileStream& bf, const std::unordered_map<T, U>& x) {
	bf.write(x.size());
	for (auto& elem : x) {
		bf.write(elem.first,elem.second);
	}
}
template<typename T, typename U>
inline void read(BinaryFileStream& bf, std::map<T, U>& x) {
	size_t size;
	bf.read(size);
	x.clear();
	x.reserve(size);
	T t{};
	U u{};
	for (size_t i = 0; i < size; ++i) {
		bf.read(t,u);
		x.emplace(std::move(t), std::move(u));
		t = T{};
		u = U{};
	}
}
template<typename T, typename U>
inline void write(BinaryFileStream& bf, const std::map<T, U>& x) {
	bf.write(x.size());
	for (auto& elem : x) {
		bf.write(elem.first,elem.second);
	}
}
template<typename T>
inline void read(BinaryFileStream& bf, std::vector<T>& x) {
	size_t size;
	bf.read(size);
	T t{};
	x.clear();
	x.reserve(size);
	for (size_t i = 0; i < size; ++i) {
		bf.read(t);
		x.emplace_back(std::move(t));
		t = T{};
	}
}
template<typename T>
inline void write(BinaryFileStream& bf, const std::vector<T>& x) {
	bf.write(x.size());
	for (auto& elem : x) {
		bf.write(elem);
	}
}

// Fixed-size array serialization
template<typename T, size_t N>
inline void read(BinaryFileStream& bf, T (&x)[N]) {
	for (size_t i = 0; i < N; ++i) {
		bf.read(x[i]);
	}
}
template<typename T, size_t N>
inline void write(BinaryFileStream& bf, const T (&x)[N]) {
	for (size_t i = 0; i < N; ++i) {
		bf.write(x[i]);
	}
}

// SFML type serialization
#include "SFML/Graphics.hpp"

inline void read(BinaryFileStream& bf, sf::Color& x) {
	bf.read(x.r,x.g,x.b,x.a);
}
inline void write(BinaryFileStream& bf, const sf::Color& x) {
	bf.write(x.r,x.g,x.b,x.a);
}
template<typename T>
inline void read(BinaryFileStream& bf, sf::Vector2<T>& x) {
	bf.read(x.x,x.y);
}
template<typename T>
inline void write(BinaryFileStream& bf, const sf::Vector2<T>& x) {
	bf.write(x.x,x.y);
}
template<typename T>
inline void read(BinaryFileStream& bf, sf::Rect<T>& x) {
	bf.read(x.position,x.size);
}
template<typename T>
inline void write(BinaryFileStream& bf, const sf::Rect<T>& x) {
	bf.write(x.position,x.size);
}
inline void read(BinaryFileStream& bf, sf::String& x) {
	std::string s;
	bf.read(s);
	x = sf::String::fromUtf8(s.begin(), s.end());
}
inline void write(BinaryFileStream& bf, const sf::String& x) {
	auto utf8 = x.toUtf8();
	bf.write(utf8.size());
	for (auto& elem : utf8)
		bf.write(elem);
}
inline void read(BinaryFileStream& bf, sf::Image& x) {
	std::string s;
	bf.read(s);
	if (!x.loadFromMemory(s.data(), s.size())) {
		throw std::runtime_error("[BinaryFileStream] Image Read Failed");
	}
}
inline void write(BinaryFileStream& bf, const sf::Image& x) {
	bf.write(*x.saveToMemory("png"));
}
inline void read(BinaryFileStream& bf, sf::Texture& x) {
	std::string s;
	bf.read(s);
	if (!x.loadFromImage(sf::Image(s.data(), s.size()))) {
		throw std::runtime_error("[BinaryFileStream] Texture Read Failed");
	}
}
inline void write(BinaryFileStream& bf, const sf::Texture& x) {
	bf.write(*x.copyToImage().saveToMemory("png"));
}

// VarianTmap serialization
#include "engine/data/VarianTmap.hpp"

template<typename Base,typename ...T>
class VarianTmapBinarySerializerWrapper {
	VarianTmap<Base>* data;
public:
	VarianTmapBinarySerializerWrapper(VarianTmap<Base>& data) : data(&data) {}
	VarianTmapBinarySerializerWrapper(const VarianTmap<Base>& data) : data(const_cast<VarianTmap<Base>*>(&data)) {}

	friend void read(BinaryFileStream& bf, VarianTmapBinarySerializerWrapper<Base,T...> x) {
		if constexpr (sizeof...(T) == 0)return;
		x.data->clear();
		size_t TypeOrderIndex = 0;
		std::unordered_map<size_t,std::type_index> TypeOrderIndexMap;
		(TypeOrderIndexMap.emplace(TypeOrderIndex++, std::type_index(typeid(T))),...);
		std::unordered_map<std::type_index, std::function<void(decltype(bf))>> readHelper;
		((readHelper[std::type_index(typeid(T))] = [&](auto& bf) {
			std::string key;
			bf.read(key);
			T* pointer=x.data->template push_back<T>(key,T{});
			bf.read(*pointer);
		}),...);
		size_t size;
		bf.read(size);
		for (size_t i = 0; i < size; i++){
			size_t TypeIndex;
			bf.read(TypeIndex);
			readHelper[TypeOrderIndexMap.at(TypeIndex)](bf);
		}
	}
	friend void write(BinaryFileStream& bf, const VarianTmapBinarySerializerWrapper<Base,T...>& x) {
		if constexpr (sizeof...(T) == 0)return;
		size_t TypeOrderIndex = 0;
		std::unordered_map<std::type_index, size_t> TypeOrderIndexMap;
		((TypeOrderIndexMap.emplace(std::type_index(typeid(T)), TypeOrderIndex++)),...);
		std::unordered_map<std::type_index, std::function<void(decltype(bf),Base*)>> writeHelper;
		((writeHelper[std::type_index(typeid(T))] = [&](auto& bf, Base* BasePointer) {
			bf.write(TypeOrderIndexMap[std::type_index(typeid(T))],x.data->find_key(BasePointer),*static_cast<T*>(BasePointer));
		}),...);
		size_t size=0;
		for (auto& elem : x.data->order()){
			if (writeHelper.find(x.data->find_type_index(elem)) != writeHelper.end())
				size++;
		}
		bf.write(size);
		for (auto& elem : x.data->order()) {
			if (writeHelper.find(x.data->find_type_index(elem)) != writeHelper.end())
				writeHelper[x.data->find_type_index(elem)](bf, elem);
		}
	}
};
#include "engine/gui/MyGUI.hpp"

namespace gui {

// 类型别名，避免重复书写长类型
using GUIvarianTmapBinarySerializer = VarianTmapBinarySerializerWrapper<UIBase, AreaObject, ImageObject, TextObject, InputObject, ButtonObject, OptionObject>;

// Style（公开成员直接访问）
inline void read(BinaryFileStream& bf, Style& x) {
	bf.read(x.backgroundColor, x.outlineColor, x.outlineThickness);
}
inline void write(BinaryFileStream& bf, const Style& x) {
	bf.write(x.backgroundColor, x.outlineColor, x.outlineThickness);
}

// DynamicPosition（getAnchor/getRelative 组装成 char type 后序列化）
inline void read(BinaryFileStream& bf, UIBase::DynamicPosition& x) {
	char type;
	float value;
	bf.read(type, value);
	int anchor = type >> 2;
	int relative = type & 0b000011;
	x.setAnchor(static_cast<UIBase::Anchor>(anchor));
	x.setRelative(static_cast<UIBase::Relative>(relative));
	x.setValue(value);
}
inline void write(BinaryFileStream& bf, const UIBase::DynamicPosition& x) {
	char type = static_cast<char>((x.getAnchor() << 2) | x.getRelative());
	bf.write(type, x.getValue());
}

// UIBase（protected 成员，使用 getter/setter）
inline void read(BinaryFileStream& bf, UIBase& x) {
	sf::Vector2<std::pair<UIBase::DynamicPosition, UIBase::DynamicPosition>> pos;
	Style styles[3];
	bool isShow;
	bf.read(pos, styles, isShow);
	x.setPositionRelative({pos.x.first, pos.x.second}, {pos.y.first, pos.y.second});
	x.setStyle(styles[0], styles[1], styles[2]);
	x.setShow(isShow);
}
inline void write(BinaryFileStream& bf, const UIBase& x) {
	bf.write(x.getDynamicPosition(), x.getStyle(0), x.getStyle(1), x.getStyle(2), x.getShow());
}

// ImageObject（protected 成员，使用 getter/setter）
inline void read(BinaryFileStream& bf, ImageObject& x) {
	read(bf, static_cast<UIBase&>(x));
	std::string imageId;
	sf::Vector2f scale;
	sf::Vector2i align;
	sf::Color imageColors[3];
	bf.read(imageId, scale, align, imageColors);
	x.setImageId(imageId);
	x.setScale(scale);
	x.setAlign(static_cast<UIBase::Align>(align.x), static_cast<UIBase::Align>(align.y));
	x.setImageColor(imageColors[0], imageColors[1], imageColors[2]);
}
inline void write(BinaryFileStream& bf, const ImageObject& x) {
	write(bf, static_cast<const UIBase&>(x));
	bf.write(x.getImageId(), x.getScale(), static_cast<sf::Vector2i>(x.getAlign()),
		x.getImageColor(0), x.getImageColor(1), x.getImageColor(2));
}

// TextStyle（公开成员直接访问）
inline void read(BinaryFileStream& bf, TextStyle& x) {
	bf.read(x.fillColor, x.outlineColor);
}
inline void write(BinaryFileStream& bf, const TextStyle& x) {
	bf.write(x.fillColor, x.outlineColor);
}

// TextObject（protected 成员，使用 getter/setter）
inline void read(BinaryFileStream& bf, TextObject& x) {
	read(bf, static_cast<UIBase&>(x));
	TextStyle textStyles[3];
	std::string font;
	unsigned int characterSize;
	sf::Vector2i align;
	float letterSpacing, lineSpacing;
	sf::String text;
	bf.read(textStyles, font, characterSize, align, letterSpacing, lineSpacing, text);
	x.setTextStyle(textStyles[0], textStyles[1], textStyles[2]);
	x.setFont(font);
	x.setCharacterSize(characterSize);
	x.setAlign(static_cast<UIBase::Align>(align.x), static_cast<UIBase::Align>(align.y));
	x.setSpacing(letterSpacing, lineSpacing);
	x.setText(text);
}
inline void write(BinaryFileStream& bf, const TextObject& x) {
	write(bf, static_cast<const UIBase&>(x));
	bf.write(x.getTextStyle(0), x.getTextStyle(1), x.getTextStyle(2), x.getFont(),
		x.getCharacterSize(), x.getAlign(), x.getLetterSpacing(), x.getLineSpacing(), x.getText());
}

// InputLimit（private 成员，使用 getter）
inline void read(BinaryFileStream& bf, InputObject::InputLimit& x) {
	bool isAllowList;
	std::vector<char32_t> single;
	std::vector<std::pair<char32_t, char32_t>> range;
	bf.read(isAllowList, single, range);
	x.setIsAllowList(isAllowList);
	x.setSingle(single);
	x.setRange(range);
}
inline void write(BinaryFileStream& bf, const InputObject::InputLimit& x) {
	bf.write(x.getIsAllowList(), x.getSingle(), x.getRange());
}

// InputObject
// 序列化逻辑：inputLimit 仅当 typeLimit == String 时才写入/读取
// 写入时：先写 sizeLimit 和 typeLimit，若 typeLimit == String 则额外写 inputLimit
// 读取时：先读 sizeLimit 和 typeLimit，若 typeLimit == String 则额外读 inputLimit 并通过 setStringTypeLimit 设置
inline void read(BinaryFileStream& bf, InputObject& x) {
	read(bf, static_cast<TextObject&>(x));
	size_t sizeLimit;
	int typeLimit;
	bf.read(sizeLimit, typeLimit);
	x.setSizeLimit(sizeLimit);
	x.setTypeLimit(static_cast<InputObject::InputType>(typeLimit));
	if (typeLimit == InputObject::String) {
		InputObject::InputLimit inputLimit;
		bf.read(inputLimit);
		x.setStringTypeLimit(inputLimit.getIsAllowList(), inputLimit.getSingle(), inputLimit.getRange());
	}
	x.setText(x.getText());
}
inline void write(BinaryFileStream& bf, const InputObject& x) {
	write(bf, static_cast<const TextObject&>(x));
	bf.write(x.getSizeLimit(), x.getTypeLimit());
	if (x.getTypeLimit() == InputObject::String) {
		bf.write(*x.getStringTypeLimit());
	}
}

// AreaObject（protected 成员使用 getter/setter，sub 公开）
inline void read(BinaryFileStream& bf, AreaObject& x) {
	read(bf, static_cast<UIBase&>(x));
	GUIvarianTmapBinarySerializer subWrapper{x.sub};
	sf::Vector2i mouseDragScrollable, mouseWheelScrollable;
	std::string option;
	bf.read(subWrapper, mouseDragScrollable, mouseWheelScrollable, option);
	x.setScrollable(mouseDragScrollable, mouseWheelScrollable);
	x.setOption(option);
}
inline void write(BinaryFileStream& bf, const AreaObject& x) {
	write(bf, static_cast<const UIBase&>(x));
	bf.write(GUIvarianTmapBinarySerializer{const_cast<VarianTmap<UIBase>&>(x.sub)},
		x.getMouseDragScrollable(), x.getMouseWheelScrollable(), x.getOption());
}

// ButtonObject（无新增成员，委托给 TextObject）
inline void read(BinaryFileStream& bf, ButtonObject& x) {
	read(bf, static_cast<TextObject&>(x));
}
inline void write(BinaryFileStream& bf, const ButtonObject& x) {
	write(bf, static_cast<const TextObject&>(x));
}

// OptionObject（无新增成员，委托给 ButtonObject）
inline void read(BinaryFileStream& bf, OptionObject& x) {
	read(bf, static_cast<ButtonObject&>(x));
}
inline void write(BinaryFileStream& bf, const OptionObject& x) {
	write(bf, static_cast<const ButtonObject&>(x));
}

} // namespace gui
