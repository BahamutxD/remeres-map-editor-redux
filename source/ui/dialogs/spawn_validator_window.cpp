#include "ui/dialogs/spawn_validator_window.h"
#include "editor/editor.h"
#include "ui/gui.h"
#include "ui/theme.h"
#include "ui/map_window.h"
#include "util/image_manager.h"

#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/spinctrl.h>
#include <wx/textctrl.h>
#include <wx/msgdlg.h>
#include <wx/menu.h>
#include <format>
#include <algorithm>

namespace {
	constexpr int ID_SCAN_MAP = 10001;
	constexpr int ID_JUMP_POS = 10002;
	constexpr int ID_EDIT_ISSUE = 10003;
	constexpr int ID_REMOVE_ISSUE = 10004;
	constexpr int ID_REMOVE_ALL = 10005;
	constexpr int ID_SEARCH_FILTER = 10006;
	constexpr int ID_KIND_FILTER = 10007;
	constexpr int ID_RULE_CHECKBOX = 10008;
}

SpawnValidatorWindow::SpawnValidatorWindow(wxWindow* parent, Editor* editor) :
	wxDialog(parent, wxID_ANY, "Spawn Validator", wxDefaultPosition, wxDefaultSize,
		wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX),
	editor(editor) {
	SetSize(FromDIP(wxSize(980, 620)));
	SetMinSize(FromDIP(wxSize(750, 480)));
	CenterOnParent();
	SetBackgroundColour(Theme::Get(Theme::Role::Surface));

	InitLayout();

	Bind(wxEVT_CLOSE_WINDOW, &SpawnValidatorWindow::OnClose, this);
	Bind(wxEVT_CHAR_HOOK, &SpawnValidatorWindow::OnCharHook, this);

	// Automatically perform initial scan when window opens
	wxCommandEvent dummy;
	OnScanMap(dummy);
}

SpawnValidatorWindow::~SpawnValidatorWindow() {
	DetachEditor();
	Unbind(wxEVT_CLOSE_WINDOW, &SpawnValidatorWindow::OnClose, this);
	Unbind(wxEVT_CHAR_HOOK, &SpawnValidatorWindow::OnCharHook, this);
}

void SpawnValidatorWindow::DetachEditor() {
	editor = nullptr;
}

void SpawnValidatorWindow::OnClose(wxCloseEvent& event) {
	if (auto* owner = wxDynamicCast(GetParent(), MapWindow); owner != nullptr) {
		owner->ForgetSpawnValidatorDialog();
	}
	Destroy();
}

void SpawnValidatorWindow::OnCharHook(wxKeyEvent& event) {
	const int key = event.GetKeyCode();
	if (key == WXK_ESCAPE) {
		Close();
		return;
	}
	if (key == WXK_DELETE || key == WXK_NUMPAD_DELETE) {
		if (selected_issue_id >= 0 && remove_button && remove_button->IsEnabled()) {
			OnRemoveSelected();
			return;
		}
	}
	if (key == WXK_RETURN || key == WXK_NUMPAD_ENTER) {
		if (selected_issue_id >= 0 && jump_button && jump_button->IsEnabled()) {
			OnJumpToSelected();
			return;
		}
	}
	event.Skip();
}

void SpawnValidatorWindow::InitLayout() {
	auto* rootSizer = new wxBoxSizer(wxHORIZONTAL);
	const int pad = Theme::Grid(1);

	// =========================================================================
	// LEFT COLUMN: Options, Filters & Actions
	// =========================================================================
	auto* leftPanel = new wxPanel(this, wxID_ANY);
	leftPanel->SetBackgroundColour(Theme::Get(Theme::Role::Surface));
	auto* leftSizer = new wxBoxSizer(wxVERTICAL);

	// Rules header
	auto* rulesHeader = new wxStaticText(leftPanel, wxID_ANY, "VALIDATION RULES");
	rulesHeader->SetFont(Theme::GetFont(10, true));
	rulesHeader->SetForegroundColour(Theme::Get(Theme::Role::Text));
	leftSizer->Add(rulesHeader, 0, wxBOTTOM, pad);

	chk_invalid_time = new wxCheckBox(leftPanel, ID_RULE_CHECKBOX, "Invalid spawn time (< 1s, > 24h)");
	chk_unknown_creature = new wxCheckBox(leftPanel, ID_RULE_CHECKBOX, "Unknown creature / kind mismatch");
	chk_blocked_tile = new wxCheckBox(leftPanel, ID_RULE_CHECKBOX, "Creature on wall / blocked tile");
	chk_empty_tile = new wxCheckBox(leftPanel, ID_RULE_CHECKBOX, "Missing / empty tile (server drop)");
	chk_unwalkable_zone = new wxCheckBox(leftPanel, ID_RULE_CHECKBOX, "Spawn zone with 0 walkable tiles");

	chk_invalid_time->SetValue(true);
	chk_unknown_creature->SetValue(true);
	chk_blocked_tile->SetValue(true);
	chk_empty_tile->SetValue(true);
	chk_unwalkable_zone->SetValue(true);

	const auto applyFont = [](wxCheckBox* cb) {
		cb->SetFont(Theme::GetFont(9, false));
	};
	applyFont(chk_invalid_time);
	applyFont(chk_unknown_creature);
	applyFont(chk_blocked_tile);
	applyFont(chk_empty_tile);
	applyFont(chk_unwalkable_zone);

	const auto onCheck = [this](wxCommandEvent& ev) { OnScanMap(ev); };
	chk_invalid_time->Bind(wxEVT_CHECKBOX, onCheck);
	chk_unknown_creature->Bind(wxEVT_CHECKBOX, onCheck);
	chk_blocked_tile->Bind(wxEVT_CHECKBOX, onCheck);
	chk_empty_tile->Bind(wxEVT_CHECKBOX, onCheck);
	chk_unwalkable_zone->Bind(wxEVT_CHECKBOX, onCheck);

	leftSizer->Add(chk_invalid_time, 0, wxBOTTOM, 4);
	leftSizer->Add(chk_unknown_creature, 0, wxBOTTOM, 4);
	leftSizer->Add(chk_blocked_tile, 0, wxBOTTOM, 4);
	leftSizer->Add(chk_empty_tile, 0, wxBOTTOM, 4);
	leftSizer->Add(chk_unwalkable_zone, 0, wxBOTTOM, pad);

	leftSizer->Add(new wxStaticLine(leftPanel), 0, wxEXPAND | wxBOTTOM, pad);

	// Filter & Search header
	auto* filterHeader = new wxStaticText(leftPanel, wxID_ANY, "FILTER & SEARCH");
	filterHeader->SetFont(Theme::GetFont(10, true));
	filterHeader->SetForegroundColour(Theme::Get(Theme::Role::Text));
	leftSizer->Add(filterHeader, 0, wxBOTTOM, pad / 2);

	wxArrayString kinds;
	kinds.Add("All");
	kinds.Add("Monsters");
	kinds.Add("NPCs");
	radio_kind_filter = new wxRadioBox(leftPanel, ID_KIND_FILTER, "Kind Filter",
		wxDefaultPosition, wxDefaultSize, kinds, 3, wxRA_SPECIFY_COLS);
	radio_kind_filter->SetFont(Theme::GetFont(9, false));
	radio_kind_filter->Bind(wxEVT_RADIOBOX, &SpawnValidatorWindow::OnKindFilterChanged, this);
	leftSizer->Add(radio_kind_filter, 0, wxEXPAND | wxBOTTOM, pad / 2);

	search_ctrl = new PaletteSearchCtrl(leftPanel, ID_SEARCH_FILTER, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	search_ctrl->SetDescriptiveText("Filter by name or coords...");
	search_ctrl->SetFont(Theme::GetFont(9, false));
	search_ctrl->Bind(wxEVT_TEXT, &SpawnValidatorWindow::OnSearchFilterChanged, this);
	leftSizer->Add(search_ctrl, 0, wxEXPAND | wxBOTTOM, pad);

	leftSizer->Add(new wxStaticLine(leftPanel), 0, wxEXPAND | wxBOTTOM, pad);

	// Action buttons
	scan_button = new wxButton(leftPanel, ID_SCAN_MAP, "Scan Map / Refresh");
	scan_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_LOCATION_CROSSHAIRS, wxSize(16, 16)));
	scan_button->SetFont(Theme::GetFont(9, true));
	scan_button->Bind(wxEVT_BUTTON, &SpawnValidatorWindow::OnScanMap, this);
	leftSizer->Add(scan_button, 0, wxEXPAND | wxBOTTOM, pad / 2);

	jump_button = new wxButton(leftPanel, ID_JUMP_POS, "Jump to Position");
	jump_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_LOCATION_ARROW, wxSize(16, 16)));
	jump_button->SetFont(Theme::GetFont(9, false));
	jump_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnJumpToSelected(); });
	leftSizer->Add(jump_button, 0, wxEXPAND | wxBOTTOM, pad / 2);

	edit_button = new wxButton(leftPanel, ID_EDIT_ISSUE, "Edit / Correct...");
	edit_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PEN, wxSize(16, 16)));
	edit_button->SetFont(Theme::GetFont(9, false));
	edit_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnEditSelected(); });
	leftSizer->Add(edit_button, 0, wxEXPAND | wxBOTTOM, pad / 2);

	remove_button = new wxButton(leftPanel, ID_REMOVE_ISSUE, "Remove Selected");
	remove_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_TRASH, wxSize(16, 16)));
	remove_button->SetFont(Theme::GetFont(9, false));
	remove_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnRemoveSelected(); });
	leftSizer->Add(remove_button, 0, wxEXPAND | wxBOTTOM, pad / 2);

	remove_all_button = new wxButton(leftPanel, ID_REMOVE_ALL, "Remove All Listed");
	remove_all_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_TRASH, wxSize(16, 16)));
	remove_all_button->SetFont(Theme::GetFont(9, false));
	remove_all_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnRemoveAll(); });
	leftSizer->Add(remove_all_button, 0, wxEXPAND | wxBOTTOM, pad);

	leftSizer->AddStretchSpacer(1);

	status_label = new wxStaticText(leftPanel, wxID_ANY, "Ready. Click 'Scan Map' to validate.");
	status_label->SetFont(Theme::GetFont(8, false));
	status_label->SetForegroundColour(Theme::Get(Theme::Role::TextSubtle));
	leftSizer->Add(status_label, 0, wxEXPAND | wxBOTTOM, pad);

	leftPanel->SetSizer(leftSizer);
	rootSizer->Add(leftPanel, 0, wxEXPAND | wxALL, pad);

	// Separator line between panels
	rootSizer->Add(new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL), 0, wxEXPAND | wxTOP | wxBOTTOM, pad);

	// =========================================================================
	// RIGHT COLUMN: NanoVGTableCtrl Results List
	// =========================================================================
	issues_table = new NanoVGTableCtrl(this, wxID_ANY);
	issues_table->SetRowHeight(34);

	std::vector<TableColumn> cols = {
		{ "id", "#", 45, NVG_ALIGN_RIGHT, true },
		{ "name", "Name", 130, NVG_ALIGN_LEFT, true },
		{ "kind", "Kind", 75, NVG_ALIGN_CENTER, true },
		{ "pos", "Position", 115, NVG_ALIGN_CENTER, true },
		{ "issue", "Issue", 135, NVG_ALIGN_LEFT, true },
		{ "details", "Details", 0, NVG_ALIGN_LEFT, true }
	};
	issues_table->SetColumns(std::move(cols));

	issues_table->SetOnSelect([this](int idx, const TableRow& row) {
		selected_issue_id = static_cast<int>(row.id);
		UpdateButtonStates();
	});

	issues_table->SetOnActivate([this](int idx, const TableRow& row) {
		selected_issue_id = static_cast<int>(row.id);
		UpdateButtonStates();
		OnJumpToSelected();
	});

	issues_table->SetOnContextMenu([this](int idx, const TableRow* row, const wxPoint& pos) {
		if (!row) {
			return;
		}
		selected_issue_id = static_cast<int>(row->id);
		UpdateButtonStates();

		wxMenu menu;
		menu.Append(ID_JUMP_POS, "Jump to Position\tEnter");
		menu.Append(ID_EDIT_ISSUE, "Edit / Correct...");
		menu.Append(ID_REMOVE_ISSUE, "Remove Selected\tDel");

		menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { OnJumpToSelected(); }, ID_JUMP_POS);
		menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { OnEditSelected(); }, ID_EDIT_ISSUE);
		menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { OnRemoveSelected(); }, ID_REMOVE_ISSUE);

		PopupMenu(&menu, pos);
	});

	issues_table->SetOnSort([this](int col, bool asc) {
		std::sort(current_issues.begin(), current_issues.end(), [col, asc](const auto& a, const auto& b) {
			bool result = false;
			switch (col) {
				case 0: result = a.id < b.id; break;
				case 1: result = a.name < b.name; break;
				case 2: result = a.is_npc < b.is_npc; break;
				case 3: result = a.pos < b.pos; break;
				case 4: result = a.GetTypeString() < b.GetTypeString(); break;
				case 5: result = a.description < b.description; break;
				default: result = a.id < b.id; break;
			}
			return asc ? result : !result;
		});
		RefreshTable();
	});

	rootSizer->Add(issues_table, 1, wxEXPAND | wxALL, pad);

	SetSizer(rootSizer);
	Layout();

	UpdateButtonStates();
}

void SpawnValidatorWindow::OnScanMap(wxCommandEvent&) {
	if (!editor) {
		return;
	}

	EditorOperations::SpawnValidationSettings settings;
	settings.check_invalid_spawntime = chk_invalid_time->GetValue();
	settings.check_unknown_creature = chk_unknown_creature->GetValue();
	settings.check_blocked_tiles = chk_blocked_tile->GetValue();
	settings.check_missing_or_empty_tiles = chk_empty_tile->GetValue();
	settings.check_unwalkable_zone = chk_unwalkable_zone->GetValue();

	current_issues = EditorOperations::SpawnValidator::ScanMap(editor->map, settings);
	selected_issue_id = -1;
	RefreshTable();
}

void SpawnValidatorWindow::OnSearchFilterChanged(wxCommandEvent&) {
	RefreshTable();
}

void SpawnValidatorWindow::OnKindFilterChanged(wxCommandEvent&) {
	RefreshTable();
}

std::vector<const EditorOperations::SpawnIssue*> SpawnValidatorWindow::GetFilteredIssues() const {
	std::vector<const EditorOperations::SpawnIssue*> filtered;
	const int kindFilter = radio_kind_filter ? radio_kind_filter->GetSelection() : 0;
	const std::string search = search_ctrl ? as_lower_str(search_ctrl->GetValue().ToStdString()) : "";

	for (const auto& issue : current_issues) {
		if (kindFilter == 1 && issue.is_npc) {
			continue;
		}
		if (kindFilter == 2 && !issue.is_npc) {
			continue;
		}

		if (!search.empty()) {
			const std::string nameLower = as_lower_str(issue.name);
			const std::string posStr = std::format("{},{},{}", issue.pos.x, issue.pos.y, issue.pos.z);
			const std::string typeLower = as_lower_str(std::string(issue.GetTypeString()));
			if (nameLower.find(search) == std::string::npos &&
			    posStr.find(search) == std::string::npos &&
			    typeLower.find(search) == std::string::npos) {
				continue;
			}
		}

		filtered.push_back(&issue);
	}
	return filtered;
}

void SpawnValidatorWindow::RefreshTable() {
	const auto filtered = GetFilteredIssues();

	std::vector<TableRow> rows;
	rows.reserve(filtered.size());

	for (const auto* issue : filtered) {
		TableRow row;
		row.id = issue->id;
		row.key = std::to_string(issue->id);
		row.cells = {
			std::to_string(issue->id),
			issue->name,
			std::string(issue->GetKindString()),
			std::format("{}, {}, {}", issue->pos.x, issue->pos.y, issue->pos.z),
			std::string(issue->GetTypeString()),
			issue->description
		};
		rows.push_back(std::move(row));
	}

	issues_table->SetRows(std::move(rows));

	if (status_label) {
		status_label->SetLabel(wxString::Format("Found %zu issues (%zu shown).", current_issues.size(), filtered.size()));
	}

	UpdateButtonStates();
}

void SpawnValidatorWindow::UpdateButtonStates() {
	const bool hasSelection = (selected_issue_id >= 0);
	const auto filtered = GetFilteredIssues();

	if (jump_button) {
		jump_button->Enable(hasSelection);
	}
	if (edit_button) {
		edit_button->Enable(hasSelection);
	}
	if (remove_button) {
		remove_button->Enable(hasSelection);
	}
	if (remove_all_button) {
		remove_all_button->Enable(!filtered.empty());
	}
}

void SpawnValidatorWindow::OnJumpToSelected() {
	if (selected_issue_id < 0) {
		return;
	}

	auto it = std::find_if(current_issues.begin(), current_issues.end(), [this](const auto& iss) {
		return iss.id == static_cast<uint32_t>(selected_issue_id);
	});
	if (it == current_issues.end()) {
		return;
	}

	g_gui.SetScreenCenterPosition(it->pos);
}

void SpawnValidatorWindow::OnEditSelected() {
	if (!editor || selected_issue_id < 0) {
		return;
	}

	auto it = std::find_if(current_issues.begin(), current_issues.end(), [this](const auto& iss) {
		return iss.id == static_cast<uint32_t>(selected_issue_id);
	});
	if (it == current_issues.end() || it->type == EditorOperations::SpawnIssueType::UnwalkableZone) {
		return;
	}

	wxDialog editDlg(this, wxID_ANY, "Correct Spawn Creature", wxDefaultPosition, wxDefaultSize);
	auto* dlgSizer = new wxBoxSizer(wxVERTICAL);
	const int pad = Theme::Grid(1);

	auto* nameLbl = new wxStaticText(&editDlg, wxID_ANY, "Creature Name:");
	nameLbl->SetFont(Theme::GetFont(9, true));
	dlgSizer->Add(nameLbl, 0, wxLEFT | wxRIGHT | wxTOP, pad);

	auto* nameCtrl = new wxTextCtrl(&editDlg, wxID_ANY, it->name);
	nameCtrl->SetMinSize(FromDIP(wxSize(280, -1)));
	dlgSizer->Add(nameCtrl, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, pad);

	auto* timeLbl = new wxStaticText(&editDlg, wxID_ANY, "Spawn Time (seconds):");
	timeLbl->SetFont(Theme::GetFont(9, true));
	dlgSizer->Add(timeLbl, 0, wxLEFT | wxRIGHT | wxTOP, pad);

	auto* timeCtrl = new wxSpinCtrl(&editDlg, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
		wxSP_ARROW_KEYS, 1, 86400, it->spawntime > 0 ? it->spawntime : 60);
	dlgSizer->Add(timeCtrl, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, pad);

	dlgSizer->Add(editDlg.CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, pad);
	editDlg.SetSizerAndFit(dlgSizer);
	editDlg.CenterOnParent();

	if (editDlg.ShowModal() != wxID_OK) {
		return;
	}

	const std::string newName = nameCtrl->GetValue().ToStdString();
	const int newTime = timeCtrl->GetValue();

	if (EditorOperations::SpawnValidator::CorrectIssue(*editor, *it, newName, newTime)) {
		wxCommandEvent ev;
		OnScanMap(ev);
		g_gui.RefreshView();
	}
}

void SpawnValidatorWindow::OnRemoveSelected() {
	if (!editor || selected_issue_id < 0) {
		return;
	}

	auto it = std::find_if(current_issues.begin(), current_issues.end(), [this](const auto& iss) {
		return iss.id == static_cast<uint32_t>(selected_issue_id);
	});
	if (it == current_issues.end()) {
		return;
	}

	if (EditorOperations::SpawnValidator::RemoveIssue(*editor, *it)) {
		wxCommandEvent ev;
		OnScanMap(ev);
		g_gui.RefreshView();
	}
}

void SpawnValidatorWindow::OnRemoveAll() {
	if (!editor) {
		return;
	}

	const auto filtered = GetFilteredIssues();
	if (filtered.empty()) {
		return;
	}

	const int answer = wxMessageBox(
		wxString::Format("Are you sure you want to remove all %zu listed broken spawn entries?\n\nThis action can be undone with Ctrl+Z.", filtered.size()),
		"Confirm Remove All",
		wxYES_NO | wxICON_WARNING,
		this
	);
	if (answer != wxYES) {
		return;
	}

	std::vector<EditorOperations::SpawnIssue> toRemove;
	toRemove.reserve(filtered.size());
	for (const auto* iss : filtered) {
		toRemove.push_back(*iss);
	}

	const size_t removedCount = EditorOperations::SpawnValidator::RemoveAllIssues(*editor, toRemove);
	if (removedCount > 0) {
		wxCommandEvent ev;
		OnScanMap(ev);
		g_gui.RefreshView();
		g_gui.SetStatusText(wxString::Format("Removed %zu broken spawn entries.", removedCount));
	}
}
