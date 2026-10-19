#include "palette/managers/collection_palette_sync.h"

#include "game/materials.h"
#include "palette/managers/palette_manager.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/brush.h"

namespace rme::collection {

void CollectionPaletteSync::Sync(
	const std::string& tilesetName,
	const std::filesystem::path& tilesetFilePath,
	std::span<const uint16_t> itemIds,
	std::span<Brush* const> brushes) {

	// 1. Register tileset file in MaterialDatabase::tileset_sources
	g_materials.getDatabase().addTilesetSource(tilesetFilePath.string());
	g_materials.getDatabase().addTilesetSource(tilesetFilePath.generic_string());

	// 2. Prepare brushes: ensure raw brushes exist and flag everything as visible
	std::vector<Brush*> activeBrushes;
	activeBrushes.reserve(brushes.size() + itemIds.size());

	for (Brush* b : brushes) {
		if (b) {
			b->flagAsVisible();
			b->setCollection();
			activeBrushes.push_back(b);
		}
	}

	for (uint16_t id : itemIds) {
		if (id != 0) {
			RAWBrush* raw = g_materials.ensureRawBrush(id);
			if (raw) {
				raw->flagAsVisible();
				raw->setCollection();
				activeBrushes.push_back(raw);
			}
		}
	}

	// 3. Build/update DynamicTilesetDefinition in PaletteCatalog
	DynamicTilesetDefinition newTileset;
	newTileset.name = tilesetName;
	for (Brush* b : activeBrushes) {
		if (b && !newTileset.containsBrush(b)) {
			newTileset.brushes.push_back(b);
		}
	}

	DynamicPaletteDefinition colPal;
	colPal.name = "Collection";
	colPal.tilesets.push_back(std::move(newTileset));
	g_materials.paletteCatalog().addDynamicPalette(std::move(colPal));

	// 4. Safely rebuild BrushPalettePanel choicebook pages without dangling pointers
	g_palettes.ReloadDynamicPalettes();

	// 5. Select "Collection" palette and jump to the new tileset
	g_palettes.SelectTilesetInPalette("Collection", tilesetName);
}

} // namespace rme::collection
