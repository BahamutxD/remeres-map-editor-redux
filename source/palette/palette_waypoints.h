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

#ifndef RME_PALETTE_WAYPOINTS_H_
#define RME_PALETTE_WAYPOINTS_H_

#include <wx/dataview.h>

#include <string>

#include "game/waypoints.h"
#include "palette/palette_common.h"

class WaypointPalettePanel : public PalettePanel {
public:
	WaypointPalettePanel(wxWindow* parent, wxWindowID id = wxID_ANY);
	~WaypointPalettePanel() override = default;

	wxString GetName() const override;

	// Select the first brush
	void SelectFirstBrush() override;
	// Returns the currently selected brush (first brush if panel is not loaded)
	Brush* GetSelectedBrush() const override;
	// Returns the currently selected brush size
	int GetSelectedBrushSize() const override;
	// Select the brush in the parameter, this only changes the look of the panel
	bool SelectBrush(const Brush* whatbrush) override;

	void OnUpdate() override;
	void OnSwitchIn() override;
	void OnSwitchOut() override;

public:
	// wxWidgets event handling
	void OnSelectWaypoint(wxDataViewEvent& event);
	void OnActivateWaypoint(wxDataViewEvent& event);
	void OnClickAddWaypoint(wxCommandEvent& event);
	void OnClickRenameWaypoint(wxCommandEvent& event);
	void OnClickRemoveWaypoint(wxCommandEvent& event);

	void SetMap(Map* map);

protected:
	// Removes a waypoint from the map, keeping the tile waypoint count
	// consistent (unplaced waypoints are never counted).
	void RemoveWaypointFromMap(Waypoint* wp);
	Waypoint* GetSelectedWaypoint() const;

	Map* map;
	std::string selected_waypoint_name_;

	wxDataViewListCtrl* waypoint_list;
	wxButton* add_waypoint_button;
	wxButton* rename_waypoint_button;
	wxButton* remove_waypoint_button;
};

#endif
