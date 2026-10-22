# Plan: Pixel-Perfect Selection for RME Redux

Related analysis: [selection-analysis.md](selection-analysis.md).

> **Implementation status:** implemented on branch `feature/pixel-perfect-selection`.
> The setting/menu, sub-tile coordinate plumbing, standard/pixel picker (items,
> multi-tile items, borders, ground, creatures), selection-controller integration,
> and geometry tests are in place. The application builds successfully with
> `build_ninja.bat`; the standalone geometry test passes. Runtime/manual testing on
> a real map and client data is still required.

## 1. Goal

Add a user-selectable picking method to Redux:

- **Standard Selection** — current behavior: spawn > creature > `Tile::getTopItem()`.
- **Pixel Perfect Selection** — pick the exact visible entity under the mouse using sprite alpha hit testing, including multi-tile items and (optionally) creatures.

The method must be selectable from the `Selection` menu, persisted across restarts, and must not change:

- the selection data model;
- selection undo/redo;
- box/lasso selection;
- map rendering;
- OTBM serialization.

The default remains **Standard Selection**, so existing users see no behavior change until they opt in.

## 2. User-facing UX

### 2.1 Menu

Add a submenu under `Selection` in `data/menubar.xml`:

```xml
<menu name="Selection Method">
    <item name="Standard Selection" action="SELECT_METHOD_STANDARD"
          help="Select the top entity on the clicked tile. Current behavior."/>
    <item name="Pixel Perfect Selection" action="SELECT_METHOD_PIXEL_PERFECT"
          help="Use sprite hit testing to select the exact item visible under the cursor."/>
</menu>
```

The two items are `wxITEM_RADIO` and live next to the existing `Selection Mode` (floor scope) submenu. They are independent:

- `Selection Mode` controls **which floors** are affected by box selection.
- `Selection Method` controls **how a single click picks an entity**.

A short tooltip in the menu item help text is enough; there is no new dialog.

### 2.2 Setting

Add to `source/app/settings.h`:

```cpp
PIXEL_PERFECT_SELECTION,
```

Add to `source/app/settings.cpp` in the `Editor` section:

```cpp
Bool(PIXEL_PERFECT_SELECTION, false);
```

The setting is global (not per-map), matching the existing floor-scope settings.

### 2.3 Menu wiring

Files:

- `source/ui/main_menubar.h`
  - Add `SELECT_METHOD_STANDARD` and `SELECT_METHOD_PIXEL_PERFECT` to `MenuBar::ActionID`.
  - Add `void OnSelectionMethodChange(wxCommandEvent& event);`.
- `source/ui/menubar/menubar_action_manager.cpp`
  - `MAKE_ACTION_ICON(SELECT_METHOD_STANDARD, wxITEM_RADIO, ICON_WAND_MAGIC_SPARKLES, OnSelectionMethodChange);`
  - `MAKE_ACTION_ICON(SELECT_METHOD_PIXEL_PERFECT, wxITEM_RADIO, ICON_CROSSHAIRS, OnSelectionMethodChange);`
- `source/ui/menubar/view_settings_handler.{h,cpp}`
  - Add `OnSelectionMethodChange(wxCommandEvent&)`.
  - In `LoadValues()`, check the correct radio item from `g_settings.getBoolean(Config::PIXEL_PERFECT_SELECTION)`.
  - In the handler, write the setting and refresh the map canvas so the next click uses it immediately.
- `source/ui/main_menubar.cpp`
  - Delegate `MainMenuBar::OnSelectionMethodChange` to `viewSettingsHandler`.

No `OnChangeViewSettings` changes are needed; the method is not a view setting.

## 3. Architecture

### 3.1 New types

Add `source/rendering/ui/selection_picker.{h,cpp}` (or `source/editor/selection/selection_picker.{h,cpp}` if it should not live under rendering; see note below).

```cpp
enum class SelectionMethod : uint8_t {
    Standard,
    PixelPerfect,
};

struct SelectionPixelOffset {
    int x = 0;
    int y = 0;
};

enum class SelectionHitKind : uint8_t {
    None,
    Ground,
    Item,
    Creature,
    Spawn,
};

struct SelectionHit {
    SelectionHitKind kind = SelectionHitKind::None;
    Tile* tile = nullptr;        // storage tile, not necessarily the clicked tile
    Item* item = nullptr;        // item or ground
    Creature* creature = nullptr;
    Spawn* spawn = nullptr;
};

class SelectionPicker {
public:
    // Returns the entity under the cursor for the given method.
    static SelectionHit Pick(Editor& editor,
                             const Position& clicked_pos,
                             const SelectionPixelOffset& pixel,
                             SelectionMethod method);
};
```

Placement note: the picker needs `Editor`, `Map`, `Tile`, `Item`, `GameSprite`, `PatternCalculator`, `g_gui.gfx`, and `g_settings`. `source/rendering/ui/selection_picker` is the natural home because `SelectionController` already lives there and this is input/picking logic, not map state. The file must be added to `source/CMakeLists.txt` (both header and source lists).

Do **not** create an `ISelectionStrategy` hierarchy like the ImGui editor. Two concrete modes and a `switch` are enough; an interface would be an abstraction with no second use case beyond the two known ones.

### 3.2 SelectionController integration

`SelectionController` remains the orchestrator. It should delegate picking and keep the selection session logic:

```cpp
SelectionHit hit = SelectionPicker::Pick(
    editor, pos, pixel,
    g_settings.getBoolean(Config::PIXEL_PERFECT_SELECTION)
        ? SelectionMethod::PixelPerfect
        : SelectionMethod::Standard);
```

The existing selection calls stay:

- `editor.selection.add(hit.tile, hit.item)` for items/ground;
- `editor.selection.add(hit.tile, hit.creature)`;
- `editor.selection.add(hit.tile, hit.spawn)`;
- `editor.selection.remove(...)` for toggles.

That preserves undo and the `BORDER_IS_GROUND` behavior.

### 3.3 Coordinate plumbing

Add sub-tile information to the existing screen-to-map path.

`source/rendering/core/map_view_math.{h,cpp}`:

```cpp
struct MainMapPixelPosition {
    int map_x = 0;
    int map_y = 0;
    int pixel_x = 0; // 0..TILE_SIZE-1
    int pixel_y = 0;
};

static MainMapPixelPosition ScreenToMapPixel(
    int screen_x, int screen_y,
    const MainMapViewport& viewport);
```

Implementation must use the same arithmetic as `ScreenToMap`:

```cpp
const int scaled_x = static_cast<int>(screen_x * viewport.scale_factor);
const int scaled_y = static_cast<int>(screen_y * viewport.scale_factor);

const int raw_x = (scaled_x < 0)
    ? (viewport.view_scroll_x + scaled_x)
    : (viewport.view_scroll_x + static_cast<int>(scaled_x * viewport.zoom));
// same for y

result.map_x = raw_x / TILE_SIZE + GetFloorTileOffset(viewport.floor);
result.map_y = raw_y / TILE_SIZE + GetFloorTileOffset(viewport.floor);
result.pixel_x = raw_x - (raw_x / TILE_SIZE) * TILE_SIZE;
result.pixel_y = raw_y - (raw_y / TILE_SIZE) * TILE_SIZE;
if (result.pixel_x < 0) result.pixel_x += TILE_SIZE;
if (result.pixel_y < 0) result.pixel_y += TILE_SIZE;
```

Keep `ScreenToMap` implemented in terms of the new helper (or share a private helper) so the two conversions cannot drift.

`source/rendering/ui/map_display.{h,cpp}`:

- Add an overload or a small result type that returns both `Position` and `SelectionPixelOffset`.
- The helper must use `GetViewStart`, `zoom`, and `GetContentScaleFactor()` exactly like `ScreenToMap`.

`source/rendering/ui/selection_controller.{h,cpp}`:

- Add `SelectionPixelOffset last_click_pixel_` (or pass the offset as a parameter and store it only for the release path).
- `HandleClick(pos, pixel, shift, ctrl, alt)` stores the pixel and uses it immediately.
- `HandlePropertiesClick(pos, pixel, shift, ctrl, alt)` uses it immediately.
- `HandleRelease` uses the stored pixel offset from the matching mouse-down. This matters because wx may report a release position one pixel away from the press position.

Exact call-site changes in `MapCanvas`:

- `OnMouseActionClick`: compute `Position` + `SelectionPixelOffset`, pass to `HandleClick`.
- `OnMousePropertiesClick`: same for `HandlePropertiesClick`.
- `OnMouseActionRelease`: pass the stored offset or let the controller keep it.
- `OnMousePropertiesRelease`: same.

Do not change `ScreenToMap`'s public behavior; other callers (minimap, screenshot, Lua, replace tool) depend on it.

## 4. Standard picker (preserve current behavior)

`SelectionPicker::PickStandard()` should reproduce `SelectionController` exactly:

```cpp
SelectionHit PickStandard(Editor& editor, const Position& pos) {
    Tile* tile = editor.map.getTile(pos);
    if (!tile) return {};

    if (tile->spawn && g_settings.getInteger(Config::SHOW_SPAWNS))
        return { SelectionHitKind::Spawn, tile, nullptr, nullptr, tile->spawn.get() };

    if (tile->creature && g_settings.getInteger(Config::SHOW_CREATURES))
        return { SelectionHitKind::Creature, tile, nullptr, tile->creature.get(), nullptr };

    if (Item* item = tile->getTopItem())
        return { item->isGroundTile() ? SelectionHitKind::Ground : SelectionHitKind::Item,
                 tile, item, nullptr, nullptr };

    return {};
}
```

This keeps current behavior for every existing click path and becomes the fallback when pixel data is unavailable.

## 5. Pixel-perfect picker

### 5.1 Overview

The picker must mirror **what Redux actually draws**, not the ImGui renderer, because the two renderers differ in:

- sublayers and depth;
- border handling;
- `GameSprite::getDrawOffset()` including composite metrics;
- ground/terrain pass versus item pass;
- ghost/hidden item options;
- multi-floor visibility.

The algorithm:

1. If client data or `g_gui.gfx` is unavailable, return `PickStandard()`.
2. Scan candidate storage tiles from the clicked tile outward to the right/below (items extend left/up from their storage tile), in visual front-to-back order.
3. For the clicked tile itself, test creatures, items, borders, and ground.
4. For neighbouring storage tiles, test only items (and optionally creatures).
5. Return the first opaque hit; ground only from the clicked tile.

### 5.2 Search range

Use a named constant:

```cpp
constexpr int kPixelSelectionSearchRange = 4;
```

This matches the ImGui feature (up to 5x5 candidate tile positions, including the clicked tile) and covers the common 1x1-4x4 item footprint. `MAX_SPRITE_PARTS` is 16 in Redux, but scanning 16x16 tiles per click is unnecessary. Document the limitation. If a real map needs larger items, raise the constant or compute a per-item bounding box; do not make it a user setting.

Loop order must be **front-to-back**, i.e. descending `dy` and descending `dx`, because a storage tile with larger `x + y` is drawn later (in front) on the isometric plane:

```cpp
for (int dy = kPixelSelectionSearchRange; dy >= 0; --dy) {
    for (int dx = kPixelSelectionSearchRange; dx >= 0; --dx) {
        Position storage{pos.x + dx, pos.y + dy, pos.z};
        SelectionPixelOffset local{pixel.x - dx * TILE_SIZE,
                                   pixel.y - dy * TILE_SIZE};
        // test tile at storage with local offset
    }
}
```

This is the same coordinate convention as the ImGui strategy: a click at `pos` with local pixel `p` maps to `p - (dx*32, dy*32)` on the storage tile.

### 5.3 Hit order inside a tile

Use the Redux render order, not a naive reverse vector walk:

1. **Creature** (if visible)
2. **Top-effect items** (`ItemFlag::TopEffect`; render sublayer `TopItem`)
3. **Common items** (later elevation steps first)
4. **Always-on-bottom items** (`RenderSublayer::BottomItem`)
5. **Border items** (`Item::isBorder()`; `RenderSublayer::Border`)
6. **Ground**
7. **Spawn** fallback if nothing above hit and the spawn is visible

Reasoning:

- `TileRenderer::RenderStaticItems()` walks `tile->items` in vector order, but the depth value determines occlusion.
- `TopEffect` items are sublayer 10, creatures are 8, common items are 4 + elevation step, bottom items are 3, borders are 1, ground is 0.
- Within the same sublayer, later items in the vector win.
- Borders are drawn in `RenderStaticTerrain`, before regular items.
- Ground is drawn first.

A first implementation can use a simpler order (creature, reverse common items, border, ground) and add exact sublayer ordering as a follow-up, but the plan is to implement the full order because wrong picking is immediately visible to users.

Do **not** pick `isMetaItem()` items. They are editor-only markers and `Tile::getTopItem()` already skips them.

### 5.4 Item pixel test

For a given item and tile-local pixel `(px, py)`:

1. Get `ItemDefinitionView it = item->getDefinition()`. If missing:
   - fall back to a 32x32 box check (so invalid/unknown items remain selectable);
   - or skip if `show_invalid_tiles` is off. Match the current standard picker's behavior so users do not lose access to items.
2. Get `GameSprite* sprite = item->getSprite()`.
   - If null, use a 32x32 box check.
3. Compute patterns with Redux's existing helper:
   ```cpp
   SpritePatterns patterns = PatternCalculator::Calculate(
       sprite, it, item, tile, tile->getPosition(), g_gui.gfx.getElapsedTime());
   ```
   This automatically handles stackable, fluid/splash, hangable, pattern x/y/z, and animation frame.
4. Compute the composite layout:
   ```cpp
   auto metrics = sprite->getPlainLayoutMetrics(
       patterns.subtype.value_or(-1),
       patterns.x, patterns.y, patterns.z, patterns.frame);
   ```
5. Compute the item's origin inside the tile:
   - The item pass starts at the tile's top-left (`draw_x = 0`, `draw_y = 0`).
   - Each preceding item in `RenderStaticItems()` subtracts its `sprite->draw_height` from the running draw position. So the item's origin is `(-elevation_x, -elevation_y)`.
   - In the clicked tile's local space, the item's top-left is therefore:
     ```cpp
     origin_x = -accumulated_elevation;
     origin_y = -accumulated_elevation;
     ```
   - `accumulated_elevation` is the sum of `sprite->draw_height` of preceding items in the same pass. It is not affected by the ground (Redux starts the item pass fresh after the terrain pass).
6. Apply the sprite draw offset:
   ```cpp
   auto [offset_x, offset_y] = sprite->getDrawOffset();
   ```
7. Walk the composite parts in the same order as `ItemDrawer::BlitItem()`:
   ```cpp
   int x_offset = 0;
   for (int cx = 0; cx < metrics.num_columns; ++cx) {
       int y_offset = 0;
       for (int cy = 0; cy < metrics.num_rows; ++cy) {
           for (int layer = 0; layer < sprite->layers; ++layer) {
               int part_x = origin_x - offset_x - x_offset;
               int part_y = origin_y - offset_y - y_offset;
               if (px >= part_x && px < part_x + metrics.column_widths[cx] &&
                   py >= part_y && py < part_y + metrics.row_heights[cy]) {
                   // test alpha at (px - part_x, py - part_y)
               }
           }
           y_offset += metrics.row_heights[cy];
       }
       x_offset += metrics.column_widths[cx];
   }
   ```
   This is the exact placement used by `ItemDrawer::BlitItem()`.

8. Resolve the sprite index for the part:
   ```cpp
   size_t index = sprite->getIndex(cx, cy, layer,
                                   patterns.x, patterns.y, patterns.z,
                                   patterns.frame);
   ```
   `GameSprite::getIndex()` already uses the same ordering as `getAtlasRegion()`.
9. Get the sprite ID:
   ```cpp
   uint32_t sprite_id = index < sprite->sprite_ids.size()
       ? sprite->sprite_ids[index] : 0;
   ```
   If `sprite_id == 0` and `index < sprite->spriteList.size()`, fall back to the `NormalImage::id` (some paths populate `spriteList` without `sprite_ids`; normally they match).
10. Read the alpha and compare to a threshold.

### 5.5 Alpha access

The sprite archive exposes RGBA:

```cpp
std::shared_ptr<SpriteArchive> archive = g_graphics.getSpriteArchive();
std::unique_ptr<uint8_t[]> rgba;
ImageDimensions dims;
if (archive->readRGBA(sprite_id, g_graphics.hasTransparency(), rgba, dims)) {
    size_t pixel = (y * dims.width + x) * 4;
    return rgba[pixel + 3] > threshold;
}
```

`NormalImage::getRGBAData()` is an alternative and already handles `id == 0` and dimension updates. For a first version, use `NormalImage` when available:

```cpp
NormalImage* image = (index < sprite->spriteList.size() &&
                      sprite->spriteList[index] &&
                      sprite->spriteList[index]->isNormalImage())
    ? static_cast<NormalImage*>(sprite->spriteList[index])
    : nullptr;

auto rgba = image ? image->getRGBAData() : nullptr;
```

Implemented threshold: `alpha > 32`, used for both simple and composite sprites.
This matches the ImGui fast path and avoids selecting antialiased fringe pixels. If
players report that very faint sprite edges are not selectable, lower the constant
in one place (`kPixelSelectionAlphaThreshold`) and verify on real sprites.

Do not read the OpenGL atlas; that would require a GPU readback and stall the frame. Always read from the CPU sprite archive.

### 5.6 Multi-tile item storage tile

When the hit comes from a candidate tile `pos + (dx, dy)`, return that **candidate tile** as `SelectionHit::tile`, not the clicked tile. This is required because:

- `Selection::add(tile, item)` updates the tile's selection state;
- `SelectionOperations::moveSelection`, copy/cut, and `TileOperations::getSelectedItems` operate on the storage tile;
- the item may be the only selected entity on that tile.

The visual highlight already covers all sprite parts because `ItemDrawer` draws the whole item when selected.

### 5.7 Creature hit test

Creature hit testing is more complex than items because of outfits, directions, addons, mounts, and animation frames. It is implemented in `SelectionPicker` with a 32x32 fallback when outfit/sprite data is unavailable.

When implemented:

1. Use the same `GameSprite` returned by `g_gui.gfx.getCreatureSprite(outfit.lookType)` as `CreatureDrawer::BlitCreature`.
2. Compute `dir = static_cast<int>(creature->getDirection())`, `pattern_z` from mount presence, `frame` from animation, and addon patterns from `outfit.lookAddon`.
3. Use `GameSprite::getOutfitLayoutMetrics(dir, addon, pattern_z, frame)` and the same `base_x = screenx - sprite->getDrawOffset().first` math as the renderer.
4. Alpha-test every drawn addon layer and, if present, the mount sprite.
5. If the outfit sprite or RGBA data is unavailable, fall back to a 32x32 box check (matches ImGui).

Creature priority should be documented: ImGui checks creature first; Redux's standard picker does too. Keep that for parity, even though a top-effect roof can visually cover a creature.

### 5.8 Spawn handling

Spawns have no sprite. The ImGui pixel-perfect strategy does not hit-test them. Redux's standard picker always prefers spawns when `SHOW_SPAWNS` is on.

Recommended behavior for Redux:

- **Standard mode:** unchanged (spawn first).
- **Pixel-perfect mode:** test creature/items/borders/ground first; if no hit, select the spawn on the clicked tile (if visible). This preserves access to spawns without letting a spawn block item picking.

Document this as a deliberate divergence from the ImGui editor.

### 5.9 Hidden/ghost items and view settings

The first version should at least respect:

- `Config::SHOW_ITEMS` — if items are hidden, do not pick items. Falling through to ground/spawn would be reasonable.
- `Config::SHOW_CREATURES` — do not pick creatures when hidden.
- `Config::SHOW_SPAWNS` — do not pick spawns when hidden.

Ghost/transparent items are still drawn; pick them (the user can see them). Do not add new filtering for `TRANSPARENT_ITEMS` or `HIDE_ITEMS_WHEN_ZOOMED` unless users report confusion; pixel-perfect should follow "what is drawn".

## 6. SelectionController changes

### 6.1 Common helper

Replace the repeated spawn/creature/top-item blocks with helpers:

```cpp
SelectionHit PickAt(const Position& pos, const SelectionPixelOffset& pixel) const;
bool IsHitSelected(const SelectionHit& hit) const;
void SelectHit(const SelectionHit& hit);       // clear + add
void ToggleHit(const SelectionHit& hit);
```

This keeps `HandleClick`, `HandleRelease`, `HandlePropertiesClick`, and `HandlePropertiesRelease` from diverging. It is a refactor inside the class being modified, allowed by the project rules.

### 6.2 Click behavior

Plain click:

```cpp
SelectionHit hit = PickAt(pos, pixel);
const bool already_selected =
    pixel_method ? IsHitSelected(hit) : ClickedTileWasSelected(pos);

if (hit.kind == SelectionHitKind::None) {
    editor.selection.start();
    editor.selection.clear();
    editor.selection.finish();
} else if (already_selected) {
    dragging = true;
    drag_start_pos = pos;
} else {
    SelectHit(hit);
    dragging = true;
    drag_start_pos = pos;
}
```

`ClickedTileWasSelected(pos)` preserves the old standard-mode rule (`tile->isSelected()` on the clicked tile). `IsHitSelected(hit)` is used only in pixel mode; it fixes the case where the clicked tile is selected but the pixel-perfect hit is a different item.

Ctrl+click: toggle `hit` exactly (not `tile->getTopItem()`).

Shift+click: unchanged box selection.

Ctrl+Shift+click: unchanged whole-tile toggle.

Right-click: use `PickAt` to prepare the context selection; if the hit is already selected, keep the multi-selection. This mirrors `prepareContextSelection` in the ImGui editor.

Release without movement: use `PickAt` so the exact item is selected even if the down event deferred it. Keep the existing `FindSelectedTileUnderCursor` only for drag detection on lower selected floors.

Double-click: unchanged (tile properties). Optionally use the hit to open item properties instead of tile properties, but that is a separate behavior change and should not be mixed into this feature.

### 6.3 Drag behavior with multi-tile items

If the picked item's storage tile is not the clicked tile:

- start dragging from the clicked position (same tile delta applies to the item);
- use the storage tile for selection;
- in pixel mode, do not use `tile->isSelected()` on the clicked tile as the drag gate; use `IsHitSelected(hit)`. Standard mode keeps the old gate.

This makes a multi-tile item draggable from any visible part.

### 6.4 `FindSelectedTileUnderCursor`

Keep the existing function for:

- dragging a selection that lives on a visible lower floor;
- backward compatibility with the current drag behavior.

Pixel picking should use the **clicked** floor and its neighboring storage tiles. If the click lands on a selected lower-floor tile, the existing lower-floor lookup may still return it; do not try to pixel-pick across floors in the first version.

## 7. File-by-file change list

| File | Change |
| --- | --- |
| `docs/selection/selection-analysis.md` | This analysis. |
| `docs/selection/pixel-perfect-selection-plan.md` | This plan. |
| `data/menubar.xml` | Add `Selection Method` submenu with two radio items. |
| `source/app/settings.h` | Add `Config::PIXEL_PERFECT_SELECTION`. |
| `source/app/settings.cpp` | Add `Bool(PIXEL_PERFECT_SELECTION, false);`. |
| `source/ui/main_menubar.h` | Add action IDs and handler declaration. |
| `source/ui/main_menubar.cpp` | Delegate handler. |
| `source/ui/menubar/menubar_action_manager.cpp` | Register radio actions and icons. |
| `source/ui/menubar/view_settings_handler.{h,cpp}` | Load/save/check the method radio items. |
| `source/rendering/core/map_view_math.{h,cpp}` | Add `ScreenToMapPixel` and share arithmetic. |
| `source/rendering/ui/map_display.{h,cpp}` | Return/pass sub-tile pixel offset on mouse down. |
| `source/rendering/ui/selection_controller.{h,cpp}` | Accept pixel offset; delegate picking; adjust drag gate. |
| `source/rendering/ui/selection_picker.{h,cpp}` | **New** standard/pixel picker and sprite alpha hit test. |
| `source/CMakeLists.txt` | Add the new picker header/source. |
| `tests/` (optional) | Pure coordinate/part-rect test if a suitable standalone test pattern is added. |

No changes to:

- `source/editor/selection.*` (selection sessions/undo);
- `source/map/tile.*` (layout);
- `source/rendering/drawers/**` (tint/overlay);
- OTBM readers/writers;
- Lua bindings.

## 8. Algorithm details and pseudocode

### 8.1 Pixel-perfect pick

```cpp
SelectionHit SelectionPicker::PickPixelPerfect(
    Editor& editor, const Position& pos, const SelectionPixelOffset& pixel) {

    if (g_gui.gfx.isUnloaded() || !g_gui.gfx.getSpriteArchive())
        return PickStandard(editor, pos);

    constexpr int R = kPixelSelectionSearchRange;

    for (int dy = R; dy >= 0; --dy) {
        for (int dx = R; dx >= 0; --dx) {
            Position storage{pos.x + dx, pos.y + dy, pos.z};
            Tile* tile = editor.map.getTile(storage);
            if (!tile)
                continue;

            SelectionPixelOffset local{
                pixel.x - dx * TILE_SIZE,
                pixel.y - dy * TILE_SIZE,
            };

            // For the clicked tile: creature, items, spawn fallback, ground.
            // For neighbour tiles: items only (creatures may bleed too; see phase 5).
            SelectionHit hit = FindHitOnTile(tile, storage, local, dx == 0 && dy == 0);
            if (hit.kind != SelectionHitKind::None)
                return hit;
        }
    }

    return {};
}
```

`FindHitOnTile` returns `None` for no hit. Do not return a `Ground` entry with a null item; the controller treats `None` as "clear selection".

### 8.2 Item alpha test

```cpp
bool HitTestItem(const Item& item, const Tile& tile,
                 const Position& tile_pos,
                 int pixel_x, int pixel_y,
                 int elevation) {
    ItemDefinitionView def = item.getDefinition();
    GameSprite* sprite = item.getSprite();
    if (!sprite) return BoxHit(pixel_x, pixel_y);
    if (!def) return BoxHit(pixel_x, pixel_y);

    SpritePatterns patterns = PatternCalculator::Calculate(
        sprite, def, &item, &tile, tile_pos, g_gui.gfx.getElapsedTime());

    auto metrics = sprite->getPlainLayoutMetrics(
        patterns.subtype.value_or(-1),
        patterns.x, patterns.y, patterns.z, patterns.frame);

    auto [offset_x, offset_y] = sprite->getDrawOffset();

    int x_offset = 0;
    for (int cx = 0; cx < metrics.num_columns; ++cx) {
        int y_offset = 0;
        for (int cy = 0; cy < metrics.num_rows; ++cy) {
            int part_x = -elevation - offset_x - x_offset;
            int part_y = -elevation - offset_y - y_offset;

            if (pixel_x >= part_x &&
                pixel_x < part_x + metrics.column_widths[cx] &&
                pixel_y >= part_y &&
                pixel_y < part_y + metrics.row_heights[cy]) {

                for (int layer = 0; layer < sprite->layers; ++layer) {
                    size_t index = sprite->getIndex(
                        cx, cy, layer,
                        patterns.x, patterns.y, patterns.z, patterns.frame);

                    if (SpritePixelOpaque(*sprite, index,
                                          pixel_x - part_x,
                                          pixel_y - part_y)) {
                        return true;
                    }
                }
            }

            y_offset += metrics.row_heights[cy];
        }
        x_offset += metrics.column_widths[cx];
    }

    return false;
}
```

Important: the `elevation` value passed here is the running elevation of **earlier items in the item pass**, not including terrain/borders. Build the list once per tile:

```cpp
int elevation = 0;
for (const auto& item : tile.items) {
    // record (item, elevation)
    if (GameSprite* s = item->getSprite())
        elevation += s->draw_height;
}
```

Then test in visual order. For a first version, walking `tile.items` backwards while tracking elevation is possible, but the sublayer order still needs to be applied.

### 8.3 Sprite alpha

```cpp
bool SpritePixelOpaque(const GameSprite& sprite, size_t index, int x, int y) {
    if (index >= sprite.spriteList.size() || !sprite.spriteList[index])
        return false;

    Image* image = sprite.spriteList[index];
    auto rgba = image->getRGBAData();
    if (!rgba) return false;

    ImageDimensions dims = image->getDimensions();
    if (x < 0 || y < 0 || x >= dims.width || y >= dims.height)
        return false;

    size_t offset = (static_cast<size_t>(y) * dims.width + x) * 4;
    return rgba[offset + 3] > kPixelSelectionAlphaThreshold;
}
```

`Image::getRGBAData()` is virtual; `NormalImage` implements it by reading `SpriteArchive`. If the sprite is not loaded, `getRGBAData()` can still read it from the archive. If it returns null, fall back to a box hit rather than silently making the item unselectable.

### 8.4 Alpha cache

Do **not** add a cache in the first commit. Measure first:

- typical click: 1–5 sprite reads;
- large composite item: up to `width * height * layers`;
- clicks are infrequent, so this is unlikely to matter.

If measurement shows unacceptable latency on protobuf sheets, add a bounded LRU alpha-mask cache in `SelectionPicker` (not in `GameSprite` or `Tile`) with a documented memory cap. Do not put per-click state on `Tile`.

## 9. Test plan

### 9.1 Manual map

Create a test map (not committed unless the team wants it under `tests/`) with:

- grass ground;
- a 1x1 transparent-edged item (flower/decoration) over ground;
- a 2x2 table or tree trunk over multiple tiles;
- a wall/border item;
- a creature;
- a spawn;
- a stack of items with elevation (e.g. tall plant + small item);
- a top-effect item (roof) over a creature/item.

### 9.2 Cases

| Case | Expected |
| --- | --- |
| Standard mode, click item | Top item selected (old behavior). |
| Pixel mode, click opaque pixel of top item | Top item selected. |
| Pixel mode, click transparent pixel of top item over a lower item | Lower visible item selected. |
| Pixel mode, click transparent pixel of ground | Nothing selected / selection cleared. |
| Pixel mode, click left/up part of a 2x2 item stored on a neighbour tile | Multi-tile item selected and its storage tile marked selected. |
| Pixel mode, click a different item on an already-selected tile | That item is selected, not merely a drag. |
| Pixel mode, drag a selected multi-tile item | Moves via `moveSelection` and remains undoable. |
| Pixel mode, Ctrl+click an item | Toggles exactly that item. |
| Pixel mode, Shift+drag | Whole-tile box selection unchanged. |
| Pixel mode, right-click | Context selection uses the exact hit. |
| Pixel mode, zoom 0.5x / 1x / 2x | Picks the same logical pixel; boundary off-by-one is acceptable but stable. |
| High-DPI display | Pixel offset remains correct (content scale factor applied). |
| Spawn/creature visible on tile | Standard unchanged; pixel mode follows the documented priority. |
| `BORDER_IS_GROUND` on/off | Border pick selects border or ground per existing behavior. |
| Undo after selection | Undo restores the previous selection state; map content unchanged. |
| Save/load | Map content and OTBM unchanged; selection is not persisted. |

### 9.3 Automated tests

The repository has standalone test sources under `tests/`, compiled manually rather than through the main CMake target.

Implemented: `tests/test_pixel_perfect_selection.cpp`

It covers:

- `MainMapViewMath::GetFloorTileOffset`;
- `ScreenToMapPixel` at zoom 0.5, 1.0, and 2.0;
- content-scale conversion (1.25);
- scroll offsets and floor offsets;
- consistency between `ScreenToMap` and `ScreenToMapPixel` over a grid of samples;
- composite sprite part selection (`FindSelectionSpritePart`) for 1x1, 2x2, variable-width, negative-origin, and out-of-bounds coordinates.

Build and run it with the MSVC environment:

```bat
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cl /nologo /std:c++20 /EHsc /MD /I source tests\test_pixel_perfect_selection.cpp source\rendering\core\map_view_math.cpp /Fe:build-ninja\test_pixel_perfect_selection.exe
build-ninja\test_pixel_perfect_selection.exe
```

Alpha/outfit hit testing is still best verified manually with a real client data set; the automated test covers the geometry that maps a click to a sprite part and local pixel.

### 9.4 Build / verification commands

```bat
cmd /c build_ninja.bat
```

Manual smoke test:

1. Open a map with a known transparent item.
2. Toggle `Selection > Selection Method > Pixel Perfect Selection`.
3. Verify the selection tint follows the exact item.
4. Restart the editor and verify the setting persisted.
5. Switch back to Standard and verify the old behavior returns.
6. Copy/paste and undo after pixel selection.

## 10. Risks and open questions

### 10.1 Sprite alpha availability

`GameSprite::spriteList` is populated with `NormalImage` objects by `GraphicsAssembler`, and `NormalImage::getRGBAData()` reads from `SpriteArchive`. This is verified in source, but not tested with every client format. Risk: a client may have a sprite path where `getRGBAData()` returns null. Fall back to a 32x32 box so items remain selectable.

### 10.2 Performance

Reading RGBA may decode a full sprite from disk (or a protobuf sheet). A click can test several sprites, especially for a tall stack. This is per-click, not per-frame, but it can still be noticeable on slow storage. Measure before adding a cache. Do not hide the cost in the render path.

### 10.3 Render-order fidelity

Redux's depth model is more complex than the ImGui editor's. If the picker uses a simple reverse-vector order, it will disagree with the screen for:

- `TopEffect` items;
- items with `AlwaysOnBottom`;
- borders;
- elevation stacks beyond depth 3.

Borders are drawn in the terrain pass (`RenderStaticTerrain`), not the item pass, and can inherit the ground's accumulated `draw_height`; this is rare (most ground has no elevation) but should be checked if a map uses elevated terrain tiles with borders.

The plan's full sublayer order is the correct target. If implementation time is tight, ship items + ground first behind the setting and document the temporary limitation; do not claim parity until the sublayer order is implemented.

### 10.4 Multi-tile range

The radius-4 scan is a deliberate parity choice, but Redux allows up to `MAX_SPRITE_PARTS` (16) parts. If a user has an item larger than the scan range, its far corner will not be pickable. Options:

- keep 4 and document;
- compute the maximum item footprint at data load and use that (more work);
- make the range a constant that can be raised later.

Do not make it a user-facing setting without a clear need.

### 10.5 Spawn priority

The ImGui editor ignores spawns in pixel mode. Redux currently prioritizes spawns. The recommended behavior (items/creatures first, spawn fallback) is a deliberate divergence. Confirm with the user before implementation if spawn selection is important on their maps.

### 10.6 Creature hit testing

Outfits, mounts, addons, directions, and animation frames make creature hit testing significantly more code than item hit testing. The implementation now covers body addons and mounts and falls back to a 32x32 box if the outfit sprite or its alpha data is unavailable. The fallback is intentional: a creature must remain selectable even when a client format does not expose the expected outfit sprites.

### 10.7 Selected-tile drag semantics

Changing the drag gate from `tile->isSelected()` to `IsHitSelected(hit)` is a behavior change in pixel mode only. It fixes the "click different item on selected tile" case, but it also means clicking empty ground on a selected tile may start a new selection instead of dragging. Define the rule explicitly:

- if the pixel hit is already selected, drag;
- if the pixel hit is not selected, select it (clear unless Ctrl is held);
- if there is no hit, clear.

Standard mode keeps the old behavior.

### 10.8 Floor scope

The floor-scope menu affects **box selection**, not click picking. Pixel-perfect click picking should use the clicked floor. If the click resolves to a selected lower-floor tile via `FindSelectedTileUnderCursor`, keep the existing drag behavior. Do not mix floor-scope logic into the pixel picker in the first version.

### 10.9 `BORDER_IS_GROUND`

`Selection::add(tile, item)` has a special case: if `BORDER_IS_GROUND` is on and the picked item is a border, it selects the ground group instead. Pixel mode should not bypass that. Verify both settings during testing.

### 10.10 Static and animated item passes

Redux renders static items in one pass (`RenderStaticItems`) and animated items in a second pass (`RenderAnimatedItems`), each with its own elevation accumulator. The first implementation can treat all items in one ordered pass because animated selectable items are normally 1x1 (water is ground, not a common item). If a map has a tall animated item, picking may disagree by one elevation step. Document this simplification or add the two-pass split later; do not silently claim exact parity for that case.

## 11. Commit plan

Create a branch from `master`:

```text
feature/pixel-perfect-selection
```

Suggested atomic commits:

1. `feat(selection): add standard/pixel-perfect selection setting and menu`
   - config key, menu XML, action IDs, handler, icons.
2. `feat(selection): plumb sub-tile pixel offset from map canvas`
   - `ScreenToMapPixel`, `MapCanvas` overload, `SelectionController` parameter/cache.
3. `feat(selection): add standard selection picker`
   - extract current spawn/creature/top-item logic; no behavior change.
4. `feat(selection): add pixel-perfect item picker`
   - new `SelectionPicker`, sprite alpha test, multi-tile, borders, ground.
5. `feat(selection): use pixel picker in selection controller`
   - wire click/right-click/release; fix selected-tile drag gate.
6. Creature pixel hit testing is included in `selection_picker.cpp`
   - body/addon/mount alpha tests with a 32x32 fallback.
7. `test(selection): add geometry tests for pixel-perfect picking`
   - coordinate, scroll/zoom/scale, and composite-part tests; manual map checklist for alpha/outfits.

Keep the setting/menu commit separate from behavior so the feature can be disabled by leaving the default `false`.

## 12. Definition of done

- [ ] `Selection > Selection Method` has working Standard and Pixel Perfect radio items.
- [ ] The choice persists across restart (`PIXEL_PERFECT_SELECTION`).
- [ ] Standard mode behavior is byte-for-byte the same as before.
- [ ] Pixel mode selects the visible entity under the cursor for:
  - 1x1 items;
  - multi-tile items (storage tile returned correctly);
  - transparent item edges;
  - borders and ground;
  - stacked/elevated items;
  - creatures, including body addons and mounts (with a documented fallback).
- [ ] Clicking a different item on an already-selected tile selects that item.
- [ ] Ctrl+click toggles the exact picked entity.
- [ ] Right-click context selection uses the exact picked entity.
- [ ] Box selection, lasso, copy/cut/paste, move selection, and undo are unchanged.
- [ ] `cmd /c build_ninja.bat` passes.
- [ ] No OTBM/serialization changes.
- [ ] No new per-frame allocations in the render path.
- [ ] The documentation states which parts were verified and which were not.
