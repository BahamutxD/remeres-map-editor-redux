////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_CREATURES_H_
#define RME_PALETTE_CREATURES_H_

#include <wx/listbox.h>
#include <wx/spinctrl.h>
#include <wx/tglbtn.h>

#include <string>
#include <vector>

#include "palette/palette_common.h"

class CreatureType;

// Crystal-style creature placement (Canary / Crystal Server, OTBM 5 only):
// searchable monster/NPC list with spawntime, spawn size and default weight,
// plus Place Monster/NPC and Place Spawn toggle buttons. One class serves both
// lists; the registry creates a "Monster" (npc=false) and an "Npc" (npc=true)
// page from it.
class CreaturePalettePanel : public PalettePanel {
public:
	explicit CreaturePalettePanel(wxWindow* parent, bool npc, wxWindowID id = wxID_ANY);
	~CreaturePalettePanel() override = default;

	wxString GetName() const override;

	void SelectFirstBrush() override { }
	Brush* GetSelectedBrush() const override;
	int GetSelectedBrushSize() const override;
	bool SelectBrush(const Brush* whatbrush) override;
	void OnUpdate() override;

	void SetMap(Map* map);

private:
	[[nodiscard]] bool supportsSpawns() const;
	void refreshList();
	void selectPlaceMode();
	void selectSpawnMode();
	void applySpinValues();
	void syncBrushFromSelection();

	void OnSearch(wxCommandEvent& event);
	void OnSelectCreature(wxCommandEvent& event);
	void OnClickPlaceButton(wxCommandEvent& event);
	void OnClickSpawnButton(wxCommandEvent& event);
	void OnChangeSpawnTime(wxSpinEvent& event);
	void OnChangeSpawnSize(wxSpinEvent& event);
	void OnChangeWeight(wxSpinEvent& event);

	const bool npc;
	Map* map = nullptr;
	bool refreshing = false;
	std::vector<class CreatureType*> shown_;

	wxTextCtrl* search_text;
	wxListBox* creature_list;
	wxSpinCtrl* spawntime_spin;
	wxSpinCtrl* spawns_size_spin;
	wxSpinCtrl* weight_spin;
	wxToggleButton* place_button;
	wxToggleButton* spawn_button;
};

#endif
