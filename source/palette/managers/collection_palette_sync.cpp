#include "palette/managers/collection_palette_sync.h"

#include "game/materials.h"
#include "game/creatures.h"
#include "game/creature.h"
#include "palette/managers/palette_manager.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/creature/creature_brush.h"
#include "brushes/brush.h"

#include "ext/pugixml.hpp"
#include <algorithm>

namespace rme::collection {

void CollectionPaletteSync::Sync(
	const std::string& paletteName,
	const std::string& tilesetName,
	const std::filesystem::path& tilesetFilePath) {

	// 1. Register tileset file in MaterialDatabase::tileset_sources
	g_materials.getDatabase().addTilesetSource(tilesetFilePath.string());
	g_materials.getDatabase().addTilesetSource(tilesetFilePath.generic_string());
	FileName fn(wxstr(tilesetFilePath.string()));
	g_materials.getDatabase().addTilesetSource(fn.GetFullPath().ToStdString());

	// 2. Prepare brushes: load all brushes and items directly from the authoritative XML file
	std::vector<Brush*> activeBrushes;
	pugi::xml_document doc;
	if (doc.load_file(tilesetFilePath.wstring().c_str())) {
		pugi::xml_node root = doc.child("tileset");
		if (root) {
			for (pugi::xml_node child = root.first_child(); child; child = child.next_sibling()) {
				std::string_view tag = child.name();
				if (tag == "brush") {
					std::string bname = child.attribute("name").as_string();
					Brush* b = g_brushes.getBrush(bname);
					if (!b) {
						if (CreatureType* ct = g_creatures[bname]) {
							b = g_materials.ensureCreatureBrush(ct);
						}
					}
					if (b) {
						b->flagAsVisible();
						b->setCollection();
						if (std::ranges::find(activeBrushes, b) == activeBrushes.end()) {
							activeBrushes.push_back(b);
						}
					}
				} else if (tag == "item") {
					if (child.attribute("id")) {
						uint16_t id = static_cast<uint16_t>(child.attribute("id").as_uint());
						if (id != 0) {
							if (RAWBrush* raw = g_materials.ensureRawBrush(id)) {
								raw->flagAsVisible();
								raw->setCollection();
								if (std::ranges::find(activeBrushes, raw) == activeBrushes.end()) {
									activeBrushes.push_back(raw);
								}
							}
						}
					} else if (child.attribute("fromid")) {
						uint32_t fromId = child.attribute("fromid").as_uint();
						uint32_t toId = child.attribute("toid").as_uint(fromId);
						for (uint32_t id = fromId; id <= toId; ++id) {
							if (id != 0) {
								if (RAWBrush* raw = g_materials.ensureRawBrush(static_cast<uint16_t>(id))) {
									raw->flagAsVisible();
									raw->setCollection();
									if (std::ranges::find(activeBrushes, raw) == activeBrushes.end()) {
										activeBrushes.push_back(raw);
									}
								}
							}
						}
					}
				}
			}
		}
	}

	// 3. Update PaletteCatalog tileset brushes directly (handles New, Append, and Overwrite)
	g_materials.paletteCatalog().setTilesetBrushes(paletteName, tilesetName, std::move(activeBrushes));

	// 4. Safely rebuild BrushPalettePanel choicebook pages without dangling pointers
	g_palettes.ReloadDynamicPalettes();

	// 5. Select target palette and jump to the new tileset
	g_palettes.SelectTilesetInPalette(paletteName, tilesetName);
}

} // namespace rme::collection
