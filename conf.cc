// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include "conf.h"

#include <arpa/inet.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <cassert>
#include <cctype>
#include <format>
#include <iostream>
#include <print>
#include <string>
#include <vector>

#include "driver.h"
#include "files.h"
#include "globals.h"
#include "joq.h"
#include "lib.h"
#include "log.h"
#include "macro.h"
#include "main.h"
#include "range.h"
#include "security.h"
#include "sep.h"
#include "stats.h"
#include "str.h"
#include "struct.h"
#include "sum.h"
#include "sysop.h"
#include "system.h"
#include "user.h"
#include "yapp.h"

FILE *conffp;

unsigned int
security_type(const std::vector<std::string> &config, int idx)
{
	unsigned int sec = 0;
	int i, n;

	if (config.size() <= CF_SECURITY)
		return 0;
	const auto fields = str::split(config[CF_SECURITY], ", ");
	n = fields.size();
	for (i = 0; i < n; i++) {
		if (isdigit(fields[i][0]))
			sec |= str::toi(config[CF_SECURITY]);
		else if (match(fields[i], "pass_word"))
			sec = (sec & ~CT_BASIC) | CT_PASSWORD;
		else if (match(fields[i], "ulist"))
			sec = (sec & ~CT_BASIC) | CT_PRESELECT;
		else if (match(fields[i], "pub_lic"))
			sec = (sec & ~CT_BASIC) | CT_OLDPUBLIC;
		else if (match(fields[i], "prot_ected"))
			sec = (sec & ~CT_BASIC) | CT_PUBLIC;
		else if (match(fields[i], "read_only"))
			sec |= CT_READONLY;
#ifdef NEWS
		else if (match(fields[i], "news_group"))
			sec |= CT_NEWS;
#endif
		else if (match(fields[i], "mail_list"))
			sec |= CT_EMAIL;
		else if (match(fields[i], "reg_istered"))
			sec |= CT_REGISTERED;
		else if (match(fields[i], "noenter"))
			sec |= CT_NOENTER;
	}

	if ((sec & CT_EMAIL) && (config.size() <= CF_EMAIL || config[CF_EMAIL].empty())) {
		sec &= ~CT_EMAIL;
		error("email address for ", compress(conflist[idx].name));
	}
#ifdef NEWS
	if (config.size() <= CF_NEWSGROUP)
		sec &= ~CT_NEWS;
#endif

#ifdef WWW
	{ /* If we find an originfile, then turn on originlist flag */
		struct stat st;
		const auto originfile = conflist[idx].location + "/originlist";
		/* was access(), but that uses uid not euid */
		if (!stat(originfile.c_str(), &st))
			sec |= CT_ORIGINLIST;
	}
#endif
	return sec;
}

bool
is_fairwitness(int idx) /* conference index */
{
	const auto config = get_config(idx);
	if (config.size() <= CF_FWLIST)
		return false;
	const auto fw = str::split(config[CF_FWLIST], ", ");
	return str::contains(fw, login) || str::contains(fw, std::to_string(uid)) || is_sysop(1);
}

std::vector<std::string>
grab_recursive_list(const std::string &dir, const std::string &filename)
{
	constexpr size_t MAX = 1000000;
	auto v = grab_file(dir, filename, GF_SILENT | GF_WORD | GF_IGNCMT);
	if (v.empty())
		return v;
	size_t nl = v.size();
	for (size_t i = 0; nl < MAX && i < nl; i++) {
		if (!v[i].starts_with("/"))
			continue;
		// If it's another file, replace element with first element
		// in list from file, and append the rest of the file's list
		// to the end of the current list.  Note that we decrement
		// the current index, so that the element we just put into
		// this position will be considered on the next iteration.
		const auto b = grab_file(v[i], "", GF_SILENT | GF_WORD | GF_IGNCMT);
		if (b.empty()) {
			// Replace file element with last in list.
			v[i--] = v[--nl];
			v.pop_back();
			continue;
		}
		// Otherwise, set the current element to the first element
		// of the new file, and append the rest of its contents, an
		// additional b.size() - 1 lines, o the list and adjust the
		// total size accordingly.  Note that we decrement the index
		// so that the newly placed element at this position will be
		// inspected on the next iteration of the loop.
		v[i--] = b[0];
		v.insert(v.end(), b.begin() + 1, b.end());
		nl += b.size() - 1;
	}

	return v;
}

// ARGUMENTS:
// conference index
// filename to check
bool
is_inlistfile(int idx, const std::string &file)
{
	const auto lines{
	    file[0] == '/' ?
		grab_recursive_list(file, "") :
		grab_recursive_list(conflist[idx].location, file)
	};
	const auto uidstr = std::to_string(uid);
	return str::contains(lines, login) || str::contains(lines, uidstr);
}

std::string_view
fullname_in_conference(status_t *stt)
{
	if (!stt->fullname[0] || str::eq(stt->fullname, DEFAULT_PSEUDO))
		return fullname;
	return stt->fullname;
}
/******************************************************************************/
/* PROCESS COMMAND LINE ARGUMENTS                                             */
/******************************************************************************
Function:    join(char *conference, short idx, int force)
Called by:   command, main
Arguments:   cf name or conference # to join, force flag
Returns:
Calls:       source() for CONF/rc and WORK/.cfrc files
             cat() for login
             get_idx to find cf name in conflist
Description:
*******************************************************************************/
char
join(const std::string &conf, int force, int secure)
{
	char buff[MAX_LINE_LENGTH];
	struct stat st;
	time_t t1;
	int cp;

	/* Initialize st_new structure */
	st_new.outp = st_glob.outp;
	st_new.inp = st_glob.inp;
	st_new.mailsize = st_glob.mailsize;
	st_new.c_security = st_new.i_current = st_new.c_status = 0;
	st_new.sumtime = 0;
#ifdef NEWS
	st_new.c_article = 0;
#endif

	/* Reset name */
	st_new.fullname = fullname;

	/* Check for existence */
	if (conf.empty())
		return 0;
	joinidx = get_idx(conf, conflist);
	if (joinidx < 0) {
		if (!(flags & O_QUIET))
			std::println("Cannot access {} {}.", conference(), conf);
		return 0;
	}
	if (debug & DB_CONF)
		std::println("join: {} dir={}", joinidx, conflist[joinidx].location);

	/* Read in config file */
	const auto config = get_config(joinidx);
	if (config.empty())
		return 0;

	/* Pass security checks */
	st_new.c_security = security_type(config, joinidx);

	if (secure)
		cp = secure;
	else {
		cp = check_acl(JOIN_RIGHT, joinidx);
		if (!cp)
			if (!(flags & O_QUIET)) {
				std::cout << "You are not allowed to access the " <<
				    compress(conflist[joinidx].name) << " " <<
				    conference() << "." << std::endl;
			}
	}

	if (!cp) {
		if (st_new.c_security & CT_READONLY)
			force |= O_READONLY;
		else {
			return 0;
		}
	}

	/* Force READONLY if login is in observer file */
	if (is_inlistfile(joinidx, "observers"))
		force |= O_READONLY;

	/* Do stuff with PARTDIR/.name.cf */
	const auto partfile = str::join("/", {partdir, config[CF_PARTFILE]});
	if (debug & DB_CONF)
		std::println("join: Partfile={}", partfile);
	if (stat(partfile.c_str(), &st)) { /* Not a member */
		if (!((flags | force) & O_OBSERVE)) {
			/* Main JOQ cmd loop */
			mode = M_JOQ;
			if ((flags | force) & O_AUTOJOIN) {
				if (!(flags & O_QUIET)) {
					const auto msg = std::format(
					    "You are being automatically registered in {}\n",
					    conflist[joinidx].location);
					wputs(buff);
				}
				command("join", 0);
			} else {
				if (!(flags & O_QUIET)) {
					std::println("You are not a member of {}",
					    conflist[joinidx].location);
					std::print("Do you wish to:");
				}
				while (mode == M_JOQ && get_command("", 0))
					;
			}

			if (status & S_QUIT) {
				std::println("registration aborted (didn't leave)");
				status &= ~S_QUIT;
				return 0;
			}
		}
		t1 = (time_t)0;
	} else {
		t1 = st.st_mtime; /* last time .*.cf was touched */
	}
	if (debug & DB_CONF)
		std::println("join: t1={:x}", t1);

	if (confidx >= 0)
		leave(0, (char **)0);

	/* was ifdef STUPID_REOPEN */
	{
		FILE *inp = st_glob.inp;
		memcpy(&st_glob, &st_new, sizeof(st_new));
		st_glob.inp = inp;
	}

	confidx = joinidx; /* passed all security checks */
	// Lock the config file.
	const auto configfile = conflist[confidx].location + "/config";
	conffp = mopen(configfile, O_R);
	read_part(config[CF_PARTFILE], part, &st_glob, confidx);

	/* Set status */
	if ((flags | force) & O_OBSERVE)
		st_glob.c_status |= CS_OBSERVER;
	if ((flags | force) & O_READONLY)
		st_glob.c_status |= CS_NORESPONSE;

	/* Allow FW to be specified by login or by UID */
	if (is_fairwitness(confidx))
		st_glob.c_status |= CS_FW;
	if (debug & DB_CONF)
		std::println("join: Status={}", status);

	st_glob.sumtime = 0;
	refresh_sum(0, confidx, sum, part, &st_glob);

	/* Source CONF/rc file and WORK/.cfrc files */
	if (flags & O_SOURCE) {
		source(conflist[confidx].location, "rc", STD_SANE, SL_OWNER);
	}

	/* Display login file */
	if (!(flags & O_QUIET)) {
		sepinit(IS_START | IS_ITEM);
		confsep(expand("linmsg", DM_VAR), confidx, &st_glob, part, 0);
		check_mail(1);
	}
	custom_log("join", M_OK);

	/* Source WORK/.cfrc files */
	if (flags & O_SOURCE) {
		mode = M_OK;
		if (st_glob.c_status & CS_JUSTJOINED) {
			source(bbsdir, ".cfjoin", 0, SL_OWNER);
			source(work, ".cfjoin", 0, SL_USER);
		}
		source(work, ".cfrc", 0, SL_USER);
	}
	return 1;
}
/******************************************************************************/
/* PROCESS COMMAND LINE ARGUMENTS                                             */
/******************************************************************************
Function:    leave
Called by:   command
Arguments:
Returns:
Calls:       cat() for logout
Description:
*******************************************************************************/
int
leave(/* ARGUMENTS: (none)                   */
    int argc, char **argv)
{
	if (debug & DB_CONF)
		std::println("leave: {}", confidx);
	if (confidx < 0)
		return 1; /* (noconf) */

	if (!argc || argv[0][0] != 'a') { /* not "abort" */
		/* Display logout */
		if (!(flags & O_QUIET)) {
			sepinit(IS_START | IS_ITEM);
			confsep(expand("loutmsg", DM_VAR), confidx, &st_glob,
			    part, 0);
		}
		custom_log("leave", M_OK);

		/* Write participation file unless observing */
		if (!(st_glob.c_status & CS_OBSERVER)) {
			const auto config = get_config(confidx);
			if (config.size() <= CF_PARTFILE)
				return 1;
			write_part(config[CF_PARTFILE]);
		}
	}
	st_glob.sumtime = 0;
	st_glob.c_status = 0;

	confidx = -1;
	mclose(conffp);
	undefine(DM_SANE);

	/* Re-source system rc file */
	source(bbsdir, "rc", STD_SUPERSANE, SL_OWNER);

	return (!argc || argv[0][0] != 'a');
}
/******************************************************************************/
/* CHECK A LIST OF CONFERENCES FOR NEW ITEMS                                  */
/******************************************************************************/
int
check(int argc, char **argv)
{
	unsigned int sec;
	bool all = false;
	size_t count = 0;
	size_t argidx = 1;
	partentry_t part2[MAX_ITEMS], *part3;
	sumentry_t sum2[MAX_ITEMS];
	status_t *st, st_temp;
	struct stat stt;
	long force;

	std::vector<std::string> args;
	for (int i = 0; i < argc; i++)
		args.push_back(argv[i]);

#ifdef INCLUDE_EXTRA_COMMANDS
	/* Simple cfname dump for WWW */
	if (flags & O_CGIBIN) {
		sepinit(IS_START);
		for (auto idx = 1uz; idx < conflist.size(); idx++) {
			sepinit(IS_ITEM);
			if (idx == conflist.size() - 1)
				sepinit(IS_CFIDX);
			confsep(expand("listmsg", DM_VAR), idx, &st_glob, part, 0);
		}
		return 1;
	}
#endif
	auto list = args.begin();
	size_t size = 0;

	/* Check for before/since dates */
	st_glob.since = st_glob.before = 0;
	if (args.size() > argidx) {
		if (match(args[argidx], "si_nce") ||
		    match(args[argidx], "S=")) {
			st_glob.since = since(args, &argidx);
			argidx++;
		} else if (match(args[argidx], "before") ||
		           match(args[argidx], "B=")) {
			st_glob.before = since(args, &argidx);
			argidx++;
		}
	}
	if (args.size() > argidx) { /* list given by user */
		size = args.size() - argidx;
		list += argidx;
	} else if (args[0][0] == 'l') { /* use conflist */
		all = 1;
		size = conflist.size() - 1;
	} else { /* use .cflist */
		refresh_list();
		size = cflist.size();
		list = cflist.begin();
	}

	sepinit(IS_START);
	for (auto i = 0uz; i < size && !(status & S_INT); i++) {
		force = 0;
		auto idx = (all) ? i + 1 : get_idx(list[i], conflist);
		const auto cfname = (all) ? compress(conflist[idx].name) : list[i];
		if (idx == ~0uz) {
			std::println("Cannot access {} {}.", conference(), cfname);
			continue;
		}
		const auto config = get_config(idx);
		if (config.empty()) {
			std::println("Cannot access {} {}.", conference(), cfname);
			continue;
		}

		/* Pass security checks */
		if (!check_acl(JOIN_RIGHT, idx))
			continue;
		if (!check_acl(RESPOND_RIGHT, idx))
			force |= O_READONLY;

		sec = security_type(config, idx);

		if (confidx >= 0 &&
		    str::eq(conflist[idx].location, conflist[confidx].location))
		{
			refresh_sum(0, confidx, sum, part, &st_glob);
			st = &st_glob;
			part3 = part;
		} else {
			st = &st_temp;
#ifdef NEWS
			st->c_article = 0;
#endif
			/* Read in partfile */
			read_part(config[CF_PARTFILE], part2, st, idx);

			/* Initialize c_status */
			st->c_status = 0;
			if (is_fairwitness(idx))
				st->c_status |= CS_FW;
			else
				st->c_status &= ~CS_FW;
			if ((flags | force) & O_OBSERVE)
				st->c_status |= CS_OBSERVER;
			else
				st->c_status &= ~CS_OBSERVER;
			if ((flags | force) & O_READONLY)
				st->c_status |= CS_NORESPONSE;
			else
				st->c_status &= ~CS_NORESPONSE;

			const auto path = str::join("/", {partdir, config[CF_PARTFILE]});
			st->parttime = (stat(path.c_str(), &stt)) ? 0 : stt.st_mtime;
			st->c_security = security_type(config, idx);

			/* Read in sumfile */
			get_status(st, sum2, part2, idx);
			part3 = part2;
		}
		st->c_security = sec;

		if ((!st_glob.before || st->sumtime < st_glob.before) &&
		    (st_glob.since <= st->sumtime)) {
			st->count = ++count;
			sepinit(IS_ITEM);
			if (args.size() < 2 && current == i)
				sepinit(IS_CFIDX);
			confsep(expand((args[0][0] == 'l') ? "listmsg" : "checkmsg", DM_VAR), idx, st, part3, 0);
		}
	}
	return 1;
}
/******************************************************************************/
/* ADVANCE TO NEXT CONFERENCES WITH NEW ITEMS                                 */
/******************************************************************************/
int
do_next(int argc, char **argv)
{
	/* LOCAL VARIABLES: */
	short idx;
	partentry_t part2[MAX_ITEMS];
	sumentry_t sum2[MAX_ITEMS];
	status_t st;
	struct stat stt;
	int cp = 0;

	if (argc > 1) {
		std::println("Bad parameters near \"{}\"", argv[1]);
		return 2;
	}
	refresh_list(); /* make sure .cflist is current */
	for (; current + 1 < cflist.size() && !(status & S_INT); current++) {
		idx = get_idx(cflist[current + 1], conflist);
		if (idx < 0) {
			std::println("Cannot access {} {}.",
			    conference(), cflist[current + 1]);
			continue;
		}
		const auto config = get_config(idx);
		if (config.empty()) {
			std::println("Cannot access {} {}.",
			    conference(), cflist[current + 1]);
			continue;
		}

		/* Check security */
		cp = check_acl(JOIN_RIGHT, idx);
		if (!cp) {
			if (!(flags & O_QUIET)) {
				std::cout << "You are not allowed to access the " <<
				    compress(conflist[joinidx].name) << " " <<
				    conference() << "." << std::endl;
			}
			continue;
		}

		if (confidx >= 0 &&
		    str::eq(conflist[idx].location, conflist[confidx].location))
		{
			refresh_sum(0, confidx, sum, part, &st_glob);
			if (st_glob.i_newresp || st_glob.i_brandnew) {
				join(cflist[++current], 0, cp);
				return 1;
			}
		} else {
			/* Read in partfile */
			read_part(config[CF_PARTFILE], part2, &st, idx);
			const auto partfile = str::join("/", {partdir, config[CF_PARTFILE]});
			st.parttime = (stat(partfile.c_str(), &stt)) ? 0 : stt.st_mtime;
			st.c_security = security_type(config, idx);
#ifdef NEWS
			st.c_article = 0;
#endif
			get_status(&st, sum2, part2, idx); /* Read in sumfile */
			st.sumtime = 0;
			if (st.i_newresp || st.i_brandnew) {
				join(cflist[++current], 0, cp);
				return 1;
			}
		}
		std::println("No new {}s in {}.", topic(), cflist[current + 1]);
	}
	std::println("No more {}s left.", conference());
	return 2;
}
/******************************************************************************/
/* RESIGN FROM (UNJOIN) THE CURRENT CONFERENCE                                */
/******************************************************************************/
int
resign(         /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
	if (argc > 1) {
		std::println("Bad parameters near \"{}\"\n", argv[1]);
		return 2;
	}
	if (st_glob.c_status & CS_OBSERVER) {
		std::println("But you don't belong to this {}!\n", conference());
		return 1;
	}
	if (get_yes("Are you sure you wish to resign? ", false)) {
		const auto config = get_config(confidx);
		if (config.empty())
			return 1;

		/* Delete participation file */
		const auto path = str::join("/", {partdir, config[CF_PARTFILE]});
		rm(path, partfile_perm());

		/* Remove from .cflist if current is set */
		if (current >= 0)
			del_cflist(cflist[current]);

		/* Become an observer */
		st_glob.c_status |= CS_OBSERVER;
		std::println("You are now just an observer.");

		/* Remove login/uid from ulist file: In this case, we don't
		 * remove it from recursive files, only if it's in the top
		 * level ulist */
		if (is_auto_ulist(confidx)) {
			const auto ulist = grab_file(conflist[confidx].location,
			    "ulist", GF_WORD | GF_SILENT | GF_IGNCMT);
			if (!ulist.empty()) {
				const auto file = conflist[confidx].location + "/ulist";
				const auto tmpfile = file + ".tmp";
				const auto uidstr = std::to_string(uid);
				for (const auto &user: ulist) {
					if (!str::eq(login, user) &&
					    !str::eq(uidstr, user)) {
						// XXX: we ought to do better error recovery...
						if (!write_file(tmpfile, user + "\n"))
							break;
					}
				}
				if (rename(tmpfile.c_str(), file.c_str()))
					error("renaming ", file);
			}
			custom_log("resign", M_OK);
		}
	}
	return 1;
}

#ifdef WWW
/******************************************************************************/
/* SEE IF A HOSTNAME ENDS WITH A SPECIFIED DOMAIN NAME                        */
/******************************************************************************/
static bool
matchhost(      /* ARGUMENTS: */
    const std::string_view &spec, /* Domain name allowed     */
    const std::string_view &host  /* Origin host to validate */
)
{
	const auto tail = host.substr(host.size() - spec.size(), host.size());
	return str::eq(spec, tail);
}
/******************************************************************************/
/* SEE IF AN ORIGIN IP ADDRESS MATCHES A SPECIFIED ADDRESS PREFIX             */
/******************************************************************************/
static bool
matchaddr(         /* ARGUMENTS:                        */
    const std::string &specstr, /* IP prefix allowed              */
    const std::string &addrstr  /* Origin IP addr to validate     */
)
{
	uint32_t subnetaddr, subnetmask, hostaddr;
	int masklen = 0;

	hostaddr = ntohl(inet_addr(addrstr.c_str()));
	const auto field = str::splits(specstr, "/");
	assert(!field.empty());
	const auto num = str::split(field[0], ".");
	if (field.size() > 1)
		masklen = str::toi(field[1]);
	else
		masklen = 8 * num.size();
	subnetmask = 0xFFFFFFFF << (32 - masklen);
	subnetaddr = inet_network(field[0].c_str()) << (32 - 8 * num.size());

	return (hostaddr & subnetmask) == (subnetaddr & subnetmask);
}

/* ARGUMENTS:           */
/* IN: Conference index */
bool
is_validorigin(int idx)
{
	const auto originlist = grab_file(conflist[idx].location,
	    "originlist", GF_WORD | GF_IGNCMT);
	if (originlist.empty())
		return true;
	for (const auto &origin: originlist) {
		if (std::isdigit(origin[0])) { /* Check IP address */
			if (matchaddr(origin.c_str(), expand("remoteaddr", DM_VAR)))
				return true;
		} else { /* Check hostname */
			if (matchhost(origin.c_str(), expand("remotehost", DM_VAR)))
				return true;
		}
	}

	return false;
}
#endif

/* ARGUMENTS: */
/* IN: conference index */
bool
check_password(int idx)
{
	const auto &conf = conflist[idx];
	const auto lines = grab_file(conf.location, "secret", 0);
	if (lines.empty())
		return false;
	const auto &password = lines[0];
	const auto prompt = str::concat({"Password for ", compress(conf.name), ": "});
	bool ok = str::eq(getpass(prompt.c_str()), password);
	if (!ok)
		std::println("Invalid password.");
	return ok;
}


/******************************************************************************/
/* Describe a conference: "describe <conf>"                                   */
/******************************************************************************/
int
describe(       /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
	if (argc != 2) {
		std::println("usage: describe <{}>", conference());
		return 1;
	}
	std::println("{}", get_desc(argv[1]));
	return 1;
}
/******************************************************************************/
/* GET INFORMATION ON CONFERENCE PARTICIPANTS                                 */
/******************************************************************************/
int
participants(   /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
	FILE *fp;
	std::vector<std::string> ulist;
	struct stat st;
	bool all = false;
	bool dump = false;
	bool reset = false;
	auto tuid = uid;
	auto tparttime = st_glob.parttime;
	std::string tlogin(login);
	std::string tfullname(st_glob.fullname);
	std::string twork(work);
	std::string tpartdir(partdir);
	std::string temail(email);

	std::string file, file2;

	/* Save user info */
	st_glob.count = 0;

	if (argc > 1) { /* User list specified */
		ulist.insert(ulist.begin(), argv + 1, argv + argc);
	} else {
		file = conflist[confidx].location + "/ulist";
		ulist = grab_recursive_list(conflist[confidx].location, "ulist");
		if (ulist.empty()) {
			all = true;
			setpwent();
		} else if (is_auto_ulist(confidx)) {
			dump = true;
			file2 = conflist[confidx].location + "/ulist.tmp";
		}
	}

	open_pipe();

	if (status & S_EXECUTE)
		fp = 0;
	else if (status & S_PAGER)
		fp = st_glob.outp;
	else
		fp = stdout;

	/* Process items */
	sepinit(IS_START);
	confsep(expand("partmsg", DM_VAR), confidx, &st_glob, part, 0);

	for (auto j = 0uz; !(status & S_INT) && (all || j < ulist.size()); j++) {
		if (all) {
			struct passwd *pwd;
			if ((pwd = getpwent()) == NULL)
				break;

			uid = pwd->pw_uid;
			login= pwd->pw_name;

			const auto gecos =
			    str::split(pwd->pw_gecos, expand("gecos", DM_VAR), false);
			st_glob.fullname.clear();
			if (!gecos.empty())
				st_glob.fullname = gecos[0];
			home = pwd->pw_dir;
		} else {
			if (j >= ulist.size())
				break;
			if (isdigit(ulist[j][0])) {
				uid = str::toi(ulist[j]); /* search by uid */
				login.clear();
			} else {
				uid = 0;
				login = ulist[j]; /* search by login */
			}
			if (!get_user(&uid, login, st_glob.fullname, home, email)) {
				std::println(" User {} not found", ulist[j]);
				if (dump)
					ulist[j].clear();
				continue;
			}
		}

		const auto config = get_config(confidx);
		if (config.size() <= CF_PARTFILE)
			return 1;

		work = home + "/.cfdir";
		partdir = get_partdir(login);

		auto partfile = str::concat({partdir, "/", config[CF_PARTFILE]});
		if (stat(partfile.c_str(), &st) < 0) {
			work = home;
			partfile = str::concat({partdir, "/", config[CF_PARTFILE]});
			if (stat(partfile.c_str(), &st) < 0) {
				/* someone resigned or deleted a part file */
				if (dump) {
					ulist[j].clear();
					reset = true;
				}
				std::string msg = str::concat({"User ", login, " not a member\n"});
				wfputs(msg, fp);
				continue;
			}
		}
		st_glob.parttime = st.st_mtime;
		st_glob.count++;
		sepinit(IS_ITEM);
		confsep(expand("partmsg", DM_VAR), confidx, &st_glob, part, 0);

		if (all) {
			std::string msg = str::concat({login, "\n"});
			write_file(file, msg);
		}
	}

	sepinit(IS_CFIDX);
	confsep(expand("partmsg", DM_VAR), confidx, &st_glob, part, 0);

	if (reset) { /* reset ulist file */
		for (const auto &u : ulist) {
			if (u.empty())
				continue;
			std::string line = u + "\n";
			if (!write_file(file2, line)) {
				reset = false;
				break;
			}
		}
	}
	if (reset)
		if (rename(file2.c_str(), file.c_str()))
			error("renaming ", file);

	if (all)
		endpwent();

	/* Restore user info */
	uid = tuid;
	login = tlogin;
	if (!get_user(&uid, login, st_glob.fullname, home, email)) {
		error("reading ", "user entry");
		endbbs(2);
	}
	st_glob.fullname = tfullname;
	st_glob.parttime = tparttime;
	work = twork;
	partdir = tpartdir;

	return 1;
}

void
ylog(int idx, const std::string_view &str)
{
	time_t t;
	time(&t);
	std::string timestamp(ctime(&t) + 4);
	timestamp.erase(20);
	const auto msg = std::format("{} {} {}\n", timestamp, login, str);
	const auto file = conflist[idx].location + "/log";
	write_file(file, msg);
}

const char *
nextconf(void)
{
	auto i = 1z;
	for (const auto &cfname: cflist) {
		auto idx = get_idx(cfname, conflist);
		if (idx == confidx)
			return i < cflist.size() ? cflist[i].c_str() : "";
		i++;
	}
	return "";
}

/* find the next conference in conference list with new items in it,
 * wrap to around to the begining of conference list only once
 * to check for new items.  Returns the name of the conference
 */
static const char *
findnewconf(int wrapped)
{
	/* LOCAL VARIABLES:       */
	short idx;
	partentry_t part2[MAX_ITEMS];
	sumentry_t sum2[MAX_ITEMS];
	status_t st;
	struct stat stt;
	int cp = 0;
	size_t nextconference = current + 1;

	refresh_list(); /* make sure .cflist is current */
	for (; nextconference < cflist.size() && !(status & S_INT);
	     nextconference++) {
		idx = get_idx(cflist[nextconference], conflist);
		if (idx == nidx) {
			if ((flags & O_QUIET) == 0)
				std::println("Cannot access {} {}.",
				    conference(), cflist[nextconference]);
			continue;
		}
		const auto config = get_config(idx);
		if (config.empty()) {
			if ((flags & O_QUIET) == 0)
				std::println("Cannot access {} {}.",
				    conference(), cflist[nextconference]);
			continue;
		}
		/* Check security */
		cp = check_acl(JOIN_RIGHT, idx);
		if (!cp) {
			if (!(flags & O_QUIET)) {
				std::println("You are not allowed to access the {} {}.",
				    compress(conflist[joinidx].name), conference());
			}
			continue;
		}
		/* don't segfault if in no conf */
		if (confidx >= 0 &&
		    str::eq(conflist[idx].location, conflist[confidx].location))
		{
			refresh_sum(0, confidx, sum, part, &st_glob);
			if (st_glob.i_newresp || st_glob.i_brandnew) {
				return cflist[nextconference].c_str();
			}
		} else {
			read_part(config[CF_PARTFILE], part2, &st,
			    idx); /* Read in partfile */
			const auto partfile = str::join("/",
			    {partdir, config[CF_PARTFILE]});
			st.parttime = (stat(partfile.c_str(), &stt)) ? 0 : stt.st_mtime;
			st.c_security = security_type(config, idx);
#ifdef NEWS
			st.c_article = 0;
#endif
			get_status(&st, sum2, part2, idx); /* Read in sumfile */
			st.sumtime = 0;
			if (st.i_newresp || st.i_brandnew) {
				return cflist[nextconference].c_str();
			}
		}
		if (!(flags & O_QUIET))
			std::println("No new {}s in {}.", topic(), cflist[nextconference]);
	}
	if (!(flags & O_QUIET)) {
		std::println("No more {}s left.", conference());
	}
	if (!wrapped) /* Found end of list, wrap to begining and
	               * check again */
		return findnewconf(1);

	/* No conference with new items left in the conference list */
	return "";
}

const char *
nextnewconf(void)
{
	return findnewconf(0);
}

const char *
prevconf(void)
{
	auto i = nidx;
	for (const auto &cfname: cflist) {
		auto idx = get_idx(cfname, conflist);
		if (idx == confidx)
			return i < cflist.size() ? cflist[i].c_str() : "";
		i++;
	}
	return "";
}

/* Get short description of a conference based on confidx */
const char *
get_desc(const std::string_view &name)
{
	for (auto i = 0; i < desclist.size(); ++i) {
		if (match(name, desclist[i].name))
			return desclist[i].location.c_str();
	}
	return desclist[0].location.c_str(); /* use the default */
}

/******************************************************************************/
/* CHECK A LIST OF CONFERENCES FOR NEW ITEMS                                  */
/******************************************************************************/
int
show_conf_index(/* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
	unsigned int sec;
	size_t count = 0;
	size_t argidx = 1;
	partentry_t part2[MAX_ITEMS], *part3;
	sumentry_t sum2[MAX_ITEMS];
	status_t *st, st_temp;
	struct stat stt;
	long force = 0;

	std::vector<std::string> args;
	for (int i = 0; i < argc; i++)
		args.push_back(argv[i]);

	/* Check for before/since dates */
	st_glob.since = st_glob.before = 0;
	if (argc > argidx) {
		if (match(argv[argidx], "si_nce") ||
		    match(argv[argidx], "S=")) {
			st_glob.since = since(args, &argidx);
			argidx++;
		} else if (match(argv[argidx], "before") ||
		           match(argv[argidx], "B=")) {
			st_glob.before = since(args, &argidx);
			argidx++;
		}
	}
	sepinit(IS_START);
	for (auto i = 1uz; i < desclist.size() && (status & S_INT) == 0; i++) {
		if (str::eq(desclist[i].name, "group")) {
			st_glob.string = desclist[i].location;
			confsep(expand("groupindexmsg", DM_VAR), -1, &st_glob, part, 0);
		} else {
			auto idx = get_idx(desclist[i].name, conflist);

			/* The following code was copied from check() */
			if (idx < 0) {
				error("reading config for ", desclist[i].name);
				continue;
			}
			const auto config = get_config(idx);
			if (config.empty()) {
				error("reading config for ", desclist[i].name);
				continue;
			}

			/* Pass security checks */
			if (!check_acl(JOIN_RIGHT, idx))
				continue;
			if (!check_acl(RESPOND_RIGHT, idx))
				force |= O_READONLY;

			sec = security_type(config, idx);

			if (confidx >= 0 &&
			    str::eq(conflist[idx].location, conflist[confidx].location))
			{
				refresh_sum(0, confidx, sum, part, &st_glob);
				st = &st_glob;
				part3 = part;
			} else {
				st = &st_temp;
#ifdef NEWS
				st->c_article = 0;
#endif
				/* Read in partfile */
				read_part(config[CF_PARTFILE], part2, st, idx);

				/* Initialize c_status */
				st->c_status = 0;
				if (is_fairwitness(idx))
					st->c_status |= CS_FW;
				else
					st->c_status &= ~CS_FW;
				if ((flags | force) & O_OBSERVE)
					st->c_status |= CS_OBSERVER;
				else
					st->c_status &= ~CS_OBSERVER;
				if ((flags | force) & O_READONLY)
					st->c_status |= CS_NORESPONSE;
				else
					st->c_status &= ~CS_NORESPONSE;

				const auto partfile = str::join("/", {partdir, config[CF_PARTFILE]});
				st->parttime = (stat(partfile.c_str(), &stt)) ? 0 : stt.st_mtime;
				st->c_security = security_type(config, idx);

				/* Read in sumfile */
				get_status(st, sum2, part2, idx);
				part3 = part2;
			}
			st->c_security = sec;

			if ((!st_glob.before || st->sumtime < st_glob.before) &&
			    (st_glob.since <= st->sumtime)) {
				st->string = desclist[i].location;
				st->count = (++count);
				sepinit(IS_ITEM);
				if (argc < 2 && current == i)
					sepinit(IS_CFIDX);
				/* End of code copied from check */
				def_macro("indexconfname", DM_VAR,
				    compress(conflist[idx].name));
				confsep(expand("confindexmsg", DM_VAR), idx, st,
				    part3, 0);
			}
		}
		st_glob.string.clear();
	}
	return 1;
}
