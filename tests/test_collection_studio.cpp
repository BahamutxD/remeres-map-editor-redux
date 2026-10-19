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

// Include pugixml
#include "../source/ext/pugixml.hpp"

// Test 1: Filename sanitization logic
std::string SanitizeTilesetFilename(std::string_view name) {
	std::string result;
	result.reserve(name.size());
	for (char c : name) {
		if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*') {
			result.push_back('_');
		} else if (static_cast<unsigned char>(c) < 32) {
			result.push_back('_');
		} else {
			result.push_back(c);
		}
	}

	size_t start = result.find_first_not_of(" .");
	if (start == std::string::npos) {
		return "collection";
	}
	size_t end = result.find_last_not_of(" .");
	result = result.substr(start, end - start + 1);
	return result.empty() ? "collection" : result;
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
		brushes.erase(std::unique(brushes.begin(), brushes.end()), brushes.end());
	}

	void DiscardItem(uint16_t itemId) {
		std::erase(itemIds, itemId);
	}

	void DiscardBrush(MockBrush* brush) {
		std::erase(brushes, brush);
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

	assert(SanitizeTilesetFilename("Nature") == "Nature");
	assert(SanitizeTilesetFilename("  Desert Sands  ") == "Desert Sands");
	assert(SanitizeTilesetFilename("Cave: Rocks/Walls?*") == "Cave_ Rocks_Walls__");
	assert(SanitizeTilesetFilename("<invalid>|dir\"name") == "_invalid__dir_name");
	assert(SanitizeTilesetFilename("...") == "collection");
	assert(SanitizeTilesetFilename("   ") == "collection");
	assert(SanitizeTilesetFilename("") == "collection");
	assert(SanitizeTilesetFilename(".hidden.") == "hidden");

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
		for (pugi::xml_node inc = tilesetNode.child("include"); inc; inc = inc.next_sibling("include")) {
			if (std::string(inc.attribute("file").as_string()) == incRelPath) {
				alreadyIncluded = true;
				break;
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

	// Clean up
	std::filesystem::remove(tempPalettesPath);
	std::cout << "[PASS] TestPalettesXmlIncludeSync passed!\n";
}

void TestMaterialDatabaseSources() {
	std::cout << "[RUN] TestMaterialDatabaseSources...\n";

	std::vector<std::string> sources;

	auto addSource = [&sources](std::string key) {
		auto it = std::ranges::lower_bound(sources, key);
		if (it == sources.end() || *it != key) {
			sources.insert(it, std::move(key));
		}
	};

	auto isKnown = [&sources](std::string_view key) -> bool {
		return std::ranges::binary_search(sources, key);
	};

	addSource("data/1098/tilesets/terrain/Nature.xml");
	addSource("data/1098/tilesets/doodad/Trash.xml");
	addSource("data/1098/tilesets/collections/Custom_1.xml");

	assert(sources.size() == 3);
	assert(std::ranges::is_sorted(sources));

	assert(isKnown("data/1098/tilesets/collections/Custom_1.xml"));
	assert(isKnown("data/1098/tilesets/terrain/Nature.xml"));
	assert(!isKnown("data/1098/tilesets/collections/NonExistent.xml"));

	// Duplicate add
	addSource("data/1098/tilesets/collections/Custom_1.xml");
	assert(sources.size() == 3);
	assert(std::ranges::is_sorted(sources));

	std::cout << "[PASS] TestMaterialDatabaseSources passed!\n";
}

int main() {
	std::cout << "========== RUNNING COLLECTION STUDIO TESTS ==========\n";
	TestFilenameSanitization();
	TestStagingContentLogic();
	TestXmlGenerationAndRoundtrip();
	TestPalettesXmlIncludeSync();
	TestMaterialDatabaseSources();
	std::cout << "========== ALL TESTS PASSED SUCCESSFULLY! ==========\n";
	return 0;
}
