# Fountains in xLights — model, effect, rendering and sequencing

This is a private addition to xLights (not upstream). It covers the **DMX Fountain model**, the
**Fountains effect**, the **value curve changes** they rely on (Bézier points, colour keyframes, Graph
Max), and the rules for **sequencing a fountain**. Read the whole file before writing or changing fountain
sequences.

Repo rules for this checkout: **don't build from the command line** (the user builds in Xcode) and
**don't write unit tests** (private repo).

---

## 1. Where everything lives

| Piece | Files |
|---|---|
| Model | `src-core/models/DMX/DmxFountain.h/.cpp` |
| Water physics (shared with the Fountain Studio web app) | `src-core/models/DMX/FountainPhysics.h/.cpp` |
| Model properties UI | `src-ui-wx/modelproperties/adapters/DmxFountainPropertyAdapter.cpp` |
| Model save/load | `src-core/XmlSerializer/BaseSerializingVisitor.cpp` (`Visit(const DmxFountain&)`), `XmlDeserializingModelFactory.cpp` (`DeserializeDmxFountain`) |
| Model registration | `DisplayAsType.h/.cpp` (`DmxFountain`), `XmlNodeKeys.h` (`DmxFountainType = "DmxFountain"`), `BaseObjectVisitor.h`, `ModelManager.cpp` (`CreateDefaultModel("DmxFountain")`), `Model.cpp` (`DetermineClass`) |
| Effect | `src-core/effects/FountainsEffect.h/.cpp`, `resources/effectmetadata/Fountains.json`, `include/Fountains.xpm`, `EffectManager` (`eff_FOUNTAINS`), `EffectPanelManager.cpp` (`RegisterJson`) |
| Render routing | `src-core/render/RenderBuffer.cpp` (fountain is **not** a `dmx_buffer`) |
| Cell grouping | `Model::GetNodeBufferGroup` / `Model::SingleLineCells` (`Model.h/.cpp`), `SubModel.cpp` (`initDefaultBuffer`) |
| Value curve core | `src-core/render/ValueCurve.h/.cpp` |
| Value curve UI | `src-ui-wx/shared/dialogs/ValueCurveDialog.h/.cpp`, `src-ui-wx/shared/controls/ValueCurveButton.h/.cpp`, `src-ui-wx/effectpanels/JsonEffectPanel.cpp`, `EffectPanelUtils.cpp` |
| Effect bar in the sequencer grid | `FountainsEffect::DrawEffectBackground`; `RenderableEffect::BackgroundNeedsPalette()`; `src-ui-wx/sequencer/EffectsGrid.cpp` (`DrawEffectBackground`) |
| iPad | `src-iPad/Bridge/XLSequenceDocument.mm` (model properties `BuildDmxFountainProps`, effect bar palette check), `LayoutEditorView.swift` ("DMX Fountain") |
| Reference visualizer | `macOS/fountain-studio` (Vue/TS; `physics.ts`, `collisions.ts`) — an Xcode **folder reference**, not a synced group |

---

## 2. The DMX Fountain model

A pool with N pumps. Each pump has **one DMX channel** for water height, and optionally an **RGB light
ring** under it (3 channels).

### Properties (`.xlights_rgbeffects.xml` attributes, on the `DmxFountain` model node)

| UI label | XML attribute | Default | Meaning |
|---|---|---|---|
| Pumps | `FountainPumps` | 8 | 1–64 |
| Arrangement | `FountainArrangement` | `Ring` | `Ring`, `RingCenter`, `Line`, `Square`, `Grid` (UI shows "Ring with Center") |
| Number Clockwise | `FountainClockwise` | 0 | Numbering direction seen from above |
| Nozzle Spread % | `FountainNozzleSpread` | 80 | How far out the nozzles sit, as a % of the pool |
| Max Water Height (ft) | `FountainMaxHeight` | 15 | A pump at 100% throws water this high |
| Center Max Water Height (ft) | `FountainCenterMaxHeight` | 0 | Ring with Center only. **0 = follows Max Water Height**; setting it equal to Max goes back to following |
| Pump Start Channel | `FountainPumpChannel` | 1 | First pump channel (1-based within the model) |
| RGB Lights | `FountainLights` | 1 | Whether each pump has an RGB light |
| Light Start Channel | `FountainLightChannel` | pumps+1 | First light channel |
| Spray / Show Pool / Droplets per Pump | `FountainSpray`, `FountainPool`, `FountainDroplets` | 1 / 1 / 700 | Preview only |

A new model gets 8 pumps, pump channels 1–8 and lights 9–32 (`AssignSequentialChannels`). Its string type
is `Single Color White`.

### Channel layout

- Pump *i* (0-based): channel `FountainPumpChannel + i`. Value 0–255 = 0–100% of that pump's max height.
  Height is linear in the value, not in pump speed.
- Light *i*: channels `FountainLightChannel + 3i` (R), `+1` (G), `+2` (B).
- Node names: `Pump 1`, `Pump 1 Red`, `Pump 1 Green`, `Pump 1 Blue`, ….
- **Ring with Center: the center pump is always the LAST pump.**

### Pump numbering and positions (`GetPumpLayout`)

- **Ring / Ring with Center:** pump 1 at the front, then around, counter-clockwise seen from above unless
  Number Clockwise is set.
- **Line:** left to right.
- **Square:** starts at the middle of the front edge and walks the perimeter, evenly spaced.
- **Grid:** front row first, left to right, rows toward the back; a short last row is centred.

### Node classes (this matters for effects)

- **Light channels** are `NodeClassRed` / `NodeClassGreen` / `NodeClassBlue`. Each takes only its own
  component of the effect colour, so a light's three channels together are a true RGB pixel.
- **Pump channels** are `NodeClassWhite`. Channel value = **min(r, g, b)** of the colour written to it.
  A pump only runs on white or grey. **Pure red, green or blue on a pump channel is 0 (off).**

### Automatic submodels (`SyncSubModels`, rebuilt on every model init)

| Always | With a center pump (Ring with Center, ≥ 2 pumps) |
|---|---|
| `Pumps` (all pump channels) | `Perimeter` (each perimeter pump followed by its light) |
| `Lights` (all light channels, if RGB Lights) | `Center` (center pump + its light) |
| | `Perimeter Pumps`, `Center Pump` |
| | `Perimeter Lights`, `Center Light` (if RGB Lights) |

They are range submodels with the `Default` buffer style. A changed layout rebuilds them in place, keeping
their object id and aliases. Submodels the user made themselves are left alone.

### Buffer styles (fountain model)

| Style | Layout | Notes |
|---|---|---|
| **Default** | One cell per **channel** (the DMX strip) | What the DMX effect writes by channel number. Pumps and lights are separate cells. |
| **Single Line** | One cell per **pump** and one per **light** (a light's R, G and B share a cell; a pump has its own beside it) | Best for chases and "one by one" effects. |
| **Per Preview** | Cells at the nodes' 2D/3D preview positions for the selected camera | Every channel of a pump sits on the pool floor under that pump. A camera looking down (top view) lets Fan, Spirals, etc. paint by position. **The pump and its light share a spot, and so a cell.** |

On a submodel with the Default style, the cells are grouped the same way as Single Line. Consecutive nodes
of the same light share a cell (`GetNodeBufferGroup`); a pump gets its own.

### Preview rendering (the water)

- **Layout tab:** every pump drawn at full height, so you can see and size the box. The box width is the
  pool (or line length); the box height is the tallest pump at 100%. Depth follows width.
- **Sequencer preview:** the water comes from the **rendered sequence data**
  (`RenderContext::GetRenderedSequenceData`), not live effect output. **Effects must be rendered** to see
  water move. The model replays each pump channel's history at 50 Hz through the physics, so water already
  in the air keeps flying after a pump drops.
- **Light ring:** drawn under each nozzle in the light's colour, **black when the light is off**, with a
  glow when lit.

### Physics (identical to fountain-studio `physics.ts` / `collisions.ts`)

- Water is a projectile with **quadratic air drag** (k = 0.12 per metre). It needs more speed to reach a
  height than without air, gets there a little sooner, and falls slower.
- **Pump delay:** one sample (20 ms).
- **Spin-up:** launch speeds up to 6.5 m/s are reached at once; above that the speed builds at 20 m/s per
  second.
- **Lowering or shutting off is immediate.** Water in the air flies on.
- **Spray:** falling water meeting rising water of the same pump explodes (`CollisionTracker`). The
  harder the closing speed, the wider the burst.

Timing per target height, from a command at 0 (for sequencing on the beat):

| Height | Launch speed | Spin-up | Command → top of water | Total air time |
|---|---|---|---|---|
| 3 ft | 4.5 m/s | 0 | ~0.44 s | ~0.9 s |
| 5 ft | 6.0 m/s | 0 | ~0.56 s | ~1.1 s |
| 10 ft | 9.4 m/s | 0.14 s | ~0.9 s | ~1.6 s |
| 15 ft | 12.8 m/s | 0.31 s | ~1.2 s | ~2.0 s |
| 20 ft | 16.5 m/s | 0.5 s | ~1.5 s | ~2.3 s |
| 30 ft | 25.5 m/s | 0.95 s | ~2.1 s | ~2.8 s |

So **water peaks well after the command**. For a jet to hit its top on a beat, start the rise about that
long before the beat. Dropping a pump doesn't make the water vanish; it falls for about half the air time.
Quick on/off pulses shorter than the rise time never reach full height. A sudden drop under a tall column
makes spray where the falling water meets the new, lower stream.

---

## 3. How effects reach the fountain (render pipeline)

- `RenderBuffer.cpp`: `dmx_buffer = IsDmxDisplayType(...) && display != DmxFountain`. The fountain is
  **not** routed through `SetPixelDMXModel`, so **every standard effect works on it** (On, Color Wash,
  Bars, Fan, Spirals, Rainbow…), not just the DMX effect.
- `GetColors` writes every node; each node class then reduces the colour as above (lights per component,
  pumps by min). For example:
  - **White On** on the whole model: all pumps 100%, all lights white.
  - **Red On** on the whole model: pumps 0 (min = 0), lights red.
  - **Grey 50% On** on `Pumps`: every pump at 50% height.
- The **DMX effect** works on the Default buffer, by channel number.
- **Layers mix normally.** A colour effect on `Lights` plus a level effect on `Pumps` (or on another layer)
  combine as in any xLights model. With Normal mixing, black is transparent, so an off pump (black) on an
  upper layer lets the layer below show.

---

## 4. The Fountains effect

One effect that drives **pump levels and light colours from a single value curve**. The curve's value is the
pump level (0–100%). The curve's **colour keyframes** are the light colour over time.

### Settings (as stored on the effect in the `.xsq`, `E_` prefix)

| Key | Values | Default | Meaning |
|---|---|---|---|
| `E_CHOICE_Fountains_Type` | `All`, `Chase` | `All` | `All`: every pump plays the curve together. `Chase`: pumps take turns. (Old saves may say `Sequential` = Chase with 0% overlap.) |
| `E_VALUECURVE_Fountains_Curves` | value curve string (§5) | flat 0% | Pump level + light colour over one pass |
| `E_SLIDER_Fountains_Curves` | 0–100 | 0 | Fallback level when no curve is saved (normally unused) |
| `E_TEXTCTRL_Fountains_Cycles` | float 0–100 | 1.0 | Passes over the effect's duration |
| `E_CHOICE_Fountains_Direction` | `Forward`, `Reverse`, `Forward Bounce`, `Reverse Bounce` | `Forward` | Chase order |
| `E_SLIDER_Fountains_PumpOffset` | 0–64 | 0 | Chase starts this many pumps along (wraps) |
| `E_SLIDER_Fountains_Overlap` | 0–95 | 0 | Chase: how much each turn overlaps the next (%) |

Direction, Pump Offset and Overlap are disabled in the panel when Type = All. The panel shows **Curves** as
an "Edit Curves..." button: the curve is **always active**, and its dialog opens in colour mode (§6). Effect
string example (one line in the `<EffectDB>` entry, comma separated):

```
E_CHOICE_Fountains_Type=Chase,E_TEXTCTRL_Fountains_Cycles=2.0,E_CHOICE_Fountains_Direction=Forward,E_SLIDER_Fountains_PumpOffset=0,E_SLIDER_Fountains_Overlap=50,E_VALUECURVE_Fountains_Curves=Active=TRUE|Id=ID_VALUECURVE_Fountains_Curves|Type=Custom|Min=0.00|Max=100.00|Colors=0.00:0000FF:B;0.50:00FFFF:B;1.00:0000FF:B|Values=0.00:0.00;0.30:1.00:E;0.70:1.00;1.00:0.00:E|
```

The effect doesn't use the colour palette; its colours come only from the curve's colour keys.

### Units: what takes a turn

The effect looks at the buffer's nodes:

- **On a fountain (or its submodels):** each pump with its light is one **unit**, numbered in buffer order.
  Pump channels get the **level** (grey `level × 2.55`); light channels get the **colour**. Other channels
  are untouched.
- **On any other model:** every node is a unit and takes the colour (one pixel at a time for Chase). The
  level isn't used.

So the effect on `Pumps` only drives water. On `Lights` it only drives colour. On the whole model or
`Perimeter` it drives both.

### Timing (`UnitTimes` in `FountainsEffect.cpp`)

Let `position` = 0→1 through the effect and `t = position × Cycles`.

- **All:** every unit plays the curve at `frac(t)`. The last frame is the end of the last pass, not a
  restart.
- **Chase:** the turn order comes from Direction (`TurnOrder`):
  - **Forward:** 0, 1, …, n−1.
  - **Reverse:** n−1 … 0.
  - **Bounces** go there and back without repeating the ends: Forward Bounce on 4 pumps is 0 1 2 3 2 1.

  Each turn's unit is `(order[turn] + PumpOffset) % units`. The turns are laid out like this:
  - **Steps:** one pass (cycle) is `turns` equal steps, and turn *k* starts at `k/turns` into each pass.
  - **Turn length:** each turn lasts `len = (1/turns) / (1 − Overlap)` of a pass. At **0% overlap**, one
    pump at a time, back to back. At **50%**, each turn lasts 2 steps, so two pumps are always running.
  - **Wrapping:** turns run on around the loop. Pump 1 starts its next turn on schedule while the last
    pumps are still finishing, so a continuous **twirl** never pauses. The first pass starts clean (no
    leftover turns from before the effect). A turn still running at the effect's end is cut off.
  - **Curve position:** during a turn, the unit is at `(t − turnStart) / len` through the curve. Outside
    its turns, a unit's pump is 0 and its light black.
  - **Bounces and long turns:** if a unit has several turns running (bounces, or `len` longer than a pass
    with high overlap and few pumps), the most recent one wins.
  - **Speed:** one lap of the pumps per Cycle, so Cycles sets the twirl speed.

### Order of writes and shared cells

Pumps are written first, then lights. If a pump and its light share a cell (**Per Preview**), the light's
colour wins, and the pump then gets min(r,g,b) of that colour. **Use the Default or Single Line buffer for
Fountains on a fountain** (the Default buffer is fine: every channel is its own cell).

### The effect bar in the sequencer grid (`DrawEffectBackground`)

It shows **the first pump's** timeline only (for Chase, pump 1's turns), looped by Cycles exactly like the
render:

1. **Colour keys and the row has lights:** the colour gradient over time. If the row also has pumps,
   there's a **black line with a white edge** showing the pump level (bottom = 0%, top = 100%). The line
   is hidden when the level is 0 throughout or the row has no pumps. On a lights-only row, the gradient's
   height follows the level when "ramps" display is on.
2. **No colour keys, or a pumps-only row (`Pumps`, `Center Pump`…):** a **filled area chart**: black
   background, white area = pump level 0–100%.

`RenderableEffect::BackgroundNeedsPalette()` (false for Fountains) stops the grid from replacing the bar with just the
icon when no palette colours are checked. The iPad grid has the same check.

---

## 5. Value curve format and evaluation

The settings string is `key=value|` pairs. These keys are new or matter here:

| Key | Format | Meaning |
|---|---|---|
| `Type` | `Custom` | Fountains is always Custom |
| `Min` / `Max` | `0.00` / `100.00` | Output range; point `y` 0–1 maps to Min–Max |
| `Values` | `x:y[;x:y…]`, optional shape suffix | Points, x and y in 0–1 |
| ↳ eased point | `x:y:E` | Smooth in and out (handles at ⅓ of each segment, flat) |
| ↳ custom Bézier | `x:y:C:inDx:inDy:outDx:outDy` | Handle offsets from the point, in 0–1 units |
| ↳ hard point | `x:y` | Straight lines into and out of it |
| `Colors` | `x:RRGGBB:B` or `:H`, `;`-separated | Colour keyframes; `B` = blend into this key, `H` = hard cut at this key |
| `WRAP` | `WRAP=TRUE` | For colours: the gradient loops from the last key back to the first |
| `VMax` | float | "Graph Max": the dialog's visible top (a zoom). It doesn't change output |

How a segment is shaped (`SegmentValue`): a cubic Bézier between neighbouring points, using the left point's
**out** handle and the right point's **in** handle. Handle x is kept inside the segment, so the curve stays
a function of x. A hard point's handle is the point itself, so two hard points give a straight line.

Colour at x (`GetColorAt`):

- **No keys:** black. **One key:** that colour everywhere, whatever its blend setting.
- **Between keys:** blend linearly into a `B` key; hold the previous colour before an `H` key.
- **Before the first key:**
  - WRAP on and the first key `B`: blends in from the last key, as a loop.
  - WRAP on and the first key `H`: holds the last colour.
  - WRAP off: the first key's colour, solid from the start (Blend or Hard).
- **After the last key:** holds the last colour, or loops toward the first key when WRAP is on and the first
  key is `B`.

For a seamless colour loop across Cycles, turn WRAP on and make the first key Blend. The level curve
doesn't loop smoothly on its own, so make its first and last y equal if the water shouldn't jump between
passes.

---

## 6. Value curve dialog changes (desktop UI, affect all effects unless noted)

- **Graph:** grey outline, dotted quarter lines and labels at 0/25/50/75/100% of the visible max. The left
  margin sizes itself to the labels.
- **Dragging a point:** a light-blue dotted guide line and a floating value label. Shift, Ctrl or Cmd snaps
  to multiples of 5. A point can be dragged past its neighbours (it is re-sorted).
- **Right-click a point:** Hard Point, Ease or Custom. Custom shows Bézier handles. Shift, Ctrl or Cmd while
  dragging a handle mirrors the opposite handle, limited to the segment.
- **Double-click a point:** edit its X (0–100%) and value.
- **Live preview:** the render refreshes about 150 ms after any change, but not while the mouse button is
  down. Cancel restores the curve and re-renders.
- **Graph Max:** a slider and text box that zooms the y axis, saved as `VMax`.
- **Cycles > 1:** "Edit Points" / "Cycles Preview" toggle (points are only editable in Edit Points).
- **Colour mode** (only when the caller allows colour; the Fountains Curves button does):
  - The curve is forced to Custom, range 0–100, Cycles 1, no timing track.
  - The dialog shows only the graph, a **colour bar**, Wrap Values, OK and Cancel.
- **Colour bar:**
  - Click to add a key (opens the colour picker; Cancel removes it). Drag to move, double-click to
    recolour, right-click for Blend/Hard, Delete/Backspace to remove.
  - Circle marker = blend key, square = hard key, light-blue ring = selected.
- **`valueCurveOnly` (effect JSON):** the slider and text box are replaced by an "Edit <label>..." button,
  and the curve is always active. A new curve is flat at 0%, with no colour keys, wrap off.

Effect metadata flags added to `_schema.json`: `valueCurveColor`, `valueCurveOnly`.

---

## 7. Sequencing guide

### Choosing the target

| Goal | Put the effect on | Buffer |
|---|---|---|
| Water and light together, one curve | the fountain model, or `Perimeter` / `Center` | Default or Single Line |
| Water only | `Pumps`, `Perimeter Pumps`, `Center Pump` | Default / Single Line |
| Colour only (any standard effect) | `Lights`, `Perimeter Lights`, `Center Light` | Single Line for one-by-one; **Per Preview with a top-down camera** for spatial effects (Fan, Spirals, Wave…) |
| Spatial water patterns | `Pumps` | Per Preview (top camera); remember pumps read **min(r,g,b)**, so use white/grey colours |
| Exact channel values | the model | Default + DMX effect |

### Fountains effect recipes

- **All jets pulse with the music:** Type All, Cycles = beats in the effect. Curve: 0 → peak → 0, eased.
  Start the effect about the rise time early (§2 table) so the water peaks on the beat.
- **Twirl / spin:** Type Chase, Forward, Overlap 50–80%, Cycles = laps. A smooth hump curve (eased 0 → 100
  → 0) makes a travelling wave around a ring. With wrapping, the motion never pauses between laps. Reverse
  spins the other way; Pump Offset rotates the starting pump.
- **One at a time:** Chase, Overlap 0%. Each pump gets an equal slice.
- **Ping-pong along a line:** Chase, Forward Bounce.
- **Colour chase with the water:** add colour keys. Each pump's light follows the same colour timeline
  during its turn and is black between turns.
- **Center vs perimeter:** put separate effects on `Center` and `Perimeter`, e.g. a slow tall center
  column with a fast perimeter twirl. Center Max Water Height can be set separately.

### Gotchas

- A pump runs only on **white/grey**: min(r,g,b). Coloured effects on pump channels can be 0.
- Water lags the command (delay, spin-up, flight). Sequence rises early. Quick pulses don't reach full
  height.
- The preview shows water only from **rendered** data. Render after changes.
- Fountains on a **Per Preview** buffer of a model containing both pumps and lights gives the pump the
  light's colour (shared cell). Use Default or Single Line.
- The Fountains curve defaults to **0%** (pumps off) until you add points.
- Cycles also repeats the colour keys. Use WRAP with a Blend first key for a seamless colour loop.
