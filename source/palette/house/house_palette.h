//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_HOUSE_PALETTE_H_
#define RME_HOUSE_PALETTE_H_

#include "palette/palette_common.h"
#include "util/nanovg_table_ctrl.h"
#include "brushes/brush.h"

#include <wx/panel.h>
#include <wx/choice.h>
#include <wx/textctrl.h>
#include <wx/tglbtn.h>
#include <memory>

class Map;
class House;
class Brush;

class HousePalette : public PalettePanel {
public:
	HousePalette(wxWindow* parent);
	~HousePalette() override;

	void SetMap(Map* map);
	void UpdateHouses();
	void UpdateHouseSize(uint32_t house_id = 0);

	// Palette operations
	wxString GetName() const override;
	Brush* GetSelectedBrush() const override;
	bool SelectBrush(const Brush* whatbrush) override;
	void OnUpdate() override;
	void SelectHouseBrush();
	void SelectExitBrush();

	// Event handlers
	void OnTownChange(wxCommandEvent& event);
	void OnSearchChange(wxCommandEvent& event);
	void OnHouseSelected(int index, const TableRow& row);
	void OnHouseActivated(int index, const TableRow& row);
	void OnHouseSort(int column, bool ascending);

	void OnAddHouse(wxCommandEvent& event);
	void OnEditHouse(wxCommandEvent& event);
	void OnRemoveHouse(wxCommandEvent& event);

	void OnHouseBrushButton(wxCommandEvent& event);
	void OnSelectExitButton(wxCommandEvent& event);

	void OnSearchCharHook(wxKeyEvent& event);

protected:
	void FilterHouses();
	House* GetSelectedHouse() const;

	Map* map = nullptr;

	uint32_t selected_house_id_ = 0;

	wxChoice* town_choice = nullptr;
	wxTextCtrl* search_ctrl = nullptr;
	NanoVGTableCtrl* house_table = nullptr;

	wxToggleButton* house_brush_button = nullptr;
	wxToggleButton* select_exit_button = nullptr;

	wxButton* add_button = nullptr;
	wxButton* edit_button = nullptr;
	wxButton* remove_button = nullptr;
};

#endif
