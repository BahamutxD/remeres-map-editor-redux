//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "editor/operations/ground_wall_outline.h"
#include "app/main.h"

#include "brushes/ground/ground_brush.h"
#include "brushes/wall/wall_brush.h"
#include "editor/action.h"
#include "editor/action_queue.h"
#include "editor/editor.h"
#include "map/map.h"
#include "map/tile.h"
#include "map/tile_operations.h"

#include <algorithm>
#include <array>
#include <ranges>
#include <vector>

namespace {
	struct Offset {
		int x;
		int y;
	};

	constexpr std::array<Offset, 8> kRing = { { { -1, -1 }, { 0, -1 }, { 1, -1 }, { -1, 0 }, { 1, 0 }, { -1, 1 }, { 0, 1 }, { 1, 1 } } };

	constexpr auto kPositionLess = [](const Position& a, const Position& b) { return a < b; };

	Position shifted(const Position& pos, Offset offset) {
		return Position(pos.x + offset.x, pos.y + offset.y, pos.z);
	}

	// Positions plus their 8 neighbours, sorted and without duplicates.
	std::vector<Position> withNeighbours(std::span<const Position> positions) {
		std::vector<Position> result;
		result.reserve(positions.size() * (kRing.size() + 1));
		for (const Position& pos : positions) {
			result.push_back(pos);
			for (Offset offset : kRing) {
				const Position neighbour = shifted(pos, offset);
				if (neighbour.isValid()) {
					result.push_back(neighbour);
				}
			}
		}
		std::ranges::sort(result, kPositionLess);
		const auto [first, last] = std::ranges::unique(result);
		result.erase(first, last);
		return result;
	}

	bool hasGround(const Map& map, const Position& pos, const GroundBrush& ground) {
		const Tile* tile = map.getTile(pos);
		return tile && tile->getGroundBrush() == &ground;
	}

	bool isOutlineTile(const Map& map, const Position& pos, const GroundBrush& ground) {
		return hasGround(map, pos, ground)
			&& std::ranges::any_of(kRing, [&](Offset offset) { return !hasGround(map, shifted(pos, offset), ground); });
	}

	bool hasWallOf(const Tile& tile, WallBrush& wall) {
		return std::ranges::any_of(tile.items, [&](const auto& item) { return item->isWall() && wall.hasWall(item.get()); });
	}

	void commitIfChanged(BatchAction& batch, std::unique_ptr<Action> action) {
		if (action->size() > 0) {
			batch.addAndCommitAction(std::move(action));
		}
	}

	// Adds the wall to outline tiles and removes it from tiles inside (or no longer part of) the area.
	void placeWalls(
		Editor& editor, BatchAction& batch, const GroundBrush& ground, WallBrush& wall, std::span<const Position> changed,
		std::span<const Position> affected
	) {
		std::unique_ptr<Action> action = editor.actionQueue->createAction(&batch);
		for (const Position& pos : affected) {
			Tile* tile = editor.map.getTile(pos);
			if (!tile) {
				continue;
			}

			const bool in_area = hasGround(editor.map, pos, ground);
			const bool was_changed = std::ranges::binary_search(changed, pos, kPositionLess);
			const bool wants_wall = isOutlineTile(editor.map, pos, ground);
			const bool has_wall = hasWallOf(*tile, wall);

			if (wants_wall && !has_wall) {
				std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
				wall.draw(&editor.map, new_tile.get(), nullptr);
				action->addChange(std::make_unique<Change>(std::move(new_tile)));
			} else if (!wants_wall && has_wall && (in_area || was_changed)) {
				std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
				TileOperations::cleanWalls(new_tile.get(), &wall);
				action->addChange(std::make_unique<Change>(std::move(new_tile)));
			}
		}
		commitIfChanged(batch, std::move(action));
	}

	// Re-aligns every wall around the affected tiles (horizontal, vertical, corner, pole).
	void alignWalls(Editor& editor, BatchAction& batch, std::span<const Position> affected) {
		std::unique_ptr<Action> action = editor.actionQueue->createAction(&batch);
		for (const Position& pos : withNeighbours(affected)) {
			Tile* tile = editor.map.getTile(pos);
			if (!tile || !tile->hasWall()) {
				continue;
			}
			std::unique_ptr<Tile> new_tile = TileOperations::deepCopy(tile, editor.map);
			TileOperations::wallize(new_tile.get(), &editor.map);
			action->addChange(std::make_unique<Change>(std::move(new_tile)));
		}
		commitIfChanged(batch, std::move(action));
	}
} // namespace

namespace GroundWallOutline {
	void apply(Editor& editor, BatchAction& batch, const GroundBrush& ground, WallBrush& wall, std::span<const Position> changed) {
		std::vector<Position> sorted_changed(changed.begin(), changed.end());
		std::ranges::sort(sorted_changed, kPositionLess);

		const std::vector<Position> affected = withNeighbours(sorted_changed);
		placeWalls(editor, batch, ground, wall, sorted_changed, affected);
		alignWalls(editor, batch, affected);
	}
} // namespace GroundWallOutline
