// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <iostream>
#include <print>

#include "str.h"

int
main(int argc, char **argv)
{
    if (argc > 1 && str::eq(argv[1], "-v")) {
        std::println("html_pager v1.0 (c)1996 Armidale Software");
        exit(0);
    }
    std::string line;
    while (std::getline(std::cin, line)) {
        std::string_view html(line);
        bool inside = false;  // Are we inside any HTML tag?
        bool inlink = false;  // Are we inside a link tag (<a> ... </a>)?
        while (!html.empty()) {
            const auto c = html.front();
            if (c == '<') {
                inside = true;
                if (str::starteqcase(html, "<a"))
                    inlink = true;
                else if (str::starteqcase(html, "</a"))
                    inlink = false;
            } else if (inside && (c == '>' || c == '\n')) {
                // XXX(cross): The '\n' seems wrong.  HTML tags
                // can span multiple lines.
                inside = false;
            }
            std::size_t len = 1;
            if (!inside && !inlink &&
                (str::starteqcase(html, "gopher://") ||
                 str::starteqcase(html, "ftp://") ||
                 str::starteqcase(html, "http://") ||
                 str::starteqcase(html, "https://") ||
                 str::starteqcase(html, "gopher://") ||
                 str::starteqcase(html, "mailto://") ||
                 str::starteqcase(html, "news:") ||
                 str::starteqcase(html, "nntp:") ||
                 str::starteqcase(html, "telnet://") ||
                 str::starteqcase(html, "wais://") ||
                 str::starteqcase(html, "file://") ||
                 str::starteqcase(html, "prospero://")))
            {
                const auto *begin = html.data();
                const auto *p = html.data();
                for (const auto *end = begin + html.size(); p != end; p++)
                    if (*p == '<' || (!isalnum(*p) && !ispunct(*p)))
                        break;
                len = p - begin;
                const auto tag = html.substr(0, len);
                std::print("<a href=\"{}\">{}</a>", tag, tag);
            } else {
                std::print("{:c}", c);
            }
            html.remove_prefix(len);
        }
        std::println("");
    }

    return EXIT_SUCCESS;
}
