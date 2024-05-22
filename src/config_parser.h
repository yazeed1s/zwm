#ifndef ZWM_CONFIG_PARSER_H
#define ZWM_CONFIG_PARSER_H

#include "type.h"
extern _key__t **conf_keys;
extern int		 _entries_;
int
load_config(config_t *c);
void
free_keys();
#endif // ZWM_CONFIG_PARSER_H