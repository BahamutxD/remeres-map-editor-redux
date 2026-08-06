#include "app/main.h"
#include "rendering/core/tile_render_cache.h"
#include "map/render_epoch.h"
#include "map/tile.h"

TileRenderCache g_tile_render_cache;

void TileRenderCache::SetEnabled(bool enabled) {
	enabled_ = enabled;
	if (!enabled_) {
		Clear();
	}
}

void TileRenderCache::SetMaxBytes(size_t max_bytes) {
	max_bytes_ = max_bytes;
	EvictUntilWithinBudget();
}

void TileRenderCache::DropStaleIfNeeded() {
	const uint64_t current_epoch = MapRenderEpoch::Current();
	if (current_epoch != last_seen_epoch_) {
		Clear();
		last_seen_epoch_ = current_epoch;
	}
}

void TileRenderCache::EvictUntilWithinBudget() {
	while (approx_bytes_used_ > max_bytes_ && !lru_.empty()) {
		Node& least_recently_used = lru_.back();
		approx_bytes_used_ -= least_recently_used.plan.ApproxBytes();
		index_.erase(least_recently_used.tile);
		lru_.pop_back();
	}
}

const CachedTilePlan* TileRenderCache::TryGet(const Tile* tile) {
	if (!enabled_) {
		return nullptr;
	}

	DropStaleIfNeeded();

	auto it = index_.find(tile);
	if (it == index_.end()) {
		return nullptr;
	}

	// Move to the front (most-recently-used position).
	lru_.splice(lru_.begin(), lru_, it->second);
	return &it->second->plan;
}

void TileRenderCache::Insert(const Tile* tile, CachedTilePlan plan) {
	if (!enabled_) {
		return;
	}

	DropStaleIfNeeded();

	// If a single tile's plan is larger than the entire budget, don't
	// bother caching it — it would just evict everything else.
	const size_t new_bytes = plan.ApproxBytes();
	if (new_bytes > max_bytes_) {
		return;
	}

	auto existing = index_.find(tile);
	if (existing != index_.end()) {
		approx_bytes_used_ -= existing->second->plan.ApproxBytes();
		lru_.erase(existing->second);
		index_.erase(existing);
	}

	lru_.push_front(Node { tile, std::move(plan) });
	index_[tile] = lru_.begin();
	approx_bytes_used_ += new_bytes;

	EvictUntilWithinBudget();
}

void TileRenderCache::Clear() {
	lru_.clear();
	index_.clear();
	approx_bytes_used_ = 0;
}
