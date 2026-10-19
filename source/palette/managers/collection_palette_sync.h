#ifndef RME_PALETTE_MANAGERS_COLLECTION_PALETTE_SYNC_H_
#define RME_PALETTE_MANAGERS_COLLECTION_PALETTE_SYNC_H_

#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <filesystem>
#include <cstdint>

class Brush;

namespace rme::collection {

class CollectionPaletteSync {
public:
	/**
	 * Synchronizes the newly saved tileset into the runtime MaterialDatabase
	 * and PaletteCatalog, triggers safe reconstruction of BrushPalettePanel
	 * choicebook pages, and activates the "Collection" palette page.
	 */
	static void Sync(
		const std::string& tilesetName,
		const std::filesystem::path& tilesetFilePath);
};

} // namespace rme::collection

#endif // RME_PALETTE_MANAGERS_COLLECTION_PALETTE_SYNC_H_
