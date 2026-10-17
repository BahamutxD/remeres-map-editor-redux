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
#include "ui/gui_ids.h"
#include "ui/theme.h"
#include "brushes/managers/brush_manager.h"
#include "editor/hotkey_manager.h"
#include "palette/palette_waypoints.h"
#include "brushes/waypoint/waypoint_brush.h"
#include "map/map.h"
#include "util/image_manager.h"

namespace {

wxColour GetPersistentSelectionBg() {
	return wxSystemSettings::GetAppearance().IsDark() ? wxColour(38, 79, 120) : wxColour(204, 232, 255);
}

wxColour GetPersistentSelectionFg() {
	return wxSystemSettings::GetAppearance().IsDark() ? *wxWHITE : *wxBLACK;
}

class PersistentWaypointListStore : public wxDataViewListStore {
public:
	explicit PersistentWaypointListStore(const std::string& selected_name) :
		selected_name_(selected_name) {}

	bool GetAttrByRow(unsigned int row, unsigned int col, wxDataViewItemAttr& attr) const override {
		if (!selected_name_.empty() && row < m_data.size()) {
			const wxVariant& val = m_data[row]->m_values[0];
			if (nstr(val.GetString()) == selected_name_) {
				attr.SetBackgroundColour(GetPersistentSelectionBg());
				attr.SetColour(GetPersistentSelectionFg());
				return true;
			}
		}
		return false;
	}

private:
	const std::string& selected_name_;
};

} // namespace

WaypointPalettePanel::WaypointPalettePanel(wxWindow* parent, wxWindowID id) :
	PalettePanel(parent, id),
	map(nullptr) {
	wxSizer* sidesizer = newd wxBoxSizer(wxVERTICAL);

	waypoint_list = newd wxDataViewListCtrl(this, PALETTE_WAYPOINT_LISTBOX, wxDefaultPosition, wxDefaultSize, wxDV_SINGLE | wxDV_ROW_LINES);
	auto* store = new PersistentWaypointListStore(selected_waypoint_name_);
	waypoint_list->AssociateModel(store);
	store->DecRef();
	waypoint_list->SetFont(Theme::GetFont(9, false));

	waypoint_list->AppendTextColumn("Name", wxDATAVIEW_CELL_INERT, 110, wxALIGN_LEFT, wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE);
	waypoint_list->AppendTextColumn("Position", wxDATAVIEW_CELL_INERT, 95, wxALIGN_LEFT, wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE);
	sidesizer->Add(waypoint_list, 1, wxEXPAND);

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

	Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &WaypointPalettePanel::OnSelectWaypoint, this, PALETTE_WAYPOINT_LISTBOX);
	Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &WaypointPalettePanel::OnActivateWaypoint, this, PALETTE_WAYPOINT_LISTBOX);
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
	if (!map || !waypoint_list) {
		return nullptr;
	}
	wxDataViewItem item = waypoint_list->GetSelection();
	if (item.IsOk()) {
		int row = waypoint_list->ItemToRow(item);
		if (row != wxNOT_FOUND) {
			wxVariant var;
			waypoint_list->GetValue(var, static_cast<unsigned int>(row), 0);
			return map->waypoints.getWaypoint(nstr(var.GetString()));
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

	waypoint_list->Freeze();
	waypoint_list->DeleteAllItems();

	if (!map) {
		waypoint_list->Enable(false);
		add_waypoint_button->Enable(false);
		rename_waypoint_button->Enable(false);
		remove_waypoint_button->Enable(false);
		waypoint_list->Thaw();
		return;
	}

	waypoint_list->Enable(true);
	add_waypoint_button->Enable(true);
	rename_waypoint_button->Enable(true);
	remove_waypoint_button->Enable(true);

	Waypoints& waypoints = map->waypoints;

	wxDataViewItem item_to_select;
	for (const auto& [name, wp] : waypoints) {
		wxVector<wxVariant> data;
		data.push_back(wxVariant(wxstr(wp->name)));
		if (wp->pos != Position()) {
			data.push_back(wxVariant(wxstr(std::format("{}, {}, {}", wp->pos.x, wp->pos.y, wp->pos.z))));
		} else {
			data.push_back(wxVariant("—"));
		}
		waypoint_list->AppendItem(data);

		if (!selected_waypoint_name_.empty() && wp->name == selected_waypoint_name_) {
			item_to_select = waypoint_list->RowToItem(waypoint_list->GetItemCount() - 1);
		}
	}

	if (item_to_select.IsOk()) {
		waypoint_list->Select(item_to_select);
		waypoint_list->EnsureVisible(item_to_select);
	}
	waypoint_list->Thaw();
	waypoint_list->Refresh();
}

void WaypointPalettePanel::OnSelectWaypoint(wxDataViewEvent& event) {
	if (!map || !g_brush_manager.waypoint_brush) {
		return;
	}

	Waypoint* wp = GetSelectedWaypoint();
	if (!wp) {
		return;
	}

	selected_waypoint_name_ = wp->name;
	g_brush_manager.waypoint_brush->setWaypoint(wp);
	waypoint_list->Refresh();
	g_gui.SelectBrush();
}

void WaypointPalettePanel::OnActivateWaypoint(wxDataViewEvent& event) {
	if (!map || !g_brush_manager.waypoint_brush) {
		return;
	}

	Waypoint* wp = GetSelectedWaypoint();
	if (!wp) {
		return;
	}

	selected_waypoint_name_ = wp->name;
	g_brush_manager.waypoint_brush->setWaypoint(wp);
	waypoint_list->Refresh();
	g_gui.SelectBrush();

	if (wp->pos != Position()) {
		g_gui.SetScreenCenterPosition(wp->pos);
	}
}

void WaypointPalettePanel::OnClickAddWaypoint(wxCommandEvent& event) {
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

void WaypointPalettePanel::OnClickRenameWaypoint(wxCommandEvent& event) {
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

	auto nwp_ptr = std::make_unique<Waypoint>(*wp);
	nwp_ptr->name = new_name;
	Waypoint* nwp = nwp_ptr.get();

	RemoveWaypointFromMap(wp);
	map->waypoints.addWaypoint(std::move(nwp_ptr));

	selected_waypoint_name_ = new_name;
	if (g_brush_manager.waypoint_brush) {
		g_brush_manager.waypoint_brush->setWaypoint(nwp);
	}

	OnUpdate();
	g_gui.SelectBrush();
}

void WaypointPalettePanel::OnClickRemoveWaypoint(wxCommandEvent& event) {
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
