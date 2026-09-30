//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_EDITOR_OPERATIONS_GROUND_WALL_OUTLINE_H
#define RME_EDITOR_OPERATIONS_GROUND_WALL_OUTLINE_H

#include "map/position.h"

#include <span>

class BatchAction;
class Editor;
class GroundBrush;
class WallBrush;

namespace GroundWallOutline {
	// Keeps `wall` around the `ground` area near the changed tiles: north and west
	// walls go one tile outside the area, south and east walls on its edge tiles.
	// Walls that end up inside the area (or away from it) are removed and the
	// affected walls are re-aligned.
	void apply(Editor& editor, BatchAction& batch, const GroundBrush& ground, WallBrush& wall, std::span<const Position> changed);

	// Removes the border items `ground` draws itself from the changed tiles and their neighbours.
	void removeGroundBorder(Editor& editor, BatchAction& batch, const GroundBrush& ground, std::span<const Position> changed);
} // namespace GroundWallOutline

#endif
