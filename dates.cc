// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include <ctype.h>
#include <string.h>
#include <time.h>

#include "lib.h"

static const char *month[] = {"jan_uary", "feb_ruary", "mar_ch", "apr_il",
    "may", "jun_e", "jul_y", "aug_ust", "sep_tember", "oct_ober", "nov_ember",
    "dec_ember"};

void
get_num(int *a, const char **ptr)
{
    while (isdigit(**ptr)) {
        *a = *a * 10 + **ptr - '0';
        (*ptr)++;
    }
}

void
get_str(int *m, const char **ptr)
{
    int i, l;
    char buff[64];

    const char *p = *ptr;
    for (l = 0; isalpha(p[l]) && l < sizeof(buff) - 1; l++) buff[l] = p[l];
    buff[l] = 0;

    for (i = 0; i < 12; i++) {
        if (match(buff, month[i])) {
            *m = i + 1;
            *ptr += l;
        }
    }
}

void
get_time(struct tm *tm, const char **ptr)
{
    tm->tm_hour = 0;
    get_num(&(tm->tm_hour), ptr);
    if (**ptr == ':') {
        (*ptr)++;
        tm->tm_min = 0;
        get_num(&(tm->tm_min), ptr);
    } else
        tm->tm_min = 0;
    if (**ptr == ':') {
        (*ptr)++;
        tm->tm_sec = 0;
        get_num(&(tm->tm_sec), ptr);
    } else
        tm->tm_sec = 0;
    while (**ptr == ' ') (*ptr)++;
    if (tolower(**ptr) == 'a' || tolower(**ptr) == 'm')
        tm->tm_hour %= 12;
    if (tolower(**ptr) == 'p' || tolower(**ptr) == 'n')
        tm->tm_hour = (tm->tm_hour % 12) + 12;
}

/* Take a string, return time_t value */
const char *
do_getdate(time_t *tt, const char *str)
{
    static const char *wkdy[] = {"Sun, ", "Mon, ", "Tue, ", "Wed, ", "Thu, ",
        "Fri, ", "Sat, ", "Sun ", "Mon ", "Tue ", "Wed ", "Thu ", "Fri ",
        "Sat "};

    struct tm tm;
    time_t t;
    int i, sgn;
    int a = 0, b = 0, c = 0, m = 0;

    time(&t); /* get current time */
    memcpy(&tm, localtime(&t), sizeof(tm));
    tm.tm_sec = tm.tm_min = tm.tm_hour = 0; /* ok on grex */

    auto ptr = str;
    while (*ptr == ' ') ptr++; /* skip leading spaces */

    for (i = 0; i < 7; i++) {
        if (!strncmp(wkdy[i], ptr, strlen(wkdy[i])))
            ptr += strlen(wkdy[i]);
    }
    if (*ptr == '+' || *ptr == '-') {
        sgn = (*ptr == '+') ? 1 : -1;
        ptr++;
        i = 0;
        while (isdigit(*ptr)) {
            i = i * 10 + (*ptr - '0');
            ptr++;
        }
        *tt = mktime(&tm) + sgn * i * 24 * 60 * 60;
    } else {
        /* Leading (timestamp) */
        if (*ptr == '(') { /* ) */
            ptr++;
            get_time(&tm, &ptr);
            while (*ptr && *ptr != ')') ptr++;
            if (*ptr == ')')
                ptr++;
            while (isspace(*ptr)) ptr++;
        }

        /* Get date */
        if (isdigit(*ptr))
            get_num(&a, &ptr);
        else
            get_str(&m, &ptr);
        while (*ptr == ' ' || *ptr == '/' || *ptr == '-') ptr++;
        if (isdigit(*ptr))
            get_num(&b, &ptr);
        else
            get_str(&m, &ptr);
        while (*ptr == ' ' || *ptr == '/' || *ptr == '-' || *ptr == ',') ptr++;
        if (isdigit(*ptr))
            get_num(&c, &ptr);
        if (c > 1900)
            c -= 1900;
        if (c)
            tm.tm_year = c;

        /* Assign values to date structure */
        if (m) {
            tm.tm_mon = m - 1;
            if (a)
                tm.tm_mday = a;
            else if (b)
                tm.tm_mday = b;
        } else if (a * b) {
            tm.tm_mon = a - 1;
            tm.tm_mday = b;
        }

        /* Trailing (timestamp) */
        while (isspace(*ptr)) ptr++;
        if (*ptr == '(') { /* ) */
            ptr++;
            get_time(&tm, &ptr);
            while (*ptr && *ptr != ')') ptr++;
            if (*ptr == ')')
                ptr++;

            /* Trailing timestamp */
        } else if (isdigit(*ptr))
            get_time(&tm, &ptr);
        /* do we need to advance ptr? */

        *tt = mktime(&tm);
        memcpy(&tm, localtime(tt), sizeof(tm));
    }
    return ptr;
}
