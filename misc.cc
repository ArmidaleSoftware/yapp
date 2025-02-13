// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include "misc.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <time.h>
#include <unistd.h>

#include <cstdio>
#include <format>
#include <iostream>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include "arch.h"
#include "change.h"
#include "config.h"
#include "driver.h"
#include "edbuf.h"
#include "files.h"
#include "globals.h"
#include "help.h"
#include "item.h"
#include "lib.h"
#include "log.h"
#include "macro.h"
#include "main.h"
#include "mem.h"
#include "options.h"
#include "range.h"
#include "sep.h"
#include "str.h"
#include "struct.h"
#include "sum.h"
#include "system.h"
#include "user.h"
#include "www.h"
#include "yapp.h"

/* Misc. commands available at all modes */
static dispatch_t misc_cmd[] = {
    {"chfn",          chfn          },
    {"newuser",       newuser       },
    {"passw_d",       passwd        },
    {"arg_set",       argset        },
    {"www_parsepost", www_parse_post},
    {"url_encode",    url_encode    },
    {"if",            do_if         },
    {"else",          do_else       },
    {"endif",         do_endif      },
    {"for_each",      foreach       },
    {"c_hange",       change        },
    {"se_t",          change        },
    {"?",             help          },
    {"h_elp",         help          },
    {"exp_lain",      help          },
    {"al_ias",        define        },
    {"def_ine",       define        },
    {"una_lias",      define        },
    {"und_efine",     define        },
    {"const_ant",     define        },
    {"log",           logevent      },
    {"ec_ho",         echo          },
    {"echoe",         echo          },
    {"echon",         echo          },
    {"echoen",        echo          },
    {"echone",        echo          },
    {"m_ail",         mail          },
    {"t_ransmit",     mail          },
    {"sen_dmail",     mail          },
    {"d_isplay",      display       },
    {"que_ry",        display       },
    {"sh_ow",         display       },
    {"load",          load_values   },
    {"save",          save_values   },
    {"t_est",         test          },
    {"rm",            do_rm         },
    {"cd",            cd            },
    {"chd_ir",        cd            },
    {"uma_sk",        do_umask      },
    {"cdate",         date          },
    {"da_te",         date          },
/* "clu_ster",  cluster,  */
#ifdef INCLUDE_EXTRA_COMMANDS
    {"cfdir",         set_cfdir     },
#endif
    {"eval_uate",     eval          },
    {"evali_n",       eval          },
    {"source",        do_source     },
    {"debug",         set_debug     },
    {"cf_list",       do_cflist     },
    /* ex_it q_uit st_op good_bye log_off log_out h_elp exp_lain sy_stem unix
     * al_ias def_ine una_lias und_efine ec_ho echoe echon echoen echone so_urce
     * m_ail t_ransmit sen_dmail chat write d_isplay que_ry p_articipants
     * w_hoison am_superuser resign chd_ir uma_sk sh_ell f_iles dir_ectory ty_pe
     * e_dit cdate da_te t_est clu_ster
     */
    {0,               0             },
};
/******************************************************************************/
/* DISPATCH CONTROL TO APPROPRIATE MISC. COMMAND FUNCTION                     */
/******************************************************************************/
char
misc_cmd_dispatch(/* ARGUMENTS:                 */
    int argc,     /* Number of arguments     */
    char **argv   /* Argument list           */
)
{
    for (auto i = 0; misc_cmd[i].name; i++)
        if (match(argv[0], misc_cmd[i].name))
            return misc_cmd[i].func(argc, argv);

    /* Command dispatch */
    if (match(argv[0], "q_uit") || match(argv[0], "st_op") ||
        match(argv[0], "ex_it")) {
        status |= S_STOP;
        return 0;
    } else if (match(argv[0], "unix_cmd")) {
        if (argc < 2)
            std::println("syntax: unix_cmd \"command\"");
        else {
            std::string cmd;
            std::string sep;
            const auto begin = argv + 1;
            const auto end = argv + argc;
            for (auto it = begin; it != end; it++, sep = " ") {
                cmd.append(sep);
                cmd.append(noquote(*it));
                // sep = " ";
            }

            /*
             * Undone at request of sno and jep
             * if (mode==M_SANE)
             *	std::println("{} rc cannot exec: {}",
             *	    Conference(), buff);
             * else
             */
            unix_cmd(cmd);
        }
    } else if (argc) {
        char *p;
        /* Check for commands of the form:
         * variable=value */
        p = strchr(argv[0], '=');
        if (p || (argc > 1 && argv[1][0] == '=')) {
            char *val; /* Arbitrary length value */
            int i, vallen;
            /* Compute max vallen */
            if (p) {
                vallen = strlen(p + 1) + 1;
                i = 1;
            } else {
                i = 2;
                vallen = strlen(argv[1] + 1) + 1;
            }
            while (i < argc) {
                if (vallen > 1)
                    vallen++;
                vallen += strlen(argv[i++]);
            }

            /* Compose val */
            val = (char *)emalloc(vallen);
            if (p) {
                *p = '\0';
                strcpy(val, p + 1);
                i = 1;
            } else {
                strcpy(val, argv[1] + 1);
                i = 2;
            }
            while (i < argc) {
                if (val[0])
                    strcat(val, " ");
                strcat(val, argv[i++]);
            }

            /* Execute command */
            def_macro(argv[0], DM_VAR, val);
            free(val);

        } else {
            std::println("Invalid command: {}", argv[0]);
        }
    }
    return 1;
}
/******************************************************************************/
/* SET UMASK VALUE                                                            */
/******************************************************************************/
int
do_umask(       /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    int i;
    if (argc < 2) {
        umask(i = umask(0));
        std::println("{:03o}", i);
    } else if (!isdigit(argv[1][0])) {
        std::println("Bad umask \"{}\"specified (must be octal)", argv[1]);
    } else {
        sscanf(argv[1], "%o", &i);
        umask(i);
    }
    return 1;
}
/******************************************************************************/
/* SEND MAIL TO ANOTHER USER                                                  */
/******************************************************************************/
int
mail(           /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    if (argc < 2) {
        unix_cmd("mail");
    } else if (flags & O_MAILTEXT) {
        unix_cmd(str::concat({"mail ", argv[1]}));
    } else {
        /* dont clear buffer, for reply cmd */
        const auto cfbuffer = str::concat({work, "/cf.buffer"});
        if (text_loop(0, "mail")) {
            std::string to(argv[1]);
            while (!to.empty()) {
                unix_cmd(std::format("mail {} < {}", to, cfbuffer));
                std::println("Mail sent to {}.", to);
                if (!(flags & O_QUIET))
                    std::println("More recipients (or <return>)? ");
                ngets(to, st_glob.inp);
            }
        }
        rm(cfbuffer, SL_USER);
    }
    return 1;
}

int
do_rm(          /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    int i;
    if (argc < 2) {
        std::println("Usage: rm filename ...");
        return 2;
    }
    for (i = 1; i < argc; i++) {
        if (rm(argv[i], SL_USER)) {
            if (!(flags & O_QUIET))
                error("removing ", argv[1]);
        }
    }
    return 1;
}
/******************************************************************************/
/* CHANGE CURRENT WORKING DIRECTORY                                           */
/******************************************************************************/
int
cd(             /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    if (argc > 2) {
        std::println("Bad parameters near \"{}\"", argv[2]);
        return 2;
    } else if (chdir((argc > 1) ? argv[1] : home.c_str()))
        error("cd'ing to ", argv[1]);
    return 1;
}
/******************************************************************************/
/* ECHO ARGUMENTS TO OUTPUT                                                   */
/******************************************************************************/
int
echo(           /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    short i;
    FILE *fp;
    /* If `echo...` but not `echo...|cmd`  (REDIRECT bit takes precedence)
     */
    if ((status & S_EXECUTE) && !(status & S_REDIRECT)) {
        fp = NULL;
    } else if (match(argv[0], "echoe") || match(argv[0], "echoen") ||
               match(argv[0], "echone")) {
        fp = stderr;
    } else {
        if (status & S_REDIRECT) {
            fp = stdout;
        } else {
            open_pipe();
            fp = st_glob.outp;
            if (!fp) {
                fp = stdout;
            }
        }
    }

    for (i = 1; i < argc; i++) {
        wfputs(argv[i], fp);
        if (i + 1 < argc)
            wfputc(' ', fp);
    }
    if (!strchr(argv[0], 'n'))
        wfputc('\n', fp);

    if (fp)
        fflush(fp); /* flush when done with wfput stuff */
    return 1;
}
/******************************************************************************/
/* LOAD VALUES                                                                */
/******************************************************************************/
int
load_values(int argc, char **argv)
{
    int i;
    char buff[MAX_LINE_LENGTH];
    int suid;
    const auto userfile = get_userfile(login, &suid);
#ifdef HAVE_DBM_OPEN
    for (i = 1; i < argc; i++) {
        const auto value = get_dbm(userfile, argv[i], SL_USER);
        if (buff[0])
            def_macro(argv[i], DM_VAR, value);
        else
            undef_name(argv[i]);
    }
#endif
    return 1;
}
/******************************************************************************/
/* SAVE VALUES                                                                */
/******************************************************************************/
int
save_values(    /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    int i;
    int suid;
    const auto userfile = get_userfile(login, &suid);
#ifdef HAVE_DBM_OPEN

    for (i = 1; i < argc; i++) {
        const auto var = expand(argv[i], DM_VAR);
        if (!var.empty() && !save_dbm(userfile, argv[i], var, SL_USER)) {
            error("modifying userfile ", userfile);
            return 1;
        }
    }
#endif
    return 1;
}
/******************************************************************************/
/* CHECK THE DATE                                                             */
/******************************************************************************/
int
date(           /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    std::vector<std::string> args;
    for (auto i = 0; i < argc; i++) args.push_back(argv[i]);
    time_t t = since(args);
    if (t < LONG_MAX) {
        if (argv[0][0] == 'c')
            std::println("{:X}", t); /* cdate command */
        else
            std::println("\nDate is: {}", get_date(t, 13));
    }

    return 1;
}

/******************************************************************************/
/* SOURCE A FILE OF COMMANDS                                                  */
/******************************************************************************/
int
do_source(      /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    short i;
    int ok = 1;
    if (argc < 2 || argc > 20)
        std::println("usage: source filename [arg ...]");
    else {
        for (i = 1; i < argc; i++) {
            const auto arg = std::format("arg{}", i - 1);
            def_macro(arg, DM_VAR, argv[i]);
        }

        ok = source(argv[1], "", 0, SL_USER);
        if (!ok && !(flags & O_QUIET))
            std::println("Cannot access {}", argv[1]);

        for (i = 1; i < argc; i++) {
            const auto arg = std::format("arg{}", i - 1);
            if (find_macro(arg, DM_VAR))
                undef_name(arg);
        }
    }
    return (ok < 0) ? 0 : 1;
}
/******************************************************************************/
/* TEST RANGES                                                                */
/******************************************************************************/
int
test(           /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    int i;
    char act[MAX_ITEMS];
    short j, k = 0, fl = 0;
    rangeinit(&st_glob, act);
    refresh_sum(0, confidx, sum, part, &st_glob);

    if (argc > 1) /* Process argc */
        range(argc, argv, &fl, act, sum, &st_glob, 0);
    if (!(fl & OF_RANGE)) {
        std::println("Error, no {} specified! (try HELP RANGE)", topic());
        return 1;
    }
    j = A_SKIP;
    for (i = 0; i < MAX_ITEMS; i++) {
        if (act[i] == A_SKIP && j == A_COVER)
            std::print("{}]", i);
        if (act[i] == A_FORCE)
            std::print("{}{}", (k++) ? "," : "", i + 1);
        if (act[i] == A_COVER && j != A_COVER)
            std::print("{}[{}-", (k++) ? "," : "", i + 1);
        j = act[i];
    }
    if (j == A_COVER)
        std::print("{}]", i);
    std::println(".");
    std::println("newflag: {}", fl);
    std::print("since  date is {}", ctime(&(st_glob.since)));
    std::print("before date is {}", ctime(&(st_glob.before)));
    if (!st_glob.string.empty())
        std::println("String is: {}", st_glob.string);
    if (!st_glob.author.empty())
        std::println("Author is: {}", st_glob.author);

    return 1;
}

static void
print_dopt(const option_t &opt)
{
    std::println(
        "{} {}", compress(opt.name), (debug & opt.mask) != 0 ? "on" : "off");
}

int
set_debug(      /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    if (argc < 2) {
        for (const auto &dopt : debug_opt) print_dopt(dopt);
        return 1;
    }
    for (auto j = 1; j < argc; j++) {
        if (str::eq(argv[j], "off")) {
            debug = 0;
            continue;
        }
        for (const auto &dopt : debug_opt)
            if (match(argv[j], dopt.name)) {
                debug ^= dopt.mask;
                print_dopt(dopt);
            }
    }
    return 1;
}

#ifdef INCLUDE_EXTRA_COMMANDS
int
set_cfdir(      /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    (void)argc;
    (void)argv;

    if (!(flags & O_QUIET)) {
        std::print("User name: ");
        std::fflush(stdout);
    }
    std::string buff;
    ngets(buff, st_glob.inp);
    const auto path = str::join("/", {home, buff});
    if (access(path.c_str(), X_OK))
        std::println("No such directory.");
    else
        work = path;
    return 1;
}
#endif

/* PROCESS A NEW SEP (of arbitrary length) */
int
eval(           /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    int i;
    char act[MAX_ITEMS], *str;
    short i_s, i_i, i_lastseen, shown = 0, rfirst = 0;
    FILE *fp;
    status_t tmp;
    /* Save state */
    memcpy(&tmp, &st_glob, sizeof(status_t));

    refresh_sum(0, confidx, sum, part, &st_glob);
    for (i = 0; i < MAX_ITEMS; i++) act[i] = 0;
    st_glob.string.clear();
    st_glob.since = st_glob.before = st_glob.r_first = 0;
    st_glob.opt_flags = 0;

    if (argc < 2) {

        /* Read from stdin until EOF */
        while ((str = xgets(st_glob.inp, stdin_stack_top)) != NULL) {
            if (mode == M_OK || mode == M_JOQ)
                confsep(str, confidx, &st_glob, part, 0);
            else if (mode == M_RFP || mode == M_EDB || mode == M_TEXT)
                itemsep(str, 0);
            else
                std::println("Unknown mode");
            free(str);
        }
        if (debug & DB_IOREDIR)
            std::println("Detected end of eval input");
        return 1;
    }
    if (match(argv[0], "evali_n")) {
        str = NULL;
        range(argc, argv, &st_glob.opt_flags, act, sum, &st_glob, 0);
    } else {
        str = argv[argc - 1];

        /* Strip quotes */
        if (str[0] == '"') {
            str++;
            if (str[strlen(str) - 1] == '"')
                str[strlen(str) - 1] = '\0';
        }
        if (argc > 2) /* Process argc */
            range(argc - 1, argv, &st_glob.opt_flags, act, sum, &st_glob, 0);
    }

    open_pipe();

    /* Transfer current pipe info to global saved state */
    tmp.outp = st_glob.outp;

    if (str && !(st_glob.opt_flags & OF_RANGE)) {

        if (mode == M_OK || mode == M_JOQ)
            confsep(str, confidx, &st_glob, part, 0);
        else if (mode == M_RFP || mode == M_EDB || mode == M_TEXT)
            itemsep(str, 0);
        else
            std::println("Unknown mode");

    } else {

        if (mode == M_OK) {

            /* Removed 4/11/96 because eval 4 'christmas'
             * "%{nextitem}" wasn't processing the search string.
             *       st_glob.string.clear();
             */
            if (st_glob.opt_flags & OF_REVERSE) {
                i_s = st_glob.i_last;
                i_i = -1;
            } else {
                i_s = st_glob.i_first;
                i_i = 1;
            }

            /* Process items */
            sepinit(IS_START);
            fp = NULL;
            for (st_glob.i_current = i_s;
                st_glob.i_current >= st_glob.i_first &&
                st_glob.i_current <= st_glob.i_last && !(status & S_INT);
                st_glob.i_current += i_i) {
                if (cover(st_glob.i_current, confidx, st_glob.opt_flags,
                        act[st_glob.i_current - 1], sum, part, &st_glob)) {
                    st_glob.i_next = nextitem(1);
                    st_glob.i_prev = nextitem(-1);
                    st_glob.r_first = rfirst;

                    if (match(argv[0], "evali_n")) {
                        while ((str = xgets(st_glob.inp, stdin_stack_top)) !=
                               NULL) {
                            itemsep(str, 0);
                            free(str);
                        }
                    } else
                        itemsep(str, 0);
                    shown++;
                }
            }
            if (!shown && (st_glob.opt_flags & (OF_BRANDNEW | OF_NEWRESP))) {
                wputs(std::format("No new {}s matched.\n", topic()));
            }
        } else if (mode == M_RFP) {
            /* Open file */

            const auto path = std::format(
                "{}/_{}", conflist[confidx].location, st_glob.i_current);

            fp = mopen(path, O_R);
            if (fp == nullptr)
                return 1;

            i_lastseen = st_glob.i_current - 1;
            if (st_glob.opt_flags & (OF_NEWRESP | OF_NORESPONSE))
                st_glob.r_first = part[i_lastseen].nr;
            else if (st_glob.since) {
                st_glob.r_first = 0;
                while (st_glob.since > re[st_glob.r_first].date) {
                    st_glob.r_first++;
                    get_resp(fp, &(re[st_glob.r_first]), (short)GR_HEADER,
                        st_glob.r_first);
                    if (st_glob.r_first >= sum[i_lastseen].nr)
                        break;
                }
            }
            st_glob.r_last = MAX_RESPONSES;
            st_glob.r_max = sum[i_lastseen].nr - 1;

            /* For each response */
            for (st_glob.r_current = st_glob.r_first;
                st_glob.r_current <= st_glob.r_last &&
                st_glob.r_current <= st_glob.r_max && !(status & S_INT);
                st_glob.r_current++) {
                get_resp(fp, &(re[st_glob.r_current]), (short)GR_HEADER,
                    st_glob.r_current);
                itemsep(str, 0);
            }

            mclose(fp);
        } else
            std::println("bad mode");
    }

    /* Restore state */
    {
        FILE *inp = st_glob.inp;
        memcpy(&st_glob, &tmp, sizeof(status_t));
        st_glob.inp = inp;
    }
    /*
    std::println("eval: fdnext={}", fdnext());
    if (!fdnext())
       abort();
    */

    return 1;
}

/*****************************************************************************/
/* FUNCTIONS FOR CONDITIONAL EXPRESSIONS                                     */
/*****************************************************************************/
static int if_stat[100], if_depth = -1;

int
test_if(void)
{
    if (if_depth < 0)
        if_stat[++if_depth] = 1;
    return if_stat[if_depth];
}

int
do_if(          /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    char buff[MAX_LINE_LENGTH];
    const char *sp = buff;

    if (!test_if()) {
        if_stat[++if_depth] = 0;
        return 1;
    }

    /* Put buffer together */
    buff[0] = '\0';
    for (int i = 1; i < argc; i++) strlcat(buff, argv[i], sizeof(buff));

    if (if_depth == 99) {
        std::println("Too many nested if's");
        return 0;
    }

    init_show();
    switch (mode) {
    case M_RFP:
    case M_TEXT:
    case M_EDB:
        if_stat[++if_depth] = itemcond(&sp, st_glob.opt_flags);
        break;

    case M_OK:
    case M_JOQ:
    default:
        if_stat[++if_depth] = confcond(&sp, confidx, &st_glob);
        break;
    }

    return 1;
}

int
do_endif(       /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    (void)argc;
    (void)argv;
    if (if_depth)
        if_depth--;
    else
        std::println("Parse error: endif outside if construct");
    return 1;
}

int
do_else(        /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    (void)argc;
    (void)argv;
    if (!if_depth)
        std::println("Parse error: else outside if construct");
    else if (if_stat[if_depth - 1])
        if_stat[if_depth] = !if_stat[if_depth];
    return 1;
}

/* ARGUMENTS:             */
/* Number of arguments */
/* Argument list       */
int foreach (int argc, char **argv)
{
    if (argc != 6) {
        std::println("usage: foreach <varname> in <listvar> do \"command\"");
        return 1;
    }

    /* Get list */
    const auto vars = str::split(expand(argv[3], DM_VAR), ", ");
    for (const auto &var : vars) {
        def_macro(argv[1], DM_VAR, var);
        command(argv[5], 1);
    }

    return 1;
}

/* ARGUMENTS:             */
/* Number of arguments */
/* Argument list       */
int
argset(int argc, char **argv)
{
    if (argc < 3) {
        /*std::println("usage: argset delimeter string");*/
        def_macro("argc", DM_VAR, "0");
        return 1;
    }

    const auto setargs = str::split(argv[2], argv[1], false);

    /*std::println("pi=!{}! fields={}", argv[2], setargs.size());*/

    // XXX: really want `std::views::enumerate` here.
    for (auto i = 0uz; i < setargs.size(); ++i) {
        const auto &arg = setargs[i];
        const auto name = "arg" + std::to_string(i);
        def_macro(name, DM_VAR, arg);
    }
    def_macro("argc", DM_VAR, std::to_string(setargs.size()));

    return 1;
}
