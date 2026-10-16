#ifndef RME_PALETTE_ZONES_H_
#define RME_PALETTE_ZONES_H_

#include <wx/treectrl.h>

#include <unordered_set>
#include <vector>

#include "palette/palette_common.h"

struct ZoneCluster;

// Lists OTBM 5 zones (Canary / Crystal Server) in a grouped tree and feeds the selected id to the zone brush
class ZonePalettePanel : public PalettePanel {
public:
	explicit ZonePalettePanel(wxWindow* parent, wxWindowID id = wxID_ANY);
	~ZonePalettePanel() override = default;

	wxString GetName() const override;

	void SelectFirstBrush() override { }
	Brush* GetSelectedBrush() const override;
	int GetSelectedBrushSize() const override {
		return 0;
	}
	bool SelectBrush(const Brush* whatbrush) override;
	void OnUpdate() override;
	void OnSwitchIn() override;

	void SetMap(Map* map);

	void UpdateZone(uint16_t zone_id);
	void UpdateTileCounts();

private:
	[[nodiscard]] bool supportsZones() const;
	[[nodiscard]] uint16_t selectedZoneId() const;

	struct RowRef {
		uint16_t zone = 0;
		int cluster = -1;
	};
	[[nodiscard]] RowRef selectedRow() const;

	void refreshChildAreas(const wxTreeItemId& parent_item, uint16_t zone_id, const std::vector<ZoneCluster>& clusters);

	void OnSelectZone(wxTreeEvent& event);
	void OnActivateZone(wxTreeEvent& event);
	void OnItemExpanding(wxTreeEvent& event);
	void OnZoneContextMenu(wxTreeEvent& event);
	void OnGoToZone(wxCommandEvent& event);
	void OnClickAdd(wxCommandEvent& event);
	void OnClickRename(wxCommandEvent& event);
	void OnClickRemove(wxCommandEvent& event);

	Map* map = nullptr;
	bool refreshing = false;
	wxTreeCtrl* zone_tree = nullptr;
	wxButton* add_button = nullptr;
	wxButton* rename_button = nullptr;
	wxButton* remove_button = nullptr;
};

#endif

