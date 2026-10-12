////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "palette/palette_creatures.h"

#include "brushes/managers/brush_manager.h"
#include "brushes/creature/creature_brush.h"
#include "brushes/spawn/spawn_brush.h"
#include "game/creatures.h"
#include "map/map.h"
#include "ui/gui.h"

#include <wx/textctrl.h>

#include <algorithm>

#include <algorithm>

CreaturePalettePanel::CreaturePalettePanel(wxWindow* parent, bool npc, wxWindowID id) :
	PalettePanel(parent, id), npc(npc) {
	auto* sidesizer = newd wxStaticBoxSizer(wxVERTICAL, this, npc ? "Npcs" : "Monsters");
	wxWindow* box = sidesizer->GetStaticBox();

	auto* searchSizer = newd wxBoxSizer(wxHORIZONTAL);
	search_text = newd wxTextCtrl(box, PALETTE_CREATURE_SEARCH, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	search_text->SetHint(npc ? "Search npc" : "Search monster");
	searchSizer->Add(search_text, 1, wxEXPAND | wxRIGHT, 4);
	auto* search_button = newd wxButton(box, PALETTE_CREATURE_SEARCH_BUTTON, "Search");
	searchSizer->Add(search_button, 0, wxEXPAND);
	sidesizer->Add(searchSizer, 0, wxEXPAND | wxBOTTOM, 4);

	// NB: no wxLB_SORT here: shown_ is pre-sorted and list rows must stay
	// index-aligned with it (GetSelectedBrush/SelectBrush use row indices).
	creature_list = newd wxListBox(box, PALETTE_CREATURE_LISTBOX, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxLB_SINGLE);
	sidesizer->Add(creature_list, 1, wxEXPAND);

	auto* brushSizer = newd wxStaticBoxSizer(wxVERTICAL, box, "Brushes");
	auto* grid = newd wxFlexGridSizer(4, 3, 6, 6);
	grid->AddGrowableCol(1);

	grid->Add(newd wxStaticText(box, wxID_ANY, "Spawntime"), 0, wxALIGN_CENTER_VERTICAL);
	spawntime_spin = newd wxSpinCtrl(box, PALETTE_CREATURE_SPAWN_TIME, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 3600, g_gui.GetSpawnTime());
	grid->Add(spawntime_spin, 0, wxEXPAND);
	place_button = newd wxToggleButton(box, PALETTE_CREATURE_BRUSH_BUTTON, npc ? "Place Npc" : "Place Monster");
	grid->Add(place_button, 0, wxEXPAND);

	grid->Add(newd wxStaticText(box, wxID_ANY, "Spawn size"), 0, wxALIGN_CENTER_VERTICAL);
	spawns_size_spin = newd wxSpinCtrl(box, PALETTE_CREATURE_SPAWN_SIZE, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 30, std::max(1, g_gui.GetBrushSize()));
	grid->Add(spawns_size_spin, 0, wxEXPAND);
	spawn_button = newd wxToggleButton(box, PALETTE_SPAWN_BRUSH_BUTTON, "Place Spawn");
	grid->Add(spawn_button, 0, wxEXPAND);

	if (!npc) {
		grid->Add(newd wxStaticText(box, wxID_ANY, "Default weight %"), 0, wxALIGN_CENTER_VERTICAL);
		weight_spin = newd wxSpinCtrl(box, PALETTE_CREATURE_WEIGHT, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 100, g_gui.GetDefaultCreatureWeight());
		grid->Add(weight_spin, 0, wxEXPAND);
		grid->AddSpacer(0);
	} else {
		weight_spin = nullptr;
	}

	brushSizer->Add(grid, 0, wxEXPAND);
	sidesizer->Add(brushSizer, 0, wxEXPAND | wxTOP, 4);

	SetSizerAndFit(sidesizer);

	Bind(wxEVT_BUTTON, &CreaturePalettePanel::OnSearch, this, PALETTE_CREATURE_SEARCH_BUTTON);
	Bind(wxEVT_TEXT_ENTER, &CreaturePalettePanel::OnSearch, this, PALETTE_CREATURE_SEARCH);
	Bind(wxEVT_LISTBOX, &CreaturePalettePanel::OnSelectCreature, this, PALETTE_CREATURE_LISTBOX);
	Bind(wxEVT_TOGGLEBUTTON, &CreaturePalettePanel::OnClickPlaceButton, this, PALETTE_CREATURE_BRUSH_BUTTON);
	Bind(wxEVT_TOGGLEBUTTON, &CreaturePalettePanel::OnClickSpawnButton, this, PALETTE_SPAWN_BRUSH_BUTTON);
	Bind(wxEVT_SPINCTRL, &CreaturePalettePanel::OnChangeSpawnTime, this, PALETTE_CREATURE_SPAWN_TIME);
	Bind(wxEVT_SPINCTRL, &CreaturePalettePanel::OnChangeSpawnSize, this, PALETTE_CREATURE_SPAWN_SIZE);
	if (weight_spin) {
		Bind(wxEVT_SPINCTRL, &CreaturePalettePanel::OnChangeWeight, this, PALETTE_CREATURE_WEIGHT);
	}

	selectPlaceMode();
	OnUpdate();
}

wxString CreaturePalettePanel::GetName() const {
	return npc ? "Npc" : "Monster";
}

void CreaturePalettePanel::SetMap(Map* m) {
	map = m;
	OnUpdate();
}

bool CreaturePalettePanel::supportsSpawns() const {
	return map && map->getVersion().otbm == MAP_OTBM_5;
}

void CreaturePalettePanel::refreshList() {
	const std::string query = nstr(search_text->GetValue().Lower());
	CreatureType* previous = nullptr;
	const int prevSel = creature_list->GetSelection();
	if (prevSel != wxNOT_FOUND && static_cast<size_t>(prevSel) < shown_.size()) {
		previous = shown_[static_cast<size_t>(prevSel)];
	}
	creature_list->Freeze();
	creature_list->Clear();
	shown_.clear();
	for (auto&& [name, type] : g_creatures) {
		if (!type || type->isNpc != npc || !type->brush) {
			continue;
		}
		if (!query.empty()) {
			const std::string lname = nstr(wxString::FromUTF8(name.c_str()).Lower());
			if (lname.find(query) == std::string::npos) {
				continue;
			}
		}
		shown_.push_back(type);
	}
	std::ranges::sort(shown_, {}, &CreatureType::name);
	for (CreatureType* type : shown_) {
		creature_list->Append(wxstr(type->name), type);
	}
	if (!shown_.empty()) {
		const auto it = std::ranges::find(shown_, previous);
		creature_list->SetSelection(it != shown_.end() ? static_cast<int>(it - shown_.begin()) : 0);
	}
	creature_list->Thaw();
}

void CreaturePalettePanel::selectPlaceMode() {
	place_button->SetValue(true);
	spawn_button->SetValue(false);
}

void CreaturePalettePanel::selectSpawnMode() {
	place_button->SetValue(false);
	spawn_button->SetValue(true);
}

void CreaturePalettePanel::syncBrushFromSelection() {
	selectPlaceMode();
	applySpinValues();
	g_gui.SelectBrush();
}

void CreaturePalettePanel::applySpinValues() {
	g_gui.SetBrushSize(spawns_size_spin->GetValue());
	g_gui.SetSpawnTime(spawntime_spin->GetValue());
	if (weight_spin) {
		g_gui.SetDefaultCreatureWeight(weight_spin->GetValue());
	}
}

Brush* CreaturePalettePanel::GetSelectedBrush() const {
	// NB: side-effect free on purpose. The framework calls this getter
	// reentrantly (tool options refresh queries the selected creature brush),
	// so applying spin values here recursed into SelectBrush until the stack
	// overflowed. Spins are applied in the click/change handlers instead.
	if (!supportsSpawns()) {
		return nullptr;
	}
	if (spawn_button->GetValue()) {
		return g_brush_manager.spawn_brush;
	}
	const int sel = creature_list->GetSelection();
	if (sel == wxNOT_FOUND || static_cast<size_t>(sel) >= shown_.size()) {
		return nullptr;
	}
	CreatureType* type = shown_[static_cast<size_t>(sel)];
	if (!type || !type->brush) {
		return nullptr;
	}
	return type->brush;
}

int CreaturePalettePanel::GetSelectedBrushSize() const {
	return spawns_size_spin->GetValue();
}

bool CreaturePalettePanel::SelectBrush(const Brush* whatbrush) {
	if (!whatbrush || !supportsSpawns()) {
		return false;
	}
	if (whatbrush == g_brush_manager.spawn_brush) {
		selectSpawnMode();
		return true;
	}
	for (size_t i = 0; i < shown_.size(); ++i) {
		if (shown_[i] && shown_[i]->brush == whatbrush) {
			creature_list->SetSelection(static_cast<int>(i));
			selectPlaceMode();
			return true;
		}
	}
	return false;
}

void CreaturePalettePanel::OnUpdate() {
	const bool enabled = supportsSpawns();
	refreshing = true;
	refreshList();
	refreshing = false;

	creature_list->Enable(enabled);
	search_text->Enable(enabled);
	spawntime_spin->Enable(enabled);
	spawns_size_spin->Enable(enabled);
	if (weight_spin) {
		weight_spin->Enable(enabled);
	}
	place_button->Enable(enabled);
	spawn_button->Enable(enabled);
	const wxString tip = (enabled || !map) ? wxString() : wxString("Creature placement requires an OTBM 5 map (Canary / Crystal Server).");
	creature_list->SetToolTip(tip);
}

void CreaturePalettePanel::OnSearch(wxCommandEvent& /*event*/) {
	if (!refreshing) {
		refreshList();
		// Filtering reselects row 0 (or the preserved row); sync the brush so
		// the active brush matches the visible selection.
		syncBrushFromSelection();
	}
}

void CreaturePalettePanel::OnSelectCreature(wxCommandEvent& /*event*/) {
	if (!refreshing) {
		syncBrushFromSelection();
	}
}

void CreaturePalettePanel::OnClickPlaceButton(wxCommandEvent& /*event*/) {
	selectPlaceMode();
	applySpinValues();
	g_gui.SelectBrush();
}

void CreaturePalettePanel::OnClickSpawnButton(wxCommandEvent& /*event*/) {
	selectSpawnMode();
	applySpinValues();
	g_gui.SelectBrush();
}

void CreaturePalettePanel::OnChangeSpawnTime(wxSpinEvent& /*event*/) {
	g_gui.SetSpawnTime(spawntime_spin->GetValue());
}

void CreaturePalettePanel::OnChangeSpawnSize(wxSpinEvent& /*event*/) {
	g_gui.SetBrushSize(spawns_size_spin->GetValue());
}

void CreaturePalettePanel::OnChangeWeight(wxSpinEvent& /*event*/) {
	if (weight_spin) {
		g_gui.SetDefaultCreatureWeight(weight_spin->GetValue());
	}
}
