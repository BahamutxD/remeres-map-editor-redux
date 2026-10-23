#include <cassert>
#include <iostream>

#include "app/application.h"
#include "app/main.h"
#include "brushes/creature/creature_brush.h"
#include "brushes/wall/wall_brush.h"
#include "game/creatures.h"
#include "game/item.h"
#include "item_definitions/core/item_definition_store.h"

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

	{
		TestWallBrush wall;
		wall.items.addWallItem(0, 1000, 100);
		wall.items.addWallItem(0, 1001, 100);
		wall.items.addWallItem(0, 1002, 100);
		wall.items.addWallItem(1, 2000, 100);

		assert(wall.findAlignmentForItem(1000) == WALL_NORTH);
		assert(wall.findNextWallVariant(1000) == 1001);
		assert(wall.findNextWallVariant(1001) == 1002);
		assert(wall.findNextWallVariant(1002) == 1000);
		assert(!wall.findNextWallVariant(9999).has_value());

		TestWallBrush windows;
		windows.items.addWallItem(0, 3000, 100);
		windows.items.addWallItem(0, 3001, 100);
		wall.setRedirect(&windows);

		assert(wall.findNextWallVariant(1002) == 3000);
		assert(wall.findNextWallVariant(3001) == 1000);
	}

	g_item_definitions.clear();
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
