// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

/* STATS.C
 * This module will cache subjects and authors for items in conferences
 * Loading entries is delayed until reference time
 * NUMCACHE conferences may be cached at a time
 *
 * Now we want to be able to make use of the subjects file (if it exists)
 * to initialize the whole cache entry at once.
 *
 * Problem:
 *
 * Another process updates subjfile, want to make sure we re-read it
 * and not append it.
 *
 * Solution: ONLY append to subjfile when entering a new item
 *           let "set sum" and "set nosum" rewrite and delete it
 */

#include "stats.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <format>
#include <map>
#include <string_view>

#include "edit.h"
#include "globals.h"
#include "lib.h"
#include "mem.h"
#include "str.h"
#include "struct.h"
#include "sum.h"
#include "yapp.h"

#define NUMCACHE 2

typedef struct {
    short idx;    /* conference index              */
    int subjfile; /* # subjects in subject file    */
#define SF_DUNNO -1
#define SF_NO 0
    std::vector<std::string> config; /* config info                   */
    std::map<std::size_t, std::string> subj;
    std::map<std::size_t, std::string> auth;
} cache_t;

static cache_t cache[NUMCACHE];
static int start = 1;

static void
load_subj(int i, int idx, int item, sumentry_t *sum);

/*
 * GET INDEX OF CF
 *
 * Called from store_auth(), store_subj(), get_subj(), get_auth(), and
 * get_config() below to find location in the cache.
 */
/* ARGUMENTS:                    */
/* Conference #                */
static int
get_cache(int idx)
{
    int i;

    /* Initialize cache */
    if (start) {
        for (i = 0; i < NUMCACHE; i++) cache[i].idx = -1;
        start = 0;
        i = 0;
    } else {

        /* Find cf if already cached */
        for (i = 0; i < NUMCACHE && idx != cache[i].idx; i++);
        if (i < NUMCACHE)
            return i;

        /* Find one to evict */
        for (i = 0; cache[i].idx == confidx; i++); /* never evict current
                                                    * cf */

        /* Evict it */
        if (cache[i].idx >= 0) {
            cache[i].config.clear();
            cache[i].subj.clear();
            cache[i].auth.clear();
        }
    }

    /* Initialize with new conference info */
    cache[i].idx = idx;
    cache[i].subjfile = SF_DUNNO;

    const auto lines = grab_file(conflist[idx].location, "config", 0);
    if (lines.empty())
        return -1;
    cache[i].config = lines;

    return i;
}
/*
 * Free up all the space in the cache.  This is called right before
 * the program exits by endbbs().
 */
void
clear_cache(void)
{
    int i;
    for (i = 0; i < NUMCACHE; i++) {
        if (cache[i].idx < 0)
            continue;
        cache[i].config.clear();
        cache[i].subj.clear();
        cache[i].auth.clear();
        cache[i].idx = -1;
    }
}

#ifdef SUBJFILE
void
clear_subj(int idx) /* Conference # */
{
    int i;
    if ((i = get_cache(idx)) < 0)
        return;

    cache[i].subjfile = SF_NO;
}
/* Rewrite the entire subjects file */
void
rewrite_subj(int idx) /* Conference # */
{
    int i;
    if ((i = get_cache(idx)) < 0)
        return;

    /*
    std::println("rewrite: before... subjfile={} confitems={}",
        cache[i].subjfile, st_glob.c_confitems);
    */

    /* Make sure subject file is up to date */
    if (cache[i].subjfile <= st_glob.c_confitems) {
        int j, st;
        const auto filename =
            str::concat({conflist[idx].location, "/subjects"});

        /* Make sure we have subjects 1-st_glob.c_confitems */
        st = cache[i].subjfile;
        if (st < 0)
            st = 0;
        for (j = st; j <= st_glob.c_confitems; j++) {
            if (!(sum[j].flags & IF_ACTIVE))
                continue;

            if (cache[i].subj[j].empty())
                load_subj(i, idx, j, sum);
        }

        /* Append the new authors/subjects to the file */
        rm(filename, SL_OWNER);
        for (j = st; j < st_glob.c_confitems; j++) {
            std::string msg("\n");
            if (!cache[i].subj[j].empty())
                msg =
                    std::format("{}:{}\n", cache[i].auth[j], cache[i].subj[j]);
            if (!write_file(filename, msg))
                break;
            cache[i].subjfile = j + 1;
            /*std::print("rewrite: {} writing: {}", j, msg);*/
        }
    }
    /*
    std::println("rewrite: after... subjfile={} confitems={}",
        cache[i].subjfile, st_glob.c_confitems);
    */
}
/*
 * Write out entries to the conference subjects file
 */
int
update_subj(int idx, /* Conference # */
    int item         /* Item #       */
)
{
    int i = get_cache(idx);
    if (i < 0)
        return;
    if (cache[i].subjfile == SF_NO)
        return;
    /* Append the new author/subject to the file */
    const auto filname = str::concat({conflist[idx].location, "/subjects"});
    const auto msg =
        std::format("{}:{}\n", cache[i].auth[item], cache[i].subj[item]);
    if (!write_file(filename, msg))
        return 0;
    cache[i].subjfile = item + 1;
    /*
    std::println("update: writing '{}:{}'",
        cache[i].auth[item], cache[i].subj[item]);
    std::println("update: after writing to {}... subjfile={} confitems={}",
        filename, cache[i].subjfile, st_glob.c_confitems);
    */
}
#endif

/*
 * READ ITEM INFORMATION INTO THE CACHE
 * Called from get_subj() and get_auth() below
 */
static void
load_subj(    /* ARGUMENTS:         */
    int i,    /* Cache index     */
    int idx,  /* Conference #    */
    int item, /* Item #          */
    sumentry_t *sum)
{
    uint32_t tmp;

#ifdef SUBJFILE
    /*
    std::println("load: before... subjfile={} confitems={}",
        cache[i].subjfile, st_glob.c_confitems);
    */
    if (cache[i].subjfile != SF_NO) {
        const auto header =
            grab_file(conflist[idx].location, "subjects", GF_SILENT);
        if (header.empty()) {
            cache[i].subjfile = SF_NO;
        } else {
            size_t sz = header.size();
            for (size_t l = 0; l < sz; l++) {
                const auto field = str::splits(header[l], ":", false);
                if (!field.empty() && cache[i].auth[l].empty())
                    cache[i].auth[l] = field[0];
                if (field.size() > 1 && cache[i].subj[l].empty())
                    cache[i].subj[l] = field[1];
            }
            cache[i].subjfile = sz;
            /*
            std::println("load: after loading... subjfile={} confitems={}",
                cache[i].subjfile, st_glob.c_confitems);
            */
        }
        if (!cache[i].auth[item].empty() && !cache[i].subj[item].empty())
            return;
    }
    /*
    std::println("load: after... subjfile={} confitems={}",
        cache[i].subjfile, st_glob.c_confitems);
    */
#endif

    const auto path = std::format("{}/_{}", conflist[idx].location, item + 1);
    const auto headers = grab_file(path, "", GF_HEADER);
    if (headers.size() < 6) {
        sum[item].nr = 0;
        dirty_sum(item);
        return;
    }
    std::string subj, auth;
    for (const auto &header : headers) {
        if (header[0] != ',')
            continue;
        if (header[1] == 'H')
            subj = std::string(str::trim(header.c_str() + 2));
        else if (header[1] == 'U') {
            const char *a = strchr(header.c_str() + 2, ',');
            auth = a != nullptr ? a + 1 : "Unknown";
        }
    }

    if (cache[i].subj[item].empty())
        cache[i].subj[item] = subj;
    if (cache[i].auth[item].empty())
        cache[i].auth[item] = auth;

    sscanf(headers[5].c_str() + 2, "%x", &tmp);
    if (tmp != sum[item].first) {
        sum[item].first = tmp;
        dirty_sum(item);
    }
}

/*
 * Currently unused
 */
void
store_auth(int idx, int item, const std::string_view &str)
{
    int i;
    if ((i = get_cache(idx)) < 0)
        return;
    cache[i].auth[item] = std::string(str);
}
/*
 * Called from item_sum(), enter(), do_enter(), and incorporate2(),
 * i.e. when loading an old item or creating a new one
 */
void
store_subj(int idx, int item, const std::string_view &str)
{
    int i;
    if ((i = get_cache(idx)) < 0)
        return;
    cache[i].subj[item] = std::string(str);
}
/* LOOKUP A SUBJECT */
const char *
get_subj(           /* ARGUMENTS:              */
    int idx,        /* Conference index      */
    int item,       /* Item number           */
    sumentry_t *sum /* Summary array         */
)
{
    /*
    if (sum[item].flags & IF_RETIRED)
        return 0;
    */
    auto i = get_cache(idx);
    if (i < 0)
        return "";
    if (cache[i].subj[item].empty())
        load_subj(i, idx, item, sum);
    return cache[i].subj[item].c_str();
}
/*
 * LOOKUP AN AUTHOR
 *
 * This is used by the A=login range spec
 */
const char *
get_auth(           /* ARGUMENTS:                     */
    int idx,        /* Conference #                 */
    int item,       /* Item #                       */
    sumentry_t *sum /* Item summary array           */
)
{
    int i;
    /*
       if (sum[item].flags & IF_RETIRED)
                    return 0;
    */
    if ((i = get_cache(idx)) < 0)
        return 0;
    if (cache[i].auth[item].empty())
        load_subj(i, idx, item, sum);
    return cache[i].auth[item].c_str();
}

static constexpr std::vector<std::string> EMPTY_CONFIG;

const std::vector<std::string> &
get_config(int idx)
{
    if (idx < 0)
        return EMPTY_CONFIG;
    const auto i = get_cache(idx);
    if (i < 0)
        return EMPTY_CONFIG;
    return cache[i].config;
}
