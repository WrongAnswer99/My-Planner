#pragma once
#include <unordered_map>
#include <map>
#include <vector>
#include <filesystem>
#include <iostream>
#include "SFML/Graphics.hpp"

// 统一工具程序的文件路径基准：绝对路径保持不变，相对路径以当前工作根目录为基准。
inline std::filesystem::path resolvePathFromRoot(const std::filesystem::path& path) {
	return path.is_absolute() ? path : std::filesystem::current_path() / path;
}

// 安全遍历文件夹中的所有文件，返回相对于 dir 的相对路径
inline std::vector<std::filesystem::path> listFiles(const std::filesystem::path& dir, int depth = 3){
	std::vector<std::filesystem::path> result;
	if (depth <= 0 || !std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
		return result;
	}

	std::error_code ec;
	for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
		if (ec) {
			ec.clear();
			continue;
		}
		if (entry.is_regular_file(ec)) {
			result.push_back(std::filesystem::relative(entry.path(), dir));
		}
		else if (entry.is_directory(ec)) {
			auto subFiles = listFiles(entry.path(), depth - 1);
			for (auto& f : subFiles) {
				result.push_back(entry.path().filename() / f);
			}
		}
	}
	return result;
}
#include "engine/resource/FontResource.hpp"
#include "engine/resource/ImageResource.hpp"
