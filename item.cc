// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include "item.h"

#include <sys/stat.h>

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <cstdio>
#include <format>
#include <print>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "arch.h"
#include "conf.h"
#include "driver.h"
#include "edbuf.h"
#include "edit.h"
#include "files.h"
#include "globals.h"
#include "joq.h"
#include "lib.h"
#include "license.h"
#include "log.h"
#include "macro.h"
#include "main.h"
#include "news.h"
#include "range.h"
#include "security.h"
#include "sep.h"
#include "stats.h"
#include "str.h"
#include "struct.h"
#include "sum.h"
#include "system.h"
#include "yapp.h"

FILE *ext_fp;
short ext_first, ext_last; /* for 'again' */
/*
 * Test to see if the current user entered the given item in the current
 * conference
 */
/* takes item number      */
int
is_enterer(int item)
{
    response_t tempre[MAX_RESPONSES];

    const auto path = std::format("{}/_{}", conflist[confidx].location, item);
    FILE *fp = mopen(path, O_R);
    if (fp == nullptr)
        return 0;
    get_item(fp, item, tempre, sum);
    get_resp(fp, re, (short)GR_HEADER, (short)0); /* Get header of #0 */
    mclose(fp);
    return (re[0].uid == uid) && str::eq(re[0].login, login);
}
/******************************************************************************/
/* CREATE A NEW ITEM                                                          */
/******************************************************************************/
int
do_enter(sumentry_t *sumthis,             /* New item summary */
    const std::string_view &sub,          /* New item subject */
    const std::vector<std::string> &text, /* New item text    */
    int idx, sumentry_t *sum, partentry_t *part, status_t *stt, long art,
    const std::string_view &mid, int uid, const std::string_view &login,
    const std::string_view &fullname)
{
    /* item = ++(stt->i_last);  * next item number */
    auto item = ++(stt->c_confitems); /* next item number */
    if (!art && !(flags & O_QUIET))
        std::print("Saving as {} {}...", topic(), item);
    if (debug & DB_ITEM)
        std::println("{} flags={} sub=!{}!", topic(), sumthis->flags, sub);
    const auto path = std::format("{}/_{}", conflist[idx].location, item);
    long mod = O_W | O_EXCL;
    if (stt->c_security & CT_BASIC)
        mod |= O_PRIVATE;
    FILE *fp = mopen(path, mod);
    if (fp == NULL)
        return 0;
    std::println(fp, "!<ps02>\n,H{}{}\n,R{:04X}\n,U{},{}\n,A{}\n,D{:08X}\n,T",
        sub, spaces(str::toi(get_conf_param("padding", PADDING)) - sub.size()),
        RF_NORMAL, uid, login, fullname, (unsigned long)sumthis->first);

    if (art) {
        std::println(fp, ",N{:06d}", art);
        std::println(fp, ",M{}", mid);
    } else {
        for (auto line = 0uz; line < text.size(); line++) {
            if (text[line][0] == ',')
                std::print(fp, ",,");
            std::println(fp, "{}", text[line]);
        }
    }
    std::println(
        fp, ",E{}", spaces(str::toi(get_conf_param("padding", PADDING))));
    fflush(fp);
    if (!std::ferror(fp)) {
        time(&(part[item - 1].last));
        part[item - 1].nr = sumthis->nr;
        dirty_part(item - 1);
    } else
        error("writing ", topic());

    /* If sum doesn't exist, we must make sure the data has been written
     * before the sum file is created. */
    memcpy(&sum[item - 1], sumthis, sizeof(sumentry_t));
    save_sum(sum, (short)(item - 1), idx, stt);
    dirty_sum(item - 1);
    mclose(fp);
    store_subj(idx, item - 1, sub);
    store_auth(idx, item - 1, login);
#ifdef SUBJFILE
    update_subj(idx, item - 1); /* update subjects file */
#endif
    if (!art && !(flags & O_QUIET))
        wputs("saved.\n");
    stt->i_numitems++;
    return 1;
}

/******************************************************************************/
/* ENTER A NEW ITEM                                                           */
/******************************************************************************/
int
enter(          /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    std::string sub;
    sumentry_t sumthis;

    if (argc > 1) {
        std::println("Bad {} range near \"{}\"", topic(), argv[1]);
        return 1;
    }
    refresh_sum(0, confidx, sum, part, &st_glob);

    if (!check_acl(ENTER_RIGHT, confidx)) {
        std::println("You are not allowed to enter new {}s.", topic());
        return 1;
    }

    if (!st_glob.i_numitems && !(st_glob.c_status & CS_FW)) {
        const auto msg =
            std::format("Only the {} can enter the first {}.  Sorry!\n",
                fairwitness(), topic());
        wputs(msg);
        return 1;
    }
    if (st_glob.i_last == MAX_ITEMS) { /* numbered 1-last */
        wputs(std::format("Too many %ss.\n", topic()));
        return 1;
    }
#ifdef NEWS
    if (st_glob.c_security & CT_NEWS) {
        make_rnhead(re, 0);
        const auto rnh = str::concat({home, "/.rnhead"});
        const auto cmd = str::concat({"Pnews -h ", rnh});
        unix_cmd(cmd);
        rm(rnh, SL_USER);
        return 1;
    }
#endif

    const auto cfbufr = str::concat({work, "/cf.buffer"});
    if (text_loop(1, "text")) {
        auto file = grab_file(work, "cf.buffer", false);
        if (file.empty()) {
            wputs("can't open cfbuffer\n");
        } else {
            /* Get the subject */
            do {
                if (!(flags & O_QUIET))
                    std::print(
                        "Enter a one line {} or ':' to edit\n? ", subject(0));
                ngets(sub, st_glob.inp); /* st_glob.inp */

                if (str::eq(sub, ":")) {
                    file.clear();
                    if (!edit(cfbufr, "", 0) ||
                        ((file = grab_file(work, "cf.buffer", false)),
                            file.empty())) {
                        wputs("can't open cfbuffer\n");
                        return 1;
                    }
                    sub.clear();
                    continue;
                }
                const auto prompt =
                    std::format("Ok to abandon {} entry? ", topic());
                if (sub.empty() && get_yes(prompt, true)) {
                    rm(cfbufr, SL_USER);
                    wputs(
                        std::format("No text -- {} entry aborted!\n", topic()));
                    return 1;
                }
            } while (sub.empty());

            /* Expand seps in subject IF first character is % */
            if (sub[0] == '%') {
                const char *str, *f;
                str = sub.c_str() + 1;
                f = get_sep(&str);
                sub = f;
            }
            {

                /* Replace any control characters with spaces */
                char *p;
                for (p = sub.data(); *p; p++)
                    if (iscntrl(*p))
                        *p = ' ';
            }

            /* Verify item entry */
            const auto prompt = std::format("Ok to enter this {}? ", topic());
            if (!get_yes(prompt, false)) {
                rm(cfbufr, SL_USER);
                wputs(std::format("No text -- {} entry aborted!\n", topic()));
                return 1;
            }
            sumthis.nr = 1;
            sumthis.flags = IF_ACTIVE;
            sumthis.last = time(&(sumthis.first));

            /* make sure nothing changed since we started */
            refresh_sum(0, confidx, sum, part, &st_glob);
            /* dont need to free anything */

            if (st_glob.c_security & CT_EMAIL) {
                char cfbufr2[MAX_LINE_LENGTH];
                const auto cfbuf2 = cfbufr + ".tmp";
                move_file(cfbufr, cfbufr2, SL_USER);

                store_subj(confidx, st_glob.i_numitems, sub);
                st_glob.i_current = st_glob.i_numitems + 1;
                make_emhead(re, 0);

                const auto cat = std::format("cat {} >> {}", cfbuf2, cfbufr);
                unix_cmd(cat);
                rm(cfbufr2, SL_USER);

                make_emtail();
                const auto sendmail = std::format(
                    "{} -t < {}", get_conf_param("sendmail", SENDMAIL), cfbufr);
                unix_cmd(sendmail);
            } else {
                do_enter(&sumthis, sub, file, confidx, sum, part, &st_glob, 0,
                    "", uid, login, fullname_in_conference(&st_glob));
            }

            refresh_stats(sum, part, &st_glob);
            st_glob.i_current = st_glob.i_last;
            custom_log("enter", M_RFP);
        }
    }
    rm(cfbufr, SL_USER);
    return 1;
}
/******************************************************************************/
/* DISPLAY CURRENT ITEM HEADER                                                */
/* Only requires st_glob.i_current to be set, nothing else                    */
/******************************************************************************/
void
show_header(void)
{
    FILE *fp;
    std::string buff;
    int scrib = 0;
    open_pipe();

    const auto path =
        std::format("{}/_{}", conflist[confidx].location, st_glob.i_current);
    if ((fp = mopen(path, O_R)) != NULL) {
        st_glob.r_current = 0; /* current resp = header */
        get_item(fp, st_glob.i_current, re, sum);
        get_resp(fp, re, (short)GR_HEADER, (short)0); /* Get header of #0 */
        /* The problem here is that itemsep uses r_current as an index
           to the information in re, so we can't show # new responses
              st_glob.r_current = sum[st_glob.i_current-1].nr
               - abs(part[st_glob.i_current-1].nr);
         */

        /* Get info about the actual item text if possible */
        if (!(re[st_glob.r_current].flags & (RF_EXPIRED | RF_SCRIBBLED)))
            get_resp(
                fp, &(re[st_glob.r_current]), (short)GR_ALL, st_glob.r_current);

        if ((re[st_glob.r_current].flags & RF_SCRIBBLED) &&
            re[st_glob.r_current].numchars > 7) {
            fseek(fp, re[st_glob.r_current].textoff, 0);
            ngets(buff, fp);
            re[st_glob.r_current].text.clear();
            re[st_glob.r_current].text.push_back(buff);
            scrib = 1;
        }
        if (flags & O_LABEL)
            sepinit(IS_CENSORED | IS_UID | IS_DATE);
        itemsep(
            expand((st_glob.opt_flags & OF_int) ? "ishort" : "isep", DM_VAR),
            0);

        /* If we've allocated the text, free it */
        if (!scrib)
            re[st_glob.r_current].text.clear();

        st_glob.r_current = 0; /* current resp = header */
        mclose(fp);
    }
}

void
show_nsep(FILE *fp)
{
    short tmp, i = st_glob.r_first;
    tmp = st_glob.r_current;

    if (st_glob.since) {
        while (
            i < sum[st_glob.i_current - 1].nr && st_glob.since > re[i].date) {
            i++;
            get_resp(fp, &(re[i]), (short)GR_HEADER, i);
            if (i >= sum[st_glob.i_current - 1].nr)
                break;
        }
    } else if (!st_glob.r_first)
        i = abs(part[st_glob.i_current - 1].nr);
    if (!i)
        i++;

    st_glob.r_current = sum[st_glob.i_current - 1].nr - i; /* calc # new */
    itemsep(expand("nsep", DM_VAR), 0);
    st_glob.r_current = tmp;
}
/******************************************************************************/
/* SHOW CURRENT RESPONSE                                                      */
/******************************************************************************/
void
show_resp(FILE *fp)
{
    std::string buff;
    int pr = 0;

    /* Get resp to initialize stats like %s (itemsep) */
    get_resp(fp, &(re[st_glob.r_current]), (short)GR_HEADER, st_glob.r_current);
    if (!(re[st_glob.r_current].flags & (RF_EXPIRED | RF_SCRIBBLED))) {
        get_resp(
            fp, &(re[st_glob.r_current]), (short)GR_ALL, st_glob.r_current);
        if (re[st_glob.r_current].flags & RF_CENSORED)
            pr = ((st_glob.opt_flags & OF_NOFORGET) || !(flags & O_FORGET));
        else
            pr = 1;
    }
    if ((re[st_glob.r_current].flags & RF_SCRIBBLED) &&
        re[st_glob.r_current].numchars > 7) {
        fseek(fp, re[st_glob.r_current].textoff, 0);
        ngets(buff, fp);
        re[st_glob.r_current].text.clear();
        re[st_glob.r_current].text.push_back(buff);
    }

    /* Print response header */
    if (st_glob.r_current) {
        if (flags & O_LABEL)
            sepinit(IS_PARENT | IS_CENSORED | IS_UID | IS_DATE);
        itemsep(expand("rsep", DM_VAR), 0);
    }

    /* Print response text */
    /* read short could do response headers only to browse resps */
    /* if (pr && !(st_glob.opt_flags & OF_int)) */
    if (pr) {
        if (!st_glob.r_current)
            wfputc('\n', st_glob.outp);
        for (st_glob.l_current = 0;
            st_glob.l_current < re[st_glob.r_current].text.size() &&
            !(status & S_INT);
            st_glob.l_current++) {
            itemsep(expand("txtsep", DM_VAR), 0);
        }
    }

    /* Unless response was scribbled (in which case text holds the
     * scribbler), free up the response text. */
    if ((re[st_glob.r_current].flags & RF_SCRIBBLED) == 0)
        re[st_glob.r_current].text.clear();

    /* Count it as seen */
    if (st_glob.r_lastseen <= st_glob.r_current)
        st_glob.r_lastseen = st_glob.r_current + 1;
}
/******************************************************************************/
/* SHOW RANGE OF RESPONSES                                                    */
/******************************************************************************/
void
show_range(void)
{
    if (st_glob.r_first > st_glob.r_last)
        return; /* invalid range */
    refresh_sum(st_glob.i_current, confidx, sum, part, &st_glob);
    /* in case new reponses came */
    st_glob.r_max = sum[st_glob.i_current - 1].nr - 1;
    st_glob.r_current =
        sum[st_glob.i_current - 1].nr - abs(part[st_glob.i_current - 1].nr);

    open_pipe();
    const auto path =
        std::format("{}/_{}", conflist[confidx].location, st_glob.i_current);
    FILE *fp = mopen(path, O_R);
    if (fp != nullptr) {
        /* For each response */
        for (st_glob.r_current = st_glob.r_first;
            st_glob.r_current <= st_glob.r_last &&
            st_glob.r_current <= st_glob.r_max && !(status & S_INT);
            st_glob.r_current++) {
            if (st_glob.r_first == 0 && st_glob.r_current == 1)
                show_nsep(fp); /* nsep between resp 0 and 1 */
            show_resp(fp);
        }
        status &= ~S_INT; /* Int terminates range */

        mclose(fp);
        st_glob.r_current--;
        itemsep(expand("zsep", DM_VAR), 0);
    }
}
/******************************************************************************/
/* READ THE CURRENT ITEM                                                      */
/******************************************************************************/
void
read_item(void)
{
    FILE *fp;
    short i_lastseen; /* Macro for current item index */
    std::string buff;
    short oldnr, topt_flags;
    long oldlast;
    time_t tsince;
    partentry_t oldpart;
    sepinit(IS_ITEM);
    if (flags & O_LABEL)
        sepinit(IS_ALL);
    i_lastseen = st_glob.i_current - 1;
    memcpy(&oldpart, &part[i_lastseen], sizeof(oldpart));
    oldnr = st_glob.r_lastseen = abs(part[i_lastseen].nr);
    st_glob.r_current = 0;
    oldlast = part[i_lastseen].last;

    if (confidx < 0)
        return;
    /* Open file */
    const auto path =
        std::format("{}/_{}", conflist[confidx].location, st_glob.i_current);
    if (!(fp = mopen(path, O_R)))
        return;
    if (!ngets(buff, fp) || !str::eq(buff, "!<ps02>")) {
        wputs("Invalid _N file format\n");
        mclose(fp);
        return;
    }
    if (!ngets(buff, fp) || !buff.starts_with(",H")) {
        wputs("Invalid _N file format\n");
        mclose(fp);
        return;
    }

    /* Get all the response offsets       */
    /* censor/scribble require this setup */
    get_item(fp, st_glob.i_current, re, sum);
    /* moved up here from above response set loop */

    get_resp(fp, re, (short)GR_HEADER, (short)0); /* Get header of #0 */
    if (st_glob.opt_flags & (OF_NEWRESP | OF_NORESPONSE))
        st_glob.r_first = abs(part[i_lastseen].nr);
    else if (st_glob.since) {
        st_glob.r_first = 0;
        while (st_glob.since > re[st_glob.r_first].date) {
            st_glob.r_first++;
            get_resp(
                fp, &(re[st_glob.r_first]), (short)GR_HEADER, st_glob.r_first);
            if (st_glob.r_first >= sum[i_lastseen].nr)
                break;
        }
    }

    /* Display item header */
    open_pipe();
    show_header();

    st_glob.r_last = MAX_RESPONSES;
    st_glob.r_max = sum[i_lastseen].nr - 1;

    /* For each response set */
    if (!(st_glob.opt_flags & OF_NORESPONSE) &&
        st_glob.r_first <= st_glob.r_max) {
        if (st_glob.r_first > 0)
            show_nsep(fp); /* nsep between header and responses */
        show_range();
    } else
        st_glob.r_current = st_glob.r_first; /* for -# command */
    if (!(st_glob.opt_flags & OF_PASS))
        status &= ~S_INT;

    /* Save range info */
    topt_flags = st_glob.opt_flags;
    tsince = st_glob.since;
    buff = st_glob.string;
    ext_fp = fp;
    ext_first = st_glob.r_first;
    ext_last = st_glob.r_last;

    /* RFP loop until pass or EOF (S_STOP) */
    while (!(status & S_STOP) && (st_glob.r_last >= 0)) {

        /* Temporarily mark seen, for forget command */
        /* but DON'T mark this as dirty since it's temporary */
        part[i_lastseen].nr = st_glob.r_lastseen;
        if ((sum[i_lastseen].flags & IF_FORGOTTEN) && part[i_lastseen].nr > 0)
            part[i_lastseen].nr = -part[i_lastseen].nr; /* stay forgotten */

        /* Main RFP cmd loop */
        mode = M_RFP;
        {
            short tmp_stat;
            tmp_stat = st_glob.c_status;
            if ((sum[i_lastseen].flags & IF_FROZEN) || (flags & O_READONLY))
                st_glob.c_status |= CS_NORESPONSE;
            /* while (mode==M_RFP && !(status & S_INT)) */
            /* test stop in case ^D hit */
            while (mode == M_RFP && !(status & S_STOP)) {
                if (st_glob.opt_flags & OF_PASS)
                    command("pass", 0);
                else if (!get_command("pass", 0)) { /* eof */
                    /* status |= S_INT; WHY was
                     * this here? */
                    if (!(status & S_STOP)) {
                        status |= S_STOP; /* instead */
                        if (!(flags & O_QUIET))
                            wputs("Stopping.\n");
                    }
                    mode = M_OK;
                    st_glob.r_last = -1;

                    if (st_glob.opt_flags & OF_REVERSE)
                        st_glob.i_current = st_glob.i_first;
                    else
                        st_glob.i_current = st_glob.i_last;
                }
            }
            st_glob.c_status = tmp_stat;
        }
    }
    /* KKK what if new items come in? */

    /* Save range info */
    st_glob.opt_flags = topt_flags;
    st_glob.since = tsince;
    st_glob.string = buff;

    /* Save to lastseen */
    /* 4/18 added NORESP check so that timestamp is only updated if they
     * actually saw the responses.  Thus, "browse new" doesn't make things
     * un-new for set sensitive. */
    if (!(st_glob.opt_flags & OF_NORESPONSE) &&
        (st_glob.r_last > -2)) { /* unless preserved or forgotten */
        part[i_lastseen].nr = st_glob.r_lastseen;
        if ((sum[i_lastseen].flags & IF_FORGOTTEN) && part[i_lastseen].nr > 0)
            part[i_lastseen].nr = -part[i_lastseen].nr; /* stay forgotten */
        if (abs(part[i_lastseen].nr) == sum[i_lastseen].nr)
            time(&(part[i_lastseen].last));
        if (flags & O_AUTOSAVE) {
            const auto config = get_config(confidx);
            if (config.size() > CF_PARTFILE)
                write_part(config[CF_PARTFILE]);
        }
    } else if (st_glob.r_last == -2) { /* unless preserved or
                                        * forgotten */
        part[i_lastseen].nr = oldnr;
        part[i_lastseen].last = oldlast;
    }
    if (memcmp(&oldpart, &part[i_lastseen], sizeof(oldpart)))
        dirty_part(i_lastseen);

    /* Must be kept open past RFP mode so 'since' command can access it */
    mclose(fp);
}
/*
 * Same as usual item read progression EXCEPT it ignores the numerical
 * count.  This is just so the WWW can select an item to view and then
 * select the next or previous one.
 */
int
nextitem(int inc)
{
    int i;
    for (i = st_glob.i_current + inc;
        i >= st_glob.i_first && i <= st_glob.i_last; i += inc) {
        if (cover(i, confidx, st_glob.opt_flags, A_COVER, sum, part, &st_glob))
            return i;
    }
    return -1;
}
/******************************************************************************/
/* READ A SET OF ITEMS IN THE CONFERENCE                                      */
/******************************************************************************/
int
do_read(        /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    short i_s, i_i, shown = 0, rfirst = 0, br;
    char act[MAX_ITEMS];

    br = (match(argv[0], "b_rowse") || match(argv[0], "s_can"));
    rangeinit(&st_glob, act);
    refresh_sum(0, confidx, sum, part, &st_glob);
    st_glob.r_first = 0;
    st_glob.opt_flags = (br) ? OF_int | OF_NORESPONSE | OF_PASS : 0;

    /* Check arguments */
    if (argc > 1)
        range(argc, argv, &st_glob.opt_flags, act, sum, &st_glob, 0);

    if (!(st_glob.opt_flags & OF_RANGE)) {
        rangetoken("all", &st_glob.opt_flags, act, sum, &st_glob);
        if (!(st_glob.opt_flags & (OF_UNSEEN | OF_FORGOTTEN | OF_RETIRED |
                                      OF_NEWRESP | OF_BRANDNEW)) &&
            !st_glob.since && !br) {
            rangetoken("new", &st_glob.opt_flags, act, sum, &st_glob);
        }
    }
    if (st_glob.opt_flags & OF_REVERSE) {
        i_s = st_glob.i_last;
        i_i = -1;
    } else {
        i_s = st_glob.i_first;
        i_i = 1;
    }
    rfirst = st_glob.r_first;

    if (match(argv[0], "pr_int"))
        confsep(expand("printmsg", DM_VAR), confidx, &st_glob, part, 0);

    /* Process items */
    sepinit(IS_START);

    for (st_glob.i_current = i_s;
        st_glob.i_current >= st_glob.i_first &&
        st_glob.i_current <= st_glob.i_last && !(status & S_INT);
        st_glob.i_current += i_i) {
        if (cover(st_glob.i_current, confidx, st_glob.opt_flags,
                act[st_glob.i_current - 1], sum, part, &st_glob)) {
            st_glob.i_next = nextitem(1);
            st_glob.i_prev = nextitem(-1);
            st_glob.r_first = rfirst;
            read_item();
            shown++;
            if (match(argv[0], "pr_int"))
                wputs("^L\n");
        }
    }
    if (!shown && (st_glob.opt_flags & (OF_BRANDNEW | OF_NEWRESP))) {
        wputs(std::format("No new {}s matched.\n", topic()));
    }

    /* Check for new mail only */
    refresh_stats(sum, part, &st_glob);
    check_mail(0);

    return 1;
}
/******************************************************************************/
/* FORGET A SET OF ITEMS IN THE CONFERENCE                                    */
/******************************************************************************/
int
forget(         /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    char act[MAX_ITEMS];
    rangeinit(&st_glob, act);
    refresh_sum(0, confidx, sum, part, &st_glob);

    if (argc < 2) {
        std::println("Error, no {} specified! (try HELP RANGE)", topic());
    } else { /* Process args */
        range(argc, argv, &st_glob.opt_flags, act, sum, &st_glob, 0);
    }

    /* Process items */
    for (auto j = st_glob.i_first; j <= st_glob.i_last && !(status & S_INT);
        j++) {
        if (cover(j, confidx, st_glob.opt_flags, act[j - 1], sum, part,
                &st_glob)) {
            if (!part[j - 1].nr) {
                /* Pretend we've read the initial text */
                part[j - 1].nr = 1;
                dirty_part(j - 1);
            }
            if (!(sum[j - 1].flags & IF_FORGOTTEN)) {
                if (!(flags & O_QUIET))
                    std::println("Forgetting {} {}", topic(), j);
                st_glob.r_lastseen = part[j - 1].nr = -part[j - 1].nr;
                sum[j - 1].flags |= IF_FORGOTTEN;
                time(&(part[j - 1].last)); /* current time */
                dirty_part(j - 1);
                dirty_sum(j - 1);
            }
        }
    }
    st_glob.r_last = -1; /* go on to next item if at RFP prompt */
    return 1;
}
/******************************************************************************/
/* LOCATE A WORD OR GROUP OF WORDS IN A CONFERENCE                            */
/******************************************************************************/
int
do_find(        /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    char act[MAX_ITEMS], pr;
    std::string str, astr;
    short icur;
    size_t j;
    FILE *fp;
    int count = 0;
    icur = st_glob.i_current;
    rangeinit(&st_glob, act);
    refresh_sum(0, confidx, sum, part, &st_glob);

    if (argc > 1) /* Process argc */
        range(argc, argv, &st_glob.opt_flags, act, sum, &st_glob, 0);
    if (!(st_glob.opt_flags & OF_RANGE))
        rangetoken("all", &st_glob.opt_flags, act, sum, &st_glob);

    if (st_glob.string.empty() && st_glob.author.empty()) {
        if (!(flags & O_QUIET))
            wputs("Find: look for what?\n");
        return 1;
    }
    str = st_glob.string;  /* so items match */
    astr = st_glob.author; /* so items match */
    lower_case(str);
    lower_case(astr);
    st_glob.string.clear();
    st_glob.author.clear();

    /* Process items */
    open_pipe();
    sepinit(IS_START);
    for (j = st_glob.i_first; j <= st_glob.i_last && !(status & S_INT); j++) {
        st_glob.i_current = j;
        if (cover(j, confidx, st_glob.opt_flags, act[j - 1], sum, part,
                &st_glob)) {
            const auto path =
                std::format("{}/_{}", conflist[confidx].location, j);
            if (!(fp = mopen(path, O_R)))
                continue;
            get_item(fp, j, re, sum);
            sepinit(IS_ITEM);
            if (flags & O_LABEL)
                sepinit(IS_ALL);

            /* For each response */
            for (st_glob.r_current = 0;
                st_glob.r_current < sum[j - 1].nr && !(status & S_INT);
                st_glob.r_current++) {

                get_resp(fp, &(re[st_glob.r_current]), (short)GR_HEADER,
                    st_glob.r_current);

                /* Scan response text */
                if (re[st_glob.r_current].flags & (RF_EXPIRED | RF_SCRIBBLED))
                    pr = 0;
                else if (re[st_glob.r_current].flags & RF_CENSORED)
                    pr = ((st_glob.opt_flags & OF_NOFORGET) ||
                          !(flags & O_FORGET));
                else
                    pr = 1;

                if (pr) {
                    get_resp(fp, &(re[st_glob.r_current]), (short)GR_ALL,
                        st_glob.r_current);
                    sepinit(IS_RESP);

                    if (astr.empty() ||
                        str::eq(re[st_glob.r_current].login, astr)) {
                        if (!str.empty()) {
                            for (st_glob.l_current = 0;
                                !(status & S_INT) &&
                                st_glob.l_current <
                                    re[st_glob.r_current].text.size();
                                st_glob.l_current++) {
                                if (strstr(
                                        lower_case(re[st_glob.r_current]
                                                       .text[st_glob.l_current])
                                            .c_str(),
                                        str.c_str())) {
                                    itemsep(expand("fsep", DM_VAR), 0);
                                    count++;
                                }
                            }
                        } else {
                            st_glob.l_current = 0;
                            itemsep(expand("fsep", DM_VAR), 0);
                            count++;
                        }
                    }
                    re[st_glob.r_current].text.clear();
                }
            }
            mclose(fp);
        }
    }

    /* Reload initial re[] contents */
    st_glob.i_current = icur;
    if (icur >= st_glob.i_first && icur <= st_glob.i_last) {
        const auto path =
            std::format("{}/_{}", conflist[confidx].location, icur);
        if ((fp = mopen(path, O_R)) != NULL) {
            get_item(fp, j, re, sum);
            mclose(fp);
        }
    }
    if (!count)
        wputs("No matches found.\n");
    return 1;
}
/******************************************************************************/
/* FREEZE/THAW/RETIRE/UNRETIRE A SET OF ITEMS IN THE CONFERENCE               */
/******************************************************************************/
int
freeze(         /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    char act[MAX_ITEMS];
    short j;
    struct stat stt;
    rangeinit(&st_glob, act);

    if (argc < 2) {
        std::println("Error, no {} specified! (try HELP RANGE)", topic());
    } else { /* Process args */
        range(argc, argv, &st_glob.opt_flags, act, sum, &st_glob, 0);
    }

    /* Process items */
    for (j = st_glob.i_first; j <= st_glob.i_last && !(status & S_INT); j++) {
        if (!act[j - 1] || !sum[j - 1].flags)
            continue;
        const auto path = std::format("{}/_{}", conflist[confidx].location, j);
        st_glob.i_current = j;

        /* Check for permission */
        if (!(st_glob.c_status & CS_FW) && !is_enterer(j)) {
            std::println("You can't do that!");
            continue;
        }
        if (!match(get_conf_param("freezelinked", FREEZE_LINKED), "true")) {
            if ((sum[j - 1].flags & IF_LINKED) &&
                (re[0].uid != uid || !str::eq(re[0].login, login))) {
                std::println("{} {} is linked!", topic(1), j);
                continue;
            }
        }

        /* Do the change */
        if (stat(path.c_str(), &stt)) {
            std::println("Error accessing {}", path);
            continue;
        }
        switch (tolower(argv[0][0])) {
        case 'f': /* Freeze   r--r--r-- */
            sum[j - 1].flags |= IF_FROZEN;
            if (chmod(path.c_str(), stt.st_mode & ~S_IWUSR))
                error("freezing ", path);
            else
                custom_log("freeze", M_RFP);
            break;
        case 't': /* Thaw     rw-r--r-- */
            sum[j - 1].flags &= ~IF_FROZEN;
            if (chmod(path.c_str(), stt.st_mode | S_IWUSR))
                error("thawing ", path);
            else
                custom_log("thaw", M_RFP);
            break;
        case 'r': /* Retire   r-xr--r-- F,R */
            sum[j - 1].flags |= IF_RETIRED;
            if (chmod(path.c_str(), stt.st_mode | S_IXUSR))
                error("retiring ", path);
            else
                custom_log("retire", M_RFP);
            break;
        case 'u': /* Unretire rw-r--r-- */
            sum[j - 1].flags &= ~IF_RETIRED;
            if (chmod(path.c_str(), stt.st_mode & ~S_IXUSR))
                error("unretiring ", path);
            else
                custom_log("unretire", M_RFP);
            break;
        }
        save_sum(sum, (short)(j - 1), confidx, &st_glob);
        dirty_sum(j - 1);
    }
    return 1;
}
/******************************************************************************/
/* LINK AN ITEM INTO THE CONFERENCE                                           */
/******************************************************************************/
int
linkfrom(       /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    short idx, i, j;
    /* unsigned int sec; */
    sumentry_t fr_sum[MAX_ITEMS];
    char act[MAX_ITEMS];
    status_t fr_st{};
    partentry_t part2[MAX_ITEMS];
    st_glob.opt_flags = 0;
    refresh_sum(0, confidx, sum, part, &st_glob);
    if (!(st_glob.c_status & CS_FW)) {
        std::println("Sorry, you can't do that!");
        return 1;
    }
    if (argc < 2) {
        std::println("No {} specified!", conference());
        return 1;
    }
    idx = get_idx(argv[1], conflist);
    if (idx < 0) {
        std::println("Cannot access {} {}.", conference(), argv[1]);
        return 1;
    }

    /* Pass security checks (from join() in conf.c) */
    if (!check_acl(JOIN_RIGHT, idx) || !check_acl(RESPOND_RIGHT, idx)) {
        std::println("You are not allowed to link {}s from the {} {}.",
            compress(conflist[idx].name), topic(), conference());
        return 1;
    }

    /* Get item range */
    if (argc < 3) {
        std::println("Error, no {} specified! (try HELP RANGE)", topic());
        return 1;
    }
    /* Read in config file */
    const auto config = get_config(idx);
    if (config.size() <= CF_PARTFILE)
        return 1;

#ifdef NEWS
    fr_st.c_article = 0;
#endif

    /* Read in partfile */
    read_part(config[CF_PARTFILE], part2, &fr_st, idx);
    fr_st.c_security = security_type(config, idx);
    rangeinit(&fr_st, act);

    /* get subjs for range */
    get_status(&fr_st, fr_sum, part2, idx);

    range(argc, argv, &st_glob.opt_flags, act, fr_sum, &fr_st, 1);

    /* Process items */
    for (j = fr_st.i_first; j <= fr_st.i_last && !(status & S_INT); j++) {
        if (!cover(
                j, idx, st_glob.opt_flags, act[j - 1], fr_sum, part2, &fr_st))
            continue;

        const auto from_path = std::format("{}/_{}", conflist[idx].location, j);
        i = ++(st_glob.c_confitems); /* next item number */
        std::print("Linking as {} {}...", topic(), i);
        const auto to_path =
            std::format("{}/_{}", conflist[confidx].location, i);
        if (link(from_path.c_str(), to_path.c_str()) != 0 &&
            symlink(from_path.c_str(), to_path.c_str()) != 0)
            error("linking from ", from_path);
        else {
            fr_sum[j - 1].flags |= IF_LINKED;
            memcpy(&(sum[i - 1]), &(fr_sum[j - 1]), sizeof(sumentry_t));
            save_sum(fr_sum, (short)(j - 1), idx, &fr_st);
            save_sum(sum, (short)(i - 1), confidx, &st_glob);
            dirty_sum(i - 1);

            /* Log action to both conferences */
            ylog(idx, std::format("linked {} {} to {} {}", topic(), j,
                          compress(conflist[confidx].name), i));
            ylog(confidx, std::format("linked {} {} from {} {}", topic(), i,
                              compress(conflist[idx].name), j));

            std::println("done.");
        }
    }
    return 1;
}
/******************************************************************************/
/* KILL A SET OF ITEMS IN THE CONFERENCE                                      */
/******************************************************************************/
int
do_kill(        /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    char act[MAX_ITEMS];
    short j;
    FILE *fp;
    rangeinit(&st_glob, act);
    refresh_sum(0, confidx, sum, part, &st_glob);

    if (argc < 2) {
        std::println("Error, no {} specified! (try HELP RANGE)", topic());
    } else { /* Process args */
        range(argc, argv, &st_glob.opt_flags, act, sum, &st_glob, 0);
    }

    /* Process items */
    for (j = st_glob.i_first; j <= st_glob.i_last && !(status & S_INT); j++) {
        if (!act[j - 1] || !sum[j - 1].flags)
            continue;
        const auto path = std::format("{}/_{}", conflist[confidx].location, j);

        /* Check for permission */
        if (!(st_glob.c_status & CS_FW)) {
            if (!(fp = mopen(path, O_R)))
                continue;
            get_item(fp, j, re, sum);
            mclose(fp);
            if (re[0].uid != uid || !str::eq(re[0].login, login) ||
                sum[j - 1].nr > 1) {
                wputs("You can't do that!\n");
                continue;
            }
        }

        /* Do the remove */
        const auto prompt = std::format("Ok to kill {} {}? ", topic(), j);
        if (get_yes(prompt, false)) {
            std::println("(Killing {} {})", topic(), j);
            rm(path, SL_OWNER);
            sum[j - 1].flags = 0;
            save_sum(sum, (short)(j - 1), confidx, &st_glob);
            dirty_sum(j - 1);
            custom_log("kill", M_RFP);
        }
    }
    st_glob.r_last = -1; /* go on to next item if at RFP prompt */
    return 1;
}
/******************************************************************************/
/* REMEMBER (UNFORGET) A SET OF ITEMS IN THE CONFERENCE                       */
/******************************************************************************/
int
remember(       /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    char act[MAX_ITEMS];
    short j;
    /* Initialize range */
    rangeinit(&st_glob, act);

    /* Process arguments */
    if (argc < 2)
        rangetoken("all", &st_glob.opt_flags, act, sum, &st_glob);
    else
        range(argc, argv, &st_glob.opt_flags, act, sum, &st_glob, 0);

    /* Process items in specified range */
    for (j = st_glob.i_first; j <= st_glob.i_last && !(status & S_INT); j++) {
        if (!act[j - 1] || !sum[j - 1].flags)
            continue;
        part[j - 1].nr = abs(part[j - 1].nr);
        sum[j - 1].flags &= ~IF_FORGOTTEN;
        dirty_part(j - 1);
        dirty_sum(j - 1);
    }
    return 1;
}
/******************************************************************************/
/* MARK A SET OF ITEMS AS BEING SEEN                                          */
/******************************************************************************/
int
fixseen(        /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    char act[MAX_ITEMS];
    short j;
    rangeinit(&st_glob, act);
    refresh_sum(0, confidx, sum, part, &st_glob);

    if (argc < 2) {
        rangetoken("all", &st_glob.opt_flags, act, sum, &st_glob);
    } else { /* Process args */
        range(argc, argv, &st_glob.opt_flags, act, sum, &st_glob, 0);
    }

    /* Process items */
    for (j = st_glob.i_first; j <= st_glob.i_last && !(status & S_INT); j++) {
        if (cover(j, confidx, st_glob.opt_flags, act[j - 1], sum, part,
                &st_glob)) {
            part[j - 1].nr = sum[j - 1].nr;
            time(&(part[j - 1].last)); /* or sum.last? */
            dirty_part(j - 1);
        }
    }
    return 1;
}
/******************************************************************************/
/* MARK A SET OF RESPONSES AS BEING SEEN                                      */
/******************************************************************************/
int
fixto(int argc, /* IN: Number of arguments */
    char **argv /* IN: Argument list       */
)
{
    char act[MAX_ITEMS];
    size_t j = 0;
    int r;
    time_t stamp;

    std::vector<std::string> args;
    for (auto i = 0; i < argc; i++) args.push_back(argv[i]);
    stamp = since(args, &j);
    argc -= j;
    argv += j;

    rangeinit(&st_glob, act);
    refresh_sum(0, confidx, sum, part, &st_glob);

    if (argc < 2) {
        rangetoken("all", &st_glob.opt_flags, act, sum, &st_glob);
    } else { /* Process args */
        range(argc, argv, &st_glob.opt_flags, act, sum, &st_glob, 0);
    }

    /* Process items */
    for (auto j = st_glob.i_first; j <= st_glob.i_last && !(status & S_INT);
        j++) {
        if (!cover(j, confidx, st_glob.opt_flags, act[j - 1], sum, part,
                &st_glob) ||
            !sum[j - 1].nr || part[j - 1].nr < 0)
            continue;

        /* Load response times */
        const auto path = std::format("{}/_{}", conflist[confidx].location, j);
        FILE *fp = mopen(path, O_R);
        if (fp != NULL) {
            get_item(fp, j, re, sum);

            /* Find first response # which is dated > timestamp */
            for (r = 0; r < sum[j - 1].nr; r++) {
                get_resp(fp, &re[r], (short)GR_HEADER, r);
                if (re[r].date > stamp)
                    break;
            }

            /* Store new info */
            part[j - 1].nr = r;
            part[j - 1].last = stamp;
            dirty_part(j - 1);

            mclose(fp);
        }
    }
    return 1;
}
