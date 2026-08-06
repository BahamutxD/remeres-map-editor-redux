#ifndef RME_RENDERING_TILE_RENDER_CACHE_H_
#define RME_RENDERING_TILE_RENDER_CACHE_H_

#include <cstdint>
#include <cstddef>
#include <vector>
#include <list>
#include <unordered_map>

#include "item_definitions/core/item_definition_store.h"

class Tile;
class Item;

// -----------------------------------------------------------------------
// TileRenderCache
// -----------------------------------------------------------------------
// Caches ONLY the side-effect-free "what is on this tile" decision that
// TileRenderer::DrawTile() re-derives every frame for every visible tile:
// which items exist, their resolved ItemDefinitionView, and a couple of
// static per-item flags (is it the ground, is it a border, is it an
// invalid OTBM item). None of this depends on the camera, the current
// frame/animation, selection state, house-highlight pulsing, or any
// DrawingOptions toggle — it only depends on the tile's actual content.
//
// Deliberately NOT cached (still computed fresh every single frame,
// exactly as before this cache existed):
//   - Animation frame, color/tint, house pulse, light
//   - Whether an invalid item's marker is actually shown (depends on the
//     live "show invalid tiles" option)
//   - Tooltips, hook/door indicator overlays, selection highlighting
// All of the above are produced by calling the exact same functions
// (BlitItem, FillItemTooltipData, ...) every frame regardless of whether
// the enumeration below came from cache or was just computed — caching
// the enumeration cannot desync visuals from tile content because nothing
// that actually draws pixels or registers overlays is skipped.
//
// Invalidation is coarse and safe by construction: MapRenderEpoch is
// bumped on any tile content change anywhere on the map (Tile::modify()
// and BaseMap::swapTile(), which together cover edits, undo, redo, and
// scripted/Lua changes). Whenever the cache's last-seen epoch differs
// from the current one, the ENTIRE cache is dropped before any lookup can
// return stale data — there is no per-tile invalidation to get wrong.
// -----------------------------------------------------------------------

struct CachedTileDrawEntry {
	Item* item = nullptr;
	ItemDefinitionView definition;
	bool is_border = false;
	bool is_invalid_otbm_item = false;
};

struct CachedTilePlan {
	std::vector<CachedTileDrawEntry> entries; // tile->items, in original order

	size_t ApproxBytes() const {
		return sizeof(CachedTilePlan) + entries.capacity() * sizeof(CachedTileDrawEntry);
	}
};

class TileRenderCache {
public:
	void SetEnabled(bool enabled);
	bool IsEnabled() const {
		return enabled_;
	}

	// Approximate RAM budget for cached plans. When exceeded, least-
	// recently-used entries are evicted until back under budget.
	void SetMaxBytes(size_t max_bytes);
	size_t GetMaxBytes() const {
		return max_bytes_;
	}

	// Returns nullptr on a miss (disabled, not present, or the map changed
	// since this entry was cached). Marks the entry as most-recently-used
	// on a hit.
	const CachedTilePlan* TryGet(const Tile* tile);

	// Stores (or replaces) the plan for this tile at the current epoch.
	// No-op while disabled.
	void Insert(const Tile* tile, CachedTilePlan plan);

	void Clear();

	size_t EntryCount() const {
		return index_.size();
	}
	size_t ApproxBytesUsed() const {
		return approx_bytes_used_;
	}

private:
	struct Node {
		const Tile* tile = nullptr;
		CachedTilePlan plan;
	};

	void DropStaleIfNeeded();
	void EvictUntilWithinBudget();

	bool enabled_ = true;
	size_t max_bytes_ = 256ull * 1024 * 1024;
	size_t approx_bytes_used_ = 0;
	uint64_t last_seen_epoch_ = 0;

	// Front = most recently used.
	std::list<Node> lru_;
	std::unordered_map<const Tile*, std::list<Node>::iterator> index_;
};

extern TileRenderCache g_tile_render_cache;

#endif
