#ifndef IS_LIB_COMMON_CONF_H
#define IS_LIB_COMMON_CONF_H

/* This module parse ini files :
 *
 * [Section1]
 *
 * var1 = value1
 * ; Comments
 * var2 = value2
 *
 * [Section2]
 *
 * # Comments
 * var3 = value3
 *
 *
 * */

typedef struct conf_section_t {
    char *name;
    char **variables;
    char **values;
    int var_nb;
} conf_section_t;

typedef struct conf_t {
    conf_section_t **sections;
    int section_nb;
} conf_t;

int parse_ini(const char *filename, conf_t **conf);
void conf_dump(int level, const conf_t *conf);

const char *conf_get(const conf_t *conf, const char *section, const char *var);

void conf_wipe(conf_t *conf);
GENERIC_DELETE(conf_t, conf);

#endif
