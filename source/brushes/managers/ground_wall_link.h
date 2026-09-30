//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_GROUND_WALL_LINK_H_
#define RME_GROUND_WALL_LINK_H_

class WallBrush;

// Holds the wall brush that ground brushes temporarily paint around their area.
class GroundWallLink {
public:
	void link(WallBrush* wall) {
		linked_wall = wall;
	}
	void unlink() {
		linked_wall = nullptr;
	}
	[[nodiscard]] WallBrush* linkedWall() const {
		return linked_wall;
	}

private:
	WallBrush* linked_wall = nullptr;
};

extern GroundWallLink g_ground_wall_link;

#endif
