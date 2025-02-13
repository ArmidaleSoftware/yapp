// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include "config.h"

#include <print>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>

#include "yapp.h"
#include "struct.h"
#include "files.h"

#define MAX_LIST_LENGTH 300

flag_t debug = 0;   /* user settable parameter flags */
flag_t flags = 0;   /* user settable parameter flags */
flag_t status = 0;  /* system status flags           */
uid_t uid;          /* User's UID                    */
std::string login;  /* User's login                  */
std::string bbsdir; /* Directory for bbs files       */

std::vector<assoc_t> conflist; /* System table of conferences   */

/******************************************************************************/
/* GENERATE STRING WITHOUT ANY _'s IN IT */
/******************************************************************************/
char *
strip_(char *s) /* ARGUMENTS: Original string  */
{
    static char buff[MAX_LINE_LENGTH];
    char *p, *q;
    for (p = buff, q = s; *q; q++)
        if (*q != '_')
            *p++ = *q;
    *p = 0;
    return buff;
}

void
where(int argc, char **argv)
{
    long inode[MAX_LIST_LENGTH];
    int i, n, fl = 0;
    struct stat st;
    char buff[MAX_LINE_LENGTH], login[MAX_LINE_LENGTH];
    char word[20][MAX_LINE_LENGTH];
    FILE *fp, *pp;
    struct statfs buf;
    /* Check for -s flag */
    if (argc > 1 && str::eq(argv[1], "-s")) {
        fl = 1;
        argc--;
        argv++;
    }

    /* Load in inode index */
    for (i = 1; i < conflist.size(); i++) {
        const auto path = str::concat({conflist[i].location, "/config"});
        if (fl) {
            statfs(path.c_str(), &buf);
            if (buf.f_type == MOUNT_NFS)
                continue;
        }
        if (!stat(path.c_str(), &st))
            inode[i] = st.st_ino;
        else
            inode[i] = 0;
    }

    if ((fp = popen("/usr/bin/fstat", "r")) == NULL) {
        std::println("Can't open fstat");
        exit(1);
    }
    std::println("USER     TT      PID CMD      CONFERENCE");
    while (fgets(buff, MAX_LINE_LENGTH, fp)) {
        if (sscanf(buff, "%s%s%s%s%s%s%s", &(word[0]), &(word[1]), &(word[2]),
                &(word[3]), &(word[4]), &(word[5]), &(word[6])) < 7 ||
            !(n = atoi(word[5])))
            continue;

        for (i = 1; i < conflist.size() && inode[i] != n; i++);
        if (i < conflist.size()) {
            const auto cmd = std::format("ps -O ruser -p {}", word[2]);
            if ((pp = popen(cmd.c_str(), "r")) == NULL) {
                strcpy(word[12], word[0]);
                strcpy(word[13], "??"); /* actually this is
                                         * available */
            } else {
                fgets(buff, MAX_LINE_LENGTH, pp);
                fgets(buff, MAX_LINE_LENGTH, pp);
                /* Real thing */
                /* 8431 thaler   s0  I+     0:00.97 nvi where.c */
                /* actually this is available */
                if (sscanf(buff, "%s%s%s%s%s%s", &(word[11]), &(word[12]),
                        &(word[13]), &(word[14]), &(word[15]),
                        &(word[16])) < 6) {
                    strcpy(word[12], word[0]);
                    strcpy(word[13], "??");
                }
                fclose(pp);
            }

            /* Check user list */
            if (argc > 1) {
                for (i = 1; i < argc && !str::eq(word[12], argv[i]); i++);
                if (i == argc)
                    continue;
            }
            std::println("{:<8} {} {:8} {:<8} {}", word[12], word[13], word[2],
                word[1], strip_(conflist[i].name));
        }
    }
    fclose(fp);
}

int
main(int argc, char *argv[])
{
    bbsdir = BBSDIR;
    if (conflist = grab_list(bbsdir, "conflist")) {
        std::println("Couldn't access conflist");
        exit(1);
    }
    where(argc, argv);
    return 0;
}

void
wputs(char *s)
{
    fputs(s, stdout); /* NO newline like puts() does */
}

void
wputchar(int c)
{
    putchar(c);
}

void
error(const st::string_view &str1, const std::string_view &str2)
{
    std::println(stderr, "Got error {} ({}) in {}{}", errno, strerror(errno),
        str1, str2);
}
