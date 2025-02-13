// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

/*
 * Receive the POST infomation.
 * Output the ticket to standard out
 * Save the body in the cf.buffer in the user's directory
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <print>

#include "str.h"

/* #define LOG "/tmp/postlog" */
#undef LOG
#define MAX_ENTRIES 10000

typedef struct {
    char *name;
    char *val;
} entry;

char *
makeword(char *line, char stop);
char *
fmakeword(FILE *f, char stop, int *len);
char
x2c(char *what);
void
unescape_url(char *url);
void
plustospace(char *str);

void
ftextout(FILE *fp, char *buff)
{
    char *q = buff;
    while (*q) {
        /* Remove the Control M's from the post */
        if (*q == '\r') {
            q++;
            continue;
        }
        fputc(*q, fp);
        q++;
    }
    fputc('\n', fp);
}

void
usage(void)
{
    std::println(stderr, "Yapp {} (c)1996 Armidale Software\n", VERSION);
    std::println(
        stderr, " usage: receive_post [-p pseudofile] [[-s] subjfile]");
    std::println(
        stderr, " -p pseudofile  Save pseudonym (if any) to indicated file");
    std::println(
        stderr, " -s subjfile    Save subject (if any) to indicated file");
    exit(1);
}

int
main(int argc, char **argv)
{
#ifdef LOG
    FILE *fp;
#endif
    entry entries[MAX_ENTRIES];
    int x, m = -1, i;
    int cl;
    char *env = getenv("REQUEST_METHOD");
    char *subjfile = NULL;
    char *pseudofile = NULL;
    const char *options = "hvs:p:"; /* Command-line options */
    extern char *optarg;
    extern int optind, opterr;
    while ((i = getopt(argc, argv, options)) != -1) {
        switch (i) {
        case 's':
            subjfile = optarg;
            break;
        case 'p':
            pseudofile = optarg;
            break;
        case 'v':
        case 'h':
        default:
            usage();
        }
    }
    argc -= optind;
    argv += optind;
    if (argc == 1 && !subjfile) {
        subjfile = argv[0];
        argc--;
        argv++;
    }
    if (argc > 0)
        usage();

#ifdef LOG
    fp = fopen("/tmp/postlog", "a");
    if (!fp)
        exit(1);
    std::println(fp, "receive_post began");
    std::println(fp, "method={}", env);
    fflush(fp);
#endif

    if (!env || !str::eq(env, "POST")) {
        std::println("This script should be referenced with a METHOD of POST.");
        std::print("If you don't understand this, see this ");
        std::println(
            "<A HREF=\"http://www.ncsa.uiuc.edu/SDG/Software/Mosaic/Docs/"
            "fill-out-forms/overview.html\">forms overview</A>.\n");
        exit(1);
    }
#ifdef LOG
    std::println(fp, "got past POST check");
    fflush(fp);
#endif

    env = getenv("CONTENT_TYPE");
    if (!env || !str::eq(env, "application/x-www-form-urlencoded")) {
        std::println("This script can only be used to decode form results.");
        exit(1);
    }
#ifdef LOG
    std::println(fp, "got past content type");
    fflush(fp);
#endif

    env = getenv("CONTENT_LENGTH");
    if (!env)
        cl = 0;
    else
        cl = atoi(env);

#ifdef LOG
    std::println(fp, "length={}", cl);
    fflush(fp);
#endif

    for (x = 0; cl && (!feof(stdin)); x++) {
        m = x;
        entries[x].val = fmakeword(stdin, '&', &cl);
        plustospace(entries[x].val);
        unescape_url(entries[x].val);
        entries[x].name = makeword(entries[x].val, '=');
    }

#ifdef LOG
    std::println(fp, "lines={}", m);
    fflush(fp);
#endif

    for (x = 0; x <= m; x++) {
        if (str::eq(entries[x].name, "ticket") ||
            str::eq(entries[x].name, "tkt")) {
            std::print("{}", entries[x].val);
#ifdef LOG
            std::println(fp, "ticket=!{}!", entries[x].val);
            fflush(fp);
#endif
        } else if (str::eq(entries[x].name, "text")) {
#ifdef LOG
            std::println(fp, "text=!{}!", entries[x].val);
            fflush(fp);
#endif
            ftextout(stderr, entries[x].val); /* to strip bad
                                               * characters */
            /*std::println(stderr, "{}", entries[x].val);*/
#ifdef LOG
            std::println(fp, "textok", entries[x].val);
            fflush(fp);
#endif
        } else if (str::eq(entries[x].name, "subj") && subjfile) {
            FILE *sfp;
#ifdef LOG
            std::println(fp, "subj=!{}!", entries[x].val);
            fflush(fp);
#endif
            if ((sfp = fopen(subjfile, "w")) != NULL) {
                std::println(sfp, "{}", entries[x].val);
                fclose(sfp);
#ifdef LOG
                std::println(fp, "wrote subject to {}", subjfile);
                fflush(fp);
#endif
            } else {
                std::println("Can't open {}", subjfile);
#ifdef LOG
                std::println(fp, "can't open {}", subjfile);
                fflush(fp);
#endif
                exit(1);
            }
        } else if (str::eq(entries[x].name, "pseudo") && pseudofile) {
            FILE *sfp;
            if ((sfp = fopen(pseudofile, "w")) != NULL) {
                std::println(sfp, "{}", entries[x].val);
                fclose(sfp);
            } else {
                std::println("Can't open {}", pseudofile);
                exit(1);
            }
        }
#ifdef LOG
        std::println(fp, "completed {}/{}", x, m);
        fflush(fp);
#endif
    }
#ifdef LOG
    std::println(fp, "exiting normally");
    fclose(fp);
#endif
    exit(0);
}
