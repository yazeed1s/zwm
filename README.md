### zwm

## Table of Contents

-   [About ZWM](#about-zwm)
-   [Motivation](#motivation)
-   [Goals](#goals)
-   [Features](#features)
-   [The underlying data structure](#the-underlying-data-structure)
-   [Screenshots](#screenshots)
-   [Installation](#installation)
-   [Configuration](#configuration)
-   [Default Keybindings](#default-keybindings)
-   [ewmh specific settings for polyabr](#ewmh-specific-settings-for-polyabr)
-   [Contributing](#contributing)

## About ZWM

zwm is a minimalistic and opinionated tiling window manager for X11. It uses XCB instead of Xlib to communicate with the X server. The underlying data structure for managing windows is a customized BSP tree.

## Motivation

The motivation behind zwm stems from a desire to create a window manager that is both lightweight and highly efficient, yet tailored to how I like to work. Sure, there are other great tiling window managers that perform well and offer robust features, but zwm was developed primarily as a learning exercise in creating a window manager from scratch.

## Goals

-   Minimalism
-   Efficiency
-   Performance

## Features

-   Compliance with a subset of [ewmh](https://specifications.freedesktop.org/wm-spec/wm-spec-1.3.html) and [icccm](https://www.x.org/releases/X11R7.6/doc/xorg-docs/specs/ICCCM/icccm.html)
-   Multiple Layouts (default, master, stack, grid).
-   Multiple virtual desktops.
-   Multi-monitor support.
-   Independent workspaces for each monitor by default.
-   Low memory footprint, runs within ~2MB of memory
-   Resize, flip, and swap windows or partitions.
-   Layouts apply to individual desktops.
-   Customizable settings.
-   Customizable window rules.
-   Keyboard-Driven, fully controlled via keyboard shortcuts.
-   Can be integrated with any status bar.
-   Config reload on the fly.

## The underlying data structure:

ZWM uses **binary space partitioning tree** ([BSP-tree](https://en.wikipedia.org/wiki/Binary_space_partitioning)) to store and manage windows. This allows for more flexible layouts.

-   Each desktop has its own pointer to a bsp-tree
-   The tree is a partition of a monitor's rectangle into smaller rectangular regions.
-   Each leaf node holds exactly one window.
-   Each node in a bsp-tree either has zero or two children.
-   Each internal node is responsible for splitting a rectangle in half.

#### The following should illustrate how bsp-tree is used to achive window managment:

```
    Window and Partition Structure in a BSP-tree:

    The layout of windows in BSPWM follows a binary tree structure:

         I         ROOT (root is an INTERNAL NODE, unless it is a leaf by definition)
       /   \
      I     I      INTERNAL NODES (screen sections/partitions)
     / \   / \
    E   E E   E    EXTERNAL NODES (windows/leaves)

    - Internal Nodes (I) represent screen sections where windows can be displayed.
    - External Nodes (E) represent the actual windows within those sections.
    - Windows (E nodes) share the width, height, and coordinates (x, y) of their   parent sections (I nodes).

    Example Structure:

         I
       /   \
      I     I
     / \   / \
    E   E E   I
             / \
            E   E

    Partition Behavior:
    - Internal nodes (I) can contain other partitions or be contained within other partitions.
    - External nodes (E) are the leaves (actual windows).

    Example of tree:
    - 1, 2, 3 are leaves/windows (EXTERNAL_NODE).
    - a, b are internal nodes (INTERNAL_NODE), or screen sections/partitions.

            1                               a                          a
                                           / \                        / \
                            --->          1   2         --->         1   b
                                                                        / \
                                                                       2   3
    Visualization of Screen Layout:

    +-----------------------+  +-----------------------+  +-----------------------+
    |                       |  |           |           |  |           |           |
    |                       |  |           |           |  |           |     2     |
    |                       |  |           |           |  |           |           |
    |           1           |  |     1     |     2     |  |     1     |-----------|
    |                       |  |           |           |  |           |           |
    |                       |  |           |           |  |           |     3     |
    |                       |  |           |           |  |           |           |
    +-----------------------+  +-----------------------+  +-----------------------+

    Another Example:
    - Numbers are the are leaves/windows (EXTERNAL_NODE).
    - Letters are internal nodes (INTERNAL_NODE), or screen sections/partitions.

             a                             a                          a
            / \                           / \                        / \
           1   b            --->         c   b         --->         c   b
              / \                       / \ / \                    / \ / \
             2   3                     1  4 2  3                  1  d 2  3
                                                             	    / \
                                                                   4   5

     +-----------------------+  +-----------------------+  +-----------------------+
     |           |           |  |           |           |  |           |           |
     |           |     2     |  |     1     |     2     |  |     1     |     2     |
     |           |           |  |           |           |  |           |           |
     |     1     |-----------|  |-----------|-----------|  |-----------|-----------|
     |           |           |  |           |           |  |     |     |           |
     |           |     3     |  |     4     |     3     |  |  4  |  5  |     3     |
     |           |           |  |           |           |  |     |     |           |
     +-----------------------+  +-----------------------+  +-----------------------+

```

## Screenshots:

<p align="left">
  <img src="https://github.com/Yazeed1s/zwm/blob/main/docs/img/img1.png" width="1000">
</p>
<p align="left">
  <img src="https://github.com/Yazeed1s/zwm/blob/main/docs/img/img2.png" width="1000">
</p>
<p align="left">
  <img src="https://github.com/Yazeed1s/zwm/blob/main/docs/img/img3.png" width="1000">
</p>

## Installation

#### Arch Linux (AUR)

```bash
yay -S zwm
```

#### Void Linux (XBPS-SRC)

Assuming you have [void-packages](https://github.com/void-linux/void-packages)

```bash
git clone https://github.com/elbachir-one/void-templates
cp void-templates/zwm/ void-packages/srcpkgs/  # Copying the zwm/ directory that has the template
cd void-packages/
./xbps-src pkg zwm
sudo xbps-install -R hostdir/binpkgs zwm
```

#### Build from source

##### Dependencies

-   gcc
-   libxcb
-   xcb-util
-   xcb-util-keysyms
-   xcb-util-wm (ewmh,icccm)
-   lxcb-randr
-   lxcb-xinerama
-   lxcb-cursor

```bash
git clone https://github.com/Yazeed1s/zwm.git
cd zwm && sudo make install
```

## Configuration

##### A config file will be generated when you first start `zwm`. The file can be found in the following location:

-   ~/.config/zwm/zwm.conf

###### As of now, the following configs are offered:

### 1- Config variables

```ini
border_width = 2
active_border_color = 0x4a4a48
normal_border_color = 0x30302f
window_gap = 10
virtual_desktops = 7
focus_follow_pointer = true
```

##### Available Variables:

-   **border_width**: Defines the width of the window borders in pixels.
-   **active_border_color**: Specifies the color of the border for the active (focused) window.
-   **normal_border_color**: Specifies the color of the border for inactive (unfocused) windows.
-   **window_gap**: Sets the gap between windows in pixels.
-   **virtual_desktops**: sets the number of virtual desktops.
-   **focus_follow_pointer**: If false, the window is focused on click; if true, the window is focused when the cursor enters it.

### 2- Commands to run on startup

##### Use the `exec` directive to specify programs that should be started when ZWM is launched.

-   For a single command:

```ini
exec = "polybar"
```

-   To specify additional arguments as a list:

```ini
exec = ["polybar", "-c", ".config/polybar/config.ini"]
```

### 3- Custom window rules

##### Custom window rules allow you to define specific behaviors for windows based on their window class.

##### Syntax:

```ini
rule = wm_class("window class name"), state(tiled|floated), desktop(1..N)
```

##### Explanation:

-   **wm_class**: The window class name used to identify the window.
    -   Use the **`xprop`** tool to find the wm_class of a window.
-   **state**: Specifies whether the window should be tiled or floated.
-   **tiled**: The window will be tiled... clearly.
-   **floated**: The window will be floated... clearly.
-   **desktop**: The virtual desktop number where the window should be placed.
    -   Use **-1** if you do not want to set it to a specific desktop.

```ini
; Example:
rule = wm_class("firefox"), state(tiled), desktop(-1)
; This rule sets "firefox" window to be tiled and does not change its virtual desktop.
```

### 4- Key bindings

-   The format for defining key bindings is: `bind = modifier + key -> action`
-   If two modifiers are used, combine them with a pipe (|). For example, alt + shift is written as `alt|shift`.
-   Note: Some functions require additional arguments to specify details of the action.
-   These arguments are provided using a colon syntax, where the function and its argument are separated by a colon.
-   Example: `func(switch_desktop:1)` means "switch to desktop 1".
-   Example: `func(resize:grow)` means "grow the size of the window".
-   Example: `func(layout:master)` means "toggle master layout".

#### Available modifires:

-   **super (meta)**, **alt**, **shift**, **ctrl**

#### Available Actions:

-   run(...): Executes a specified process.

    -   Example: `bind = super + return -> run("alacritty")`
    -   To run a process with arguments, use a list:
        Example: `bind = super + p -> run(["rofi", "-show", "drun"])`

-   func(...): Calls a predefined function. The following functions are available:

    -   **kill**: Kills the focused window.
    -   **switch_desktop**: Switches to a specified virtual desktop.
    -   **fullscreen**: Toggles fullscreen mode for the focused window.
    -   **swap**: Swaps the focused window with its sibling.
    -   **transfer_node**: Moves the focused window to another virtual desktop.
    -   **layout**: Toggles the specified layout (master, deafult, stack).
    -   **traverse**: (In stack layout only) Moves focus to the window above or below.
    -   **flip**: Changes the window's orientation; if the window is primarily vertical, it becomes horizontal, and vice versa.
    -   **cycle_window**: Moves focus to the window in the specified direction (up, down, left, right).
    -   **cycle_desktop**: Cycles through the virtual desktops (left, right).
    -   **resize**: Adjusts the size of the focused window (grow, shrink).
    -   **reload_config**: Reloads the configuration file without restarting ZWM.
    -   **shift_window**: Shift the floating window's position to the specified direction by 10px (up, down, left, right).
    -   **gap_handler**: Increase or decrease window gaps (GROW, SHRINK).
    -   **change_state**: Set window state (FLAOTING, TILED).
    -   **grow_floating_window**: Grow floating window (horizontally or vertically).
    -   **shrink_floating_window**: Shrink floating window (horizontally or vertically).
    -   **cycle_monitors**: Cycle between monitors (left or right, relative to the linked-list order not the physical positioning).

-   Default keys

```ini
bind = super + return -> run("alacritty")
bind = super + space -> run("dmenu_run")
bind = super + p -> run(["rofi","-show", "drun"])
bind = super + w -> func(kill)
bind = super + 1 -> func(switch_desktop:1)
bind = super + 2 -> func(switch_desktop:2)
bind = super + 3 -> func(switch_desktop:3)
bind = super + 4 -> func(switch_desktop:4)
bind = super + 5 -> func(switch_desktop:5)
bind = super + 6 -> func(switch_desktop:6)
bind = super + 7 -> func(switch_desktop:7)
bind = super + l -> func(resize:grow)
bind = super + h -> func(resize:shrink)
bind = super + i -> func(gap_handler:grow)
bind = super + d -> func(gap_handler:shrink)
bind = super + f -> func(fullscreen)
bind = super + s -> func(swap)
bind = super + up -> func(cycle_window:up)
bind = super + right -> func(cycle_window:right)
bind = super + left -> func(cycle_window:left)
bind = super + down -> func(cycle_window:down)
bind = shift + up -> func(shift_window:up)
bind = shift + right -> func(shift_window:right)
bind = shift + left -> func(shift_window:left)
bind = shift + down -> func(shift_window:down)
bind = shift + f -> func(change_state:float)
bind = shift + t -> func(change_state:tile)
bind = super|shift + t -> func(shrink_floating_window:horizontal)
bind = super|shift + g -> func(shrink_floating_window:vertical)
bind = super|shift + y -> func(grow_floating_window:horizontal)
bind = super|shift + h -> func(grow_floating_window:vertical)
bind = super|shift + left -> func(cycle_desktop:left)
bind = super|shift + right -> func(cycle_desktop:right)
bind = super|ctrl + right -> func(cycle_monitors:next)
bind = super|ctrl + left -> func(cycle_monitors:prev)
bind = super|shift + 1 -> func(transfer_node:1)
bind = super|shift + 2 -> func(transfer_node:2)
bind = super|shift + 3 -> func(transfer_node:3)
bind = super|shift + 4 -> func(transfer_node:4)
bind = super|shift + 5 -> func(transfer_node:5)
bind = super|shift + 6 -> func(transfer_node:6)
bind = super|shift + 7 -> func(transfer_node:7)
bind = super|shift + m -> func(layout:master)
bind = super|shift + s -> func(layout:stack)
bind = super|shift + d -> func(layout:default)
bind = super|shift + k -> func(traverse:up)
bind = super|shift + j -> func(traverse:down)
bind = super|shift + f -> func(flip)
bind = super|shift + r -> func(reload_config)
```

More options will be added in the future as development progresses.

## Default Keybindings

| Key                      | Description                          |
| ------------------------ | ------------------------------------ |
| `super + w`              | kill/close window                    |
| `super + return`         | launch a terminal (alacritty)        |
| `super + space`          | launch dmenu                         |
| `super + p `             | launch rofi                          |
| `super + [1..N]`         | switch to desktop                    |
| `super + l`              | resize window (grow/expand)          |
| `super + h`              | resize window (shrink)               |
| `super + f`              | toggle fullscreen                    |
| `super + shift + [1..N]` | transfer window to a diff desktop    |
| `super + shift + m`      | toggle master layout                 |
| `super + shift + s`      | toggle stack layout                  |
| `super + shift + d`      | toggle default layout                |
| `super + shift + j/k`    | traverse the stack                   |
| `super + shift + f`      | flip the window/partion              |
| `super + shift + r`      | hot-reload                           |
| `super + shift + y`      | grow floating windows horizontally   |
| `super + shift + h`      | grow floating windows vertically     |
| `super + shift + t`      | shrink floating windows horizontally |
| `super + shift + g`      | shrink floating windows vertically   |
| `super + ctrl + →`       | focus/change monitor right           |
| `super + ctrl + ←`       | focus/change monitor left            |
| `super + s`              | swap window's orientation            |
| `super + ←`              | focus window on the left             |
| `super + ↑`              | focus window above                   |
| `super + →`              | focus window on the right            |
| `super + ↓`              | focus window below                   |
| `super + shift + →`      | cycle desktop right                  |
| `super + shift + ←`      | cycle desktop left                   |
| `shift + ←`              | shift window to the left by 10px     |
| `shift + ↑`              | shift window up by 10px              |
| `shift + →`              | shift window to the right by 10px    |
| `shift + ↓`              | shift window down by 10px            |
| `super + i`              | increase window gaps by 5px          |
| `super + d`              | decrease window gaps by 5px          |
| `super + t`              | tile window                          |
| `super + f`              | float window                         |

## ewmh specific settings for polyabr

### To display the window name (CLASS_NAME):

```ini
[module/xwindow]
type 			= internal/xwindow
format 			= <label>
; Available tokens:
;   %title%
;   %instance% (first part of the WM_CLASS atom, new in version 3.7.0)
;   %class%    (second part of the WM_CLASS atom, new in version 3.7.0)
; Default: %title%
label 		= %title%
label-maxlen 	= 50
label-empty 	= "[null]"
```

### To display workspaces:

```ini
[module/ewmh]
type = internal/xworkspaces
label-active 			= %index%
label-active-background 	= ${colors.bg}
label-active-underline		= ${colors.blue}
label-active-padding		= 1
label-occupied 			= %index%
label-occupied-padding 		= 1
label-urgent 			= %index%!
label-urgent-background 	= ${colors.red}
label-urgent-padding 		= 1
label-empty 			= %index%
label-empty-foreground 		= ${colors.gray}
label-empty-padding 		= 1
label-separator 		= " "
```

For further custmization please refer to polybar's wiki.

## Contributing

If you would like to add a feature or to fix a bug please feel free to submit a PR.
