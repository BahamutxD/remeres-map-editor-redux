//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

// ============================================================================
// Waypoint palette

#include "app/main.h"

#include <format>

#include "ui/gui.h"
#include "brushes/managers/brush_manager.h"
#include "editor/hotkey_manager.h"
#include "palette/palette_waypoints.h"
#include "brushes/waypoint/waypoint_brush.h"
#include "map/map.h"
#include "util/image_manager.h"

WaypointPalettePanel::WaypointPalettePanel(wxWindow* parent, wxWindowID id) :
	PalettePanel(parent, id),
	map(nullptr) {
	wxSizer* sidesizer = newd wxBoxSizer(wxVERTICAL);

	waypoint_list = newd wxListCtrl(this, PALETTE_WAYPOINT_LISTBOX, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_EDIT_LABELS);
	waypoint_list->InsertColumn(0, "Name", wxLIST_FORMAT_LEFT, 110);
	waypoint_list->InsertColumn(1, "Position", wxLIST_FORMAT_LEFT, 95);
	sidesizer->Add(waypoint_list, 1, wxEXPAND);

	wxSizer* tmpsizer = newd wxBoxSizer(wxHORIZONTAL);
	add_waypoint_button = newd wxButton(this, PALETTE_WAYPOINT_ADD_WAYPOINT, "Add", wxDefaultPosition, wxSize(50, -1));
	add_waypoint_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PLUS, wxSize(16, 16)));
	tmpsizer->Add(add_waypoint_button, 1, wxEXPAND);
	remove_waypoint_button = newd wxButton(this, PALETTE_WAYPOINT_REMOVE_WAYPOINT, "Remove", wxDefaultPosition, wxSize(70, -1));
	remove_waypoint_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_MINUS, wxSize(16, 16)));
	tmpsizer->Add(remove_waypoint_button, 1, wxEXPAND);
	sidesizer->Add(tmpsizer, 0, wxEXPAND | wxTOP, 4);

	SetSizerAndFit(sidesizer);

	Bind(wxEVT_BUTTON, &WaypointPalettePanel::OnClickAddWaypoint, this, PALETTE_WAYPOINT_ADD_WAYPOINT);
	Bind(wxEVT_BUTTON, &WaypointPalettePanel::OnClickRemoveWaypoint, this, PALETTE_WAYPOINT_REMOVE_WAYPOINT);

	Bind(wxEVT_LIST_BEGIN_LABEL_EDIT, &WaypointPalettePanel::OnBeginEditWaypointLabel, this, PALETTE_WAYPOINT_LISTBOX);
	Bind(wxEVT_LIST_END_LABEL_EDIT, &WaypointPalettePanel::OnEditWaypointLabel, this, PALETTE_WAYPOINT_LISTBOX);
	Bind(wxEVT_LIST_ITEM_SELECTED, &WaypointPalettePanel::OnSelectWaypoint, this, PALETTE_WAYPOINT_LISTBOX);
	Bind(wxEVT_LIST_ITEM_ACTIVATED, &WaypointPalettePanel::OnActivateWaypoint, this, PALETTE_WAYPOINT_LISTBOX);
}

void WaypointPalettePanel::OnSwitchIn() {
	PalettePanel::OnSwitchIn();
}

void WaypointPalettePanel::OnSwitchOut() {
	PalettePanel::OnSwitchOut();
}

void WaypointPalettePanel::SetMap(Map* m) {
	map = m;
	this->Enable(m);
}

void WaypointPalettePanel::SelectFirstBrush() {
	// SelectWaypointBrush();
}

Brush* WaypointPalettePanel::GetSelectedBrush() const {
	if (!g_brush_manager.waypoint_brush) {
		return nullptr;
	}
	long item = waypoint_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	g_brush_manager.waypoint_brush->setWaypoint(
		(item == -1 || !map) ? nullptr : map->waypoints.getWaypoint(nstr(waypoint_list->GetItemText(item)))
	);
	return g_brush_manager.waypoint_brush;
}

bool WaypointPalettePanel::SelectBrush(const Brush* whatbrush) {
	return g_brush_manager.waypoint_brush && whatbrush == g_brush_manager.waypoint_brush;
}

int WaypointPalettePanel::GetSelectedBrushSize() const {
	return 0;
}

wxString WaypointPalettePanel::GetName() const {
	return "Waypoint";
}

void WaypointPalettePanel::OnUpdate() {
	if (map) {
		if (wxTextCtrl* tc = waypoint_list->GetEditControl()) {
			Waypoint* wp = map->waypoints.getWaypoint(nstr(tc->GetValue()));
			if (wp && wp->pos == Position()) {
				// Unplaced waypoints are never counted on a tile, drop it as is.
				map->waypoints.removeWaypoint(wp->name);
			}
		}
	}

	std::string selected_name;
	const long selected_index = waypoint_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selected_index != -1) {
		selected_name = nstr(waypoint_list->GetItemText(selected_index));
	} else if (g_brush_manager.waypoint_brush) {
		selected_name = g_brush_manager.waypoint_brush->getWaypoint();
	}

	waypoint_list->Freeze();
	waypoint_list->DeleteAllItems();

	if (!map) {
		waypoint_list->Enable(false);
		add_waypoint_button->Enable(false);
		remove_waypoint_button->Enable(false);
		waypoint_list->Thaw();
		return;
	}

	waypoint_list->Enable(true);
	add_waypoint_button->Enable(true);
	remove_waypoint_button->Enable(true);

	Waypoints& waypoints = map->waypoints;

	long row_to_select = -1;
	for (const auto& [name, wp] : waypoints) {
		const long row = waypoint_list->InsertItem(waypoint_list->GetItemCount(), wxstr(wp->name));
		if (wp->pos != Position()) {
			waypoint_list->SetItem(row, 1, wxstr(std::format("{}, {}, {}", wp->pos.x, wp->pos.y, wp->pos.z)));
		} else {
			waypoint_list->SetItem(row, 1, "—");
		}
		if (!selected_name.empty() && wp->name == selected_name) {
			row_to_select = row;
		}
	}

	if (row_to_select != -1) {
		waypoint_list->SetItemState(row_to_select, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
		waypoint_list->EnsureVisible(row_to_select);
	}
	waypoint_list->Thaw();
}

void WaypointPalettePanel::OnSelectWaypoint(wxListEvent& event) {
	if (!map || !g_brush_manager.waypoint_brush) {
		return;
	}

	const long index = event.GetIndex();
	std::string wpname = nstr(index >= 0 ? waypoint_list->GetItemText(index) : event.GetText());
	Waypoint* wp = map->waypoints.getWaypoint(wpname);
	if (!wp) {
		return;
	}

	g_brush_manager.waypoint_brush->setWaypoint(wp);
}

void WaypointPalettePanel::OnActivateWaypoint(wxListEvent& event) {
	if (!map || !g_brush_manager.waypoint_brush) {
		return;
	}

	const long index = event.GetIndex();
	std::string wpname = nstr(index >= 0 ? waypoint_list->GetItemText(index) : event.GetText());
	Waypoint* wp = map->waypoints.getWaypoint(wpname);
	if (!wp) {
		return;
	}

	g_brush_manager.waypoint_brush->setWaypoint(wp);

	// Only center the view on placed waypoints; an unplaced waypoint
	// (uninitialized position) would otherwise scroll to the map origin.
	if (wp->pos != Position()) {
		g_gui.SetScreenCenterPosition(wp->pos);
	}
}

void WaypointPalettePanel::OnBeginEditWaypointLabel(wxListEvent& event) {
	// We need to disable all hotkeys, so we can type properly
	g_hotkeys.DisableHotkeys();
}

void WaypointPalettePanel::OnEditWaypointLabel(wxListEvent& event) {
	std::string wpname = nstr(event.GetLabel());
	std::string oldwpname = nstr(waypoint_list->GetItemText(event.GetIndex()));
	Waypoint* wp = map->waypoints.getWaypoint(oldwpname);

	if (event.IsEditCancelled()) {
		// A pending "add waypoint" entry registers an unnamed waypoint in the
		// map while the label is being edited; cancelling the edit must not
		// leave that phantom behind, otherwise the map saves a waypoint with
		// no name and an uninitialized position.
		if (oldwpname.empty()) {
			if (Waypoint* phantom = map->waypoints.getWaypoint("")) {
				RemoveWaypointFromMap(phantom);
			}
			if (const long row = event.GetIndex(); row >= 0) {
				waypoint_list->DeleteItem(row);
			}
			g_gui.RefreshPalettes();
		}
		g_hotkeys.EnableHotkeys();
		return;
	}

	if (wpname == "") {
		if (Waypoint* removed = map->waypoints.getWaypoint(oldwpname)) {
			RemoveWaypointFromMap(removed);
		}
		g_gui.RefreshPalettes();
	} else if (wp) {
		if (wpname == oldwpname) {
			; // do nothing
		} else {
			if (map->waypoints.getWaypoint(wpname)) {
				// Already exists a waypoint with this name!
				g_gui.SetStatusText("There already is a waypoint with this name.");
				event.Veto();
				if (oldwpname == "") {
					map->waypoints.removeWaypoint(oldwpname);
					g_gui.RefreshPalettes();
				}
			} else {
				auto nwp_ptr = std::make_unique<Waypoint>(*wp);
				nwp_ptr->name = wpname;
				Waypoint* nwp = nwp_ptr.get();

				Waypoint* rwp = map->waypoints.getWaypoint(oldwpname);
				if (rwp) {
					RemoveWaypointFromMap(rwp);
				}

				map->waypoints.addWaypoint(std::move(nwp_ptr));
				if (g_brush_manager.waypoint_brush) {
					g_brush_manager.waypoint_brush->setWaypoint(nwp);
				}

				// Refresh other palettes
				refresh_timer.Start(300, true);
			}
		}
	}

	if (event.IsAllowed()) {
		g_hotkeys.EnableHotkeys();
	} else {
		g_hotkeys.EnableHotkeys();
	}
}

void WaypointPalettePanel::OnClickAddWaypoint(wxCommandEvent& event) {
	if (map) {
		map->waypoints.addWaypoint(std::make_unique<Waypoint>());
		long i = waypoint_list->InsertItem(0, "");
		waypoint_list->SetItem(i, 1, "—");
		waypoint_list->EditLabel(i);

		// g_gui.RefreshPalettes();
	}
}

void WaypointPalettePanel::OnClickRemoveWaypoint(wxCommandEvent& event) {
	if (!map) {
		return;
	}

	long item = waypoint_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (item != -1) {
		if (Waypoint* wp = map->waypoints.getWaypoint(nstr(waypoint_list->GetItemText(item)))) {
			RemoveWaypointFromMap(wp);
		}
		waypoint_list->DeleteItem(item);
		refresh_timer.Start(300, true);
	}
}

void WaypointPalettePanel::RemoveWaypointFromMap(Waypoint* wp) {
	if (!map || !wp) {
		return;
	}

	// Only placed waypoints contribute to the tile waypoint count; an
	// unplaced one (default position) was never counted, so decrementing
	// it would corrupt the count of whatever tile sits at the origin.
	if (wp->pos != Position()) {
		if (map->getTile(wp->pos)) {
			map->getTileL(wp->pos)->decreaseWaypointCount();
		}
	}

	map->waypoints.removeWaypoint(wp->name);
}
