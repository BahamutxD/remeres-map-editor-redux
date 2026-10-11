//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "brushes/zone/zone_brush.h"

#include "map/basemap.h"
#include "map/map.h"
#include "map/tile.h"

bool ZoneBrush::canDraw(BaseMap* map, const Position& position) const {
	if (zone_id == 0) {
		return false;
	}
	// Zones are an OTBM 5 (Canary / Crystal Server) feature; painting them onto
	// older maps would create data that cannot be saved (node 19 is OTBM 5 only).
	const Map* full_map = dynamic_cast<const Map*>(map);
	if (!full_map || full_map->getVersion().otbm != MAP_OTBM_5) {
		return false;
	}
	const Tile* tile = map->getTile(position);
	return tile && tile->hasGround();
}

void ZoneBrush::draw(BaseMap* /*map*/, Tile* tile, void* /*parameter*/) {
	if (tile->hasGround()) {
		tile->addZone(zone_id);
	}
}

void ZoneBrush::undraw(BaseMap* /*map*/, Tile* tile) {
	tile->removeZone(zone_id);
}
