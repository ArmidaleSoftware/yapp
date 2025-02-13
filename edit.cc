// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

/* Routines for dealing with editing responses and subjects */

/*
 * Caveat: note that another process may be in the middle of reading or
 * writing to the item file, and the sum file contains offsets into the
 * item file which will all change if the header or the response text
 * changes.
 *
 * One option would be, for editing a response, to scribble the old
 * response, and copy the text to the entry buffer and let you enter
 * a new one (which is completely safe).
 *
 * Another option would be to go to a DBM format item file which would
 * obviate the need for keeping offsets.
 *
 * Another option would be to lock the sum file, wait a second
 * or two, then do the change and regenerate the sum file.  This is not
 * safe because it doesn't 100% guarantee someone else won't be messed up
 * (say they're going to do a scribble and you change the subject to be
 * longer or shorter...).  If only the sum file is locked, changes are
 * pretty good though, that Yapp will be done reading the item file
 * within a second or two.
 *
 * Editing the last response in the file should be safe, however, since no
 * offsets in the sum file change.
 *
 * So, for editing responses, modify the text only if it's the last response
 * and do the scribble/append otherwise.
 */

#include <sys/types.h>
#include <sys/unistd.h>
#include <sys/wait.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <format>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include "arch.h"
#include "conf.h"
#include "edbuf.h"
#include "files.h"
#include "globals.h"
#include "item.h"
#include "lib.h"
#include "license.h"
#include "log.h"
#include "macro.h"
#include "main.h"
#include "range.h"
#include "rfp.h"
#include "sep.h"
#include "stats.h"
#include "str.h"
#include "struct.h"
#include "sum.h"
#include "system.h"
#include "user.h"
#include "yapp.h"

/* n is # of spaces in range [0,512] */
char *
spaces(int n)
{
    static char spc[MAX_LINE_LENGTH], init = 0;
    if (!init) {
        memset(spc, ' ', sizeof(spc) - 1);
        spc[sizeof(spc) - 1] = '\0';
        init = 1;
    }
    return (n >= 0) ? spc + sizeof(spc) - 1 - n : spc + sizeof(spc) - 1;
}

namespace
{
int
dump_file(const std::string &dir,         /* IN: Directory to put file in    */
    const std::string &filename,          /* IN: Filename to write text into */
    const std::vector<std::string> &text, /* IN: Text to write out           */
    int mod                               /* IN: File open mode              */
)
{
    std::string path(dir);
    if (!filename.empty()) {
        path.append("/");
        path.append(filename);
    }
    FILE *fp = mopen(path, mod);
    if (fp == NULL)
        return 0;
    for (const auto &line : text) std::println(fp, "{}", line);
    mclose(fp);
    return 1;
}
} // namespace
/*
 * Change subject of st_glob.i_current item in confidx conference
 * We ONLY allow this if the offsets don't change (i.e., new subject fits
 * into space holding old subject).  Newly created items now hold 78 bytes
 * of space for the subject, regardless of the string length.
 */
int
retitle(int argc, char **argv)
{
    char act[MAX_ITEMS];

    rangeinit(&st_glob, act);

    if (argc < 2) {
        std::println("Error, no {} specified! (try HELP RANGE)", topic());
    } else { /* Process args */
        range(argc, argv, &st_glob.opt_flags, act, sum, &st_glob, 0);
    }

    /* Process items */
    for (auto j = st_glob.i_first; j <= st_glob.i_last && !(status & S_INT);
        j++) {
        if (!act[j - 1] || !sum[j - 1].flags)
            continue;

        /* Check for permission */
        if (!(st_glob.c_status & CS_FW) && !is_enterer(j)) {
            std::println("You can't do that!");
            continue;
        }
        if (!(flags & O_QUIET)) {
            std::println(
                "Old subject was:\n> {}", get_subj(confidx, j - 1, sum));
            std::println("Enter new {} or return to keep old", subject());
            std::print("? ");
            std::fflush(stdout);
        }
        std::string sub;
        ngets(sub, st_glob.inp); /* st_glob.inp */
        if (sub.empty())
            return 1; /* keep old */

        /* Expand seps in subject IF first character is % */
        if (sub[0] == '%') {
            const char *str, *f;
            str = sub.c_str() + 1;
            f = get_sep(&str);
            sub = f;
        }

        /* Determine length of old subject */
        const auto itemfile =
            std::format("{}/_{}", conflist[confidx].location, j);
        const auto header = grab_file(itemfile, "", GF_HEADER);
        if (header.size() < 2)
            continue;
        /* Do some error checking, should never happen */
        auto len = header[1].size() - 2;
        if (len > MAX_LINE_LENGTH) {
            error("subject too long in ", itemfile);
            return 1;
        }

        /* Truncate if necessary */
        if (sub.size() > len) {
            sub.erase(len);
            if (!(flags & O_QUIET))
                std::print("Truncated subject to: {}", sub);
        }

        /* Store in memory */
        store_subj(confidx, j - 1, sub);

        /* Store into item file */
        FILE *fp = mopen(itemfile, O_RPLUS);
        if (fp == NULL) {
            custom_log("retitle", M_RFP);
            continue;
        }
        if (fseek(fp, 10L, 0)) {
            error("fseeking in ", itemfile);
            mclose(fp);
            continue;
        }
        if (len - sub.size() < MAX_LINE_LENGTH) {
            std::println(fp, "{}{}", sub, spaces(len - sub.size()));
            mclose(fp);
            continue;
        }
        int spaces_added;
        int spaces_to_add = len - sub.size();
        /* Add new text */
        std::print(fp, "{}", sub);
        for (spaces_added = 0; spaces_to_add - spaces_added >= MAX_LINE_LENGTH;
            spaces_added += (MAX_LINE_LENGTH - 1)) {
            std::print(fp, "{}", spaces(MAX_LINE_LENGTH - 1));
        }
        /* Add any remaining spaces */
        std::println(fp, "{}", spaces(spaces_to_add - spaces_added));
        mclose(fp);
#ifdef SUBJFILE
        XXX /* Fill this in if we ever use SUBJFILE */
#endif
            custom_log("retitle", M_RFP);
    }
    return 1;
}
/******************************************************************************
 * EDIT A RESPONSE IN THE CURRENT ITEM                                        *
 *                                                                            *
 * If the new text fits into the old space, we overwrite it (padding it if    *
 * necessary).  If not, we just scribble the old response, and add a new      *
 * one at the end of the item.                                                *
 ******************************************************************************/
/* ARGUMENTS:               */
/* Number of arguments      */
/* Argument list            */
int
modify(int argc, char **argv)
{
    char buff[MAX_LINE_LENGTH];
    int i, j, n, oldlen, newlen;
    int cpid, wpid;
    int statusp, ok;
    extern FILE *ext_fp;

    if (st_glob.c_security & CT_EMAIL) {
        wputs("Can't modify in an email conference!\n");
        return 1;
    }
#ifdef NEWS
    if (st_glob.c_security & CT_NEWS) {
        wputs("Can't modify in a news conference!\n");
        return 1;
    }
#endif

    /* Validate arguments */
    if (argc < 2 || sscanf(argv[1], "%d", &i) < 1) {
        wputs("You must specify a response number.\n");
        return 1;
    } else if (argc > 2) {
        std::println("Bad parameters near \"{}\"", argv[2]);
        return 2;
    }
    refresh_sum(0, confidx, sum, part, &st_glob);
    if (i < 0 || i >= sum[st_glob.i_current - 1].nr) {
        wputs("Can't go that far! near \"<newline>\"\n");
        return 1;
    }

    /* Check for permission to modify (only the author can modify) */
    if (!re[i].date)
        get_resp(ext_fp, &(re[i]), GR_HEADER, i);
    /*
     * Only match logins if coming from or entered from web.  Slightly
     * dangerous, but it's better than not letting the author edit
     */
    if (uid == get_nobody_uid() || re[i].uid == get_nobody_uid())
        ok = str::eq(login, re[i].login);
    else
        ok = (uid == re[i].uid);
    if (!ok) {
        std::println("You don't have permission to affect response {}.", i);
        return 1;
    }
    if (sum[st_glob.i_current - 1].flags & IF_FROZEN) {
        wputs(std::format("Cannot modify frozen %ss!\n", topic()));
        return 1;
    }
    if (re[i].flags & RF_SCRIBBLED) {
        wputs("Cannot modify a scribbled response!\n");
        return 1;
    }
    if (re[i].offset < 0) {
        wputs("Offset error.\n"); /* should never happen */
        return 1;
    }

    /* Get old text */
    const auto item_path =
        std::format("{}/_{}", conflist[confidx].location, st_glob.i_current);
    FILE *fp = mopen(item_path, O_R);
    if (fp == NULL)
        return 1;
    get_resp(fp, &(re[i]), GR_ALL, i);
    mclose(fp);

    /* Fork & setuid down when creating cf.buffer */
    fflush(stdout);
    if (status & S_PAGER)
        fflush(st_glob.outp);

    cpid = fork();
    if (cpid) { /* parent */
        if (cpid < 0)
            return -1; /* error: couldn't fork */
        while ((wpid = wait(&statusp)) != cpid && wpid != -1);
    } else { /* child */
        if (setuid(getuid()))
            error("setuid", "");
        setgid(getgid());

        /* Save to cf.buffer */
        dump_file(work, "cf.buffer", re[i].text, O_W);

        exit(0);
    }

    /* Get new text */
    std::vector<std::string> text;
    if (!text_loop(0, "text") ||
        ((text = grab_file(work, "cf.buffer", GF_NOHEADER)), text.empty())) {
        re[i].text.clear();
        return 1;
    }

    /* Write old text to censorlog */
    const auto path = str::concat({bbsdir, "/censored"});
    if ((fp = mopen(path, O_A | O_PRIVATE)) != NULL) {
        std::println(fp, ",C {} {} {} resp {} rflg {} {},{} {} date {}",
            conflist[confidx].location, topic(), st_glob.i_current, i,
            RF_CENSORED | RF_SCRIBBLED, login, uid,
            get_date(time((time_t *)0), 0), fullname_in_conference(&st_glob));
        std::println(fp, ",R{:04X}", re[i].flags);
        std::println(fp, ",U{},{}", re[i].uid, re[i].login);
        std::println(fp, ",A{}", re[i].fullname);
        std::println(fp, ",D{:08X}", re[i].date);
        std::println(fp, ",T");
        for (const auto &line : re[i].text) std::println(fp, "{}", line);
        std::println(fp, ",E");
        mclose(fp);
    }

    /* Escape the new text if needed, and see if it fits into the old space */
    oldlen = (re[i].endoff - 3) - re[i].textoff;
    n = text.size();
    for (newlen = 0, j = 0; j < n; j++) {
        if (text[j][0] == ',') {
            text[j].insert(0, ",,");
        }
        newlen += text[j].size() + 1; /* count 1 for the newline */
    }

    /* Overwrite the old text with either scribbling or with the new text */
    if ((fp = mopen(buff, O_RPLUS)) != NULL) {
        int is_at_end = 0;
        /* If it's the last response, the space can be extended */
        fseek(fp, 0L, SEEK_END);
        if (ftell(fp) == re[i].endoff) {
            oldlen = newlen + str::toi(get_conf_param("padding", PADDING));
            is_at_end = 1;
        }

        /* Append if the new text is longer */
        if (newlen > oldlen) {
            fseek(fp, re[i].offset, 0);
            std::println(fp, ",R{:04}", RF_CENSORED | RF_SCRIBBLED);
            fseek(fp, re[i].textoff, 0);
            const auto over =
                std::format("{} {} {} ", login, get_date(time((time_t *)0), 0),
                    fullname_in_conference(&st_glob));
            const auto len = over.size();
            for (j = oldlen; j > 76; j -= 76) {
                for (size_t k = 0; k < 75; k++)
                    std::print(fp, "{:c}", over[k % len]);
                std::println(fp, "");
            }
            for (size_t k = 0; k < j - 1; k++)
                std::println(fp, "{:c}", over[k % len]);
            std::println(fp, "");
            std::println(fp, ",E");
        } else {
            /* Replace if the new text fits in the old space */
            int k;
            /* Write new timestamp */
            for (k = 14; k > 7; k--) {
                int c;
                fseek(fp, re[i].textoff - k, 0);
                c = getc(fp);
                if (c == ',')
                    break;
            }
            if (k == 7) {
                const auto errmsg = std::format("{} #{}.{}",
                    compress(conflist[confidx].name).c_str(), st_glob.i_current,
                    i);
                error("lost date in ", errmsg);
            } else {
                fseek(fp, re[i].textoff - k, 0);
                std::println(fp, ",D{:08X}", time(NULL));
            }

            /* Write new text and pad with spaces after the ,E */
            fseek(fp, re[i].textoff, 0);
            for (j = 0; j < n - 1; j++) std::println(fp, "{}", text[j]);
            if (oldlen - newlen < MAX_LINE_LENGTH) {
                std::println(fp, "{}", text[j]);
                std::println(fp, ",E{}", spaces(oldlen - newlen));
            } else {
                int spaces_added;
                int spaces_to_add = oldlen - newlen;
                std::println(fp, "{}", text[j]);
                std::print(fp, ",E");
                for (spaces_added = 0;
                    spaces_to_add - spaces_added >= MAX_LINE_LENGTH;
                    spaces_added += (MAX_LINE_LENGTH - 1)) {
                    std::print(fp, "{}", spaces(MAX_LINE_LENGTH - 1));
                }

                /* Add any remaining spaces */
                std::println(fp, "{}", spaces(spaces_to_add - spaces_added));
            }

            if (is_at_end)
                ftruncate(fileno(fp), ftell(fp));
        }
        mclose(fp);

        /* Added 4/18, since sum file wasn't being updated, causing
         * set sensitive to fail. */
        sum[st_glob.i_current - 1].last = time((time_t *)0);
        save_sum(sum, (short)(st_glob.i_current - 1), confidx, &st_glob);
        dirty_sum(st_glob.i_current - 1);
    }
    if (newlen > oldlen) {
        /* At this point, the old text is scribbled, and the new text
         * is in text, so just do a normal add response. */
        add_response(&sum[st_glob.i_current - 1], text, confidx, sum, part,
            &st_glob, 0, "", uid, login, re[i].fullname, re[i].parent);
    }

    /* free_sum(sum); unneeded, always SF_FAST */
    re[i].text.clear();

    custom_log("edit", M_RFP);

    /* Remove the cf.buffer file now that we are done */
    const auto cfbuffer = str::concat({work, "/cf.buffer"});
    rm(cfbuffer, SL_USER);

    return 1;
}
