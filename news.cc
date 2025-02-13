// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include "news.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <algorithm>
#include <format>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include "arch.h"
#include "conf.h"
#include "dates.h"
#include "driver.h"
#include "files.h"
#include "globals.h"
#include "item.h"
#include "lib.h"
#include "license.h"
#include "macro.h"
#include "mem.h"
#include "range.h"
#include "rfp.h"
#include "sep.h"
#include "stats.h"
#include "str.h"
#include "sum.h"
#include "user.h"
#include "yapp.h"

/******************************************************************************/
/* ADD .SIGNATURE TO A MAIL MESSAGE                                           */
/******************************************************************************/
int
make_emtail(void)
{
    fflush(stdout);
    if (status & S_PAGER)
        fflush(st_glob.outp);

    /* Fork & setuid down when creating cf.buffer */
    auto cpid = fork();
    if (cpid < 0)
        return -1; /* error: couldn't fork */
    if (cpid) {    /* parent */
        pid_t wpid;
        int statusp;
        while ((wpid = wait(&statusp)) != cpid && wpid != -1);
        /* post = !statusp; */
    } else { /* child */
        if (setuid(getuid()))
            error("setuid", "");
        setgid(getgid());

        /* Save to cf.buffer */
        const auto cfbuffer = str::concat({work, "/cf.buffer"});
        FILE *fp = mopen(cfbuffer, O_A);
        if (fp == NULL)
            return 1;

        const auto sigpath = str::concat({home, "/.signature"});
        FILE *sp = mopen(sigpath, O_R | O_SILENT);
        if (sp != NULL) {
            std::println(fp, "--");
            std::string buff;
            while (ngets(buff, sp)) std::println(fp, "{}", buff);
            mclose(sp);
        }
        mclose(fp);
        exit(0);
    }
    return 1;
}
/******************************************************************************/
/* CREATE EMAIL HEADER                                                        */
/******************************************************************************/
int
make_emhead(response_t *re, int par)
{
    FILE *fp, *pp;
    flag_t ss;
    int cpid, wpid;
    int statusp;

    fflush(stdout);
    if (status & S_PAGER)
        fflush(st_glob.outp);

    /* Fork & setuid down when creating cf.buffer */
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
        const auto cfbuffer = str::concat({work, "/cf.buffer"});
        if ((fp = mopen(cfbuffer, O_W)) == NULL) {
            /*
             * re[i].text.clear();
             */
            return 1;
        }
        pp = st_glob.outp;
        ss = status;
        st_glob.r_current = par - 1;
        st_glob.outp = fp;
        status |= S_PAGER;

        itemsep(expand("mailheader", DM_VAR), 0);
        if (par)
            dump_reply("mailsep");

        st_glob.outp = pp;
        status = ss;
        mclose(fp);
        exit(0);
    }
    return 1;
}
/*
    if (par>0)
        std::println(fp, "References: <{}>",
            message_id(compress(conflist[confidx].name),
                st_glob.i_current, curr, re));
*/

#ifdef NEWS

/* So far each item can only have 1 response */

/******************************************************************************/
/* CREATE NEWS HEADER                                                         */
/******************************************************************************/
int
make_rnhead(response_t *re, int par)
{
    flag_t ss;
    short curr;
    int statusp;

    const char *sub = get_subj(confidx, st_glob.i_current - 1, sum);
    const auto config = get_config(confidx);
    if (config.size() <= CF_NEWSGROUP)
        return 0;

    fflush(stdout);
    if (status & S_PAGER)
        fflush(st_glob.outp);

    /* Fork & setuid down when creating .rnhead */
    auto cpid = fork();
    if (cpid < 0)
        return -1;   /* error: couldn't fork */
    if (cpid != 0) { /* parent */
        pid_t wpid;
        while ((wpid = wait(&statusp)) != cpid && wpid != -1);
        /* post = !statusp; */
    } else { /* child */
        if (setuid(getuid()))
            error("setuid", "");
        setgid(getgid());

        const auto path = str::concat({home, "/.rnhead"});
        FILE *fp = mopen(path, O_W);
        if (fp == nullptr)
            exit(1);
        std::println(fp, "Newsgroups: {}", config[CF_NEWSGROUP]);
        std::println(
            fp, "Subject: {}{}", (sub) ? "Re: " : "", (sub) ? sub : "");
        std::println(fp, "Summary: ");
        std::println(fp, "Expires: ");

        curr = par - 1;
        /* add parents mids:
         * std::println(fp, "References:");
         * std::vector<std::string> refs;
         * for (auto i = par - 1; i >= 0; i = re[i].parent - 1) {
         *	refs.push_back(std::format("<{}>",
         *	    message_id(compress(conflist[confidx].name)),
         *	        st_glob.i_current,curr,re));
         * }
         * for (const auto &ref: std::ranges::reverse_view(refs))
         * 	std:print(fp, "{}", ref);
         * std::println(fp,"{}");
         */
        if (par > 0)
            std::println(fp, "References: <{}>",
                message_id(compress(conflist[confidx].name), st_glob.i_current,
                    curr, re));
        std::println(fp, "Sender: ");
        std::println(fp, "Followup-To: ");
        std::println(fp, "Distribution: world");
        std::println(fp, "Organization: ");
        std::println(fp, "Keywords: ");
        std::println(fp, "");
        if (par > 0) { /* response to something? */
            FILE *pp = st_glob.outp;
            ss = status;
            st_glob.r_current = par - 1;
            st_glob.outp = fp;
            status |= S_PAGER;

            dump_reply("newssep");

            st_glob.outp = pp;
            status = ss;

            /* dump_reply("newssep"); don't dump to screen */
        }
        const auto sigpath = str::concat({home, "/.signature"});
        FILE *sp = mopen(sigpath, O_R | O_SILENT);
        if (sp != nullptr) {
            std::println(fp, "--");
            std::string buff;
            while (ngets(buff, sp)) std::println(fp, "{}s", buff);
            mclose(sp);
        }
        mclose(fp);
        exit(0);
    }
    return 1;
}
#endif

/*
 * Take some test and header info, and attempt to put it into a
 * given conference.  If the conference type has the CT_REGISTERED
 * bit set, make sure the author is a registered user first.
 */
static void
incorporate2(long art,                    /* Article number to incorporate */
    sumentry_t *sum,                      /* Item summary array to fill in */
    partentry_t *part,                    /* Participation info            */
    status_t *stt, int idx,               /* Conference index              */
    sumentry_t *thiss, char *sj,          /* Subject */
    const std::vector<std::string> &text, /* The actual body of text */
    char *mid,                            /* Unique message ID */
    char *fromL,                          /* Login of author */
    char *fromF                           /* Fullname of author */
)
{
    int i, sec;
    const auto config = get_config(idx);

    if (config.empty())
        return;

    /* Get conference security type */
    sec = security_type(config, idx);

    /* If registered bit is set, make sure fromL is a registered user */
    if (sec & CT_REGISTERED) {
        const auto parts = str::split(fromL, "@");
        std::string fullname;
        std::string home;
        std::string email;
        int found = 0;
        uid_t uid = 0;

        /* Check if fromL is a Unix user */
        std::string login_part(parts[0]);
        if (get_user(&uid, login_part, fullname, home, email)) {
            /* a local user was found with that login */

            if (parts.size() < 2 || str::eqcase(parts[1], hostname)) {
                /* Unix user found */
                found = 1;
            } else if (str::eqcase(fromL, email)) {
                /* Local user found the fast way:
                 * local login == remote login */
                found = 2;
            }
        }

        /* Check if fromL is a registered user with that email address
         */
        if (!found && email2login(fromL))
            found = 3;

        /* If not registered, skip this conference (don't post) */
        if (!found)
            return;
    }

    /* Find what item it should go in */
    i = stt->i_last + 1;
    /* Duplicate subjects are separate items */
    /* if (!strncmp(sj,"Re: ",4)) */
    for (i = stt->i_first;
        i <= stt->i_last &&
        (!sum[i - 1].nr || !sum[i - 1].flags ||
            (strncasecmp(sj + 4, get_subj(idx, i - 1, sum), MAX_LINE_LENGTH) &&
                strncasecmp(sj, get_subj(idx, i - 1, sum), MAX_LINE_LENGTH)));
        i++);
    /*
     * Duplicate subjects in same item
     * for (i = stt->i_first;
     *    i <= stt->i_last &&
     *         (sum[i-1].nr == 0 ||
     *            ((!str::eq(sj+4, get_subj(idx,i-1,sum)) || strncmp(sj,"Re:
     * ",4)) && str::eq(sj, get_subj(idx, i-1, sum)))); i++);
     */
    if (i > stt->i_last) {
        i = stt->i_last + 1;

        /* Enter a new item */
        /* std::println("{} Subject '{}' is new {} {}", art, sj, topic(), i); */
        if (!(flags & O_INCORPORATE))
            std::print(".");
        thiss->nr = 1;
        thiss->flags = IF_ACTIVE;
        do_enter(
            thiss, sj, text, idx, sum, part, stt, art, mid, uid, fromL, fromF);
        store_subj(idx, i - 1, sj);
    } else {
        short resp = 0;
        /* KKK Find previous reference for parent */
        /* Response to item i */
        /* std::println("{} Subject '{}' is response to {} {}",
         *     art, sj, topic(), i);
         */
        if (!(flags & O_INCORPORATE))
            std::print(".");
        stt->i_current = i;
        add_response(thiss, text, idx, sum, part, stt, art, mid, uid, fromL,
            fromF, resp);
    }
}
/******************************************************************************/
/* INCORPORATE: Incorporate a new article (art) into an item                */
/******************************************************************************/
int
incorporate(               /* ARGUMENTS: */
    long art,              /* Article number to incorporate */
    sumentry_t *sum,       /* Item summary array to fill in */
    partentry_t *part,     /* Participation info            */
    status_t *stt, int idx /* Conference index              */
)
{
    FILE *fp = NULL;
    std::string line;
    char sj2[MAX_LINE_LENGTH], mid[MAX_LINE_LENGTH], *sj,
        fromF[MAX_LINE_LENGTH], fromL[MAX_LINE_LENGTH];
    short i, j, k;
    sumentry_t thiss;
    std::vector<std::string_view> tolist[3];
    std::vector<std::string> text;
    std::vector<assoc_t> maillist;
    int placed = 0;

    if (flags & O_INCORPORATE) {
        maillist = grab_list(bbsdir, "maillist", 0);
        fp = st_glob.inp; /* st_glob.inp; */
#ifdef NEWS
    } else {
        /* Load in Subject and Date */
        const auto config = get_config(idx);
        if (config.size() <= CF_NEWSGROUP)
            return 0;
        const auto path =
            std::format("{}/{}/{}", get_conf_param("newsdir", NEWSDIR),
                dot2slash(config[CF_NEWSGROUP]), art);
        if ((fp = mopen(path, O_R)) == NULL)
            return 0;
#endif
    }

    sj = sj2;
    do {
        ngets(line, fp);
        char *buff = line.data();
        if (!strncmp(buff, "Subject: ", 9)) {
            if (!strncasecmp(buff + 9, "Re: ", 4))
                strcpy(sj, buff + 13);
            else
                strcpy(sj, buff + 9);
            while (sj[strlen(sj) - 1] == ' ') sj[strlen(sj) - 1] = 0;
            while (*sj == ' ') sj++;

        } else if ((flags & O_INCORPORATE) && !strncmp(buff, "To: ", 3)) {
            tolist[0] = str::split(buff + 4, ", ");
        } else if ((flags & O_INCORPORATE) && !strncmp(buff, "Cc: ", 3)) {
            tolist[1] = str::split(buff + 4, ", ");
        } else if ((flags & O_INCORPORATE) &&
                   !strncmp(buff, "Resent-To: ", 10)) {
            tolist[2] = str::split(buff + 11, ", ");
            /* KKK similarly for
             * newsgroups? */

        } else if (!strncmp(buff, "Date: ", 6)) {
            do_getdate(&(thiss.last), buff + 6);
            thiss.first = thiss.last;
        } else if (!strncmp(buff, "From: ", 6)) {
            char *from = buff + 6;
            char *p, *q;
            if ((p = strchr(from, '(')) != NULL) {
                /* login (fullname) */
                sscanf(from, "%s", fromL);
                q = strchr(p, ')');
                strncpy(fromF, p + 1, q - p - 1);
                fromF[q - p - 1] = '\0';
            } else if ((p = strchr(from, '<')) != NULL) {
                /* "fullname" <login> */
                char path[MAX_LINE_LENGTH];
                strncpy(path, from, p - from - 1);
                path[p - from - 1] = '\0';
                auto s = noquote(path);
                strcpy(fromF, s.c_str());
                q = strchr(p, '>');
                strncpy(fromL, p + 1, q - p - 1);
                fromL[q - p - 1] = '\0';
            } else { /* login */
                strcpy(fromL, from);
                strcpy(fromF, fromL);
            }
        } else if (!strncasecmp(buff, "Message-ID: <", 13)) {
            char *p;
            p = strchr(buff, '>');
            *p = '\0';
            strcpy(mid, buff + 13);
        }
    } while (!line.empty()); /* until blank line */
    if (flags & O_INCORPORATE)
        text = grab_more(fp, NULL, NULL);
    else /* for INCORPORATE, fp is stdin */
        mclose(fp);

    /* Incorporate message into each conference */
    if (flags & O_INCORPORATE) {
        for (k = 0; k <= 2; k++) {
            if (tolist[k].empty())
                continue;

            for (j = 0; j < tolist[k].size(); j++) {
                auto s = str::ltrim(tolist[k][j], "<");
                s = str::rtrim(s, ">");
                i = get_idx(s, maillist);
                if (i < 0)
                    continue;
                idx = get_idx(maillist[i].location, conflist);
                if (idx < 0)
                    continue;

                /* read in sumfile */
                get_status(&st_glob, sum, part, idx);
                incorporate2(art, sum, part, stt, idx, &thiss, sj, text, mid,
                    fromL, fromF);
                placed = 1;
            }
        }
        if (!placed) {
            if ((idx = get_idx(maillist[0].location, conflist)) > 0) {
                /* read in sumfile */
                get_status(&st_glob, sum, part, idx);
                incorporate2(art, sum, part, stt, idx, &thiss, sj, text, mid,
                    fromL, fromF);
            }
        }
    } else
        incorporate2(
            art, sum, part, stt, idx, &thiss, sj, text, mid, fromL, fromF);

    return 1;
}

#ifdef NEWS
void
news_show_header(void)
{
    short pr;

    open_pipe();
    const auto config = get_config(confidx);
    if (config.size() <= CF_NEWSGROUP)
        return;
    const auto path =
        std::format("{}/{}/{}", get_conf_param("newsdir", NEWSDIR),
            dot2slash(config[CF_NEWSGROUP]), st_glob.i_current);
    FILE *fp = mopen(path, O_R);
    if (fp == nullptr)
        return;
    /* current resp = header */
    st_glob.r_current = 0;
    /* Get header of #0 */
    get_resp(fp, re, (short)GR_HEADER, (short)0);
    /* The problem here is that itemsep uses r_current as an index
       to the information in re, so we can't show # new responses
          st_glob.r_current = sum[st_glob.i_current-1].nr
           - abs(part[st_glob.i_current-1].nr);
     */

    /* Get info about the actual item text if possible */
    if (re[st_glob.r_current].flags & (RF_EXPIRED | RF_SCRIBBLED))
        pr = 0;
    else if (re[st_glob.r_current].flags & RF_CENSORED)
        pr = ((st_glob.opt_flags & OF_NOFORGET) || !(flags & O_FORGET));
    else
        pr = 1;

    std::string buff;
    if (pr)
        get_resp(
            fp, &(re[st_glob.r_current]), (short)GR_ALL, st_glob.r_current);
    if ((re[st_glob.r_current].flags & RF_SCRIBBLED) &&
        re[st_glob.r_current].numchars > 7) {
        fseek(fp, re[st_glob.r_current].textoff, 0);
        ngets(buff, fp);
        re[st_glob.r_current].text.clear();
        re[st_glob.r_current].text.push_back(buff);
    }
    if (flags & O_LABEL)
        sepinit(IS_CENSORED | IS_UID | IS_DATE);
    itemsep(
        expand((st_glob.opt_flags & OF_int) ? "ishort" : "isep", DM_VAR), 0);
    if (pr) {
        re[st_glob.r_current].text.clear();
    }
    st_glob.r_current = 0; /* current resp = header */
    mclose(fp);
}
/******************************************************************************/
/* GENERATE UNIQUE MESSAGE ID FOR A RESPONSE                                  */
/******************************************************************************/
/* Takes response to make id for */
const std::string &
message_id(const std::string_view &c, int i, int r, response_t *re)
{
    if (re[r].mid.empty()) {
        re[r].mid = std::format(
            "{}.{}@{}", get_date(re[r].date, 14), re[r].uid, hostname);
    }
    return re[r].mid;
}
/******************************************************************************/
/* GET AN ACTUAL ARTICLE                                                      */
/******************************************************************************/
void
get_article(       /* ARGUMENTS                      */
    response_t *re /* Response to fill in         */
)
{
    /* LOCAL VARIABLES:               */
    char done = 0;

    re->text.clear();
    const auto config = get_config(confidx);
    if (config.size() <= CF_NEWSGROUP) {
        re->flags |= RF_EXPIRED;
        return;
    }
    const auto path =
        std::format("{}/{}/{}", get_conf_param("newsdir", NEWSDIR),
            dot2slash(config[CF_NEWSGROUP]), re->article);
    /* Article file pointer        */
    FILE *fp = mopen(path, O_R | O_SILENT);
    if (fp == nullptr) {
        re->flags |= RF_EXPIRED;
        /* anything else? */
        return;
    }

    /* Get response */
    re->flags = 0;
    re->mid.clear();
    re->parent = re->article = 0;

    while (!done && !(status & S_INT)) {
        std::string line;
        if (!ngets(line, fp)) {
            done = 1;
            break;
        }
        char *buff = line.data();
        if (!strncmp(buff, "Message-ID: <", 13)) {
            char *p;
            p = strchr(buff, '>');
            *p = '\0';
            re->mid = buff + 13;
        } else if (!strlen(buff)) {
            long textoff;
            textoff = ftell(fp);
            re->text = grab_more(fp, (flags & O_SIGNATURE) ? NULL : "--", NULL);
            re->numchars = ftell(fp) - textoff;
            done = 1;
            break;
        }
    }
    mclose(fp);
}
/******************************************************************************/
/* DOT2SLASH: Return directory/string/form of news.group.string passed in     */
/******************************************************************************/
std::string
dot2slash(                      /* ARGUMENTS:                      */
    const std::string_view &str /* Dot-separated string         */
)
{
    std::string out(str);
    std::replace(out.begin(), out.end(), '.', '/');
    return out;
}
/******************************************************************************/
/* REFRESH_NEWS: Look for any new articles, and if any are found, incorporate */
/* them into item files                                                       */
/******************************************************************************/
void
refresh_news(          /* ARGUMENTS:                     */
    sumentry_t *sum,   /* Summary array to update     */
    partentry_t *part, /* Participation info          */
    status_t *stt,     /* Status info to update       */
    int idx            /* Conference index to update  */
)
{
    struct stat st;
    long article;
    DIR *fp;
    FILE *artp;
    struct dirent *dp;
    int i;

    const auto config = get_config(idx);
    if (config.size() <= CF_NEWSGROUP)
        return;

    const auto path = str::join("/",
        {get_conf_param("newsdir", NEWSDIR), dot2slash(config[CF_NEWSGROUP])});
    if (stat(path.c_str(), &st)) {
        error("refreshing ", path);
        return;
    }

    /* Is there new stuff? */
    if (st.st_mtime != stt->sumtime) {
        long mod;
        struct stat artst;
        const auto artpath = str::concat({conflist[idx].location, "/article"});

        /* Create if doesn't exist, else update */
        if (stat(artpath.c_str(), &artst))
            mod = O_W;
        else
            mod = O_RPLUS;

        /* if (stt->c_security & CT_BASIC) mod |= O_PRIVATE; */
        if ((artp = mopen(artpath, mod)) == NULL)
            return; /* can't lock */

        if ((fp = opendir(path.c_str())) == NULL) {
            error("opening ", path);
            return;
        }
        refresh_stats(sum, part, stt); /* update stt */

        /* Load in stats 1 piece at a time - the slow stuff */
        article = stt->c_article;
        for (dp = readdir(fp); dp != NULL && !(status & S_INT);
            dp = readdir(fp)) {
            long i2;
            if (sscanf(dp->d_name, "%ld", &i2) == 1 && i2 > stt->c_article) {
                incorporate(i2, sum, part, stt, idx);
                if (i2 > article) {
                    article = i2;
                    fseek(artp, 0L, 0);
                    std::println(artp, "{}", article);
                }
                refresh_stats(sum, part, stt); /* update stt */
            }
        }
        closedir(fp);

        /* Check for expired */
        for (i = stt->i_first; i <= stt->i_last; i++) {
            response_t re;
            const auto path = std::format("{}/_{}", conflist[idx].location, i);
            FILE *fp2 = mopen(path, O_R);
            if (fp2 != nullptr) {
                re.fullname.clear();
                re.login.clear();
                re.mid.clear();
                re.text.clear();
                re.offset = -1;

                get_resp(fp2, &re, GR_ALL, 0);
                if (re.flags & RF_EXPIRED) {
                    sum[i - 1].flags |= IF_EXPIRED;
                    dirty_sum(i - 1);
                }
                mclose(fp2);

                re.fullname.clear();
                re.login.clear();
                re.text.clear();
                re.mid.clear();
            }
        }

        stt->sumtime = st.st_mtime;
        stt->c_article = article;
        refresh_stats(sum, part, stt); /* update stt */
        save_sum(sum, 0, idx, stt);

        mclose(artp); /* release lock on article file */
    }
}
#endif
