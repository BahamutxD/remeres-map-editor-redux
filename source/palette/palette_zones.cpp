//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "palette/palette_zones.h"

#include "brushes/managers/brush_manager.h"
#include "brushes/zone/zone_brush.h"
#include "map/map.h"
#include "ui/gui.h"
#include "editor/editor.h"
#include "editor/action_queue.h"
#include "util/image_manager.h"
#include "rendering/indicators/zone_colors.h"

#include <wx/textdlg.h>
#include <wx/menu.h>
#include <wx/dcmemory.h>

#include <algorithm>
#include <format>
#include <ranges>

namespace {

class ZoneTreeItemData : public wxTreeItemData {
public:
	ZoneTreeItemData(uint16_t zone_id, int cluster_idx)
		: zone(zone_id), cluster(cluster_idx) {}

	uint16_t zone = 0;
	int cluster = -1; // -1 for zone parent node, >= 0 for area child node
};

wxBitmap createZoneColorIcon(uint16_t zone_id, int size = 14) {
	const glm::vec3 rgb = rme::rendering::getZoneColor(zone_id);
	const wxColour fillColor(
		static_cast<unsigned char>(std::clamp(rgb.r * 255.0f, 0.0f, 255.0f)),
		static_cast<unsigned char>(std::clamp(rgb.g * 255.0f, 0.0f, 255.0f)),
		static_cast<unsigned char>(std::clamp(rgb.b * 255.0f, 0.0f, 255.0f))
	);
	const wxColour borderColor(
		static_cast<unsigned char>(std::clamp(rgb.r * 180.0f, 0.0f, 255.0f)),
		static_cast<unsigned char>(std::clamp(rgb.g * 180.0f, 0.0f, 255.0f)),
		static_cast<unsigned char>(std::clamp(rgb.b * 180.0f, 0.0f, 255.0f))
	);

	wxBitmap bmp(size, size);
	wxMemoryDC dc;
	dc.SelectObject(bmp);
	dc.SetBackground(*wxTRANSPARENT_BRUSH);
	dc.Clear();
	dc.SetPen(wxPen(borderColor, 1));
	dc.SetBrush(wxBrush(fillColor));
	dc.DrawRoundedRectangle(1, 1, size - 2, size - 2, 2.0);
	dc.SelectObject(wxNullBitmap);
	return bmp;
}

} // namespace

ZonePalettePanel::ZonePalettePanel(wxWindow* parent, wxWindowID id) :
	PalettePanel(parent, id) {
	auto* sidesizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Zones");
	wxWindow* box = sidesizer->GetStaticBox();

	zone_tree = newd wxTreeCtrl(box, PALETTE_ZONE_LISTBOX, wxDefaultPosition, wxDefaultSize,
		wxTR_HIDE_ROOT | wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_SINGLE);
	image_list = new wxImageList(14, 14, true);
	zone_tree->AssignImageList(image_list);
	sidesizer->Add(zone_tree, 1, wxEXPAND);

	auto* buttons = newd wxBoxSizer(wxHORIZONTAL);
	add_button = newd wxButton(box, PALETTE_ZONE_ADD, "Add");
	add_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PLUS, wxSize(16, 16)));
	rename_button = newd wxButton(box, PALETTE_ZONE_RENAME, "Rename");
	rename_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PEN, wxSize(16, 16)));
	remove_button = newd wxButton(box, PALETTE_ZONE_REMOVE, "Remove");
	remove_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_MINUS, wxSize(16, 16)));
	buttons->Add(add_button, 1, wxEXPAND);
	buttons->Add(rename_button, 1, wxEXPAND);
	buttons->Add(remove_button, 1, wxEXPAND);
	sidesizer->Add(buttons, 0, wxEXPAND);

	SetSizerAndFit(sidesizer);

	Bind(wxEVT_TREE_SEL_CHANGED, &ZonePalettePanel::OnSelectZone, this, PALETTE_ZONE_LISTBOX);
	Bind(wxEVT_TREE_ITEM_ACTIVATED, &ZonePalettePanel::OnActivateZone, this, PALETTE_ZONE_LISTBOX);
	Bind(wxEVT_TREE_ITEM_EXPANDING, &ZonePalettePanel::OnItemExpanding, this, PALETTE_ZONE_LISTBOX);
	Bind(wxEVT_TREE_ITEM_RIGHT_CLICK, &ZonePalettePanel::OnZoneContextMenu, this, PALETTE_ZONE_LISTBOX);
	Bind(wxEVT_BUTTON, &ZonePalettePanel::OnClickAdd, this, PALETTE_ZONE_ADD);
	Bind(wxEVT_BUTTON, &ZonePalettePanel::OnClickRename, this, PALETTE_ZONE_RENAME);
	Bind(wxEVT_BUTTON, &ZonePalettePanel::OnClickRemove, this, PALETTE_ZONE_REMOVE);
	Bind(wxEVT_MENU, &ZonePalettePanel::OnGoToZone, this, PALETTE_ZONE_GOTO);
	Bind(wxEVT_MENU, &ZonePalettePanel::OnClickRename, this, PALETTE_ZONE_RENAME);
	Bind(wxEVT_MENU, &ZonePalettePanel::OnClickRemove, this, PALETTE_ZONE_REMOVE);
}

wxString ZonePalettePanel::GetName() const {
	return "Zone";
}

void ZonePalettePanel::SetMap(Map* m) {
	map = m;
	if (image_list) {
		image_list->RemoveAll();
	}
	zone_icon_map_.clear();
	OnUpdate();
}

bool ZonePalettePanel::supportsZones() const {
	return map && map->getVersion().otbm == MAP_OTBM_5;
}

uint16_t ZonePalettePanel::selectedZoneId() const {
	return selectedRow().zone;
}

ZonePalettePanel::RowRef ZonePalettePanel::selectedRow() const {
	if (!zone_tree) {
		return {};
	}
	const wxTreeItemId item = zone_tree->GetSelection();
	if (!item.IsOk()) {
		return {};
	}
	auto* data = dynamic_cast<ZoneTreeItemData*>(zone_tree->GetItemData(item));
	if (!data) {
		return {};
	}
	return RowRef{ data->zone, data->cluster };
}

int ZonePalettePanel::getOrCreateZoneIcon(uint16_t zone_id) {
	if (!image_list) {
		return -1;
	}
	if (auto it = zone_icon_map_.find(zone_id); it != zone_icon_map_.end()) {
		return it->second;
	}
	const wxBitmap bmp = createZoneColorIcon(zone_id, 14);
	const int img_idx = image_list->Add(bmp);
	zone_icon_map_[zone_id] = img_idx;
	return img_idx;
}

Brush* ZonePalettePanel::GetSelectedBrush() const {
	const uint16_t id = supportsZones() ? selectedZoneId() : 0;
	return id != 0 ? g_brush_manager.zone_brush : nullptr;
}

bool ZonePalettePanel::SelectBrush(const Brush* whatbrush) {
	return whatbrush == g_brush_manager.zone_brush;
}

void ZonePalettePanel::OnSwitchIn() {
	PalettePanel::OnSwitchIn();
	OnUpdate();
	const uint16_t id = supportsZones() ? selectedZoneId() : 0;
	if (g_brush_manager.zone_brush) {
		g_brush_manager.zone_brush->setZone(id);
	}
}

void ZonePalettePanel::OnUpdate() {
	const uint16_t keep = g_brush_manager.zone_brush ? g_brush_manager.zone_brush->getZone() : 0;
	const bool enabled = supportsZones();
	const RowRef prev_selected = selectedRow();

	refreshing = true;
	zone_tree->Freeze();

	// Record expanded state of zones before clearing
	std::unordered_set<uint16_t> expanded_zones;
	wxTreeItemId root = zone_tree->GetRootItem();
	if (root.IsOk()) {
		wxTreeItemIdValue cookie;
		wxTreeItemId child = zone_tree->GetFirstChild(root, cookie);
		while (child.IsOk()) {
			if (zone_tree->IsExpanded(child)) {
				if (auto* data = dynamic_cast<ZoneTreeItemData*>(zone_tree->GetItemData(child))) {
					expanded_zones.insert(data->zone);
				}
			}
			child = zone_tree->GetNextChild(root, cookie);
		}
	}

	if (image_list) {
		image_list->RemoveAll();
	}
	zone_icon_map_.clear();

	zone_tree->DeleteAllItems();
	root = zone_tree->AddRoot("Zones");

	wxTreeItemId item_to_select;

	if (enabled && map) {
		for (const auto& zone : map->zones) {
			if (zone.id == 0) {
				continue;
			}
			const auto& positions = map->getZoneTilePositions(zone.id);
			const size_t total_tiles = positions.size();
			const auto& clusters = map->getZoneClusters(zone.id);

			wxString zone_label;
			if (clusters.size() > 1) {
				zone_label = wxstr(std::format("[{}] {} ({} areas · {} tiles)", zone.id, zone.name, clusters.size(), total_tiles));
			} else {
				zone_label = wxstr(std::format("[{}] {} ({} tiles)", zone.id, zone.name, total_tiles));
			}

			const int icon_idx = getOrCreateZoneIcon(zone.id);
			wxTreeItemId zone_item = zone_tree->AppendItem(root, zone_label, icon_idx, icon_idx, new ZoneTreeItemData(zone.id, -1));

			if (prev_selected.zone == zone.id && prev_selected.cluster == -1) {
				item_to_select = zone_item;
			} else if (!item_to_select.IsOk() && zone.id == keep) {
				item_to_select = zone_item;
			}

			// Add child nodes for distinct areas if zone has more than 1 area
			if (clusters.size() > 1) {
				for (size_t ci = 0; ci < clusters.size(); ++ci) {
					const auto& c = clusters[ci];
					wxString area_label = wxstr(std::format("Area {} @ ({}, {}, z{}) · {} tiles",
						ci + 1, c.centroid_pos.x, c.centroid_pos.y, c.centroid_pos.z, c.tile_count));
					wxTreeItemId area_item = zone_tree->AppendItem(zone_item, area_label, -1, -1,
						new ZoneTreeItemData(zone.id, static_cast<int>(ci)));

					if (prev_selected.zone == zone.id && prev_selected.cluster == static_cast<int>(ci)) {
						item_to_select = area_item;
					}
				}
			}

			// Restore expanded state
			if (expanded_zones.contains(zone.id)) {
				zone_tree->Expand(zone_item);
			}
		}
	}

	if (item_to_select.IsOk()) {
		zone_tree->SelectItem(item_to_select);
	}

	zone_tree->Thaw();
	refreshing = false;

	zone_tree->Enable(enabled);
	add_button->Enable(enabled);
	rename_button->Enable(enabled);
	remove_button->Enable(enabled);
	zone_tree->SetToolTip(enabled || !map ? wxString() : wxString("Zones require an OTBM 5 map (Canary / Crystal Server)."));
}

void ZonePalettePanel::OnSelectZone(wxTreeEvent& event) {
	if (!refreshing && event.GetItem().IsOk()) {
		auto* data = dynamic_cast<ZoneTreeItemData*>(zone_tree->GetItemData(event.GetItem()));
		const uint16_t id = (data && supportsZones()) ? data->zone : 0;
		if (g_brush_manager.zone_brush) {
			g_brush_manager.zone_brush->setZone(id);
		}
		g_gui.SelectBrush();
		if (data && data->cluster >= 0) {
			const auto& clusters = map->getZoneClusters(data->zone);
			if (static_cast<size_t>(data->cluster) < clusters.size()) {
				const auto& region = clusters[static_cast<size_t>(data->cluster)];
				g_gui.SetStatusText(wxstr(std::format("Zone {} Area {}/{}: {} tiles @ ({}, {}, z{})",
					data->zone, data->cluster + 1, clusters.size(), region.tile_count,
					region.centroid_pos.x, region.centroid_pos.y, region.centroid_pos.z)));
			}
		}
	}
}

void ZonePalettePanel::OnActivateZone(wxTreeEvent& /*event*/) {
	wxCommandEvent unused;
	OnGoToZone(unused);
}

void ZonePalettePanel::OnItemExpanding(wxTreeEvent& event) {
	if (!event.GetItem().IsOk() || !supportsZones() || !map) {
		return;
	}
	auto* data = dynamic_cast<ZoneTreeItemData*>(zone_tree->GetItemData(event.GetItem()));
	if (data && data->cluster == -1 && data->zone != 0) {
		const auto& clusters = map->getZoneClusters(data->zone);
		refreshChildAreas(event.GetItem(), data->zone, clusters);
	}
}

void ZonePalettePanel::OnZoneContextMenu(wxTreeEvent& event) {
	if (!supportsZones() || !event.GetItem().IsOk()) {
		return;
	}
	zone_tree->SelectItem(event.GetItem());

	wxMenu menu;
	menu.Append(PALETTE_ZONE_GOTO, "Go to area (double-click)")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_MAP_PIN, wxSize(16, 16)));
	menu.AppendSeparator();
	menu.Append(PALETTE_ZONE_RENAME, "Rename...")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_PEN, wxSize(16, 16)));
	menu.Append(PALETTE_ZONE_REMOVE, "Remove...")->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_MINUS, wxSize(16, 16)));
	PopupMenu(&menu);
}

void ZonePalettePanel::OnGoToZone(wxCommandEvent& /*event*/) {
	const RowRef row = supportsZones() ? selectedRow() : RowRef{};
	if (row.zone == 0) {
		return;
	}
	wxBusyCursor busy;
	const auto& clusters = map->getZoneClusters(row.zone);
	if (!clusters.empty()) {
		if (row.cluster >= 0 && static_cast<size_t>(row.cluster) < clusters.size()) {
			const auto& region = clusters[static_cast<size_t>(row.cluster)];
			g_gui.SetScreenCenterPosition(region.centroid_pos);
			g_gui.SetStatusText(wxstr(std::format("Zone {} Area {}/{}: {} tiles @ ({}, {}, z{})",
				row.zone, row.cluster + 1, clusters.size(), region.tile_count,
				region.centroid_pos.x, region.centroid_pos.y, region.centroid_pos.z)));
		} else {
			const auto it = std::ranges::max_element(clusters, {}, &ZoneCluster::tile_count);
			g_gui.SetScreenCenterPosition(it->centroid_pos);
			const size_t total_tiles = map->getZoneTilePositions(row.zone).size();
			g_gui.SetStatusText(wxstr(std::format("Zone {}: {} tiles across {} area(s) @ ({}, {}, z{})",
				row.zone, total_tiles, clusters.size(), it->centroid_pos.x, it->centroid_pos.y, it->centroid_pos.z)));
		}
	} else {
		g_gui.SetStatusText("This zone has no tiles yet.");
	}
}

void ZonePalettePanel::OnClickAdd(wxCommandEvent& /*event*/) {
	if (!supportsZones()) {
		return;
	}
	const std::string name = nstr(wxGetTextFromUser("Zone name:", "Add Zone", wxEmptyString, this).Trim().Trim(false));
	if (name.empty()) {
		return;
	}
	const uint16_t id = map->addZone(name);
	if (id == 0) {
		g_gui.SetStatusText("A zone with this name already exists.");
		return;
	}
	g_brush_manager.zone_brush->setZone(id);
	OnUpdate();
	g_gui.SelectBrush();
}

void ZonePalettePanel::OnClickRename(wxCommandEvent& /*event*/) {
	const uint16_t id = selectedZoneId();
	const Map::Zone* zone = supportsZones() && id != 0 ? map->findZone(id) : nullptr;
	if (!zone) {
		return;
	}
	const std::string name = nstr(wxGetTextFromUser("Zone name:", "Rename Zone", wxstr(zone->name), this).Trim().Trim(false));
	if (name.empty() || name == zone->name) {
		return;
	}
	if (!map->renameZone(id, name)) {
		g_gui.SetStatusText("A zone with this name already exists.");
		return;
	}
	OnUpdate();
}

void ZonePalettePanel::OnClickRemove(wxCommandEvent& /*event*/) {
	const uint16_t id = selectedZoneId();
	const Map::Zone* zone = supportsZones() && id != 0 ? map->findZone(id) : nullptr;
	if (!zone) {
		return;
	}
	const wxString question = wxstr(std::format("Remove zone \"{}\" (id {}) and clear it from every tile?\nThis cannot be undone.", zone->name, id));
	if (wxMessageBox(question, "Remove Zone", wxYES_NO | wxICON_WARNING, this) != wxYES) {
		return;
	}
	if (Editor* editor = g_gui.GetCurrentEditor()) {
		if (editor->actionQueue) {
			editor->actionQueue->clear();
		}
	}
	map->removeZone(id);
	g_brush_manager.zone_brush->setZone(0);
	OnUpdate();
	g_gui.SelectBrush();
	g_gui.RefreshView();
}

void ZonePalettePanel::UpdateZone(uint16_t zone_id) {
	if (!supportsZones() || !zone_tree || zone_id == 0 || !map) {
		return;
	}
	wxTreeItemId root = zone_tree->GetRootItem();
	if (!root.IsOk()) {
		return;
	}

	const Map::Zone* zone = map->findZone(zone_id);
	if (!zone) {
		return;
	}

	wxTreeItemIdValue cookie;
	wxTreeItemId child = zone_tree->GetFirstChild(root, cookie);
	while (child.IsOk()) {
		auto* data = dynamic_cast<ZoneTreeItemData*>(zone_tree->GetItemData(child));
		if (data && data->zone == zone_id && data->cluster == -1) {
			const auto& positions = map->getZoneTilePositions(zone_id);
			const size_t total_tiles = positions.size();
			const auto& clusters = map->getZoneClusters(zone_id);

			wxString zone_label;
			if (clusters.size() > 1) {
				zone_label = wxstr(std::format("[{}] {} ({} areas · {} tiles)", zone->id, zone->name, clusters.size(), total_tiles));
			} else {
				zone_label = wxstr(std::format("[{}] {} ({} tiles)", zone->id, zone->name, total_tiles));
			}
			zone_tree->SetItemText(child, zone_label);

			// Always refresh child areas if multiple clusters exist or if it previously had children
			if (clusters.size() > 1 || zone_tree->GetChildrenCount(child, false) > 0) {
				refreshChildAreas(child, zone_id, clusters);
			}
			return;
		}
		child = zone_tree->GetNextChild(root, cookie);
	}
}

void ZonePalettePanel::UpdateTileCounts() {
	if (!supportsZones() || !zone_tree || !map) {
		return;
	}
	wxTreeItemId root = zone_tree->GetRootItem();
	if (!root.IsOk()) {
		return;
	}

	wxTreeItemIdValue cookie;
	wxTreeItemId child = zone_tree->GetFirstChild(root, cookie);
	while (child.IsOk()) {
		auto* data = dynamic_cast<ZoneTreeItemData*>(zone_tree->GetItemData(child));
		if (data && data->cluster == -1) {
			const uint16_t zid = data->zone;
			const Map::Zone* zone = map->findZone(zid);
			if (zone) {
				const auto& positions = map->getZoneTilePositions(zid);
				const size_t total_tiles = positions.size();
				const auto& clusters = map->getZoneClusters(zid);

				wxString zone_label;
				if (clusters.size() > 1) {
					zone_label = wxstr(std::format("[{}] {} ({} areas · {} tiles)", zone->id, zone->name, clusters.size(), total_tiles));
				} else {
					zone_label = wxstr(std::format("[{}] {} ({} tiles)", zone->id, zone->name, total_tiles));
				}
				zone_tree->SetItemText(child, zone_label);

				if (clusters.size() > 1 || zone_tree->GetChildrenCount(child, false) > 0) {
					refreshChildAreas(child, zid, clusters);
				}
			}
		}
		child = zone_tree->GetNextChild(root, cookie);
	}
}

void ZonePalettePanel::refreshChildAreas(const wxTreeItemId& parent_item, uint16_t zone_id, const std::vector<ZoneCluster>& clusters) {
	if (clusters.size() <= 1) {
		if (zone_tree->GetChildrenCount(parent_item, false) > 0) {
			zone_tree->DeleteChildren(parent_item);
		}
		return;
	}

	const size_t existing_count = zone_tree->GetChildrenCount(parent_item, false);
	if (existing_count != clusters.size()) {
		zone_tree->DeleteChildren(parent_item);
		for (size_t ci = 0; ci < clusters.size(); ++ci) {
			const auto& c = clusters[ci];
			wxString area_label = wxstr(std::format("Area {} @ ({}, {}, z{}) · {} tiles",
				ci + 1, c.centroid_pos.x, c.centroid_pos.y, c.centroid_pos.z, c.tile_count));
			zone_tree->AppendItem(parent_item, area_label, -1, -1,
				new ZoneTreeItemData(zone_id, static_cast<int>(ci)));
		}
	} else {
		wxTreeItemIdValue cookie;
		wxTreeItemId child = zone_tree->GetFirstChild(parent_item, cookie);
		size_t ci = 0;
		while (child.IsOk() && ci < clusters.size()) {
			const auto& c = clusters[ci];
			wxString area_label = wxstr(std::format("Area {} @ ({}, {}, z{}) · {} tiles",
				ci + 1, c.centroid_pos.x, c.centroid_pos.y, c.centroid_pos.z, c.tile_count));
			zone_tree->SetItemText(child, area_label);
			child = zone_tree->GetNextChild(parent_item, cookie);
			++ci;
		}
	}
}
