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
	// Keeps `wall` on the outline of the `ground` area around the changed tiles:
	// edge tiles of the area get the wall, interior tiles (and tiles that lost the
	// ground) lose it, and the affected walls are re-aligned.
	void apply(Editor& editor, BatchAction& batch, const GroundBrush& ground, WallBrush& wall, std::span<const Position> changed);
} // namespace GroundWallOutline

#endif
