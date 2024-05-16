### zwm

## About ZWM
zwm is a minimalistic and opinionated tiling window manager for X. It uses XCB instead of Xlib to communicate with the X server. The underlying data structure for managing windows is a customized BSP tree. zwm operates with a memory footprint of only ~2 MB.

## Motivation
The motivation behind zwm stems from a desire to create a window manager that is both lightweight and highly efficient. While other tiling window managers perform well and offer robust features, zwm was developed primarily as a learning exercise in creating a window manager from scratch.

## Goals
- Minimalism
- Efficiency
- Performance

## Features
- [ewmh](https://specifications.freedesktop.org/wm-spec/wm-spec-1.3.html) compliant
- Multi-layout (default, master, stack, grid)
- Virtual desktops
- Window/partion resize
- Window/partion flip
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
As of now, the following configs are offered:
```conf
border_width = 5
active_border_color = 0x83a598
normal_border_color = 0x83a598
window_gap = 5
```
More options will be added in the future as development progresses.

## Keybindings
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

## ewmh specific settings for polyabr
### To display the window name (CLASS_NAME):
``` conf
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
```conf
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