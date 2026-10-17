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

#include "app/main.h"

#include "palette/town/edit_town_dialog.h"
#include "map/map.h"
#include "map/tile.h"
#include "game/town.h"
#include "game/house.h"
#include "ui/positionctrl.h"
#include "ui/gui.h"
#include "ui/dialog_util.h"
#include "util/image_manager.h"
#include "ui/managers/minimap_manager.h"

EditTownDialog::EditTownDialog(wxWindow* parent, Map* map, Town* town) :
	wxDialog(parent, wxID_ANY, "Edit Town", wxDefaultPosition, FROM_DIP(parent, wxSize(300, 260))),
	map(map),
	town(town) {
	ASSERT(map);
	ASSERT(town);

	SetTitle(wxstr(std::format("Edit Town: {} (ID: {})", town->getName(), town->getID())));

	auto* topsizer = newd wxBoxSizer(wxVERTICAL);

	// Town Details static box
	auto* detailsBox = newd wxStaticBoxSizer(wxVERTICAL, this, "Town Details");
	auto* detailsGrid = newd wxFlexGridSizer(2, 5, 10);
	detailsGrid->AddGrowableCol(1);

	// Name
	town_name = wxstr(town->getName());
	detailsGrid->Add(newd wxStaticText(detailsBox->GetStaticBox(), wxID_ANY, "Name:"), wxSizerFlags(0).CenterVertical().Border(wxLEFT, 5));
	name_field = newd wxTextCtrl(detailsBox->GetStaticBox(), wxID_ANY, town_name, wxDefaultPosition, wxDefaultSize, 0, wxTextValidator(wxFILTER_ASCII, &town_name));
	name_field->SetToolTip("Town display name");
	detailsGrid->Add(name_field, wxSizerFlags(1).Expand());

	// ID
	detailsGrid->Add(newd wxStaticText(detailsBox->GetStaticBox(), wxID_ANY, "ID:"), wxSizerFlags(0).CenterVertical().Border(wxLEFT, 5));
	id_field = newd wxSpinCtrl(detailsBox->GetStaticBox(), wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 0xFFFF, static_cast<int>(town->getID()));
	id_field->SetToolTip("Town ID (1-65535). Warning: changing existing ID affects linked houses and server player spawn data.");
	detailsGrid->Add(id_field, wxSizerFlags(1).Expand());

	detailsBox->Add(detailsGrid, 1, wxEXPAND | wxALL, 5);
	topsizer->Add(detailsBox, 0, wxEXPAND | wxALL, 10);

	// Temple Position
	temple_position = newd PositionCtrl(this, "Temple Position", 0, 0, 0, map->getWidth(), map->getHeight());
	temple_position->SetPosition(town->getTemplePosition());

	auto* goto_button = newd wxButton(temple_position->GetStaticBox(), wxID_ANY, "Go To");
	goto_button->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_LOCATION_ARROW));
	goto_button->SetToolTip("Center map camera on temple position");
	temple_position->Add(goto_button, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
	topsizer->Add(temple_position, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

	// OK / Cancel buttons
	auto* btnSizer = newd wxBoxSizer(wxHORIZONTAL);
	auto* okBtn = newd wxButton(this, wxID_OK, "OK");
	okBtn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_CHECK));
	okBtn->SetToolTip("Save changes");
	btnSizer->Add(okBtn, wxSizerFlags(1).Center());

	auto* cancelBtn = newd wxButton(this, wxID_CANCEL, "Cancel");
	cancelBtn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_XMARK));
	cancelBtn->SetToolTip("Cancel");
	btnSizer->Add(cancelBtn, wxSizerFlags(1).Center());

	topsizer->Add(btnSizer, 0, wxCENTER | wxALL, 10);

	SetSizerAndFit(topsizer);
	Centre(wxBOTH);

	goto_button->Bind(wxEVT_BUTTON, &EditTownDialog::OnClickSelectTemplePosition, this);
	okBtn->Bind(wxEVT_BUTTON, &EditTownDialog::OnClickOK, this);
	cancelBtn->Bind(wxEVT_BUTTON, &EditTownDialog::OnClickCancel, this);

	SetIcons(IMAGE_MANAGER.GetIconBundle(ICON_CITY));
}

EditTownDialog::~EditTownDialog() = default;

void EditTownDialog::OnClickSelectTemplePosition(wxCommandEvent& WXUNUSED(event)) {
	Position templepos = temple_position->GetPosition();
	if (templepos.isValid()) {
		g_gui.SetScreenCenterPosition(templepos);
	}
}

void EditTownDialog::OnClickOK(wxCommandEvent& WXUNUSED(event)) {
	if (!Validate() || !TransferDataFromWindow()) {
		return;
	}

	wxString new_name = name_field->GetValue().Trim();
	if (new_name.IsEmpty()) {
		DialogUtil::PopupDialog(this, "Error", "Town name cannot be empty.", wxOK);
		return;
	}

	Position new_temple = temple_position->GetPosition();
	if (!new_temple.isValid() || new_temple.x > map->getWidth() || new_temple.y > map->getHeight()) {
		DialogUtil::PopupDialog(this, "Error", "Invalid temple position specified.", wxOK);
		return;
	}

	uint32_t new_id = static_cast<uint32_t>(id_field->GetValue());
	uint32_t old_id = town->getID();
	bool cascade_houses = false;

	if (new_id != old_id) {
		if (map->towns.getTown(new_id) != nullptr) {
			DialogUtil::PopupDialog(this, "Error", wxstr(std::format("A town with ID {} already exists.", new_id)), wxOK);
			return;
		}

		size_t house_count = 0;
		for (const auto& [_, house] : map->houses) {
			if (house && house->townid == old_id) {
				++house_count;
			}
		}

		if (house_count > 0) {
			wxString msg = wxString::Format(
				"Changing Town ID from %u to %u has major consequences:\n\n"
				"• OTBM Map & Houses: %zu houses are currently linked to this town.\n"
				"• Server Database: Player spawn points ('players' table) and NPC scripts rely on Town ID.\n\n"
				"Do you want to update all %zu associated houses to use the new Town ID %u?",
				old_id, new_id, house_count, house_count, new_id
			);
			int answer = DialogUtil::PopupDialog(this, "Warning: Town ID Changed", msg, wxYES_NO | wxCANCEL);
			if (answer == wxCANCEL || answer == wxID_CANCEL) {
				return;
			}
			if (answer == wxYES || answer == wxID_YES) {
				cascade_houses = true;
			}
		} else {
			int answer = DialogUtil::PopupDialog(this, "Warning",
				"Changing existing town IDs on a production server WILL HAVE DATABASE CONSEQUENCES\n"
				"such as invalidating player spawn points and scripts.\n\n"
				"Are you sure you want to continue?", wxYES | wxNO);
			if (answer == wxNO || answer == wxID_NO) {
				return;
			}
		}

		if (!map->towns.changeId(town, new_id)) {
			DialogUtil::PopupDialog(this, "Error", "Failed to change Town ID.", wxOK);
			return;
		}

		if (cascade_houses) {
			map->updateTownIdForHouses(old_id, new_id);
		}
	}

	// Update Name
	town->setName(nstr(new_name));

	// Update Temple Position
	Position old_temple = town->getTemplePosition();
	if (old_temple != new_temple) {
		if (old_temple != Position()) {
			if (auto* tile = map->getTile(old_temple)) {
				if (auto* loc = tile->getLocation()) {
					loc->decreaseTownCount();
				}
			}
		}

		if (auto* tile = map->getOrCreateTile(new_temple)) {
			if (auto* loc = tile->getLocation()) {
				loc->increaseTownCount();
			}
		}

		town->setTemplePosition(new_temple);

		if (old_temple != Position() && old_temple.isValid()) {
			map->getChangeTracker().markTileDirty(old_temple);
			g_minimap.MarkTileDirty(*map, old_temple);
		}
		if (new_temple.isValid()) {
			map->getChangeTracker().markTileDirty(new_temple);
			g_minimap.MarkTileDirty(*map, new_temple);
		}
	}

	map->doChange();
	g_gui.RefreshPalettes();
	EndModal(wxID_OK);
}

void EditTownDialog::OnClickCancel(wxCommandEvent& WXUNUSED(event)) {
	EndModal(wxID_CANCEL);
}
