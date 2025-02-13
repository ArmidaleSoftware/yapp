// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#pragma once

/* The following 5 defines are all that you should need to configure the bbs
 * conferencing system for your machine:
 * BBSDIR is the main subdirectory under which the whole system exists.  On
 * M-net it is /usr/bbs.  The 'conflist' and main 'rc' files should exist there.
 * MAILDIR should be set to the directory which contains the user mailbox files.
 * SENDMAIL should be the full pathname of sendmail
 */

/* Config param defaults: */
#define BBSDIR "/arbornet/m-net/bbs"
#define MAILDIR "/var/mail"
#define NEWSDIR "/var/news"
#define SENDMAIL "/usr/sbin/sendmail"

/* Password file containing encrypted passwords, only readable by cfadm */
#define PASS_FILE "/arbornet/m-net/bbs/etc/.htpasswd"
#define USER_FILE "/arbornet/m-net/bbs/etc/passwd"
#define USER_HOME "/arbornet/m-net/bbs/home"
#define LICENSEDIR "/arbornet/m-net/bbs/license"
#define NOBODY "nobody" /* needed for HP-UX */
#define CFADM "cfadm"   /* to send official email from */
#define FREEZE_LINKED "true"
#define CENSOR_FROZEN "true"
#define PADDING "0"
#define VERIFY_EMAIL "false"
#define USERDBM "false"

/* Define BYTESWAP to be 1 if you want a consistent sum file format.  This
 * is useful on networks of machines of differing architectures.  The only
 * reason to change this is that on some machines, if you using YAPP with
 * other programs that access the same files, the other programs may not
 * do byteswapping (PicoSpan does).  In this case, define BYTESWAP to be 0.
 */
#define BYTESWAP "1"

#define MAX_LINE_LENGTH 1024uz  /* Buffer size for a line of text */
#define MAX_FILE_NAME_LENGTH 80 /* Length of a file name          */
#define MAX_STRING_LENGTH 64    /* String length of our language  */
#define MAX_IDENT_LENGTH 32     /* Identifier length in our lang  */
#define CMD_DEPTH 20
#define MAX_ARGS 128
#define STDIN_STACK_SIZE 20	/* # nested input redirections    */

#define MAX_RESPONSES 2000	/* # of responses/item            */
#define MAX_ITEMS 6000		/* # of items/conf                */

/* Default variable values */
#define PAGER "/usr/bin/less"
#define EDITOR "/bin/ed"
#define SHELL "/bin/sh"
#define PROMPT "\nYAPP: "
#define NOCONFP                                                                \
	"\n\
Type HELP CONFERENCES for a list of ${conference}s.\n\
Type JOIN <NAME> to access a ${conference}.\n\
YAPP: "
#define RFPPROMPT "\n[%{curitem}/%l] Respond, forget, or pass? "
#define OBVPROMPT "\n[%{curitem}/%l] Can't respond, pass? "
#define JOQPROMPT "\nJoin, quit, or help? "
#define EDBPROMPT "Ok to enter this response? "
#define SCRIBOK "Ok to scribble this response? "
#define TEXT ">" /* No separators allowed for this prompt only */
#define ESCAPE ":"

#define CONFERENCE "conference"
#define ITEM "item"
#define FAIRWITNESS "fairwitness"
#define SUBJECT "subject"

/* : , / -  ( ) +        are used in dates               */
/* ! > | ` ~             are used for unix commands      */
/* < > % { }             are used for separators         */
/* | &                   are reserved for future expansion for separators */
/* .                     is used to specify responses    */
/* #                     is used for comments            */
/* _                     is used for specifying commands */
/* = ^ $                 is used for range specs         */
/* " '                   are used for strings            */
/* @ * ? [ ] ;           are unused                      */
#define CMDDEL ";"
#define BUFDEL ";"

#define GECOS "," /* (char *)0 */
#define NSEP "\n%(N%r new of %)%zn response%S total."
#define Iint "%(B$^{item} Resps $^{subject}\n\n%)%4i %5n %h"
#define ISEP "\n$^{item} %i entered %d by %a (%l)\n %h"
#define RSEP "\n#%i.%r %a (%l) "
#define FSEP "%(R#%i.%r %a (%l)\n%)%7N: %L"
#define TXTSEP "%(L%(F%7N:%)%X%L%)"
#define ZSEP "%(T^L%)%c"
#define CENSORED "   <censored>"
#define SCRIBBLED "   <censored & scribbled>"
#define PRINTMSG "%QPrinted from ${conference} %d (%s)"
#define MAILMSG "You have %(2xmore %)mail."
#define LINMSG                                                                 \
	"\
%(1x%(2x\n%)%0g%)\
%(2x%(3N%3g%)\
%(1x\n%)%(n%(r%r newresponse ${item}%S%)\
%(b%(r and %)%b brandnew ${item}%S%)\n%)\
%(i%i ${item}%S numbered %f-%l\n%)\
%(sYou are a ${fairwitness} in this ${conference}.\n%)\
%(lYou are an observer of this ${conference}.\n%)\
%(jYour name is \"%u\" in this ${conference}.\n\
%(~O%(4F\n%4g%E\n>>>> New users: type HELP for help.\n%)%)%)%)%c"
#define LOUTMSG "%(1x%1g%)%c"
#define INDXMSG "%(1x%2g%)%c"
#define BULLMSG "%(1x%3g%)%c"
#define WELLMSG "%(1x%4g%)%c"
#define PARTMSG                                                                \
	"\
%(2x%10v %o %u%)\
%(4x\n%k participant%S total.%)\
%(1x\n     login           last time on      name\n%)"
#define CHECKMSG                                                               \
	"%(1x\nNew resp ${item}s  $^{conference} name\n%)%(2x%(k--%E  "        \
	"%)%(B>%E %)%4r %4b    %s%(B  (where you currently are!)%)%)"
#define GROUPINDEXMSG "                    **%C**"
#define CONFINDEXMSG "%20s.....%C"
#define CONFMSG                                                                \
	"%Q\
${conference} name   : %s\n\
Directory         : %d\n\
Participation file: %w/%p\n\
Security type     : %t\n\
%(i%i ${item}%S numbered %f-%l%ENo ${item}s yet.%)"
#define JOINMSG                                                                \
	"Join: which ${conference}?\n\
%QYou are currently in the %s ${conference}."
#define LISTMSG                                                                \
	"\
%(1x\n${item}s sec time                     ${conference}\n%)\
%(2x%(B>%E %)%4i %2t%(sF%E%(lR%E %)%) %m %s%)"
#define REPLYSEP                                                               \
	"\
%(1xIn #%i.%r of the %C ${conference}, you write:%)\
%(2x> %L%)\
%(4x%)"
#ifdef NEWS
#define NEWSSEP                                                                \
	"\
%(1xIn article <%m>, %a writes:%)\
%(2x> %L%)\
%(4x%)"
#endif

/* DEFAULT_PSEUDO is the value of the user's fullname in the conference
 * which means to always mirror the user's global fullname.
 */
#define DEFAULT_PSEUDO "none"

/* Option flags */
/*#define O_DEBUG         0x0001         * Turn all debugging options on  */
#define O_BUFFER 0x0001      /* Don't use a pager              */
#define O_DEFAULT 0x0002     /* Go to noconf mode at start     */
#define O_OBSERVE 0x0004     /* Force observer status          */
#define O_STRIP 0x0008       /* Turn strip flag on             */
#define O_SOURCE 0x0010      /* Don't source .cfrc and CONF/rc */
#define O_INCORPORATE 0x0020 /* Incorporate                    */

#define O_STAY 0x0040          /* Stay on curr item after resp   */
#define O_DOT 0x0080           /* '.' won't end text entry       */
#define O_EDALWAYS 0x0100      /* Go directly to editor for text */
#define O_METOO 0x0200         /* */
#define O_NUMBERED 0x0400      /* Number text in responses       */
#define O_DATE 0x0800          /* Display dates on items         */
#define O_UID 0x1000           /* Display uids on items          */
#define O_MAILTEXT 0x2000      /* Display uids on items          */
#define O_AUTOSAVE 0x4000      /* Display uids on items          */
#define O_VERBOSE 0x00008000   /* Display commands in rc files   */
#define O_SCRIBBLER 0x00010000 /* Display login of scribbler     */
#define O_SIGNATURE 0x00020000 /* Display signiture of authors   */
#define O_READONLY 0x00040000  /* Force noresponse status        */
#define O_FORGET 0x00080000    /* */
#ifdef INCLUDE_EXTRA_COMMANDS
#define O_CGIBIN 0x00100000 /* "WWW Server via CGI" mode      */
#endif
#define O_QUIET 0x00200000     /* Don't display login/logout msg */
#define O_SENSITIVE 0x00400000 /* Censored items come up new     */
#define O_UNSEEN 0x00800000    /* Only 1st & last are new        */
#define O_AUTOJOIN 0x01000000  /* Skip joq prompt and just join  */
#define O_LABEL 0x02000000     /* Display extra IS_ALL labels    */

/* Debug flags */
#define DB_MEMORY 0x0001  /* Xalloc module */
#define DB_CONF 0x0002    /* conf module */
#define DB_MACRO 0x0004   /* macro module */
#define DB_RANGE 0x0008   /* range module */
#define DB_DRIVER 0x0010  /* driver module */
#define DB_FILES 0x0020   /* files module */
#define DB_PART 0x0040    /* joq module */
#define DB_ARCH 0x0080    /* arch module */
#define DB_LIB 0x0100     /* lib module */
#define DB_SUM 0x0200     /* sum module */
#define DB_ITEM 0x0400    /* item module */
#define DB_USER 0x0800    /* user module */
#define DB_PIPE 0x1000    /* file descriptors */
#define DB_IOREDIR 0x2000 /* standard I/O redirection */

/* Status flags */
#define S_INT 0x0001      /* Interrupt hit? */
#define S_PIPE 0x0008     /* In a pipe? */
#define S_QUIT 0x0010     /* Abort join */
#define S_MAIL 0x0020     /* User has mail? */
#define S_MOREMAIL 0x0400 /* User has MORE mail? */
#define S_EXECUTE 0x0800  /* Executing a `command` */
#define S_PAGER 0x1000    /* Pager is in use? */
#define S_STOP 0x2000     /* Stop processing more ; cmds */
#define S_SOCKET 0x4000
#define S_BATCH 0x8000 /* Batch mode, ignore blank lines */
#define S_REDIRECT 0x10000
#define S_NOSTDIN 0x20000 /* stdin is redirected */
#define S_NOAUTH 0x40000  /* Not yet authenticated */

/* Conference status flags */
#define CS_OBSERVER 0x0002   /* you're an observer in this cf */
#define CS_FW 0x0004         /* you're a fairwitness of this cf */
#define CS_NORESPONSE 0x0080 /* Response not possible */
#define CS_OTHERCONF 0x0100  /* you were in another cf */
#define CS_JUSTJOINED 0x0200 /* you just joined this cf */

/* Input Modes */
#define M_OK 0   /* Ok:              */
#define M_RFP 1  /* Respond or pass? */
#define M_TEXT 2 /* >                */
#define M_JOQ 3  /* Join or quit     */
#define M_EDB 4  /* Ok to enter this response? */

/* Conference type flags */
#define CT_OLDPUBLIC 0x00 /* 0000 */
#define CT_PASSWORD 0x05  /* 0101 */
#define CT_PRESELECT 0x04 /* 0100 */
#define CT_PARANOID 0x06  /* 0110 */
#define CT_PUBLIC 0x08    /* 1000 */
#define CT_READONLY 0x10  /* 10000 */
#define CT_BASIC 0x0F     /* 01111 */
#ifdef NEWS
#define CT_NEWS 0x20 /* 00100000 */
#endif
#define CT_EMAIL 0x40       /* 01000000 */
#define CT_REGISTERED 0x80  /* 10000000 only registered users can post */
#define CT_NOENTER 0x100    /* only hosts can enter new items */
#define CT_ORIGINLIST 0x200 /* limit users by origin host */
#define CT_VISIBLE (~(CT_ORIGINLIST))

/* Config file fields */
#define CF_MAGIC 0
#define CF_PARTFILE 1
#define CF_TIMELIMIT 2
#define CF_FWLIST 3
#define CF_SECURITY 4
#ifdef NEWS
#define CF_NEWSGROUP 5
#endif
#define CF_EMAIL 5

/* Item flags */
#define IF_FORGOTTEN 0x0001
#define IF_RETIRED 0x0002
#define IF_ACTIVE 0x0030
#define IF_FROZEN 0x0040
#define IF_PARTY 0x0080
#define IF_LINKED 0x0100
#define IF_EXPIRED 0x0200
#define IF_SAVEMASK 0xFFFE /* Save only these flags */

/* Entity types */
#define ET_INTEGER 0
#define ET_STRING 1

/* STDIN types */
#define STD_SKIP 0
#define STD_TTY 1
#define STD_FILE 2
#define STD_SFILE 3
#define STD_SPIPE 4
#define STD_TYPE 0xF
#define STD_SANE 16
#define STD_SUPERSANE 32

/*
 *  Key words to look up names of important fields
 *  and database and table names
 */

#define F_LOGIN "login"
#define F_PASSWORD "password"
#define F_EMAIL "email"
#define F_FULLNAME "fullname"
#define F_TABLENAME "tablename"
#define F_DATABASE "database"
