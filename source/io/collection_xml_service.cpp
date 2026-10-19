#include "io/collection_xml_service.h"

#include "app/managers/version_manager.h"
#include "brushes/brush.h"
#include "util/file_system.h"
#include "util/common.h"
#include "ext/pugixml.hpp"

#include <algorithm>
#include <set>
#include <format>
#include <spdlog/spdlog.h>

namespace rme::collection {


std::filesystem::path CollectionXmlService::GetCollectionsDirectory() {
	wxString modularPath = g_version.GetModularDataPath();
	std::filesystem::path baseDir;
	if (!modularPath.empty()) {
		baseDir = modularPath.ToStdWstring();
	} else {
		baseDir = std::filesystem::path(FileSystem::GetDataDirectory().ToStdWstring()) / "1098";
	}
	return baseDir / "tilesets" / "collections";
}

std::filesystem::path CollectionXmlService::GetPalettesXmlPath() {
	wxString modularPath = g_version.GetModularDataPath();
	std::filesystem::path baseDir;
	if (!modularPath.empty()) {
		baseDir = modularPath.ToStdWstring();
	} else {
		baseDir = std::filesystem::path(FileSystem::GetDataDirectory().ToStdWstring()) / "1098";
	}
	return baseDir / "palettes.xml";
}

std::filesystem::path CollectionXmlService::GetTilesetFilePath(std::string_view tilesetName) {
	std::string filename = SanitizeTilesetFilename(tilesetName) + ".xml";
	return GetCollectionsDirectory() / filename;
}

std::vector<std::string> CollectionXmlService::GetExistingCollectionTilesetNames() {
	std::vector<std::string> names;
	std::set<std::string> uniqueNames;

	// 1. Scan directory
	std::filesystem::path collDir = GetCollectionsDirectory();
	std::error_code ec;
	if (std::filesystem::exists(collDir, ec) && std::filesystem::is_directory(collDir, ec)) {
		for (const auto& entry : std::filesystem::directory_iterator(collDir, ec)) {
			if (!entry.is_regular_file()) {
				continue;
			}
			if (entry.path().extension() == ".xml") {
				pugi::xml_document doc;
				if (doc.load_file(entry.path().wstring().c_str())) {
					pugi::xml_node root = doc.child("tileset");
					if (root && root.attribute("name")) {
						std::string name = root.attribute("name").as_string();
						if (!name.empty()) {
							uniqueNames.insert(std::move(name));
							continue;
						}
					}
				}
				uniqueNames.insert(entry.path().stem().string());
			}
		}
	}

	// 2. Scan palettes.xml under <palette name="Collection">
	std::filesystem::path palettesPath = GetPalettesXmlPath();
	if (std::filesystem::exists(palettesPath, ec)) {
		pugi::xml_document palDoc;
		if (palDoc.load_file(palettesPath.wstring().c_str())) {
			pugi::xml_node root = palDoc.child("materials");
			if (!root) {
				root = palDoc.child("palettes");
			}
			if (!root) {
				root = palDoc.first_child();
			}

			for (pugi::xml_node palNode = root ? root.child("palette") : palDoc.child("palette"); palNode; palNode = palNode.next_sibling("palette")) {
				if (as_lower_str(palNode.attribute("name").as_string()) == "collection") {
					for (pugi::xml_node tilesetNode = palNode.child("tileset"); tilesetNode; tilesetNode = tilesetNode.next_sibling("tileset")) {
						for (pugi::xml_node incNode = tilesetNode.child("include"); incNode; incNode = incNode.next_sibling("include")) {
							std::string relPath = incNode.attribute("file").as_string();
							if (!relPath.empty()) {
								std::filesystem::path fullPath = palettesPath.parent_path() / relPath;
								if (std::filesystem::exists(fullPath, ec)) {
									pugi::xml_document incDoc;
									if (incDoc.load_file(fullPath.wstring().c_str())) {
										pugi::xml_node tNode = incDoc.child("tileset");
										if (tNode && tNode.attribute("name")) {
											std::string name = tNode.attribute("name").as_string();
											if (!name.empty()) {
												uniqueNames.insert(std::move(name));
												continue;
											}
										}
									}
								}
								std::filesystem::path fPath(relPath);
								std::string stem = fPath.stem().string();
								if (!stem.empty()) {
									uniqueNames.insert(std::move(stem));
								}
							}
						}
					}
				}
			}
		}
	}

	names.assign(uniqueNames.begin(), uniqueNames.end());
	std::ranges::sort(names);
	return names;
}

bool CollectionXmlService::SaveTilesetXml(
	const std::string& tilesetName,
	std::span<const uint16_t> itemIds,
	std::span<Brush* const> brushes,
	bool appendToExisting,
	std::string& outErrorMessage) {
	try {
		std::filesystem::path collDir = GetCollectionsDirectory();
		std::error_code ec;
		std::filesystem::create_directories(collDir, ec);
		if (ec) {
			outErrorMessage = std::format("Failed to create collections directory: {}", ec.message());
			return false;
		}

		std::filesystem::path filePath = GetTilesetFilePath(tilesetName);

		std::vector<std::string> finalBrushNames;
		std::vector<uint16_t> finalItemIds;
		std::set<std::string> brushSet;
		std::set<uint16_t> itemSet;

		// If appendToExisting and file already exists, load existing contents first
		if (appendToExisting && std::filesystem::exists(filePath, ec)) {
			pugi::xml_document existingDoc;
			pugi::xml_parse_result res = existingDoc.load_file(filePath.wstring().c_str());
			if (res) {
				pugi::xml_node root = existingDoc.child("tileset");
				if (root) {
					for (pugi::xml_node child = root.first_child(); child; child = child.next_sibling()) {
						std::string childName = as_lower_str(child.name());
						if (childName == "brush") {
							std::string bname = child.attribute("name").as_string();
							if (!bname.empty() && !brushSet.contains(bname)) {
								brushSet.insert(bname);
								finalBrushNames.push_back(std::move(bname));
							}
						} else if (childName == "item") {
							if (child.attribute("id")) {
								uint16_t id = static_cast<uint16_t>(child.attribute("id").as_uint());
								if (id != 0 && !itemSet.contains(id)) {
									itemSet.insert(id);
									finalItemIds.push_back(id);
								}
							} else if (child.attribute("fromid")) {
								uint32_t fromId = child.attribute("fromid").as_uint();
								uint32_t toId = child.attribute("toid").as_uint(fromId);
								for (uint32_t id = fromId; id <= toId; ++id) {
									if (id != 0 && !itemSet.contains(static_cast<uint16_t>(id))) {
										itemSet.insert(static_cast<uint16_t>(id));
										finalItemIds.push_back(static_cast<uint16_t>(id));
									}
								}
							}
						}
					}
				}
			}
		}

		// Append new brushes
		for (Brush* b : brushes) {
			if (b && !b->getName().empty() && !brushSet.contains(b->getName())) {
				brushSet.insert(b->getName());
				finalBrushNames.push_back(b->getName());
			}
		}

		// Append new items
		for (uint16_t id : itemIds) {
			if (id != 0 && !itemSet.contains(id)) {
				itemSet.insert(id);
				finalItemIds.push_back(id);
			}
		}

		// Construct document
		pugi::xml_document doc;
		pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
		decl.append_attribute("version").set_value("1.0");
		decl.append_attribute("encoding").set_value("utf-8");

		pugi::xml_node root = doc.append_child("tileset");
		root.append_attribute("name").set_value(tilesetName.c_str());

		for (const std::string& bname : finalBrushNames) {
			pugi::xml_node brushNode = root.append_child("brush");
			brushNode.append_attribute("name").set_value(bname.c_str());
		}

		for (uint16_t id : finalItemIds) {
			pugi::xml_node itemNode = root.append_child("item");
			itemNode.append_attribute("id").set_value(id);
		}

		if (!doc.save_file(filePath.wstring().c_str(), "    ", pugi::format_default, pugi::encoding_utf8)) {
			outErrorMessage = std::format("Failed to save tileset XML to '{}'", filePath.string());
			return false;
		}

		return true;
	} catch (const std::exception& e) {
		outErrorMessage = std::format("Exception writing tileset XML: {}", e.what());
		return false;
	}
}

bool CollectionXmlService::EnsurePaletteInclude(
	const std::string& tilesetName,
	std::string& outErrorMessage) {
	try {
		std::filesystem::path palettesPath = GetPalettesXmlPath();
		std::error_code ec;
		if (!std::filesystem::exists(palettesPath, ec)) {
			outErrorMessage = std::format("palettes.xml not found at '{}'", palettesPath.string());
			return false;
		}

		pugi::xml_document doc;
		pugi::xml_parse_result res = doc.load_file(palettesPath.wstring().c_str());
		if (!res) {
			outErrorMessage = std::format("Failed to parse palettes.xml: {}", res.description());
			return false;
		}

		std::string filename = SanitizeTilesetFilename(tilesetName) + ".xml";
		std::string includeRelPath = "tilesets/collections/" + filename;

		pugi::xml_node root = doc.child("materials");
		if (!root) {
			root = doc.child("palettes");
		}
		if (!root) {
			root = doc.first_child();
		}

		pugi::xml_node collectionPal;
		for (pugi::xml_node palNode = root ? root.child("palette") : doc.child("palette"); palNode; palNode = palNode.next_sibling("palette")) {
			if (as_lower_str(palNode.attribute("name").as_string()) == "collection") {
				collectionPal = palNode;
				break;
			}
		}

		if (!collectionPal) {
			if (root) {
				collectionPal = root.append_child("palette");
				collectionPal.append_attribute("name").set_value("Collection");
			} else {
				outErrorMessage = "Invalid palettes.xml structure: no root found";
				return false;
			}
		}

		pugi::xml_node tilesetNode = collectionPal.child("tileset");
		if (!tilesetNode) {
			tilesetNode = collectionPal.append_child("tileset");
		}

		// Check if include already present
		bool alreadyIncluded = false;
		for (pugi::xml_node inc = tilesetNode.child("include"); inc; inc = inc.next_sibling("include")) {
			std::string existingFile = inc.attribute("file").as_string();
			if (std::filesystem::path(existingFile).generic_string() == includeRelPath) {
				alreadyIncluded = true;
				break;
			}
		}

		if (!alreadyIncluded) {
			pugi::xml_node newInc = tilesetNode.append_child("include");
			newInc.append_attribute("file").set_value(includeRelPath.c_str());
		}

		if (!doc.save_file(palettesPath.wstring().c_str(), "    ", pugi::format_default, pugi::encoding_utf8)) {
			outErrorMessage = std::format("Failed to save updated palettes.xml to '{}'", palettesPath.string());
			return false;
		}

		return true;
	} catch (const std::exception& e) {
		outErrorMessage = std::format("Exception updating palettes.xml: {}", e.what());
		return false;
	}
}

} // namespace rme::collection
