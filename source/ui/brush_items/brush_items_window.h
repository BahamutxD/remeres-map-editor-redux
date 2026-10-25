//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_BRUSH_ITEMS_WINDOW_H_
#define RME_UI_BRUSH_ITEMS_WINDOW_H_

#include "app/main.h"
#include "util/virtual_item_grid.h"
#include <wx/panel.h>
#include <vector>
#include <string>

class Brush;
class TilesetGridToolbar;
class wxStaticText;
class wxButton;

class BrushItemsGrid : public VirtualItemGrid {
public:
	explicit BrushItemsGrid(wxWindow* parent);
	~BrushItemsGrid() override = default;

	void SetItems(std::vector<uint16_t> items);
	void SetFilter(const std::string& query);
	void SetSelectedItem(uint16_t id);
	[[nodiscard]] uint16_t GetSelectedItem() const {
		return m_selectedItemId;
	}
	[[nodiscard]] bool ContainsItem(uint16_t id) const;

	size_t GetItemCount() const override;
	uint16_t GetItem(size_t index) const override;
	wxString GetItemName(size_t index) const override;
	wxString GetItemTooltip(size_t index) const override;
	bool IsItemSelected(int index) const override;

	void SetSort(TilesetSortKey key, TilesetSortDirection dir) override;
	void ClearSort() override;

protected:
	void OnItemSelected(int index) override;

private:
	void ApplyFilterAndSort();

	std::vector<uint16_t> m_rawItems;
	std::vector<uint16_t> m_displayItems;
	std::string m_filterQuery;
	uint16_t m_selectedItemId = 0;
};

class BrushItemsWindow : public wxPanel {
public:
	explicit BrushItemsWindow(wxWindow* parent);
	~BrushItemsWindow() override = default;

	void SetActiveBrush(const Brush* brush);
	void RefreshItems();

private:
	void BuildUI();
	void UpdateHeader();

	const Brush* m_currentSourceBrush = nullptr;
	std::string m_lastSourceBrushName;

	wxPanel* m_headerPanel = nullptr;
	wxStaticText* m_brushNameLabel = nullptr;
	wxStaticText* m_itemCountLabel = nullptr;
	wxButton* m_useBrushBtn = nullptr;
	TilesetGridToolbar* m_toolbar = nullptr;
	BrushItemsGrid* m_grid = nullptr;
	wxStaticText* m_emptyLabel = nullptr;
};

#endif // RME_UI_BRUSH_ITEMS_WINDOW_H_
