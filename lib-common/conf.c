#include <lib-common/mem.h>
#include <lib-common/blob.h>

#include "conf.h"

#define CONF_DBG_LVL 3

static conf_section_t *conf_section_init(conf_section_t *section)
{
    p_clear(section, 1);
    return section;
}
static void conf_section_wipe(conf_section_t *section)
{
    p_delete(&section->name);
    while (section->var_nb > 0) {
        section->var_nb--;
        p_delete(&section->variables[section->var_nb]);
        p_delete(&section->values[section->var_nb]);
    }
    p_delete(&section->variables);
    p_delete(&section->values);
}
GENERIC_NEW(conf_section_t, conf_section);
GENERIC_DELETE(conf_section_t, conf_section);

static conf_t *conf_init(conf_t *conf)
{
    conf->sections = NULL;
    conf->section_nb = 0;
    return conf;
}
GENERIC_NEW(conf_t, conf);
void conf_wipe(conf_t *conf)
{
    while (conf->section_nb > 0) {
        conf->section_nb--;
        conf_section_delete(&conf->sections[conf->section_nb]);
    }
    p_delete(&conf->sections);
}

const char *conf_get(const conf_t *conf, const char *section, const char *var)
{
    int i;
    conf_section_t *s;
    
    if (!section || !var) {
        return NULL;
    }
    
    for (i = 0; i < conf->section_nb; i++) {
        s = conf->sections[i];
        if (!strcasecmp(s->name, section)) {
            for (i = 0; i < s->var_nb; i++) {
                if (!strcasecmp(s->variables[i], var)) {
                    return s->values[i];
                }
            }
            return NULL;
        }
    }
    return NULL;
}


/**
 *  Parse functions
 *
 */

static void section_add_var(conf_section_t *section,
                            const char *var, const char *var_end,
                            const char *val, const char *val_end)
{
    int i;
    
    section->variables = p_renew(char *, section->variables,
                                 section->var_nb,
                                 section->var_nb + 1);
    section->values    = p_renew(char *, section->values,
                                 section->var_nb,
                                 section->var_nb + 1);
    i = var_end - var;
    section->variables[section->var_nb] = p_new(char, i + 1);
    pstrcpylen(section->variables[section->var_nb], i + 1, var, i);
    i = val_end - val;
    section->values[section->var_nb] = p_new(char, i + 1);
    pstrcpylen(section->values[section->var_nb], i + 1, val, i);
    section->var_nb++;
}

static void conf_add_section(conf_t *conf, conf_section_t *section)
{
    conf->sections = p_renew(conf_section_t *, conf->sections,
                                 conf->section_nb,
                                 conf->section_nb + 1);
    conf->sections[conf->section_nb] = section;
    conf->section_nb++;
}

static inline void readline_aux(blob_t *buf, blob_t *line)
{
    int i = blob_search_cstr(buf, 0, "\n");
    while (i == 0 && buf->len > 0) {
        blob_kill_first(buf, 1);
        i = blob_search_cstr(buf, 0, "\n");
    }
    if (i < 0) {
        blob_set(line, buf);
        blob_resize(buf, 0);
    } else
    if (i > 0) {
        blob_set_data(line, buf->data, i);
        blob_kill_first(buf, i);
    } else {
        blob_resize(buf, 0);
        blob_resize(line, 0);
    }
}

static inline void readline(blob_t *buf, blob_t *line)
{
    while(true) {
        readline_aux(buf, line);
        if (*blob_get_cstr(line) == '#' ||
            *blob_get_cstr(line) == ';') {
            e_debug(CONF_DBG_LVL + 1, E_PREFIX("Comment : %s\n"), blob_get_cstr(line));
        }
        else {
            e_debug(CONF_DBG_LVL + 1, E_PREFIX("Read line : %s\n"), blob_get_cstr(line));
            return;
        }
    }
}

int parse_ini(const char *filename, conf_t **conf)
{
    FILE* f;
    blob_t buf;
    blob_t buf_line;
    int i;
    const char *p, *start;
    const char *var_start, *var_end;
    const char *val_start, *val_end;
    conf_section_t *section;

    f = fopen(filename, "r");
    if (!f) {
        e_debug(CONF_DBG_LVL, E_PREFIX("could not open %s for reading\n"), filename);
        return 1;
    }

    blob_init(&buf);
    blob_init(&buf_line);

    while (!feof(f)) {
        i = blob_append_fread(&buf, 1, 4096, f);
        e_debug(CONF_DBG_LVL + 1, E_PREFIX("read %d bytes...\n"), i);
    }
    fclose(f);
    if (buf.len == 0) {
        return 1;
    }
     
    *conf = conf_new();
    
    while (buf.len > 0) {
        
        /* Read a new line */
        readline(&buf, &buf_line);

        if (buf_line.len == 0) {
            e_debug(CONF_DBG_LVL + 1, E_PREFIX("End of input...\n"));
            break;
        }
section_parse:
        /* Search start of section name */
        i = blob_search_cstr(&buf_line, 0, "[");
        if (i < 0) {
            e_debug(CONF_DBG_LVL, E_PREFIX("Junk line : %s\n"), blob_get_cstr(&buf_line));
            continue;
        }
        blob_kill_first(&buf_line, i + 1);

        start = blob_get_cstr(&buf_line);

        /* Search end of section name */
        i = blob_search_cstr(&buf_line, 0, "]");
        if (i < 0) {
            e_debug(CONF_DBG_LVL, E_PREFIX("Could not find section name end : %s\n"), blob_get_cstr(&buf_line));
            continue;
        }

        /* Copy name in the section struct */
        section = conf_section_new();
        section->name = p_new(char, i + 1);
        pstrcpylen(section->name, i + 1, start, i);
        
        e_debug(CONF_DBG_LVL + 1, E_PREFIX("section name : %s\n"), section->name);
        
        /* Add this section in the conf struct */
        conf_add_section(*conf, section);
            
        e_debug(CONF_DBG_LVL + 1, E_PREFIX(" Read 1 section name : buffer remaining:\n%s"), blob_get_cstr(&buf));

        /* Add varnames in section */
        while (buf.len > 0) {
            readline(&buf, &buf_line);

            if (buf_line.len == 0) {
                break;
            }
            
            p = blob_get_cstr(&buf_line);
            var_start = skipspaces(p);
            /* Skip first spaces */
            blob_kill_first(&buf_line, var_start - p);

            var_start = blob_get_cstr(&buf_line);
            /* Check that we are not reading a section name instead */
            if (*var_start == '[') {
                // This line is a section name
                goto section_parse;
            }
            // Search end of var
            i = blob_search_cstr(&buf_line, 0, " ");
            if (i <= 0) {
                e_debug(CONF_DBG_LVL + 1, E_PREFIX("No space or leading space on this line -> dropped : %s\n"), blob_get_cstr(&buf_line));
                continue;
            }
            e_debug(CONF_DBG_LVL + 1, E_PREFIX(" next ' ' at %d\n"), i);
            var_end = var_start + i;
            e_debug(CONF_DBG_LVL + 1, E_PREFIX("  varname : %.*s\n"), var_end - var_start, var_start);
            // Search =
            i = blob_search_cstr(&buf_line, i, "=");
            if (i < 0) {
                // No value
                e_debug(CONF_DBG_LVL, E_PREFIX("No value on this line -> dropped : %s\n"), blob_get_cstr(&buf_line));
                continue;
            }
            e_debug(CONF_DBG_LVL + 1, E_PREFIX(" next '=' at %d\n"), i);
            p = var_start + i + 1;
            // Skip spaces after =
            val_start = skipspaces(p);
            i += val_start - p;

            // Read until end of line
            // FIXME: trim the value on the right
            i = blob_search_cstr(&buf_line, i, "\n");
            if (i > 0) {
                e_debug(CONF_DBG_LVL + 1, E_PREFIX(" next '\\n' at %d\n"), i);
                val_end = var_start + i;
            }
            else {
                // EOF
                val_end = var_start + buf_line.len;
            }
            e_debug(CONF_DBG_LVL + 1, E_PREFIX("  value   : %.*s\n"), val_end - val_start, val_start);
            section_add_var(section, var_start, var_end, val_start, val_end);

        }
    }
    blob_wipe(&buf);
    blob_wipe(&buf_line);
    
    return 0;
}

void conf_dump(int level, const conf_t *conf)
{
    int i, j;
    conf_section_t *section;
    
    e_debug(level, "Conf with %d sections :\n", conf->section_nb);
    
    for (i = 0; i < conf->section_nb; i++)
    {
        section = conf->sections[i];
        e_debug(level, "[%s]\n", section->name);

        for (j = 0; j < section->var_nb; j++) {
            e_debug(level, "%s = %s\n", section->variables[j], section->values[j]);
        }
    }
}

