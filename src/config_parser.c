/*
 * BSD 2-Clause License
 * Copyright (c) 2024, Yazeed Alharthi
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *	  1. Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 *
 *	  2. Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config_parser.h"
#include "helper.h"
#include "tree.h"
#include "type.h"
#include "zwm.h"
#include <X11/keysym.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <xcb/xproto.h>

#define MAX_LINE_LENGTH (2 << 7)
#define MAX_KEYBINDINGS 45

#ifdef __LTEST__
#define CONF_PATH "./zwm.conf"
#else
#define CONF_PATH ".config/zwm/zwm.conf"
#endif
#define ALT	  XCB_MOD_MASK_1
#define SUPER XCB_MOD_MASK_4
#define SHIFT XCB_MOD_MASK_SHIFT
#define CTRL  XCB_MOD_MASK_CONTROL

typedef enum {
	WHITE_SPACE,
	CURLY_BRACKET,
	PARENTHESIS,
	SQUARE_BRACKET,
	QUOTATION
} trim_token_t;

_key__t **conf_keys = NULL;
int		  _entries_ = 0;

// clang-format off
static conf_mapper_t _cmapper_[] = { 
 	{"run", 					 exec_process}, 
 	{"kill", 		    close_or_kill_wrapper}, 
 	{"switch_desktop", switch_desktop_wrapper}, 
 	{"resize", 		horizontal_resize_wrapper}, 
 	{"fullscreen", 	   set_fullscreen_wrapper}, 
 	{"swap", 				swap_node_wrapper}, 
 	{"transfer_node", 	transfer_node_wrapper}, 
 	{"layout", 				   layout_handler}, 
 	{"traverse",       traverse_stack_wrapper}, 
	{"flip", 	            flip_node_wrapper},
	{"cycle_window", 	    cycle_win_wrapper},
	{"reload_config",   reload_config_wrapper},
	{"cycle_desktop",   cycle_desktop_wrapper}
}; 

static key_mapper_t	 _kmapper_[] = { 
 	{"0", 0x0030}, {"1", 0x0031},
	{"2", 0x0032}, {"3", 0x0033},
	{"4", 0x0034}, {"5", 0x0035}, 
 	{"6", 0x0036}, {"7", 0x0037},
	{"8", 0x0038}, {"9", 0x0039},
	{"a", 0x0061}, {"b", 0x0062}, 
	{"c", 0x0063}, {"d", 0x0064},
	{"e", 0x0065}, {"f", 0x0066},
	{"g", 0x0067}, {"h", 0x0068}, 
 	{"i", 0x0069}, {"j", 0x006a},
	{"k", 0x006b}, {"l", 0x006c},
	{"m", 0x006d}, {"n", 0x006e}, 
	{"o", 0x006f}, {"p", 0x0070},
	{"q", 0x0071}, {"r", 0x0072},
	{"s", 0x0073}, {"t", 0x0074}, 
	{"u", 0x0075}, {"v", 0x0076},
	{"w", 0x0077}, {"x", 0x0078},
	{"y", 0x0079}, {"z", 0x007a}, 
	{"space", 	   	     0x0020},
	{"return", 	         0xff0d},
	{"left", 	   	     0xff51},
	{"up", 	   	         0xff52},
	{"right", 	         0xff53}, 
	{"down", 	         0xff54}, 
 	{"super",             SUPER}, 
 	{"alt",                 ALT}, 
 	{"ctrl",    		   CTRL}, 
 	{"shift", 		      SHIFT}, 
    {"sup+sh",      SUPER|SHIFT}, 
};
// clang-format on

int (*str_to_func(char *ch))(arg_t *)
{
	int n = sizeof(_cmapper_) / sizeof(_cmapper_[0]);
	for (int i = 0; i < n; i++) {
		if (strcmp(_cmapper_[i].func_name, ch) == 0) {
			return _cmapper_[i].function_ptr;
		}
	}
	return NULL;
}

char *
func_to_str(int (*ptr)(arg_t *))
{
	int n = sizeof(_cmapper_) / sizeof(_cmapper_[0]);
	for (int i = 0; i < n; i++) {
		if (_cmapper_[i].function_ptr == ptr) {
			return _cmapper_[i].func_name;
		}
	}
	return NULL;
}

uint32_t
str_to_key(char *ch)
{
	int n = sizeof(_kmapper_) / sizeof(_kmapper_[0]);
	for (int i = 0; i < n; i++) {
		if (strcmp(_kmapper_[i].key, ch) == 0) {
			return _kmapper_[i].keysym;
		}
	}

	return -1;
}

char *
key_to_str(uint32_t val)
{
	int n = sizeof(_kmapper_) / sizeof(_kmapper_[0]);
	for (int i = 0; i < n; i++) {
		if (_kmapper_[i].keysym == val) {
			return _kmapper_[i].key;
		}
	}

	return NULL;
}

int
file_exists(const char *filename)
{
	FILE *file = fopen(filename, "r");
	if (file != NULL) {
		fclose(file);
		return -1;
	}
	return 0;
}

void
print_key_array(void)
{
	for (int i = 0; i < _entries_; i++) {
		if (conf_keys[i]->arg != NULL) {
			if (conf_keys[i]->arg->cmd != NULL) {
				for (int j = 0; j < conf_keys[i]->arg->argc; ++j) {
					_LOG_(DEBUG, "cmd = %s", conf_keys[i]->arg->cmd[j]);
				}
			}
			_LOG_(DEBUG,
				  "key %d = { \n mod = %s \n keysym = %s, func = %s, "
				  "\nargs = {.idx = %d, .d = %d, .r = %d, .t = %d}",
				  i,
				  key_to_str(conf_keys[i]->mod),
				  key_to_str(conf_keys[i]->keysym),
				  func_to_str(conf_keys[i]->function_ptr),
				  conf_keys[i]->arg->idx,
				  conf_keys[i]->arg->d,
				  conf_keys[i]->arg->r,
				  conf_keys[i]->arg->t,
				  conf_keys[i]->arg->t);
		}
	}
}

int
write_default_config(const char *filename, config_t *c)
{
	// clang-format off
const char *content =
    "; ----------- ZWM Configuration File -------------\n"
    ";\n"
    "; This configuration file is used to customize the Zee Window Manager (ZWM) settings.\n"
    "; It allows you to define window appearance, behavior, and key bindings for various actions.\n"
    "\n"
    "; Command to run on startup\n"
    "; Use the exec directive to specify programs that should be started when ZWM is launched.\n"
    "; For single command: use -> exec = \"command\"\n"
    "; You can also specify additional arguments as a list.\n"
    "exec = [\"polybar\", \"-c\", \".config/polybar/config.ini\"]\n"
    "\n"
    "; Available Variables:\n"
    "; These variables control the appearance and behavior of the window manager.\n"
    ";\n"
    "; - border_width: defines the width of the window borders in pixels.\n"
    "border_width = 2\n"
    "; - active_border_color: specifies the color of the border for the active (focused) window.\n"
    "active_border_color = 0x83a598\n"
    "; - normal_border_color: specifies the color of the border for inactive (unfocused) windows.\n"
    "normal_border_color = 0x30302f\n"
    "; - window_gap: sets the gap between windows in pixels.\n"
    "window_gap = 10\n"
    "; - virtual_desktops: sets the number of virtual desktops available.\n"
    "virtual_desktops = 7\n"
    "; - focus_follow_pointer: If false, the window is focused on click.\n"
    ";                        If true, the window is focused when the cursor enters it.\n"
    "focus_follow_pointer = true\n"
    "\n"
    "; Key Bindings:\n"
    "; Define keyboard shortcuts to perform various actions.\n"
    "; The syntax for defining key bindings is:\n"
    "; bind = modifier + key -> action/function\n"
    "; If two modifiers are used, combine them with a pipe (|). For example, alt + shift is written as alt|shift.\n"
    "; Colon Syntax:\n"
    "; Some functions require additional arguments to specify details of the action.\n"
    "; These arguments are provided using a colon syntax, where the function and its argument\n"
    "; are separated by a colon.\n"
    "; Example: func(switch_desktop:1) means \"switch to desktop 1\".\n"
    "; Example: func(resize:grow) means \"grow the size of the window\".\n"
    "; Example: func(layout:master) means \"toggle master layout\".\n"
    "\n"
    "; Available Modifiers:\n"
    "; - super: The \"Windows\" key or \"Command\" key on a Mac.\n"
    "; - alt: The \"Alt\" key.\n"
    "; - shift: The \"Shift\" key.\n"
    "; - ctr: The \"Control\" key.\n"
    ";\n"
    "; Available Actions:\n"
    "; These actions specify what happens when a key binding is triggered.\n"
    "; - run(...): Executes a specified process.\n"
    ";   - Example: bind = super + return -> run(\"alacritty\")\n"
    ";   - To run a process with arguments, use a list:\n"
    ";     Example: bind = super + p -> run([\"rofi\", \"-show\", \"drun\"])\n"
    ";\n"
    "; - func(...): Calls a predefined function. The following functions are available:\n"
    ";   - kill: Kills the focused window.\n"
    ";   - switch_desktop: Switches to a specified virtual desktop.\n"
    ";   - fullscreen: Toggles fullscreen mode for the focused window.\n"
    ";   - swap: Swaps the focused window with its sibling.\n"
    ";   - transfer_node: Moves the focused window to another virtual desktop.\n"
    ";   - layout: Toggles the specified layout (master, default, stack).\n"
    ";   - traverse: (In stack layout only) Moves focus to the window above or below.\n"
    ";   - flip: Changes the window's orientation; if the window is primarily vertical, it becomes horizontal, and vice versa.\n"
    ";   - cycle_window: Moves focus to the window in the specified direction (up, down, left, right).\n"
    ";   - cycle_desktop: Cycles through the virtual desktops (left, right).\n"
    ";   - resize: Adjusts the size of the focused window (grow, shrink).\n"
    ";   - reload_config: Reloads the configuration file without restarting ZWM.\n"
    "\n"
    "; Define key bindings\n"
    "\n"
    "; run processes on some keys\n"
    "bind = super + return -> run(\"alacritty\")\n"
    "bind = super + space -> run(\"dmenu_run\")\n"
    "bind = super + p -> run([\"rofi\", \"-show\", \"drun\"])\n"
    "\n"
    "; kill the focused window\n"
    "bind = super + w -> func(kill)\n"
    "\n"
    "; switch to specific virtual desktops\n"
    "bind = super + 1 -> func(switch_desktop:1)\n"
    "bind = super + 2 -> func(switch_desktop:2)\n"
    "bind = super + 3 -> func(switch_desktop:3)\n"
    "bind = super + 4 -> func(switch_desktop:4)\n"
    "bind = super + 5 -> func(switch_desktop:5)\n"
    "bind = super + 6 -> func(switch_desktop:6)\n"
    "bind = super + 7 -> func(switch_desktop:7)\n"
    "\n"
    "; resize the focused window\n"
    "bind = super + l -> func(resize:grow)\n"
    "bind = super + h -> func(resize:shrink)\n"
    "\n"
    "; toggle fullscreen mode\n"
    "bind = super + f -> func(fullscreen)\n"
    "\n"
    "; swap the focused window with its sibling\n"
    "bind = super + s -> func(swap)\n"
    "\n"
    "; cycle focus between windows\n"
    "bind = super + up -> func(cycle_window:up)\n"
    "bind = super + right -> func(cycle_window:right)\n"
    "bind = super + left -> func(cycle_window:left)\n"
    "bind = super + down -> func(cycle_window:down)\n"
    "\n"
    "; cycle through virtual desktops\n"
    "bind = super|shift + left -> func(cycle_desktop:left)\n"
    "bind = super|shift + right -> func(cycle_desktop:right)\n"
    "\n"
    "; transfer the focused window to another virtual desktop\n"
    "bind = super|shift + 1 -> func(transfer_node:1)\n"
    "bind = super|shift + 2 -> func(transfer_node:2)\n"
    "bind = super|shift + 3 -> func(transfer_node:3)\n"
    "bind = super|shift + 4 -> func(transfer_node:4)\n"
    "bind = super|shift + 5 -> func(transfer_node:5)\n"
    "bind = super|shift + 6 -> func(transfer_node:6)\n"
    "bind = super|shift + 7 -> func(transfer_node:7)\n"
    "\n"
    "; change the layout\n"
    "bind = super|shift + m -> func(layout:master)\n"
    "bind = super|shift + s -> func(layout:stack)\n"
    "bind = super|shift + d -> func(layout:default)\n"
    "\n"
    "; traverse the stack layout\n"
    "bind = super|shift + k -> func(traverse:up)\n"
    "bind = super|shift + j -> func(traverse:down)\n"
    "\n"
    "; flip the orientation of the window\n"
    "bind = super|shift + f -> func(flip)\n"
    "\n"
    "; reload the configuration file\n"
    "bind = super|shift + r -> func(reload_config)\n"
    "\n";

	// clang-format on
	char dir_path[strlen(filename) + 1];
	strcpy(dir_path, filename);
	char *last_slash = strrchr(dir_path, '/');
	if (last_slash != NULL) {
		*last_slash = '\0';
		struct stat st;
		if (stat(dir_path, &st) == -1) {
			if (mkdir(dir_path, 0777) == -1) {
				_LOG_(ERROR, "Failed to create directory: %s\n", dir_path);
				return -1;
			}
		}
	}

	FILE *file = fopen(filename, "w");
	if (file == NULL) {
		_LOG_(ERROR, "Failed to create config file: %s\n", filename);
		return -1;
	}

	size_t len	   = strlen(content);
	size_t written = fwrite(content, 1, len, file);
	if (written != len) {
		fclose(file);
		_LOG_(ERROR, "Error writing to file: %s\n", filename);
		return -1;
	}

	c->active_border_color	= 0x83a598;
	c->normal_border_color	= 0x30302f;
	c->border_width			= 2;
	c->window_gap			= 10;
	c->virtual_desktops		= 7;
	c->focus_follow_pointer = true;

	fclose(file);
	return 0;
}

void
trim(char *str, trim_token_t t)
{
	if (str == NULL) {
		return;
	}

	char *end	= str + strlen(str) - 1;
	char *start = str;
	char  start_token, end_token;

	switch (t) {
	case WHITE_SPACE: {
		start_token = ' ';
		end_token	= ' ';
		break;
	}
	case CURLY_BRACKET: {
		start_token = '{';
		end_token	= '}';
		break;
	}
	case PARENTHESIS: {
		start_token = '(';
		end_token	= ')';
		break;
	}
	case SQUARE_BRACKET: {
		start_token = '[';
		end_token	= ']';
		break;
	}
	case QUOTATION: {
		start_token = '"';
		end_token	= '"';
		break;
	}
	default: return;
	}

	while (end >= str &&
		   (*end == end_token || (t == WHITE_SPACE && isspace(*end)))) {
		*end = '\0';
		end--;
	}

	while (*start == start_token ||
		   (t == WHITE_SPACE && isspace(*start))) {
		start++;
	}

	if (start != str) {
		memmove(str, start, strlen(start) + 1);
	}
}

char **
split_string(const char *str, char delimiter, int *count)
{
	int i		   = 0;
	int num_tokens = 1;

	for (i = 0; str[i] != '\0'; i++) {
		if (str[i] == delimiter) {
			num_tokens++;
		}
	}

	char **tokens = (char **)malloc(num_tokens * sizeof(char *));
	if (tokens == NULL) {
		_LOG_(ERROR, "failed to allocate memory");
		exit(EXIT_FAILURE);
	}
	char *str_copy = strdup(str);
	if (str_copy == NULL) {
		_LOG_(ERROR, "failed to duplicate string");
		exit(EXIT_FAILURE);
	}

	char *token = strtok(str_copy, &delimiter);
	i			= 0;
	while (token != NULL) {
		tokens[i] = strdup(token);
		if (tokens[i] == NULL) {
			_LOG_(ERROR, "failed to duplicate token");
			exit(EXIT_FAILURE);
		}
		i++;
		token = strtok(NULL, &delimiter);
	}
	*count = num_tokens;
	free(str_copy);
	return tokens;
}

void
free_tokens(char **tokens, int count)
{
	for (int i = 0; i < count; i++) {
		free(tokens[i]);
	}
	free(tokens);
}

bool
key_exist(_key__t *key)
{
	for (int i = 0; i < _entries_; i++) {
		if (conf_keys[i]->function_ptr == key->function_ptr &&
			conf_keys[i]->keysym == key->keysym) {
			return true;
		}
	}

	return false;
}

char *
extract_func_body(const char *str)
{
	const char *start = strchr(str, '(');
	if (!start) {
		return NULL;
	}

	const char *end = strchr(start, ')');
	if (!end) {
		return NULL;
	}
	size_t length = end - start + 1;
	char  *result = (char *)malloc(length + 1);
	if (!result) {
		_LOG_(ERROR, "failed to allocate memory");
		exit(EXIT_FAILURE);
	}

	strncpy(result, start, length);
	result[length] = '\0';
	return result;
}

uint32_t
parse_mod_key(char *mod)
{
#ifdef _DEBUG__
	_LOG_(DEBUG, "recieved mod key = (%s)", mod);
#endif
	uint32_t _mod = str_to_key(mod);
	uint32_t mask = -1;
	if ((int)_mod == -1) {
		int	   count;
		char **mods = split_string(mod, '|', &count);
		if (mods == NULL) {
			_LOG_(ERROR, "failed to split string %s\n", mod);
			return -1;
		}
		// #ifdef _DEBUG__
		// 		_LOG_(DEBUG,
		// 			  "mod (%s) splited into (%s), (%s)\n",
		// 			  mod,
		// 			  mods[0],
		// 			  mods[1]);
		// #endif
		uint32_t mask1 = str_to_key(mods[0]);
		uint32_t mask2 = str_to_key(mods[1]);
		mask		   = mask1 | mask2;
		free_tokens(mods, count);
	} else {
		mask = _mod;
	}
	return mask;
}

uint32_t
parse_keysym(char *keysym)
{
	uint32_t keysym_ = str_to_key(keysym);
	if ((int)keysym_ == -1) {
		_LOG_(ERROR, "failed to find keysym %s\n", keysym);
		return -1;
	}

	return keysym_;
}

void
err_cleanup(_key__t *k)
{
	if (k) {
		if (k->arg->cmd) {
			for (int i = 0; i < k->arg->argc; i++) {
				free(k->arg->cmd[i]);
				k->arg->cmd[i] = NULL;
			}
			free(k->arg->cmd);
		}
		free(k->arg);
		k->arg = NULL;
		free(k);
		k = NULL;
	}
}

void
build_run_func(char	   *func_param,
			   _key__t *key,
			   uint32_t mod,
			   uint32_t keysym)
{
	key->mod	= mod;
	key->keysym = (xcb_keysym_t)keysym;
	if (strchr(func_param, '[')) {
		trim(func_param, SQUARE_BRACKET);
		int	   count = 0;
		char **args	 = split_string(func_param, ',', &count);
		if (args == NULL) {
			_LOG_(ERROR, "failed to split string %s", func_param);
			return;
		}
		char **arr = (char **)malloc(count * sizeof(char *));
		for (int i = 0; i < count; i++) {
			arr[i] = strdup(args[i]);
			trim(arr[i], WHITE_SPACE);
			trim(arr[i], QUOTATION);
		}
		free(args);
		key->arg->cmd  = arr;
		key->arg->argc = count;
	} else {
		trim(func_param, WHITE_SPACE);
		trim(func_param, QUOTATION);
		char **arr = (char **)malloc(1 * sizeof(char *));
		arr[0]	   = strdup(func_param);
		if (arr[0] == NULL) {
			_LOG_(ERROR, "failed to duplicate token");
			return;
		}
		key->arg->cmd  = arr;
		key->arg->argc = 1;
	}
}

void
set_key_args(_key__t *key, char *func, char *arg)
{
	if (strcmp(func, "cycle_window") == 0) {
		if (strcmp(arg, "up") == 0) {
			key->arg->d = UP;
		} else if (strcmp(arg, "right") == 0) {
			key->arg->d = RIGHT;
		} else if (strcmp(arg, "left") == 0) {
			key->arg->d = LEFT;
		} else if (strcmp(arg, "down") == 0) {
			key->arg->d = DOWN;
		}
	} else if (strcmp(func, "layout") == 0) {
		if (strcmp(arg, "master") == 0) {
			key->arg->t = MASTER;
		} else if (strcmp(arg, "default") == 0) {
			key->arg->t = DEFAULT;
		} else if (strcmp(arg, "grid") == 0) {
			key->arg->t = GRID;
		} else if (strcmp(arg, "stack") == 0) {
			key->arg->t = STACK;
		}
	} else if (strcmp(func, "cycle_desktop") == 0) {
		if (strcmp(arg, "left") == 0) {
			key->arg->d = LEFT;
		} else if (strcmp(arg, "right") == 0) {
			key->arg->d = RIGHT;
		}
	} else if (strcmp(func, "resize") == 0) {
		if (strcmp(arg, "grow") == 0) {
			key->arg->r = GROW;
		} else if (strcmp(arg, "shrink") == 0) {
			key->arg->r = SHRINK;
		}
	} else if (strcmp(func, "switch_desktop") == 0) {
		char *_num = key_to_str(key->keysym);
		int	  idx  = atoi(_num);
		idx--;
		key->arg->idx = idx;
	} else if (strcmp(func, "transfer_node") == 0) {
		char *_num = key_to_str(key->keysym);
		int	  idx  = atoi(_num);
		idx--;
		key->arg->idx = idx;
	} else if (strcmp(func, "traverse") == 0) {
		if (strcmp(arg, "up") == 0) {
			key->arg->d = UP;
		} else if (strcmp(arg, "down") == 0) {
			key->arg->d = DOWN;
		}
	}
}

int
construct_key(char *mod, char *keysym, char *func, _key__t *key)
{
	bool	 run_func	= false;
	uint32_t _keysym	= -1;
	uint32_t _mod		= -1;
	int (*ptr)(arg_t *) = NULL;

	// parse mod key
	_mod				= parse_mod_key(mod);
	if ((int)_mod == -1) {
		_LOG_(
			ERROR, "failed to parse mod key for %s, func %s\n", mod, func);
		return -1;
	}

	// parse keysym if not null
	if (keysym != NULL) {
		_keysym = parse_keysym(keysym);
		if ((int)_keysym == -1) {
			_LOG_(ERROR, "failed to parse keysym for %s\n", keysym);
			return -1;
		}
	} else {
		_LOG_(INFO,
			  "keysym is null, func must be switch or transfer %s\n",
			  func);
	}

	if (strncmp(func, "run", 3) == 0) {
		run_func = true;
		_LOG_(INFO, "found run func %s, ...\n", func);
	}

	char *func_param = extract_func_body(func);
	if (func_param == NULL) {
		_LOG_(ERROR, "failed to extract func body for %s\n", func);
		return -1;
	}

	trim(func_param, PARENTHESIS);

	if (strchr(func_param, ':')) {
		int	   count = 0;
		char **s	 = split_string(func_param, ':', &count);
		if (s == NULL || count != 2) {
			_LOG_(ERROR,
				  "failed to split string or incorrect count for %s\n",
				  func_param);
			free(func_param);
			if (s != NULL)
				free(s);
			return -1;
		}

		char *f = strdup(s[0]);
		char *a = strdup(s[1]);
		ptr		= str_to_func(f);
		if (ptr == NULL) {
			_LOG_(ERROR, "failed to find function pointer for %s", f);
			free(func_param);
			free(s);
			return -1;
		}
		key->mod		  = _mod;
		key->keysym		  = _keysym;
		key->function_ptr = ptr;

		set_key_args(key, f, a);
		free_tokens(s, count);
		free(f);
		free(a);
		free(func_param);
		return 0;
	}

	// handle "run" functions
	if (run_func) {
		ptr = str_to_func("run");
		if (ptr == NULL) {
			_LOG_(ERROR,
				  "failed to find run func pointer for %s",
				  func_param);
			free(func_param);
			return -1;
		}
		build_run_func(func_param, key, _mod, _keysym);
		key->function_ptr = ptr;
		free(func_param);
		return 0;
	}

	// handle other functions
	ptr = str_to_func(func_param);
	if (ptr == NULL) {
		_LOG_(ERROR, "failed to find function pointer for %s", func_param);
		free(func_param);
		return -1;
	}

	key->mod		  = _mod;
	key->keysym		  = _keysym;
	key->function_ptr = ptr;
	free(func_param);
	return 0;
}

int
parse_keybinding(char *str, _key__t *key)
{
	if (strstr(str, "->") == NULL) {
		_LOG_(ERROR, "invalide key format %s ", str);
		return -1;
	}

	// bool  keysym_exists = strchr(str, '+') != NULL;
	char plus		   = '+';
	bool keysym_exists = false;
	int	 i			   = 0;
	while (str[i] != '\0') {
		if (str[i] == plus) {
			keysym_exists = !keysym_exists;
			break;
		}
		i++;
	}

	char *mod	 = NULL;
	char *keysym = NULL;
	char *func	 = NULL;

	if (keysym_exists) {
		char *plus_token = strtok(str, "+");
		mod				 = plus_token ? plus_token : NULL;
		plus_token		 = strtok(NULL, "->");
		keysym			 = plus_token ? plus_token : NULL;
		func			 = strtok(NULL, "");
		if (mod)
			trim(mod, WHITE_SPACE);
		if (keysym)
			trim(keysym, WHITE_SPACE);
		if (func) {
			func++;
			trim(func, WHITE_SPACE);
		}
	} else {
		char *arrow_token = strtok(str, "->");
		mod				  = arrow_token ? arrow_token : NULL;
		func			  = strtok(NULL, "");
		if (mod)
			trim(mod, WHITE_SPACE);
		if (func) {
			func++;
			trim(func, WHITE_SPACE);
		}
	}
	return construct_key(mod, keysym, func, key);
}

_key__t *
init_key(void)
{
	_key__t *key = (_key__t *)calloc(1, sizeof(_key__t));
	arg_t	*a	 = (arg_t *)calloc(1, sizeof(arg_t));

	if (a == NULL || key == NULL) {
		_LOG_(ERROR, "failed to calloc _key__t or arg_t");
		return NULL;
	}

	key->arg = a;

	return key;
}

void
handle_exec_cmd(char *cmd)
{
	_LOG_(DEBUG, "exec command = (%s)", cmd);
	pid_t pid = fork();

	if (pid == 0) {
		if (strchr(cmd, ',') != 0) {
			trim(cmd, SQUARE_BRACKET);
			int	   count = 0;
			char **s	 = split_string(cmd, ',', &count);
			if (s == NULL)
				return;
			const char *args[count + 1];
			for (int i = 0; i < count; ++i) {
				trim(s[i], WHITE_SPACE);
				trim(s[i], QUOTATION);
				args[i] = s[i];
				_LOG_(INFO, "arg exec = %s", s[i]);
			}
			args[count] = NULL;
			execvp(args[0], (char *const *)args);
			free_tokens(s, count);
			_LOG_(ERROR, "execvp failed");
			exit(EXIT_FAILURE);
		} else {
			trim(cmd, QUOTATION);
			execlp(cmd, cmd, (char *)NULL);
			_LOG_(ERROR, "execlp failed");
			exit(EXIT_FAILURE);
		}
	} else if (pid < 0) {
		_LOG_(ERROR, "fork failed");
		exit(EXIT_FAILURE);
	}
}

int
parse_config(const char *filename, config_t *c, bool reload)
{
	FILE *file = fopen(filename, "r");
	if (file == NULL) {
		_LOG_(ERROR, "Error: Could not open file '%s'\n", filename);
		return -1;
	}

	char line[MAX_LINE_LENGTH];
	while (fgets(line, MAX_LINE_LENGTH, file) != NULL) {
		char *key	= strtok(line, "=");
		char *value = strtok(NULL, "\n");
		if (*key == ';') {
			continue;
		}
		trim(key, WHITE_SPACE);
		trim(value, WHITE_SPACE);
#ifdef _DEBUG__
		_LOG_(DEBUG,
			  "config line = (%s) key = (%s) value = (%s)\n",
			  line,
			  key,
			  value);
#endif
		if (key == NULL || value == NULL) {
			continue;
		}

		if (reload && strcmp(key, "bind") == 0) {
			continue;
		}

		if (strcmp(key, "exec") == 0) {
			handle_exec_cmd(value);
		} else if (strcmp(key, "border_width") == 0) {
			c->border_width = atoi(value);
		} else if (strcmp(key, "active_border_color") == 0) {
			c->active_border_color =
				(unsigned int)strtoul(value, NULL, 16);
		} else if (strcmp(key, "normal_border_color") == 0) {
			c->normal_border_color =
				(unsigned int)strtoul(value, NULL, 16);
		} else if (strcmp(key, "window_gap") == 0) {
			c->window_gap = atoi(value);
		} else if (strcmp(key, "virtual_desktops") == 0) {
			c->virtual_desktops = atoi(value);
		} else if (strcmp(key, "focus_follow_pointer") == 0) {
			if (strcmp(value, "true") == 0) {
				c->focus_follow_pointer = true;
			} else if (strcmp(value, "false") == 0) {
				c->focus_follow_pointer = false;
			} else {
				_LOG_(ERROR,
					  "Invalid value for focus_follow_pointer: %s\n",
					  value);
			}
		} else if (strcmp(key, "bind") == 0) {
			if (conf_keys == NULL) {
				conf_keys = (_key__t **)malloc(MAX_KEYBINDINGS *
											   sizeof(_key__t *));
				if (conf_keys == NULL) {
					fprintf(stderr,
							"Failed to allocate memory for conf_keys "
							"array\n");
					return 1;
				}
			}
			_key__t *k = init_key();
			if (parse_keybinding(value, k) != 0) {
				err_cleanup(k);
				free_keys();
				_LOG_(ERROR, "error while parsing keys");
				goto out;
			}
			conf_keys[_entries_] = k;
			_entries_++;
		}
	}

out:
	fclose(file);
	return 0;
}

void
free_keys(void)
{
	if (conf_keys) {
		for (int i = 0; i < _entries_; ++i) {
			if (conf_keys[i]->arg->cmd != NULL) {
				for (int j = 0; j < conf_keys[i]->arg->argc; j++) {
					free(conf_keys[i]->arg->cmd[j]);
					conf_keys[i]->arg->cmd[j] = NULL;
				}
				free(conf_keys[i]->arg->cmd);
				conf_keys[i]->arg->cmd = NULL;
			}
			free(conf_keys[i]->arg);
			conf_keys[i]->arg = NULL;
			free(conf_keys[i]);
			conf_keys[i] = NULL;
		}
		free(conf_keys);
		conf_keys = NULL;
		_entries_ = -1;
	}
}

int
reload_config(config_t *c)
{

	const char *filename = CONF_PATH;
	return parse_config(filename, c, true);
}

int
load_config(config_t *c)
{
	const char *filename = CONF_PATH;
	return !file_exists(filename) ? write_default_config(filename, c)
								  : parse_config(filename, c, false);
}
