// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

/*
 * PHASE 1: Conference Subsystem
 *        Be able to enter/exit the program, and move between conferences
 *        Commands: join, next, quit, source
 *        Files: rc files, cflist, login, logout, bull
 */

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <cstdio>
#include <iostream>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include "conf.h"
#include "driver.h"
#include "files.h"
#include "globals.h"
#include "joq.h"
#include "lib.h"
#include "license.h"
#include "macro.h"
#include "main.h"
#include "options.h"
#include "security.h"
#include "sep.h"
#include "stats.h"
#include "sum.h"
#include "str.h"
#include "struct.h"
#include "sysop.h"
#include "system.h"
#include "user.h"
#include "yapp.h"

const char *cfiles[] = {"logi_n", "logo_ut", "in_dex", "b_ull",
    "we_lcome", "html_header", "rc", "sec_ret", "ul_ist",
    "origin_list", "obs_ervers", "acl", "rc.www", ""};

//*****************************************************************************
// CHANGE SYSTEM PARAMETERS
//*****************************************************************************
// ARGUMENTS:
// Number of arguments
// Argument list
int
change(int argc, char **argv)
{
	std::string buff;
	bool done = false;
	short i, j;

	if (argc < 2) {
		std::println("Change what?");
		return 1;
	}
	for (j = 1; j < argc; j++) {
		/* Security measure */
		if ((orig_stdin[stdin_stack_top].type & STD_SANE) &&
		    match(argv[j], "noverbose"))
			continue;

		/* Process changing flags */
		for (const auto &opt : option) {
			if (match(argv[j], opt.name)) {
				flags |= opt.mask;
				done = true;
				break;
			} else if (strncmp(argv[j], "no", 2) == 0 &&
			           match(argv[j] + 2, opt.name)) {
				flags &= ~opt.mask;
				done = true;
				break;
			}
		}
		if (done)
			continue;

		if (match(argv[j], "n_ame") || match(argv[j], "full_name") ||
		    match(argv[j], "u_ser")) {
			std::println("Your old name is: {}", st_glob.fullname);
			if (!(flags & O_QUIET)) {
				std::print("Enter replacement or return to keep old? ");
				std::fflush(stdout);
			}
			if (ngets(buff, st_glob.inp) && !buff.empty()) {
				/* Expand seps in name IF first character is %
				 */
				if (buff[0] == '%') {
					const char *str, *f;
					str = buff.c_str() + 1;
					f = get_sep(&str);
					buff = f;
				}
				if (sane_fullname(buff))
					st_glob.fullname = buff;
			} else
				std::println("Name not changed.");
		} else if (match(argv[j], "p_assword") ||
		           match(argv[j], "passwd")) {
			passwd(0, NULL);
		} else if (match(argv[j], "li_st")) {
			if (partfile_perm() == SL_USER)
				edit(partdir, ".cflist", 0);
			else
				priv_edit(partdir, ".cflist",
				    2); /* 2=force install */
			refresh_list();
		} else if (match(argv[j], "cfonce")) {
			edit(work, ".cfonce", 0);
		} else if (match(argv[j], "cfrc")) {
			edit(work, ".cfrc", 0);
		} else if (match(argv[j], "cfjoin")) {
			edit(work, ".cfjoin", 0);
		} else if (match(argv[j], "cgirc") ||
		           match(argv[j], "illegal") ||
		           match(argv[j], "matched")) {
			struct stat st;
			const auto defpath =
			    str::concat({get_conf_param("bbsdir", BBSDIR), "/www"});
			const auto wwwdir = get_conf_param("wwwdir", defpath);
			if (!is_sysop(0))
				return 1;
			std::string file;
			if (argv[j][0] == 'c')
				file = str::concat({wwwdir, "/rc.", expand("cgidir", DM_VAR)});
			else
				file = str::join("/", {wwwdir, argv[j]});

			/* Assert existence of file */
			if (stat(file.c_str(), &st)) {
				FILE *fp = fopen(file.c_str(), "w");
				if (fp == NULL) {
					error("creating ", file);
					return 1;
				}
				fclose(fp);
			}
			priv_edit(file, "", 0);
			return 1;
		} else if (match(argv[j], "ig_noreeof")) {
			unix_cmd("/bin/stty eof ^-");
		} else if (match(argv[j], "noig_noreeof")) {
			unix_cmd("/bin/stty eof ^D");
		} else if (match(argv[j], "ch_at")) {
			unix_cmd("mesg y");
		} else if (match(argv[j], "noch_at")) {
			unix_cmd("mesg n");
		} else if (match(argv[j], "resign")) {
			command("resign", 0);
		} else if (match(argv[j], "sa_ne")) {
			undefine(DM_SANE);
		} else if (match(argv[j], "supers_ane")) {
			undefine(DM_SANE | DM_SUPERSANE);
		} else if (match(argv[j], "save_seen")) {
			if (confidx >= 0) {
				const auto config = get_config(confidx);
				if (config.size() > CF_PARTFILE)
					write_part(config[CF_PARTFILE]);
			}
		} else if (match(argv[j], "sum_mary")) {
			if (!(st_glob.c_status & CS_FW)) {
				wputs("Sorry, you can't do that!\n");
				return 1;
			}
			std::println("Regenerating summary file; please wait");
			refresh_sum(0, confidx, sum, part, &st_glob);
			save_sum(sum, (short)-1, confidx, &st_glob);
#ifdef SUBJFILE
			rewrite_subj(confidx); /* re-write subjects file */
#endif
		} else if (match(argv[j], "nosum_mary")) {
			if (!(st_glob.c_status & CS_FW)) {
				wputs("Sorry, you can't do that!\n");
				return 1;
			}
			const auto path = str::concat({conflist[confidx].location, "/sum"});
			rm(path, SL_OWNER);
#ifdef SUBJFILE
			const auto subjpath = str::concat({conflist[confidx].location, "/subjects"});
			rm(subjpath, SL_OWNER);
			clear_subj(confidx); /* re-write subjects file */
#endif
		} else if (match(argv[j], "rel_oad")) {
			if (confidx >= 0) {
				const auto config = get_config(confidx);
				if (config.size() > CF_PARTFILE)
					read_part(config[CF_PARTFILE], part, &st_glob, confidx);
			}
			st_glob.sumtime = 0;
		} else if (match(argv[j], "config")) {
			if (!is_sysop(0))
				return 1;
			if (confidx >= 0) {
				priv_edit(
				    conflist[confidx].location, "config", 0);
				clear_cache();
				const auto config = get_config(confidx);
				if (!config.empty()) {
					st_glob.c_security = security_type(config, confidx);
					upd_maillist(st_glob.c_security, config, confidx);
				}
			}
			return 1;
		} else {
			for (i = 0; cfiles[i][0]; i++) {
				if (match(argv[j], cfiles[i])) {
					if (i == 11) {
						if (!check_acl(CHACL_RIGHT, confidx)) {
							if ((flags & O_QUIET) == 0)
								std::println("Permission denied.");
							return 1;
						}
					} else {
						if (!(st_glob.c_status &
						        CS_FW)) {
							std::println("You aren't a {}.",
							    fairwitness());
							return 1;
						}
					}
					priv_edit(conflist[confidx].location, compress(cfiles[i]), 0);
					const auto path = str::join("/",
					    {conflist[confidx].location, compress(cfiles[i])});
					chmod(path.c_str(),
					    (st_glob.c_security & CT_BASIC)
					        ? 0600
					        : 0644);

					/* Re-initialize acl when doing "change
					 * acl" */
					if (i == 11)
						reinit_acl();
					return 1;
				}
			}
			std::println("Bad parameters near \"{}\"", argv[j]);
			return 2;
		}
	}
	return 1;
}
/******************************************************************************/
/* DISPLAY SYSTEM PARAMETERS                                                  */
/******************************************************************************/
/* ARGUMENTS:             */
/* Number of arguments */
/* Argument list       */
int
display(int argc, char **argv)
{
	time_t t;
	const char *noconferr = "Not in a conference!\n";
	short i, j;
	bool done = false;

	for (j = 1; j < argc; j++) {
		/* Display flag settings */
		for (const auto &opt : option) {
			if (match(argv[j], opt.name)) {
				std::println("{} flag is {}",
				    compress(opt.name),
				    (flags & opt.mask) ? "on" : "off");
				done = true;
				break;
			}
		}
		if (match(argv[j], "ma_iltext")) {
			refresh_stats(sum, part, &st_glob);
			check_mail(1);
		}
		if (done)
			continue;

		if (match(argv[j], "fl_ags")) {
			for (const auto &opt : option) {
				std::print("{:<10} : {}{}",
				    compress(opt.name),
				    (flags & opt.mask) ? "ON " : "off",
				    (i % 4 == 3) ? "\n" : "    ");
			}
			if (i % 4)
				std::println("");
		} else if (match(argv[j], "c_onference")) {
			refresh_stats(sum, part, &st_glob);
			sepinit(IS_START);
			confsep(expand("confmsg", DM_VAR), confidx, &st_glob,
			    part, 0);
		} else if (match(argv[j], "conferences")) {
			command("list", 0);
		} else if (match(argv[j], "d_ate") || match(argv[j], "t_ime")) {
			time(&t);
			std::print("Time is {}", ctime(&t));
		} else if (match(argv[j], "def_initions") ||
		           match(argv[j], "ma_cros")) {
			command("define", 0);
		} else if (match(argv[j], "v_ersion")) {
			extern const char *regto;
			std::println("YAPP {}  Copyright (c)1995 Armidale Software", VERSION);
			std::println("{}", regto);
		} else if (match(argv[j], "ret_ired")) {
			int c = 0;
			refresh_sum(0, confidx, sum, part, &st_glob);
			std::print("{}s retired:", Topic());
			for (i = st_glob.i_first; i <= st_glob.i_last; i++) {
				if (sum[i - 1].flags & IF_RETIRED) {
					if (!c)
						std::println("");
					std::print("{:4}", i);
					c++;
				}
			}
			if (c)
				std::println("Total: {} {}s retired.\n", c, topic());
			else
				std::println(" <none>");
		} else if (match(argv[j], "fro_zen")) {
			int c = 0;
			refresh_sum(0, confidx, sum, part, &st_glob);
			std::print("{}s frozen:", Topic());
			for (i = st_glob.i_first; i <= st_glob.i_last; i++) {
				if (sum[i - 1].flags & IF_FROZEN) {
					if (!c)
						std::println("");
					std::println("{:4}", i);
					c++;
				}
			}
			if (c)
				std::println("\nTotal: {} {}s frozen.", c, topic());
			else
				std::println(" <none>");
		} else if (match(argv[j], "f_orgotten")) {
			int c = 0;
			std::print("{}s forgotten:", Topic());
			for (i = st_glob.i_first; i <= st_glob.i_last; i++) {
				if (part[i - 1].nr < 0) {
					if (!c)
						std::println("");
					std::println("{:4}", i);
					c++;
				}
			}
			if (c)
				std::println("\nTotal: {} {}s forgotten.", c, topic());
			else
				std::println(" <none>");
		} else if (match(argv[j], "sup_eruser")) {
			std::println("fw superuser {}",
			    (st_glob.c_status & CS_FW) ? "yes" : "no");
		} else if (match(argv[j], "fw_slist") ||
		           match(argv[j], "fair_witnesslist") ||
		           match(argv[j], "fair_witnesses")) {
			if (confidx < 0)
				wputs(noconferr);
			else {
				const auto config = get_config(confidx);
				if (config.size() > CF_FWLIST)
					std::println("fair witnesses: {}", config[CF_FWLIST]);
			}
		} else if (match(argv[j], "i_ndex") ||
		           match(argv[j], "ag_enda")) {
			sepinit(IS_START);
			confsep(expand("indxmsg", DM_VAR), confidx, &st_glob,
			    part, 0);
		} else if (match(argv[j], "li_st")) {
			show_cflist();
		} else if (match(argv[j], "b_ulletin")) {
			sepinit(IS_START);
			confsep(expand("bullmsg", DM_VAR), confidx, &st_glob,
			    part, 0);
		} else if (match(argv[j], "w_elcome")) {
			sepinit(IS_START);
			confsep(expand("wellmsg", DM_VAR), confidx, &st_glob,
			    part, 0);
		} else if (match(argv[j], "logi_n")) {
			sepinit(IS_START);
			confsep(expand("linmsg", DM_VAR), confidx, &st_glob,
			    part, 0);
		} else if (match(argv[j], "logo_ut")) {
			sepinit(IS_START);
			confsep(expand("loutmsg", DM_VAR), confidx, &st_glob,
			    part, 0);
		} else if (match(argv[j], "cfjoin")) {
			more(work, ".cfjoin");
		} else if (match(argv[j], "cfonce")) {
			more(work, ".cfonce");
		} else if (match(argv[j], "cfrc")) {
			more(work, ".cfrc");
#ifdef WWW
		} else if (match(argv[j], "origin_list")) {
			if (confidx < 0)
				wputs(noconferr);
			else
				more(conflist[confidx].location, "originlist");
#endif
		} else if (match(argv[j], "ul_ist")) {
			if (confidx < 0)
				wputs(noconferr);
			else
				more(conflist[confidx].location, "ulist");
		} else if (match(argv[j], "obs_ervers")) {
			if (confidx < 0)
				wputs(noconferr);
			else
				more(conflist[confidx].location, "observers");
		} else if (match(argv[j], "acl")) {
			if (confidx < 0)
				wputs(noconferr);
			else
				more(conflist[confidx].location, "acl");
		} else if (match(argv[j], "html_header")) {
			if (confidx < 0)
				wputs(noconferr);
			else
				more(conflist[confidx].location, "htmlheader");
		} else if (match(argv[j], "rc")) {
			if (confidx < 0)
				wputs(noconferr);
			else
				more(conflist[confidx].location, "rc");
		} else if (match(argv[j], "wwwrc") ||
		           match(argv[j], "rc.www")) {
			/* conference specific WWW modified rc file */
			if (confidx < 0)
				wputs(noconferr);
			else
				more(conflist[confidx].location, "rc.www");
		} else if (match(argv[j], "log_messages")) {
			if (confidx < 0)
				wputs(noconferr);
			else {
				std::println("login message:");
				more(conflist[confidx].location, "login");
				std::println("logout message:");
				more(conflist[confidx].location, "logout");
			}
		} else if (match(argv[j], "n_ew")) {
			refresh_sum(0, confidx, sum, part, &st_glob);
			sepinit(IS_ITEM);
			open_pipe();
			confsep(expand("linmsg", DM_VAR), confidx, &st_glob,
			    part, 0);
			check_mail(1);
		} else if (match(argv[j], "n_ame") || match(argv[j], "u_ser"))
			std::println("User: {}", fullname_in_conference(&st_glob));
		else if (match(argv[j], "p_articipants")) {
			participants(0, (char **)0);
		} else if (match(argv[j], "s_een")) {
			int c = 0;
			FILE *fp;
			refresh_sum(0, confidx, sum, part, &st_glob);

			/* Display seen item status */
			open_pipe();
			if (status & S_PAGER)
				fp = st_glob.outp;
			else
				fp = stdout;
			std::println(fp, "{} se re fl   lastseen             etime                mtime", topic());
			std::println(fp, "");
			for (i = st_glob.i_first; i <= st_glob.i_last; i++) {
				if (!part[i - 1].nr)
					continue;
				std::print(fp, "{:4} {:2} {:2} {:2X}   {} ", i,
				    abs(part[i - 1].nr), sum[i - 1].nr,
				    sum[i - 1].flags,
				    get_date(part[i - 1].last, 0).c_str() + 4);
				std::print(fp, "{} ", get_date(sum[i - 1].first, 0).c_str() + 4);
				std::println(fp, "{}", get_date(sum[i - 1].last, 0) .c_str()+ 4);
				c++;
			}
			std::println(fp, "total {} {}s in seen map", c, topic());
			std::println(fp, "");
		} else if (match(argv[j], "s_ize"))
			std::println("No longer supported");
		else if (match(argv[j], "strings"))
			std::println("No longer supported");
		else if (match(argv[j], "fds"))
			mdump();
		else if (match(argv[j], "w_hoison"))
			unix_cmd("who");
		else if (const auto var = expand(argv[j], ~0); !var.empty())
			std::println("{} = {}", argv[j], var);
		else {
			std::println("Bad parameters near \"{}\"", argv[j]);
			return 2;
		}
	}
	return 1;
}
