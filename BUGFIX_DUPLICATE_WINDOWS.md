# Bug Fix: Duplicate Window Issue When Opening Links

## Problem Description

When opening a link from VSCode (desktop 1) that targets Firefox (desktop 0), the following unexpected behavior occurred:

1. The window manager switched to desktop 0 (correct behavior)
2. Firefox appeared to **also** be on desktop 1 (where VSCode is)
3. An identical "ghost" Firefox window was visible on desktop 0
4. Both Firefox windows appeared to be the same - killing one killed both
5. This created the illusion of having two Firefox windows when only one X11 window actually existed

## Root Cause: The Real Issue

**The bug was entirely in `handle_map_request()` - the desktop switching behavior was actually correct.**

### What Should Happen (Normal Flow)

1. User clicks link in VSCode (desktop 1)
2. Firefox (on desktop 0) receives the URL
3. Firefox sends `_NET_ACTIVE_WINDOW` client message
4. WM switches to desktop 0 (this is correct!)
5. Firefox displays the new page on desktop 0
6. Everything works as expected

### What Was Actually Happening (The Bug)

The `handle_map_request()` function had a critical flaw in how it checked for duplicate windows:

**Code location:** `src/zwm.c:5245-5253` (handle_map_request)

```c
// BUGGY CODE - only searched current desktop
if (find_node_by_window_id(curr_monitor->desk->tree, win) != NULL) {
    return 0; // Window already exists
}
```

This check only looked at the **current desktop's tree**, not all desktops.

### The Race Condition Sequence

Here's the exact sequence of events that caused the bug:

1. **User clicks link in VSCode** (on desktop 1)
2. **Firefox receives URL** (Firefox is on desktop 0)
3. **Firefox sends `_NET_ACTIVE_WINDOW`** message
4. **WM calls `handle_net_active_window()`** which initiates `switch_desktop(0)`
5. **During the desktop switch:**
   - `curr_monitor->desk` is still pointing to desktop 1 (hasn't updated yet)
   - `hide_windows(desktop_1_tree)` is called
   - `show_windows(desktop_0_tree)` is called
6. **Firefox sends `MAP_REQUEST`** (to show the new page)
7. **`handle_map_request()` is called** - this is where the bug occurs:
   - `curr_monitor->desk` is **still pointing to desktop 1** (race condition!)
   - The duplicate check looks at desktop 1's tree: `find_node_by_window_id(curr_monitor->desk->tree, win)`
   - Firefox is NOT in desktop 1's tree (it's in desktop 0's tree)
   - **The check fails to detect the duplicate**
   - Firefox gets added to desktop 1's tree as well
8. **Now the same X11 window exists in TWO desktop trees:**
   - One node in desktop 0's tree (original)
   - One node in desktop 1's tree (duplicate)
9. **Desktop switch completes**, `curr_monitor->desk` updates to desktop 0
10. **You see Firefox on desktop 0** (from the original node)
11. **You switch back to desktop 1** and see Firefox there too (from the duplicate node)
12. **Both are the same window** - killing one kills both because they share the same window ID

### Why Did Firefox Send a MAP_REQUEST?

Firefox sent a `MAP_REQUEST` because:
- When it receives a URL to open, it may need to raise/show its window
- Even if the window is already mapped, applications often send map requests to ensure visibility
- This is normal X11 behavior - applications request to be mapped when they want to be visible

## Solution: Fix the Duplicate Check

The fix was simple but critical: change `handle_map_request()` to check **all desktops** instead of just the current one.

**Code location:** `src/zwm.c:5245-5253` (handle_map_request)

### Before (Buggy):
```c
// Only checked current desktop - WRONG!
if (find_node_by_window_id(curr_monitor->desk->tree, win) != NULL) {
    return 0;
}
```

### After (Fixed):
```c
// Check ALL desktops - CORRECT!
if (client_exist_in_desktops(win)) {
    _LOG_(DEBUG,
          "[MAP_REQUEST] win %d already exists in a desktop, ignoring duplicate map request",
          win);
    return 0;
}
```

The `client_exist_in_desktops()` function searches through **all monitors and all desktops** to find if a window already exists anywhere in the window manager's tree structures.

## Result

- Desktop switching works correctly (switches to desktop 0 when Firefox requests activation)
- No duplicate/ghost windows are created
- Firefox stays in exactly one desktop tree
- Opening links from other applications works as expected
- Each X11 window exists in exactly one desktop tree node

## Key Insight

The bug was a **race condition** between:
1. `switch_desktop()` updating `curr_monitor->desk`
2. `handle_map_request()` checking for duplicates using `curr_monitor->desk`

When the map request arrived during the desktop switch, `curr_monitor->desk` was still pointing to the old desktop, causing the duplicate check to look in the wrong place.

The fix eliminates this race condition by checking **all desktops** regardless of which one is currently active.

## Files Modified

- `src/zwm.c` (lines 5245-5253): `handle_map_request()` - Fixed duplicate window check

## Date

December 6, 2025
