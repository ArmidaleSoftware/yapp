// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

/*
 * HTML sanity filter
 * Phase 1: map < > & " to escape sequences
 * with -c: map \n to newline
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <print>

void
usage(void)
{
    std::println(stderr, "Yapp {} (c)1996 Armidale Software", VERSION);
    std::println(stderr, "n usage: html_filter [-cn]");
    std::println(stderr, " -c   Map \\n to newline");
    std::println(stderr, " -n   Don't output newlines");
    exit(1);
}

int
main(int argc, char **argv)
{
    int c;
    int newline = 1,           /* pass newlines through */
        back = 0, convert = 0; /* convert \n to newline */

    const char *options = "hvnc"; /* Command-line options */
    extern int optind;
    while ((c = getopt(argc, argv, options)) != -1) {
        switch (c) {
        case 'n':
            newline = 0;
            break;
        case 'c':
            convert = 1;
            break;
        case 'v':
        case 'h':
        default:
            usage();
        }
    }
    argc -= optind;
    argv += optind;

    while ((c = getchar()) != EOF) {
        if (convert) {
            if (back) {
                if (c == 'n')
                    c = '\n';
                back = 0;
            } else if (c == '\\') {
                back = 1;
                continue;
            }
        }
        switch (c) {
        case '>':
            std::print("&gt;");
            break;
        case '<':
            std::print("&lt;");
            break;
        case '&':
            std::print("&amp;");
            break;
        case '"':
            std::print("&quot;");
            break;
        case '\n':
            if (newline)
                putchar(c);
            break;
        default:
            putchar(c);
        }
    }
    exit(0);
}
