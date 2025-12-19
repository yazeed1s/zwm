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
#include "type.h"
#include "zwm.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <xcb/xproto.h>

#define MAX_LINE_LENGTH (2 << 9)
#define MAX_KEYBINDINGS 45

#ifdef __LTEST__
#define CONF_PATH "./zwm.conf"
#define TEMPLATE_PATH "./zwm.conf"
#else
#define CONF_PATH ".config/zwm/zwm.conf"
#define TEMPLATE_PATH "/usr/share/zwm/zwm.conf"
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

rule_t	   *rule_head = NULL;
conf_key_t *key_head  = NULL;

static void
free_tokens(char **, int);

/* clang-format off */
static const conf_mapper_t _cmapper_[] = {
    DEFINE_MAPPING("run",            		 exec_process),
    DEFINE_MAPPING("kill",           		 close_or_kill_wrapper),
    DEFINE_MAPPING("switch_desktop", 		 switch_desktop_wrapper),
    DEFINE_MAPPING("resize",         		 dynamic_resize_wrapper),
    DEFINE_MAPPING("fullscreen",     		 set_fullscreen_wrapper),
    DEFINE_MAPPING("swap",           		 swap_node_wrapper),
    DEFINE_MAPPING("transfer_node",  		 transfer_node_wrapper),
    DEFINE_MAPPING("layout",         		 layout_handler),
    DEFINE_MAPPING("traverse",       		 traverse_stack_wrapper),
    DEFINE_MAPPING("flip",           		 flip_node_wrapper),
    DEFINE_MAPPING("cycle_window",   		 cycle_win_wrapper),
    DEFINE_MAPPING("reload_config",  		 reload_config_wrapper),
    DEFINE_MAPPING("cycle_desktop",  		 cycle_desktop_wrapper),
    DEFINE_MAPPING("cycle_monitors",  		 cycle_monitors),
    DEFINE_MAPPING("shift_window",   		 shift_floating_window),
    DEFINE_MAPPING("grow_floating_window",   grow_floating_window),
    DEFINE_MAPPING("shrink_floating_window", shrink_floating_window),
    DEFINE_MAPPING("gap_handler",    		 gap_handler),
    DEFINE_MAPPING("change_state",  		 change_state),
};

static key_mapper_t _kmapper_[] = {
    DEFINE_MAPPING("0",        0x0030), DEFINE_MAPPING("1",        0x0031),
    DEFINE_MAPPING("2",        0x0032), DEFINE_MAPPING("3",        0x0033),
    DEFINE_MAPPING("4",        0x0034), DEFINE_MAPPING("5",        0x0035),
    DEFINE_MAPPING("6",        0x0036), DEFINE_MAPPING("7",        0x0037),
    DEFINE_MAPPING("8",        0x0038), DEFINE_MAPPING("9",        0x0039),
    DEFINE_MAPPING("a",        0x0061), DEFINE_MAPPING("b",        0x0062),
    DEFINE_MAPPING("c",        0x0063), DEFINE_MAPPING("d",        0x0064),
    DEFINE_MAPPING("e",        0x0065), DEFINE_MAPPING("f",        0x0066),
    DEFINE_MAPPING("g",        0x0067), DEFINE_MAPPING("h",        0x0068),
    DEFINE_MAPPING("i",        0x0069), DEFINE_MAPPING("j",        0x006a),
    DEFINE_MAPPING("k",        0x006b), DEFINE_MAPPING("l",        0x006c),
    DEFINE_MAPPING("m",        0x006d), DEFINE_MAPPING("n",        0x006e),
    DEFINE_MAPPING("o",        0x006f), DEFINE_MAPPING("p",        0x0070),
    DEFINE_MAPPING("q",        0x0071), DEFINE_MAPPING("r",        0x0072),
    DEFINE_MAPPING("s",        0x0073), DEFINE_MAPPING("t",        0x0074),
    DEFINE_MAPPING("u",        0x0075), DEFINE_MAPPING("v",        0x0076),
    DEFINE_MAPPING("w",        0x0077), DEFINE_MAPPING("x",        0x0078),
    DEFINE_MAPPING("y",        0x0079), DEFINE_MAPPING("z",        0x007a),
    DEFINE_MAPPING("space",    0x0020), DEFINE_MAPPING("return",   0xff0d),
    DEFINE_MAPPING("left",     0xff51), DEFINE_MAPPING("up",       0xff52),
    DEFINE_MAPPING("right",    0xff53), DEFINE_MAPPING("down",     0xff54),
    DEFINE_MAPPING("super",     SUPER), DEFINE_MAPPING("alt",        ALT ),
    DEFINE_MAPPING("ctrl",      CTRL ), DEFINE_MAPPING("shift",     SHIFT),
    DEFINE_MAPPING("sup+sh", SUPER | SHIFT),
};
/* clang-format on */

static int (*str_to_func(char *ch))(arg_t *)
{
	int n = sizeof(_cmapper_) / sizeof(_cmapper_[0]);
	for (int i = 0; i < n; i++) {
		if (strcmp(_cmapper_[i].func_name, ch) == 0) {
			return _cmapper_[i].execute;
		}
	}
	return NULL;
}

char *
func_to_str(int (*ptr)(arg_t *))
{
	int n = sizeof(_cmapper_) / sizeof(_cmapper_[0]);
	for (int i = 0; i < n; i++) {
		if (_cmapper_[i].execute == ptr) {
			return _cmapper_[i].func_name;
		}
	}
	return NULL;
}

static uint32_t
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

static char *
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

static int
file_exists(const char *filename)
{
	FILE *file = fopen(filename, "r");
	if (file) {
		fclose(file);
		return -1;
	}
	return 0;
}

static void
print_key_array(void)
{
	conf_key_t *current = key_head;
	int			c		= 0;
	while (current) {
		if (current->arg) {
			if (current->arg->cmd) {
				for (int j = 0; j < current->arg->argc; ++j) {
					_LOG_(DEBUG, "cmd = %s", current->arg->cmd[j]);
				}
			}
			_LOG_(DEBUG,
				  "key %d = { \n mod = %s \n keysym = %s, func = %s, "
				  "\nargs = {.idx = %d, .d = %d, .r = %d, .t = %d}",
				  c,
				  key_to_str(current->mod),
				  key_to_str(current->keysym),
				  func_to_str(current->execute),
				  current->arg->idx,
				  current->arg->d,
				  current->arg->r,
				  current->arg->t,
				  current->arg->t);
		}
		c++;
		current = current->next;
	}
}

static int
write_default_config(const char *filename, config_t *c)
{
	const char *template_path = TEMPLATE_PATH;

	/* Create directory if it doesn't exist */
	char dir_path[strlen(filename) + 1];
	strcpy(dir_path, filename);
	char *last_slash = strrchr(dir_path, '/');
	if (last_slash) {
		*last_slash = '\0';
		struct stat st;
		if (stat(dir_path, &st) == -1) {
			if (mkdir(dir_path, 0777) == -1) {
				_LOG_(ERROR, "failed to create directory: %s", dir_path);
				return -1;
			}
		}
	}

	/* Open template file for reading */
	FILE *template_file = fopen(template_path, "r");
	if (template_file == NULL) {
		_LOG_(ERROR, "failed to open template file: %s", template_path);
		return -1;
	}

	/* Open destination file for writing */
	FILE *dest_file = fopen(filename, "w");
	if (dest_file == NULL) {
		_LOG_(ERROR, "failed to create config file: %s", filename);
		fclose(template_file);
		return -1;
	}

	/* Copy template to destination */
	char buffer[4096];
	size_t bytes;
	while ((bytes = fread(buffer, 1, sizeof(buffer), template_file)) > 0) {
		if (fwrite(buffer, 1, bytes, dest_file) != bytes) {
			_LOG_(ERROR, "error writing to file: %s", filename);
			fclose(template_file);
			fclose(dest_file);
			return -1;
		}
	}

	fclose(template_file);
	fclose(dest_file);

	/* Set default config values */
	c->active_border_color	= 0x4a4a48;
	c->normal_border_color	= 0x30302f;
	c->border_width			= 2;
	c->window_gap			= 10;
	c->virtual_desktops		= 7;
	c->focus_follow_pointer = true;

	return 0;
}

static void
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

	while (*start == start_token || (t == WHITE_SPACE && isspace(*start))) {
		start++;
	}

	if (start != str) {
		memmove(str, start, strlen(start) + 1);
	}
}

/* caller must free using free_tokens(...) */
static char **
split_string(const char *str, char delimiter, int *count)
{
	int i		   = 0;
	int num_tokens = 1;
	for (i = 0; str[i] != '\0'; i++) {
		if (str[i] == delimiter) {
			num_tokens++;
		}
	}

	char **tokens = (char **)malloc((num_tokens + 1) * sizeof(char *));
	if (tokens == NULL) {
		_LOG_(ERROR, "failed to allocate memory");
		return NULL;
	}

	char *str_copy = strdup(str);
	if (str_copy == NULL) {
		_LOG_(ERROR, "failed to duplicate string");
		_FREE_(tokens);
		return NULL;
	}

	char  delim_str[2] = {delimiter, '\0'};
	char *token		   = strtok(str_copy, delim_str);
	i				   = 0;
	while (token && i < num_tokens) {
		tokens[i] = strdup(token);
		if (tokens[i] == NULL) {
			_LOG_(ERROR, "failed to duplicate token");
			_FREE_(str_copy);
			free_tokens(tokens, i);
			return NULL;
		}
		i++;
		token = strtok(NULL, delim_str);
	}
	tokens[i] = NULL;

	*count	  = i;
	_FREE_(str_copy);
	return tokens;
}

static void
free_tokens(char **tokens, int count)
{
	if (tokens) {
		for (int i = 0; i < count; i++) {
			if (tokens[i]) {
				_FREE_(tokens[i]);
			}
		}
	}
	_FREE_(tokens);
}

static bool
key_exist(conf_key_t *key)
{
	conf_key_t *current = key_head;
	while (current) {
		if (current->execute == key->execute &&
			current->keysym == key->keysym) {
			return true;
		}
		current = current->next;
	}

	return false;
}

/* caller must free */
static char *
extract_body(const char *str)
{
	const char *start = strchr(str, '(');
	if (start == NULL) {
		return NULL;
	}

	const char *end = strchr(start, ')');
	if (end == NULL) {
		return NULL;
	}
	size_t length = end - start + 1;
	char  *result = (char *)malloc(length + 1);
	if (result == NULL) {
		_LOG_(ERROR, "failed to allocate memory");
		return NULL;
	}

	strncpy(result, start, length);
	result[length] = '\0';
	return result;
}

static uint32_t
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
			_LOG_(ERROR, "failed to split string %s", mod);
			return -1;
		}
		uint32_t mask1 = str_to_key(mods[0]);
		if ((int)mask1 == -1) {
			_LOG_(ERROR, "failed to find key (%s)", mods[0]);
		}
		uint32_t mask2 = str_to_key(mods[1]);
		if ((int)mask2 == -1) {
			_LOG_(ERROR, "failed to find key (%s)", mods[1]);
		}
		mask = mask1 | mask2;
		free_tokens(mods, count);
	} else {
		mask = _mod;
	}
	return mask;
}

static uint32_t
parse_keysym(char *keysym)
{
	uint32_t keysym_ = str_to_key(keysym);
	if ((int)keysym_ == -1) {
		_LOG_(ERROR, "failed to find keysym %s", keysym);
		return -1;
	}

	return keysym_;
}

static void
err_cleanup(conf_key_t *k)
{
	if (k) {
		if (k->arg) {
			if (k->arg->cmd) {
				for (int i = 0; i < k->arg->argc; i++) {
					_FREE_(k->arg->cmd[i]);
				}
				_FREE_(k->arg->cmd);
			}
			_FREE_(k->arg);
		}
		_FREE_(k);
	}
}

static void
build_run_func(char *func_param, conf_key_t *key, uint32_t mod, uint32_t keysym)
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
		key->arg->cmd = (char **)malloc((count + 1) * sizeof(char *));
		if (key->arg->cmd == NULL) {
			_LOG_(ERROR, "failed to allocate memory for cmd array");
			free_tokens(args, count);
			return;
		}
		key->arg->argc = count;
		for (int i = 0; i < key->arg->argc; i++) {
			trim(args[i], WHITE_SPACE);
			trim(args[i], QUOTATION);
			key->arg->cmd[i] = strdup(args[i]);
			if (key->arg->cmd[i] == NULL) {
				_LOG_(ERROR, "failed to duplicate token");
				free_tokens(args, count);
				return;
			}
		}
		free_tokens(args, count);
	} else {
		trim(func_param, WHITE_SPACE);
		trim(func_param, QUOTATION);
		key->arg->cmd = (char **)malloc(1 * sizeof(char *));
		if (key->arg->cmd == NULL) {
			_LOG_(ERROR, "failed to allocate memory for cmd array");
			return;
		}
		key->arg->cmd[0] = strdup(func_param);
		if (key->arg->cmd[0] == NULL) {
			_LOG_(ERROR, "failed to duplicate token");
			key->arg->cmd = NULL;
			return;
		}
		key->arg->argc = 1;
	}
}

static void
set_key_args(conf_key_t *key, char *func, char *arg)
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
	} else if (strcmp(func, "shift_window") == 0) {
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
	} else if (strcmp(func, "gap_handler") == 0) {
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
	} else if (strcmp(func, "change_state") == 0) {
		if (strcmp(arg, "float") == 0) {
			key->arg->s = FLOATING;
		} else if (strcmp(arg, "tile") == 0) {
			key->arg->s = TILED;
		}
	} else if (strcmp(func, "shrink_floating_window") == 0) {
		if (strcmp(arg, "horizontal") == 0) {
			key->arg->rd = HORIZONTAL_DIR;
		} else if (strcmp(arg, "vertical") == 0) {
			key->arg->rd = VERTICAL_DIR;
		}
	} else if (strcmp(func, "grow_floating_window") == 0) {
		if (strcmp(arg, "horizontal") == 0) {
			key->arg->rd = HORIZONTAL_DIR;
		} else if (strcmp(arg, "vertical") == 0) {
			key->arg->rd = VERTICAL_DIR;
		}
	} else if (strcmp(func, "cycle_monitors") == 0) {
		if (strcmp(arg, "next") == 0) {
			key->arg->tr = NEXT;
		} else if (strcmp(arg, "prev") == 0) {
			key->arg->tr = PREV;
		}
	}
}

static int
construct_key(char *mod, char *keysym, char *func, conf_key_t *key)
{
	bool	 run_func	= false;
	uint32_t _keysym	= -1;
	uint32_t _mod		= -1;
	int (*ptr)(arg_t *) = NULL;

	/* parse mod key */
	_mod				= parse_mod_key(mod);
	if ((int)_mod == -1) {
		_LOG_(ERROR, "failed to parse mod key for %s, func %s", mod, func);
		return -1;
	}

	/* parse keysym if not null */
	if (keysym) {
		_keysym = parse_keysym(keysym);
		if ((int)_keysym == -1) {
			_LOG_(ERROR, "failed to parse keysym for %s", keysym);
			return -1;
		}
	} else {
		_LOG_(INFO, "keysym is null, func must be switch or transfer %s", func);
	}

	if (strncmp(func, "run", 3) == 0) {
		run_func = true;
#ifdef _DEBUG__
		_LOG_(INFO, "found run func %s, ...", func);
#endif
	}

	char *func_param = extract_body(func);
	if (func_param == NULL) {
		_LOG_(ERROR, "failed to extract func body for %s", func);
		return -1;
	}

	trim(func_param, PARENTHESIS);

	if (strchr(func_param, ':')) {
		int	   count = 0;
		char **s	 = split_string(func_param, ':', &count);
		if (s == NULL || count != 2) {
			_LOG_(ERROR,
				  "failed to split string or incorrect count for %s",
				  func_param);
			_FREE_(func_param);
			if (s)
				_FREE_(s);
			return -1;
		}

		char *f = strdup(s[0]);
		char *a = strdup(s[1]);
		ptr		= str_to_func(f);
		if (ptr == NULL) {
			_LOG_(ERROR, "failed to find function pointer for %s", f);
			_FREE_(func_param);
			_FREE_(s);
			return -1;
		}
		key->mod	 = _mod;
		key->keysym	 = _keysym;
		key->execute = ptr;

		set_key_args(key, f, a);
		free_tokens(s, count);
		_FREE_(f);
		_FREE_(a);
		_FREE_(func_param);
		return 0;
	}

	/* handle "run" functions */
	if (run_func) {
		ptr = str_to_func("run");
		if (ptr == NULL) {
			_LOG_(ERROR, "failed to find run func pointer for %s", func_param);
			_FREE_(func_param);
			return -1;
		}
		build_run_func(func_param, key, _mod, _keysym);
		key->execute = ptr;
		_FREE_(func_param);
		return 0;
	}

	/* handle other functions */
	ptr = str_to_func(func_param);
	if (ptr == NULL) {
		_LOG_(ERROR, "failed to find function pointer for %s", func_param);
		_FREE_(func_param);
		return -1;
	}

	key->mod	 = _mod;
	key->keysym	 = _keysym;
	key->execute = ptr;
	_FREE_(func_param);
	return 0;
}

static int
parse_keybinding(char *str, conf_key_t *key)
{
	if (strstr(str, "->") == NULL) {
		_LOG_(ERROR, "invalide key format %s ", str);
		return -1;
	}

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

static conf_key_t *
init_key(void)
{
	conf_key_t *key = (conf_key_t *)calloc(1, sizeof(conf_key_t));
	if (key == NULL) {
		_LOG_(ERROR, "failed to calloc conf_key_t");
		return NULL;
	}

	key->arg = (arg_t *)calloc(1, sizeof(arg_t));
	if (key->arg == NULL) {
		_LOG_(ERROR, "failed to calloc arg_t");
		_FREE_(key);
		return NULL;
	}

	key->arg->cmd  = NULL;
	key->arg->argc = 0;
	key->next	   = NULL;

	return key;
}

static void
add_key(conf_key_t **head, conf_key_t *k)
{
	if (*head == NULL) {
		*head = k;
		return;
	}
	conf_key_t *current = *head;
	while (current->next) {
		current = current->next;
	}
	current->next = k;
}

static rule_t *
init_rule(void)
{
	rule_t *rule = (rule_t *)calloc(1, sizeof(rule_t));

	if (rule == NULL) {
		_LOG_(ERROR, "failed to calloc rule_t");
		return NULL;
	}
	rule->next = NULL;
	return rule;
}

static void
add_rule(rule_t **head, rule_t *r)
{
	if (*head == NULL) {
		*head = r;
		return;
	}
	rule_t *current = *head;
	while (current->next) {
		current = current->next;
	}
	current->next = r;
}

static void
handle_exec_cmd(char *cmd)
{
#ifdef _DEBUG__
	_LOG_(DEBUG, "exec command = (%s)", cmd);
#endif

	pid_t pid = fork();

	if (pid == 0) {
		if (strchr(cmd, ',')) {
			trim(cmd, SQUARE_BRACKET);
			int	   count = 0;
			char **s	 = split_string(cmd, ',', &count);
			if (s == NULL)
				_exit(EXIT_FAILURE);
			const char *args[count + 1];
			for (int i = 0; i < count; ++i) {
				trim(s[i], WHITE_SPACE);
				trim(s[i], QUOTATION);
				args[i] = s[i];
#ifdef _DEBUG__
				_LOG_(INFO, "arg exec = %s", s[i]);
#endif
			}
			args[count] = NULL;
			execvp(args[0], (char *const *)args);
			free_tokens(s, count);
			_LOG_(ERROR, "execvp failed");
			_exit(EXIT_FAILURE);
		} else {
			trim(cmd, QUOTATION);
			execlp(cmd, cmd, (char *)NULL);
			_LOG_(ERROR, "execlp failed");
			_exit(EXIT_FAILURE);
		}
	} else if (pid < 0) {
		_LOG_(ERROR, "fork failed");
		_exit(EXIT_FAILURE);
	}
}

static int
construct_rule(char *class, char *state, char *desktop_number, rule_t *rule)
{
	if (class == NULL || state == NULL || desktop_number == NULL) {
		_LOG_(ERROR, "rules are empty");
		return -1;
	}

	/* wm_class */
	char *c = extract_body(class);
	if (c == NULL) {
		_LOG_(ERROR, "while extracting class rule body (%s)", class);
		return -1;
	}

	trim(c, PARENTHESIS);
	trim(c, QUOTATION);
	uint32_t c_len = strlen(c);
	strncpy(rule->win_name, c, c_len);

	/* w_state */
	char *s = extract_body(state);
	if (s == NULL) {
		_LOG_(ERROR, "while extracting state rule body");
		return -1;
	}
	state_t enum_state = -1;
	trim(s, PARENTHESIS);
	if (strcmp(s, "tiled") == 0) {
		enum_state = TILED;
	} else if (strcmp(s, "floated") == 0) {
		enum_state = FLOATING;
	}
	rule->state = enum_state;

	/* w_desktop */
	char *d		= extract_body(desktop_number);
	if (d == NULL) {
		_LOG_(ERROR, "while extracting desktop rule body");
		return -1;
	}

	trim(d, PARENTHESIS);
	rule->desktop_id = atoi(d);

	_LOG_(INFO,
		  "constructed rule = win name = (%s), state = (%s), desktop = (%d)",
		  rule->win_name,
		  rule->state == TILED ? "TILED" : "FLOATED",
		  rule->desktop_id);
	_FREE_(c);
	_FREE_(s);
	_FREE_(d);

	return 0;
}

rule_t *
get_window_rule(xcb_window_t win)
{
	xcb_icccm_get_wm_class_reply_t t_reply;
	xcb_get_property_cookie_t cn = xcb_icccm_get_wm_class(wm->connection, win);
	const uint8_t			  wr =
		xcb_icccm_get_wm_class_reply(wm->connection, cn, &t_reply, NULL);
	if (wr == 1) {
		rule_t *current = rule_head;
		while (current) {
			if (strcasecmp(current->win_name, t_reply.class_name) == 0) {
				xcb_icccm_get_wm_class_reply_wipe(&t_reply);
				return current;
			}
			current = current->next;
		}
		xcb_icccm_get_wm_class_reply_wipe(&t_reply);
	}
	return NULL;
}

static int
parse_rule(char *value, rule_t *rule)
{
	if (value == NULL) {
		return -1;
	}

	trim(value, WHITE_SPACE);
	int	   count = 0;
	char **rules = split_string(value, ',', &count);
	if (rules == NULL)
		return -1;

	if (count != 3) {
		_LOG_(ERROR, "while splitting window rule");
		free_tokens(rules, count);
		return -1;
	}

	char *win_name	  = rules[0];
	char *win_state	  = rules[1];
	char *win_desktop = rules[2];

	int	  result	  = construct_rule(win_name, win_state, win_desktop, rule);

	free_tokens(rules, count);

	return result;
}

static int
parse_config_line(char *key, char *value, config_t *c, bool reload)
{
	if (strcmp(key, "exec") == 0) {
		if (!reload)
			handle_exec_cmd(value);
	} else if (strcmp(key, "border_width") == 0) {
		c->border_width = atoi(value);
	} else if (strcmp(key, "active_border_color") == 0) {
		c->active_border_color = (unsigned int)strtoul(value, NULL, 16);
	} else if (strcmp(key, "normal_border_color") == 0) {
		c->normal_border_color = (unsigned int)strtoul(value, NULL, 16);
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
			_LOG_(ERROR, "invalid value for focus_follow_pointer: %s", value);
			return -1;
		}
	} else if (strcmp(key, "focus_follow_spawn") == 0) {
		if (strcmp(value, "true") == 0) {
			c->focus_follow_spawn = true;
		} else if (strcmp(value, "false") == 0) {
			c->focus_follow_spawn = false;
		} else {
			_LOG_(ERROR, "invalid value for focus_follow_spawn: %s", value);
			return -1;
		}
	} else if (strcmp(key, "restore_last_focus") == 0) {
		if (strcmp(value, "true") == 0) {
			c->restore_last_focus = true;
		} else if (strcmp(value, "false") == 0) {
			c->restore_last_focus = false;
		} else {
			_LOG_(ERROR, "invalid value for focus_follow_spawn: %s", value);
			return -1;
		}
	} else if (strcmp(key, "rule") == 0) {
		rule_t *rule = init_rule();
		if (rule == NULL) {
			_LOG_(ERROR, "failed to allocate memory for rule_t");
			return -1;
		}
		if (parse_rule(value, rule) != 0) {
			_FREE_(rule);
			_LOG_(ERROR, "error while parsing rule %s", value);
			return -1;
		}
		add_rule(&rule_head, rule);
	} else if (strcmp(key, "bind") == 0) {
		conf_key_t *k = init_key();
		if (k == NULL) {
			_LOG_(ERROR, "failed to allocate memory for _key__t");
			return -1;
		}
		if (parse_keybinding(value, k) != 0) {
			err_cleanup(k);
			_LOG_(ERROR, "error while parsing keys");
			return -1;
		}
		add_key(&key_head, k);
	} else {
		_LOG_(WARNING, "unknown config key: %s", key);
	}
	return 0;
}

static int
parse_config(const char *filename, config_t *c, bool reload)
{
	FILE *file = fopen(filename, "r");
	if (file == NULL) {
		_LOG_(ERROR, "error: could not open file '%s'", filename);
		return -1;
	}

	char line[MAX_LINE_LENGTH];
	while (fgets(line, MAX_LINE_LENGTH, file)) {
		if (line[0] == ' ' || line[0] == '\t' || line[0] == '\n' ||
			line[0] == '\v' || line[0] == '\f' || line[0] == '\r' ||
			line[0] == ';') {
			continue;
		}
		char *key	= strtok(line, "=");
		char *value = strtok(NULL, "\n");

		if (key == NULL || value == NULL) {
			continue;
		}

		trim(key, WHITE_SPACE);
		trim(value, WHITE_SPACE);

#ifdef _DEBUG__
		_LOG_(DEBUG,
			  "config line = (%s) key = (%s) value = (%s)",
			  line,
			  key,
			  value);
#endif

		if (parse_config_line(key, value, c, reload) != 0) {
			fclose(file);
			return -1;
		}
	}

	fclose(file);
	return 0;
}

void
free_rules(void)
{
	rule_t *current = rule_head;
	while (current) {
		rule_t *next = current->next;
		_FREE_(current);
		current = next;
	}
	rule_head = NULL;
}

void
free_keys(void)
{
	conf_key_t *current = key_head;
	while (current) {
		conf_key_t *next = current->next;
		if (!current->arg) {
			current = next;
			continue;
		}
		if (!current->arg->cmd) {
			current = next;
			continue;
		}
		for (int j = 0; j < current->arg->argc; j++) {
			if (current->arg->cmd && current->arg->cmd[j]) {
				_FREE_(current->arg->cmd[j]);
			}
		}
		_FREE_(current->arg->cmd);
		_FREE_(current->arg);
		_FREE_(current);
		current = next;
	}
	key_head = NULL;
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
	if (!file_exists(filename)) {
		write_default_config(filename, c);
	}
	return parse_config(filename, c, false);
}
