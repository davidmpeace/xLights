# Fountains in xLights: model, effect, rendering and sequencing

This is a private addition to xLights (not upstream). It covers:

- the **DMX Fountain model** and how its water behaves;
- the **Fountains effect**;
- the **value curve changes** they rely on (Bézier points, colour keyframes, Graph Max);
- **inline editing** in the sequencer grid;
- how to **sequence a fountain**: targets, timing, recipes, and the exact `.xsq` settings.

Read the whole file before writing or changing fountain sequences. §2.8 (water timing) and §8 (sequencing
guide) are the ones that decide whether a sequence looks right.

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
| Render routing | `src-core/render/RenderBuffer.cpp` (the fountain is **not** a `dmx_buffer`) |
| Cell grouping | `Model::GetNodeBufferGroup` / `Model::SingleLineCells` (`Model.h/.cpp`), `SubModel.cpp` (`initDefaultBuffer`) |
| Value curve core | `src-core/render/ValueCurve.h/.cpp` |
| Value curve UI | `src-ui-wx/shared/dialogs/ValueCurveDialog.h/.cpp`, `src-ui-wx/shared/controls/ValueCurveButton.h/.cpp`, `src-ui-wx/effectpanels/JsonEffectPanel.cpp`, `EffectPanelUtils.cpp` |
| Effect bar in the sequencer grid | `FountainsEffect::DrawEffectBackground`; `RenderableEffect::BackgroundNeedsPalette()`; `src-ui-wx/sequencer/EffectsGrid.cpp` (`DrawEffectBackground`) |
| Inline editing in the grid | `src-ui-wx/sequencer/EffectsGridInline.cpp`; `RenderableEffect::SupportsInlineEdit` and friends; `FountainsEffect::GetInlineEditCurveSpan` |
| Reference visualizer | `macOS/fountain-studio` (Vue/TS; `physics.ts`, `collisions.ts`). It's an Xcode **folder reference**, not a synced group. |

---

## 2. The DMX Fountain model

A pool with N pumps. Each pump has **one DMX channel** for its water height and, optionally, an **RGB light
ring** under it (3 channels).

### 2.1 Properties

These are attributes on the `DmxFountain` model node in `xlights_rgbeffects.xml`.

| UI label | XML attribute | Default | Range / meaning |
|---|---|---|---|
| Pumps | `FountainPumps` | 8 | 1–64 |
| Arrangement | `FountainArrangement` | `Ring` | `Ring`, `RingCenter`, `Line`, `Square`, `Grid` (the UI shows `RingCenter` as "Ring with Center") |
| Number Clockwise | `FountainClockwise` | 0 | Numbering direction, seen from above |
| Nozzle Spread % | `FountainNozzleSpread` | 80 | 0–100: how far out the nozzles sit, as a % of the pool's diameter, line length or side |
| Max Water Height (ft) | `FountainMaxHeight` | 15 | 0.5–500: a pump at 100% throws water this high |
| Center Max Water Height (ft) | `FountainCenterMaxHeight` | 0 | Ring with Center only. **0 means it follows Max Water Height**, and setting it equal to Max goes back to following. |
| Pump Start Channel | `FountainPumpChannel` | 1 | First pump channel (1-based within the model) |
| RGB Lights | `FountainLights` | 1 | Whether each pump has an RGB light |
| Light Start Channel | `FountainLightChannel` | pumps + 1 | First light channel |
| Spray | `FountainSpray` | 1 | Preview only: draw the spray where falling water hits rising water |
| Show Pool | `FountainPool` | 1 | Preview only |
| Droplets per Pump | `FountainDroplets` | 700 | Preview only, 50–5000 |

A new model gets 8 pumps, with pump channels 1–8 and lights on 9–32 (`AssignSequentialChannels`). Its
string type is `Single Color White`.

**Check Sequence** (`CheckModelSettings`) reports an error when:

- the model has fewer channels than its pumps and lights need;
- the pump channels overlap the light channels.

### 2.2 Channel layout

- **Pump *i*** (0-based) is channel `FountainPumpChannel + i`.
  - The value 0–255 means 0–100% of **that pump's** max height.
  - Height is linear in the value, not in pump speed.
- **Light *i*** is three channels: `FountainLightChannel + 3i` (red), `+1` (green), `+2` (blue).
- **Node names:** `Pump 1`, `Pump 1 Red`, `Pump 1 Green`, `Pump 1 Blue`, and so on.
- **Ring with Center: the center pump is always the LAST pump.** It can have its own max height.

### 2.3 Pump numbering and positions (`GetPumpLayout`)

"Front" is the side facing the viewer in the layout.

- **Ring / Ring with Center:**
  - Pump 1 is at the front.
  - The numbering runs counter-clockwise seen from above (front → right → back → left), or clockwise if
    Number Clockwise is set.
  - The center pump (last) is in the middle.
- **Line:** left to right.
- **Square:** starts at the middle of the front edge and walks the perimeter, evenly spaced. It goes to the
  right first unless Number Clockwise is set.
- **Grid:** as square as possible (columns = ⌈√pumps⌉). Rows run front to back, each left to right. A short
  last row is centred.
- **One pump:** in the middle.

**The chase order of the Fountains effect is this numbering.** On a ring, Forward therefore spins
counter-clockwise seen from above, or clockwise with Number Clockwise set.

### 2.4 Node classes: how a colour becomes a pump level or a light

- **Light channels** are `NodeClassRed` / `NodeClassGreen` / `NodeClassBlue`. Each takes only its own
  component of the colour written to it, so a light's three channels together are a true RGB pixel.
- **Pump channels** are `NodeClassWhite`. **The channel value is min(r, g, b)** of the colour written to it.
  - A pump only runs on white or grey: a grey at *g*% gives *g*% height.
  - **Pure red, green or blue on a pump channel is 0 (off).** So is any saturated colour.
  - Yellow (255,255,0) is also 0.

### 2.5 Default submodels (`CreateDefaultSubModels`, only when the model is made)

A new fountain starts with these submodels:

| Always | Only with a center pump (Ring with Center, ≥ 2 pumps) |
|---|---|
| `Pumps`: all pump channels | `Perimeter`: each perimeter pump followed by its light (needs RGB Lights) |
| `Lights`: all light channels (needs RGB Lights) | `Center`: the center pump and its light (needs RGB Lights) |
| | `Perimeter Pumps`, `Center Pump` |
| | `Perimeter Lights`, `Center Light` (need RGB Lights) |

- They are range submodels with the `Default` buffer style, made once when the model is created
  (`ModelManager::CreateDefaultModel`).
- **After that they are ordinary submodels.** The user can rename, change or delete them, and the changes
  stick. Nothing re-creates or updates them.
- **Changing the pumps, arrangement, channels or RGB Lights later doesn't update them.** If the layout changes,
  check their ranges (or delete them and add new ones in the Submodels dialog).
- A fountain may therefore have none of them, different ones, or renamed ones. **Check which submodels the
  model actually has before sequencing on them.**
- Any submodel works with every effect. For example, a user submodel of a few pumps can take a Fountains
  effect of its own.

### 2.6 Buffer styles (fountain model)

| Style | Cells | Use |
|---|---|---|
| **Default** | One cell per **channel**: the DMX strip, pumps and lights in channel order | The DMX effect writes by channel number. Fine for Fountains: every channel is its own cell. |
| **Single Line** | One cell per **pump** and one per **light**. A light's R, G and B share a cell; its pump has its own cell beside it. | Chases and "one by one" effects. |
| **Per Preview** | Cells at the nodes' positions as the selected camera sees them | Spatial effects. **A pump and its light share a spot, and so a cell.** |

On a submodel with the Default style, cells are grouped the way Single Line groups them: the three channels
of a light share a cell, and a pump gets its own (`GetNodeBufferGroup`).

**Per Preview on a fountain.** Every channel sits on the pool floor under its own pump (`InitModel`), so the
camera decides the layout:

- **Camera `2D` (the default) flattens the pool to the front view.** It drops depth, so the buffer is a single
  row. Pumps at the same left-right position share a cell. On an 8-pump ring, for example, pumps 1 and 5
  (front and back) merge, and so do 2 and 4, and 6 and 8.
- **For spatial patterns, use a top-down 3D viewpoint.** A saved viewpoint looking straight down maps the
  pool onto the buffer as it really is. Then Fan, Spirals, Pinwheel, Ripple, Shockwave and others paint by
  where each pump is.
- **Make that viewpoint orthographic.** Tick **Orthogonal** in the 3D layout's right-click menu before saving
  it. That avoids perspective distortion: there's no shrinking toward the edges, and a ring stays a true
  circle.
- Choose the viewpoint in the effect's buffer settings: `B_CHOICE_PerPreviewCamera=<viewpoint name>` (§7.4).

### 2.7 The preview (the water)

- **Layout tab:** every pump is drawn at full height, so you can see and size the box.
  - The box width is the pool (or line length), and its height is the tallest pump at 100%.
  - Depth follows width.
  - Lights are drawn dark.
- **Sequencer and house preview:** the water comes from the **rendered sequence data**, not live effect
  output.
  - **Effects must be rendered** to see the water move.
  - The model replays each pump channel's history at 50 Hz through the physics, so water already in the air
    keeps flying after a pump drops.
  - Paused, the water stands still. After a seek, it shows what would be in the air at that moment.
  - Without any rendered data, the pumps hold the levels their channels show now.
- **Light ring:** drawn under each nozzle in its light's colour.
  - It's black when the light is off and glows when lit.
  - The water above is tinted by its light, and spray takes the colour of the light under it.
- **Spray** needs rendered data and Spray on. Its bursts are replayed from about the last few seconds.

### 2.8 Physics (identical to fountain-studio `physics.ts` / `collisions.ts`)

Measured on a real fountain:

- **Drag:** water is a projectile with **quadratic air drag** (k = 0.12 per metre). To reach a height it has
  to leave faster than it would without air, gets there a little sooner, and falls more slowly.
- **Nozzle:** water leaves the nozzle 0.08 m above the pool.
- **Pump delay:** one sample (20 ms).
- **Spin-up:**
  - Launch speeds up to 6.5 m/s are reached at once.
  - Above that, the speed builds at 20 m/s per second.
  - Short jets respond instantly; tall jets take a while to "pump up".
- **Lowering or shutting off is immediate.** Water already in the air flies on.
- **Spray:**
  - Falling water that lands on rising water from the same pump explodes (`CollisionTracker`).
  - The harder the closing speed, the wider the burst.
  - Typical cause: a quick drop under a tall column, then raising the pump again while that water is still
    falling.

**Timing per target height, from a command at 0 (from rest).** Use this table for sequencing on the beat.

| Height | Launch speed | Spin-up | Command → water at its top | Total air time |
|---|---|---|---|---|
| 3 ft | 4.5 m/s | 0 | ~0.44 s | ~0.9 s |
| 5 ft | 6.0 m/s | 0 | ~0.56 s | ~1.1 s |
| 10 ft | 9.4 m/s | 0.14 s | ~0.9 s | ~1.6 s |
| 15 ft | 12.8 m/s | 0.31 s | ~1.2 s | ~2.0 s |
| 20 ft | 16.5 m/s | 0.5 s | ~1.5 s | ~2.3 s |
| 30 ft | 25.5 m/s | 0.95 s | ~2.1 s | ~2.8 s |

"Height" is the water's actual height: **level % × that pump's Max Water Height**. A 15 ft pump at 50% is a
7.5 ft jet.

What this means when sequencing:

- **Water peaks well after the command.** For a jet to top out on a beat, start its rise about the
  "command → top" time before the beat.
- **Dropping a pump doesn't make the water vanish.** The water falls for roughly half the air time, so a
  "cut" on a beat looks like a collapse that lands about ½ × air time later.
- **Short pulses never reach full height.** A pulse shorter than the rise time is cut off on the way up.
  Between full-height pulses, leave at least the total air time.
- **Spin-up limits fast tall motion.** Rapid big swings on a tall pump turn into a smaller, laggier wave. Use
  smaller swings or slower motion for tall jets.
- **Spray:** a sudden drop under a tall column, followed by a rise, makes spray where the falling water meets
  the new stream. Use it on purpose for a "burst", and avoid it when you want clean columns.

---

## 3. How effects reach the fountain (render pipeline)

- `RenderBuffer.cpp`: `dmx_buffer = IsDmxDisplayType(...) && display != DmxFountain`. The fountain is **not**
  routed through `SetPixelDMXModel`, so **every standard effect works on it** (On, Color Wash, Bars, Fan,
  Spirals, Ripple, Shockwave, Butterfly…), not just the DMX effect.
- Effects write colours into cells, and each node class then reduces the colour (§2.4). For example:
  - **White On** on the whole model: all pumps at 100%, all lights white.
  - **Red On** on the whole model: pumps at 0 (min = 0), lights red.
  - **Grey 50% On** on `Pumps`: every pump at 50% height.
- **The DMX effect** works on the Default buffer, by channel number. Use it for exact raw values.

### 3.1 Settings applied on top of any effect

These scale pumps and lights alike:

- **Fade in / fade out** (`T_TEXTCTRL_Fadein` / `T_TEXTCTRL_Fadeout`, seconds):
  - The layer's brightness is multiplied by the fade, so pump levels ramp and the water height ramps with
    them.
  - The fade multiplies whatever the effect is doing then: a curve at 60% fades from 60% to 0.
  - The other transition types (Wipe, Dissolve…) mask cell by cell in buffer order. On Single Line that
    sweeps pump by pump.
- **Brightness** (`C_SLIDER_Brightness`, 0–400, default 100):
  - Multiplies the colour's value, so it scales pump levels too. 200 doubles them, clipped at 100%.
  - It can take a value curve (`C_VALUECURVE_Brightness`) for a level envelope on top of any effect.
- **Hue / saturation adjustments** don't move a grey pump level (grey stays grey). They do change the
  lights.

### 3.2 Layers

Layers mix normally. The top layer (layer 1) is applied over the ones below.

- **Normal** (the default): black is transparent. An off pump (black) or dark light on an upper layer lets
  the layer below show.
- **Max** (`T_CHOICE_LayerMethod=Max`): takes the higher of the two per channel. Useful for combining two pump
  patterns, for example a slow base swell plus a chase on top. Each pump runs at the higher level.
- **Additive** adds, clipped at 100%. **Min** takes the lower.
- A colour effect on `Lights` plus a level effect on `Pumps` (or two layers of one model) combine as in any
  xLights model.

---

## 4. The Fountains effect

One effect drives **pump levels and light colours from a single value curve**:

- the curve's **value** is the pump level (0–100%);
- the curve's **colour keyframes** are the light colour over time.

### 4.1 Settings (as stored on the effect in the `.xsq`, `E_` prefix)

| Key | Values | Default | Meaning |
|---|---|---|---|
| `E_CHOICE_Fountains_Type` | `All`, `Chase` | `All` | `All`: every pump plays the curve together. `Chase`: pumps take turns. Old saves may say `Sequential`, which is Chase with 0% overlap. |
| `E_VALUECURVE_Fountains_Curves` | value curve string (§6) | flat 0% | Pump level and light colour over one pass (All) or one turn (Chase) |
| `E_SLIDER_Fountains_Curves` | 0–100 | 0 | Flat level used only when no curve is saved. Lights stay black then. Normally unused. |
| `E_TEXTCTRL_Fountains_Cycles` | float 0–100 (the panel steps by 0.1) | 1.0 | Passes over the effect's duration. 0 freezes the pattern at its start. |
| `E_CHOICE_Fountains_Direction` | `Forward`, `Reverse`, `Forward Bounce`, `Reverse Bounce` | `Forward` | Chase order |
| `E_SLIDER_Fountains_PumpOffset` | 0–64 | 0 | The chase starts this many units along (wraps) |
| `E_SLIDER_Fountains_Overlap` | 0–95 | 0 | Chase: how much each turn overlaps the next (%) |

In the panel:

- Direction, Pump Offset and Overlap are disabled when Type = All.
- **Curves** is an "Edit Curves..." button. The curve is **always active**, and its dialog opens in colour
  mode (§7.1).
- **Reset**, beside it, puts the curve back to its default: flat at 0%, no colour keys, wrap off. It asks
  first when there's something to lose. The effect then re-renders with its pumps off.
- The effect doesn't use the colour palette. Its colours come only from the curve's colour keys.

### 4.2 Units: what takes a turn

The effect looks at the buffer's nodes and groups them into **units**:

- **On a fountain (or its submodels):** each pump together with its light is one **unit**.
  - Units are numbered in the order the buffer lists them, which is pump order.
  - Pump channels get the **level** (grey `level × 2.55`). Light channels get the **colour**.
  - Any other channel is left alone.
- **On a group** (with a buffer that isn't Per Model): **each member of the group is one unit**, in the group's
  order. All of a member's pumps take the level together and all its lights take the colour.
  - A group of submodels such as `Fountain Pair 1`…`Fountain Pair 4` chases pair by pair: 4 turns, not 8.
  - A group of whole fountains chases fountain by fountain.
  - A group of other models chases model by model.
  - A channel that's in two members belongs to the first one.
  - With a **Per Model** buffer style, each member renders on its own, so inside each member its pumps take
    the turns again.
- **On any other model:** every node is a unit and takes the colour (one pixel at a time for Chase). The level
  isn't used, so **without colour keys nothing lights up** (no keys = black).

So the target decides what's driven:

- on `Pumps`, only water;
- on `Lights`, only colour;
- on the whole model, `Perimeter` or `Center`, both.

### 4.3 Timing (`UnitTimes` in `FountainsEffect.cpp`)

Let `D` be the effect's duration, `C` = Cycles, `position` = 0→1 through the effect, and `t = position × C`
(passes so far).

**All.** Every unit plays the curve at `frac(t)`:

- One pass lasts `D / C` seconds, and the curve's x = 0→1 spans one pass.
- The very last frame is the end of the last pass, not a restart.

**Chase:**

- **Turn order** comes from Direction (`TurnOrder`). There are `T` turns per pass:
  - **Forward:** 0, 1, …, n−1, so `T = n`.
  - **Reverse:** n−1, …, 0, so `T = n`.
  - **Bounces** go there and back without repeating the ends. Forward Bounce on 4 pumps is 0 1 2 3 2 1, so
    `T = 2n − 2` (n > 2).
  - Each turn's unit is `(order[turn] + PumpOffset) % n`.
- **Steps:** one pass is `T` equal steps, and turn *k* starts at `k / T` of each pass.
  - In seconds: **step = D / (C × T)**.
- **Turn length:** each turn lasts `len = step / (1 − Overlap)`.
  - In seconds: **turn = D / (C × T × (1 − Overlap))**.
  - **The curve's x = 0→1 spans one turn**, not the pass.
  - About `1 / (1 − Overlap)` units are running at any moment: 1 at 0%, 2 at 50%, 4 at 75%, 10 at 90%.
- **Wrapping:** turns run on around the loop. Pump 1 starts its next turn on schedule while the last pumps
  are still finishing, so a continuous **twirl** never pauses between laps.
  - The first pass starts clean: no turns are left over from before the effect. At high overlap the pattern
    therefore "fills in" over the first pass.
  - A turn still running at the effect's end is cut off.
- **Outside its turns,** a unit's pump is 0 and its light is black.
- **Several turns at once:** if a unit has more than one turn running (bounces, or very high overlap with few
  pumps), the most recent one wins.
- **Speed:** one pass = one lap of the order per Cycle, so **Cycles sets the twirl speed**. For a bounce, one
  Cycle is there and back.

Worked example: an 8-pump ring, a 16 s effect, Chase Forward, Cycles 2, Overlap 75%.

- step = 16 / (2 × 8) = **1 s**, and turn = 1 / 0.25 = **4 s**.
- A new pump starts every second, each plays the whole curve over 4 s, and 4 pumps are up at once.
- One lap takes 8 s.
- With a 0 → 100% → 0 eased hump on a 15 ft fountain, each jet has 2 s to rise. That's enough to reach the
  top (~1.2 s, §2.8).

**Effects are independent.** Nothing carries over from one effect to the next. A chase restarts clean at the
start of the next effect, and the next effect's curve starts at its x = 0. For one continuous motion, use
**one long effect with more Cycles** rather than several short ones. The physical water does carry over:
water in the air keeps flying across effect boundaries.

### 4.4 Order of writes and shared cells

Pumps are written first, then lights. If a pump and its light share a cell (**Per Preview**), the light's
colour wins and the pump then gets min(r,g,b) of that colour. A blue light, for example, means pump 0.

**Use the Default or Single Line buffer for Fountains on a fountain.**

### 4.5 The effect bar in the sequencer grid (`DrawEffectBackground`)

It shows **the first unit's** timeline only (for Chase, pump 1's turns), looped by Cycles exactly like the
render. The chase timing uses the row's unit count (taken from the row's Default buffer).

- **With colour keys, on a row that has lights:** the colour gradient over time.
  - If the row also has pumps, a **black line with a white edge** shows the pump level (bottom = 0%,
    top = 100%).
  - The line is hidden when the level is 0 throughout or the row has no pumps.
  - On a lights-only row with the grid's "ramps" display on, the gradient's height follows the level.
- **With no colour keys, or on a pumps-only row (`Pumps`, `Center Pump`…):** a **filled area chart**, a white
  area over black showing the pump level 0–100%.

`RenderableEffect::BackgroundNeedsPalette()` is false for Fountains. That stops the grid from replacing the
bar with just the icon when no palette colours are checked.

---

## 5. Inline editing in the grid

Fountains, Fire (Height curve) and On (colour keys only) can be edited **directly on the effect** in the
sequencer grid.

- **Turning it on:** drag a row taller. Every supporting effect in a row at least **5× the normal row height**
  is drawn in inline mode. Rows go up to 20×.
- **Saving:** the row height is saved in the `.xsq` (the `rowHeight` attribute on `EffectLayer` /
  `SubModelEffectLayer`), so the rows come back in inline mode.
- **Layout of a Fountains effect in inline mode:**
  - the thin bar at the top still moves and resizes the effect;
  - below it is the level curve (a black line with a white edge, square points);
  - at the bottom is the colour strip with its keys (circle = blend key, square = hard key).
- **What is drawn:** the editable span is **the first unit's first turn** (Chase) or **the first pass**
  (All), placed where it plays in the effect. That's exactly the part of the effect the curve's x = 0→1
  covers. If the effect ends before that turn finishes, the span is cut there.
- **Mouse:**
  - Click a point or key to select it, then drag to move it. The first and last points stay at x = 0 and 1.
    Shift, Ctrl or Cmd snaps the level to 5% steps.
  - Ctrl/Cmd-click an empty spot to add a point, or a key in the colour strip (which opens the colour
    picker; Cancel removes the key). The cursor shows a hand with a plus.
  - Double-click a point to type its position and value, or a key to recolour it.
  - Right-click a point for Hard Point / Ease / Custom. Right-click a key for Blend / Hard.
  - A Custom point shows its Bézier handles; Shift/Ctrl/Cmd while dragging a handle mirrors the other one.
  - Delete / Backspace removes the selected point (not the ends) or key.
  - A plain click on an empty spot selects the effect.
- **Undo and render:** each edit is one undo step and re-renders the effect.

---

## 6. Value curve format and evaluation

### 6.1 The string

The setting is `key=value|` pairs, written in this order: `Active`, `Id`, `Type`, `Min`, `Max`, `Colors`,
`VMax`, …, `WRAP`, `RV`, `Values`. The keys that matter here:

| Key | Format | Meaning |
|---|---|---|
| `Active` | `TRUE` | Must be present for the curve to be used |
| `Id` | `ID_VALUECURVE_Fountains_Curves` | The control the curve belongs to |
| `Type` | `Custom` | Fountains is always Custom |
| `Min` / `Max` | `0.00` / `100.00` | Output range. Point `y` 0–1 maps to Min–Max, so y = 0.6 is 60%. |
| `Colors` | `x:RRGGBB:B` or `x:RRGGBB:H`, `;`-separated | Colour keyframes: `B` = blend into this key, `H` = hard cut at this key |
| `VMax` | float | "Graph Max": the dialog's visible top (a zoom). It doesn't change the output. |
| `WRAP` | `WRAP=TRUE` | For colours: the gradient loops from the last key back to the first (§6.3) |
| `RV` | `RV=TRUE` | Values are stored as real values (written by the panel; harmless) |
| `Values` | `x:y[;x:y…]`, each with an optional shape suffix | The points |

The point shapes:

| Format | Shape |
|---|---|
| `x:y` | Hard point: straight lines into and out of it |
| `x:y:E` | Eased point: smooth in and out, handles flat at ⅓ of each segment |
| `x:y:C:inDx:inDy:outDx:outDy` | Custom Bézier point: handle offsets from the point, in 0–1 units |

**Number rules:**

- Numbers have up to 3 decimals, and a trailing zero is dropped (`0.5` is written `0.50`; `0.125` stays
  `0.125`).
- Point x snaps to a **0.005 grid** (200 steps).
- There must be a point at **x = 0** and one at **x = 1**.
- When saved, a hard middle point with the same y as both its hard neighbours is removed.
- **The string contains no commas**, so it sits safely in the comma-separated effect settings.

### 6.2 How a segment is shaped (`SegmentValue`)

Each segment is a cubic Bézier between neighbouring points, using the left point's **out** handle and the
right point's **in** handle. Handle x is kept inside the segment, so the curve stays a function of x. A hard
point's handle is the point itself, so two hard points give a straight line.

- **Ease the peaks and the bottoms** (`:E`) for natural water motion: the jet swells and settles.
- **Use hard points for sharp hits.** The pump's spin-up rounds them anyway.

### 6.3 Colour at x (`GetColorAt`)

- **No keys:** black. **One key:** that colour everywhere, whatever its blend setting.
- **Between keys:** blend linearly into a `B` key; hold the previous colour up to an `H` key.
- **Before the first key:**
  - WRAP on, first key `B`: blends in from the last key, as a loop.
  - WRAP on, first key `H`: holds the last colour.
  - WRAP off: the first key's colour, solid from the start (Blend or Hard).
- **After the last key:** holds the last colour, or loops toward the first key when WRAP is on and the first
  key is `B`.

For a seamless colour loop across Cycles, turn WRAP on and make the first key Blend. The level curve doesn't
loop on its own, so make its first and last y equal if the water shouldn't jump between passes.

---

## 7. Desktop UI

### 7.1 Value curve dialog (affects all effects unless noted)

- **Graph:** a grey outline, dotted quarter lines and labels at 0/25/50/75/100% of the visible max. The left
  margin sizes itself to the labels.
- **Dragging a point:** a light-blue dotted guide line and a floating value label appear. Shift, Ctrl or Cmd
  snaps to multiples of 5. A point can be dragged past its neighbours (it's re-sorted).
- **Right-click a point:** Hard Point, Ease or Custom. Custom shows Bézier handles; Shift, Ctrl or Cmd while
  dragging a handle mirrors the opposite one, within the segment.
- **Double-click a point:** edit its X (0–100%) and value.
- **Live preview:** the render refreshes about 150 ms after any change, but not while the mouse button is
  down. Cancel restores the curve and re-renders.
- **Graph Max:** a slider and text box that zoom the y axis, saved as `VMax`.
- **Cycles > 1:** an "Edit Points" / "Cycles Preview" toggle. Points can only be edited in Edit Points.
- **Colour mode** (only when the caller allows colour; the Fountains Curves button does):
  - The curve is forced to Custom, range 0–100, Cycles 1, no timing track.
  - The dialog shows only the graph, a **colour bar**, Wrap Values, OK and Cancel.
- **Colour bar:**
  - Click to add a key; this opens the colour picker, and Cancel removes the key.
  - Drag to move a key, double-click to recolour it, right-click for Blend/Hard, Delete/Backspace to remove.
  - Circle marker = blend key, square = hard key, light-blue ring = selected.
- **`valueCurveOnly` (effect JSON):** the slider and text box are replaced by an "Edit <label>..." button,
  and the curve is always active. A new curve is flat at 0%, with no colour keys and wrap off.
- **`valueCurveReset` (effect JSON, with `valueCurveOnly`):** adds a **Reset** button beside Edit that puts
  the curve back to that default.

Effect metadata flags added to `_schema.json`: `valueCurveColor`, `valueCurveOnly`, `valueCurveReset`.

### 7.2 Effect string example

This is one `<Effect>` entry in the `.xsq` `<EffectDB>`, comma-separated. It's a twirl with a blue → cyan →
blue loop:

```
E_CHOICE_Fountains_Type=Chase,E_CHOICE_Fountains_Direction=Forward,E_SLIDER_Fountains_Overlap=75,E_SLIDER_Fountains_PumpOffset=0,E_TEXTCTRL_Fountains_Cycles=2.0,E_VALUECURVE_Fountains_Curves=Active=TRUE|Id=ID_VALUECURVE_Fountains_Curves|Type=Custom|Min=0.00|Max=100.00|Colors=0.00:0000FF:B;0.50:00FFFF:B;1.00:0000FF:B|WRAP=TRUE|Values=0.00:0.00;0.50:1.00:E;1.00:0.00|,B_CHOICE_BufferStyle=Single Line
```

### 7.3 Where effects sit in the `.xsq`

- `<EffectDB>` holds one `<Effect>` text node per distinct settings string. An effect refers to it by its
  0-based index (`ref`).
- `<ElementEffects>` holds:
  - `<Element type="model" name="<model>">`;
  - for the model itself, one `<EffectLayer>` per layer (the first is the top layer);
  - for submodels, `<SubModelEffectLayer name="Pumps">` (with `layer="1"`, … for the second and later
    layers);
  - each effect as `<Effect ref="N" name="Fountains" startTime="ms" endTime="ms"/>`.
- `palette` refers to `<ColorPalettes>`. Fountains ignores it, so it may be omitted.
- `rowHeight="5"` on a layer turns on inline editing (§5).
- A model's row also needs an entry under `<DisplayElements>`. It's easiest to edit a sequence that already
  has the fountain in it.
- **Times:** start and end in milliseconds, on the sequence's frame grid (multiples of the frame time,
  usually 25 or 50 ms). Effects on one layer must not overlap.

### 7.4 Buffer and blending keys used with fountains

| Key | Example | Meaning |
|---|---|---|
| `B_CHOICE_BufferStyle` | `Default`, `Single Line`, `Per Preview` | Buffer style (§2.6) |
| `B_CHOICE_PerPreviewCamera` | `2D` or a saved 3D viewpoint name | Per Preview camera (§2.6) |
| `T_TEXTCTRL_Fadein` / `T_TEXTCTRL_Fadeout` | `1.00` | Fade in / out, seconds |
| `T_CHOICE_In_Transition_Type` / `T_CHOICE_Out_Transition_Type` | `Fade`, `Wipe`, … | Transition type |
| `T_CHOICE_LayerMethod` | `Normal`, `Max`, `Additive`, … | How this layer mixes with the ones below |
| `C_SLIDER_Brightness` | `100` | Brightness %; scales pump levels too |

---

## 8. Sequencing guide

### 8.1 Choosing the target

| Goal | Put the effect on | Buffer |
|---|---|---|
| Water and light together, one curve | the fountain model, or `Perimeter` / `Center` | Default or Single Line |
| Water only | `Pumps`, `Perimeter Pumps`, `Center Pump` | Default / Single Line |
| Colour only (any standard effect) | `Lights`, `Perimeter Lights`, `Center Light` | Single Line for one-by-one; **Per Preview with an orthographic top-down viewpoint** for spatial effects (Fan, Spirals, Pinwheel, Ripple, Shockwave…) |
| Spatial water patterns (waves across the pool, a ripple out from the center) | `Pumps` | Per Preview with an orthographic top-down viewpoint. Pumps read **min(r,g,b)**, so use white/grey colours. |
| Exact channel values | the model | Default + DMX effect |

### 8.2 Fountains effect recipes

| Look | Type | Direction / overlap | Curve | Notes |
|---|---|---|---|---|
| All jets pulse with the music | All | — | 0 → peak → 0, eased | Cycles = beats in the effect, if each beat is longer than the air time at that height. Start the effect about the rise time early (§2.8) so the water peaks on the beat. |
| Steady columns with colour changes | All | — | flat at the height | Colour keys for the colour story; Cycles 1 |
| Swell | All | — | 0 → 100% over the whole effect, eased | Cycles 1 |
| Twirl / spin around a ring | Chase | Forward, Overlap 50–80% | Smooth hump (eased 0 → 100 → 0) | A travelling wave. Cycles = laps; it never pauses between laps. Reverse spins the other way; Pump Offset rotates the starting pump. Keep each turn (§4.3) longer than the jet's rise time or the wave flattens. |
| One at a time | Chase | Overlap 0% | — | Each pump gets an equal slice: step = turn = D / (C × n) |
| Ping-pong along a line | Chase | Forward Bounce | — | One Cycle is there and back |
| Colour chase with the water | Chase | — | Add colour keys | Each pump's light follows the same colour timeline during its turn and is black between turns. With high overlap that makes a comet: the colours trail behind the wave. |
| Center vs perimeter | — | — | — | Separate effects on `Center` and `Perimeter`, for example a slow tall center column under a fast perimeter twirl. Center Max Water Height can differ. |
| Accent hits | — | — | — | A second layer with Max mixing (`T_CHOICE_LayerMethod=Max`) and short All pulses over a base pattern |
| Burst (spray on purpose) | — | — | Tall → quick drop to ~30% → back up while the water is still falling | Makes visible spray |

### 8.3 Fitting the water to the music

- **Rise time:** start a rise "command → top" before the beat (~1.2 s for a 15 ft jet at 100%; see §2.8).
  Lower jets react faster: a 5 ft jet is ~0.56 s.
- **Shorter, faster motion needs lower heights.** Within one beat interval a jet only reaches what it can in
  that time. At 120 BPM (0.5 s per beat), full-height pulses on a 15 ft pump never get near the top. Pulse
  around 3 ft (a ~0.44 s rise) on every beat, or go taller on every other beat.
- **Big moments:** tall columns need time. Bring them up during a build so they're at the top on the drop.
- **Endings:** water in the air keeps falling for about half the air time. To finish on a hit, cut the pumps
  that long before the musical end, or let the fall be the ending.
- **Contrast:** alternating levels and quiet moments make the tall moments read. Holding everything at 100%
  flattens the show.

### 8.4 Gotchas

- A pump runs only on **white/grey**: min(r,g,b). Coloured effects on pump channels can be 0.
- Water lags the command (delay, spin-up, flight). Sequence rises early. Quick pulses don't reach full height.
- The preview shows water only from **rendered** data. Render after changes.
- Fountains on a **Per Preview** buffer of a model that has both pumps and lights gives each pump its light's
  colour (shared cell). Use Default or Single Line.
- Per Preview with the default **`2D` camera** flattens a ring to its front view, and pumps at the same
  left-right position merge. Use a top-down (orthographic) viewpoint.
- The Fountains curve defaults to **0%** (pumps off) until you add points.
- Fountains on a non-fountain model needs colour keys, or it stays black.
- Cycles also repeats the colour keys. Use WRAP with a Blend first key for a seamless colour loop.
- A chase restarts at each new effect. Use one long effect with more Cycles for continuous motion.
