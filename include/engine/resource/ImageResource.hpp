#pragma once
#include "SFML/Graphics.hpp"
#include <string>
#include <unordered_map>
#include <filesystem>
#include "engine/resource/Resources.hpp"

const std::filesystem::path defaultImageDir = "resources/images";

//存储图片Texture
class ImageManager {
private:
	std::string defaultImageName = "default";
	std::unordered_map<std::string, sf::Texture>image;
	//noncopyable
	ImageManager(const ImageManager& other) = delete;
	ImageManager& operator=(const ImageManager& _f) = delete;
public:
	ImageManager() {};
	ImageManager(std::string defaultImageName) : defaultImageName(defaultImageName) {};
	std::string getDefaultImageName() {
		return defaultImageName;
	}
	void setDefaultImageName(std::string defaultImageName) {
		this->defaultImageName = defaultImageName;
	}
	void loadAuto(){
		auto files = listFiles(defaultImageDir);
		for (const auto& file : files) {
			loadImage(file);
		}
	}
	bool loadImage(const std::filesystem::path& relativePath) {
		auto fullPath = defaultImageDir / relativePath;
		std::string name = relativePath.filename().stem().string();
		if (image.count(name)) {
			return false;
		}
		return image[name].loadFromFile(fullPath.string());
	}
	bool loadImage(const std::string& name, const std::filesystem::path& filename) {
		if (image.count(name)){
			throw std::runtime_error("[ImageManager::loadImage] Image already exists");
		}
		return image[name].loadFromFile(filename);
	}
	sf::Texture& operator[](const std::string& name) {
		if (image.count(name)) {
			return image[name];
		}
		return image.at(defaultImageName);
	}
	
};
inline ImageManager imageManager("default");
