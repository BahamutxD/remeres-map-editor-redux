#include <cassert>
#include <iostream>

#include "app/application.h"
#include "app/main.h"
#include "brushes/creature/creature_brush.h"
#include "brushes/door/door_brush.h"
#include "brushes/wall/wall_brush.h"
#include "game/creatures.h"
#include "game/item.h"
#include "item_definitions/core/item_definition_store.h"
#include "map/tile.h"

// These two symbols live in application.cpp, which is intentionally excluded
// from the test link so its main() does not conflict with the test entry point.
Application& wxGetApp() {
	static Application* application = nullptr;
	return *application;
}

void Application::Unload() {
}

namespace {

class TestWallBrush : public WallBrush {
public:
	void setRedirect(WallBrush* brush) {
		redirect_to = brush;
	}
};

void addRotatable(ServerItemId id, ServerItemId rotate_to) {
	ResolvedItemDefinitionRow row;
	row.server_id = id;
	row.client_id = id;
	row.flags = flagMask(ItemFlag::Rotatable);
	row.rotate_to = static_cast<uint16_t>(rotate_to);
	g_item_definitions.append(std::move(row));
}

} // namespace

int main() {
	WallBrush::init();

	{
		CreatureType type;
		CreatureBrush brush(&type);
		assert(brush.getMaxVariation() == 4);
		assert(brush.getDirection() == SOUTH);
		assert(brush.getVariation() == 2);

		brush.setVariation(0);
		assert(brush.getDirection() == NORTH);
		brush.setVariation(1);
		assert(brush.getDirection() == EAST);
		brush.setVariation(3);
		assert(brush.getDirection() == WEST);
		brush.setVariation(99);
		assert(brush.getDirection() == WEST);
	}

	g_item_definitions.clear();

	{
		TestWallBrush wall;
		wall.items.addWallItem(0, 1000, 100);
		wall.items.addWallItem(0, 1001, 100);
		wall.items.addWallItem(0, 1002, 100);
		wall.items.addWallItem(1, 2000, 100);

		for (ServerItemId id : { 1000, 1001, 1002, 2000 }) {
			ResolvedItemDefinitionRow row;
			row.server_id = id;
			row.client_id = id;
			row.flags = flagMask(ItemFlag::IsWall);
			row.border_alignment = static_cast<BorderType>(0);
			g_item_definitions.append(std::move(row));

			ItemEditorData editor_data;
			editor_data.brush = &wall;
			g_item_definitions.setEditorData(id, editor_data);
		}

		assert(wall.findAlignmentForItem(1000) == static_cast<BorderType>(0));
		assert(wall.findNextWallVariant(1000) == 1001);
		assert(wall.findNextWallVariant(1001) == 1002);
		assert(wall.findNextWallVariant(1002) == 1000);
		assert(!wall.findNextWallVariant(9999).has_value());

		Tile tile(0, 0, 0);
		tile.addItem(std::make_unique<Item>(1000, 0));
		bool special = true;
		wall.draw(nullptr, &tile, &special);
		assert(tile.items.front()->getID() == 1001);
		wall.draw(nullptr, &tile, &special);
		assert(tile.items.front()->getID() == 1002);
		wall.draw(nullptr, &tile, &special);
		assert(tile.items.front()->getID() == 1000);

		TestWallBrush windows;
		windows.items.addWallItem(0, 3000, 100);
		windows.items.addWallItem(0, 3001, 100);
		wall.setRedirect(&windows);

		assert(wall.findNextWallVariant(1002) == 3000);
		assert(wall.findNextWallVariant(3001) == 1000);
	}

	{
		TestWallBrush rotated;
		rotated.items.addWallItem(WALL_SOUTH_END, 4000, 100);
		rotated.items.addWallItem(WALL_SOUTH_END, 4001, 100);
		rotated.items.addWallItem(WALL_WEST_END, 4010, 100);
		rotated.items.addWallItem(WALL_WEST_END, 4011, 100);
		rotated.items.addWallItem(WALL_EAST_END, 4020, 100);
		rotated.items.addWallItem(WALL_EAST_END, 4021, 100);
		rotated.items.addWallItem(WALL_NORTHEAST_DIAGONAL, 4030, 100);
		rotated.items.addWallItem(WALL_NORTH_END, 4040, 100);

		assert(rotated.findRotatedWallVariant(4001, true) == 4011);
		assert(rotated.findRotatedWallVariant(4001, false) == 4021);
		assert(!rotated.findRotatedWallVariant(4030, false).has_value()); // no NW target
		assert(rotated.findRotatedWallVariant(4010, true) == 4040);
	}

	{
		TestWallBrush wall;
		wall.items.addDoorItem(WALL_HORIZONTAL, 5000, WALL_DOOR_NORMAL, false);
		wall.items.addDoorItem(WALL_HORIZONTAL, 5001, WALL_DOOR_NORMAL, false);

		for (const auto& [id, open] : { std::pair<ServerItemId, bool> { 5000, false }, { 5001, true } }) {
			ResolvedItemDefinitionRow row;
			row.server_id = id;
			row.client_id = id;
			row.flags = flagMask(ItemFlag::IsWall) | flagMask(ItemFlag::IsBrushDoor);
			row.border_alignment = WALL_HORIZONTAL;
			if (open) {
				row.flags |= flagMask(ItemFlag::IsOpen);
			}
			g_item_definitions.append(std::move(row));

			ItemEditorData editor_data;
			editor_data.brush = &wall;
			g_item_definitions.setEditorData(id, editor_data);
		}

		Tile tile(0, 0, 0);
		tile.addItem(std::make_unique<Item>(5000, 0));

		DoorBrush door(WALL_DOOR_NORMAL);
		bool alt = true;
		door.draw(nullptr, &tile, &alt);
		assert(tile.items.front()->getID() == 5001);
		door.draw(nullptr, &tile, &alt);
		assert(tile.items.front()->getID() == 5000);
	}

	addRotatable(100, 101);
	addRotatable(101, 102);
	addRotatable(102, 103);
	addRotatable(103, 100);

	{
		Item item(100, 0);
		assert(item.isRoteable());
		item.doRotate();
		assert(item.getID() == 101);
		item.doRotateBackwards();
		assert(item.getID() == 100);
		item.doRotateBackwards();
		assert(item.getID() == 103);
	}

	{
		Item item(200, 0);
		item.doRotate();
		assert(item.getID() == 200);
		item.doRotateBackwards();
		assert(item.getID() == 200);
	}

	std::cout << "universal rotator tests passed\n";
	return 0;
}
