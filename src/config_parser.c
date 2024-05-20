#include "logger.h"
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
#include <xcb/xproto.h>

#define MAX_LINE_LENGTH 100
#define CONF_PATH		".config/zwm/zwm.conf"
#define ALT_MASK		XCB_MOD_MASK_1
#define SUPER_MASK		XCB_MOD_MASK_4
#define SHIFT_MASK		XCB_MOD_MASK_SHIFT
#define CTRL_MASK		XCB_MOD_MASK_CONTROL

typedef enum {
	WHITE_SPACE,
	CURLY_BRACKET,
	PARENTHESIS,
	SQUARE_BRACKET
} trim_token_t;

int	 _keys_					  = 0;
bool transfer_node_is_filled  = false;
bool switch_desktop_is_filled = false;

// clang-format off
static conf_mapper_t _cmapper_[] = { 
 	{"run", exec_process}, 
 	{"kill", close_or_kill_wrapper}, 
 	{"switch_desktop", switch_desktop_wrapper}, 
 	{"grow", horizontal_resize_wrapper}, 
 	{"shrink", horizontal_resize_wrapper}, 
 	{"fullscreen", set_fullscreen_wrapper}, 
 	{"swap", swap_node_wrapper}, 
 	{"transfer_node", transfer_node_wrapper}, 
 	{"master", layout_handler}, 
 	{"default", layout_handler}, 
 	{"grid", layout_handler}, 
 	{"stack", layout_handler}, 
 	{"traverse_up", traverse_stack_wrapper}, 
 	{"traverse_down", traverse_stack_wrapper}, 
	{"flip", flip_node_wrapper}
}; 

static key_mapper_t	 _kmapper_[] = { 
 	{"0", XK_0},{"1", XK_1},{"2", XK_2}, 
 	{"3", XK_3},{"4", XK_4},{"5", XK_5}, 
 	{"6", XK_6},{"7", XK_7},{"8", XK_8}, 
 	{"9", XK_9},{"a", XK_a},{"b", XK_b}, 
 	{"c", XK_c},{"d", XK_d},{"e", XK_e}, 
 	{"f", XK_f},{"g", XK_g},{"h", XK_h}, 
 	{"i", XK_i},{"j", XK_j},{"k", XK_k}, 
 	{"l", XK_l},{"m", XK_m},{"n", XK_n}, 
 	{"o", XK_o},{"p", XK_p},{"q", XK_q}, 
 	{"r", XK_r},{"s", XK_s},{"t", XK_t}, 
 	{"u", XK_u},{"v", XK_v},{"w", XK_w}, 
 	{"x", XK_x},{"y", XK_y},{"z", XK_z}, 
	{"space", XK_space},
	{"return", XK_Return}, 
 	{"super", (xcb_keysym_t)SUPER_MASK}, 
 	{"alt",     (xcb_keysym_t)ALT_MASK}, 
 	{"ctr",    (xcb_keysym_t)CTRL_MASK}, 
 	{"shift", (xcb_keysym_t)SHIFT_MASK}, 
};
// clang-format on

int (*str_to_func(char *ch))(const arg_t *)
{
	int n = sizeof(_cmapper_) / sizeof(_cmapper_[0]);
	for (int i = 0; i < n; i++) {
		if (_cmapper_[i].func == ch) {
			return _cmapper_[i].function_ptr;
		}
	}
	return NULL;
}

uint32_t
str_to_key(char *ch)
{
	int n = sizeof(_kmapper_) / sizeof(_kmapper_[0]);
	for (int i = 0; i < n; i++) {
		if (_kmapper_[i].key == ch) {
			return _kmapper_[i].val;
		}
	}

	return -1;
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

int
write_default_config(const char *filename, config_t *c)
{
	const char *content =
		"border_width = 2\n"
		"active_border_color = 0x83a598\n"
		"normal_border_color = 0x30302f\n"
		"window_gap = 10\n"
		"key = {super + return -> run(\"alacritty\")}\n"
		"key = {super + space -> run(\"dmenu_run\")}\n"
		"key = {super + p -> run([\"rofi\",\"-show\", \"drun\"])}\n"
		"key = {super + w -> func(kill)}\n"
		"key = {super -> func(switch_desktop)}\n"
		"key = {super + l -> func(grow)}\n"
		"key = {super + h -> func(shrink)}\n"
		"key = {super + f -> func(fullscreen)}\n"
		"key = {super + s -> func(swap)}\n"
		"key = {super|shift -> func(transfer_node)}\n"
		"key = {super|shift + m -> func(master)}\n"
		"key = {super|shift + s -> func(stack)}\n"
		"key = {super|shift + d -> func(default)}\n"
		"key = {super|shift + k -> func(traverse_up)}\n"
		"key = {super|shift + j -> func(traverse_down)}\n"
		"key = {super|shift + f -> func(flip)}\n";

	char dir_path[strlen(filename) + 1];
	strcpy(dir_path, filename);
	char *last_slash = strrchr(dir_path, '/');
	if (last_slash != NULL) {
		*last_slash = '\0';
		struct stat st;
		if (stat(dir_path, &st) == -1) {
			if (mkdir(dir_path, 0777) == -1) {
				log_message(
					ERROR, "Failed to create directory: %s\n", dir_path);
				return -1;
			}
		}
	}

	FILE *file = fopen(filename, "w");
	if (file == NULL) {
		log_message(ERROR, "Failed to create config file: %s\n", filename);
		return -1;
	}

	c->active_border_color = 0x83a598;
	c->normal_border_color = 0x30302f;
	c->border_width		   = 2;
	c->window_gap		   = 5;
	fprintf(file, "%s", content);
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
		perror("Failed to allocate memory");
		exit(EXIT_FAILURE);
	}
	char *str_copy = strdup(str);
	if (str_copy == NULL) {
		perror("Failed to duplicate string");
		exit(EXIT_FAILURE);
	}

	char *token = strtok(str_copy, &delimiter);
	i			= 0;
	while (token != NULL) {
		tokens[i] = strdup(token);
		if (tokens[i] == NULL) {
			perror("Failed to duplicate token");
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
	for (int i = 0; i < _keys_; i++) {
		if (conf.keys[i].function_ptr == key->function_ptr &&
			conf.keys[i].keysym == key->keysym) {
			return true;
		}
	}

	return false;
}

void
add_key(char *mod, char *keysym, char *func)
{
	_key__t	 key  = {};
	uint32_t _mod = str_to_key(mod);

	if ((int)_mod == -1) {
		int	   count;
		char **mods = split_string(mod, '|', &count);
		if (mods == NULL || count != 2)
			return;

		uint32_t mask1 = str_to_key(mods[0]);
		uint32_t mask2 = str_to_key(mods[1]);
		key.mod		   = mask1 | mask2;
		free_tokens(mods, count);
	} else {
		key.mod = _mod;
	}

	if (keysym == NULL) {
		if ((strcmp(func, "switch_desktop") == 0 &&
			 !switch_desktop_is_filled) ||
			(strcmp(func, "transfer_node") == 0 && !transfer_node_is_filled)) {

			int d_count				  = wm->n_of_desktops;
			int (*ptr)(const arg_t *) = str_to_func(func);

			for (int i = 0; i < d_count; i++) {
				_key__t k			= {.mod			 = key.mod,
									   .keysym		 = (xcb_keysym_t)(XK_0 + i),
									   .function_ptr = ptr,
									   .arg			 = &((arg_t){.idx = i})};
				conf.keys[_keys_++] = k;
			}
			if (strcmp(func, "switch_desktop") == 0) {
				switch_desktop_is_filled = true;
			} else {
				transfer_node_is_filled = true;
			}
		}
		return;
	}

	key.keysym				  = (xcb_keysym_t)str_to_key(keysym);
	int (*ptr)(const arg_t *) = str_to_func(func);
	key.function_ptr		  = ptr;

	if (strcmp(func, "grow") == 0) {
		key.arg = &((arg_t){.r = GROW});
	} else if (strcmp(func, "shrink") == 0) {
		key.arg = &((arg_t){.r = SHRINK});
	} else if (strcmp(func, "master") == 0) {
		key.arg = &((arg_t){.t = MASTER});
	} else if (strcmp(func, "default") == 0) {
		key.arg = &((arg_t){.t = DEFAULT});
	} else if (strcmp(func, "grid") == 0) {
		key.arg = &((arg_t){.t = GRID});
	} else if (strcmp(func, "stack") == 0) {
		key.arg = &((arg_t){.t = STACK});
	} else if (strcmp(func, "traverse_up") == 0) {
		key.arg = &((arg_t){.d = UP});
	} else if (strcmp(func, "traverse_down") == 0) {
		key.arg = &((arg_t){.d = DOWN});
	}

	conf.keys[_keys_++] = key;
}

void
parse_keybinding(char *str)
{
#ifdef _DEBUG__
	log_message(DEBUG, "value before trim = %s ", str);
	trim(str, CURLY_BRACKET);
	log_message(DEBUG, "value after trim = %s ", str);
#endif
	trim(str, CURLY_BRACKET);

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
#ifdef _DEBUG__
	log_message(DEBUG,
				"key val to parse = (%s)\n mod = (%s) \n keysym = (%s) \n "
				"func = (%s)",
				str,
				mod,
				keysym,
				func);
#endif
	add_key(mod, keysym, func);
}

int
parse_config(const char *filename, config_t *c)
{
	FILE *file = fopen(filename, "r");
	if (file == NULL) {
		fprintf(stderr, "Error: Could not open file '%s'\n", filename);
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
		log_message(DEBUG,
					"config line = (%s) key = (%s) value = (%s)\n",
					line,
					key,
					value);
#endif
		if (key == NULL || value == NULL) {
			continue;
		}
		if (strcmp(key, "border_width") == 0) {
			c->border_width = atoi(value);
		} else if (strcmp(key, "active_border_color") == 0) {
			c->active_border_color = (unsigned int)strtoul(value, NULL, 16);
		} else if (strcmp(key, "normal_border_color") == 0) {
			c->normal_border_color = (unsigned int)strtoul(value, NULL, 16);
		} else if (strcmp(key, "window_gap") == 0) {
			c->window_gap = atoi(value);
		} else if (strcmp(key, "key") == 0) {
			parse_keybinding(value);
		}
	}

	fclose(file);
	return 0;
}

int
load_config(config_t *c)
{
	const char *filename = CONF_PATH;
	return !file_exists(filename) ? write_default_config(filename, c)
								  : parse_config(filename, c);
}