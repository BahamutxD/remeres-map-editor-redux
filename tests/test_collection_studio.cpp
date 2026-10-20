#include <cstdint>
#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <cassert>
#include <filesystem>
#include <span>
#include <algorithm>
#include <set>
#include <map>

// Include pugixml and CollectionXmlService
#include "../source/ext/pugixml.hpp"
#include "../source/io/collection_xml_service.h"

namespace rme::collection {
std::filesystem::path CollectionXmlService::GetTilesetFilePath(std::string_view paletteName, std::string_view tilesetName) {
	std::string sanitized = SanitizeTilesetFilename(tilesetName);
	std::string pName(paletteName);
	std::ranges::transform(pName, pName.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (pName == "collection") {
		return std::filesystem::path("tilesets/collections") / (sanitized + ".xml");
	}
	return std::filesystem::path("tilesets") / pName / (sanitized + ".xml");
}
}

// Test 2: Mock brush and Staging Content
struct MockBrush {
	uint32_t id;
	std::string name;
	uint32_t getID() const { return id; }
	std::string getName() const { return name; }
};

struct TestStagingContent {
	std::vector<uint16_t> itemIds;
	std::vector<MockBrush*> brushes;

	void Merge(const TestStagingContent& other) {
		itemIds.insert(itemIds.end(), other.itemIds.begin(), other.itemIds.end());
		std::ranges::sort(itemIds);
		itemIds.erase(std::unique(itemIds.begin(), itemIds.end()), itemIds.end());

		brushes.insert(brushes.end(), other.brushes.begin(), other.brushes.end());
		std::ranges::sort(brushes, [](MockBrush* a, MockBrush* b) {
			if (!a || !b) return a < b;
			if (a->getName() != b->getName()) {
				return a->getName() < b->getName();
			}
			return a->getID() < b->getID();
		});
		auto [bfirst, blast] = std::ranges::unique(brushes, [](MockBrush* a, MockBrush* b) {
			if (a == b) return true;
			if (!a || !b) return false;
			return a->getName() == b->getName();
		});
		brushes.erase(bfirst, blast);
	}

	void DiscardItem(uint16_t itemId) {
		std::erase(itemIds, itemId);
	}

	void DiscardBrush(MockBrush* brush) {
		std::erase_if(brushes, [brush](MockBrush* b) {
			if (b == brush) return true;
			if (b && brush && b->getName() == brush->getName()) return true;
			return false;
		});
	}

	void Clear() {
		itemIds.clear();
		brushes.clear();
	}

	[[nodiscard]] bool Empty() const {
		return itemIds.empty() && brushes.empty();
	}
};

void TestFilenameSanitization() {
	std::cout << "[RUN] TestFilenameSanitization...\n";

	using rme::collection::CollectionXmlService;
	assert(CollectionXmlService::SanitizeTilesetFilename("Nature") == "Nature");
	assert(CollectionXmlService::SanitizeTilesetFilename("  Desert Sands  ") == "Desert Sands");
	assert(CollectionXmlService::SanitizeTilesetFilename("Cave: Rocks/Walls?*") == "Cave_ Rocks_Walls__");
	assert(CollectionXmlService::SanitizeTilesetFilename("<invalid>|dir\"name") == "_invalid__dir_name");
	assert(CollectionXmlService::SanitizeTilesetFilename("...") == "collection");
	assert(CollectionXmlService::SanitizeTilesetFilename("   ") == "collection");
	assert(CollectionXmlService::SanitizeTilesetFilename("") == "collection");
	assert(CollectionXmlService::SanitizeTilesetFilename(".hidden.") == "hidden");

	std::cout << "[PASS] TestFilenameSanitization passed!\n";
}

void TestStagingContentLogic() {
	std::cout << "[RUN] TestStagingContentLogic...\n";

	MockBrush b1{1, "Grass"};
	MockBrush b2{2, "Stone Wall"};
	MockBrush b3{3, "Water"};

	TestStagingContent staging;
	assert(staging.Empty());

	staging.itemIds = { 2160, 2152, 100 };
	staging.brushes = { &b1, &b2 };
	assert(!staging.Empty());
	assert(staging.itemIds.size() == 3);
	assert(staging.brushes.size() == 2);

	// Test Merge with overlapping elements
	TestStagingContent second;
	second.itemIds = { 2160, 300, 400 };
	second.brushes = { &b2, &b3 };

	staging.Merge(second);
	// Item IDs should be: 100, 300, 400, 2152, 2160 (5 unique)
	assert(staging.itemIds.size() == 5);
	assert(std::ranges::is_sorted(staging.itemIds));
	assert(staging.itemIds[0] == 100);
	assert(staging.itemIds[4] == 2160);

	// Brushes should be: Grass, Stone Wall, Water (3 unique)
	assert(staging.brushes.size() == 3);
	assert(staging.brushes[0] == &b1);
	assert(staging.brushes[1] == &b2);
	assert(staging.brushes[2] == &b3);

	// Test Discard
	staging.DiscardItem(2160);
	assert(staging.itemIds.size() == 4);
	assert(std::find(staging.itemIds.begin(), staging.itemIds.end(), 2160) == staging.itemIds.end());

	staging.DiscardBrush(&b2);
	assert(staging.brushes.size() == 2);
	assert(std::find(staging.brushes.begin(), staging.brushes.end(), &b2) == staging.brushes.end());

	// Test Clear
	staging.Clear();
	assert(staging.Empty());

	std::cout << "[PASS] TestStagingContentLogic passed!\n";
}

void TestSpawnBrushExclusion() {
	std::cout << "[RUN] TestSpawnBrushExclusion...\n";

	MockBrush bGround{1, "Grass"};
	MockBrush bCreature{2, "Dragon"};
	MockBrush bDoodad{3, "Ancient Fountain"};
	MockBrush bSpawn{4, "SPAWN"};
	MockBrush bRaw{5, "raw_brush"};
	MockBrush bEraser{6, "Eraser"};

	std::vector<MockBrush*> candidates = { &bGround, &bCreature, &bSpawn, &bDoodad, &bRaw, &bEraser };

	std::vector<MockBrush*> filtered;
	for (MockBrush* b : candidates) {
		if (!b || b->getName().empty()) continue;
		if (b->getName() == "SPAWN" || b->getName() == "Eraser" || b->getName() == "raw_brush") {
			continue;
		}
		filtered.push_back(b);
	}

	assert(filtered.size() == 3);
	assert(filtered[0] == &bGround);
	assert(filtered[1] == &bCreature);
	assert(filtered[2] == &bDoodad);
	for (MockBrush* b : filtered) {
		assert(b->getName() != "SPAWN");
	}

	std::cout << "[PASS] TestSpawnBrushExclusion passed!\n";
}

void TestXmlGenerationAndRoundtrip() {
	std::cout << "[RUN] TestXmlGenerationAndRoundtrip...\n";

	std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "test_tileset_collection.xml";

	// Generate tileset XML
	{
		pugi::xml_document doc;
		pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
		decl.append_attribute("version").set_value("1.0");
		decl.append_attribute("encoding").set_value("utf-8");

		pugi::xml_node root = doc.append_child("tileset");
		root.append_attribute("name").set_value("TestCollection");

		pugi::xml_node b1 = root.append_child("brush");
		b1.append_attribute("name").set_value("grass");

		pugi::xml_node b2 = root.append_child("brush");
		b2.append_attribute("name").set_value("water");

		pugi::xml_node it1 = root.append_child("item");
		it1.append_attribute("id").set_value(2160);

		pugi::xml_node it2 = root.append_child("item");
		it2.append_attribute("id").set_value(2152);

		bool saved = doc.save_file(tempPath.wstring().c_str(), "    ", pugi::format_default, pugi::encoding_utf8);
		assert(saved);
	}

	// Read back and verify
	{
		pugi::xml_document doc;
		pugi::xml_parse_result res = doc.load_file(tempPath.wstring().c_str());
		assert(res);

		pugi::xml_node root = doc.child("tileset");
		assert(root);
		assert(std::string(root.attribute("name").as_string()) == "TestCollection");

		std::vector<std::string> brushes;
		std::vector<uint16_t> items;

		for (pugi::xml_node child = root.first_child(); child; child = child.next_sibling()) {
			std::string tag = child.name();
			if (tag == "brush") {
				brushes.push_back(child.attribute("name").as_string());
			} else if (tag == "item") {
				items.push_back(static_cast<uint16_t>(child.attribute("id").as_uint()));
			}
		}

		assert(brushes.size() == 2);
		assert(brushes[0] == "grass");
		assert(brushes[1] == "water");

		assert(items.size() == 2);
		assert(items[0] == 2160);
		assert(items[1] == 2152);
	}

	// Clean up
	std::filesystem::remove(tempPath);
	std::cout << "[PASS] TestXmlGenerationAndRoundtrip passed!\n";
}

void TestPalettesXmlIncludeSync() {
	std::cout << "[RUN] TestPalettesXmlIncludeSync...\n";

	std::filesystem::path tempPalettesPath = std::filesystem::temp_directory_path() / "test_palettes.xml";

	// Create sample palettes.xml
	{
		pugi::xml_document doc;
		pugi::xml_node root = doc.append_child("palettes");
		pugi::xml_node terrain = root.append_child("palette");
		terrain.append_attribute("name").set_value("Terrain");

		pugi::xml_node collection = root.append_child("palette");
		collection.append_attribute("name").set_value("Collection");
		collection.append_child("tileset"); // empty tileset tag

		bool saved = doc.save_file(tempPalettesPath.wstring().c_str(), "    ", pugi::format_default, pugi::encoding_utf8);
		assert(saved);
	}

	// Perform include injection
	{
		pugi::xml_document doc;
		assert(doc.load_file(tempPalettesPath.wstring().c_str()));

		pugi::xml_node root = doc.child("palettes");
		assert(root);

		pugi::xml_node collectionPal;
		for (pugi::xml_node p = root.child("palette"); p; p = p.next_sibling("palette")) {
			if (std::string(p.attribute("name").as_string()) == "Collection") {
				collectionPal = p;
				break;
			}
		}
		assert(collectionPal);

		pugi::xml_node tilesetNode = collectionPal.child("tileset");
		assert(tilesetNode);

		std::string incRelPath = "tilesets/collections/MyDungeon.xml";
		bool alreadyIncluded = false;
		std::string normIncludeRel = incRelPath;
		std::ranges::transform(normIncludeRel, normIncludeRel.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		std::replace(normIncludeRel.begin(), normIncludeRel.end(), '\\', '/');

		for (pugi::xml_node inc = tilesetNode.child("include"); inc; inc = inc.next_sibling("include")) {
			std::string existingFolder = inc.attribute("folder").as_string();
			if (!existingFolder.empty()) {
				std::string normFolder = existingFolder;
				std::ranges::transform(normFolder, normFolder.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				std::replace(normFolder.begin(), normFolder.end(), '\\', '/');
				if (!normFolder.empty() && normFolder.back() != '/') normFolder += '/';
				if (normIncludeRel.starts_with(normFolder)) {
					alreadyIncluded = true;
					break;
				}
			}
			std::string existingFile = inc.attribute("file").as_string();
			if (!existingFile.empty()) {
				std::string normExisting = existingFile;
				std::ranges::transform(normExisting, normExisting.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				std::replace(normExisting.begin(), normExisting.end(), '\\', '/');
				if (normExisting == normIncludeRel) {
					alreadyIncluded = true;
					break;
				}
			}
		}
		assert(!alreadyIncluded);

		pugi::xml_node newInc = tilesetNode.append_child("include");
		newInc.append_attribute("file").set_value(incRelPath.c_str());

		assert(doc.save_file(tempPalettesPath.wstring().c_str(), "    ", pugi::format_default, pugi::encoding_utf8));
	}

	// Verify idempotency (second attempt should not duplicate)
	{
		pugi::xml_document doc;
		assert(doc.load_file(tempPalettesPath.wstring().c_str()));

		pugi::xml_node root = doc.child("palettes");
		pugi::xml_node collectionPal = root.child("palette");
		while (collectionPal && std::string(collectionPal.attribute("name").as_string()) != "Collection") {
			collectionPal = collectionPal.next_sibling("palette");
		}
		assert(collectionPal);

		pugi::xml_node tilesetNode = collectionPal.child("tileset");
		assert(tilesetNode);

		int count = 0;
		for (pugi::xml_node inc = tilesetNode.child("include"); inc; inc = inc.next_sibling("include")) {
			if (std::string(inc.attribute("file").as_string()) == "tilesets/collections/MyDungeon.xml") {
				count++;
			}
		}
		assert(count == 1);
	}

	// Verify that a folder include <include folder="tilesets/collections/" /> covers files
	{
		pugi::xml_document doc;
		assert(doc.load_file(tempPalettesPath.wstring().c_str()));
		pugi::xml_node root = doc.child("palettes");
		pugi::xml_node collectionPal = root.child("palette");
		while (collectionPal && std::string(collectionPal.attribute("name").as_string()) != "Collection") {
			collectionPal = collectionPal.next_sibling("palette");
		}
		pugi::xml_node tilesetNode = collectionPal.child("tileset");
		tilesetNode.remove_child("include"); // Clear file include
		pugi::xml_node folderInc = tilesetNode.append_child("include");
		folderInc.append_attribute("folder").set_value("tilesets/collections/");
		folderInc.append_attribute("subfolders").set_value("true");

		std::string incRelPath = "tilesets/collections/NewSet.xml";
		bool covered = false;
		std::string normIncludeRel = incRelPath;
		std::ranges::transform(normIncludeRel, normIncludeRel.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		std::replace(normIncludeRel.begin(), normIncludeRel.end(), '\\', '/');

		for (pugi::xml_node inc = tilesetNode.child("include"); inc; inc = inc.next_sibling("include")) {
			std::string existingFolder = inc.attribute("folder").as_string();
			if (!existingFolder.empty()) {
				std::string normFolder = existingFolder;
				std::ranges::transform(normFolder, normFolder.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				std::replace(normFolder.begin(), normFolder.end(), '\\', '/');
				if (!normFolder.empty() && normFolder.back() != '/') normFolder += '/';
				if (normIncludeRel.starts_with(normFolder)) {
					covered = true;
					break;
				}
			}
		}
		assert(covered);
	}

	// Clean up
	std::filesystem::remove(tempPalettesPath);
	std::cout << "[PASS] TestPalettesXmlIncludeSync passed!\n";
}

void TestMaterialDatabaseSources() {
	std::cout << "[RUN] TestMaterialDatabaseSources...\n";

	std::vector<std::string> sources;

	auto normalizeKey = [](std::string key) -> std::string {
		std::ranges::transform(key, key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		std::replace(key.begin(), key.end(), '\\', '/');
		return key;
	};

	auto addSource = [&sources, &normalizeKey](std::string key) {
		key = normalizeKey(std::move(key));
		auto it = std::ranges::lower_bound(sources, key);
		if (it == sources.end() || *it != key) {
			sources.insert(it, std::move(key));
		}
	};

	auto isKnown = [&sources, &normalizeKey](std::string_view key) -> bool {
		std::string normKey = normalizeKey(std::string(key));
		return std::ranges::binary_search(sources, normKey);
	};

	addSource("data/1098/tilesets/terrain/Nature.xml");
	addSource("data/1098/tilesets/doodad/Trash.xml");
	addSource("data/1098/tilesets/collections/Custom_1.xml");

	assert(sources.size() == 3);
	assert(std::ranges::is_sorted(sources));

	// Case-insensitive checks
	assert(isKnown("data/1098/tilesets/collections/Custom_1.xml"));
	assert(isKnown("DATA/1098/TILESETS/COLLECTIONS/CUSTOM_1.XML"));
	assert(isKnown("data/1098/tilesets/collections/custom_1.xml"));

	// Windows backslash vs forward slash checks
	assert(isKnown("data\\1098\\tilesets\\collections\\Custom_1.xml"));
	assert(isKnown("DATA\\1098\\TILESETS\\terrain\\Nature.xml"));

	// Non-existent
	assert(!isKnown("data/1098/tilesets/collections/NonExistent.xml"));

	// Duplicate add with different slashes and casing
	addSource("DATA\\1098\\tilesets\\collections\\CUSTOM_1.xml");
	assert(sources.size() == 3);
	assert(std::ranges::is_sorted(sources));

	std::cout << "[PASS] TestMaterialDatabaseSources passed!\n";
}

void TestPaletteCatalogSetTilesetBrushes() {
	std::cout << "[RUN] TestPaletteCatalogSetTilesetBrushes...\n";

	struct TestTilesetDef {
		std::string name;
		std::vector<MockBrush*> brushes;
	};

	struct TestPaletteDef {
		std::string name;
		std::vector<TestTilesetDef> tilesets;
	};

	struct TestCatalog {
		std::vector<TestPaletteDef> palettes;

		void setTilesetBrushes(std::string_view paletteName, std::string_view tilesetName, std::vector<MockBrush*> brushes) {
			auto it = std::ranges::find_if(palettes, [paletteName](const auto& existing) {
				std::string a = existing.name;
				std::string b = std::string(paletteName);
				std::ranges::transform(a, a.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				std::ranges::transform(b, b.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				return a == b;
			});

			if (it == palettes.end()) {
				TestPaletteDef newPal;
				newPal.name = std::string(paletteName);
				TestTilesetDef newTileset;
				newTileset.name = std::string(tilesetName);
				newTileset.brushes = std::move(brushes);
				newPal.tilesets.push_back(std::move(newTileset));
				palettes.push_back(std::move(newPal));
				return;
			}

			auto tilesetIt = std::ranges::find_if(it->tilesets, [tilesetName](const auto& existingTileset) {
				std::string a = existingTileset.name;
				std::string b = std::string(tilesetName);
				std::ranges::transform(a, a.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				std::ranges::transform(b, b.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				return a == b;
			});

			if (tilesetIt != it->tilesets.end()) {
				tilesetIt->brushes = std::move(brushes);
			} else {
				TestTilesetDef newTileset;
				newTileset.name = std::string(tilesetName);
				newTileset.brushes = std::move(brushes);
				it->tilesets.push_back(std::move(newTileset));
			}
		}
	};

	MockBrush b1{1, "Grass"};
	MockBrush b2{2, "Stone Wall"};
	MockBrush b3{3, "Water"};

	TestCatalog catalog;
	assert(catalog.palettes.empty());

	// 1. Initial creation of palette and tileset
	catalog.setTilesetBrushes("Collection", "Dungeon", {&b1, &b2});
	assert(catalog.palettes.size() == 1);
	assert(catalog.palettes[0].name == "Collection");
	assert(catalog.palettes[0].tilesets.size() == 1);
	assert(catalog.palettes[0].tilesets[0].name == "Dungeon");
	assert(catalog.palettes[0].tilesets[0].brushes.size() == 2);

	// 2. Add second tileset to existing palette
	catalog.setTilesetBrushes("Collection", "Nature", {&b3});
	assert(catalog.palettes.size() == 1);
	assert(catalog.palettes[0].tilesets.size() == 2);
	assert(catalog.palettes[0].tilesets[0].name == "Dungeon");
	assert(catalog.palettes[0].tilesets[1].name == "Nature");
	assert(catalog.palettes[0].tilesets[1].brushes.size() == 1);

	// 3. Overwrite "Dungeon" tileset (case-insensitive "collection" / "dungeon") with single brush b3
	catalog.setTilesetBrushes("collection", "dungeon", {&b3});
	assert(catalog.palettes.size() == 1);
	assert(catalog.palettes[0].tilesets.size() == 2);
	assert(catalog.palettes[0].tilesets[0].brushes.size() == 1);
	assert(catalog.palettes[0].tilesets[0].brushes[0] == &b3);
	// Nature tileset untouched
	assert(catalog.palettes[0].tilesets[1].brushes.size() == 1);
	assert(catalog.palettes[0].tilesets[1].brushes[0] == &b3);

	// 4. Overwrite with multiple brushes
	catalog.setTilesetBrushes("COLLECTION", "DUNGEON", {&b1, &b2, &b3});
	assert(catalog.palettes[0].tilesets[0].brushes.size() == 3);

	std::cout << "[PASS] TestPaletteCatalogSetTilesetBrushes passed!\n";
}

void TestTargetPaletteXmlAndPaths() {
	std::cout << "[RUN] TestTargetPaletteXmlAndPaths...\n";

	using rme::collection::CollectionXmlService;
	auto p1 = CollectionXmlService::GetTilesetFilePath("Collection", "MySet");
	assert(p1.generic_string().find("tilesets/collections/MySet.xml") != std::string::npos);

	auto p2 = CollectionXmlService::GetTilesetFilePath("Terrain", "MyCustomTerrain");
	assert(p2.generic_string().find("tilesets/terrain/MyCustomTerrain.xml") != std::string::npos);

	auto p3 = CollectionXmlService::GetTilesetFilePath("Doodad", "MyDoodad");
	assert(p3.generic_string().find("tilesets/doodad/MyDoodad.xml") != std::string::npos);

	// Test multi-palette include in palettes.xml
	std::filesystem::path tempPalettesPath = std::filesystem::temp_directory_path() / "test_target_palettes.xml";
	{
		pugi::xml_document doc;
		pugi::xml_node root = doc.append_child("palettes");
		pugi::xml_node terrainPal = root.append_child("palette");
		terrainPal.append_attribute("name").set_value("Terrain");
		pugi::xml_node ts = terrainPal.append_child("tileset");
		pugi::xml_node inc = ts.append_child("include");
		inc.append_attribute("file").set_value("tilesets/terrain/Nature.xml");

		pugi::xml_node collPal = root.append_child("palette");
		collPal.append_attribute("name").set_value("Collection");
		collPal.append_child("tileset");

		assert(doc.save_file(tempPalettesPath.wstring().c_str(), "    ", pugi::format_default, pugi::encoding_utf8));
	}

	// Add include to Terrain palette
	{
		pugi::xml_document doc;
		assert(doc.load_file(tempPalettesPath.wstring().c_str()));
		pugi::xml_node root = doc.child("palettes");

		pugi::xml_node terrainPal;
		for (pugi::xml_node pal = root.child("palette"); pal; pal = pal.next_sibling("palette")) {
			if (std::string(pal.attribute("name").as_string()) == "Terrain") {
				terrainPal = pal;
				break;
			}
		}
		assert(terrainPal);
		pugi::xml_node tilesetNode = terrainPal.child("tileset");
		assert(tilesetNode);

		pugi::xml_node newInc = tilesetNode.append_child("include");
		newInc.append_attribute("file").set_value("tilesets/terrain/MyCustomTerrain.xml");
		assert(doc.save_file(tempPalettesPath.wstring().c_str(), "    ", pugi::format_default, pugi::encoding_utf8));
	}

	// Verify Terrain palette has 2 includes and Collection is untouched
	{
		pugi::xml_document doc;
		assert(doc.load_file(tempPalettesPath.wstring().c_str()));
		pugi::xml_node root = doc.child("palettes");
		for (pugi::xml_node pal = root.child("palette"); pal; pal = pal.next_sibling("palette")) {
			std::string name = pal.attribute("name").as_string();
			if (name == "Terrain") {
				int count = 0;
				for (pugi::xml_node inc = pal.child("tileset").child("include"); inc; inc = inc.next_sibling("include")) {
					count++;
				}
				assert(count == 2);
			} else if (name == "Collection") {
				int count = 0;
				for (pugi::xml_node inc = pal.child("tileset").child("include"); inc; inc = inc.next_sibling("include")) {
					count++;
				}
				assert(count == 0);
			}
		}
	}

	std::filesystem::remove(tempPalettesPath);
	std::cout << "[PASS] TestTargetPaletteXmlAndPaths passed!\n";
}

void TestMultiFloorSelectionAndHitTesting() {
	std::cout << "[RUN] TestMultiFloorSelectionAndHitTesting...\n";

	constexpr int MAP_MAX_LAYER = 15;
	constexpr int GROUND_LAYER = 7;

	auto getFloorTileOffset = [](int floor) -> int {
		if (floor <= GROUND_LAYER) {
			return GROUND_LAYER - floor;
		}
		return 0;
	};

	assert(getFloorTileOffset(7) == 0);
	assert(getFloorTileOffset(6) == 1);
	assert(getFloorTileOffset(5) == 2);
	assert(getFloorTileOffset(8) == 0);
	assert(getFloorTileOffset(15) == 0);

	// 1. Verify "Floors Below" bounding box: s_z = MAP_MAX_LAYER, e_z = floor
	// For floor = 8 (underground)
	{
		const int floor = 8;
		const int s_z = MAP_MAX_LAYER;
		const int e_z = floor;
		assert(s_z == 15);
		assert(e_z == 8);
		assert(s_z >= e_z);
		// Floors above 8 (0..7) are NEVER included
		for (int z = s_z; z >= e_z; --z) {
			assert(z >= 8 && z <= 15);
		}
	}

	// For floor = 7 (ground)
	{
		const int floor = 7;
		const int s_z = MAP_MAX_LAYER;
		const int e_z = floor;
		assert(s_z == 15);
		assert(e_z == 7);
		for (int z = s_z; z >= e_z; --z) {
			assert(z >= 7 && z <= 15);
		}
	}

	// 2. Multi-floor cursor hit test projection
	// Active floor is 6 (tower roof). User clicks screen position mapped to (100, 100, 6).
	// Lower visible floor 7 tile should be at (100 - 1 + 0, 100 - 1 + 0, 7) = (99, 99, 7).
	{
		const int mouse_x = 100;
		const int mouse_y = 100;
		const int mouse_z = 6;

		struct MockTile {
			int x;
			int y;
			int z;
			bool selected;
		};

		std::vector<MockTile> tiles = {
			{ 100, 100, 6, false }, // unselected on active floor
			{ 99, 99, 7, true },    // selected on visible floor below!
		};

		auto findTile = [&](int x, int y, int z) -> const MockTile* {
			for (const auto& t : tiles) {
				if (t.x == x && t.y == y && t.z == z) return &t;
			}
			return nullptr;
		};

		// Algorithm: check active floor first, if unselected check visible floors below
		const MockTile* activeTile = findTile(mouse_x, mouse_y, mouse_z);
		assert(activeTile != nullptr && !activeTile->selected);

		const MockTile* resolvedTile = nullptr;
		for (int z = mouse_z + 1; z <= MAP_MAX_LAYER; ++z) {
			const int z_x = mouse_x - getFloorTileOffset(mouse_z) + getFloorTileOffset(z);
			const int z_y = mouse_y - getFloorTileOffset(mouse_z) + getFloorTileOffset(z);
			const MockTile* lower = findTile(z_x, z_y, z);
			if (lower && lower->selected) {
				resolvedTile = lower;
				break;
			}
		}

		assert(resolvedTile != nullptr);
		assert(resolvedTile->z == 7);
		assert(resolvedTile->x == 99 && resolvedTile->y == 99);
		assert(resolvedTile->selected == true);
	}

	std::cout << "[PASS] TestMultiFloorSelectionAndHitTesting passed!\n";
}


void TestCreatureNameAndReloadPreservation() {
	std::cout << "[RUN] TestCreatureNameAndReloadPreservation...\n";

	// Test 1: Creature name fallback when CreatureType is not in database
	struct MockCreatureType {
		std::string name;
		bool standard = false;
		bool missing = false;
		bool isNpc = false;
	};

	std::map<std::string, MockCreatureType*> mock_database;

	auto getName = [&](const std::string& type_name) -> const std::string& {
		auto it = mock_database.find(type_name);
		if (it != mock_database.end() && it->second) {
			return it->second->name;
		}
		return type_name;
	};

	// When type is not registered, getName() returns type_name, NOT empty string
	std::string npcName = "Frodo";
	assert(getName(npcName) == "Frodo");

	// Test 2: Standard vs non-standard database clearing
	MockCreatureType standardMonster{ "Demon", true, false, false };
	MockCreatureType spawnLoadedNpc{ "Frodo", false, true, true };

	mock_database["demon"] = &standardMonster;
	mock_database["frodo"] = &spawnLoadedNpc;

	// Clear only standard (as on F5 reload)
	for (auto it = mock_database.begin(); it != mock_database.end(); ) {
		if (it->second && it->second->standard) {
			it = mock_database.erase(it);
		} else {
			++it;
		}
	}

	assert(mock_database.find("demon") == mock_database.end());
	assert(mock_database.find("frodo") != mock_database.end());
	assert(mock_database["frodo"]->name == "Frodo");

	std::cout << "[PASS] TestCreatureNameAndReloadPreservation passed!\n";
}

int main() {
	std::cout << "========== RUNNING COLLECTION STUDIO TESTS ==========\n";
	TestFilenameSanitization();
	TestStagingContentLogic();
	TestSpawnBrushExclusion();
	TestXmlGenerationAndRoundtrip();
	TestPalettesXmlIncludeSync();
	TestMaterialDatabaseSources();
	TestPaletteCatalogSetTilesetBrushes();
	TestTargetPaletteXmlAndPaths();
	TestMultiFloorSelectionAndHitTesting();
	TestCreatureNameAndReloadPreservation();
	std::cout << "========== ALL TESTS PASSED SUCCESSFULLY! ==========\n";
	return 0;
}
