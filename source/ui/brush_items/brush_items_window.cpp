//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "ui/brush_items/brush_items_window.h"
#include "brushes/brush.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/managers/brush_manager.h"
#include "game/materials.h"
#include "item_definitions/core/item_definition_store.h"
#include "palette/controls/tileset_grid_toolbar.h"
#include "ui/gui.h"
#include "ui/theme.h"
#include "util/image_manager.h"
#include "util/common.h"

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/button.h>

#include <unordered_set>
#include <algorithm>
#include <format>
#include <ranges>

namespace {
	wxButton* CreateIconButton(wxWindow* parent, std::string_view icon, const wxString& tooltip, const wxSize& size) {
		auto* button = new wxButton(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, size, wxBU_NOTEXT | wxBU_EXACTFIT);
		button->SetBitmap(IMAGE_MANAGER.GetBitmap(icon, parent->FromDIP(wxSize(16, 16)), Theme::Get(Theme::Role::Text)));
		button->SetToolTip(tooltip);
		return button;
	}
} // namespace

// ============================================================================
// BrushItemsGrid
// ============================================================================

BrushItemsGrid::BrushItemsGrid(wxWindow* parent) :
	VirtualItemGrid(parent, wxID_ANY) {
	SetIconSize(32);
	SetShowLabels(true);
}

void BrushItemsGrid::SetItems(std::vector<uint16_t> items) {
	m_rawItems = std::move(items);
	ApplyFilterAndSort();
	if (m_selectedItemId != 0) {
		SetSelectedItem(m_selectedItemId);
	}
}

void BrushItemsGrid::SetFilter(const std::string& query) {
	m_filterQuery = query;
	ApplyFilterAndSort();
}

void BrushItemsGrid::SetSelectedItem(uint16_t id) {
	m_selectedItemId = id;
	auto it = std::ranges::find(m_displayItems, id);
	if (it != m_displayItems.end()) {
		SetSelection(static_cast<int>(std::distance(m_displayItems.begin(), it)));
	} else {
		SetSelection(-1);
	}
	Refresh();
}

bool BrushItemsGrid::ContainsItem(uint16_t id) const {
	return std::ranges::find(m_rawItems, id) != m_rawItems.end();
}

size_t BrushItemsGrid::GetItemCount() const {
	return m_displayItems.size();
}

uint16_t BrushItemsGrid::GetItem(size_t index) const {
	return (index < m_displayItems.size()) ? m_displayItems[index] : 0;
}

wxString BrushItemsGrid::GetItemName(size_t index) const {
	uint16_t id = GetItem(index);
	if (const auto def = g_item_definitions.get(id)) {
		const std::string_view name = def.name();
		if (!name.empty()) {
			return wxstr(name);
		}
	}
	return wxstr(std::format("Item {}", id));
}

wxString BrushItemsGrid::GetItemTooltip(size_t index) const {
	uint16_t id = GetItem(index);
	if (const auto def = g_item_definitions.get(id)) {
		const std::string_view name = def.name();
		return wxstr(std::format("{} (ID: {})", name.empty() ? "Item" : name, id));
	}
	return wxstr(std::format("Item {}", id));
}

bool BrushItemsGrid::IsItemSelected(int index) const {
	if (index >= 0 && index < static_cast<int>(m_displayItems.size())) {
		return m_displayItems[index] == m_selectedItemId;
	}
	return false;
}

void BrushItemsGrid::SetSort(TilesetSortKey key, TilesetSortDirection dir) {
	VirtualItemGrid::SetSort(key, dir);
	ApplyFilterAndSort();
}

void BrushItemsGrid::ClearSort() {
	VirtualItemGrid::ClearSort();
	ApplyFilterAndSort();
}

void BrushItemsGrid::OnItemSelected(int index) {
	if (index >= 0 && index < static_cast<int>(m_displayItems.size())) {
		uint16_t id = m_displayItems[index];
		m_selectedItemId = id;
		SetSelection(index);
		if (RAWBrush* raw = g_materials.ensureRawBrush(id)) {
			g_gui.SelectBrush(raw);
		}
	}
}

void BrushItemsGrid::ApplyFilterAndSort() {
	m_displayItems.clear();
	if (m_filterQuery.empty()) {
		m_displayItems = m_rawItems;
	} else {
		wxString q = wxstr(m_filterQuery).Lower();
		for (uint16_t id : m_rawItems) {
			if (std::to_string(id).find(m_filterQuery) != std::string::npos) {
				m_displayItems.push_back(id);
				continue;
			}
			if (const auto def = g_item_definitions.get(id)) {
				if (wxstr(def.name()).Lower().Find(q) != wxNOT_FOUND) {
					m_displayItems.push_back(id);
				}
			}
		}
	}

	if (HasSort()) {
		ApplySortTo(m_displayItems);
	}

	RefreshGrid();
	Refresh();
}

// ============================================================================
// BrushItemsWindow
// ============================================================================

BrushItemsWindow::BrushItemsWindow(wxWindow* parent) :
	wxPanel(parent, wxID_ANY) {
	BuildUI();
	UpdateHeader();
}

void BrushItemsWindow::BuildUI() {
	SetBackgroundColour(Theme::Get(Theme::Role::Surface));

	wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

	// Header panel
	m_headerPanel = new wxPanel(this, wxID_ANY);
	m_headerPanel->SetBackgroundColour(Theme::Get(Theme::Role::Header));
	wxBoxSizer* headerSizer = new wxBoxSizer(wxHORIZONTAL);

	m_brushNameLabel = new wxStaticText(m_headerPanel, wxID_ANY, "No Brush Selected");
	wxFont titleFont = m_brushNameLabel->GetFont();
	titleFont.SetWeight(wxFONTWEIGHT_BOLD);
	m_brushNameLabel->SetFont(titleFont);
	m_brushNameLabel->SetForegroundColour(Theme::Get(Theme::Role::Text));
	headerSizer->Add(m_brushNameLabel, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);

	m_itemCountLabel = new wxStaticText(m_headerPanel, wxID_ANY, "");
	m_itemCountLabel->SetForegroundColour(Theme::Get(Theme::Role::TextSubtle));
	headerSizer->Add(m_itemCountLabel, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 6);

	const wxSize btnSize = FromDIP(wxSize(28, 24));
	m_useBrushBtn = CreateIconButton(m_headerPanel, ICON_PAINTBRUSH, "Switch drawing tool back to this composite brush", btnSize);
	m_useBrushBtn->Enable(false);
	m_useBrushBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
		if (m_currentSourceBrush) {
			g_gui.SelectBrush(m_currentSourceBrush);
			if (m_grid) {
				m_grid->SetSelectedItem(0);
			}
		}
	});
	headerSizer->Add(m_useBrushBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);

	m_headerPanel->SetSizer(headerSizer);
	mainSizer->Add(m_headerPanel, 0, wxEXPAND);

	// Grid
	m_grid = new BrushItemsGrid(this);

	// Toolbar
	TilesetGridToolbar::Actions actions;
	actions.filter = [this](const std::string& text) {
		if (m_grid) {
			m_grid->SetFilter(text);
		}
	};
	actions.sort = [this](TilesetSortKey key, TilesetSortDirection dir) {
		if (m_grid) {
			m_grid->SetSort(key, dir);
		}
	};
	actions.clear_sort = [this]() {
		if (m_grid) {
			m_grid->ClearSort();
		}
	};
	actions.show_labels = [this](bool show) {
		if (m_grid) {
			m_grid->SetShowLabels(show);
		}
	};
	actions.icon_size = [this](int size) {
		if (m_grid) {
			m_grid->SetIconSize(size);
		}
	};
	actions.brush_indicators = [this](bool dogEar) {
		if (m_grid) {
			m_grid->SetBrushIndicators(dogEar);
		}
		if (m_toolbar) {
			m_toolbar->SetIndicatorState(dogEar);
		}
	};
	actions.grid_spacing = [this](int spacingPx) {
		if (m_grid) {
			m_grid->SetGridSpacing(spacingPx);
		}
		if (m_toolbar) {
			m_toolbar->SetGridSpacing(spacingPx);
		}
	};

	m_toolbar = new TilesetGridToolbar(this, std::move(actions), TilesetGridToolbar::Part_Standard);
	m_toolbar->SetState(m_grid->GetShowLabels(), m_grid->GetIconSize(), m_grid->HasSort(), m_grid->GetSortKey(), m_grid->GetSortDirection(), false);
	mainSizer->Add(m_toolbar, 0, wxEXPAND);

	// Empty state banner
	m_emptyLabel = new wxStaticText(this, wxID_ANY, "Select a brush to view its items.", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER_HORIZONTAL);
	m_emptyLabel->SetForegroundColour(Theme::Get(Theme::Role::TextSubtle));
	mainSizer->Add(m_emptyLabel, 0, wxEXPAND | wxTOP | wxBOTTOM, 24);

	mainSizer->Add(m_grid, 1, wxEXPAND);

	SetSizer(mainSizer);
}

void BrushItemsWindow::SetActiveBrush(const Brush* brush) {
	if (!brush) {
		m_currentSourceBrush = nullptr;
		m_lastSourceBrushName.clear();
		if (m_grid) {
			m_grid->SetItems({});
		}
		UpdateHeader();
		return;
	}

	// If a RAWBrush was chosen, and it belongs to our currently active source brush,
	// keep the grid intact and only highlight the active item!
	if (const RAWBrush* raw = brush->as<RAWBrush>()) {
		const uint16_t raw_id = raw->getItemID();
		if (m_currentSourceBrush != nullptr && m_grid && m_grid->ContainsItem(raw_id)) {
			m_grid->SetSelectedItem(raw_id);
			return;
		}
	}

	// If the brush is identical to current source brush, do not rebuild
	if (brush == m_currentSourceBrush && brush->getName() == m_lastSourceBrushName) {
		return;
	}

	m_currentSourceBrush = brush;
	m_lastSourceBrushName = brush->getName();

	std::vector<uint16_t> items;
	const_cast<Brush*>(brush)->getRelatedItems(items);

	// Deduplicate preserving insertion order
	std::vector<uint16_t> unique_items;
	std::unordered_set<uint16_t> seen;
	unique_items.reserve(items.size());
	for (uint16_t id : items) {
		if (id != 0 && seen.insert(id).second) {
			unique_items.push_back(id);
		}
	}

	if (m_grid) {
		m_grid->SetItems(std::move(unique_items));
		if (const RAWBrush* raw = brush->as<RAWBrush>()) {
			m_grid->SetSelectedItem(raw->getItemID());
		} else {
			m_grid->SetSelectedItem(0);
		}
	}

	UpdateHeader();
}

void BrushItemsWindow::RefreshItems() {
	SetActiveBrush(m_currentSourceBrush);
}

void BrushItemsWindow::UpdateHeader() {
	if (!m_brushNameLabel || !m_itemCountLabel || !m_useBrushBtn) {
		return;
	}

	if (!m_currentSourceBrush) {
		m_brushNameLabel->SetLabel("No Brush Selected");
		m_itemCountLabel->SetLabel("");
		m_useBrushBtn->Enable(false);
		if (m_emptyLabel) {
			m_emptyLabel->SetLabel("Select a brush to view its items.");
			m_emptyLabel->Show(true);
		}
		if (m_grid) m_grid->Show(false);
	} else {
		m_brushNameLabel->SetLabel(wxstr(m_currentSourceBrush->getName()));
		const size_t count = m_grid ? m_grid->GetItemCount() : 0;
		m_itemCountLabel->SetLabel(wxstr(std::format("({} {})", count, count == 1 ? "item" : "items")));
		const bool isComposite = !m_currentSourceBrush->is<RAWBrush>();
		m_useBrushBtn->Enable(isComposite);
		if (m_emptyLabel) {
			m_emptyLabel->SetLabel("No items associated with this brush.");
			m_emptyLabel->Show(count == 0);
		}
		if (m_grid) m_grid->Show(count > 0);
	}

	Layout();
}
