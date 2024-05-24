### zwm

## About ZWM
zwm is a minimalistic and opinionated tiling window manager for X. It uses XCB instead of Xlib to communicate with the X server. The underlying data structure for managing windows is a customized BSP tree. zwm operates with a memory footprint of only ~2 MB.

## Motivation
The motivation behind zwm stems from a desire to create a window manager that is both lightweight and highly efficient, yet tailored to how I like to work. Sure, there are other great tiling window managers that perform well and offer robust features, but zwm was developed primarily as a learning exercise in creating a window manager from scratch. 

## Goals
- Minimalism
- Efficiency
- Performance

## Features
- [ewmh](https://specifications.freedesktop.org/wm-spec/wm-spec-1.3.html) compliant
- Multi-layout (default, master, stack, grid)
- Virtual desktops
- Resize window/partion 
- Flip window/parition
- Swap window/partion
- layout applies to the desktop rather than the whole wm 
- ...

## Installation
### Build from source
```
git clone https://github.com/Yazeed1s/zwm.git
cd zwm && make all
sudo make install
```
## Configuration
A config file will be generated when you first start `zwm`. The file can be found in the following location:
* ~/.config/zwm/zwm.conf

#### As of now, the following configs are offered:

### 1- Config variables
```ini
border_width = 2
active_border_color = 0x83a598
normal_border_color = 0x30302f
window_gap = 10
virtual_desktops = 5
```
Available Variables:
- border_width: Defines the width of the window borders in pixels.
- active_border_color: Specifies the color of the border for the active (focused) window.
- normal_border_color: Specifies the color of the border for inactive (unfocused) windows.
- window_gap: Sets the gap between windows in pixels.
- virtual_desktops: sets the number of virtual desktops.

### 2- Config keys
```ini
key = {super + return -> run("alacritty")}
key = {super + space -> run("dmenu_run")}
key = {super + p -> run(["rofi","-show", "drun"])}
key = {super + w -> func(kill)}
key = {super + 1 -> func(switch_desktop)}
key = {super + 2 -> func(switch_desktop)}
key = {super + 3 -> func(switch_desktop)}
key = {super + 4 -> func(switch_desktop)}
key = {super + 5 -> func(switch_desktop)}
key = {super + l -> func(grow)}
key = {super + h -> func(shrink)}
key = {super + f -> func(fullscreen)}
key = {super + s -> func(swap)}
key = {super|shift + 1 -> func(transfer_node)}
key = {super|shift + 2 -> func(transfer_node)}
key = {super|shift + 3 -> func(transfer_node)}
key = {super|shift + 4 -> func(transfer_node)}
key = {super|shift + 5 -> func(transfer_node)}
key = {super|shift + m -> func(master)}
key = {super|shift + s -> func(stack)}
key = {super|shift + d -> func(default)}
key = {super|shift + k -> func(traverse_up)}
key = {super|shift + j -> func(traverse_down)}
key = {super|shift + f -> func(flip)}
```
### Key Bindings:
- The format for defining key bindings is: `key = {modifier + key -> action}`
- If two modifiers are used, combine them with a pipe (|). For example, alt + shift is written as `alt|shift`.

#### Available Actions:
- run(...): Executes a specified process.
   - Example: `key = {super + return -> run("alacritty")}`
   - To run a process with arguments, use a list:
     Example: `key = {super + p -> run(["rofi", "-show", "drun"])}`

- func(...): Calls a predefined function. The following functions are available:
   - kill: Kills the focused window.
   - switch_desktop: Switches to a specified virtual desktop.
   - grow: Increases the size of the focused window.
   - shrink: Decreases the size of the focused window.
   - fullscreen: Toggles fullscreen mode for the focused window.
   - swap: Swaps the focused window with its sibling.
   - transfer_node: Moves the focused window to another virtual desktop.
   - master: Toggles the master layout and sets the focused window as the master window.
   - stack: Toggles the stacking layout and sets the focused window as the top window.
   - default: Toggles the default layout.
   - traverse_up: (In stack layout only) Moves focus to the window above.
   - traverse_down: (In stack layout only) Moves focus to the window below.
   - flip: Changes the window's orientation; if the window is primarily vertical, it becomes horizontal, and vice versa.
     

More options will be added in the future as development progresses.

## Default Keybindings
| Key            | Description |
| ---------------| ----------- |
| `super + w`       | kill/close window |
| `super + return`       | launches a terminal (alacritty)                            |
| `super + space`       | launches dmenu |
| `super + p `      | launches rofi |
| `super + [desktop number 1..N]`            | switches the desktop |
| `super + l`            | resize window (grow/expand) |
| `super + h`            | resize window (shrink) |
| `super + f`            | toggles fullscreen |
| `super + shift + [desktop number 1..N]`          | transfers window to a diff desktop |
| `super + shift + m`        | toggles master layout |
| `super + shift + s`            | toggles stack layout |
| `super + shift + d`            | toggles default layout |
| `super + shift + j/k`            | traverse the stack |
| `super + shift + f`            | flips the window/partion |
| `super + s`            | swaps window's orientation |

## ewmh specific settings for polyabr
### To display the window name (CLASS_NAME):
``` ini
[module/xwindow]
type 			= internal/xwindow
format 			= <label>
; Available tokens:
;   %title%
;   %instance% (first part of the WM_CLASS atom, new in version 3.7.0)
;   %class%    (second part of the WM_CLASS atom, new in version 3.7.0)
; Default: %title%
label 			= %title%
label-maxlen 	= 50
label-empty 	= "[null]"
```
### To display workspaces:
```ini
[module/ewmh]
type = internal/xworkspaces
label-active 				= %index%
label-active-background 	= ${colors.bg}
label-active-underline		= ${colors.blue}
label-active-padding		= 1
label-occupied 				= %index%
label-occupied-padding 		= 1
label-urgent 				= %index%!
label-urgent-background 	= ${colors.red}
label-urgent-padding 		= 1
label-empty 				= %index%
label-empty-foreground 		= ${colors.gray}
label-empty-padding 		= 1
label-separator 			= " "
```
For further custmization please refer to polybar's wiki.

## Contributing
If you would like to add a feature or to fix a bug please feel free to send a PR.
