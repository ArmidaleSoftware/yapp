// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

/* WWW specific commands */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include "driver.h"
#include "globals.h"
#include "lib.h"
#include "macro.h"
#include "main.h"
#include "mem.h"
#include "str.h"
#include "yapp.h"

static inline int
asciihex2nibble(char c)
{
    // ASCII digits are 0x30 ('0') to 0x39 ('9')
    // ASCII letters 'A'..'F' are 0x41..0x46
    // ASCII letters 'a'..'f' are 0x61..0x66
    return (c & 0xF) + ((c & 0x40) ? 9 : 0);
}

static char
x2c(const std::string_view &what)
{
    assert(what.size() == 2);
    return (asciihex2nibble(what[0]) << 4) | asciihex2nibble(what[1]);
}

static std::string &
plustospace(std::string &str)
{
    std::replace(str.begin(), str.end(), '+', ' ');
    return str;
}

/*
 * To be compliant with RFC-1738, any characters other than alphanumerics
 * and "$-_.+!*'()," must be encoded in URLs.
 */
int
url_encode(int argc, char **argv)
{
    FILE *fp;
    char *from = argv[1];

    /* If `url...` but not `url...|cmd`  (REDIRECT bit takes precedence) */
    if ((status & S_EXECUTE) && !(status & S_REDIRECT)) {
        fp = NULL;
    } else {
        if (status & S_REDIRECT) {
            fp = stdout;
        } else {
            open_pipe();
            fp = st_glob.outp;
            if (!fp) {
                fp = stdout;
            }
        }
    }

    if (argc < 2) {
        std::println("syntax: url_encode string");
        return 1;
    }
    while (*from) {
        if (isalnum(*from) || strchr("$-_.+!*'(),", *from))
            wfputc(*from++, fp);
        else {
            const auto hex = std::format("%%{:02X}", *from++);
            wfputs(hex, fp);
        }
    }
    if (fp)
        fflush(fp); /* flush after done with wfput stuff */
    return 1;
}

static std::string &
unescape_url(std::string &url)
{
    for (auto x = 0, y = 0; x < url.size(); ++x, ++y) {
        url[x] = url[y];
        if (url[x] == '%') {
            assert((url.size() - x) >= 2);
            url[x] = x2c(std::string_view{&url[y], 2});
            y += 2;
        }
    }
    return url;
}

/*
 * With the -c option, converts newlines to 2 characters ("\n")
 * Also, if any variable already exists, the value is appended
 * to allow posting lists (from checkbox,select,etc).
 */
int
www_parse_post(int argc, char **argv)
{
    int m = -1, cumul = 0;
    int cl;
    const auto env = expand("requestmethod", DM_VAR);
    char *buff;
    int convert = 0;
    if (argc > 1 && strchr(argv[1], 'c'))
        convert = 1;

    const auto *env2 = getenv("CONTENT_TYPE");
    if (env.empty() || !str::eq(env, "POST") || env2 == nullptr ||
        !str::eq(env2, "application/x-www-form-urlencoded")) {
        def_macro("error", DM_VAR,
            "This command can only be used to decode form results.");
        return 1;
    }
    const auto *cenv = getenv("CONTENT_LENGTH");
    cl = (cenv) ? atoi(cenv) : 0;

    buff = (char *)emalloc(cl + 1);

    /* Read in the whole thing */
    for (cumul = 0; cumul < cl; cumul += m) {
        m = read(saved_stdin[0].fd, buff + cumul, cl - cumul);
        if (m < 0) {
            error("reading full ", "content length");
            break;
        }
    }
    /*
    std::println("Read {}/{} bytes", cumul, cl);
    fflush(stdout);
    */

    if (cumul > 0) {
        buff[cumul] = '\0';
        auto vars = str::splits(buff, "&", false);
        for (auto &var : vars) {
            plustospace(var);
            unescape_url(var);
            const auto vi = var.find('=');
            if (vi == std::string::npos)
                continue;
            auto name = std::string_view(var.begin(), var.begin() + vi);
            auto raw = std::string_view(var.begin() + vi + 1, var.end());
            std::string value;
            const auto prev = expand(name, DM_VAR);
            if (!prev.empty()) {
                value = prev;
                value.push_back(' ');
            }
            for (const auto c : raw) {
                if (convert) {
                    switch (c) {
                    case '\r': // Ignore carriage return
                        continue;
                    case '\n': // Convert newline to "\n"
                        value.append("\\n");
                        continue;
                    case '%': // escape '%'
                        value.push_back('%');
                        break;
                    }
                }
                value.push_back(c);
            }
            def_macro(name, DM_VAR, value);
        }
    }
    free(buff);
    return 1;
}

void
urlset(void)
{
    auto str = expand("querystring", DM_VAR);
    if (str.empty())
        return;
    plustospace(str);
    unescape_url(str);
    const auto vars = str::split(str, "&", false);
    for (size_t v = 0; v < vars.size(); v++) {
        const auto fields = str::split(vars[v], "=", false);
        if (fields.size() == 2 && !fields[0].empty())
            def_macro(fields[0], DM_VAR, fields[1]);
    }
}
