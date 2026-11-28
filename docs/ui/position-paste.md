# Position Formatting & XYZ Paste Pattern

## 1. Pattern & Key Files
- `map/position_format.{h,cpp}` — pure Copy Position formatting (`FormatPosition`, `FormatPositionRange`) and strict single-position/range parsing (`ParsePosition`, `PositionBounds`).
- `util/number_input.h` — stateless numeric field clamping/fallback (`ResolveNumberInput`), rejecting non-integer pastes instead of concatenating digits.
- `ui/position_paste.{h,cpp}` — `BindPositionPaste(x, y, z, bounds, on_pasted)` binds `wxEVT_TEXT_PASTE` across three `NumberTextCtrl` fields so any Copy Position format fills X, Y, Z at once.

## 2. When to Use It
Do:
- Use `NumberTextCtrl` (plain `wxTextCtrl`, not `wxSpinCtrl`) and `BindPositionPaste` for every XYZ coordinate triplet so native `WM_PASTE` / `wxEVT_TEXT_PASTE` is delivered on wxMSW, wxGTK, and wxOSX.
- Call `CheckRange()` (via `PositionCtrl::CheckRange()` or directly on each `NumberTextCtrl`) before `GetIntValue()` on dialog `OK` or toolbar actions.
Don't:
- Don't capture raw `this` in `BindPositionPaste` or child field lambdas without unbinding in the parent's destructor (`wxWindowBase::DestroyChildren()` emits `wxEVT_KILL_FOCUS` after the derived destructor completes); use `wxWeakRef` and unbind member handlers.
- Don't call `current_map` inside panel paste bounds lambdas without checking `g_gui.GetCurrentEditor()` and `current_map == &editor->map`.

## 3. Minimal Usage
```cpp
const wxWeakRef<TeleportPropertyPanel> self(this);
BindPositionPaste(x_field, y_field, z_field, [self] {
	const Editor* editor = g_gui.GetCurrentEditor();
	return (self && editor && self->current_map == &editor->map)
		? PositionBounds { editor->map.getWidth(), editor->map.getHeight() }
		: PositionBounds {};
});
```

## 4. Invariants & Pitfalls
- `PositionBounds::contains` enforces tile indices `0 <= x < width`, `0 <= y < height`, `0 <= z <= MAP_MAX_LAYER`; for copied ranges, both `from` and `to` corners must satisfy `bounds`.
- `ResolveNumberInput` checks `minval > maxval` before `std::clamp` and clamps the fallback `lastval` to `[minval, maxval]`.
- `NumberTextCtrl::TryBefore` skips `wxTextValidator` for `wxEVT_TEXT_PASTE` so `BindPositionPaste` receives the event first, and `MainMenuBar::OnPaste` forwards `Ctrl+V` to any focused `NumberTextCtrl`.

## Keyboard Flow
- Enter confirms an XYZ dialog: call `SetDefault()` on its OK button (Go To Position, Edit Town, Item Properties). With `wxTE_PROCESS_ENTER`, `NumberTextCtrl::OnTextEnter` runs `CheckRange()` and then `Skip()`s, so the default button still fires; without the flag Enter goes to the default button directly.
- Don't consume `wxEVT_TEXT_ENTER` in a shared field control: a handler that doesn't `Skip()` silently disables Enter-to-OK in every dialog using it.
- The OK handler must still call `CheckRange()` itself: Enter can close the dialog before the field loses focus.
- Tab follows child creation order: create X, Y, Z before extra buttons (e.g. Edit Town's Pick from Map) and before OK/Cancel. `wxAuiToolBar` has no Tab traversal, so `PositionToolBar::OnKeyUp` moves focus by hand.
