# Selection and Pixel-Perfect Selection: ImGui Editor vs RME Redux

Status: research/documentation only. No production code was changed for this document.

Repositories inspected:

- **ImGui editor**: `E:\Github\tibia-imgui-map-editor\ImguiMapEditor`, commit `83ac353a01f950cd0c6bfad8b172fb3961b07d19` (2026-07-10).
- **RME Redux**: `E:\Github\remeres-map-editor-redux`, branch `master`, commit `68834db7ac777eade9a603ae87ae94b53ca81ff2` (2026-10-22).

The goal is to understand how the ImGui editor's **Pixel Perfect Selection** works, how Redux selects things today, and what has to change in Redux to support the same user-visible behavior. The implementation plan is in [pixel-perfect-selection-plan.md](pixel-perfect-selection-plan.md).

---

## 1. Executive summary

### ImGui editor

Pixel Perfect Selection is a **selection strategy**, not a separate selection model. The editor already stores selections at entity granularity (`SelectionEntry` with an `EntityId`). The only thing that changes when the user enables pixel-perfect mode is **which entity is picked under the mouse**:

- Smart mode picks logically: spawn, then creature, then the top item, then ground.
- Pixel-perfect mode walks the visible sprites from top to bottom and does an **alpha test at the exact sub-tile pixel** under the cursor.
- The cursor's sub-tile pixel offset is computed in the UI layer and passed down as `glm::vec2 pixel_offset`.
- The mode is a persisted boolean (`selection.use_pixel_perfect`) exposed as two radio entries in both the `Selection` menu and the ribbon.

### Redux

Redux selection is already **item-granular at the storage level**: each `Item`, `Creature`, `Spawn`, and `Tile` has a `selected` flag, and `Selection` stores the selected `Tile*` set plus an undo session.

But **picking is tile-level only**:

- Mouse events produce a `Position` only. `ScreenToMap` throws away the sub-tile pixel.
- `SelectionController` picks `spawn > creature > tile->getTopItem()`.
- `getTopItem()` is the last non-meta item in the tile's item vector, or the ground if there are no items.
- There is no sprite alpha test, no multi-tile bleed-area picking, and no user-facing choice between standard and pixel-perfect selection.

### Main gap

The missing pieces are:

1. A sub-tile pixel offset from the map canvas to the selection controller.
2. A sprite hit-testing picker that mirrors Redux's renderer (draw offsets, elevation, patterns, multi-tile sprites, alpha).
3. A persisted setting plus menu radio items for **Standard** vs **Pixel Perfect**.
4. A small behavior adjustment so clicking a different item on an already-selected tile selects that item instead of only starting a drag.

The existing selection storage, undo, and rendering do **not** need to be replaced.

---

## 2. ImGui editor: Pixel Perfect Selection

### 2.1 File map

| Concern | File |
| --- | --- |
| Selection strategy interface | `Application/Selection/ISelectionStrategy.h` |
| Smart/default strategy | `Application/Selection/SmartSelectionStrategy.{h,cpp}` |
| Pixel-perfect strategy | `Application/Selection/PixelPerfectSelectionStrategy.{h,cpp}` |
| Selection settings | `Domain/SelectionSettings.h` |
| Settings persistence | `Services/SettingsRegistry.cpp` |
| Input controller | `Controllers/MapInputController.{h,cpp}` |
| Pixel offset computation | `UI/Map/MapPanelInput.cpp` |
| Menu entry | `UI/Map/SelectionMenu.cpp` |
| Ribbon entry | `UI/Ribbon/Panels/SelectionPanel.cpp` |
| Selection data model | `Domain/Selection/SelectionEntry.h`, `SelectionBucket.h` |
| Selection service | `Services/Selection/SelectionService.{h,cpp}` |
| Selection rendering | `Rendering/Overlays/SelectionOverlay.cpp`, `Rendering/Map/TileRenderer.cpp` |
| History integration | `Domain/History/HistoryEntry.{h,cpp}`, `HistoryManager.cpp` |

### 2.2 User-facing behavior (front end)

The mode is a single boolean in `Domain::SelectionSettings`:

```cpp
struct SelectionSettings {
    SelectionFloorScope floor_scope = SelectionFloorScope::CurrentFloor;
    bool use_pixel_perfect = false;
};
```

It is persisted under the key `selection.use_pixel_perfect` by `SettingsRegistry::load()` / `save()`.

There are two UI surfaces, both radio-style:

1. **Selection menu** (`UI/Map/SelectionMenu.cpp`):
   - `Smart Selection` (`ICON_FA_WAND_MAGIC_SPARKLES`)
   - `Pixel Perfect` (`ICON_FA_CROSSHAIRS`)
   - The same menu also contains floor scope options (`Current Floor`, `Visible Floors`, `All Floors`).

2. **Ribbon panel** (`UI/Ribbon/Panels/SelectionPanel.cpp`):
   - Two radio buttons mirroring the menu.
   - Tooltip: "Pixel Perfect Selection — Use sprite hit testing to select the exact item under cursor".

There is no separate "pixel selection tool" in the toolbar. The mode is orthogonal to the current tool: it only affects picking.

### 2.3 Input path and pixel offset

`MapPanelInput` is where sub-tile information is produced. For a click it computes:

```cpp
Domain::Position pos = camera.screenToTile(mouse_pos);
glm::vec2 click_tile_screen = camera.tileToScreen(pos);
glm::vec2 click_pixel_offset =
    (mouse_pos - click_tile_screen) / camera.getZoom();
```

`MapViewCamera::screenToTile()` finds the tile with `floor(camera + local / (TILE_SIZE * zoom))`; `tileToScreen()` converts a tile back to its top-left screen position. Dividing the remaining screen delta by zoom yields a pixel offset in the **32x32 tile space**, independent of zoom.

That offset is passed to:

- `MapInputController::onLeftClick(pos, mods, pixel_offset, session)`
- `MapInputController::onRightClick(...)`
- `MapInputController::onDoubleClick(...)`
- `MapInputController::isSomethingSelectedAt(...)` (used to defer selection on mouse-down so dragging a selected item works)

`MapInputController` owns the active strategy:

```cpp
std::unique_ptr<ISelectionStrategy> current_strategy_;
bool last_was_pixel_perfect_;
```

`ensureCorrectStrategy()` recreates the strategy when `settings.use_pixel_perfect` changes, or when client data is replaced. This is why toggling the menu option takes effect immediately.

### 2.4 Click semantics

`MapInputController::onLeftClick()`:

- `Ctrl+Alt+Click`: smart-pick a brush from the tile, using the strategy to prefer the item under the cursor.
- Brush mode: paint/erase; selection picking is skipped.
- `Ctrl+Shift+Click`: toggle the **whole tile** (all entities).
- `Shift+Click`: clear selection and select the whole tile.
- `Ctrl+Click`: toggle the picked entity.
- Plain click: clear selection and add the picked entity.

Right-click prepares the context selection with the same strategy. Double-click opens properties for the picked item (or spawn/creature by tile priority).

Important: **rectangle/lasso selection are not pixel-perfect**. They operate on whole tiles (`selectRegion`, `LassoSelectionProcessor`). Pixel-perfect only changes single-entity picking.

### 2.5 Backend selection model

The ImGui editor uses a value-based selection model:

- `EntityType`: `Ground`, `Item`, `Creature`, `Spawn`.
- `EntityId`: `{ Position position; EntityType type; uint64_t local_id; }`.
  - For items and creatures, `local_id` is the pointer reinterpreted as an integer.
  - For ground, it is `0`.
- `SelectionEntry`: `EntityId`, a non-owning `entity_ptr` for validation, and a cached `item_id` for copy operations.
- `SelectionBucket`: hash map `EntityId -> SelectionEntry` plus a position index `Position -> {EntityId...}` for O(1) `hasSelectionAt`.
- `SelectionService`: high-level operations (`selectAt`, `selectRegion`, `toggleEntity`, snapshots, observers).

Selection is not stored as a `selected` flag on items. The renderer asks the selection provider whether a given item is selected (`TileRenderer::isItemSelected`). Only creatures/spawns have their domain `select()`/`deselect()` flags synchronized by `SelectionService::syncSelectionState()`.

Undo/redo stores selection snapshots in history entries; the map content and selection are restored together.

### 2.6 Selection rendering

- `Rendering/Overlays/SelectionOverlay.cpp` draws:
  - selected spawns as a cyan rectangle;
  - the drag box while box-selecting;
  - lasso lines.
- Selected items and ground are tinted in `Rendering/Map/TileRenderer.cpp`: the color is halved when `isItemSelected(item)` is true (ground is passed through `GroundRenderer` with `is_ground_selected`).

### 2.7 The pixel-perfect algorithm

`PixelPerfectSelectionStrategy::selectAt(map, pos, pixel_offset)`:

1. It scans a **4x4 region of tiles** from `(pos.x + 4, pos.y + 4)` down to `(pos.x, pos.y)`, same z.
2. For each candidate tile, it computes the click offset relative to that tile:
   ```cpp
   candidate_offset = pixel_offset - glm::vec2(dx * 32, dy * 32);
   ```
3. It calls `findHitOnTile(tile, candidate_pos, candidate_offset)`.
4. It accepts a `Creature` or `Item` hit immediately, even from a neighbouring tile.
5. It accepts a `Ground` hit only on the exact target tile (`dx == 0 && dy == 0`).

Why scan to the right/below? Items are stored at their "origin" tile and are drawn extending left/up for multi-tile sprites. A click on the left/up part of a 2x2 table is physically inside `tile - 1`, but the item is stored in `tile`. The scan recovers that storage tile.

`findHitOnTile()` orders hits as:

1. **Creature** (if the tile has one)
2. **Items**, top-to-bottom
3. **Ground**

#### Creature hit test

- Gets the creature type/outfit through `ClientDataService`.
- Falls back to a 32x32 box if there is no outfit data.
- For multi-tile outfits, computes the sub-tile from the adjusted offset.
- Reads the sprite's RGBA data and accepts if `alpha > 32`.

The creature's draw displacement (`has_offset`, `offset_x/y`) is added back to the click offset because the renderer subtracts it when drawing.

#### Item hit test

`hitTestItem()` handles two paths.

**Fast path**: 1x1 stackable items.

- Uses the subtype/count to choose `sprite_ids[subtype_index]`.
- Loads the sprite through `ClientDataService::getSpriteReader()`.
- Reads the alpha at the local pixel inside the 32x32 sprite.
- Accepts if `alpha > 32`.

**Slow path**: multi-tile, patterned, layered, or animated items.

- Computes pattern coordinates from the tile position:
  - `pattern_x = tile_pos.x % pat_x`
  - `pattern_y = tile_pos.y % pat_y`
  - `pattern_z = tile_pos.z % pat_z`
- Applies overrides for:
  - hangable items (`hookSouth`/`hookEast`);
  - fluid/splash subtypes;
  - stackable counts.
- Computes the sprite index with the classic frame/layer/pattern/height/width ordering:
  ```cpp
  ((((frame % frames) * pat_z + pattern_z) * pat_y + pattern_y) * pat_x + pattern_x) * layers + layer
  ```
  followed by `* height + cy`, `* width + cx`.
- Converts the click offset into a sub-tile:
  ```cpp
  cx = -floor(local_x / 32);
  cy = -floor(local_y / 32);
  ```
- Rejects if outside `[0, width) x [0, height)`.
- Reads the alpha at `local_x + cx * 32`, `local_y + cy * 32`.
- Accepts if `alpha > 10` (lower threshold than the fast path).
- Uses `item_type->draw_offset_x/y` to shift the local coordinates first.

#### Elevation

Before hit-testing items, the strategy builds a render list from bottom to top:

```cpp
float accumulated_elevation = 0.0f;
for (item in tile.items) {
    draw_x = -accumulated_elevation;
    draw_y = -accumulated_elevation;
    render_list.push_back({item, {draw_x, draw_y}});
    if (item_type->hasElevation())
        accumulated_elevation += item_type->elevation;
}
```

It then tests the list in reverse (top to bottom), subtracting each item's render offset from the click offset. This makes tall items and items stacked with elevation pick correctly.

### 2.8 Limitations and quirks of the ImGui implementation

- The search range is hardcoded to **4 tiles**.
- The math assumes 32x32 sprite parts; it uses `item_type->draw_offset_x/y`, but not the renderer's composite width/height metrics.
- Two different alpha thresholds (`> 32` fast path, `> 10` slow path).
- It ignores editor view settings such as hidden items, ghost items, or `SHOW_CREATURES`; it will hit-test things that may not be drawn.
- It ignores floors: always the clicked `pos.z`.
- It allocates a `std::vector<ItemRenderInfo>` per tile per click.
- It does not use the actual render order around `OnTop` items; it assumes item-vector order plus elevation.
- Creature fallback is a full 32x32 box when outfit/sprite data is missing.
- It is used only for single clicks, not box/lasso selection.

These quirks are acceptable in the ImGui editor but should be reviewed when porting to Redux, which has a more complex render order (`RenderSublayer`, depth buffer, borders, ghost mode, floors).

---

## 3. Redux: How selection works today

### 3.1 File map

| Concern | File |
| --- | --- |
| Selection state and sessions | `source/editor/selection.{h,cpp}` |
| Selection worker for box selection | `source/editor/selection_thread.{h,cpp}` |
| Selection operations (move/destroy/borderize/randomize) | `source/editor/operations/selection_operations.{h,cpp}` |
| Selection input controller | `source/rendering/ui/selection_controller.{h,cpp}` |
| Map canvas mouse events | `source/rendering/ui/map_display.{h,cpp}` |
| Screen-to-map conversion | `source/rendering/core/coordinate_mapper.{h,cpp}`, `map_view_math.{h,cpp}` |
| Selection rendering | `source/rendering/drawers/entities/item_drawer.cpp`, `creature_drawer.cpp`, `source/rendering/drawers/overlays/selection_drawer.cpp`, `source/rendering/core/chunk_cache_manager.cpp` |
| Selection flags | `source/map/tile.h`, `source/game/item.h` |
| Selection type settings | `source/app/settings.{h,cpp}` |
| Selection menu actions | `data/menubar.xml`, `source/ui/menubar/view_settings_handler.cpp`, `source/ui/menubar/menubar_action_manager.cpp` |

### 3.2 User-facing behavior (front end)

Redux has a **Selection Mode** and a **floor scope** setting, but no picking-algorithm setting.

Selection Mode:

- Entered with `Space` (`KeyboardHandler::OnKeyDown` calls `g_gui.SwitchMode()`), the map tab "gem" (`MapWindow::OnGem`), a stored position hotkey, or by right-clicking while in drawing mode (`MapCanvas::OnMousePropertiesClick`).
- In selection mode, left-click selects, left-drag moves the current selection, `Shift+Drag` box-selects, `Ctrl+Click` toggles a single item, `Ctrl+Shift+Click` toggles the whole tile.
- `Delete` destroys the selection. The current keyboard handler does not use `Escape` as a general clear-selection shortcut; it only cancels temple placement.

Floor scope (`Selection > Selection Mode` in `data/menubar.xml`):

- `Compensate Selection`
- `Current Floor`
- `Floors Below`
- `Visible Floors`

These are radio menu items wired to `ViewSettingsHandler::OnSelectionTypeChange()`, stored as `Config::SELECTION_TYPE` and `Config::COMPENSATED_SELECT`.

There is no menu entry for "Smart" vs "Pixel Perfect"; the only picker is the built-in top-item behavior.

### 3.3 Input path

`MapCanvas` receives wx mouse events and converts the cursor to a map tile:

```cpp
int mouse_map_x, mouse_map_y;
ScreenToMap(event.GetX(), event.GetY(), &mouse_map_x, &mouse_map_y);
selection_controller->HandleClick(
    Position(mouse_map_x, mouse_map_y, floor),
    event.ShiftDown(), event.ControlDown(), event.AltDown());
```

`ScreenToMap` calls `CoordinateMapper::ScreenToMap` -> `MainMapViewMath::ScreenToMap`, which computes:

```cpp
map_x = (view_scroll_x + screen_x * scale_factor * zoom) / TILE_SIZE;
map_y = (view_scroll_y + screen_y * scale_factor * zoom) / TILE_SIZE;
map_x += GROUND_LAYER - floor; // for floors <= GROUND_LAYER
map_y += GROUND_LAYER - floor;
```

The `int` division discards the sub-tile pixel. The selection controller therefore only knows the tile, not where inside the 32x32 tile the user clicked.

Mouse event call sites:

- `OnMouseActionClick` -> `SelectionController::HandleClick`
- `OnMouseActionRelease` -> `SelectionController::HandleRelease`
- `OnMousePropertiesClick/Release` -> `HandlePropertiesClick/Release`
- `OnMouseLeftDoubleClick` -> `HandleDoubleClick`
- `OnMouseMove` -> `HandleDrag` and, in drawing mode, `DrawingController`

### 3.4 Backend selection model

Redux selection is stored **in the map objects themselves**, with a list of selected tiles for fast iteration:

- `Item`, `Creature`, `Spawn` have `selected` flags (`Item::select()`, `Creature::select()`, etc.).
- `Tile` has `TILESTATE_SELECTED`.
- `Selection` owns:
  ```cpp
  std::vector<Tile*> tiles;
  std::vector<Tile*> pending_adds;
  std::vector<Tile*> pending_removes;
  std::unique_ptr<BatchAction> session;
  std::unique_ptr<Action> subsession;
  ```
- `tiles` is kept sorted by position for cache locality and O(log n) membership checks.
- `Selection::add(tile, item)` applies the `BORDER_IS_GROUND` special case. Inside a selection session it records a modified tile copy in the current `ACTION_SELECT` action; outside a session it mutates the live item and adds the tile to `tiles`. The committed action calls `TileOperations::updateSelectionState(tile)`.
- `TileOperations::updateSelectionState` computes whether any entity on the tile is selected and toggles `TILESTATE_SELECTED`.
- `Selection::start()/commit()/finish()` create `ACTION_SELECT` actions and push a `BatchAction` through `ActionQueue` for undo/redo. `SelectionController` uses a session so every click is undoable.
- `SelectionThread` is used for large box selections; each worker selects whole tiles into a `SUBTHREAD` subsession, then `Selection::join()` merges the result.

`Selection` also exposes `onSelectionChange` and listener callbacks; Lua scripts receive a `selectionChange` event when a selection finishes.

### 3.5 What a click actually selects

`SelectionController::HandleClick()` contains the core picker:

```cpp
// Ctrl+Alt = pick a brush from the tile.
if (ctrl_down && alt_down) {
    ...
}

else if (g_gui.IsSelectionMode()) {
    if (pasting) { ... }
    else if (shift_down) {
        // Begin box selection. Clear first unless Ctrl is held.
    } else if (ctrl_down) {
        Tile* tile = FindSelectedTileUnderCursor(pos);
        if (tile) {
            if (tile->spawn && SHOW_SPAWNS) {
                toggle spawn
            } else if (tile->creature && SHOW_CREATURES) {
                toggle creature
            } else {
                Item* item = tile->getTopItem();
                if (item) toggle item
            }
        }
    } else {
        Tile* tile = FindSelectedTileUnderCursor(pos);
        if (!tile) {
            clear selection
        } else if (tile->isSelected()) {
            start dragging
        } else {
            clear selection
            if (spawn visible) select spawn
            else if (creature visible) select creature
            else if (Item* item = tile->getTopItem()) select item
        }
    }
}
```

`Tile::getTopItem()`:

```cpp
Item* Tile::getTopItem() const {
    if (!items.empty() && !items.back()->isMetaItem())
        return items.back().get();
    if (ground && !ground->isMetaItem())
        return ground.get();
    return nullptr;
}
```

Consequences:

- The pick is the **last item in the tile's vector**, not the visually topmost item under the cursor.
- Ground is only picked if there are no regular items (or the last item is a meta item).
- There is no alpha test, so clicking a transparent pixel of a large item selects that item if it is last.
- Clicking the left/up bleed area of a multi-tile item on an adjacent tile selects whatever is on the clicked tile, not the multi-tile item.
- `FindSelectedTileUnderCursor` can return a selected tile on a visible lower floor; this supports dragging multi-floor selections.
- `tile->isSelected()` is the gate for "start dragging", so clicking a different item on an already-selected tile starts a drag instead of picking the new item.

`HandleRelease` repeats a simplified version of the picker when the mouse did not move:

```cpp
Tile* tile = FindSelectedTileUnderCursor(pos);
if (tile) {
    if (spawn ... && !spawn->isSelected()) add spawn
    else if (creature ... && !creature->isSelected()) add creature
    else {
        Item* item = tile->getTopItem();
        if (item && !item->isSelected()) add item
    }
}
```

`HandlePropertiesClick/Release` (right-click) uses the same top-item logic and ensures selection mode is active.

`HandleDoubleClick` opens tile properties, not a specific item.

### 3.6 Selection rendering

- `ItemDrawer::BlitItem` checks `item->isSelected()` and halves RGB (`red >>= 1`, etc.). The same check is duplicated in the chunk-cache baker (`chunk_cache_manager.cpp`).
- `CreatureDrawer::BlitCreature` halves the creature color when `creature->isSelected()`.
- `TileRenderer` tints a selected ground through `GroundRenderer`.
- `SelectionDrawer` draws only the transient box (`boundbox_selection`, `transient_selection_bounds`) and does not draw a per-item outline.
- Because selection is stored on the item, a pixel-perfect item selection will automatically show the correct tint with **no renderer change**.

### 3.7 Undo, save, and OTBM

- Selection changes are user-visible but are not map content; OTBM serialization does not write selection flags.
- Selection actions still go through `ActionQueue` (`ACTION_SELECT`) so that selection changes are undoable. `Selection::add` deep-copies the tile for the action.
- Pixel-perfect picking must keep using `Selection::add(...)`/`Selection::remove(...)`; it must not mutate item flags directly.

---

## 4. Comparison

| Concern | ImGui editor | Redux | Gap for parity |
| --- | --- | --- | --- |
| Selection granularity | Entity (`EntityId` + service bucket) | Entity flags on map objects + selected tile list | None; Redux can already select an exact item. |
| Default pick | Spawn > Creature > Top item > Ground | Spawn > Creature > Top item (`items.back()`) | Similar, but Redux's "top" is vector order, not visual order. |
| Pixel hit test | Sprite RGBA alpha at sub-tile pixel | None | Must add. |
| Sub-tile pixel info | Computed in `MapPanelInput` and passed down | Thrown away by `ScreenToMap` | Must add coordinate plumbing. |
| Multi-tile items | Scan 4 neighboring tiles and reverse-render | Only the clicked tile | Must scan storage tiles. |
| Elevation | Accumulated `elevation` in hit test | Renderer has `draw_height` elevation; picker ignores it | Must mirror renderer. |
| Pattern/subtype | Item-type pattern logic in strategy | `PatternCalculator` already exists | Reuse Redux's `PatternCalculator`. |
| Draw offsets | `item_type->draw_offset_x/y` | `GameSprite::getDrawOffset()` includes composite metrics | Use Redux math, not ImGui math. |
| Creature hit test | Outfit sprite alpha, fallback box | Box only (creature priority) | Optional phase; can fall back to standard. |
| Spawn hit test | Ignored in pixel mode | Spawn always wins if visible | Decide priority; document divergence. |
| UI choice | Menu + ribbon radio, persisted bool | Only floor scope menu | Add menu radio + config bool. |
| Undo | Selection snapshots in history | `ACTION_SELECT` batch in `ActionQueue` | None; reuse existing path. |
| Rendering | Tint via selection provider | `selected` flag tint | None; existing tint works. |
| Box/lasso | Tile-based, unaffected | Tile-based `SelectionThread` | Keep tile-based. |
| Floors | Current floor only | Floor scope + lower-floor selected tile lookup | Pixel picker needs to respect the resolved tile. |
| Performance | Per-click vector + sprite decode | No per-click sprite decode today | Add per-click only; optional alpha cache. |

### Practical interpretation

Redux is **closer** to the goal than it first looks: it already selects individual items and already has undoable item-level selection. The work is almost entirely in **picking** and **input plumbing**, not in the selection model.

The one architectural difference that matters: the ImGui editor stores selection in a service, while Redux stores `selected` flags on map objects. That means the Redux picker must return the **storage tile** for a multi-tile item, because `Selection::add(tile, item)` marks the item and updates that tile's selection state.

---

## 5. Constraints that the Redux implementation must respect

From `AGENTS.md` and the existing code:

- Do not mutate tiles from input handlers. Selection changes are not map edits, but they should still go through `Selection` (which creates the undo action), not raw `item->select()`.
- Every user-visible map change must be undoable. Pixel-perfect selection only changes selection, and `Selection` already handles `ACTION_SELECT`.
- Do not change OTBM or map serialization.
- Do not change `Tile` layout as part of this feature.
- Keep the render hot path free of per-frame allocation. Hit testing runs on click, not per frame, so it may allocate, but avoid putting it in `MapDrawer`/`TileRenderer`.
- Respect client versions. Sprite data must come from `g_gui.gfx` / the loaded `GameSprite`; do not hardcode sprite IDs.
- Use `Theme` for UI. The menu/tooltips use the existing menu system, so no new hardcoded colors.
- Branch from `master`; keep the feature atomic.

---

## 6. What was verified and what was not

Verified by reading the source:

- The ImGui pixel-perfect strategy, its 4-tile scan, alpha thresholds, elevation handling, and menu/ribbon wiring.
- The ImGui selection service and history snapshot integration.
- Redux's `Selection` session/undo model, tile/item flags, and `SelectionController` picker.
- Redux's coordinate conversion and the fact that it discards sub-tile pixels.
- Redux's sprite pipeline: `GameSprite` has `spriteList` of `NormalImage`, and `NormalImage::getRGBAData()` can read RGBA from `SpriteArchive`.
- Redux's render order and the `RenderSublayer` depth model.

Not verified by running the applications:

- Exact visual behavior at non-integer zoom and high-DPI scale factors.
- The cost of reading sprite RGBA on click for large stacks or protobuf sprite sheets.
- Whether every DAT/protobuf client format exposes all needed sprite IDs in `GameSprite::spriteList`.
- The exact behavior of `BORDER_IS_GROUND` when a border item is picked in pixel mode.
- Creature outfit hit testing with mounts/addons in all client versions.

These are called out again in the implementation plan's risks section.
