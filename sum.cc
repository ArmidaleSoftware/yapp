// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include "sum.h"

#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <format>
#include <print>
#include <string>
#include <vector>

#include "files.h"
#include "globals.h"
#include "lib.h"
#include "license.h"
#include "macro.h"
#include "news.h"
#include "range.h"
#include "sep.h"
#include "stats.h"
#include "str.h"
#include "struct.h"
#include "yapp.h"

#define ST_LONG 0x01
#define ST_SWAP 0x02

#define INT_MAGIC 0x00001537L
#define INT_BACK 0x37150000L
#define LONG_MAGIC 0x12345678L
#define LONG_BACK 0x78563412L
#define OLD_YAPP 0x58585858L

typedef struct {
    uint32_t flags, nr;
    time_t last, first;
} longsumentry_t;

static int part_is_dirty = 0;
static int sum_is_dirty = 0;

/* Compute partfilename hash to appear in sum file */
uint32_t
get_hash(const std::string_view &str) /* IN : participation filename */
{
    uint32_t ret = 0;
    for (const auto *p = str.begin(); p != str.end(); ++p) ret = (ret * 4) ^ *p;
    return ret;
}

uint32_t
get_sumtype(const char *buff)
{
    uint32_t temp, sumtype;
    /* constant used for byte swap check */
    short swap = str::toi(get_conf_param("byteswap", BYTESWAP));
    /* Determine byte order & file type */
    memcpy((char *)&temp, buff, sizeof(uint32_t));
    switch (temp) {
    case INT_MAGIC:
        sumtype = ST_LONG;
        break;
    case INT_BACK:
        sumtype = ST_LONG | ST_SWAP;
        break;
    case LONG_MAGIC:
        sumtype = ST_LONG;
        break;
    case LONG_BACK:
        sumtype = ST_SWAP;
        break;
    case OLD_YAPP:
        swap = (*((char *)&swap));
        sumtype = swap * ST_SWAP;
        break;
    default:
        std::println("invalid sum type = {:08X}", temp);
        swap = (*((char *)&swap)); /* check which byte contains
                                    * the 1 */
        sumtype = swap * ST_SWAP;
        break;
    }

#ifdef SDEBUG
    std::println("sum type = {:08X} ({})", temp, sumtype);
    std::println("{} {}", (sumtype & ST_LONG) ? "long" : "short",
        (sumtype & ST_SWAP) ? "swap" : "normal");
#endif
    return sumtype;
}

/******************************************************************************/
/* SWAP BYTES IF LOW BIT IN HIGH BYTE (INTEL)                                 */
/* This is so machines with Intel processors and those with Motorola          */
/* processors can both use the same data files on the same filesystem         */
/******************************************************************************/
static void
byteswap(         /* ARGUMENTS:                */
    char *word,   /* Byte string to reverse */
    size_t length /* Number of bytes        */
)
{
    if (length < 2)
        return;
    for (int i = 0; i <= (length - 2) / 2; i++) {
        word[i] ^= word[length - i - 1];
        word[length - i - 1] ^= word[i];
        word[i] ^= word[length - i - 1];
    }
}

#ifdef NEWS
void
save_article(long art, int idx)
{
    const auto path = str::concat({conflist[idx].location, "/article"});

    /* Create if doesn't exist, else update */
    long mod;
    struct stat st;
    if (stat(path.c_str(), &st))
        mod = O_W;
    else
        mod = O_RPLUS;

    /* if (stt->c_security & CT_BASIC) mod |= O_PRIVATE; */
    FILE *fp = mopen(path, mod);
    if (fp == NULL)
        return;
    std::println(fp, "{}", art);
    mclose(fp);
}

void
load_article(long *art, int idx)
{
    const auto path = str::concat({conflist[idx].location, "/article"});
    FILE *fp = mopen(path, O_R);
    if (fp == NULL) {
        *art = 0;
        return;
    }
    fscanf(fp, "%ld\n", art);
    mclose(fp);
}
#endif

/******************************************************************************/
/* SAVE SUM FILE FOR CONFERENCE                                               */
/******************************************************************************/
void
save_sum(sumentry_t *newsum, /* IN/OUT: Modified record              */
    int where,               /* IN:     Index of modified record     */
    int idx,                 /* IN:     Conference to write file for */
    status_t *stt            /* */
)
{
    FILE *fp;
    short i = 1;
    char buff[17];
    sumentry_t entry;
    longsumentry_t longentry;
    /* constant used for * byte swap check */
    short swap = str::toi(get_conf_param("byteswap", BYTESWAP));
    long mod;
    struct stat st;
    uint32_t temp, sumtype;

    if (debug & DB_SUM)
        std::println("SAVE_SUM {:x} {}", (uintptr_t)newsum, where);

    swap = (*((char *)&swap)); /* check which byte contains the 1 */

    const auto path = str::concat({conflist[idx].location, "/sum"});

    /* Create if doesn't exist, else update */
    if (stat(path.c_str(), &st) != 0)
        mod = O_W;
    else
        mod = O_RPLUS;

    if (stt->c_security & CT_BASIC)
        mod |= O_PRIVATE;
    if ((fp = mopen(path, mod)) == NULL)
        return;

    /* Determine file type */
    if (mod == O_W || (i = fread(buff, 20, 1, fp)) < 20) /* new file */
        sumtype = ST_LONG;
    else
        sumtype = get_sumtype(buff + 12);
    rewind(fp);

    const auto config = get_config(idx);
    if (config.size() <= CF_PARTFILE)
        return;

    /* Write header */
    if (sumtype & ST_LONG) {
        fwrite("!<sm02>\n", 8, 1, fp);
        temp = get_hash(config[CF_PARTFILE]);
        fwrite((char *)&temp, sizeof(uint32_t), 1, fp);
        temp = INT_MAGIC;
        fwrite((char *)&temp, sizeof(uint32_t), 1, fp);
        temp = LONG_MAGIC;
        fwrite((char *)&temp, sizeof(uint32_t), 1, fp);
    } else {
        fwrite("!<pr03>\n", 8, 1, fp);
        short tshort = get_hash(config[CF_PARTFILE]);
        fwrite((char *)&tshort, sizeof(uint32_t), 1, fp);
        temp = INT_MAGIC;
        fwrite((char *)&tshort, sizeof(uint32_t), 1, fp);
        temp = LONG_MAGIC;
        fwrite((char *)&temp, sizeof(uint32_t), 1, fp);
    }

    if (debug & DB_SUM)
        std::println("Saving {} {}s\n", stt->c_confitems, topic());
    if (where + 1 > stt->c_confitems)
        stt->c_confitems = where + 1;
    for (i = 1; i <= stt->c_confitems; i++) {
        uint32_t t;
        /* if (newsum[i-1].nr) std::println("{}: {}",i,newsum[i-1].nr); */
        t = newsum[i - 1].flags;
        newsum[i - 1].flags &= IF_SAVEMASK;

        if (sumtype & ST_LONG) {
            longentry.nr = newsum[i - 1].nr;
            longentry.flags = newsum[i - 1].flags;
            longentry.last = newsum[i - 1].last;
            longentry.first = newsum[i - 1].first;
            if (sumtype & ST_SWAP) {
                byteswap((char *)&(longentry.nr), sizeof(uint32_t));
                byteswap((char *)&(longentry.flags), sizeof(uint32_t));
                byteswap((char *)&(longentry.first), sizeof(time_t));
                byteswap((char *)&(longentry.last), sizeof(time_t));
            }
            fwrite((char *)&longentry, sizeof(longsumentry_t), 1, fp);
        } else {
            memcpy(
                (char *)&entry, (char *)&(newsum[i - 1]), sizeof(sumentry_t));
            if (sumtype & ST_SWAP) {
                byteswap((char *)&(entry.nr), sizeof(uint32_t));
                byteswap((char *)&(entry.flags), sizeof(uint32_t));
                byteswap((char *)&(entry.first), sizeof(time_t));
                byteswap((char *)&(entry.last), sizeof(time_t));
            }
            fwrite((char *)&entry, sizeof(sumentry_t), 1, fp);
        }

        newsum[i - 1].flags = t;
    }

    mclose(fp);
}

void
refresh_sum(int item,  /* IN:     Item index               */
    int idx,           /* IN:     Conference index         */
    sumentry_t *sum,   /* IN/OUT: Item summary             */
    partentry_t *part, /* User participation info  */
    status_t *stt      /* IN/OUT: Status structure         */
)
{
    struct stat st;
    short last, first;

    if (idx < 0)
        return;

#ifdef NEWS
    const auto config = get_config(idx);
    if (config.size() > CF_NEWSGROUP && (stt->c_security & CT_NEWS) != 0) {
        const auto path = str::join("/", {get_conf_param("newsdir", NEWSDIR),
                                             dot2slash(config[CF_NEWSGROUP])});
        if (stat(path.c_str(), &st)) {
            stt->sumtime = 0;
            st.st_mtime = 1;
        }
        if (st.st_mtime != stt->sumtime) {
            load_sum(sum, part, stt, idx);
            stt->sumtime = st.st_mtime;
        }
        /* update stt */
        refresh_stats(sum, part, stt);
        return;
    }
#endif

    /* Is global information current? */
    const auto path = str::concat({conflist[idx].location, "/sum"});
    if (stat(path.c_str(), &st) != 0) {
        stt->sumtime = 0;
        st.st_mtime = 1;
    }
    if (st.st_mtime != stt->sumtime) {
        /* Load global information */
        load_sum(sum, part, stt, idx);
        stt->sumtime = st.st_mtime;
        refresh_stats(sum, part, stt);
    }

    /* Are links current? */
    last = (item > 0) ? item : MAX_ITEMS;
    first = (item > 0) ? item : 1;
    for (int i = first - 1; i < last; i++) refresh_link(stt, sum, part, idx, i);

    /* Need to refresh stats anyway, in case part[] changed */
    if (sum_is_dirty || part_is_dirty)
        refresh_stats(sum, part, stt); /* update stt */
}

int
item_sum(int i,        /* IN: Item number                   */
    sumentry_t *sum,   /* Item summary array to fill in */
    partentry_t *part, /* Participation info            */
    int idx,           /* IN: Conference index              */
    status_t *stt)
{
    FILE *fp;
    struct stat st;
    std::string buff;

    sum[i].flags = sum[i].nr = 0;
    dirty_sum(i);

    const auto config = get_config(idx);
    if (config.empty())
        return 0;

    const auto path = std::format("{}/_{}", conflist[idx].location, i + 1);
    if (stat(path.c_str(), &st))
        return 0;
    else
        sum[i].flags |= IF_ACTIVE;

    if (st.st_nlink > 1)
        sum[i].flags |= IF_LINKED;
    if (!(st.st_mode & S_IWUSR))
        sum[i].flags |= IF_FROZEN;
    if (part[i].nr < 0)
        sum[i].flags |= IF_FORGOTTEN;
    sum[i].last = st.st_mtime;
    if (!(st.st_mode & S_IRUSR) || !(fp = mopen(path, O_R))) {
        sum[i].flags |= IF_RETIRED;
        sum[i].nr = 0;
        sum[i].first = 0;
    } else {
        if (st.st_mode & S_IXUSR)
            sum[i].flags |= IF_RETIRED;
        sum[i].nr = 0; /* count them */

        ngets(buff, fp); /* magic - ignore */
        ngets(buff, fp); /* H - ignore if FAST */
        store_subj(idx, i, buff.c_str() + 2);
        ngets(buff, fp); /* R - ignore */
        ngets(buff, fp); /* U - ignore */
        auto p = strchr(buff.c_str() + 2, ',');
        if (p != nullptr)
            store_auth(idx, i, p + 1);
        ngets(buff, fp); /* A - ignore */
        ngets(buff, fp); /* date */
        sscanf(buff.c_str() + 2, "%lx", &(sum[i].first));
        while (ngets(buff, fp)) {
            if (str::eq(buff, ",T"))
                sum[i].nr++;

#ifdef NEWS
            if (buff.starts_with(",N")) {
                long art = atoi(buff.c_str() + 2);

                /* Check to see if it has expired */
                const auto buff =
                    std::format("{}/{}/{}", get_conf_param("newsdir", NEWSDIR),
                        dot2slash(config[CF_NEWSGROUP]), art);
                std::print("Checking {} ", buff);
                FILE *nfp = mopen(buff, O_R | O_SILENT);
                if (nfp == nullptr) {
                    sum[i].flags |= IF_EXPIRED;
                    std::println("EXPIRED");
                } else {
                    mclose(nfp);
                    std::println("OK");
                }

                if (art > stt->c_article)
                    stt->c_article = art;
                /* std::println("Found article {}",art); */
            }
#endif
        }
        mclose(fp);
    }
    return 1;
}

/******************************************************************************/
/* Load SUM data for arbitrary conference (requires part be done previously)  */
/******************************************************************************/
void
load_sum(              /* ARGUMENTS: */
    sumentry_t *sum,   /* Item summary array to fill in */
    partentry_t *part, /* Participation info */
    status_t *stt,     /* */
    int idx            /* IN: Conference index */
)
{
    FILE *fp;
    short i = 1, j;
    struct stat st;
    char buff[MAX_LINE_LENGTH];
    short confitems = 0;
    /* constant used for * byte swap check */
    short swap = str::toi(get_conf_param("byteswap", BYTESWAP));
    uint32_t sumtype = 0;
    uint32_t temp;
    short tshort;

    swap = (*((char *)&swap));
    for (j = 0; j < MAX_ITEMS; j++) {
        sum[j].nr = sum[j].flags = 0;
        dirty_sum(j);
    }

    /* For NFS mounted cf, open is 27 secs with lock(failing) and 4
     * without the lock.  Why should we lock it anyway? */
    const auto path = str::concat({conflist[idx].location, "/sum"});
    fp = mopen(path, O_R | O_SILENT);

    /* If SUM doesn't exist */
    if (fp == NULL) {
        /* if ((fp=mopen(path,O_RPLUS|O_LOCK|O_SILENT))==NULL) */
        DIR *fp = opendir(conflist[idx].location.c_str());
        if (fp == NULL) {
            error("opening ", path);
            return;
        }

        /* Load in stats 1 piece at a time - the slow stuff */
        struct dirent *dp;
        for (dp = readdir(fp); dp != NULL; dp = readdir(fp)) {
            long i2;
            if (sscanf(dp->d_name, "_%ld", &i2) == 1) {
                i = i2 - 1;
                if (item_sum(i, sum, part, idx, stt)) {
                    confitems++;
                    if (i2 > stt->c_confitems)
                        stt->c_confitems = i2;
                }
            }
        }
        closedir(fp);

        /* Load in stats 1 piece at a time - the slow stuff for (i=0;
         * i<MAX_ITEMS; i++) { confitems +=
         * item_sum(i,sum,part,idx,stt); } */

#ifdef NEWS
        /* Update ITEM files with new articles */
        /* std::println("Article={}",stt->c_article); */
        if (stt->c_security & CT_NEWS)
            refresh_news(sum, part, stt, idx);
#endif
        refresh_stats(sum, part, stt);
        save_sum(sum, (short)-1, idx, stt);
        return;
    }

    /* Read in SUM file - the fast stuff */
    if (stat(path.c_str(), &st) == 0)
        stt->sumtime = st.st_mtime;

    i = 0;
    buff[0] = 0;
    if (fp != nullptr)
        i = fread(buff, 20, 1, fp);
    if (i <= 0 || (memcmp(buff, "!<sm02>\n", 8) != 0 &&
                      memcmp(buff, "!<pr03>\n", 8) != 0)) {
        if (fp != nullptr)
            mclose(fp);
        errno = 0;
        error(path, " failed magic check");
        /* std::println("WARNING: {} failed magic check",path); */

        /* Load in stats 1 piece at a time - the slow stuff */
        for (i = 0; i < MAX_ITEMS; i++) {
            if (item_sum(i, sum, part, idx, stt)) {
                confitems++;
                if (i + 1 > stt->c_confitems)
                    stt->c_confitems = i + 1;
            }
        }
        refresh_stats(sum, part, stt);
        save_sum(sum, (short)-1, idx, stt);
        return;
    }

    /*
       fread((char *)&confitems, sizeof(confitems),1,fp); * skip first 16
       bytes * fread((char *)sum,sizeof(sumentry_t),1,fp); *fseek fails for
       some reason *
    */

    /* Determine byte order & file type */
    sumtype = get_sumtype(buff + 12);
    const auto config = get_config(idx);
    if (config.size() <= CF_PARTFILE)
        return;

    if (sumtype & ST_LONG) {
        /* skip 4 more bytes */
        // fread((char *)&temp, sizeof(uint32_t), 1, fp);
        memcpy((char *)&temp, buff + 8, sizeof(uint32_t));
        if (sumtype & ST_SWAP)
            byteswap((char *)&temp, sizeof(uint32_t));
        if (temp != get_hash(config[CF_PARTFILE])) {
            errno = 0;
            error("bad participation filename hash for ", config[CF_PARTFILE]);
        }
    } else {
        memcpy((char *)&tshort, buff + 8, sizeof(short));
        if (sumtype & ST_SWAP)
            byteswap((char *)&temp, sizeof(short));
        if (tshort != (short)get_hash(config[CF_PARTFILE])) {
            errno = 0;
            error("bad participation filename hash for ", config[CF_PARTFILE]);
        }
    }

#ifdef NEWS
    if (stt->c_security & CT_NEWS)
        load_article(&stt->c_article, idx);
#endif

    if (sumtype & ST_LONG) {
        longsumentry_t longsum[MAX_ITEMS];
        confitems =
            fread((char *)longsum, sizeof(longsumentry_t), MAX_ITEMS, fp);

        for (i = 0; i < confitems; i++) {
            if (!longsum[i].nr)
                continue; /* skip if deleted */

            if (sumtype & ST_SWAP) {
                byteswap((char *)&(longsum[i].nr), sizeof(uint32_t));
                byteswap((char *)&(longsum[i].flags), sizeof(uint32_t));
                byteswap((char *)&(longsum[i].first), sizeof(time_t));
                byteswap((char *)&(longsum[i].last), sizeof(time_t));
            }
            sum[i].nr = longsum[i].nr;
            sum[i].flags = longsum[i].flags;
            sum[i].first = longsum[i].first;
            sum[i].last = longsum[i].last;
        }

    } else {
        confitems = fread((char *)sum, sizeof(sumentry_t), MAX_ITEMS, fp);

        for (i = 0; i < confitems; i++) {
            if (!sum[i].nr)
                continue; /* skip if deleted */

            /* Check for byte swapping and such */
            if (sumtype & ST_SWAP) {
                byteswap((char *)&(sum[i].nr), sizeof(uint32_t));
                byteswap((char *)&(sum[i].flags), sizeof(uint32_t));
                byteswap((char *)&(sum[i].first), sizeof(time_t));
                byteswap((char *)&(sum[i].last), sizeof(time_t));
            }
        }
    }

    mclose(fp);
    if (debug & DB_SUM)
        std::println("confitems={}", confitems);
    stt->c_confitems = confitems;

    for (i = 0; i < confitems; i++) {
        if (!sum[i].nr)
            continue; /* skip if deleted */

        if (sum[i].nr < 0 || sum[i].nr > MAX_RESPONSES) {
            std::println("Invalid format of sum file, nr={}", sum[i].nr);
            break;
        }
        if (part[i].nr < 0)
            sum[i].flags |= IF_FORGOTTEN;

        /* verify it's still linked, didnt used to check sumtime */
        refresh_link(stt, sum, part, idx, i);
    }

    for (; i < MAX_ITEMS; i++) { sum[i].flags = sum[i].nr = 0; }

#ifdef NEWS
    /* Update ITEM files with new articles */
    if (stt->c_security & CT_NEWS)
        refresh_news(sum, part, stt, idx);
#endif
    refresh_stats(sum, part, stt);
}

void
dirty_part(int i)
{
    part_is_dirty = 1;
}

void
dirty_sum(int i)
{
    sum_is_dirty = 1;
}

void
refresh_stats(sumentry_t *sum, /* IN:     Sum array to fill in (optional)    */
    partentry_t *part,         /* IN:     Previously read participation data */
    status_t *st               /* IN/OUT: pointer to status structure        */
)
{
    int i, last, first, n;
    st->i_brandnew = st->i_newresp = st->i_unseen = 0;
    st->r_totalnewresp = 0;
    st->i_last = 0;

    /* Find first valid item */
    i = 0;
    while (i < MAX_ITEMS &&
           (!sum[i].nr || !sum[i].flags || (sum[i].flags & IF_EXPIRED)))
        i++;
    first = i + 1;

    /* Find last valid item */
    i = MAX_ITEMS - 1;
    while (i >= first &&
           (!sum[i].nr || !sum[i].flags || (sum[i].flags & IF_EXPIRED)))
        i--;
    last = i + 1;

    for (n = 0, i = first - 1; i < last; i++) {
        if (sum[i].nr) {
            if (!sum[i].flags)
                continue;
            if (!(sum[i].flags & IF_EXPIRED))
                n++;
            if ((sum[i].flags & (IF_RETIRED | IF_FORGOTTEN | IF_EXPIRED)) &&
                (flags & O_FORGET))
                continue;
            if (!part[i].nr && sum[i].nr) {
                st->i_unseen++; /* unseen */
                if (is_brandnew(&part[i], &sum[i]))
                    st->i_brandnew++;
            } else if (is_newresp(&part[i], &sum[i]))
                st->i_newresp++;
            st->r_totalnewresp += sum[i].nr - abs(part[i].nr);
        }
    }

    if (!n) {
        first = 1;
        last = 0;
    }
    st->i_first = first;
    st->i_last = last;
    st->i_numitems = n;

    part_is_dirty = 0;
    sum_is_dirty = 0;
}

void
refresh_link(status_t *stt, /* pointer to status structure */
    sumentry_t *sum,        /* IN/OUT: Sum array to fill in (optional) */
    partentry_t *part,      /* Previously read participation data  */
    int idx,                /* IN: Index of conference to process     */
    int i                   /* IN: Item index                         */
)
{
    /* verify it's still linked */
    if ((sum[i].flags & IF_LINKED) == 0)
        return;
    const auto path = std::format("{}/_{}", conflist[idx].location, i + 1);
    struct stat st;
    if (stat(path.c_str(), &st) || st.st_nlink < 2) {
        sum[i].flags &= ~IF_LINKED;
        dirty_sum(i);
    }
    if (st.st_mtime > stt->sumtime) { /* new activity */
        item_sum(i, sum, part, idx, stt);
    }
}

/******************************************************************************/
/* UPDATE THE GLOBAL STATUS STRUCTURE, OPTIONALLY GET ITEM SUBJECTS           */
/******************************************************************************/
/* note: does load_sum and not free_sum if argument is there */
void
get_status(            /* ARGUMENTS:                            */
    status_t *st,      /* pointer to status structure */
    sumentry_t *s,     /* Sum array to fill in (optional)    */
    partentry_t *part, /* Previously read participation data */
    int idx            /* IN: Index of conference to process     */
)
{
    sumentry_t s1[MAX_ITEMS];
    sumentry_t *sum;
    short i;
    sum = (s) ? s : s1;

    if (st != (&st_glob))
        st->sumtime = 0;
    refresh_sum(0, idx, sum, part, st);

    /* Are links current? */
    for (i = 0; i < MAX_ITEMS; i++) refresh_link(st, sum, part, idx, i);

    refresh_stats(sum, part, st);
}

void
check_mail(int f)
{
    static int prev = 0;
    int f2 = f;
    std::string mail;
    struct stat st;

    /*
     * Mail is currently only checked in conf.c, when should new mail be
     * reported?  At Ok:? or only when join or display new? If conf.c is
     * the only place, perhaps it should be moved there. Note: the above
     * was fixed when seps were added
     */
    mail = expand("mailbox", DM_VAR);
    if (mail.empty())
        mail = getenv("MAIL");
    if (mail.empty()) {
        mail = std::format("{}/{}", get_conf_param("maildir", MAILDIR), login);
    }
    if (stat(mail.c_str(), &st) == 0 && st.st_size > 0) {
        status |= S_MAIL;
        if (st_glob.mailsize && st.st_size > st_glob.mailsize)
            status |= S_MOREMAIL;
        st_glob.mailsize = st.st_size;
    } else {
        status &= ~S_MAIL;
        st_glob.mailsize = 0;
    }

    if (status & S_MAIL) {
        if (!prev)
            f2 = 1;
        if (status & S_MOREMAIL) {
            sepinit(IS_START | IS_ITEM);
            f2 = 1;
        }
        if (f2)
            confsep(expand("mailmsg", DM_VAR), confidx, &st_glob, part, 0);
        status &= ~S_MOREMAIL;
    }
    prev = (status & S_MAIL);
}
