// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include "driver.h"

#include <sys/signal.h>
#include <sys/types.h>

#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include "conf.h"
#include "edbuf.h"
#include "edit.h"
#include "files.h"
#include "item.h"
#include "joq.h"
#include "lib.h"
#include "license.h"
#include "macro.h"
#include "mem.h"
#include "misc.h"
#include "news.h"
#include "options.h"
#include "rfp.h"
#include "security.h"
#include "sep.h"
#include "stats.h"
#include "str.h"
#include "struct.h"
#include "sysop.h"
#include "system.h"
#include "user.h"
#include "www.h"
#include "yapp.h"

// Status info
#ifdef WWW
unsigned long ticket = 0;
#endif
flag_t flags = 0;		// user settable parameter flags
unsigned char mode = M_OK;	// input mode (which prompt)
flag_t status = 0;		// system status flags
flag_t debug = 0;		// debug flags

// Conference info
int current = -1;		// current index to cflist
int confidx = -1;		// current index to conflist
int defidx = -1;
int joinidx = -1;		// current index to conflist
std::string confname;		// name of current conference
std::vector<std::string> cflist;
std::string cfliststr;		// cflist in a string
std::vector<std::string> fw;	// List of FW's for current conf
partentry_t part[MAX_ITEMS];    // User participation info
/* System info */
std::string bbsdir;		// Directory for bbs files
std::string helpdir;		// Directory for help files
std::vector<assoc_t> conflist;	// System table of conferences
std::vector<assoc_t> desclist;	// System table of conference descriptions
std::string hostname;           // System host name

// Info on the user
uid_t uid;			// User's UID
std::string login;		// User's login
std::string fullname;		// User's fullname from passwd
std::string email;		// User's email address
std::string home;		// User's home directory
std::string work;		// User's work directory
std::string partdir;		// User's participation file dir
int cgi_item;			// single item # in cgi mode
int cgi_resp;			// single resp # in cgi mode

// Item statistics
status_t st_glob;		// statistics on current conference
status_t st_new;		// statistics on new conference to join
sumentry_t sum[MAX_ITEMS];	// items in current conference
response_t re[MAX_RESPONSES];	// responses to current item

// Variables global to this module only
static char *cmdbuf = NULL;
std::string pipebuf;
char *retbuf = NULL;
char evalbuf[MAX_LINE_LENGTH];
FILE *pipe_input = NULL;
int stdin_stack_top = 0;               // 0 is always real_stdin
stdin_t orig_stdin[STDIN_STACK_SIZE];  // original fp's opened
stdin_t saved_stdin[STDIN_STACK_SIZE]; // dup'ed fds when pushed

// Push a stream onto the standard input stack.
// Takes new fp and type.
void
push_stdin(FILE *fp, int type)
{
	int old_fd, i;
	FILE *old_fp;

	if (!fp || fileno(fp) < 1) {
		std::println("Invalid fp passed to push_stdin()!");
		return;
	}
	if (stdin_stack_top + 1 >= STDIN_STACK_SIZE) {
		error("out of stdin stack space");
		endbbs(1);
	}
	orig_stdin[stdin_stack_top + 1].type = type;
	orig_stdin[stdin_stack_top + 1].fp = fp;
	orig_stdin[stdin_stack_top + 1].fd = fileno(fp);

	old_fd = dup(0);
	old_fp = st_glob.inp;
	/* close stdin */
	close(0);
	i = dup(fileno(fp));
	if (i)
		std::println("Dup error, i={} fd={} old={}", i, fileno(fp), old_fd);
	st_glob.inp = fdopen(0, "r");
	saved_stdin[stdin_stack_top].type = orig_stdin[stdin_stack_top].type;
	saved_stdin[stdin_stack_top].fp = old_fp;
	saved_stdin[stdin_stack_top].fd = old_fd;
	if (debug & DB_IOREDIR) {
		std::println("orig_stdin[{}]={}   saved_stdin[{}]={}",
		    stdin_stack_top + 1, orig_stdin[stdin_stack_top + 1].fd,
		    stdin_stack_top, saved_stdin[stdin_stack_top].fd);
	}
	stdin_stack_top++;
}

void
pop_stdin(void)
{
	if (stdin_stack_top <= 0) {
		error("tried to pop off null stdin stack");
		endbbs(1);
	}
	/* close stdin */
	switch (orig_stdin[stdin_stack_top].type & STD_TYPE) {
	case STD_FILE:
		mclose(orig_stdin[stdin_stack_top].fp);
		break;
	case STD_SFILE:
		smclose(orig_stdin[stdin_stack_top].fp);
		break;
	case STD_SPIPE:
		spclose(orig_stdin[stdin_stack_top].fp);
		break;
	}
	close(0);

	auto fd = saved_stdin[--stdin_stack_top].fd;
	if (debug & DB_IOREDIR)
		std::println("restoring {} as stdin", fd);
	dup(fd);
	st_glob.inp = fdopen(0, "r");
	close(fd);
	if (saved_stdin[stdin_stack_top].fp)
		st_glob.inp = saved_stdin[stdin_stack_top].fp;

	clearerr(st_glob.inp);
}

void
open_pipe(void)
{
	if (status & S_REDIRECT)
		return;

	/* Need to check if pager exists */
	if (!(status & S_PAGER)) {
		if (pipebuf.empty())
			pipebuf = expand("pager", DM_VAR);
		if (!pipebuf.empty()) {
			/* Compress it a bit */
			auto pager = std::string(strimw(pipebuf));
			if (pager.front() == '"') {
				pager.erase(0, 1);
				if (!pager.empty() && pager.back() == '"')
					pager.pop_back();
			}
			if (pager.front() == '\'') {
				pager.erase(0, 1);
				if (!pager.empty() && pager.back() == '\'')
					pager.pop_back();
			}
			if (!(flags & O_BUFFER))
				pager.clear();
			if (!pager.empty()) {
				st_glob.outp = spopen(pager);
				if (st_glob.outp == NULL)
					pager.clear();
				else
					status |= S_PAGER;
			}
			if (pager.empty())
				st_glob.outp = stdout;

		} else
			st_glob.outp = stdout;
	}
}

// Print a prompt for an input mode.
void
print_prompt(int mod)
{
	const char *str = NULL;
	if (flags & O_QUIET)
		return;

	switch (mod) {
	case M_OK: /* In a conference or not? */
		str = (confidx < 0) ? "noconfp" : "prompt";
		break;
	case M_RFP:
		str = (!check_acl(RESPOND_RIGHT, confidx)) ? "obvprompt"
		                                           : "rfpprompt";
		break;
	case M_TEXT:
		str = "text";
		break;
	case M_JOQ:
		str = "joqprompt";
		break;
	case M_EDB:
		str = "edbprompt";
		break;
	}
	if (debug & DB_DRIVER)
		std::println("!{}!", str);
	/* expand seps & print */
	confsep(expand(str, DM_VAR), confidx, &st_glob, part, 1);
}

// COMMAND LOOP: PRINT PROMPT & GET RESPONSE
// RETURNS: 0 on eof, 1 else
// ARGUMENTS:
// Default command (if any)
// Min level of stdin to use
bool
get_command(const std::string_view &def, int lvl)
{
	char *inbuff, *inb;
	bool ok = true;

	status &= ~S_INT; /* Clear interrupt */
	if (status & S_PAGER)
		spclose(st_glob.outp);

	inbuff = cmdbuf;
	if (inbuff == NULL) {
		int c;
		/* Pop up stdin stack until we're not sitting at EOF */
		while (orig_stdin[stdin_stack_top].type != STD_TTY &&
		       ((c = getc(st_glob.inp)) == EOF) &&
		       (stdin_stack_top > 0 + (orig_stdin[0].type == STD_SKIP)))
			pop_stdin();
		if (orig_stdin[stdin_stack_top].type != STD_TTY && c != EOF)
			ungetc(c, st_glob.inp);

		if (stdin_stack_top < lvl) {
			return 0;
		}

		/* If taking input from the keyboard, print a prompt */
		if (isatty(fileno(st_glob.inp)))
			print_prompt(mode);

		if (mode == M_OK)
			status &= ~S_STOP;
		inbuff = xgets(st_glob.inp, lvl);
		ok = inbuff != NULL;
		if (ok && orig_stdin[stdin_stack_top].type != STD_TTY &&
		    (flags & O_VERBOSE)) {
			std::println("command: {}", inbuff);
			fflush(stdout);
		}
	}
	if (cmdbuf != NULL || ok) {
		/* Strip leading & trailing spaces */
		inb = trim(inbuff);

		/* ignore blank lines in batch mode */
		if (*inb == '\0') {
			if ((status & S_BATCH) != 0)
				ok = true;
			else
				ok = command(std::string(def), 0);
		} else
			ok = command(inb, 0);
	}
	free(inbuff);

	return ok;
}

// Clean up memory and exit.  Takes exit status.
void
endbbs(int ret)
{
	int i;

	if (status & S_PAGER)
		spclose(st_glob.outp);

	if (debug & DB_DRIVER)
		std::println("endbbs:");
	if (confidx >= 0)
		leave(0, (char **)0); /* leave current conference */

	/* Must close stdins after leave() since leave opens /usr/bbs/rc for
	 * stdin */
	while (stdin_stack_top > 0)
		pop_stdin();

	/* Free config */
	free_config();

	for (i = 0; i < MAX_RESPONSES; i++) {
		re[i].login.clear();
		re[i].fullname.clear();
#ifdef NEWS
		re[i].mid.clear();
#endif
	}

	clear_cache(); /* throw out stat cache */
	undefine(~0);  /* undefine all macros */
	pipebuf.clear();
	free(cmdbuf);
	free(retbuf);
	mcheck(); /* verify that files are closed */

	exit(ret);
}

// Opens the BBS cluster.  Takes BBS and help directories.
void
open_cluster(const std::string &bdir, const std::string &hdir)
{
	/* Free up space, etc */
	conflist.clear();
	desclist.clear();
	defidx = -1;

	/* Read in $BBS/conflist */
	bbsdir = bdir;
	helpdir = hdir;
	if (helpdir.empty())
		helpdir = bbsdir + "/help";
	conflist = grab_list(bbsdir, "conflist", 0);
	desclist = grab_list(bbsdir, "desclist", 0);
	if (conflist.empty())
		endbbs(2);

	for (auto i = 1uz; i < conflist.size(); i++) {
		if (str::eq(conflist[0].location, conflist[i].location) ||
		    match(conflist[0].location, conflist[i].name))
			defidx = i;
	}
	if (defidx < 0)
		std::println("Warning: bad default conference");

	/* Source system rc file */
	source(bbsdir, "rc", STD_SUPERSANE, SL_OWNER);
}

static int int_on = 1;

void
ints_on(void)
{
	struct sigaction vec;
	sigaction(SIGINT, NULL, &vec);
	vec.sa_flags &= ~SA_RESTART;
	sigaction(SIGINT, &vec, NULL);
	int_on = 1;
}

void
ints_off(void)
{
	struct sigaction vec;
	sigaction(SIGINT, NULL, &vec);
	vec.sa_flags |= SA_RESTART;
	sigaction(SIGINT, &vec, NULL);
	int_on = 0;
}

void
handle_alarm(int sig)
{
	(void)sig;
	error("out of time");
	exit(1);
}

/******************************************************************************/
/* PROCESS COMMAND LINE ARGUMENTS                                             */
/******************************************************************************
Function:    init
Called by:   main
Arguments:   Command line arguments
Returns:
Calls:       source for .cfonce and system rc
             grab_file to get .cflist and conflist
Description: Sets up global variables, i.e. uid, login, envars,
             workdir, processes rc file for system and for user
******************************************************************************/
void
init(int argc, char **argv)
{
	short c, o, i;
	extern char *optarg;
	extern int optind, opterr;
	char xfile[MAX_LINE_LENGTH];
	char syshostname[_POSIX_HOST_NAME_MAX];
	char *mail;
	int forcejoin = 0;
	assoc_t *defconf;

	orig_stdin[0].type = STD_TTY;

	if (gethostname(syshostname, sizeof(syshostname)) != 0)
		error("getting host name");
	hostname = syshostname;

	/* If hostname is not fully qualified, see what we can do to get it */
	if (hostname.find('.') == std::string::npos) {
		FILE *fp;
		if ((fp = fopen("/etc/resolv.conf", "r")) != NULL) {
			char *buff, field[80], value[80];
			while ((buff = xgets(fp, 0)) != NULL) {
				int nfields = sscanf(buff, "%s%s", field, value);
				free(buff);
				if (nfields == 2 && str::eq(field, "domain")) {
					hostname.append(".");
					hostname.append(value);
					break;
				}
			}
			fclose(fp);
		}
	}
	str::lowercase(hostname); /* convert upper to lower case */

	read_config();

	signal(SIGINT, handle_int);
	signal(SIGPIPE, handle_pipe);
	signal(SIGALRM, handle_alarm);
	if (getuid() == get_nobody_uid())
		alarm(600); /* web process will abort after 10 minutes */
	ints_off();

	/* Initialize options */
	for (const auto &opt : option)
		if (opt.dflt)
			flags |= opt.mask;
	for (const auto &dopt : debug_opt)
		if (dopt.dflt)
			debug |= dopt.mask;

	/* Set up user variables */
	evalbuf[0] = '\0';
	pipebuf.clear();
	free(cmdbuf);
	cmdbuf = NULL;
	st_glob.c_status = 0;
#ifdef NEWS
	st_glob.c_article = 0;
#endif
	st_glob.inp = stdin;

	/* Get current user info */
	uid = 0;
	login.clear();
	if (!get_user(&uid, login, st_glob.fullname, home, email)) {
		error("reading ", "user entry");
		endbbs(2);
	}
	fullname = st_glob.fullname;
	if (str::eq(login, "nobody"))
		status |= S_NOAUTH;

	/* Process command line options here */
	if (!uid || uid == geteuid()) {
		std::println("login {} -- invoking bbs -{}",
		    login, (uid) ? "n" : "no");
		flags &= ~(O_SOURCE); /* for security */
		if (!uid)
			flags |= O_OBSERVE | O_READONLY;
	}
	confname.clear();
	xfile[0] = '\0';
	while ((c = getopt(argc, argv, options)) != -1) {
		o = strchr(options, c) - options;
		if (o >= 0 && o < 8)
			flags ^= option[o].mask;
		else if (c == 'j') {
			confname = optarg;
			forcejoin = O_AUTOJOIN;
		} else if (c == 'x') {
			strcpy(xfile, optarg);
		}
		if (c == 'o')  // -o does observer and readonly
			flags ^= O_READONLY;
	}
#ifdef INCLUDE_EXTRA_COMMANDS
	if ((flags & O_CGIBIN) ||
	    str::eq(argv[0] + strlen(argv[0]) - 8, "yapp-cgi")) {
		flags = (flags & ~O_BUFFER) | O_QUIET | O_CGIBIN | O_OBSERVE;
		argv += optind;
		argc -= optind;
		if (argc < 1) {
			confname.clear();
			flags &= ~O_DEFAULT;
		} else
			confname = argv[0];
		if (argc < 2)
			cgi_item = -1;
		else
			cgi_item = atoi(argv[1]);
		if (argc < 3)
			cgi_resp = -1;
		else
			cgi_resp = atoi(argv[2]);
	} else
#endif
	if (optind < argc)
		confname = argv[argc - 1];

	for (i = 0; i < MAX_RESPONSES; i++) {
		re[i].fullname.clear();
		re[i].login.clear();
#ifdef NEWS
		re[i].mid.clear();
		re[i].article = 0;
#endif
	}

	/* Set up user customizations */
	def_macro("today", DM_PARAM, "+0");
	mail = getenv("SHELL");
	if (mail)
		def_macro("shell", DM_VAR | DM_ENVAR, mail);
	mail = getenv("EDITOR");
	if (mail)
		def_macro("editor", DM_VAR | DM_ENVAR, mail);
	mail = getenv("MESG");
	if (mail)
		def_macro("mesg", DM_VAR | DM_ENVAR, mail);
	urlset(); /* do QUERY_STRING sets */

	/* Read in /usr/bbs/conflist */
	open_cluster(get_conf_param("bbsdir", BBSDIR), get_conf_param("helpdir", ""));

	/* Print identification */
	if (!(flags & O_QUIET))
		command("display version", 0);

	/* Source .cfonce, etc */
	login_user();

	defconf = NULL;
	if (defidx >= 0) {
		defconf = &conflist[defidx];
	}

	/* Join initial conference */
	if (debug & DB_DRIVER && defconf != NULL)
		std::cout << "Default: " << defidx << " " << defconf->name << std::endl;
	if (flags & O_INCORPORATE) {
		/* Only accept -i from root, daemon, and Yapp owner */
		if (!uid || uid == 1 || uid == geteuid()) {
			endbbs(!incorporate(0, sum, part, &st_glob, -1));
		}
		endbbs(2);
	} else if (!(flags & O_DEFAULT) || defconf == NULL) {
		current = -1;
		st_glob.i_current = 0; /* No current item */
	} else if (!confname.empty())
		join(confname, forcejoin, 0);
	else if (!cflist.empty()) {
		join(cflist[current = 0], O_AUTOJOIN, 0);
	} else {
		/* force join */
		join(compress(defconf->name), O_AUTOJOIN, 0);
	}

#ifdef INCLUDE_EXTRA_COMMANDS
	/* CGI stuff */
	if (flags & O_CGIBIN) {
		source(bbsdir, "cgi_cfonce", 0, SL_OWNER);
		if (!confname.empty()) { /* GET CONFERENCE LIST             */
			command("list", 0);
		} else if (cgi_item < 0) { /* GET ITEM INDEX FOR A CONF       */
			command("browse all", 0);
		} else if (cgi_resp < 0) {
			/* GET RESPONSE INDEX FOR AN ITEM? */
			command(std::format("read {} pass", cgi_item), 0);
		} else { /* RETRIEVE RESPONSE */
			st_glob.i_current = cgi_item;
			st_glob.r_first = st_glob.r_last = cgi_resp;
			show_header();
			show_range();
		}
		endbbs(0);
	}
#endif

	/* Batch mode */
	if (*xfile) {
		/* When popping stdin, don't wait for input at real_stdin */
		orig_stdin[0].type = STD_SKIP;
		def_macro("batchfile", DM_VAR, xfile);
		{
			/* reopen xfile as stdin */
			FILE *fp = mopen(xfile, O_R);
			if (!fp) {
				std::println("Couldn't open {}", xfile);
				endbbs(0);
			}
			if (debug & DB_IOREDIR)
				std::println("Redirecting input from {} (fd {})",
				    xfile, fileno(fp));
			push_stdin(fp, STD_FILE); /* real_stdin =
			                           * new_stdin(fp); */
		}
		status |= S_BATCH; /* set to ignore blank lines */
	}
}

/******************************************************************************/
/* PROCESS COMMAND LINE ARGUMENTS                                             */
/******************************************************************************
Function:    char source(char *dir, char *filename, int fl)
Called by:   init
Arguments:   File to source
Returns:
Calls:       command for each statement
Description: Executes commands in a file, does NOT grab_file since it
             only needs 1-time sequential access.
 As of 10/3/96, this just pushes the file on stdin and lets xgets/ngets
 close it upon EOF.
*******************************************************************************/
char
source(
    const std::string &dir,	 /* IN : Directory containing file        */
    const std::string &filename, /* IN : Filename of commands to execute  */
    int fl,			 /* IN : Extra flags to set during exec   */
    int sec			 /* IN : open file as user or as cfadm?   */
)
{
	std::string path(dir);
	if (!filename.empty()) {
		path.append("/");
		path.append(filename);
	}

	if (debug & DB_DRIVER)
		std::println("source: {}", path);
	FILE *fp = (sec == SL_OWNER) ? mopen(path, O_R | O_SILENT) : smopenr(path, O_R | O_SILENT);
	if (fp == nullptr)
		return 0;

	/* Save standard input */
	if (!fileno(fp))
		std::println("save error 1");
	if (debug & DB_IOREDIR)
		std::println("Redirecting input from {} (fd {})", path, fileno(fp));
	push_stdin(fp, STD_FILE | fl);

	/* Execute commands until we pop back to the previous level */
	{
		int lvl = stdin_stack_top;
		while (stdin_stack_top >= lvl) get_command("", lvl);
	}
	return 1;

}

static int fd_stack[3] = {-1, -1, -1};
static int fd_top = 0;

void
push_fd(int fd)
{
	if (fd_top >= 3)
		error("pushing ", "fd");
	else
		fd_stack[fd_top++] = fd;
}

int
pop_fd(void)
{
	int i;
	if (fd_top > 0) {
		fd_top--;
		i = fd_stack[fd_top];
		fd_stack[fd_top] = -1;
		return i;
	}
	return -1;
}

/*
 * History expansion
 * !* = all arguments
 * Takes: Expanded Macro, original arguments.
 */
std::string
expand_history(const std::string &mac, const std::string_view &orig_args)
{
	static constexpr auto ws = " \t\r\n\f\v";

	// Skip leading whitespace in origargs
	auto oargs = orig_args;
	if (auto i = oargs.find_first_not_of(ws); i != std::string_view::npos)
		oargs.remove_prefix(i);

	auto s = 0uz;
	auto e = mac.find("!*");
	if (e == std::string_view::npos)
		return mac + std::string(oargs);
	std::string out;
	while (e != std::string_view::npos) {
		out.append(mac.substr(s, e));
		out.append(oargs);
		s = e + 2;
		e = mac.find("!*", s);
	}
	out.append(mac.substr(s));

	return out;
}
/******************************************************************************/
/* PROCESS COMMAND LINE ARGUMENTS                                             */
/******************************************************************************
Function:    char command(char *command)
Called by:   main
Arguments:   command to process
Returns:     0 if done, 1 else
Calls:       join() for "join", "next" commands
             leave() for "next", "quit" commands
Description: For all command modes, this processes a user command.
             Interrupts go back to here, without changing command mode.
*******************************************************************************/
/* ARGUMENTS:                       */
/* Command to execute               */
/* Recursion depth                  */
char
command(const std::string &stro, int lvl)
{
	int argc = 0; /* Number of arguments */
	int i, skip = 0;
	char *argv[MAX_ARGS], cmddel, bufdel;
	const char *Eptr;
	char state = 1, ok = 1, *newstr = NULL, *tmpstr;
	char *cmd = NULL;
	/* Redirection information */
	int saved_fd[3];
	int is_pipe[3];
	FILE *curr_fp[3];
	int prev_redir = (status & S_REDIRECT);
	int prev_nostdin = (status & S_NOSTDIN);
	int prev_top = fd_top;
	std::string wordfile;

	/* FreeBSD needs the second line below and NOT the first, or we see
	 * duplicate footers in the read command.  HP-UX, on the other hand
	 * has a broken freopen() and needs to re-seek */

	/* Helpcmd section */
	const char *str = stro.c_str();
	if (mode == M_OK && stro.empty() && !(status & S_BATCH) &&
	    isatty(fileno(st_glob.inp))) {
		const auto helpcmd = expand("helpcmd", DM_VAR);
		if (!helpcmd.empty()) {
			str = helpcmd.c_str();
		}
	}
	for (i = 0; i < 3; i++) {
		saved_fd[i] = -1;
		is_pipe[i] = 0;
	}

	if (!str || !*str)
		return 1;

	if (debug & DB_DRIVER)
		std::println("command: '{}' level: {}", str, lvl);
	if (lvl > CMD_DEPTH) {
		std::println("Too many expansions.");
		return 0;
	}
	const char *Sptr = str;
	while (isspace(*Sptr)) Sptr++; /* skip whitespace */

	/* Skip if inside false condition, but we have to parse args to find
	 * ';' for start of next command */
	if (!test_if() && strncmp("endif", Sptr, 5) &&
	    strncmp("else", Sptr, 4) && strncmp("if ", Sptr, 3))
		skip = 1;

	/* Process shell escape */
	if (str[0] == '!') {
		if (!skip) {
			unix_cmd(str + 1);
			std::println("!");
		}
		return 1;
	}

	/* And comments */
	if (*Sptr == '#')
		return 1;

	cmddel = expand("cmddel", DM_VAR)[0];
	bufdel = expand("bufdel", DM_VAR)[0];

	/* Get arguments using a state machine for lexical analysis */
	pipebuf.clear();
	free(cmdbuf);
	cmdbuf = NULL;
	while (state && argc < MAX_ARGS) {
		switch (state) {
		case 1: /* between words */
			while (isspace(*Sptr)) Sptr++;
			if (*Sptr == cmddel) {
				Sptr++;
				state = 0;
			} else if (*Sptr == bufdel) {
				Eptr = ++Sptr;
				state = 6;
			} else if (*Sptr == '|') {
				Eptr = ++Sptr;
				state = 7;
			} else if (*Sptr == '(') {
				Eptr = Sptr;
				state = 10;
			} else if (*Sptr == '>') {
				Eptr = ++Sptr;
				state = 9;
			} else if (*Sptr == '<') {
				Eptr = ++Sptr;
				state = 12;
			} else if (*Sptr == '\'') {
				Eptr = ++Sptr;
				state = 4;
			} else if (*Sptr == '`') {
				Eptr = ++Sptr;
				state = 8;
			} else if (*Sptr == '\"') {
				Eptr = Sptr;
				state = 3;
			} else if (*Sptr == '%') {
				Eptr = ++Sptr;
				state = 11;
			} else if (*Sptr == '\\') {
				Eptr = Sptr;
				state = 5;
			} else if (*Sptr) {
				Eptr = Sptr;
				state = 2;
			} else
				state = 0;
			break;

		case 2: /* normal word */
			while (*Eptr && !isspace(*Eptr) && *Eptr != cmddel &&
			       *Eptr != bufdel && !strchr("|`'>\\\"", *Eptr) &&
			       !(Eptr > Sptr &&
			           *(Eptr - 1) == '=') /* '=' terminates word */
			       && (argc || *Sptr == '-' || isdigit(*Sptr) ||
			              !isdigit(*Eptr)))
				Eptr++;

			argv[argc++] = estrndup(Sptr, Eptr - Sptr);
			Sptr = Eptr;

			if (argc == 1) {
				auto cmd2 = expand(argv[0], (mode == M_RFP) ? DM_RFP : DM_OK);
				if (!cmd2.empty()) {
					auto tmp = expand_history(cmd2, Eptr);

					/* Undo first argument */
					free(argv[0]);
					argv[0] = NULL;
					argc = 0;

					/* Store cmd for later freeing */
					free(cmd);
					cmd = estrdup(tmp.c_str());
					Sptr = cmd; /* Parse cmd instead */
				}
			}
			state = 1;
			break;

		case 3: /* "stuff" */
		{
			int quot = 0;
			char *p;
			const char *q;

			/* First expand backtick commands */
			for (Eptr = Sptr + 1;
			     *Eptr && (*Eptr != '\"' || *(Eptr - 1) == '\\');
			     Eptr++) {
				if (*Eptr == '`' && *(Eptr - 1) != '\\') {
					const char *Cptr = Eptr + 1;
					/* Find end of command */
					do {
						Eptr++;
					} while (*Eptr && *Eptr != '`');

					free(cmd);
					cmd = estrndup(Cptr, Eptr - Cptr);
					if (*Eptr)
						Eptr++; /* Set Eptr to
						         * next char
						         * after end `
						         */

					status |= S_EXECUTE;
					evalbuf[0] = '\0';
					command(cmd, lvl + 1);
					status &= ~S_EXECUTE;
					free(cmd);
					cmd = NULL;

					/* We want to end up with Sptr
					 * pointing to a buffer which
					 * contains the inital text,
					 * followed by the output,
					 * followed by whatever was
					 * left in the original
					 * buffer. */
					/* before */
					std::string sa(Sptr, Cptr - Sptr - 1);
					q = evalbuf;
					while (*q) {
						if (*q == '\n' || *q == '\r') {
							q++;
						} else if (*q == '"' &&
						           (q == evalbuf || q[-1] != '\\')) {
							/* escape quotes */
							sa.push_back('\\');
							sa.push_back(*q++);
						} else
							sa.push_back(*q++);
					}
					sa.append(Eptr);
					free(newstr);
					Sptr = newstr = estrdup(sa.c_str());
				}
			}

			/* Count occurrences of \" and set Eptr to end of string */
			Eptr = Sptr; /* reset to start of buffer after " */
			do {
				Eptr++;
				if (*Eptr == '\"' && *(Eptr - 1) == '\\')
					quot++;
			} while (*Eptr && (*Eptr != '\"' || *(Eptr - 1) == '\\'));

			/* Include the quotes in the arg */
			argv[argc] = (char *)emalloc(Eptr - Sptr + 2 - quot);
			p = argv[argc];
			q = Sptr;
			while (q <= Eptr) {
				if (*q == '\\' && q[1] == '"')
					q++;
				*p++ = *q++;
			}
			*p = '\0';
			argc++;

			if (*Eptr)
				Eptr++;
			Sptr = Eptr;
			state = 1;
			break;
		}


		case 4: /* 'stuff' */
			{
				int quot = 0;
				char *p;
				const char *q;
				while (*Eptr &&
				       (*Eptr != '\'' || *(Eptr - 1) == '\\')) {
					Eptr++;
					if (*Eptr == '\'' && *(Eptr - 1) == '\\')
						quot++;
				}
				argv[argc] = (char *)emalloc(Eptr - Sptr + 2 - quot);
				p = argv[argc];
				q = Sptr;
				while (q < Eptr) {
					if (*q == '\\' && q[1] == '\'')
						q++;
					*p++ = *q++;
				}
				*p = '\0';
				argc++;

				if (*Eptr)
					Eptr++;
				Sptr = Eptr;
				state = 1;
				break;
			}

		case 5: /* \\ */
			argv[argc] = estrdup("\\");
			Sptr = Eptr + 1;
			state = 1;
			break;

		case 6: /* ,stuff */
			do {
				Eptr++;
			} while (*Eptr && *Eptr != cmddel);
			free(cmdbuf);
			cmdbuf = estrndup(Sptr, Eptr - Sptr);
			Sptr = Eptr;
			state = 1;
			break;

		case 7: /* | stuff */
		{       /* Also |& */
#define OP_STDOUT 0x00
#define OP_STDERR 0x01
			int op = OP_STDOUT;
			char pcmd[MAX_LINE_LENGTH], *str;
			if (*Eptr == '&') {
				op |= OP_STDERR;
				Eptr++;
			}
			i = (op & OP_STDERR) ? 2 : 1;

			/* Skip whitespace */
			while (isspace(*Eptr)) Eptr++;

			/* Get pcmd */
			str = pcmd;
			while (*Eptr && *Eptr != cmddel && *Eptr != bufdel) {
				*str++ = *Eptr++;
			}
			*str = '\0';

			if (!skip) {

				/* Expand command if sep */
				if (pcmd[0] == '%') {
					const char *f, *s;
					s = pcmd + 1;
					f = get_sep(&s);
					strcpy(pcmd, f);
				}

				/* Open the pipe */
				if (sdpopen((status & S_EXECUTE) ? &pipe_input
				                                 : NULL,
				        &curr_fp[i], pcmd)) {
					is_pipe[i] = 1;

					/* Close the old std file
					 * descriptor */
					saved_fd[i] = dup(i);
					if (debug & DB_PIPE) {
						std::println(stderr,
						    "saved fd {} in fd {}",
						    i, saved_fd[i]);
						fflush(stderr);
					}
					close(i);

					/* Open the new std file
					 * descriptor (as USER) */
					dup(fileno(curr_fp[i]));
					if (debug & DB_PIPE) {
						std::println(stderr,
						    "installed fd {} as new fd {}",
						    fileno(curr_fp[i]), i);
						fflush(stderr);
					}

					/* Push i on the "stack" */
					push_fd(i);
					status |= S_REDIRECT;
				}
			}
			Sptr = Eptr;
			state = 1;
			break;
		}

		case 8: /* `command` */
			do {
				Eptr++;
			} while (*Eptr && *Eptr != '`');

			if (!skip) {
				auto old = cmd;
				cmd = estrndup(cmd, Eptr - Sptr);
				free(old);
				if (*Eptr)
					Eptr++; /* Set Eptr to next char
					         * after end ` */

				status |= S_EXECUTE;
				evalbuf[0] = '\0';
				command(cmd, lvl + 1);
				status &= ~S_EXECUTE;
				free(cmd);
				cmd = NULL;

				/* evalbuf should now contain all the
				 * output even if obtained via a pipe */

				/* We want to end up with Sptr pointing
				 * to a buffer which contains the
				 * output, followed by whatever was left
				 * in the original buffer.  This way,
				 * the output will be split into fields
				 * as well, as sh does */
				tmpstr = (char *)emalloc(strlen(evalbuf) + strlen(Eptr) + 1);
				strcpy(tmpstr, evalbuf);
				strcat(tmpstr, Eptr);
				free(newstr);
				Sptr = newstr = tmpstr;
			} else
				Sptr = Eptr;
			state = 1;
			break;

		case 9: /* > file */
		{       /* Also >>, >&, >>& */
#define OP_STDOUT 0x00
#define OP_STDERR 0x01
#define OP_APPEND 0x10
			int op = OP_STDOUT;
			int fd = 1; /* stdout */
			std::string filename;

			/* Get actual operator */
			if (*Eptr == '>') {
				op |= OP_APPEND;
				Eptr++;
			}
			if (*Eptr == '&') {
				op |= OP_STDERR;
				fd = 2; /* stderr */
				Eptr++;
			}

			/* Skip whitespace */
			while (isspace(*Eptr))
				Eptr++;

			/* Get filename */
			while (*Eptr != '\0' &&
			   *Eptr != cmddel &&
			   *Eptr != bufdel &&
			   *Eptr != ' ')
			{
				filename.push_back(*Eptr++);
			}

			if (!skip) {

				/* Expand filename if sep */
				if (filename[0] == '%') {
					const char *s = filename.c_str() + 1;
					filename = get_sep(&s);
				}

				/* Open the file */
				curr_fp[fd] = smopenw(filename,
				    (op & OP_APPEND) ? O_A : O_W);
				if (!curr_fp[fd]) {
					error("redirecting output to ", filename);
				} else {
					is_pipe[fd] =
					    0; /* just a normal file */

					/* Close the old std file
					 * descriptor */
					saved_fd[fd] = dup(fd);
					close(fd);

					dup(fileno(curr_fp[fd]));
					push_fd(fd);

					status |= S_REDIRECT;
				}
			}
			Sptr = Eptr;
			state = 1;
			break;
		}

		case 10: /* (stuff) */
			do {
				Eptr++;
			} while (
			    *Eptr && (*Eptr != ')' || *(Eptr - 1) == '\\'));
			argv[argc] = estrndup(Sptr, Eptr - Sptr + 1);
			if (*Eptr)
				Eptr++;
			Sptr = Eptr;
			state = 1;
			break;

		case 11: /* %separator stuff */
		{
			const char *str = get_sep(&Eptr);

			/* only make an arg if * something there */
			if (str && *str) {
				argv[argc++] = estrdup(str);
			}
			Sptr = Eptr;
			state = 1;
			break;
		}

		case 12: /* < file, << word */
		{
			std::string filename;
			std::string word;
#define OP_FILEIN 0x00
#define OP_WORDIN 0x01
			int op = OP_FILEIN;
			/* Get actual operator */
			if (*Eptr == '<') {
				op |= OP_WORDIN;
				Eptr++;
			}

			/* Skip whitespace */
			while (isspace(*Eptr))
				Eptr++;

			/* Get word */
			while (*Eptr != '\0' &&
			    *Eptr != cmddel &&
			    *Eptr != bufdel &&
			    *Eptr != ' ')
			{
				word.push_back(*Eptr++);
			}

			if (op & OP_WORDIN) { /* << word */
				char *buff = NULL;
				/* Save lines in a temp file until we
				 * see word or EOF */
				wordfile = std::format("/tmp/word.{}", getpid());
				FILE *fp = smopenw(wordfile, O_W);
				while ((buff = xgets(st_glob.inp,
				            stdin_stack_top)) != NULL) {
					if (str::eq(buff, word))
						break;
					std::println(fp, "{}", buff);
					free(buff);
					buff = NULL;
				}
				if (buff)
					free(buff);
				smclose(fp);

				filename = wordfile;
			} else { /* < file */
				filename = word;

				/* Expand filename if sep */
				if (filename[0] == '%') {
					const char *f, *str;
					str = filename.c_str() + 1;
					f = get_sep(&str);
					filename = f;
				}
			}

			if (!skip) {

				/* Open the file */
				curr_fp[0] = smopenr(filename, O_R);
				if (!curr_fp[0])
					std::println("smopenr returned null");
				is_pipe[0] = 0;

				if (debug & DB_IOREDIR)
					std::println("Redirecting input from {} (fd {})",
					    filename, fileno(curr_fp[0]));
				push_stdin(curr_fp[0], STD_FILE);


				/* We don't want to set S_REDIRECT
				 * here since we want the pager to be
				 * used when we're only redirecting
				 * input. This is used by (for
				 * example) the change htmlheader
				 * script to filter HTML text from
				 * within a template. */
				status |= S_NOSTDIN;
			}
			Sptr = Eptr;
			state = 1;
			break;
		}
		}
	}
	if (argc && !skip) {
		/* Execute command */
		switch (mode) {
		case M_OK:
			ok = ok_cmd_dispatch(argc, argv);
			break;
		case M_JOQ:
			ok = joq_cmd_dispatch(argc, argv);
			break;
		case M_TEXT:
			ok = text_cmd_dispatch(argc, argv);
			break;
		case M_RFP:
			ok = rfp_cmd_dispatch(argc, argv);
			break;
		case M_EDB:
			ok = edb_cmd_dispatch(argc, argv);
			break;
		default:
			std::println("Unknown mode {}\n", mode);
			break;
		}

	} else
		ok = 1; /* don't abort on null command */

	/* Free args */
	for (i = 0; i < argc; i++)
		free(argv[i]);

	/* Now restore original file descriptor state */
	while (fd_top > prev_top) {
		i = pop_fd();
		close(i);

		/* Remove temporary file for "<< word" syntax */
		if (!i && !wordfile.empty()) {
			rm(wordfile, SL_USER);
			wordfile.clear();
		}
		if (is_pipe[i]) {
			sdpclose(pipe_input, curr_fp[i]);
			pipe_input = NULL;
		} else
			smclose(curr_fp[i]);

		/* Restore saved file descriptor */
		dup(saved_fd[i]);
		close(saved_fd[i]);
		if (debug & DB_PIPE) {
			std::println(stderr, "Restored fd {} to fd {}", saved_fd[i], i);
			fflush(stderr);
		}

		/* Clear saved info */
		saved_fd[i] = -1;
		is_pipe[i] = 0;
	}
	if (!prev_redir)
		status &= ~S_REDIRECT;
	if (!prev_nostdin)
		status &= ~S_NOSTDIN;

	/* Do next ; cmd unless EOF or command says to halt (ok==2) */
	if (ok == 1 && *Sptr && !(status & S_STOP)) {
		/* 2/2/96: this was lvl+1 below, but it broke 'if' nesting
		 * since commands after the ';' appeared to be a level deeper,
		 * so 'else' and 'endif' couldn't appear after a ';' */
		ok = command(Sptr, lvl);
	}
	free(newstr);
	free(cmd);
	return ok;
}

/* Commands available at the Ok: prompt only */
static dispatch_t ok_cmd[] = {
    { "i_tem", do_read, },
    { "r_ead", do_read, },
    { "pr_int", do_read, },
    { "e_nter", enter, },
    { "s_can", do_read, },
    { "b_rowse", do_read, },
    /* j_oin */
    { "le_ave", leave, },
    { "n_ext", do_next, },
    { "che_ck", check, },
    { "rem_ember", remember, },
    { "forget", forget, },
    { "unfor_get", remember, },
    { "k_ill", do_kill, },
    { "retitle", retitle, },
    { "freeze", freeze, },
    { "thaw", freeze, },
    /* sync_hronous */
    /* async_hronous */
    { "retire", freeze, },
    { "unretire", freeze, },
    { "f_ind", do_find, },
    { "l_ocate", do_find, },
    { "seen", fixseen, },
    { "fix_seen", fixseen, },
    { "fixto", fixto, },
    { "re_spond", respond, },
    /* lpr_int */
    { "li_nkfrom", linkfrom, },
    { "abort", leave, },
    /* ex_it q_uit st_op good_bye log_off log_out h_elp exp_lain sy_stem unix
     * al_ias def_ine una_lias und_efine ec_ho echoe echon echoen echone so_urce
     * m_ail t_ransmit sen_dmail chat write d_isplay que_ry */
    { "p_articipants", participants, },
    { "desc_ribe", describe, },
    /* w_hoison am_superuser */
    { "resign", resign, },
    /* chd_ir uma_sk sh_ell f_iles dir_ectory ty_pe e_dit cdate da_te t_est
     * clu_ster
     */
    { "ps_eudonym", respond, },
    { "list", check, },
    { "index", show_conf_index, },
#ifdef WWW
#ifdef INCLUDE_EXTRA_COMMANDS
    { "authenticate", authenticate, },
#endif
#endif
    { "cfcreate", cfcreate, },
    { "cfdelete", cfdelete, },
    { 0, 0 },
};
/******************************************************************************/
/* DISPATCH CONTROL TO APPROPRIATE MISC. COMMAND FUNCTION                     */
/******************************************************************************/
char
ok_cmd_dispatch(/* ARGUMENTS:                 */
    int argc,   /* Number of arguments     */
    char **argv /* Argument list           */
)
{
	int i;
	for (i = 0; ok_cmd[i].name; i++)
		if (match(argv[0], ok_cmd[i].name))
			return ok_cmd[i].func(argc, argv);

	/* Command dispatch */
	if (match(argv[0], "j_oin")) {
		if (argc == 2)
			join(argv[1], 0, 0);
		else if (confidx >= 0) {
			confsep(expand("joinmsg", DM_VAR), confidx, &st_glob,
			    part, 0);
		} else
			std::println("Not in a {}!", conference());
	} else
		return misc_cmd_dispatch(argc, argv);
	return 1;
}
/******************************************************************************/
/* PROCESS A GENERIC SIGNAL (if enabled)                                      */
/******************************************************************************/
void
handle_other(int sig)
{
	if (status & S_PIPE)
		std::println("{} Pipe interrupt {}!", getpid(), sig);
	else
		std::println("{} Interrupt {}!", getpid(), sig);
	status |= S_INT;
}
/******************************************************************************/
/* PROCESS A USER INTERRUPT SIGNAL                                            */
/******************************************************************************/
void
handle_int(int sig) /* ARGUMENTS: (none)  */
{
	(void)sig;
	if (!(status & S_PIPE)) {
		std::println("Interrupt!");
		status |= S_INT;
	}
	signal(SIGINT, handle_int);
}
/******************************************************************************/
/* PROCESS AN INTERRUPT CAUSED BY A PIPE ABORTING                             */
/******************************************************************************/
void
handle_pipe(int sig)
{
	(void)sig;
	if (status & S_PAGER)
		std::println("Pipe interrupt?\n");
	signal(SIGPIPE, handle_pipe);
	status |= S_INT;
}
