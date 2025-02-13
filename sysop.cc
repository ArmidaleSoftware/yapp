// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

/* SYSOP.C - cfadm-only commands like cfcreate and cfdelete */

#include <sys/stat.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cstdio>
#include <format>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include "conf.h"
#include "files.h"
#include "globals.h"
#include "lib.h"
#include "license.h"
#include "log.h"
#include "macro.h"
#include "mem.h"
#include "security.h"
#include "stats.h"
#include "str.h"
#include "struct.h"
#include "sum.h"
#include "system.h"
#include "user.h"
#include "yapp.h"

/******************************************************************************/
/* CHECK WHETHER USER QUALIFIES AS THE SYSOP/CFADM                            */
/******************************************************************************/
int
is_sysop(int silent) /* Skip error message? */
{
    if (uid == get_nobody_uid()) {
        if (str::eq(login, get_sysop_login()))
            return 1;
    } else if (uid == geteuid())
        return 1;
    if (!silent)
        std::println("Permission denied.");
    return 0;
}

void
reload_conflist(void)
{
    if (!(flags & O_QUIET))
        std::println("Reloading conflist...");
    const auto &buff = confidx >= 0 ? conflist[confidx].name : "";
    const auto &path = joinidx >= 0 ? conflist[joinidx].name : "";
    conflist = grab_list(bbsdir, "conflist", 0);
    desclist = grab_list(bbsdir, "desclist", 0);
    for (auto i = 1; i < conflist.size(); i++) {
        if (str::eq(conflist[0].location, conflist[i].location) ||
            match(conflist[0].location, conflist[i].name))
            defidx = i;
        if (!buff.empty() && str::eq(buff, conflist[i].name))
            confidx = i;
        if (!path.empty() && str::eq(path, conflist[i].name))
            joinidx = i;
    }
    if (defidx < 0)
        std::println("Warning: bad default {}", conference());
}
/******************************************************************************/
/* CREATE A CONFERENCE                                                        */
/******************************************************************************/
int
cfcreate(       /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    char *cfshort = NULL, *cflong = NULL, *cfemail = NULL, *cfsubdir = NULL,
         *cftype = NULL, *cfhosts = NULL;
    int ok = 1, chacl, previdx;

    if (!is_sysop(0))
        return 1;

    const bool prompt = (flags & O_QUIET) == 0;
    /* Get configuration information */
    if (prompt) {
        std::print("Short name (including underlines): ");
        std::fflush(stdout);
    }
    cfshort = xgets(st_glob.inp, 0);
    if (!cfshort || !cfshort[0])
        ok = 0;

    if (ok) {
        if (prompt) {
            std::println("Enter one-line description");
            std::print("> ");
            std::fflush(stdout);
        }
        cflong = xgets(st_glob.inp, 0);
        if (!cflong || !cflong[0])
            ok = 0;
    }
    std::string confdir, cfpath;
    if (ok) {
        if (prompt) {
            std::println("Subdirectory [{}]: ", compress(cfshort));
            std::fflush(stdout);
        }
        cfsubdir = xgets(st_glob.inp, 0);
        if (!cfsubdir[0]) {
            free(cfsubdir);
            cfsubdir = estrdup(compress(cfshort).c_str());
        }
        confdir = str::concat({get_conf_param("bbsdir", BBSDIR), "/confs"});
        cfpath = str::join("/", {get_conf_param("confdir", confdir), cfsubdir});

        if (!cfsubdir || !cfsubdir[0])
            ok = 0;
    }
    if (ok) {
        if (prompt) {
            std::print("Fairwitnesses: ");
            std::fflush(stdout);
        }
        cfhosts = xgets(st_glob.inp, 0);
        if (!cfhosts || !cfhosts[0])
            ok = 0;
    }
    if (ok) {
        if (prompt) {
            std::print("Security type: ");
            std::fflush(stdout);
        }
        cftype = xgets(st_glob.inp, 0);
        if (!cftype || !cftype[0])
            ok = 0;
    }
    if (ok) {
        const auto prompt = std::format(
            "Let a {} change the access control list? ", fairwitness());
        chacl = get_yes(prompt, true);
    }
    if (ok) {
        if (prompt) {
            std::println("Email address(es) (only used for mail type {}s): ",
                conference());
            std::fflush(stdout);
        }
        cfemail = xgets(st_glob.inp, 0);
        if (!cfemail)
            ok = 0;
    }

    /* Create the conference */
    if (ok) {
        if (prompt)
            std::println("Creating conflist entry...");
        const auto path = str::concat({bbsdir, "/conflist"});
        const auto content = std::format("{}:{}\n", cfshort, cfpath);
        ok = write_file(path, content);
    }
    if (ok) {
        if ((flags & O_QUIET) == 0)
            std::println("Creating desclist entry...");
        const auto path = str::concat({bbsdir, "/desclist"});
        const auto content = std::format("{}:{}\n", compress(cfshort), cflong);
        ok = write_file(path, content);
    }
    if (ok && cfemail[0]) {
        const auto emails = str::split(cfemail, " ");
        if ((flags & O_QUIET) == 0)
            std::println("Creating maillist entry...");
        /* create maillist entry */
        const auto path = str::concat({bbsdir, "/maillist"});
        for (const auto &addr : emails) {
            const auto line = std::format("{}:{}\n", addr, compress(cfshort));
            ok = write_file(path, line);
            if (!ok)
                break;
        }
    }
    if (ok) {
        if (prompt)
            std::println("Creating directory...");
        mkdir_all(cfpath, 0755);
    }
    if (ok) {
        if (prompt)
            std::println("Creating config file...");
        const auto path =
            str::concat({cfpath, "/config"}); /* create config file */
        const auto content = std::format("!<pc02>\n.{}.cf\n0\n{}\n{}\n{}\n",
            cfsubdir, cfhosts, cftype, cfemail);
        ok = write_file(path, content);
        chmod(path.c_str(), 0644);
    }
    if (ok) {
        if (prompt)
            std::println("Creating login file...");
        const auto path =
            str::concat({cfpath, "/login"}); /* create login file */
        const auto content = std::format(
            "Welcome to the {} {}.  This file may be edited by a {}.\n",
            compress(cfshort), conference(), fairwitness());
        ok = write_file(path, content);
        chmod(path.c_str(), 0644);
    }
    if (ok) {
        if (!(flags & O_QUIET))
            std::println("Creating logout file...");
        const auto path =
            str::concat({cfpath, "/logout"}); /* create logout file */
        const auto content = std::format("You are now leaving the {} {}.  This "
                                         "file may be edited by a {}.\n",
            compress(cfshort), conference(), fairwitness());
        write_file(path, content);
        chmod(path.c_str(), 0644);
    }
    reload_conflist(); /* Must be done before load_acl() */
    previdx = confidx; /* Save confidx to restore after expanding
                        * macros */
    confidx = get_idx(compress(cfshort), conflist);
    if (confidx < 0)
        ok = 0;
    load_acl(confidx); /* load acl for new conference */

    if (ok) {
        if (!(flags & O_QUIET))
            std::println("Creating acl file...");
        const auto path = str::concat({cfpath, "/acl"}); /* create acl file */
        std::string content;
        write_file(path, std::format("r {}\n", expand("racl", DM_VAR)));
        write_file(path, std::format("w {}\n", expand("wacl", DM_VAR)));
        write_file(path, std::format("c {}\n", expand("cacl", DM_VAR)));
        write_file(
            path, std::format("a {}\n", (chacl) ? "+f:ulist" : "+sysop"));
        chmod(path.c_str(), 0644);
    }
    /* Restore original conference index */
    confidx = previdx;

    /* Free up space */
    free(cfshort);
    free(cflong);
    free(cfemail);
    free(cfsubdir);
    free(cftype);
    free(cfhosts);

    custom_log("cfcreate", M_OK);

    load_acl(confidx); /* reload acl for current conference */

    return 1;
}
/******************************************************************************/
/* DELETE CURRENT OR OTHER CONFERENCE                                         */
/******************************************************************************/
int
cfdelete(       /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    char *cfshort;
    sumentry_t fr_sum[MAX_ITEMS];
    status_t fr_st;
    partentry_t part2[MAX_ITEMS];
    int idx = confidx; /* the conference index to delete */
    FILE *fp;
    int perm;

    if (!is_sysop(0))
        return 1;

    /* If no conference was specified, prompt for one */
    if (argc > 1)
        cfshort = argv[1];
    else {
        if (!(flags & O_QUIET)) {
            std::print("Short name (including underlines): ");
            std::fflush(stdout);
        }
        cfshort = xgets(st_glob.inp, 0);
    }

    idx = get_idx(cfshort, conflist);
    if (argc < 2)
        free(cfshort);
    if (idx < 0) {
        std::println("Cannot access {} {}.", conference(), cfshort);
        return 1;
    }

    /* Verify that conference has no active items */
    get_status(&fr_st, fr_sum, part2, idx);
    if (fr_st.i_first <= fr_st.i_last) {
        std::println("{} is not empty.", conference(1));
        return 1;
    }

    /* Leave conference if we're in it now */
    if (confidx >= 0 && confidx == idx) {
        if (!(flags & O_QUIET))
            std::println("Leaving {}...", conference());
        leave(0, (char **)0);
    }

    /* Remove all maillist entries */
    if (fr_st.c_security & CT_EMAIL) {
        const auto maillist = grab_list(bbsdir, "maillist", 0);
        if (!maillist.empty()) {
            const auto path = str::concat({bbsdir, "/maillist"});
            FILE *fp = mopen(path, O_W);
            if (fp != NULL) {
                std::println(fp, "!<hl01>");
                std::println(fp, "{}", maillist[0].location);
                for (auto i = 1z; i < maillist.size(); i++)
                    if (!match(maillist[i].location, conflist[idx].name))
                        std::println(fp, "{}:{}", maillist[i].name,
                            maillist[i].location);
                mclose(fp);
            }
        }
    }

    /* If we're using cfadm-owned participation files, delete them */
    if ((perm = partfile_perm()) == SL_OWNER) {
        if (!(flags & O_QUIET))
            std::println("Removing members' participation files...");
        const auto config = get_config(idx);
        if (config.size() > CF_PARTFILE) {
            const auto ulist =
                grab_recursive_list(conflist[idx].location, "ulist");
            for (const auto &user : ulist) {
                const auto path = get_partdir(user);
                const auto partfile =
                    str::join("/", {path, config[CF_PARTFILE]});
                rm(partfile, SL_OWNER);
                const auto ucflist =
                    grab_file(path, ".cflist", GF_WORD | GF_SILENT | GF_IGNCMT);
                if (ucflist.empty())
                    continue;
                const auto cflistfile = str::concat({path, "/.cflist"});
                auto newfp = mopen(cflistfile, O_W);
                if (newfp == NULL)
                    continue;
                for (const auto &ucf : cflist) {
                    auto k = get_idx(ucf.c_str(), conflist);
                    if (!str::eq(conflist[k].location, conflist[idx].location))
                        std::println(newfp, "{}", ucf);
                }
                mclose(newfp);
            }
        }
    }

    /* Remove conflist entries */
    if (!(flags & O_QUIET))
        std::println("Removing conflist entries...");
    const auto path = str::concat({bbsdir, "/conflist"});
    if ((fp = mopen(path, O_W)) != NULL) {
        std::println(fp, "!<hl01>");
        std::println(fp, "{}", conflist[0].location);
        const auto max = conflist.size();
        for (auto i = 1; i < max; i++)
            if (!str::eq(conflist[i].location, conflist[idx].location))
                std::println(
                    fp, "{}:{}", conflist[i].name, conflist[i].location);
        mclose(fp);
    }

    /* Remove desclist entry */
    if (!(flags & O_QUIET))
        std::println("Removing desclist entry...");
    const auto descpath = str::concat({bbsdir, "/desclist"});
    if ((fp = mopen(descpath, O_W)) != NULL) {
        std::println(fp, "!<hl01>");
        std::println(fp, "{}", desclist[0].location);
        for (auto i = 1; i < desclist.size(); i++)
            if (!match(desclist[i].name, conflist[idx].name))
                std::println(
                    fp, "{}:{}", desclist[i].name, desclist[i].location);
        mclose(fp);
    }

    /* Delete the whole subdirectory */
    if (!(flags & O_QUIET))
        std::println("Removing directory...");
    const auto cmd = std::format("rm -rf {}", conflist[idx].location);
    system(cmd.c_str());

    reload_conflist();
    custom_log("cfdelete", M_OK);

    return 1;
}

/* ARGUMENTS: */
/* Conference security type */
/* Configuration file for conference */
/* Conference of addresses to update */
void
upd_maillist(int security, const std::vector<std::string> &config, int idx)
{
    if ((security & CT_EMAIL) == 0)
        return;
    const auto maillist = grab_list(bbsdir, "maillist", 0);
    std::vector<std::string_view> addr;
    if (config.size() > CF_EMAIL)
        addr = str::split(config[CF_EMAIL], " ,");
    /* Output the contents of the maillist array */
    const auto path = str::concat({bbsdir, "/maillist"});
    FILE *fp = mopen(path, O_W);
    if (fp == NULL)
        return;
    std::println(fp, "!<hl01>");
    std::println(fp, "{}", maillist[0].location);
    const auto conf_name = compress(conflist[idx].name);
    auto j = 0z;
    for (const auto &entry : maillist) {
        if (str::eq(conf_name, entry.location)) {
            /* Output the current address for the conference */
            std::println(fp, "{}:{}", addr[j++], conf_name);
            continue;
        }
        std::println(fp, "{}:{}", entry.name, entry.location);
    }
    /* Add any additional addresses */
    for (; j < addr.size(); j++) std::println(fp, "{}:{}", addr[j], conf_name);
    mclose(fp);
}
