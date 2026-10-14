//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "palette/controls/ground_wall_link_menu.h"
#include "app/main.h"

#include "brushes/brush.h"
#include "brushes/managers/ground_wall_link.h"
#include "brushes/wall/wall_brush.h"
#include "ui/gui.h"

#include <format>

namespace {
	enum MenuId { ID_LINK = wxID_HIGHEST + 1, ID_UNLINK, ID_KEEP_GROUND_BORDER };

	wxString linkedStatus(const WallBrush* wall) {
		return wxString::FromUTF8(std::format("Grounds now paint with wall: {}", wall->getName()));
	}
} // namespace

namespace GroundWallLinkMenu {
	bool show(wxWindow* parent, Brush* clicked) {
		WallBrush* linked = g_ground_wall_link.linkedWall();
		WallBrush* clicked_wall = (clicked && clicked->is<WallBrush>()) ? clicked->as<WallBrush>() : nullptr;

		wxMenu menu;
		if (clicked_wall && clicked_wall != linked) {
			menu.Append(ID_LINK, "Paint grounds with this wall");
		}
		if (linked || clicked_wall) {
			menu.AppendCheckItem(ID_KEEP_GROUND_BORDER, "Keep the ground's own border")->Check(g_ground_wall_link.keepGroundBorder());
		}
		if (linked) {
			menu.Append(ID_UNLINK, wxString::FromUTF8(std::format("Stop painting grounds with \"{}\"", linked->getName())));
		}
		if (menu.GetMenuItemCount() == 0) {
			return false;
		}

		switch (parent->GetPopupMenuSelectionFromUser(menu)) {
			case ID_LINK:
				g_ground_wall_link.link(clicked_wall);
				g_gui.SetStatusText(linkedStatus(clicked_wall));
				return true;
			case ID_KEEP_GROUND_BORDER:
				g_ground_wall_link.setKeepGroundBorder(!g_ground_wall_link.keepGroundBorder());
				g_gui.SetStatusText(g_ground_wall_link.keepGroundBorder() ? "Grounds keep their own border" : "Grounds paint without their own border");
				return true;
			case ID_UNLINK:
				g_ground_wall_link.unlink();
				g_gui.SetStatusText("Grounds paint without walls");
				return true;
			default:
				return false;
		}
	}
} // namespace GroundWallLinkMenu
