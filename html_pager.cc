// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <print>

#include "str.h"

int
main(int argc, char **argv)
{
    char buff[1024], tag[1024];
    char *p;
    int inside,    /* true if we're inside a link <a> ... </a> */
        intag = 0; /* true if we're inside an html tag < ... > */
    int i;

    if (argc > 1 && str::eq(argv[1], "-v")) {
        std::println("html_pager v1.0 (c)1996 Armidale Software");
        exit(0);
    }
    while (fgets(buff, 1024, stdin)) {

        inside = 0;
        for (p = buff; *p;) {
            if (*p == '<') {
                if (toupper(p[1]) == 'A')
                    intag = 1;
                else if (p[1] == '/' && toupper(p[2]) == 'A')
                    intag = 0;
                inside = 1;
            } else if (inside && (*p == '>' || *p == '\n')) {
                inside = 0;
            }
            if (!inside && !intag &&
                (!strncasecmp(p, "ftp:/", 5) || !strncasecmp(p, "http://", 7) ||
                    !strncasecmp(p, "gopher://", 9) ||
                    !strncasecmp(p, "mailto://", 9) ||
                    !strncasecmp(p, "news:", 5) ||
                    !strncasecmp(p, "nntp:", 5) ||
                    !strncasecmp(p, "telnet://", 9) ||
                    !strncasecmp(p, "wais:/", 6) ||
                    !strncasecmp(p, "file:/", 6) ||
                    !strncasecmp(p, "prospero://", 11))) {
                for (i = 0; p[i] != '<' && (isalnum(p[i]) || ispunct(p[i]));
                    i++)
                    tag[i] = p[i];
                if (ispunct(tag[i - 1]) && tag[i - 1] != '/')
                    i--;
                tag[i] = '\0';
                std::print("<A HREF=\"{}\">{}</A>", tag, tag);
                p += i;
                continue;
            } else
                putchar(*p++);
        }
    }
    exit(0);
}
