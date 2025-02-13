// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <print>

#include "mem.h"

#define LF 10
#define CR 13

char *
makeword(char *line, char stop)
{
    const char *sep = strchr(line, stop);
    char *word;
    size_t llen, wlen;

    if (sep == NULL)
        return estrdup("");
    llen = strlen(line);
    wlen = sep - line;
    word = estrndup(line, wlen);
    memmove(line, line + wlen, llen - wlen);

    return word;
}

char *
fmakeword(FILE *f, char stop, int *cl)
{
    int wsize;
    char *word;
    int ll;
    wsize = 102400;
    ll = 0;
    word = (char *)emalloc(wsize + 1);

    while (1) {
        word[ll] = (char)fgetc(f);
        if (ll == wsize) {
            word[ll + 1] = '\0';
            wsize += 102400;
            word = (char *)erealloc(word, wsize + 1);
        }
        --(*cl);
        if ((word[ll] == stop) || (feof(f)) || (!(*cl))) {
            if (word[ll] != stop)
                ll++;
            word[ll] = '\0';
            return word;
        }
        ++ll;
    }
}

char
x2c(char *what)
{
    char digit;
    digit = (what[0] >= 'A' ? ((what[0] & 0xdf) - 'A') + 10 : (what[0] - '0'));
    digit *= 16;
    digit += (what[1] >= 'A' ? ((what[1] & 0xdf) - 'A') + 10 : (what[1] - '0'));
    return (digit);
}

void
unescape_url(char *url)
{
    int x, y;
    for (x = 0, y = 0; url[y]; ++x, ++y) {
        if ((url[x] = url[y]) == '%') {
            url[x] = x2c(&url[y + 1]);
            y += 2;
        }
    }
    url[x] = '\0';
}

void
plustospace(char *str)
{
    int x;
    for (x = 0; str[x]; x++)
        if (str[x] == '+')
            str[x] = ' ';
}

int
rind(char *s, char c)
{
    int x;
    for (x = strlen(s) - 1; x != -1; x--)
        if (s[x] == c)
            return x;
    return -1;
}

int
ygetline(char *s, int n, FILE *f)
{
    int i = 0;
    while (1) {
        s[i] = (char)fgetc(f);

        if (s[i] == CR)
            s[i] = fgetc(f);

        if ((s[i] == 0x4) || (s[i] == LF) || (i == (n - 1))) {
            s[i] = '\0';
            return (feof(f) ? 1 : 0);
        }
        ++i;
    }
}

void
send_fd(FILE *src, FILE *dst)
{
    int c;
    for (;;) {
        c = fgetc(src);
        if (feof(src))
            return;
        fputc(c, dst);
    }
}

void
escape_shell_cmd(char *cmd)
{
    int x, y, l;
    l = strlen(cmd);
    for (x = 0; cmd[x]; x++) {
        if (strchr("&;`'\"|*?~<>^()[]{}$\\", cmd[x]) != NULL) {
            for (y = l + 1; y > x; y--) cmd[y] = cmd[y - 1];
            l++; /* length has been increased */
            cmd[x] = '\\';
            x++; /* skip the character */
        }
    }
}

void
html_header(char *title)
{
    std::println("Content-type: text/html");
    std::print("\n");
    std::print("<HTML><HEAD><TITLE>{}</TITLE></HEAD>", title);
    std::println("<BODY><H1>{}</H1>", title);
}
