// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

/*
 * HTML sanity filter
 * Phase 1: map < > & " to escape sequences
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <print>

#include "mem.h"
#include "str.h"

#define STACKSIZE 1000

int stack[STACKSIZE];
int top = 0;

/* Illegal case-insensitive strings */
const char *illegal[] = {
    "HTML", "BODY", "HEAD", "TITLE", "ADDRESS", "ISINDEX", NULL};

const char *matching[] = {"H1", "H2", "H3", "H4", "H5", "H6", "A", "UL", "OL",
    "DL", "PRE", "BLOCKQUOTE", "DFN", "EM", "CITE", "CODE", "KBD", "SAMP",
    "STRONG", "VAR", "B", "I", "TT", "BLINK", "FORM", "SELECT", "TEXTAREA",
    "SUP", "SUB", "DIV", "FONT", "CENTER", "NOFRAMES", "FRAMESET", "TABLE",
    "TD", "TR", NULL};

char *Umatching[100];
char *Uillegal[100];

void
push(int i)
{
    if (top == STACKSIZE) {
        std::println("Stack overflow");
        exit(1);
    }
    stack[top++] = i;
}

int
pop(void)
{
    if (top == 0)
        return -1;
    top--;
    return stack[top];
}
/*
 * Load custom tags into malloc'ed space pointed to by elements of
 * Uillegal and Umatching arrays, and make sure those arrays
 * are NULL terminated
 */
int
load_illegal_tags(char *file)
{
    FILE *fp;
    char buff[80];
    int i;
    fp = fopen(file, "r");
    if (!fp)
        return 0; /* use non-custom tags */

    /* Load illegal tags first */
    i = 0;
    while (fscanf(fp, "%s", buff) == 1) {
        if (str::eq(buff, "|"))
            break;
        Uillegal[i] = estrdup(buff);
        i++;
    }
    Uillegal[i] = NULL;

    fclose(fp);
    return 1;
}

int
load_matched_tags(char *file)
{
    FILE *fp;
    char buff[80];
    int i;
    fp = fopen(file, "r");
    if (!fp)
        return 0; /* use non-custom tags */

    /* Now load matching tags */
    i = 0;
    while (fscanf(fp, "%s", buff) == 1) {
        Umatching[i] = estrdup(buff);
        i++;
    }
    Umatching[i] = NULL;

    fclose(fp);
    return 1;
}

void
usage(void)
{
    std::println(stderr, "Yapp {} (c)1996 Armidale Software", VERSION);
    std::println(stderr, " usage: html_check [-h] [-m file] [-i file]");
    std::println(stderr, " -h       Allow HTML header as legal");
    std::println(
        stderr, " -i file  Use tags in file as those which are illegal");
    std::println(
        stderr, " -m file  Use tags in file as those which must be matched");
    exit(1);
}
/* Also match quotes ("...") inside < > */

int
main(int argc, char **argv)
{
    FILE *fp;
    char buff[1024], *p;
    const char *tag;
    int html = 0, undo, quot = 0;
    int i;
    int allow_header = 0, illegal_tags = 0, matched_tags = 0;
    const char *options = "hvi:m:"; /* Command-line options */
    extern char *optarg;
    extern int optind, opterr;
    while ((i = getopt(argc, argv, options)) != -1) {
        switch (i) {
        case 'h':
            allow_header = 1;
            break;
        case 'm':
            matched_tags = load_matched_tags(optarg);
            break;
        case 'i':
            illegal_tags = load_illegal_tags(optarg);
            break;
        case 'v':
        default:
            usage();
        }
    }
    argc -= optind;
    argv += optind;

    if (argc > 0) {
        if ((fp = fopen(argv[0], "r")) == NULL) {
            std::println(stderr, "Can't open {}", argv[0]);
            exit(1);
        }
    } else
        fp = stdin;

    while (fgets(buff, sizeof(buff), fp) != NULL) {
        for (p = buff; *p;) {
            /* Match < > */
            if (*p == '<') {
                if (html) {
                    std::println("Found \"<\" inside HTML tag in "
                                 "\{}\".",
                        buff);
                    std::println("Use \"&lt;\" instead of \"<\" if you want a "
                                 "less-than sign to appear.");
                    exit(1);
                }
                html++;
                quot = 0;

                p++;
                if (*p == '/') {
                    undo = 1;
                    p++;
                } else
                    undo = 0;

                /* Compare to list of illegal tags */
                if (!allow_header) {
                    i = 0;
                    tag = (illegal_tags) ? Uillegal[i] : illegal[i];
                    while (tag) {
                        if (!strncasecmp(p, tag, strlen(tag)) &&
                            !isalnum(p[strlen(tag)])) {
                            std::println("Illegal HTML tag found: {}", tag);
                            exit(1);
                        }
                        i++;
                        tag = (illegal_tags) ? Uillegal[i] : illegal[i];
                    }
                }

                /* Compare to list of matching tags */
                i = 0;
                tag = (matched_tags) ? Umatching[i] : matching[i];
                while (tag) {
                    if (!strncasecmp(p, tag, strlen(tag)) &&
                        !isalnum(p[strlen(tag)])) {

                        /* Disallow overlapping tags
                         * like B I /B /I But allow H1
                         * A /A H1 */
                        if (undo) {
                            int j = pop();
                            if (j < 0) {
                                std::println(
                                    "closing {} tag found without opening tag",
                                    tag);
                                exit(1);
                            }
                            if (i != j) {
                                std::println("tags {} and {} overlap in \"{}\"",
                                    tag,
                                    ((matched_tags) ? Umatching[j]
                                                    : matching[j]),
                                    buff);
                                exit(1);
                            }
                        } else {
                            push(i);
                        }
                        break;
                    }
                    i++;
                    tag = (matched_tags) ? Umatching[i] : matching[i];
                }
            } else if (*p == '>') {
                if (!html) {
                    std::println(
                        "\">\" appears outside an HTML tag in \"{}\".", buff);
                    std::println("Use \"&gt;\" instead of \">\" if you want a "
                                 "greater-than sign to appear.");
                    exit(1);
                }
                html--;
                if (quot) {
                    std::println(
                        "Missing end quote in HTML tag in \"{}\".", buff);
                    exit(1);
                }
                p++;
            } else if (*p == '"' && html) {
                quot = 1 - quot;
                p++;
            } else
                p++;
        }
    }

    /* Make sure not still in tag */
    if (html) {
        std::println("Missing \">\" at end of HTML tag.");
        std::println("Use \"&lt;\" instead of \"<\" if you want a less-than "
                     "sign to appear.");
        exit(1);
    }

    /* Make sure stack is empty */
    if (top) {
        i = pop();
        std::println("Missing ending {} tag",
            (matched_tags) ? Umatching[i] : matching[i]);
        exit(1);
    }

    /* Ok, no problems found */
    exit(0);
}
