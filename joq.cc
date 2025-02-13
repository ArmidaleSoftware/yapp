// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include "joq.h"

#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <format>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include "conf.h"
#include "files.h"
#include "globals.h"
#include "help.h"
#include "lib.h"
#include "log.h"
#include "misc.h"
#include "security.h"
#include "stats.h"
#include "str.h"
#include "struct.h"
#include "sum.h"
#include "system.h"
#include "user.h"
#include "yapp.h"

/******************************************************************************/
/* DISPATCH CONTROL TO APPROPRIATE JOQ COMMAND FUNCTION                       */
/******************************************************************************/
char
joq_cmd_dispatch(/* ARGUMENTS:                  */
    int argc,    /* Number of arguments      */
    char **argv  /* Argument list            */
)
{
    if (match(argv[0], "r_egister") || match(argv[0], "j_oin") ||
        match(argv[0], "p_articipate")) {
        if (confidx >= 0)
            leave(0, (char **)0);
        write_part("");
        st_new.c_status |= CS_JUSTJOINED;
        mode = M_OK;

        /* Unless ulist file is referenced in an acl, add login to
         * ulist */
        const auto config = get_config(joinidx);
        if (!config.empty()) {
            if (is_auto_ulist(joinidx)) {
                if (!is_inlistfile(joinidx, "ulist")) {
                    const auto path =
                        str::join("/", {conflist[joinidx].location, "ulist"});
                    write_file(path, std::format("{}\n", login));
                    custom_log("newjoin", M_OK);
                }
            }
        }
    } else if (match(argv[0], "o_bserver")) {
        sumentry_t sum2[MAX_ITEMS];
        short i;
        if (confidx >= 0)
            leave(0, (char **)0);
        st_new.c_status |= (CS_OBSERVER | CS_JUSTJOINED);
        mode = M_OK;

        /* Initialize part[] */
        for (i = 0; i < MAX_ITEMS; i++) {
            part[i].nr = part[i].last = 0;
            dirty_part(i);
        }
        get_status(&st_new, sum2, part, joinidx);
        st_new.sumtime = 0;
        for (i = st_new.i_first + 1; i < st_new.i_last; i++)
            time(&(part[i - 1].last));
    } else if (match(argv[0], "h_elp"))
        help(argc, argv);
    else if (match(argv[0], "q_uit")) {
        status |= S_QUIT;
        mode = M_OK;
    } else
        return misc_cmd_dispatch(argc, argv);
    return 1;
}

static void
write_part2(const std::string &partfile, status_t *stt, sumentry_t *sum3)
{
    if (debug & DB_PART) {
        std::println("after split: Partfile={}", partfile);
        fflush(stdout);
    }
    if (debug & DB_PART)
        std::println("file {} uid {} euid {}", partfile, getuid(), geteuid());

    /* KKK*** in the future, allocate string array of #items+2, save lines
     * in there, call dump_file, and free the array */
    const auto path = std::format("{}.new", partfile);
    auto ferrno = 0;
    FILE *fp = mopen(path, O_W);
    if (fp == NULL) /* "w" */
        exit(1);
    if (debug & DB_PART)
        std::println("open succeeded");
    std::println(fp, "!<pr03>");
    std::println(fp, "{}", stt->fullname);

    if (debug & DB_PART)
        std::println("first {} last {}", stt->i_first, stt->i_last);
    for (auto i = stt->i_first; i <= stt->i_last; i++) {
        if (debug & DB_PART)
            std::print("sum3[{}]={} ", i - 1, sum3[i - 1].nr);
        if (sum3[i - 1].nr || part[i - 1].last) {
            std::println(fp, "{} {} {:X}", i, part[i - 1].nr, part[i - 1].last);
            fflush(fp);
            if (debug & DB_PART)
                std::print(": {} {} {:X}", i, part[i - 1].nr, part[i - 1].last);
        }
        if (debug & DB_PART)
            std::print("\n");
    }
    ferrno = ferror(fp);
    mclose(fp);

    /* Now atomically replace the old participation file */
    if (!ferrno)
        rename(path.c_str(), partfile.c_str());
    else {
        error("writing ", partfile);
        unlink(path.c_str());
    }
}
/******************************************************************************/
/* WRITE OUT A USER PARTICIPATION FILE FOR THE CURRENT CONFERENCE             */
/******************************************************************************/
/* Takes filename */
void
write_part(const std::string &partfile)
{
    std::string file;
    short i, cpid, wpid;
    status_t *stt;
    sumentry_t sum2[MAX_ITEMS], *sum3;

    if (st_glob.c_status & CS_OBSERVER)
        return;

    if (!partfile.empty()) {
        file = partfile;
        sum3 = sum;
        stt = &st_glob;
    } else {
        const auto config = get_config(joinidx);
        if (config.empty() || config.size() <= CF_PARTFILE)
            return;
        file = config[CF_PARTFILE];
        sum3 = sum2;
        stt = &st_new;

        /* Initialize part[] */
        for (i = 0; i < MAX_ITEMS; i++) {
            part[i].nr = part[i].last = 0;
            dirty_part(i);
        }
        get_status(stt, sum2, part, joinidx);
        if (flags & O_UNSEEN) {
            for (i = st_new.i_first + 1; i < st_new.i_last; i++)
                time(&(part[i - 1].last));
        }
        stt->sumtime = 0;
    }

    /* Create WORK/.name.cf */
    const auto path = str::join("/", {partdir, file});
    if (debug & DB_PART) {
        std::println("before split: Partfile={}", path);
        fflush(stdout);
    }
    if (partfile_perm() == SL_OWNER) {
        write_part2(path, stt, sum3);
        return;
    }

    /* FORK */
    fflush(stdout);
    if (status & S_PAGER)
        fflush(st_glob.outp);

    cpid = fork();
    if (cpid) { /* parent */
        if (cpid < 0)
            return; /* error: couldn't fork */
        while ((wpid = wait((int *)0)) != cpid && wpid != -1);
    } else { /* child */
        signal(SIGINT, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);
        close(0); /* make sure we don't touch stdin */

        setuid(getuid());
        setgid(getgid());

        write_part2(path, stt, sum3);
        exit(0);
    } /* ENDFORK */

    if (debug & DB_PART)
        std::println("write_part: fullname={}", st_glob.fullname);
}
/******************************************************************************/
/* READ IN A USER PARTICIPATION FILE FOR SOME CONFERENCE                      */
/******************************************************************************/
char
read_part(                       /* ARGUMENTS:       */
    const std::string &partfile, /* Filename         */
    partentry_t part[MAX_ITEMS], /* Array to fill in */
    status_t *stt, int idx)
{
    for (auto i = 0; i < MAX_ITEMS; i++) {
        part[i].nr = part[i].last = 0;
        dirty_part(i);
    }
    stt->fullname = st_glob.fullname;
    const auto partf = grab_file(partdir, partfile, GF_SILENT);
    if (partf.empty()) {
        /* Newly joined, Initialize part[] */
        for (auto i = 0; i < MAX_ITEMS; i++) part[i].nr = part[i].last = 0;
        sumentry_t sum2[MAX_ITEMS];
        get_status(stt, sum2, part, idx);
        stt->sumtime = 0;
        for (auto i = stt->i_first + 1; i < stt->i_last; i++)
            time(&(part[i - 1].last));
        return 0;
    }
    if (!str::eq(partf[0], "!<pr03>"))
        std::println("Invalid participation file format.");
    else if (partf.size() > 1)
        stt->fullname = partf[1];
    for (const auto &pf : partf) {
        short a, b;
        long d;
        sscanf(pf.c_str(), "%hd %hd %lx", &a, &b, &d);
        if (a >= 1 && a <= MAX_ITEMS) {
            part[a - 1].nr = b;
            part[a - 1].last = d;
        }
    }
    const auto path = str::join("/", {partdir, partfile});
    struct stat st{};
    if (!stat(path.c_str(), &st) && st.st_size > 0)
        stt->parttime = st.st_mtime;
    if (debug & DB_PART)
        std::println("read_part: fullname={}", st_glob.fullname);
    return 1;
}
