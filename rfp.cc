// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include "rfp.h"

#include <sys/stat.h>
#include <sys/wait.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <format>
#include <iostream>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include "arch.h"
#include "conf.h"
#include "driver.h"
#include "edbuf.h"
#include "edit.h"
#include "files.h"
#include "globals.h"
#include "item.h"
#include "lib.h"
#include "license.h"
#include "log.h"
#include "macro.h"
#include "main.h"
#include "misc.h"
#include "news.h"
#include "range.h"
#include "security.h"
#include "sep.h"
#include "stats.h"
#include "str.h"
#include "struct.h"
#include "sum.h"
#include "system.h"
#include "user.h"
#include "yapp.h"

extern FILE *ext_fp;
extern int ext_first, ext_last;
/* Commands available in RFP mode */
static dispatch_t rfp_cmd[] = {
    {"edit",       modify     },
    {"cen_sor",    censor     },
    {"scr_ibble",  censor     },
    {"uncen_sor",  uncensor   },
    {"e_nter",     enter      },
    {"pr_eserve",  preserve   },
    {"po_stpone",  preserve   },
    {"hide",       preserve   },
    {"p_ass",      preserve   },
    {"n_ew",       preserve   },
    {"wait",       preserve   },
#ifdef INCLUDE_EXTRA_COMMANDS
    {"tree",       tree       },
#endif
    {"*freeze",    freeze     },
    {"*thaw",      freeze     },
    {"*forget",    freeze     },
    {"*retire",    freeze     },
    {"*unretire",  freeze     },
    {"r_espond",   rfp_respond},
    {"ps_eudonym", rfp_respond},
    {"*retitle",   retitle    },
    {"*rem_ember", remember   },
    {"*unfor_get", remember   },
    {"*kill",      do_kill    },
    {"*f_ind",     do_find    },
    {"*lo_cate",   do_find    },
    /* "*fix_seen",  fixseen,  this by itself doesn't work */
    {"reply",      reply      },
    {0,            0          },
};

/******************************************************************************/
/* DISPATCH CONTROL TO APPROPRIATE RFP MODE FUNCTION                          */
/******************************************************************************/
char
rfp_cmd_dispatch(/* ARGUMENTS:                  */
    int argc,    /* Number of arguments      */
    char **argv  /* Argument list            */
)
{
    int a, b, c;
    int i, j;

    std::vector<std::string> args;
    for (auto i = 0; i < argc; i++) args.push_back(argv[i]);

    for (i = 0; rfp_cmd[i].name; i++) {
        if (match(argv[0], rfp_cmd[i].name))
            return rfp_cmd[i].func(argc, argv);
        if (rfp_cmd[i].name[0] == '*' && match(argv[0], rfp_cmd[i].name + 1)) {
            mode = M_OK;
            st_glob.r_first = st_glob.r_last + 1;
            auto cmd = std::format(
                "{} {}", compress(rfp_cmd[i].name + 1), st_glob.i_current);
            for (j = 1; j < argc; j++) {
                cmd.append(" ");
                cmd.append(argv[j]);
            }
            return command(cmd, 0);
        }
    }

    /* Command dispatch */
    if (match(argv[0], "h_eader")) {
        show_header();
    } else if (match(argv[0], "text")) { /* re-read from 0 */
        st_glob.r_first = 0;
        st_glob.r_last = MAX_RESPONSES;
        show_range();
    } else if (match(argv[0], "a_gain")) { /* re-read same range */
        sepinit(IS_ITEM);
        if (flags & O_LABEL)
            sepinit(IS_ALL);
        show_header();
        st_glob.r_first = ext_first;
        st_glob.r_last = ext_last;
        if (st_glob.r_first > 0)
            show_nsep(ext_fp); /* nsep between header
                                * and responses */
        show_range();
    } else if (match(argv[0], "^") || match(argv[0], "fi_rst")) {
        st_glob.r_first = 1;
        st_glob.r_last = MAX_RESPONSES;
        show_range();
    } else if (match(argv[0], ".") || match(argv[0], "th_is") ||
               match(argv[0], "cu_rrent")) {
        st_glob.r_first = st_glob.r_current;
        st_glob.r_last = MAX_RESPONSES;
        show_range();
    } else if (match(argv[0], "$") || match(argv[0], "l_ast")) {
        st_glob.r_first = st_glob.r_max;
        st_glob.r_last = MAX_RESPONSES;
        show_range();
    }
#ifdef INCLUDE_EXTRA_COMMANDS
    else if (match(argv[0], "up") || match(argv[0], "par_ent")) {
        int a;
        if ((a = parent(st_glob.r_current)) < 0)
            std::println("No previous response");
        else {
            st_glob.r_first = a;
            st_glob.r_last = a;
            show_range();
        }
    } else if (match(argv[0], "chi_ldren") || match(argv[0], "do_wn")) {
        int a;
        /* Find 1st child */
        if ((a = child(st_glob.r_current)) < 0)
            std::println("No children");
        else {
            st_glob.r_first = a;
            st_glob.r_last = a;
            show_range();
        }
    } else if (match(argv[0], "sib_ling") || match(argv[0], "ri_ght")) {
        int a;
        /* Find  next sibling */
        if ((a = sibling(st_glob.r_current)) < 0)
            std::println("No more siblings");
        else {
            st_glob.r_first = a;
            st_glob.r_last = a;
            show_range();
        }
    } else if (match(argv[0], "sync_hronous")) {
        mode = M_OK; /* KKK */
    } else if (match(argv[0], "async_hronous")) {
        mode = M_OK; /* KKK */
    }
#endif
    else if (match(argv[0], "si_nce")) {
        size_t i = 0;
        time_t t = since(args, &i);
        for (i = st_glob.r_max; i >= 0; i--) {
            if (!re[i].date)
                get_resp(ext_fp, &(re[i]), (int)GR_HEADER, i);
            if (re[i].date < t)
                break;
        }
        st_glob.r_first = i + 1;
        st_glob.r_last = MAX_RESPONSES;
        show_range();
    } else if (match(argv[0], "onl_y")) {
        if (argc > 2) {
            std::println("Bad parameters near \"{}\"", argv[2]);
            return 2;
        } else if (argc > 1 && sscanf(argv[1], "%d", &a) == 1) {
            int prev_opt_flags = st_glob.opt_flags;
            st_glob.r_first = a;
            st_glob.r_last = a;
            st_glob.opt_flags |= OF_NOFORGET; /* force display of hidden resp */
            show_range();
            st_glob.opt_flags = prev_opt_flags;
        } else
            wputs("You must specify a comment number\n");
    } else if (argc && sscanf(argv[0], "%d-%d", &a, &b) == 2) {
        if (b < a) {
            c = a;
            a = b;
            b = c;
        }
        if (a < 0) {
            std::println("Response #{} is too small", a);
        } else if (b > st_glob.r_max) {
            std::println("Response #{} is too big (last {})", a, st_glob.r_max);
        } else {
            st_glob.r_first = a;
            st_glob.r_last = b;
            show_range();
        }
    } else if (argc && (sscanf(argv[0], "%d", &a) == 1 ||
                           str::eq(argv[0], "-") || str::eq(argv[0], "+"))) {
        if (str::eq(argv[0], "-"))
            a = -1;
        if (str::eq(argv[0], "+"))
            a = 1;
        if (argv[0][0] == '+' || argv[0][0] == '-')
            a += st_glob.r_current;
        if (a < 0) {
            std::println("Response #{} is too small", a);
        } else if (a > st_glob.r_max) {
            std::println("Response #{} is too big (last {})", a, st_glob.r_max);
        } else {
            st_glob.r_first = a;
            st_glob.r_last = MAX_RESPONSES;
            show_range();
        }
    } else {
        a = misc_cmd_dispatch(argc, argv);
        if (!a)
            preserve(argc, argv);
        return a;
    }

    return 1;
}

/******************************************************************************/
/* ADD A NEW RESPONSE                                                         */
/******************************************************************************/
void
add_response(sumentry_t *sumthis,         /* New item summary */
    const std::vector<std::string> &text, /* New item text    */
    int idx, sumentry_t *sum, partentry_t *part, status_t *stt, long art,
    const std::string_view &mid, int uid, const std::string_view &login,
    const std::string_view &fullname, int resp)
{
    int item, line, nr;
    FILE *fp;

    item = stt->i_current;
    nr = sum[item - 1].nr;
    const auto path = std::format("{}/_{}", conflist[idx].location, item);

    /* Prevent duplicate responses: */
    if ((fp = mopen(path, O_R)) != NULL) {
        int prev, nl_prev, nl_new, dup;
        prev = sum[item - 1].nr - 1;
        get_resp(fp, &re[prev], (int)GR_ALL, sum[item - 1].nr - 1);
        mclose(fp);
        nl_prev = re[prev].text.size();
        nl_new = text.size();
        dup = (nl_prev == nl_new && str::eq(login, re[prev].login));
        for (line = 0; dup && line < nl_new; line++)
            dup = str::eq(text[line], re[prev].text[line]);
        re[prev].text.clear();
        if (dup) {
            if (!(flags & O_QUIET))
                std::println("Duplicate response aborted");
            return;
        }
    }

    /* Begin critical section */
    if ((fp = mopen(path, O_A | O_NOCREATE)) != NULL) {
        int n;
        /* was: update before open, in case of a link - why? (was
         * wrong) */
        if (!art)
            refresh_sum(item, idx, sum, part, stt);

        n = sum[item - 1].nr - nr;
        if (n > 1) {
            std::println(
                "Warning: {} comments slipped in ahead of yours at {}-{}!", n,
                nr, sum[item - 1].nr - 1);
        } else if (n == 1) {
            std::println("Warning: a comment slipped in ahead of yours at {}!",
                sum[item - 1].nr - 1);
        } else if (!(flags & O_STAY))
            stt->r_last = -1;

        re[sum[item - 1].nr].offset = ftell(fp);
        std::println(fp, ",R{:04}", RF_NORMAL);
        std::println(fp, ",U{},{}", uid, login);
        std::println(fp, ",A{}", fullname);
        std::println(fp, ",D{:08X}", (art) ? sumthis->last : time((time_t *)0));
        if (resp)
            std::println(fp, ",P{}", resp - 1);
        std::println(fp, ",T");
        re[sum[item - 1].nr].parent = resp;
        re[sum[item - 1].nr].textoff = ftell(fp);
        re[sum[item - 1].nr].numchars = -1;
        if (art) {
            std::println(fp, ",N{:06}", art);
            std::println(fp, ",M{}", mid);
        } else {
            for (const auto &line : text) {
                if (line[0] == ',')
                    std::print(fp, ",,");
                std::println(fp, "{}", line);
            }
        }
        std::println(
            fp, ",E{}", spaces(str::toi(get_conf_param("padding", PADDING))));
        if (!ferror(fp)) {
            /* Update seen */
            stt->r_current = stt->r_max = sum[item - 1].nr;
            /* if (!(flags & O_METOO) &&
             * stt->r_lastseen==sum[item-1].nr)  */
            time(&(part[item - 1].last));
            if (!(flags & O_METOO)) {
                part[item - 1].nr = sum[item - 1].nr;
                stt->r_lastseen = stt->r_current + 1;
            }
            dirty_part(item - 1);

            sum[item - 1].last = time((time_t *)0);
            sum[item - 1].nr++;
            save_sum(sum, (int)(item - 1), idx, stt);
            dirty_sum(item - 1);

            skip_new_response(confidx, item - 1, sum[item - 1].nr);
        } else
            error("writing response", "");
        mclose(fp);
    }
    /* End critical section */
}

/******************************************************************************/
/* ENTER A RESPONSE INTO THE CURRENT ITEM                                     */
/******************************************************************************/
void
do_respond(  /* ARGUMENTS:                      */
    int ps,  /* Use a pseudo?                */
    int resp /* Response to prev. response # */
)
{
    int nr;
    unsigned char omode;
    FILE *fp;

    if (!check_acl(RESPOND_RIGHT, confidx)) {
        std::println("You only have read access.");
        return;
    }
    if (sum[st_glob.i_current - 1].flags & IF_FROZEN) {
        wputs(std::format("{} is frozen!\n", Topic()));
        return;
    }
    nr = sum[st_glob.i_current - 1].nr;
    if (resp > nr) {
        std::println("Highest response # is {}", nr - 1);
        return;
    }

    /* Get pseudo */
    std::string pseudo;
    if (ps) {
        if (!(flags & O_QUIET)) {
            std::print("What's your handle? ");
            std::fflush(stdout);
        }
        std::string buff;
        if (!ngets(buff, st_glob.inp)) /* st_glob.inp */
            return;
        if (buff[0] == '%') {
            const char *f, *str;
            str = buff.c_str() + 1;
            f = get_sep(&str);
            buff = f;
        }
        if (!buff.empty())
            pseudo = buff;
        else {
            wputs(std::format(
                "Resonse aborted!  Returning to current {}.\n", topic()));
            return;
        }
    } else
        pseudo = fullname_in_conference(&st_glob);

    if (nr >= MAX_RESPONSES) {
        wputs(std::format("Too many resposnes on this {}!\n", topic()));
        return;
    }
#ifdef NEWS
    if (st_glob.c_security & CT_NEWS) {
        if (!resp)
            resp = nr;

        if (resp > 0) {
            const auto config = get_config(confidx);
            if (config.size() <= CF_NEWSGROUP)
                return;
            const auto path =
                std::format("{}/{}/{}", get_conf_param("newsdir", NEWSDIR),
                    dot2slash(config[CF_NEWSGROUP]), st_glob.i_current);
            fp = mopen(path, O_R);
            if (fp == NULL)
                return;
            get_resp(fp, &(re[resp - 1]), GR_ALL, resp - 1);
            mclose(fp);
        }
        make_rnhead(re, resp);
        if (resp > 0)
            re[resp - 1].text.clear();
        const auto rnh = str::concat({home, "/.rnhead"});
        unix_cmd(std::format("Pnews -h {}", rnh));
        rm(rnh, SL_USER);
        return;
    }
#endif

    if (st_glob.c_security & CT_EMAIL) {
        /* Load parent for inclusion */
        if (resp > 0) {
            const auto path = std::format(
                "{}/_{}", conflist[confidx].location, st_glob.i_current);
            if ((fp = mopen(path, O_R)) == NULL)
                return;
            get_resp(fp, &(re[resp - 1]), GR_ALL, resp - 1);
            mclose(fp);
        }
        make_emhead(re, resp);
        if (resp > 0)
            re[resp - 1].text.clear();
    }

    /* Delete old if not EMAIL */
    if (text_loop(!(st_glob.c_security & CT_EMAIL), "response") &&
        get_yes("Ok to enter this response? ", false)) { /* success */
        omode = mode;
        mode = M_OK;

        std::vector<std::string> file;
        if (st_glob.c_security & CT_EMAIL) {
            make_emtail();
            const auto cmd = std::format("{} -t < {}/cf.buffer",
                get_conf_param("sendmail", SENDMAIL), work);
            unix_cmd(cmd);

        } else if ((file = grab_file(work, "cf.buffer", 0)), file.empty())
            wputs("The file cf.buffer doesn't seem to exist.\n");
        else {
            if (file.empty()) {
                wputs("No text in buffer!\n");
            } else {
                add_response(&(sum[st_glob.i_current - 1]), file, confidx, sum,
                    part, &st_glob, 0, "", uid, login, pseudo, resp);
                custom_log("respond", M_RFP);
            }
        }

        if (flags & O_STAY)
            mode = omode;
    } else {
        wputs(std::format(
            "Response aborted!  Returning to current {}.\n", topic()));
    }

    /* Delete text buffer */
    const auto cfbuffer = str::concat({work, "/cf.buffer"});
    rm(cfbuffer, SL_USER);
}

/******************************************************************************/
/* ADD A RESPONSE TO THE CURRENT ITEM                                         */
/******************************************************************************/
int
respond(        /* ARGUMENTS:                  */
    int argc,   /* Number of arguments      */
    char **argv /* Argument list            */
)
{
    char act[MAX_ITEMS];
    short j, fl;

    if (!check_acl(RESPOND_RIGHT, confidx)) {
        std::println("You only have read access.");
        return 1;
    }
    rangeinit(&st_glob, act);
    refresh_sum(0, confidx, sum, part, &st_glob);
    st_glob.r_first = -1;

    fl = 0;
    if (argc < 2) {
        std::println("Error, no {} specified! (try HELP RANGE)", topic(false));
    } else { /* Process args */
        range(argc, argv, &fl, act, sum, &st_glob, 0);
    }

    /* Process items */
    for (j = st_glob.i_first; j <= st_glob.i_last && !(status & S_INT); j++) {
        if (!act[j - 1] || !sum[j - 1].flags)
            continue;
        st_glob.i_current = j;
        if (!(flags & O_QUIET))
            show_header();
        if (status & S_PAGER)
            spclose(st_glob.outp);
        do_respond(
            argc > 0 && match(argv[0], "ps_eudonym"), st_glob.r_first + 1);
    }
    return 1;
}

/******************************************************************************/
/* ENTER A RESPONSE IN THE CURRENT ITEM                                       */
/******************************************************************************/
int
rfp_respond(    /* ARGUMENTS:                  */
    int argc,   /* Number of arguments      */
    char **argv /* Argument list            */
)
{
    int a = -1;
    if (argc > 2 || (argc > 1 && (sscanf(argv[1], "%d", &a) < 1 || a < 0))) {
        std::println("Bad parameters near \"{}\"", argv[(argc > 2) ? 2 : 1]);
        return 2;
    } else
        do_respond(argc > 0 && match(argv[0], "ps_eudonym"), (int)a + 1);

    return 1;
}

void
dump_reply(const char *sep)
{
    sepinit(IS_START);
    itemsep(expand(sep, DM_VAR), 0);
    for (st_glob.l_current = 0; static_cast<size_t>(st_glob.l_current) <
                                    re[st_glob.r_current].text.size() &&
                                !(status & S_INT);
        st_glob.l_current++) {
        sepinit(IS_ITEM);
        itemsep(expand(sep, DM_VAR), 0);
    }
    sepinit(IS_CFIDX);
    itemsep(expand(sep, DM_VAR), 0);
}

/******************************************************************************/
/* MAIL A REPLY TO THE AUTHOR OF A RESPONSE                                   */
/******************************************************************************/
int
reply(          /* ARGUMENTS:                  */
    int argc,   /* Number of arguments      */
    char **argv /* Argument list            */
)
{
    int i;
    flag_t ss;
    int cpid, wpid;
    int statusp;

    /* Validate arguments */
    if (argc < 2 || sscanf(argv[1], "%d", &i) < 1) {
        std::println("You must specify a comment number.");
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
    if (re[i].flags & RF_CENSORED) {
        wputs("Cannot reply to censored response!\n");
        return 1;
    }
    if (re[i].offset < 0) {
        std::println("Offset error."); /* should never happen */
        return 1;
    }

    /* Get complete text */
    std::string path;
#ifdef NEWS
    if (st_glob.c_security & CT_NEWS) {
        const auto config = get_config(confidx);
        if (config.size() <= CF_NEWSGROUP)
            return 1;
        path = std::format("{}/{}/{}", get_conf_param("newsdir", NEWSDIR),
            dot2slash(config[CF_NEWSGROUP]), st_glob.i_current);
    } else
#endif
        path = std::format(
            "{}/_{}", conflist[confidx].location, st_glob.i_current);
    FILE *fp = mopen(path, O_R);
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
        /* post = !statusp; */
    } else { /* child */
        if (setuid(getuid()))
            error("setuid", "");
        setgid(getgid());

        /* Save to cf.buffer */
        const auto path = str::concat({work, "/cf.buffer"});
        FILE *fp = mopen(path, O_W);
        if (fp == NULL) {
            re[i].text.clear();
            return 1;
        }
        FILE *pp = st_glob.outp;
        ss = status;
        st_glob.r_current = i;
        st_glob.outp = fp;
        status |= S_PAGER;

        dump_reply("replysep");

        st_glob.outp = pp;
        status = ss;

        dump_reply("replysep");

        mclose(fp);
        exit(0);
    }

    /* Invoke mailer */
    const auto cmd = std::format("mail {}", re[i].login);
    command(cmd.c_str(), 0);
    re[i].text.clear();
    mode = M_RFP;
    return 1;
}

/******************************************************************************/
/* CENSOR A RESPONSE IN THE CURRENT ITEM                                      */
/* XXX: Duplicates logic in `edit.cc`.                                        */
/******************************************************************************/
/* ARGUMENTS:          */
/* Number of arguments */
/* Argument list       */
int
censor(int argc, char **argv)
{
    int i, typ, j, k;
    FILE *fp;
    int len;
    int frozen = 0; /* 1 if we need to unfreeze,censor,freeze */
    struct stat stt;
    typ = (match(argv[0], "scr_ibble")) ? RF_CENSORED | RF_SCRIBBLED
                                        : RF_CENSORED;

    /* Validate arguments */
    if (argc < 2 || sscanf(argv[1], "%d", &i) < 1) {
        std::println("You must specify a comment number.");
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

    /* Check for permission to censor */
    if (!re[i].date)
        get_resp(ext_fp, &(re[i]), GR_HEADER, i);
    if (!(st_glob.c_status & CS_FW) &&
        (uid != re[i].uid ||
            (uid == get_nobody_uid() && !str::eq(login, re[i].login)))) {
        std::println("You don't have permission to affect response {}.", i);
        return 1;
    }
    if (sum[st_glob.i_current - 1].flags & IF_FROZEN) {
        if (!match(get_conf_param("censorfrozen", CENSOR_FROZEN), "true")) {
            wputs(std::format("Cannot censor frozen {}s!\n", topic()));
            return 1;
        } else
            frozen = 1;
    }
    if ((re[i].flags & typ) == typ)
        return 1; /* already done */
    if (re[i].offset < 0) {
        std::println("Offset error."); /* should never happen */
        return 1;
    }
    if (typ & RF_SCRIBBLED && !get_yes(expand("scribok", DM_VAR), false))
        return 1;

    const auto path =
        std::format("{}/_{}", conflist[confidx].location, st_glob.i_current);
    if (frozen) {
        stat(path.c_str(), &stt);
        chmod(path.c_str(), stt.st_mode | S_IWUSR);
    }
    if ((fp = mopen(path, O_RPLUS)) != NULL) {
        if (frozen)
            chmod(path.c_str(), stt.st_mode & ~S_IWUSR);
        if (fseek(fp, re[i].offset, 0))
            error("fseeking in ", path);
        std::println(fp, ",R{:04}", typ);

        /* log it and overwrite it, unless it's a news article */
#ifdef NEWS
        if (!re[i].article) {
#endif
            get_resp(fp, &(re[i]), GR_ALL, i);
            fseek(fp, re[i].textoff, 0);
            if (typ & RF_SCRIBBLED) {
                const auto over = std::format("{} {} {} ", login,
                    get_date(time((time_t *)0), 0),
                    fullname_in_conference(&st_glob));
                len = over.size();
                /* was j=re[i].numchars below */
                for (j = (re[i].endoff - 3) - re[i].textoff; j > 76; j -= 76) {
                    for (k = 0; k < 75; k++)
                        std::print(fp, "{:c}", over[k % len]);
                    std::println(fp, "");
                }
                for (k = 0; k < j - 1; k++) fputc(over[k % len], fp);
                std::println(fp, "");
                std::println(fp, ",E");
            }
#ifdef NEWS
        }
#endif
        mclose(fp);

        /* Added 4/18, since sum file wasn't being updated, causing
         * set sensitive to fail. */
        sum[st_glob.i_current - 1].last = time((time_t *)0);
        save_sum(sum, (int)(st_glob.i_current - 1), confidx, &st_glob);
        dirty_sum(st_glob.i_current - 1);
    } else if (frozen)
        chmod(path.c_str(), stt.st_mode & ~S_IWUSR);

    /* free_sum(sum); unneeded, always SF_FAST */

    /* Write to censorlog */
    const auto censored = str::concat({bbsdir, "/censored"});
    if ((fp = mopen(censored, O_A | O_PRIVATE)) != NULL) {
        std::println(fp, ",C {} {} {} resp {} rflg {} {},{} {} date {}",
            conflist[confidx].location, topic(), st_glob.i_current, i, typ,
            login, uid, get_date(time((time_t *)0), 0),
            fullname_in_conference(&st_glob));
        std::println(fp, ",R{:04X}", re[i].flags);
        std::println(fp, ",U{},{}", re[i].uid, re[i].login);
        std::println(fp, ",A{}", re[i].fullname);
        std::println(fp, ",D{:08X}", re[i].date);
        std::println(fp, ",T");
        for (const auto &line : re[i].text) std::println(fp, "{}", line);
        std::println(fp, ",E");
        mclose(fp);
    }
    re[i].text.clear();
    re[i].flags = typ;

    custom_log((typ == RF_CENSORED) ? "censor" : "scribble", M_RFP);
    return 1;
}

/******************************************************************************/
/* UN-CENSOR A RESPONSE IN THE CURRENT ITEM                                   */
/******************************************************************************/
int
uncensor(       /* ARGUMENTS:                  */
    int argc,   /* Number of arguments      */
    char **argv /* Argument list            */
)
{
    int i;
    FILE *fp;

    /* Validate arguments */
    if (argc < 2 || sscanf(argv[1], "%d", &i) < 1) {
        std::println("You must specify a comment number.");
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

    /* Check for permission to uncensor */
    if (!re[i].date)
        get_resp(ext_fp, &(re[i]), GR_HEADER, i);
    if (!(st_glob.c_status & CS_FW) &&
        (uid != re[i].uid ||
            (uid == get_nobody_uid() && !str::eq(login, re[i].login)))) {
        std::println("You don't have permission to affect response {}.", i);
        return 1;
    }
    if (sum[st_glob.i_current - 1].flags & IF_FROZEN) {
        wputs(std::format("Cannot uncensor frozen {}s!\n", topic()));
        return 1;
    }
    if (!(re[i].flags & (RF_CENSORED | RF_SCRIBBLED)))
        return 1; /* already done */
    if (re[i].offset < 0) {
        std::println("Offset error."); /* should never happen */
        return 1;
    }
    const auto path =
        std::format("{}/_{}", conflist[confidx].location, st_glob.i_current);
    if ((fp = mopen(path, O_RPLUS)) != NULL) {
        if (fseek(fp, re[i].offset, 0))
            error("fseeking in ", path);
        std::println(fp, ",R{:04}", RF_NORMAL);

        mclose(fp);

        /* Added 4/18, since sum file wasn't being updated, causing
         * set sensitive to fail. */
        sum[st_glob.i_current - 1].last = time((time_t *)0);
        save_sum(sum, st_glob.i_current - 1, confidx, &st_glob);
        dirty_sum(st_glob.i_current - 1);
    }
    re[i].flags = RF_NORMAL;

    /* free_sum(sum); unneeded, always SF_FAST */

    return 1;
}

/******************************************************************************/
/* PRESERVE RESPONSES IN THE CURRENT ITEM                                     */
/******************************************************************************/
int
preserve(       /* ARGUMENTS:                  */
    int argc,   /* Number of arguments      */
    char **argv /* Argument list            */
)
{
    int i;
    int i_i;

    if (match(argv[0], "pr_eserve") || match(argv[0], "po_stpone") ||
        match(argv[0], "n_ew") || match(argv[0], "wait")) {
        if (!(flags & O_QUIET)) {
            wputs(std::format("This {} will still be new.\n", topic()));
        }
        st_glob.r_last = -2;
    } else
        st_glob.r_last = -1; /* re-read nothing */

    /* Lots of ways to stop, so check inverse */
    i_i = st_glob.i_current - 1;
    if (!match(argv[0], "pr_eserve") && !match(argv[0], "po_stpone") &&
        !match(argv[0], "p_ass") && !match(argv[0], "hide")) {
        if (st_glob.opt_flags & OF_REVERSE)
            st_glob.i_current = st_glob.i_first;
        else
            st_glob.i_current = st_glob.i_last;
        if (!(flags & O_QUIET))
            wputs("Stopping.\n");
        status |= S_STOP;
    }
    if (argc < 2) {
        mode = M_OK;
        return 1;
    }

    /* Validate arguments */
    if (sscanf(argv[1], "%d", &i) < 1) {
        std::println("You must specify a comment number.");
        return 1;
    } else if (argc > 2) {
        std::println("Bad parameters near \"{}\"", argv[2]);
        return 2;
    }
    refresh_sum(0, confidx, sum, part, &st_glob);
    if (i < 0 || i >= sum[i_i].nr) {
        wputs("Can't go that far! near \"<newline>\"\n");
        return 1;
    }

    /* Do it */
    part[i_i].nr = st_glob.r_lastseen = i;
    if (st_glob.r_last == -2) { /* preserve/new */
        st_glob.r_last = -1;
        part[i_i].last = sum[i_i].last - 1;
    } else
        time(&(part[i_i].last));
    dirty_part(i_i);
    mode = M_OK;

    return 1;
}

#ifdef INCLUDE_EXTRA_COMMANDS
int stack[MAX_RESPONSES], top = 0;
void
traverse(int i)
{
    int c, l, s;
    std::print("{}{:3d}", (top) ? "-" : " ", i);
    stack[top++] = i;
    c = child(i);
    if (c < 0)
        std::println("");
    else
        traverse(c);

    top--;
    if (!top)
        return;

    c = sibling(i);
    if (c >= 0) {
        for (l = 1; l <= top; l++) {
            std::print("   ");
            /* std::print("({})", stack[l]); */
            s = sibling(stack[l]);
            if (s < 0)
                std::print(" ");
            else if (l < top)
                std::print("|");
            else if (sibling(s) < 0)
                std::print("\\");
            else
                std::print("+");
        }
        traverse(c);
    }
}

/******************************************************************************/
/* DISPLAY RESPONSE TREE                                                      */
/******************************************************************************/
int
tree(           /* ARGUMENTS:                  */
    int argc,   /* Number of arguments      */
    char **argv /* Argument list            */
)
{
    int i = 0;
    /* Validate arguments */
    if (argc > 2 || (argc > 1 && sscanf(argv[1], "%d", &i) < 1)) {
        std::println("Bad parameters near \"{}\"", argv[2]);
        return 2;
    }
    refresh_sum(0, confidx, sum, part, &st_glob);
    if (i < 0 || i >= sum[st_glob.i_current - 1].nr) {
        wputs("Can't go that far! near \"<newline>\"\n");
        return 1;
    }
    traverse(i);
    return 1;
}

int
sibling(int r)
{
    int a, p;
    /* Find next sibling */
    p = parent(r);
    a = r + 1;
    if (!re[a].date)
        get_resp(ext_fp, &(re[a]), GR_HEADER, a);
    while (a <= st_glob.r_max && parent(a) != p) {
        a++;
        if (!re[a].date)
            get_resp(ext_fp, &(re[a]), GR_HEADER, a);
    }
    if (a > st_glob.r_max)
        return -1;
    else
        return a;
}

int
parent(int r)
{
    return (re[r].parent < 1) ? r - 1 : re[r].parent - 1;
}

int
child(int r)
{
    int a, p;
    /* Find 1st child */
    a = p = r + 1;
    if (!re[a].date)
        get_resp(ext_fp, &(re[a]), GR_HEADER, a);
    if (re[a].parent && re[a].parent != p) {
        a++;
        if (!re[a].date)
            get_resp(ext_fp, &(re[a]), GR_HEADER, a);
        while (a <= st_glob.r_max && re[a].parent != p) {
            a++;
            if (!re[a].date)
                get_resp(ext_fp, &(re[a]), GR_HEADER, a);
        }
    }
    return (a > st_glob.r_max) ? -1 : a;
}
#endif
