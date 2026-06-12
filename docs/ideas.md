# ZWM — Feature and Layout Ideas

Everything here is a suggestion. Nothing is planned or scheduled.
Pick what interests you; ignore the rest.

---

## Layouts

### 1. Fibonacci / Spiral
Each new window splits the largest remaining region in alternating directions,
producing a Fibonacci-rectangle spiral. The first window takes the full screen,
the second takes half, the third takes half of the remaining half in the
perpendicular axis, and so on. Feels like DEFAULT but the split axis rotates
automatically instead of following the node orientation.

### 2. Dwindle
A variant of Fibonacci where every new window always takes the right or bottom
half of the previously focused window's partition, and older windows shrink
proportionally. Similar to how some WMs call it "dwindle" — windows dwindle as
more are added.

### 3. Centered Master
Like MASTER but the master window sits in the center column at a configurable
width, and the remaining windows share the left and right sides equally. Already
similar to THREE_COL, but the key difference is the master is the largest/center
and its identity persists across inserts/deletes without the three-column side
distribution logic.

### 4. Wide / Horizontal Master
MASTER with the master occupying the top half of the screen (horizontal split)
and the remaining windows tiling in the bottom strip. The current MASTER is
always a left-side vertical split. A wide variant adds a second axis option so
you can choose vertical or horizontal master orientation.

### 5. Tab / Stacking with Tab Bar
Like MONOCLE but with a visible tab strip along the top (or bottom) of the
window area showing window titles. Clicking or pressing a key cycles the visible
window. Requires the WM to draw a lightweight tab strip itself (X drawing calls)
or delegate it to a secondary bar process via IPC.

### 6. Max
Every tiled window is stretched to the full usable area — no gaps, no borders.
Simpler than MONOCLE: MONOCLE tracks a selected visible window; Max just maps
every window at full size and lets the normal focus/stacking mechanism determine
which one is visible. Useful for presentations or single-tasking.

### 7. Column / N-Column
Generalized THREE_COL where the number of columns is configurable (1, 2, 3, 4…).
Windows are distributed round-robin across columns. Adding a fourth column makes
it a quad-column layout. The number of columns could be a per-desktop setting.

### 8. Paper / Scrollable
Windows are arranged in a single horizontal row, each at a fixed width. The
"viewport" scrolls left and right to reveal windows that extend off-screen.
Focus follows the viewport; the WM snaps the view to the focused window.
Good for ultrawide monitors.

### 9. Equal / Fair
All tiled windows are resized to equal area. The WM automatically decides how
to split the screen (rows × columns closest to square for N windows) and
reflows when windows are added or removed. Similar to GRID but prefers aspect-
ratio-balanced cells rather than pure column distribution.

### 10. Centered Floating
A pseudo-layout where every new tiled window is automatically floated and
centered at a configurable fraction of the screen (e.g., 70% width × 80%
height). Not really a tiling layout — intended for "focus one thing at a time"
workflows without the hidden-window complexity of MONOCLE.

---

## Window Management Features

### 1. Scratchpad
A window that can be toggled on/off over the current desktop regardless of which
desktop it was spawned on. Super + grave (or any key) hides/shows it. The window
is not part of any desktop's BSP tree — it floats above everything when visible.
Common use case: a persistent terminal. Can support multiple named scratchpads.

### 2. Window Marks / Tags
Assign a letter or number to any window (`mark a`) and jump to it from anywhere
(`go-to-mark a`). The WM switches to the window's desktop and focuses it. Marks
persist across desktop switches. Think Vim marks but for windows.

### 3. Sticky Windows
A window marked sticky appears on every desktop simultaneously. Floating sticky
windows stay in the same screen position regardless of which desktop is active.
Tiled sticky windows are inserted into each desktop's tree (complex) — most WMs
restrict sticky to floating only.

### 4. Window Groups / Tabbing
Multiple windows can be grouped into one logical container that occupies a single
BSP leaf. Only one window in the group is visible at a time; cycle with a key.
The group looks like a single tiled window from the layout's perspective. Similar
to i3's tabbed containers.

### 5. Per-Desktop Gaps
Override `window_gap` on a per-desktop basis. Desktop 1 might have gap=0 for
coding; desktop 3 might have gap=20 for reading. Applied at render time; no
global gap change needed.

### 6. Smart Gaps
Automatically set gap to 0 when there is only one visible tiled window on the
desktop. Restores the configured gap when a second window appears. Common
request in tiling WMs; saves screen space for single-window workflows.

### 7. Proportion Resize with Mouse in All Layouts
Right now mouse resize on DEFAULT resizes the shared edge. Extending this so
that MASTER, THREE_COL, and DECK master-split resize all work with the right
click drag on any edge (not just the main split line).

### 8. Window Cycling History (Alt-Tab Style)
Maintain a per-desktop MRU stack and expose `cycle-focus-prev` / `cycle-focus-next`
keybinds that walk through recently focused windows in MRU order. Different from
directional focus (which is spatial) — this is purely time-based. The MRU
counter is already tracked; this just exposes it as a navigation action.

### 9. Urgent / Attention Flag
When a window sets `_NET_WM_STATE_DEMANDS_ATTENTION` or `XUrgencyHint`, mark
its desktop indicator in the status bar (already EWMH-compliant) and optionally
flash the border. Optional config key to auto-focus urgent windows or just flag
them.

### 10. Window Rules: Floating Size
Extend the existing rule syntax to specify initial floating dimensions and
position:
```ini
rule = wm_class("mpv"), state(floated), desktop(-1), size(800x600), pos(center)
```
Position values: `center`, `top-left`, `top-right`, `bottom-left`,
`bottom-right`, or absolute `x,y` pixels.

### 11. Window Rules: Layout Override
Specify which layout a desktop should use when a matching window spawns on it,
or force a window to a specific desktop that is then auto-switched to a given
layout:
```ini
rule = wm_class("gimp"), state(floated), desktop(3), layout(default)
```

### 12. Window Rules: Script Hook
Run a shell command when a window matching a rule appears:
```ini
rule = wm_class("zoom"), state(tiled), desktop(2), on_spawn("notify-send 'Zoom opened'")
```

### 13. Inhibit Fullscreen for Specific Windows
Rule to prevent a window from going fullscreen even if it requests it. Useful
for video players that auto-fullscreen on spawn.

### 14. Focus Boundaries / Focus Wrap
Configurable behavior for directional focus at screen edges: whether focus wraps
around to the opposite side, stops at the edge, or jumps to the next monitor.

### 15. Window Borders: Per-State Colors
Currently: `active_border_color` and `normal_border_color`. Extend to:
- `urgent_border_color` for windows with the urgency/attention flag
- `floating_border_color` to visually distinguish floating windows
- `fullscreen_border_color` (or no border at all for fullscreen — configurable)

### 16. Border Radius (Rounded Corners)
Requires XShape extension. Apply a rounded-corner mask to window borders.
Configurable radius in pixels (`border_radius = 8`). Has a small performance
cost for every window.

### 17. Inner Gaps
Separate `outer_gap` (space between windows and monitor edge) from `inner_gap`
(space between adjacent windows). Currently the single `window_gap` applies to
both. Many users want larger outer padding and smaller inner gaps.

### 18. Window Minimization
Map `_NET_WM_STATE_HIDDEN` / iconify to a real minimize: the window is unmapped
and removed from the visible layout but kept in a per-desktop minimized list.
Restore by picking from a list via a keybind or dmenu script. ZWM currently
handles WM_STATE_ICONIC in the unmap path but doesn't expose manual minimize.

### 19. Expose / Overview Mode
A temporary layout that shrinks and arranges all windows on the current desktop
into a grid overview (similar to macOS Mission Control or KWin's Present
Windows). Press a key to enter, click or press a key to select. Leaves the
normal layout unchanged on exit. Requires temporary geometry overrides and a
selection mechanism.

### 20. Window Opacity / Transparency
Set `_NET_WM_WINDOW_OPACITY` on focused vs unfocused windows:
```ini
active_opacity   = 1.0
inactive_opacity = 0.85
```
Works with a compositor (picom etc.). The WM just sets the property; the
compositor does the actual blending.

### 21. Invert Focus Split Direction
When a new window splits the focused partition, let the user configure whether
the new window lands on the first_child or second_child side. Currently fixed.

### 22. Lock Layout
A per-desktop flag that prevents the layout from being changed by accident. Good
for a dedicated terminal desktop where you always want DEFAULT. Toggled by a
keybind.

---

## Multi-Monitor Features

### 1. Follow Focus to Monitor
When switching monitors (`cycle_monitors`), automatically focus the last focused
window on the new monitor without requiring an explicit window focus action after
the monitor switch.

### 2. Move Window to Next / Prev Monitor
A keybind that transfers the focused window to the next/previous monitor's active
desktop, analogous to `transfer_node` for desktops.

### 3. Mirror Layout Across Monitors
Apply the same layout change simultaneously to the matching desktop index on all
monitors. Optional; some users want independent per-monitor layouts (current
behavior) while others want mirrored changes.

### 4. Per-Monitor Wallpaper via WM IPC
ZWM sets `_XROOTPMAP_ID` / `_XROOTMAP_ID` on each monitor's root, allowing
tools like `feh` or `nitrogen` to set per-monitor wallpapers. The WM itself
doesn't render wallpapers but can coordinate the root-window property.

### 5. Primary Monitor Flag
Mark one monitor as "primary" in the config. Window rules with `desktop(-1)` on
a freshly opened window are directed to the primary monitor. Currently windows
open on `curr_monitor`.

---

## IPC / Scripting

### 1. Unix Socket IPC
Expose a Unix domain socket that accepts text commands mirroring the existing
keybind actions (`focus left`, `layout master`, `kill`, etc.). Allows external
scripts, status bars, and tools to drive the WM without keybinds. Modeled after
bspwm's `bspc` or i3's IPC socket.

### 2. Event Subscription over IPC
Clients connected to the socket can subscribe to events: window focus change,
desktop switch, layout change, window spawn/kill. Useful for writing status bar
modules that react to WM state without polling.

### 3. JSON State Dump
A command (via IPC or a signal) that prints the current WM state as JSON:
monitors, desktops, windows, layouts, focus state. Makes scripting and debugging
much easier.

### 4. Named Desktops
Allow desktops to have user-defined names in the config (`desktop_names = code,
web, chat, media`). Names are exposed via `_NET_DESKTOP_NAMES`. Polybar's EWMH
module already reads this and can display names instead of numbers.

### 5. Reload Specific Desktop Layout
IPC command to apply a layout to a specific desktop by index or name without
switching to it.

---

## Usability / Quality of Life

### 1. Window Snapping (Floating)
When dragging a floating window, snap it to screen edges, to other windows, or
to a configurable grid. Threshold in pixels: within N pixels of an edge, the
window snaps to it.

### 2. Resize Floating with Scroll
`super + scroll` resizes the floating window under the cursor uniformly (grow /
shrink from center). Faster than the current corner-drag approach for quick size
adjustments.

### 3. Focus-on-Hover Delay
Add a `focus_follow_pointer_delay` config value (milliseconds). The window under
the cursor only gets focus after the pointer has been over it for that long. Cuts
down on accidental focus switches when moving the mouse across the screen.

### 4. No-Focus Windows
A window rule flag `focusable(false)` that prevents the WM from ever giving that
window input focus (useful for on-screen overlays, notification windows, or
reference panels you never want to accidentally type into).

### 5. Warp Cursor to Focused Window
When focus changes via keyboard (switch desktop, transfer, traverse), warp the
cursor to the center of the newly focused window. Configurable: `warp_cursor =
true/false`. Avoids the cursor being stranded on the wrong side of the screen.

### 6. Floating Window Stack Order Lock
A rule or keybind that pins a floating window to always-on-top or always-on-
bottom (relative to other floating windows but still above/below tiled as
appropriate). Currently floating windows are always LAYER_ABOVE; this would let
you demote a specific floating window to LAYER_BELOW tiled.

### 7. Center Floating Window Command
A keybind that centers the currently focused floating window on its monitor,
without resizing it. Quick alternative to shift-window-by-10px.

### 8. Remembered Floating Positions
When a floating window is tiled and later re-floated, restore it to its last
floating position and size instead of computing a new centered default position.
The `floating_rectangle` is already stored on the node; this just means not
overwriting it on re-float if it was previously set.

### 9. Config Variable: Default Layout per Desktop
```ini
desktop_layouts = default, master, stack, default, monocle, default, default
```
Each desktop starts with a specific layout instead of all defaulting to DEFAULT.

### 10. Animated Transitions (Optional / Compositor-Assisted)
When moving or resizing windows, instead of jumping instantly, interpolate
geometry over a few frames. Requires either a compositing approach or rapid
intermediate `xcb_configure_window` calls with a timer. Performance-sensitive;
best kept optional and off by default.

---

## EWMH / ICCCM Compliance

### 1. `_NET_WM_STRUT` (non-partial) Fallback
Some older bars only set `_NET_WM_STRUT` (the 4-value version, no per-edge
ranges). ZWM currently only reads `_NET_WM_STRUT_PARTIAL`. Adding a fallback
read of `_NET_WM_STRUT` when partial is absent would improve compatibility with
older panel software.

### 2. `_NET_WORKAREA`
Publish the usable area for each desktop as `_NET_WORKAREA`. Some applications
(dialog boxes, splash screens) use this to position themselves within the usable
area rather than the raw monitor rectangle.

### 3. `_NET_WM_VISIBLE_NAME`
When a window has a long title and the WM shortens it for display, publish the
shortened version as `_NET_WM_VISIBLE_NAME`. Largely cosmetic but satisfies
strict EWMH compliance checkers.

### 4. ICCCM Size Hints Enforcement
Respect `WM_NORMAL_HINTS` `PMinSize`, `PMaxSize`, and `PResizeInc` when tiling.
Currently ZWM overrides all geometry. Enforcing hints would prevent windows from
being tiled below their minimum size, which can crash some applications (terminal
emulators with a minimum cell-count requirement).

### 5. `_NET_WM_STATE_DEMANDS_ATTENTION` Handling
When an application sets this state, visually indicate the desktop (polybar
already shows it as "urgent" if the EWMH module is configured). Optionally, add
a config key `auto_focus_urgent = true/false` to switch to the demanding window
automatically.

---

## Developer / Debug

### 1. Built-in Window Tree Dump
A keybind or IPC command that prints the full BSP tree for the current desktop
to stderr or a log file. Useful for debugging layout bugs. The `log_tree_nodes`
function already exists; this just exposes it at runtime.

### 2. Reload Without Restart
`reload_config` already exists. Extend it to also re-evaluate window rules
against existing windows (re-apply rules to already-open windows), not just
apply rules to new ones.

### 3. Dry-Run Config Parse
A `--check` CLI flag that parses the config file, reports errors, and exits
without starting the WM. Useful in scripts and shell aliases before reloading.

### 4. Config Schema / Man Page
A `zwm.conf(5)` man page listing every config key, its type, default value, and
description. Auto-generated from a central config-key table in the source would
keep it in sync.
