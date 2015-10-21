/*
 * Parse INI-style configuration file.
 *
 * Copyright (C) 2015 Serge Vakulenko
 *
 * This file is part of PIC32PROG project, which is distributed
 * under the terms of the GNU General Public License (GPL).
 * See the accompanying file "COPYING" for more details.
 */
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "target.h"

static const char *confname;
static char *bufr;
static int bsize;
static char *cursec;

extern char *progname;

/*
 * Scan to the end of a comment.
 */
static int eat_comment(FILE *fp)
{
    int c;

    c = getc(fp);
    while (c > 0 && c != '\n')
        c = getc(fp);
    return c;
}

/*
 * Skip whitespaces to end of line.
 */
static int eat_whitespace(FILE *fp)
{
    int c;

    c = getc(fp);
    while (isspace(c) && c != '\n')
        c = getc(fp);
    return c;
}

/*
 * Search for continuation backshash, starting from line end,
 * When found, return it's index.
 * When no continuation, return -1.
 */
static int find_continuation(char *line, int pos)
{
    pos--;
    while (pos >= 0 && isspace(line [pos]))
        pos--;
    if (pos >= 0 && line[pos] == '\\')
        return pos;
    /* No continuation. */
    return -1;
}

/*
 * Scan a parameter name (or name and value pair) and pass the value (or
 * values) to function pfunc().
 */
static void parse_parameter(FILE *fp, void (*pfunc)(char*, char*, char*), int c)
{
    int i = 0;          /* position withing bufr */
    int end = 0;        /* bufr[end] is current end-of-string */
    int vstart = 0;     /* starting position of the parameter */

    /* Loop until we found the start of the value */
    while (vstart == 0) {
        /* Ensure there's space for next char */
        if (i > (bsize-2)) {
            bsize += 1024;
            bufr = realloc(bufr, bsize);
            if (! bufr) {
                fprintf(stderr, "%s: malloc failed\n", confname);
                exit(-1);
            }
        }
        switch (c) {
        case '=':
            if (end == 0) {
                fprintf(stderr, "%s: invalid parameter name\n", confname);
                exit(-1);
            }
            bufr[end++] = '\0';
            i = end;
            vstart = end;
            bufr[i] = '\0';
            break;

        case ';':           /* comment line */
        case '#':
            c = eat_comment(fp);
        case '\n':
            i = find_continuation(bufr, i);
            if (i < 0) {
                /* End of line, but no assignment symbol. */
                bufr[end]='\0';
                fprintf(stderr, "%s: bad line, ignored: `%s'\n",
                    confname, bufr);
                return;
            }
            end = ((i > 0) && (bufr[i-1] == ' ')) ? (i-1) : (i);
            c = getc(fp);
            break;

        case '\0':
        case EOF:
            bufr[i] = '\0';
            fprintf(stderr, "%s: unexpected end-of-file at %s: func\n",
                confname, bufr);
            exit(-1);

        default:
            if (isspace(c)) {
                bufr[end] = ' ';
                i = end + 1;
                c = eat_whitespace(fp);
            } else {
                bufr[i++] = c;
                end = i;
                c = getc(fp);
            }
            break;
        }
    }

    /* Now parse the value */
    c = eat_whitespace(fp);
    while (c > 0) {
        if (i > (bsize-2)) {
            bsize += 1024;
            bufr = realloc(bufr, bsize);
            if (! bufr) {
                fprintf(stderr, "%s: malloc failed\n", confname);
                exit(-1);
            }
        }
        switch(c) {
        case '\r':
            c = getc(fp);
            break;

        case ';':           /* comment line */
        case '#':
            c = eat_comment(fp);
        case '\n':
            i = find_continuation(bufr, i);
            if (i < 0)
                c = 0;
            else {
                for (end=i; (end >= 0) && isspace(bufr[end]); end--)
                    ;
                c = getc(fp);
            }
            break;

        default:
            bufr[i++] = c;
            if (! isspace(c))
                end = i;
            c = getc(fp);
            break;
        }
    }
    bufr[end] = '\0';
    pfunc(cursec, bufr, &bufr [vstart]);
}

/*
 * Scan a section name and remember it in `cursec'.
 */
static void parse_section(FILE *fp)
{
    int c, i, end;

    /* We've already got the '['. Scan past initial white space. */
    c = eat_whitespace(fp);
    i = 0;
    end = 0;
    while (c > 0) {
        if (i > (bsize-2)) {
            bsize += 1024;
            bufr = realloc(bufr, bsize);
            if (! bufr) {
                fprintf(stderr, "%s: malloc failed\n", confname);
                exit(-1);
            }
        }
        switch (c) {
        case ']':       /* found the closing bracked */
            bufr[end] = '\0';
            if (end == 0) {
                fprintf(stderr, "%s: empty section name\n", confname);
                exit(-1);
            }
            /* Register a section. */
            if (cursec)
                free(cursec);
            cursec = strdup(bufr);

            eat_comment(fp);
            return;

        case '\n':
            i = find_continuation(bufr, i);
            if (i < 0) {
                bufr [end] = 0;
                fprintf(stderr, "%s: invalid line: '%s'\n",
                    confname, bufr);
                exit(-1);
            }
            end = ((i > 0) && (bufr[i-1] == ' ')) ? (i-1) : (i);
            c = getc(fp);
            break;

        default:
            if (isspace(c)) {
                bufr[end] = ' ';
                i = end + 1;
                c = eat_whitespace(fp);
            } else {
                bufr[i++] = c;
                end = i;
                c = getc(fp);
            }
            break;
        }
    }
}

/*
 * This function is called for every parameter found in the config file.
 */
static void configure_parameter(char *section, char *param, char *value)
{
    static char *last_section = 0, *family;
    static unsigned id, flash_kbytes;
    char *ep;

    //printf("Configure: [%s] %s = %s\n", section, param, value);
    if (! section) {
        /* Empty section name.
         * This is the case for the initial part of config file.
         * Usually some generic parameters are placed here. */
        fprintf(stderr, "%s: Unknown parameter: %s = %s\n",
            confname, param, value);
        return;
    }

    if (last_section && strcmp(section, last_section) != 0) {
        /* Last section finished.
         * Use collected data to add a new CPU variant. */
        if (! id || ! family || ! flash_kbytes) {
            fprintf(stderr, "%s: Not enough parameters for section %s\n",
                confname, last_section);
        } else
            target_add_variant(last_section, id, family, flash_kbytes);

        free(last_section);
        last_section = 0;
    }

    if (! param) {
        /* No parameter name: end of file reached. */
        free(last_section);
        last_section = 0;
        if (family) {
            free(family);
            family = 0;
        }
        return;
    }

    /* Store the section name. */
    if (! last_section)
        last_section = strdup(section);

    /* We have a new parameter for the current section. */
    if (strcasecmp(param, "id") == 0) {
        id = strtoul(value, 0, 0);
        if (debug_level > 1)
            printf("[%s] Id = %07x\n", section, id);

    } else if (strcasecmp(param, "family") == 0) {
        if (family)
            free(family);
        family = strdup(value);
        if (debug_level > 1)
            printf("[%s] Family = %s\n", section, family);

    } else if (strcasecmp(param, "flash") == 0) {
        flash_kbytes = strtoul(value, &ep, 0);
        if (*ep == 'k' || *ep == 'K') {
            /* Size in kilobytes. */
        } else if (*ep == 'm' || *ep == 'M') {
            /* Size in megabytes. */
            flash_kbytes *= 1024;
        } else {
            fprintf(stderr, "%s: Invalid Flash size: %s\n",
                confname, value);
        }
        if (debug_level > 1)
            printf("[%s] Flash = %uk\n", section, flash_kbytes);
    } else {
        fprintf(stderr, "%s: Unknown parameter: %s = %s\n",
            confname, param, value);
    }
}

/*
 * Read configuration file and setup target parameters.
 */
void target_configure()
{
    FILE *fp;
    int c;

    /*
     * Find the configuration file, if any.
     * (1) First, try a path from PIC32PROG_CONF_FILE environment variable.
     * (2) Otherwise, look for a file in the local directory.
     * (3) Otherwise, use /usr/local/etc/ directory (on Unix)
     *     or a directory where pic32prog.exe resides (on Windows)
     */
    confname = getenv("PIC32PROG_CONF_FILE");
    if (! confname)
        confname = "pic32prog.conf";

    if (access(confname, 0) < 0) {
#if defined(__CYGWIN32__) || defined(MINGW32)
        char *p = strrchr(progname, '\\');
        if (p) {
            char *buf = malloc(p - progname + 16);
            if (! buf) {
                fprintf(stderr, "%s: out of memory\n", progname);
                exit(-1);
            }
            strncpy(buf, progname, p - progname);
            strcpy(buf + (p - progname), "\\pic32prog.conf");
            confname = buf;
        } else
            confname = "c:\\pic32prog.conf";
#else
        confname = "/usr/local/etc/pic32prog.conf";
#endif
    }

    fp = fopen(confname, "r");
    if (! fp) {
        /* No config file available: that's OK. */
        return;
    }
    bsize = 1024;
    bufr = (char*) malloc(bsize);
    if (! bufr) {
        fprintf(stderr, "%s: malloc failed\n", confname);
        fclose(fp);
        exit(-1);
    }

    /* Parse file. */
    c = eat_whitespace(fp);
    while (c > 0) {
        switch (c) {
        case '\n':          /* blank line */
            c = eat_whitespace(fp);
            break;

        case ';':           /* comment line */
        case '#':
            c = eat_comment(fp);
            break;

        case '[':           /* section header */
            parse_section(fp);
            c = eat_whitespace(fp);
            break;

        case '\\':          /* bogus backslash */
            c = eat_whitespace(fp);
            break;

        default:            /* parameter line */
            parse_parameter(fp, configure_parameter, c);
            c = eat_whitespace(fp);
            break;
        }
    }
    configure_parameter("", 0, 0); /* Final call: end of file. */
    fclose(fp);
    if (cursec) {
        free(cursec);
        cursec = 0;
    }
    free(bufr);
    bufr = 0;
    bsize = 0;
}
