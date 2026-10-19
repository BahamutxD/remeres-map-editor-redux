#include "ui/dialogs/collection_options_panel.h"

#include "io/collection_xml_service.h"
#include "util/common.h"

#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/msgdlg.h>
#include <wx/dialog.h>

#include <algorithm>
#include <format>

namespace rme::ui {

namespace {

class CollisionDialog : public wxDialog {
public:
	enum Action {
		Append = 1001,
		Overwrite,
		Rename,
		Cancel
	};

	CollisionDialog(wxWindow* parent, const std::string& tilesetName) :
		wxDialog(parent, wxID_ANY, "Tileset Already Exists", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE) {
		wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

		wxString msg = wxString::Format(
			"A tileset named \"%s\" already exists in Collections.\n\nWould you like to append these elements to it instead?",
			wxstr(tilesetName)
		);
		wxStaticText* text = new wxStaticText(this, wxID_ANY, msg);
		mainSizer->Add(text, 0, wxALL, 16);

		wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
		wxButton* appendBtn = new wxButton(this, wxID_ANY, "&Append");
		wxButton* overwriteBtn = new wxButton(this, wxID_ANY, "&Overwrite");
		wxButton* renameBtn = new wxButton(this, wxID_ANY, "&Rename");
		wxButton* cancelBtn = new wxButton(this, wxID_CANCEL, "&Cancel");

		appendBtn->SetDefault();

		appendBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(Append); });
		overwriteBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(Overwrite); });
		renameBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(Rename); });
		cancelBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(Cancel); });

		btnSizer->Add(appendBtn, 0, wxRIGHT, 6);
		btnSizer->Add(overwriteBtn, 0, wxRIGHT, 6);
		btnSizer->Add(renameBtn, 0, wxRIGHT, 6);
		btnSizer->Add(cancelBtn, 0);

		mainSizer->Add(btnSizer, 0, wxALIGN_RIGHT | wxBOTTOM | wxLEFT | wxRIGHT, 16);
		SetSizerAndFit(mainSizer);
		CenterOnParent();
	}
};

} // namespace

CollectionOptionsPanel::CollectionOptionsPanel(wxWindow* parent, Callbacks callbacks) :
	wxPanel(parent, wxID_ANY),
	m_callbacks(std::move(callbacks)) {
	InitUI();
}

void CollectionOptionsPanel::InitUI() {
	wxBoxSizer* rootSizer = new wxBoxSizer(wxVERTICAL);

	// Group: Target Tileset Mode
	wxStaticBoxSizer* targetBox = new wxStaticBoxSizer(wxVERTICAL, this, "Target Tileset");

	m_radioNew = new wxRadioButton(targetBox->GetStaticBox(), wxID_ANY, "Create New Tileset", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	targetBox->Add(m_radioNew, 0, wxTOP | wxLEFT | wxRIGHT, 6);

	m_nameInput = new wxTextCtrl(targetBox->GetStaticBox(), wxID_ANY, wxstr(GenerateDefaultTilesetName()));
	targetBox->Add(m_nameInput, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

	m_radioExisting = new wxRadioButton(targetBox->GetStaticBox(), wxID_ANY, "Add to Existing Tileset");
	targetBox->Add(m_radioExisting, 0, wxTOP | wxLEFT | wxRIGHT, 6);

	m_existingChoice = new wxChoice(targetBox->GetStaticBox(), wxID_ANY);
	m_existingChoice->Enable(false);
	targetBox->Add(m_existingChoice, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

	rootSizer->Add(targetBox, 0, wxEXPAND | wxALL, 6);

	// Group: Inclusion Filters
	wxStaticBoxSizer* includeBox = new wxStaticBoxSizer(wxVERTICAL, this, "Include in Tileset");
	m_includeItemsCheck = new wxCheckBox(includeBox->GetStaticBox(), wxID_ANY, "Include Items (0)");
	m_includeItemsCheck->SetValue(true);
	includeBox->Add(m_includeItemsCheck, 0, wxALL, 6);

	m_includeBrushesCheck = new wxCheckBox(includeBox->GetStaticBox(), wxID_ANY, "Include Brushes (0)");
	m_includeBrushesCheck->SetValue(true);
	includeBox->Add(m_includeBrushesCheck, 0, wxALL, 6);

	rootSizer->Add(includeBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

	// Target File Path preview
	wxStaticBoxSizer* pathBox = new wxStaticBoxSizer(wxVERTICAL, this, "Target File");
	m_filePathLabel = new wxStaticText(pathBox->GetStaticBox(), wxID_ANY, "Target: tilesets/collections/...");
	m_filePathLabel->SetFont(m_filePathLabel->GetFont().Smaller());
	m_filePathLabel->Wrap(240);
	pathBox->Add(m_filePathLabel, 0, wxEXPAND | wxALL, 6);

	rootSizer->Add(pathBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

	// Spacer to push action buttons to bottom
	rootSizer->AddStretchSpacer(1);

	// Action Buttons
	m_accumulateBtn = new wxButton(this, wxID_ANY, "&Add Current Selection");
	m_accumulateBtn->SetToolTip("Add items and brushes from the current map selection");
	rootSizer->Add(m_accumulateBtn, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

	m_saveBtn = new wxButton(this, wxID_ANY, "&Save to Collection");
	m_saveBtn->SetDefault();
	rootSizer->Add(m_saveBtn, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

	m_clearBtn = new wxButton(this, wxID_ANY, "&Clear All");
	rootSizer->Add(m_clearBtn, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

	SetSizer(rootSizer);

	// Event bindings
	m_radioNew->Bind(wxEVT_RADIOBUTTON, &CollectionOptionsPanel::OnModeChanged, this);
	m_radioExisting->Bind(wxEVT_RADIOBUTTON, &CollectionOptionsPanel::OnModeChanged, this);
	m_nameInput->Bind(wxEVT_TEXT, &CollectionOptionsPanel::OnNameChanged, this);
	m_existingChoice->Bind(wxEVT_CHOICE, &CollectionOptionsPanel::OnExistingChanged, this);

	m_accumulateBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
		if (m_callbacks.onAccumulateSelection) {
			m_callbacks.onAccumulateSelection();
		}
	});

	m_saveBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
		PerformSave();
	});

	m_clearBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
		if (m_callbacks.onClearAll) {
			m_callbacks.onClearAll();
		}
	});

	RefreshExistingTilesets();
	UpdateFilePreview();
}

std::string CollectionOptionsPanel::GenerateDefaultTilesetName() const {
	auto existing = rme::collection::CollectionXmlService::GetExistingCollectionTilesetNames();
	for (int i = 1; i <= 999; ++i) {
		std::string candidate = std::format("Collection_{}", i);
		bool found = false;
		for (const auto& ex : existing) {
			if (as_lower_str(ex) == as_lower_str(candidate)) {
				found = true;
				break;
			}
		}
		if (!found) {
			return candidate;
		}
	}
	return "Collection_Custom";
}

void CollectionOptionsPanel::RefreshExistingTilesets() {
	auto names = rme::collection::CollectionXmlService::GetExistingCollectionTilesetNames();
	m_existingChoice->Clear();
	for (const auto& name : names) {
		m_existingChoice->Append(wxstr(name));
	}

	if (!names.empty()) {
		m_existingChoice->SetSelection(0);
	}

	if (names.empty() && m_radioExisting->GetValue()) {
		m_radioNew->SetValue(true);
		m_nameInput->Enable(true);
		m_existingChoice->Enable(false);
	}
	UpdateFilePreview();
}

void CollectionOptionsPanel::EnableAccumulate(bool enable) {
	if (m_accumulateBtn) {
		m_accumulateBtn->Enable(enable);
	}
}

void CollectionOptionsPanel::UpdateCounts(size_t itemCount, size_t brushCount) {
	m_itemCount = itemCount;
	m_brushCount = brushCount;

	m_includeItemsCheck->SetLabel(wxString::Format("Include Items (%zu)", m_itemCount));
	m_includeBrushesCheck->SetLabel(wxString::Format("Include Brushes (%zu)", m_brushCount));
}

void CollectionOptionsPanel::OnModeChanged(wxCommandEvent&) {
	bool isNew = m_radioNew->GetValue();
	m_nameInput->Enable(isNew);
	m_existingChoice->Enable(!isNew);
	UpdateFilePreview();
}

void CollectionOptionsPanel::OnNameChanged(wxCommandEvent&) {
	UpdateFilePreview();
}

void CollectionOptionsPanel::OnExistingChanged(wxCommandEvent&) {
	UpdateFilePreview();
}

void CollectionOptionsPanel::UpdateFilePreview() {
	std::string targetName = GetTargetTilesetName();
	std::string filename = rme::collection::CollectionXmlService::SanitizeTilesetFilename(targetName) + ".xml";
	m_filePathLabel->SetLabel(wxString::Format("Target:\ntilesets/collections/%s", wxstr(filename)));
	m_filePathLabel->Wrap(240);
	Layout();
}

std::string CollectionOptionsPanel::GetTargetTilesetName() const {
	if (m_radioNew->GetValue()) {
		return m_nameInput->GetValue().ToStdString();
	}
	if (m_existingChoice->GetSelection() != wxNOT_FOUND) {
		return m_existingChoice->GetStringSelection().ToStdString();
	}
	return "Collection";
}

void CollectionOptionsPanel::PerformSave() {
	// 1. Empty staging guard
	const bool incItems = IsIncludeItems();
	const bool incBrushes = IsIncludeBrushes();

	if (!incItems && !incBrushes) {
		wxMessageBox(
			"No elements selected to be saved.\nPlease enable 'Include Items' and/or 'Include Brushes'.",
			"Collection Studio",
			wxOK | wxICON_INFORMATION,
			this
		);
		return;
	}

	const size_t effectiveItems = incItems ? m_itemCount : 0;
	const size_t effectiveBrushes = incBrushes ? m_brushCount : 0;

	if (effectiveItems == 0 && effectiveBrushes == 0) {
		wxMessageBox(
			"No items or brushes are selected to be added to the collection.\nPlease keep at least one element.",
			"Collection Studio",
			wxOK | wxICON_INFORMATION,
			this
		);
		return;
	}

	// 2. Validate Target Tileset
	const bool isNew = m_radioNew->GetValue();
	std::string targetName;

	if (isNew) {
		targetName = m_nameInput->GetValue().ToStdString();
		// Trim whitespace
		size_t first = targetName.find_first_not_of(" \t\r\n");
		if (first == std::string::npos) {
			wxMessageBox("Please enter a valid tileset name.", "Collection Studio", wxOK | wxICON_WARNING, this);
			m_nameInput->SetFocus();
			return;
		}
		size_t last = targetName.find_last_not_of(" \t\r\n");
		targetName = targetName.substr(first, last - first + 1);

		// Collision check
		auto existing = rme::collection::CollectionXmlService::GetExistingCollectionTilesetNames();
		bool collision = false;
		for (const auto& ex : existing) {
			if (as_lower_str(ex) == as_lower_str(targetName)) {
				collision = true;
				break;
			}
		}

		if (collision) {
			CollisionDialog dlg(this, targetName);
			int result = dlg.ShowModal();
			if (result == CollisionDialog::Append) {
				if (m_callbacks.onSave) {
					m_callbacks.onSave(targetName, false, true, incItems, incBrushes);
				}
				return;
			} else if (result == CollisionDialog::Overwrite) {
				if (m_callbacks.onSave) {
					m_callbacks.onSave(targetName, true, false, incItems, incBrushes);
				}
				return;
			} else if (result == CollisionDialog::Rename) {
				m_nameInput->SetFocus();
				m_nameInput->SelectAll();
				return;
			} else {
				return; // Cancel
			}
		}

		if (m_callbacks.onSave) {
			m_callbacks.onSave(targetName, true, false, incItems, incBrushes);
		}
	} else {
		if (m_existingChoice->GetCount() == 0 || m_existingChoice->GetSelection() == wxNOT_FOUND) {
			wxMessageBox("Please select an existing collection tileset, or create a new one.", "Collection Studio", wxOK | wxICON_WARNING, this);
			return;
		}
		targetName = m_existingChoice->GetStringSelection().ToStdString();
		if (m_callbacks.onSave) {
			m_callbacks.onSave(targetName, false, true, incItems, incBrushes);
		}
	}
}

} // namespace rme::ui
