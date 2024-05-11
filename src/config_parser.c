#include "type.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LENGTH 100

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