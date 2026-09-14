#pragma once
#include <nlohmann/json.hpp>
#include "SFML/Graphics.hpp"
#include "engine/data/VarianTmap.hpp"

// Fixed-size array JSON serialization - MUST be defined before any other specialization
// that might trigger instantiation of array types like char[2]
namespace nlohmann {
	template<typename T, size_t N>
	struct adl_serializer<T[N], std::enable_if_t<std::is_class_v<T>>> {
		static void to_json(json& j, const T (&x)[N]) {
			j = json::array();
			for (size_t i = 0; i < N; i++) {
				j.push_back(x[i]);
			}
		}
		static void from_json(const json& j, T (&x)[N]) {
			for (size_t i = 0; i < N; i++) {
				j.at(i).get_to(x[i]);
			}
		}
	};
}

// SFML type JSON serialization using adl_serializer specialization
// This avoids injecting into the sf namespace
namespace nlohmann {
	template<>
	struct adl_serializer<sf::Color> {
		static void to_json(json& j, const sf::Color& x) {
			j = json{ {"r", x.r}, {"g", x.g}, {"b", x.b}, {"a", x.a} };
		}
		static void from_json(const json& j, sf::Color& x) {
			j.at("r").get_to(x.r);
			j.at("g").get_to(x.g);
			j.at("b").get_to(x.b);
			j.at("a").get_to(x.a);
		}
	};

	template<typename T>
	struct adl_serializer<sf::Vector2<T>> {
		static void to_json(json& j, const sf::Vector2<T>& x) {
			j = json{ {"x", x.x}, {"y", x.y} };
		}
		static void from_json(const json& j, sf::Vector2<T>& x) {
			j.at("x").get_to(x.x);
			j.at("y").get_to(x.y);
		}
	};

	template<typename T>
	struct adl_serializer<sf::Rect<T>> {
		static void to_json(json& j, const sf::Rect<T>& x) {
			j = json{ {"position", x.position}, {"size", x.size} };
		}
		static void from_json(const json& j, sf::Rect<T>& x) {
			j.at("position").get_to(x.position);
			j.at("size").get_to(x.size);
		}
	};

	template<>
	struct adl_serializer<sf::String> {
		static void to_json(json& j, const sf::String& x) {
			auto utf8 = x.toUtf8();
			j = std::string(utf8.begin(), utf8.end());
		}
		static void from_json(const json& j, sf::String& x) {
			std::string s = j.get<std::string>();
			x = sf::String::fromUtf8(s.begin(), s.end());
		}
	};

	template<>
	struct adl_serializer<sf::Image> {
		static void to_json(json& j, const sf::Image& x) {
			auto memory = x.saveToMemory("png");
			if (!memory) {
				throw std::runtime_error("[JsonExtensions] Image saveToMemory failed");
			}
			j = json::binary(*memory);
		}
		static void from_json(const json& j, sf::Image& x) {
			auto data = j.get_binary();
			if (!x.loadFromMemory(data.data(), data.size())) {
				throw std::runtime_error("[JsonExtensions] Image from_json failed");
			}
		}
	};

	template<>
	struct adl_serializer<sf::Texture> {
		static void to_json(json& j, const sf::Texture& x) {
			auto memory = x.copyToImage().saveToMemory("png");
			if (!memory) {
				throw std::runtime_error("[JsonExtensions] Texture saveToMemory failed");
			}
			j = json::binary(*memory);
		}
		static void from_json(const json& j, sf::Texture& x) {
			auto data = j.get_binary();
			sf::Image img;
			if (!img.loadFromMemory(data.data(), data.size())) {
				throw std::runtime_error("[JsonExtensions] Texture from_json failed (image load)");
			}
			if (!x.loadFromImage(img)) {
				throw std::runtime_error("[JsonExtensions] Texture from_json failed (texture load)");
			}
		}
	};
}

// VarianTmap JSON serialization
// 参照 BinaryFileStream 的 VarianTmapSerializerWrapper read/write 逻辑
// 使用 TypeOrderIndex（类型序号）而非 type_index，保证跨平台一致性
template<typename Base, typename ...T>
class VarianTmapJsonSerializerWrapper {
	VarianTmap<Base>* data;
public:
	VarianTmapJsonSerializerWrapper(VarianTmap<Base>& data) : data(&data) {}
	VarianTmapJsonSerializerWrapper(const VarianTmap<Base>& data) : data(const_cast<VarianTmap<Base>*>(&data)) {}

	friend void to_json(nlohmann::json& j, const VarianTmapJsonSerializerWrapper<Base, T...>& x) {
		if constexpr (sizeof...(T) == 0) { j = nlohmann::json::array(); return; }
		size_t TypeOrderIndex = 0;
		std::unordered_map<std::type_index, size_t> TypeOrderIndexMap;
		((TypeOrderIndexMap.emplace(std::type_index(typeid(T)), TypeOrderIndex++)), ...);
		std::unordered_map<std::type_index, std::function<void(nlohmann::json&, Base*)>> writeHelper;
		((writeHelper[std::type_index(typeid(T))] = [&](nlohmann::json& j, Base* BasePointer) {
			j.push_back(nlohmann::json{
				{"type_index", TypeOrderIndexMap[std::type_index(typeid(T))]},
				{"!type_name", typeid(T).name()},
				{"key", x.data->find_key(BasePointer)},
				{"value", *static_cast<T*>(BasePointer)}
			});
		}), ...);
		j = nlohmann::json::array();
		for (auto& elem : x.data->order()) {
			if (writeHelper.find(x.data->find_type_index(elem)) != writeHelper.end())
				writeHelper[x.data->find_type_index(elem)](j, elem);
		}
	}

	friend void from_json(const nlohmann::json& j, VarianTmapJsonSerializerWrapper<Base, T...> x) {
		if constexpr (sizeof...(T) == 0) return;
		x.data->clear();
		size_t TypeOrderIndex = 0;
		std::unordered_map<size_t, std::type_index> TypeOrderIndexMap;
		(TypeOrderIndexMap.emplace(TypeOrderIndex++, std::type_index(typeid(T))), ...);
		std::unordered_map<std::type_index, std::function<void(const nlohmann::json&)>> readHelper;
		((readHelper[std::type_index(typeid(T))] = [&](const nlohmann::json& elem) {
			std::string key;
			elem.at("key").get_to(key);
			T* pointer = x.data->template push_back<T>(key, T{});
			elem.at("value").get_to(*pointer);
		}), ...);
		for (const auto& elem : j) {
			size_t TypeIndex;
			elem.at("type_index").get_to(TypeIndex);
			readHelper[TypeOrderIndexMap.at(TypeIndex)](elem);
		}
	}
};

#include "engine/gui/MyGUI.hpp"

// 类型别名，避免重复书写长类型
namespace gui {
using GUIvarianTmapJsonSerializer = VarianTmapJsonSerializerWrapper<UIBase, AreaObject, ImageObject, TextObject, InputObject, ButtonObject, OptionObject>;
}
namespace nlohmann {

// Style（公开成员直接访问）
template<>
struct adl_serializer<gui::Style> {
	static void to_json(json& j, const gui::Style& x) {
		j = json{ {"backgroundColor", x.backgroundColor}, {"outlineColor", x.outlineColor}, {"outlineThickness", x.outlineThickness} };
	}
	static void from_json(const json& j, gui::Style& x) {
		j.at("backgroundColor").get_to(x.backgroundColor);
		j.at("outlineColor").get_to(x.outlineColor);
		j.at("outlineThickness").get_to(x.outlineThickness);
	}
};

// DynamicPosition（type 拆为 anchor + relative 分别存储）
template<>
struct adl_serializer<gui::UIBase::DynamicPosition> {
	static void to_json(json& j, const gui::UIBase::DynamicPosition& x) {
		j = json{ {"anchor", x.getAnchor()}, {"relative", x.getRelative()}, {"value", x.getValue()} };
	}
	static void from_json(const json& j, gui::UIBase::DynamicPosition& x) {
		int anchor, relative;
		float value;
		j.at("anchor").get_to(anchor);
		j.at("relative").get_to(relative);
		j.at("value").get_to(value);
		x.setAnchor(static_cast<gui::UIBase::Anchor>(anchor));
		x.setRelative(static_cast<gui::UIBase::Relative>(relative));
		x.setValue(value);
	}
};

// UIBase（protected 成员，使用 getter/setter）
template<>
struct adl_serializer<gui::UIBase> {
	static void to_json(json& j, const gui::UIBase& x) {
		j = json{ {"position", x.getDynamicPosition()}, {"styles", {x.getStyle(0), x.getStyle(1), x.getStyle(2)}}, {"isShow", x.getShow()} };
	}
	static void from_json(const json& j, gui::UIBase& x) {
		sf::Vector2<std::pair<gui::UIBase::DynamicPosition, gui::UIBase::DynamicPosition>> pos;
		std::array<gui::Style, 3> styles;
		bool isShow;
		j.at("position").get_to(pos);
		j.at("styles").get_to(styles);
		j.at("isShow").get_to(isShow);
		x.setPositionRelative({pos.x.first, pos.x.second}, {pos.y.first, pos.y.second});
		x.setStyle(styles[0], styles[1], styles[2]);
		x.setShow(isShow);
	}
};

// ImageObject（protected 成员，使用 getter/setter）
template<>
struct adl_serializer<gui::ImageObject> {
	static void to_json(json& j, const gui::ImageObject& x) {
		json base = static_cast<const gui::UIBase&>(x);
		j = json{ {"UIBase", base}, {"imageId", x.getImageId()}, {"scale", x.getScale()}, {"align", x.getAlign()}, {"imageColors", {x.getImageColor(0), x.getImageColor(1), x.getImageColor(2)}} };
	}
	static void from_json(const json& j, gui::ImageObject& x) {
		json base;
		j.at("UIBase").get_to(base);
		base.get_to(static_cast<gui::UIBase&>(x));
		std::string imageId;
		sf::Vector2f scale;
		sf::Vector2i align;
		std::array<sf::Color, 3> imageColors;
		j.at("imageId").get_to(imageId);
		j.at("scale").get_to(scale);
		j.at("align").get_to(align);
		j.at("imageColors").get_to(imageColors);
		x.setImageId(imageId);
		x.setScale(scale);
		x.setAlign(static_cast<gui::UIBase::Align>(align.x), static_cast<gui::UIBase::Align>(align.y));
		x.setImageColor(imageColors[0], imageColors[1], imageColors[2]);
	}
};

// TextStyle（公开成员直接访问）
template<>
struct adl_serializer<gui::TextStyle> {
	static void to_json(json& j, const gui::TextStyle& x) {
		j = json{ {"fillColor", x.fillColor}, {"outlineColor", x.outlineColor} };
	}
	static void from_json(const json& j, gui::TextStyle& x) {
		j.at("fillColor").get_to(x.fillColor);
		j.at("outlineColor").get_to(x.outlineColor);
	}
};

// TextObject（protected 成员，使用 getter/setter）
template<>
struct adl_serializer<gui::TextObject> {
	static void to_json(json& j, const gui::TextObject& x) {
		json base = static_cast<const gui::UIBase&>(x);
		j = json{ {"UIBase", base}, {"textStyles", {x.getTextStyle(0), x.getTextStyle(1), x.getTextStyle(2)}}, {"font", x.getFont()}, {"characterSize", x.getCharacterSize()}, {"align", x.getAlign()}, {"letterSpacing", x.getLetterSpacing()}, {"lineSpacing", x.getLineSpacing()}, {"text", x.getText()} };
	}
	static void from_json(const json& j, gui::TextObject& x) {
		json base;
		j.at("UIBase").get_to(base);
		base.get_to(static_cast<gui::UIBase&>(x));
		std::array<gui::TextStyle, 3> textStyles;
		std::string font;
		unsigned int characterSize;
		sf::Vector2i align;
		float letterSpacing, lineSpacing;
		sf::String text;
		j.at("textStyles").get_to(textStyles);
		j.at("font").get_to(font);
		j.at("characterSize").get_to(characterSize);
		j.at("align").get_to(align);
		j.at("letterSpacing").get_to(letterSpacing);
		j.at("lineSpacing").get_to(lineSpacing);
		j.at("text").get_to(text);
		x.setTextStyle(textStyles[0], textStyles[1], textStyles[2]);
		x.setFont(font);
		x.setCharacterSize(characterSize);
		x.setAlign(static_cast<gui::UIBase::Align>(align.x), static_cast<gui::UIBase::Align>(align.y));
		x.setSpacing(letterSpacing, lineSpacing);
		x.setText(text);
	}
};

// InputLimit（private 成员，使用 getter）
template<>
struct adl_serializer<gui::InputObject::InputLimit> {
	static void to_json(json& j, const gui::InputObject::InputLimit& x) {
		j = json{ {"isAllowList", x.getIsAllowList()}, {"single", x.getSingle()}, {"range", x.getRange()} };
	}
	static void from_json(const json& j, gui::InputObject::InputLimit& x) {
		bool isAllowList;
		std::vector<char32_t> single;
		std::vector<std::pair<char32_t, char32_t>> range;
		j.at("isAllowList").get_to(isAllowList);
		j.at("single").get_to(single);
		j.at("range").get_to(range);
		x.setIsAllowList(isAllowList);
		x.setSingle(single);
		x.setRange(range);
	}
};

// InputObject
// 序列化逻辑：inputLimit 仅当 typeLimit == String 时才写入有意义的输入限制
// 导出时：若 typeLimit == String 则输出实际的 inputLimit（通过 getStringTypeLimit），否则输出空构造的 InputLimit{}
// 导入时：先读 typeLimit，仅当 typeLimit == String 才赋值 inputLimit（通过 setStringTypeLimit）
template<>
struct adl_serializer<gui::InputObject> {
	static void to_json(json& j, const gui::InputObject& x) {
		json base = static_cast<const gui::TextObject&>(x);
		if (x.getTypeLimit() == gui::InputObject::String) {
			j = json{ {"TextObject", base}, {"sizeLimit", x.getSizeLimit()}, {"typeLimit", x.getTypeLimit()}, {"inputLimit", *x.getStringTypeLimit()} };
		} else {
			j = json{ {"TextObject", base}, {"sizeLimit", x.getSizeLimit()}, {"typeLimit", x.getTypeLimit()}, {"inputLimit", gui::InputObject::InputLimit{}} };
		}
	}
	static void from_json(const json& j, gui::InputObject& x) {
		json base;
		j.at("TextObject").get_to(base);
		base.get_to(static_cast<gui::TextObject&>(x));
		size_t sizeLimit;
		int typeLimit;
		j.at("sizeLimit").get_to(sizeLimit);
		j.at("typeLimit").get_to(typeLimit);
		x.setSizeLimit(sizeLimit);
		x.setTypeLimit(static_cast<gui::InputObject::InputType>(typeLimit));
		if (typeLimit == gui::InputObject::String) {
			gui::InputObject::InputLimit inputLimit;
			j.at("inputLimit").get_to(inputLimit);
			x.setStringTypeLimit(inputLimit.getIsAllowList(), inputLimit.getSingle(), inputLimit.getRange());
		}
		x.setText(x.getText());
	}
};

// ButtonObject（无新增成员，委托给 TextObject）
template<>
struct adl_serializer<gui::ButtonObject> {
	static void to_json(json& j, const gui::ButtonObject& x) {
		j = static_cast<const gui::TextObject&>(x);
	}
	static void from_json(const json& j, gui::ButtonObject& x) {
		j.get_to(static_cast<gui::TextObject&>(x));
	}
};

// OptionObject（无新增成员，委托给 ButtonObject）
template<>
struct adl_serializer<gui::OptionObject> {
	static void to_json(json& j, const gui::OptionObject& x) {
		j = static_cast<const gui::ButtonObject&>(x);
	}
	static void from_json(const json& j, gui::OptionObject& x) {
		j.get_to(static_cast<gui::ButtonObject&>(x));
	}
};

// AreaObject（protected 成员使用 getter/setter，sub 公开）
template<>
struct adl_serializer<gui::AreaObject> {
	static void to_json(json& j, const gui::AreaObject& x) {
		json base = static_cast<const gui::UIBase&>(x);
		j = json{ {"UIBase", base}, {"sub", gui::GUIvarianTmapJsonSerializer{const_cast<VarianTmap<gui::UIBase>&>(x.sub)}}, {"mouseDragScrollable", x.getMouseDragScrollable()}, {"mouseWheelScrollable", x.getMouseWheelScrollable()}, {"option", x.getOption()} };
	}
	static void from_json(const json& j, gui::AreaObject& x) {
		json base;
		j.at("UIBase").get_to(base);
		base.get_to(static_cast<gui::UIBase&>(x));
		auto subWrapper = gui::GUIvarianTmapJsonSerializer{x.sub};
		j.at("sub").get_to(subWrapper);
		sf::Vector2i mouseDragScrollable, mouseWheelScrollable;
		std::string option;
		j.at("mouseDragScrollable").get_to(mouseDragScrollable);
		j.at("mouseWheelScrollable").get_to(mouseWheelScrollable);
		j.at("option").get_to(option);
		x.setScrollable(mouseDragScrollable, mouseWheelScrollable);
		x.setOption(option);
	}
};

}
