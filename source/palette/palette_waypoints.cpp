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
#include <algorithm>

#include "ui/gui.h"
#include "ui/gui_ids.h"
#include "ui/theme.h"
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

	waypoint_table = newd NanoVGTableCtrl(this, PALETTE_WAYPOINT_LISTBOX);
	std::vector<TableColumn> cols = {
		{ "name", "Name", 0, NVG_ALIGN_LEFT, true },
		{ "pos", "Position", 110, NVG_ALIGN_LEFT, true }
	};
	waypoint_table->SetColumns(std::move(cols));

	waypoint_table->SetOnSelect([this](int idx, const TableRow& row) {
		OnSelectWaypoint(idx, row);
	});
	waypoint_table->SetOnActivate([this](int idx, const TableRow& row) {
		OnActivateWaypoint(idx, row);
	});
	waypoint_table->SetOnSort([this](int col, bool asc) {
		OnSortWaypoints(col, asc);
	});

	sidesizer->Add(waypoint_table, 1, wxEXPAND);

	wxSizer* tmpsizer = newd wxBoxSizer(wxHORIZONTAL);
	add_waypoint_button = newd wxButton(this, PALETTE_WAYPOINT_ADD_WAYPOINT, "Add", wxDefaultPosition, wxSize(50, -1));
	add_waypoint_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PLUS, wxSize(16, 16)));
	add_waypoint_button->SetFont(Theme::GetFont(9, false));
	tmpsizer->Add(add_waypoint_button, 1, wxEXPAND | wxRIGHT, 2);

	rename_waypoint_button = newd wxButton(this, PALETTE_WAYPOINT_RENAME_WAYPOINT, "Rename", wxDefaultPosition, wxSize(60, -1));
	rename_waypoint_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PEN, wxSize(16, 16)));
	rename_waypoint_button->SetFont(Theme::GetFont(9, false));
	tmpsizer->Add(rename_waypoint_button, 1, wxEXPAND | wxRIGHT, 2);

	remove_waypoint_button = newd wxButton(this, PALETTE_WAYPOINT_REMOVE_WAYPOINT, "Remove", wxDefaultPosition, wxSize(60, -1));
	remove_waypoint_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_MINUS, wxSize(16, 16)));
	remove_waypoint_button->SetFont(Theme::GetFont(9, false));
	tmpsizer->Add(remove_waypoint_button, 1, wxEXPAND);
	sidesizer->Add(tmpsizer, 0, wxEXPAND | wxTOP, 4);

	SetSizerAndFit(sidesizer);

	Bind(wxEVT_BUTTON, &WaypointPalettePanel::OnClickAddWaypoint, this, PALETTE_WAYPOINT_ADD_WAYPOINT);
	Bind(wxEVT_BUTTON, &WaypointPalettePanel::OnClickRenameWaypoint, this, PALETTE_WAYPOINT_RENAME_WAYPOINT);
	Bind(wxEVT_BUTTON, &WaypointPalettePanel::OnClickRemoveWaypoint, this, PALETTE_WAYPOINT_REMOVE_WAYPOINT);
}

void WaypointPalettePanel::OnSwitchIn() {
	PalettePanel::OnSwitchIn();
}

void WaypointPalettePanel::OnSwitchOut() {
	PalettePanel::OnSwitchOut();
}

void WaypointPalettePanel::SetMap(Map* m) {
	if (map != m) {
		selected_waypoint_name_.clear();
		if (g_brush_manager.waypoint_brush) {
			g_brush_manager.waypoint_brush->setWaypoint(nullptr);
		}
	}
	map = m;
	this->Enable(m != nullptr);
	OnUpdate();
}

void WaypointPalettePanel::SelectFirstBrush() {
}

Waypoint* WaypointPalettePanel::GetSelectedWaypoint() const {
	if (!map) {
		return nullptr;
	}
	if (waypoint_table) {
		const TableRow* row = waypoint_table->GetSelectedRow();
		if (row && !row->key.empty()) {
			return map->waypoints.getWaypoint(row->key);
		}
	}
	if (!selected_waypoint_name_.empty()) {
		return map->waypoints.getWaypoint(selected_waypoint_name_);
	}
	return nullptr;
}

Brush* WaypointPalettePanel::GetSelectedBrush() const {
	if (!g_brush_manager.waypoint_brush) {
		return nullptr;
	}
	Waypoint* wp = GetSelectedWaypoint();
	g_brush_manager.waypoint_brush->setWaypoint(wp);
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
	if (selected_waypoint_name_.empty() && g_brush_manager.waypoint_brush) {
		selected_waypoint_name_ = g_brush_manager.waypoint_brush->getWaypoint();
	}

	if (!map) {
		waypoint_table->ClearRows();
		waypoint_table->Enable(false);
		add_waypoint_button->Enable(false);
		rename_waypoint_button->Enable(false);
		remove_waypoint_button->Enable(false);
		return;
	}

	waypoint_table->Enable(true);
	add_waypoint_button->Enable(true);
	rename_waypoint_button->Enable(true);
	remove_waypoint_button->Enable(true);

	std::vector<TableRow> rows;
	for (const auto& [name, wp] : map->waypoints) {
		TableRow row;
		row.key = wp->name;
		row.cells.push_back(wp->name);
		if (wp->pos != Position()) {
			row.cells.push_back(std::format("{}, {}, {}", wp->pos.x, wp->pos.y, wp->pos.z));
		} else {
			row.cells.push_back("—");
		}
		row.client_data = wp.get();
		rows.push_back(std::move(row));
	}

	const int sort_col = waypoint_table->GetSortColumn();
	const bool asc = waypoint_table->IsSortAscending();

	if (sort_col == 1) {
		// Sort by position (X, then Y, then Z)
		std::stable_sort(rows.begin(), rows.end(), [asc](const TableRow& a, const TableRow& b) {
			auto* wpA = static_cast<const Waypoint*>(a.client_data);
			auto* wpB = static_cast<const Waypoint*>(b.client_data);
			if (!wpA || !wpB) {
				return false;
			}
			const bool aPlaced = (wpA->pos != Position());
			const bool bPlaced = (wpB->pos != Position());
			if (aPlaced != bPlaced) {
				return asc ? aPlaced : bPlaced;
			}
			if (wpA->pos != wpB->pos) {
				return asc ? (wpA->pos < wpB->pos) : (wpB->pos < wpA->pos);
			}
			return asc ? (wpA->name < wpB->name) : (wpB->name < wpA->name);
		});
	} else if (sort_col == 0) {
		// Sort by name
		std::stable_sort(rows.begin(), rows.end(), [asc](const TableRow& a, const TableRow& b) {
			int cmp = wxStricmp(wxstr(a.cells[0]), wxstr(b.cells[0]));
			return asc ? (cmp < 0) : (cmp > 0);
		});
	}

	waypoint_table->SetRows(std::move(rows));

	if (!selected_waypoint_name_.empty()) {
		waypoint_table->SelectByKey(selected_waypoint_name_);
	}
}

void WaypointPalettePanel::OnSelectWaypoint(int /*index*/, const TableRow& row) {
	if (!map || !g_brush_manager.waypoint_brush) {
		return;
	}

	Waypoint* wp = map->waypoints.getWaypoint(row.key);
	if (!wp) {
		return;
	}

	selected_waypoint_name_ = wp->name;
	g_brush_manager.waypoint_brush->setWaypoint(wp);
	g_gui.SelectBrush();
}

void WaypointPalettePanel::OnActivateWaypoint(int /*index*/, const TableRow& row) {
	if (!map || !g_brush_manager.waypoint_brush) {
		return;
	}

	Waypoint* wp = map->waypoints.getWaypoint(row.key);
	if (!wp) {
		return;
	}

	selected_waypoint_name_ = wp->name;
	g_brush_manager.waypoint_brush->setWaypoint(wp);
	g_gui.SelectBrush();

	if (wp->pos != Position()) {
		g_gui.SetScreenCenterPosition(wp->pos);
	}
}

void WaypointPalettePanel::OnSortWaypoints(int /*column*/, bool /*ascending*/) {
	OnUpdate();
}

void WaypointPalettePanel::OnClickAddWaypoint(wxCommandEvent& /*event*/) {
	if (!map) {
		return;
	}

	const std::string name = nstr(wxGetTextFromUser("Waypoint name:", "Add Waypoint", wxEmptyString, this).Trim().Trim(false));
	if (name.empty()) {
		return;
	}

	if (map->waypoints.getWaypoint(name)) {
		g_gui.SetStatusText("A waypoint with this name already exists.");
		return;
	}

	auto wp = std::make_unique<Waypoint>();
	wp->name = name;
	Waypoint* wp_ptr = wp.get();
	map->waypoints.addWaypoint(std::move(wp));

	selected_waypoint_name_ = name;
	if (g_brush_manager.waypoint_brush) {
		g_brush_manager.waypoint_brush->setWaypoint(wp_ptr);
	}

	OnUpdate();
	g_gui.SelectBrush();
}

void WaypointPalettePanel::OnClickRenameWaypoint(wxCommandEvent& /*event*/) {
	if (!map) {
		return;
	}

	Waypoint* wp = GetSelectedWaypoint();
	if (!wp) {
		return;
	}

	const std::string old_name = wp->name;
	const std::string new_name = nstr(wxGetTextFromUser("New waypoint name:", "Rename Waypoint", wxstr(old_name), this).Trim().Trim(false));
	if (new_name.empty() || new_name == old_name) {
		return;
	}

	if (map->waypoints.getWaypoint(new_name)) {
		g_gui.SetStatusText("A waypoint with this name already exists.");
		return;
	}

	if (!map->waypoints.renameWaypoint(old_name, new_name)) {
		g_gui.SetStatusText("Failed to rename waypoint.");
		return;
	}

	selected_waypoint_name_ = new_name;
	Waypoint* nwp = map->waypoints.getWaypoint(new_name);
	if (g_brush_manager.waypoint_brush) {
		g_brush_manager.waypoint_brush->setWaypoint(nwp);
	}

	OnUpdate();
	g_gui.SelectBrush();
}

void WaypointPalettePanel::OnClickRemoveWaypoint(wxCommandEvent& /*event*/) {
	if (!map) {
		return;
	}

	Waypoint* wp = GetSelectedWaypoint();
	if (!wp) {
		return;
	}

	const std::string name = wp->name;
	RemoveWaypointFromMap(wp);
	map->waypoints.removeWaypoint(name);

	if (selected_waypoint_name_ == name) {
		selected_waypoint_name_.clear();
		if (g_brush_manager.waypoint_brush) {
			g_brush_manager.waypoint_brush->setWaypoint(nullptr);
		}
	}

	OnUpdate();
	g_gui.SelectBrush();
}

void WaypointPalettePanel::RemoveWaypointFromMap(Waypoint* wp) {
	if (!map || !wp) {
		return;
	}

	if (wp->pos != Position()) {
		if (map->getTile(wp->pos)) {
			map->getTileL(wp->pos)->decreaseWaypointCount();
		}
	}
}
