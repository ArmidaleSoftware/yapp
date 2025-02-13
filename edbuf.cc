// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include "edbuf.h"

#include <sys/stat.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <print>
#include <string>
#include <string_view>
#include <vector>

#include "driver.h"
#include "files.h"
#include "globals.h"
#include "help.h"
#include "lib.h"
#include "macro.h"
#include "main.h"
#include "misc.h"
#include "str.h"
#include "struct.h"
#include "system.h"
#include "yapp.h"

static FILE *file;
static char post;
static char oldmode;
static int resp;

/* Regarding mode: NOW USES oldmode
 * Set mode back to RFP for respond command
 * Enter command will set mode to OK itself
 */

/******************************************************************************/
/* MAIN TEXT ENTRY LOOP                                                       */
/******************************************************************************/
char
cfbufr_open(/* ARGUMENTS: */
    int flg /* File open type (r,w, etc) */
)
{
	const auto path = str::concat({work, "/cf.buffer"});
	if ((file = smopenw(path.c_str(), flg)) == NULL)
		return 0;
	if (!(flags & O_QUIET))
		std::println(R"(Type "." to exit or ":help".)");
	return 1;
}
/******************************************************************************/
char
text_loop(		// ARGUMENTS: (none)
    bool is_new,	// True if we should start from scratch, false
			// if modifying
    const char *label	// What to ask for ("text", "response", etc)
)
{
	bool ok = true;

	if (flags & O_EDALWAYS)
		return edit(work, "cf.buffer", 0);

	post = 0;

	if (is_new) {
		struct stat st{};
		const auto fromname = str::concat({work, "/cf.buffer"});
		if (stat(fromname.c_str(), &st) == 0) { /* cf.buffer exists */
			const auto toname = std::format("{}/cbf.{}", work, getpid());
			if (copy_file(fromname, toname, SL_USER))
				error("renaming cf.buffer to ", toname);
		}
	}
	if (!cfbufr_open(is_new ? O_WPLUS : O_APLUS))
		return 0; /* "w+" */
	if (!(flags & O_QUIET)) {
		if (is_new) {
			text_print(0, NULL);
			std::println("Enter {}:", label);
		} else {
			std::println("(Continue your {} entry)", label);
		}
	}
	oldmode = mode;
	mode = M_TEXT;
	while (mode == M_TEXT && ok) {
		/* For optimization purposes, we do not allow seps in TEXT
		 * mode prompt.  This could be changed back if confsep would
		 * dump out most strings quickly without accessing the disk. */
		if (!(flags & O_QUIET))
			wputs(TEXT);

		std::string line;
		ok = ngets(line, st_glob.inp);
		if (ok && (status & S_INT)) {
			status &= ~S_INT; /* Clear interrupt */
			ok = !get_yes(std::format("Abort {}? ", label), true);
			if (!ok)
				post = -1;
		}

		if (!ok) {
			std::println("");
			mode = oldmode; /* Ctrl-D same as "." */
			post++;         /* post on ^D or .  don't post on ^C */
		} else if (!line.empty() && line[0] == ':') {
			if (line.size() > 1)
				ok = command(line.c_str() + 1, 0);
			else
				ok = command("e", 0);
		} else if ((flags & O_DOT) && str::eq(line, ".")) {
			mode = oldmode; /* done */
			post = 1;
		} else { /* Add to file buffer */
			std::println(file, "{}", line);
			if (ferror(stdout))
				ok = 0;
			else
				fflush(file);
		}
	}
	smclose(file);
	return post;
}
/* Commands available while in text entry mode */
static dispatch_t text_cmd[] = {
    { "q_uit", text_abort, },
    { "c_ommand", text_done, },
    { "ok", text_done, },
    { "p_rint", text_print, },
    { "e_dit", text_edit, },
    { "v_isual", text_edit, },
    { "h_elp", help, },
    { "?", help, },
    { "cl_ear", text_clear, },
    { "em_pty", text_clear, },
    { "r_ead", text_read, },
    { "w_rite", text_write, },
    { 0, 0 },
};
/******************************************************************************/
/* DISPATCH CONTROL TO APPROPRIATE TEXT COMMAND FUNCTION                      */
/******************************************************************************/
char
text_cmd_dispatch(/* ARGUMENTS:                  */
    int argc,     /* Number of arguments      */
    char **argv   /* Argument list            */
)
{
	int i;
	for (i = 0; text_cmd[i].name; i++)
		if (match(argv[0], text_cmd[i].name))
			return text_cmd[i].func(argc, argv);

	/* Command dispatch */
	if (match(argv[0], "d_one")       /* same as . on a new line */
	    || match(argv[0], "st_op")    /* ? */
	    || match(argv[0], "ex_it")) { /* ? */
		mode = oldmode;
		post = 1; /* mark as done */
	} else {
		std::println("Don't understand that!\n");
		text_abort(argc, argv);
	}
	return 1;
}
/******************************************************************************/
/* READ TEXT FROM A FILE INTO THE BUFFER                                      */
/******************************************************************************/
int
text_read(      /* ARGUMENTS:          */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
	/* PicoSpan puts spaces into the filename, we don't */
	if (argc != 2) {
		std::println("Syntax: r filename");
		return 1;
	}

	/* This is done inside the secure portion writing to the cf.buffer
	 * file, so it's already secure */
	if ((flags & O_QUIET) == 0)
		std::println("Reading {}", argv[1]);
	FILE *fp = mopen(argv[1], O_R);
	if (fp == NULL)
		return 1;
	std::string line;
	while (ngets(line, fp))
		std::println(file, "{}", line);
	mclose(fp);

	return 1;
}
/******************************************************************************/
/* WRITE TEXT IN BUFFER OUT TO A FILE                                         */
/******************************************************************************/
int
text_write(     /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
	FILE *fp;

	/* PicoSpan puts spaces into the filename, we don't */
	if (argc != 2) {
		std::println("Syntax: w filename");
		return 1;
	}

	/* This is done inside the secure portion writing to the cf.buffer
	 * file, so is already secure */
	std::println("Writing {}", argv[1]);
	if ((fp = mopen(argv[1], O_W)) == NULL) { /* use normal umask */
		return 1;
	}

	smclose(file);
	const auto filename = str::concat({work, "/cf.buffer"});
	if ((file = smopenr(filename, O_R)) == NULL)
		return 0;

	std::string buff;
	while (ngets(buff, file))
		std::println(fp, "{}", buff);
	mclose(fp);

	smclose(file);
	if ((file = smopenw(filename, O_APLUS)) == NULL)
		return 0;

	return 1;
}
/******************************************************************************/
/* DUMP TEXT IN BUFFER AND START OVER                                         */
/******************************************************************************/
int
text_clear(     /* ARGUMENTS:          */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
	mclose(file);
	if (!(flags & O_QUIET))
		std::println("Enter your {}:",
		    (oldmode == M_OK) ? "text" : "response");
	if (!cfbufr_open(O_WPLUS)) /* "w+" */
		mode = oldmode;    /* abort */
	else
		mode = M_TEXT;
	return 1;
}
/******************************************************************************/
/* REPRINT CURRENT CONTENTS OF BUFFER                                         */
/******************************************************************************/
int
text_print(     /* ARGUMENTS:          */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
	/* XXX */
	smclose(file);

	const auto filename = str::concat({work, "/cf.buffer"});
	if ((file = smopenr(filename, O_R)) == NULL)
		return 0;

	/*
	   fseek(file,0L,0);
	*/
	std::string buff;
	while (ngets(buff, file) && (status & S_INT) == 0)
		std::println(" {}", buff);
	smclose(file);

	if ((file = smopenw(filename, O_APLUS)) == NULL)
		return 0;
	return 1;
}
/******************************************************************************/
/* INVOKE UNIX EDITOR ON THE BUFFER                                           */
/******************************************************************************/
int
text_edit(      /* ARGUMENTS:          */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
	int visual = (argv[0][0] == 'v'); /* 'v_isual' check */
	if (mode == M_TEXT &&
	    str::eq(expand((visual) ? "visual" : "editor", DM_VAR), "builtin")) {
		if (!(flags & O_QUIET))
			std::println("Already in builtin editor.");
	} else {
		mclose(file);
		edit(work, "cf.buffer", visual);
		std::println("(Continue your {} entry)",
		    (resp) ? "response" : "text");
		cfbufr_open(O_APLUS); /* a+ */
	}
	return 1;
}
/******************************************************************************/
/* ABORT TEXT ENTRY MODE                                                      */
/******************************************************************************/
int
text_abort(     /* ARGUMENTS:          */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
	if (get_yes("Ok to abandon text? ", true))
		mode = oldmode;
	return 1;
}
/******************************************************************************/
/* END TEXT ENTRY MODE AND POST IT                                            */
/******************************************************************************/
int
text_done(      /* ARGUMENTS:          */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
	/* Main EDB cmd loop */
	mode = M_EDB;
	while (mode == M_EDB && get_command("", 0))
		;
	return 1;
}
/******************************************************************************/
/* FIGURE OUT WHAT TO DO WHEN ESCAPING OUT OF TEXT MODE                       */
/******************************************************************************/
char
edb_cmd_dispatch(/* ARGUMENTS:          */
    int argc,    /* Number of arguments */
    char **argv  /* Argument list       */
)
{
	/* Command dispatch */
	if (match(argv[0], "n_on") || match(argv[0], "nop_e")) {
		/* std::println("Response aborted!  Returning to current {}.",
		 * topic()); */
		mode = oldmode;
	} else if (match(argv[0], "y_es") || match(argv[0], "ok")) {
		post = 1;
		mode = oldmode;
	} else if (match(argv[0], "ed_it"))
		text_edit(argc, argv);
	else if (match(argv[0], "ag_ain") || match(argv[0], "c_ontinue")) {
		mode = M_TEXT;
		if (!(flags & O_QUIET)) {
			std::println("(Continue your text entry)");
			std::println(R"(Type "." to exit or ":help".)");
		}
	} else if (match(argv[0], "pr_int"))
		text_print(argc, argv);
	else if (match(argv[0], "em_pty") || match(argv[0], "cl_ear"))
		text_clear(argc, argv);
	else
		return misc_cmd_dispatch(argc, argv);
	return 1;
}
