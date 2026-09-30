//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_CONTROLS_GROUND_WALL_LINK_MENU_H_
#define RME_PALETTE_CONTROLS_GROUND_WALL_LINK_MENU_H_

class Brush;
class wxWindow;

namespace GroundWallLinkMenu {
	// Shows the "paint grounds with this wall" context menu for a palette brush.
	// Returns true if the linked wall changed.
	bool show(wxWindow* parent, Brush* clicked);
} // namespace GroundWallLinkMenu

#endif
