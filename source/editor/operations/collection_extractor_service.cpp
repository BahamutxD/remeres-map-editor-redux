#include "editor/operations/collection_extractor_service.h"

#include "editor/editor.h"
#include "map/tile.h"
#include "game/item.h"
#include "brushes/brush.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/ground/ground_brush.h"
#include "ui/replace_tool/map_item_collector.h"

#include <algorithm>

namespace rme::collection {

void CollectionStagingContent::Merge(const CollectionStagingContent& other) {
	// Merge item IDs
	itemIds.insert(itemIds.end(), other.itemIds.begin(), other.itemIds.end());
	std::ranges::sort(itemIds);
	itemIds.erase(std::unique(itemIds.begin(), itemIds.end()), itemIds.end());

	// Merge brushes
	brushes.insert(brushes.end(), other.brushes.begin(), other.brushes.end());
	std::ranges::sort(brushes, [](Brush* a, Brush* b) {
		if (!a || !b) {
			return a < b;
		}
		if (a->getName() != b->getName()) {
			return a->getName() < b->getName();
		}
		return a->getID() < b->getID();
	});
	brushes.erase(std::unique(brushes.begin(), brushes.end()), brushes.end());
}

void CollectionStagingContent::DiscardItem(uint16_t itemId) {
	std::erase(itemIds, itemId);
}

void CollectionStagingContent::DiscardBrush(Brush* brush) {
	std::erase(brushes, brush);
}

void CollectionStagingContent::Clear() {
	itemIds.clear();
	brushes.clear();
}

CollectionStagingContent CollectionExtractorService::ExtractFromSelection(Editor* editor) {
	CollectionStagingContent content;
	if (!editor) {
		return content;
	}

	// 1. Unique item IDs
	content.itemIds = rme::replace_tool::MapItemCollector::CollectSelectionItemIds(editor);
	std::ranges::sort(content.itemIds);

	// 2. Composite brushes from selection tiles
	std::vector<Brush*> candidates;
	for (Tile* tile : editor->selection.getTiles()) {
		if (!tile) {
			continue;
		}

		if (tile->hasGround()) {
			if (GroundBrush* gb = tile->getGroundBrush()) {
				candidates.push_back(gb);
			} else if (tile->ground) {
				if (Brush* b = tile->ground->getBrush()) {
					candidates.push_back(b);
				}
			}
		}

		for (const auto& item : tile->items) {
			if (!item) {
				continue;
			}
			if (Brush* b = item->getBrush()) {
				candidates.push_back(b);
			}
			if (Brush* db = item->getDoodadBrush()) {
				candidates.push_back(db);
			}
		}
	}

	for (Brush* b : candidates) {
		if (!b) {
			continue;
		}
		if (b->getName().empty()) {
			continue;
		}
		// Filter out raw and eraser brushes
		if (b->is<RAWBrush>() || b->is<EraserBrush>()) {
			continue;
		}
		content.brushes.push_back(b);
	}

	std::ranges::sort(content.brushes, [](Brush* a, Brush* b) {
		if (a->getName() != b->getName()) {
			return a->getName() < b->getName();
		}
		return a->getID() < b->getID();
	});
	content.brushes.erase(std::unique(content.brushes.begin(), content.brushes.end()), content.brushes.end());

	return content;
}

} // namespace rme::collection
