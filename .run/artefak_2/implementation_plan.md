# Layer Management Implementation Plan

Based on the prompt, the application needs a visual "Layers" list on the right side, defined by the order in `layer.conf`. We will track valid and "corrupt" presets, and allow moving layers up/down to reorder them in the list.

## 1. Feature Specifications
- **Layer Panel (Right Side UI)**: The window width will be extended (e.g. from 400 to 650) to accommodate a Layer List on the right.
- **`layer.conf` Synchronization**: During `DiscoverPresets()`, the order of `presets` will be matched against `./soundpack/layer.conf`. Any preset folders not inside `layer.conf` will be appended to the bottom automatically. Changes to ordering will trigger an auto-save to `layer.conf`.
- **Corrupt Folders Handling**: Previously, un-parseable or malformed `config.json` folders were skipped completely. Now, they will be loaded as `PresetInfo` with an `isCorrupt = true` flag. These will still appear in the layer list but show a small red ["corrupt"] tag.
- **Reordering UI**: Next to each preset in the layer pane, we will render Up/Down mini-buttons (or just handle `+/-` keys for the active layer) to shift the layer list visually. Clicking the preset names in the list will load them (acting like UP/DOWN keys but with direct mouse selection).

## 2. Updated Architecture Components

### `PresetManager.h` & `PresetManager.cpp`
- **Data Model**:
  - `PresetInfo` will have `bool isCorrupt;` and store its internal folder name (like `preset1`).
- **Methods**:
  - `DiscoverPresets()` will read `layer.conf` first, load all folders, rank them by rank in `layer.conf`, and inject missing ones.
  - `MoveLayerUp(int pos)` / `MoveLayerDown(int pos)` to shift the preset vector.
  - `SaveLayerConf()`: writes the current `PresetInfo` ordering to `layer.conf`.
- **Corrupt handling**: `json` exceptions won't abort discovering the preset; instead, they flag `isCorrupt = true`. 

### `main.cpp`
- **Window Extension**: `CreateWindowEx` width changed to accommodate the new side panel. 
- **Painting the Panel (`WM_PAINT`)**:
  - Iterate `presets`. Draw an un-selected vs selected background to visually confirm current layer.
  - Draw `Name`. If `isCorrupt`, draw red text `[corrupt]`.
  - Draw minimalist `[^]` and `[v]` buttons for moving.
- **Mouse Interactions (`WM_LBUTTONDOWN`)**:
  - Check `x/y` coordinates against the new Layer List items. If clicked a name, `LoadPreset(index)`. If clicked `[^]`, move layer up and save.

## 3. User Review Required
> [!IMPORTANT]
> The plan involves UI adjustments directly in GDI. Do you want "Move Up/Down" to be clickable buttons on the screen (UI approach), or a keyboard hotkey like `SHIFT + UP / SHIFT + DOWN` to move the currently selected layer physically? We intend to do both: clickable arrows next to the layer name alongside standard click-to-select. Let me know if that is agreeable!
