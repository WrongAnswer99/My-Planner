#pragma once
#include "SFML/Graphics.hpp"
#include <string>
#include <unordered_map>
#include <filesystem>
#include "engine/resource/Resources.hpp"

const std::filesystem::path defaultFontDir = "resources/fonts";

class FontManager {
private:
	std::string defaultFontName = "default";
	std::unordered_map<std::string, sf::Font>font;
	//noncopyable
	FontManager(const FontManager& other) = delete;
	FontManager& operator=(const FontManager& _f) = delete;
public:
	FontManager() {};
	FontManager(std::string defaultFontName) : defaultFontName(defaultFontName) {};
	std::string getDefaultFontName() {
		return defaultFontName;
	}
	void setDefaultFontName(std::string defaultFontName) {
		this->defaultFontName = defaultFontName;
	}
	void loadAuto(){
		auto files = listFiles(defaultFontDir);
		for (const auto& file : files) {
			loadFont(file);
		}
	}
	bool loadFont(const std::filesystem::path& relativePath) {
		auto fullPath = defaultFontDir / relativePath;
		std::string name = relativePath.filename().stem().string();
		if (font.count(name)) {
			return false;
		}
		return font[name].openFromFile(fullPath.string());
	}
	sf::Font& operator[](const std::string& name) {
		if (font.count(name)) {
			return font[name];
		}
		return font.at(defaultFontName);
	}
};
inline FontManager fontManager("default");
