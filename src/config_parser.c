#include "type.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LENGTH 100
#define CONF_PATH		".config/zwm/zwm.conf.ini"

int
file_exists(const char *filename)
{
	FILE *file = fopen(filename, "r");
	if (file != NULL) {
		fclose(file);
		return 1;
	}
	return 0;
}

int
write_default_config(const char *filename, config_t *c)
{
	const char *content = "border_width = 2\n"
						  "border_color = 0x83a598\n"
						  "window_gap = 5\n";
	FILE	   *file	= fopen(filename, "w");
	if (file == NULL) {
		return -1;
	}
	c->border_color = 0x83a598;
	c->border_width = 2;
	c->window_gap	= 5;
	fprintf(file, "%s", content);
	fclose(file);
	return 0;
}

void
trim_whitespaces(char *str)
{
	char *end	= str + strlen(str);
	char *start = str;
	if (*end == '\0') {
		end--;
		while ((*end == ' ' || *end == '\t') && end > str) {
			*end = '\0';
			end--;
		}
	}

	while (*start == ' ' || *start == '\t') {
		start++;
	}
	if (start != str) {
		size_t len = strlen(start);
		memmove(str, start, len + 1);
	}
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
		if (*key == '#') {
			continue;
		}
		trim_whitespaces(key);
		trim_whitespaces(value);
		printf("key = (%s)\n", key);
		printf("value = (%s)\n", value);
		if (key == NULL || value == NULL) {
			continue;
		}
		if (strcmp(key, "border_width") == 0) {
			c->border_width = atoi(value);
		} else if (strcmp(key, "border_color") == 0) {
			c->border_color = (unsigned int)strtoul(value, NULL, 16);
		} else if (strcmp(key, "window_gap") == 0) {
			c->window_gap = atoi(value);
		}
	}

	fclose(file);
	return 0;
}

int
load_config(config_t *c)
{
	const char *filename = CONF_PATH;
	if (!file_exists(filename)) {
		return write_default_config(filename, c);
	} else {
		return parse_config(filename, c);
	}
}