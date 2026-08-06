#ifndef RME_MAP_RENDER_EPOCH_H
#define RME_MAP_RENDER_EPOCH_H

#include <atomic>
#include <cstdint>

// Lightweight, dependency-free counter used to invalidate the rendering
// layer's per-tile draw cache (see rendering/core/tile_render_cache.h).
//
// The map/tile layer must never depend on the rendering layer, so this
// counter lives here instead: map code just bumps it whenever tile content
// actually changes (see Tile::modify() and BaseMap::swapTile()), and the
// renderer polls it once per frame to know whether any cached data might be
// stale. This intentionally invalidates the ENTIRE cache on any edit,
// anywhere on the map, rather than trying to track which specific tiles
// changed — it is a coarser invalidation, but it means the cache can never
// show stale content after an edit, undo, or redo, regardless of which code
// path performed the change.
namespace MapRenderEpoch {

inline std::atomic<uint64_t> counter { 1 };

inline void Bump() {
	counter.fetch_add(1, std::memory_order_relaxed);
}

inline uint64_t Current() {
	return counter.load(std::memory_order_relaxed);
}

} // namespace MapRenderEpoch

#endif
