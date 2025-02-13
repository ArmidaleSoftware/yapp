// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include "macro.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cstdint>
#include <functional>
#include <iostream>
#include <optional>
#include <print>
#include <ranges>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "conf.h"
#include "driver.h"
#include "globals.h"
#include "lib.h"
#include "license.h"
#include "mem.h"
#include "range.h"
#include "security.h"
#include "stats.h"
#include "str.h"
#include "struct.h"
#include "user.h"
#include "yapp.h"

/* Message-Id: <%14D.%{uid}@%{hostname}>\n */
#define MAILHEADER                                                             \
    "\
Date: %15D\n\
From: \"%{fullname}\" <%{email}>\n\
Subject: %(pRe: %)%h\n\
Message-Id: <%14D.%{uid}@%{hostname}>\n\
To: %{address}\n\
%(rIn-Reply-To: <%m> from \"%a\" at %d\n%)"

#define MAILSEP                                                                \
    "\
%(1x%a writes:%)\
%(2x> %L%)\
%(4x%)"

extern int exit_status; // XXX

namespace
{

// The current set of macros.
// XXX: Should be a trie.
std::vector<macro_t> macros;

const std::unordered_set<std::string_view> rovars{
    "fullname",
    "login",
    "uid",
    "work",
    "home",
    "hostname",
    "address",
    "mailheader",
    "pathinfo",
    "requestmethod",
    "pid",
    "exit",
    "version",
    "status",
    "mode",
    "lowresp",
    "fwlist",
    "remotehost",
    "querystring",
    "confname",
    "bbsdir",
    "wwwdir",
    "sysop",
    "cflist",
    "conflist",
    "fromlogin",
    "cgidir",
    "euid",
    "nobody",
    "partdir",
    "remoteaddr",
    "racl",
    "wacl",
    "cacl",
    "aacl",
    "cursubj",
    "hitstoday",
    "newresps",
    "confdir",
    "isnew",
    "isbrandnew",
    "isnewresp",
    "canracl",
    "canwacl",
    "cancacl",
    "canaacl",
    "userfile",
    "verifyemail",
    "userdbm",
    "ticket",
};

inline std::string
getenv_or(const char *name, const std::string_view &def)
{
    if (const char *val = getenv(name); val != nullptr)
        return val;
    return std::string(def);
}

inline std::string
getconf_or(const std::size_t ci, const std::string_view &def)
{
    const auto config = get_config(confidx);
    if (config.size() > ci)
        return config[ci];
    return std::string(def);
}

const std::unordered_map<std::string_view, std::function<std::string()>>
    defaults{
        {"aacl",
         [] {
                load_acl(confidx);
                return acl_list[CHACL_RIGHT];
            }                                                                  },
        {"alpha",         [] { return getenv_or("ALPHA", ""); }                },
        {"address",       [] { return getconf_or(CF_EMAIL, ""); }              },
        {"beta",          [] { return getenv_or("BETA", ""); }                 },
        {"bufdel",        [] { return BUFDEL; }                                },
        {"bullmsg",       [] { return BULLMSG; }                               },
        {"brandnew",      [] { return std::to_string(st_glob.i_brandnew); }    },
        {"bbsdir",        [] { return get_conf_param("bbsdir", BBSDIR); }      },
        {"conference",    [] { return CONFERENCE; }                            },
        {"cmddel",        [] { return CMDDEL; }                                },
        {"censored",      [] { return CENSORED; }                              },
        {"checkmsg",      [] { return CHECKMSG; }                              },
        {"confindexmsg",  [] { return CONFINDEXMSG; }                          },
        {"confmsg",       [] { return CONFMSG; }                               },
        {"curitem",       [] { return std::to_string(st_glob.i_current); }     },
        {"curresp",       [] { return std::to_string(st_glob.r_current); }     },
        {"curline",       [] { return std::to_string(st_glob.l_current); }     },
        {"cfadm",         [] { return get_conf_param("cfadm", CFADM); }        },
        {"cflist",        [] { return cfliststr; }                             },
        {"cursubj",
         [] {
                if (confidx >= 0 && st_glob.i_current <= st_glob.i_last)
                    return get_subj(confidx, st_glob.i_current - 1, sum);
                return "";
            }                                                                  },
        {"confname",
         [] {
                return confidx < 0 ? "noconf"
                                   : compress(conflist[confidx].name);
            }                                                                  },
        {"confdir",
         [] {
                return (confidx >= 0) ? conflist[confidx].location : "noconf";
            }                                                                  },
        {"conflist",
         [] {
                std::string out = " ";
                for (const auto &conf : conflist)
                    out.append(compress(conf.name)).append(" ");
                return out;
            }                                                                  },
        {"cgidir",
         [] {
                auto name = getenv_or("SCRIPT_NAME", ".");
                if (const auto pos = name.rfind("/"); pos != std::string::npos)
                    name.erase(pos);
                return name;
            }                                                                  },
        {"cacl",
         [] {
                load_acl(confidx);
                return acl_list[ENTER_RIGHT];
            }                                                                  },
        {"canaacl",
         [] { return std::to_string(check_acl(CHACL_RIGHT, confidx)); }        },
        {"canracl",
         [] { return std::to_string(check_acl(JOIN_RIGHT, confidx)); }         },
        {"canwacl",
         [] { return std::to_string(check_acl(RESPOND_RIGHT, confidx)); }      },
        {"cancacl",
         [] { return std::to_string(check_acl(ENTER_RIGHT, confidx)); }        },
        {"delta",         [] { return getenv_or("DELTA", ""); }                },
        {"editor",        [] { return getenv_or("EDITOR", EDITOR); }           },
        {"edbprompt",     [] { return EDBPROMPT; }                             },
        {"escape",        [] { return ESCAPE; }                                },
        {"email",         [] { return email; }                                 },
        {"euid",          [] { return std::to_string(geteuid()); }             },
        {"exit",          [] { return std::to_string(exit_status); }           },
        {"fairwitness",   [] { return FAIRWITNESS; }                           },
        {"fsep",          [] { return FSEP; }                                  },
        {"firstitem",     [] { return std::to_string(st_glob.i_first); }       },
        {"fromlogin",     [] { return re[st_glob.r_current].login; }           },
        {"fullname",
         [] { return std::string(fullname_in_conference(&st_glob)); }          },
        {"fwlist",        [] { return getconf_or(CF_FWLIST, ""); }             },
        {"gamma",         [] { return getenv_or("GAMMA", ""); }                },
        {"gecos",         [] { return GECOS; }                                 },
        {"groupindexmsg", [] { return GROUPINDEXMSG; }                         },
        {"hitstoday",     [] { return std::to_string(get_hits_today()); }      },
        {"highresp",      [] { return std::to_string(st_glob.r_last); }        },
        {"home",          [] { return home; }                                  },
        {"hostname",      [] { return hostname; }                              },
        {"item",          [] { return ITEM; }                                  },
        {"ishort",        [] { return Iint; }                                  },
        {"isep",          [] { return ISEP; }                                  },
        {"indxmsg",       [] { return INDXMSG; }                               },
        {"isbrandnew",
         [] {
                auto i = st_glob.i_current - 1;
                return std::to_string(is_brandnew(&part[i], &sum[i]));
            }                                                                  },
        {"isnewresp",
         [] {
                auto i = st_glob.i_current - 1;
                return std::to_string(is_newresp(&part[i], &sum[i]));
            }                                                                  },
        {"isnew",
         [] {
                auto i = st_glob.i_current - 1;
                return std::to_string(is_newresp(&part[i], &sum[i]) ||
                                      is_brandnew(&part[i], &sum[i]));
            }                                                                  },
        {"joqprompt",     [] { return JOQPROMPT; }                             },
        {"joinmsg",       [] { return JOINMSG; }                               },
        {"linmsg",        [] { return LINMSG; }                                },
        {"loutmsg",       [] { return LOUTMSG; }                               },
        {"listmsg",       [] { return LISTMSG; }                               },
        {"lastitem",      [] { return std::to_string(st_glob.i_last); }        },
        {"lowresp",       [] { return std::to_string(st_glob.r_first); }       },
        {"lastresp",
         [] { return std::to_string(sum[st_glob.i_current - 1].nr - 1); }      },
        {"login",         [] { return login; }                                 },
        {"mailmsg",       [] { return MAILMSG; }                               },
        {"mailsep",       [] { return MAILSEP; }                               },
        {"mailheader",    [] { return MAILHEADER; }                            },
        {"mode",          [] { return std::to_string((int)mode); }             },
        {"noconfp",       [] { return NOCONFP; }                               },
        {"nsep",          [] { return NSEP; }                                  },
        {"newssep",       [] { return NEWSSEP; }                               },
        {"nextconf",      [] { return nextconf(); }                            },
        {"nextnewconf",   [] { return nextnewconf(); }                         },
        {"nextitem",      [] { return std::to_string(st_glob.i_next); }        },
        {"newresp",       [] { return std::to_string(st_glob.i_newresp); }     },
        {"numitems",      [] { return std::to_string(st_glob.i_numitems); }    },
        {"newresps",
         [] {
                auto i = st_glob.i_current - 1;
                return std::to_string(sum[i].nr - abs(part[i].nr));
            }                                                                  },
        {"nobody",        [] { return get_conf_param("nobody", NOBODY); }      },
        {"obvprompt",     [] { return OBVPROMPT; }                             },
        {"pathinfo",
         [] {
                auto info = getenv_or("PATH_INFO", " ");
                info.erase(0, 1);
                return info;
            }                                                                  },
        {"pid",           [] { return std::to_string(getpid()); }              },
        {"printmsg",      [] { return PRINTMSG; }                              },
        {"partmsg",       [] { return PARTMSG; }                               },
        {"prevconf",      [] { return prevconf(); }                            },
        {"previtem",      [] { return std::to_string(st_glob.i_prev); }        },
        {"partdir",       [] { return partdir; }                               },
        {"prompt",        [] { return PROMPT; }                                },
        {"querystring",   [] { return getenv_or("QUERY_STRING", ""); }         },
        {"remotehost",
         [] {
                if (getuid() != get_nobody_uid())
                    return hostname; /* localhost */
                return getenv_or("REMOTE_HOST", "");
            }                                                                  },
        {"remoteaddr",
         [] {
                if (getuid() != get_nobody_uid()) // localaddr
                    return std::string("127.0.0.1");
                return getenv_or("REMOTE_ADDR", "");
            }                                                                  },
        {"requestmethod", [] { return getenv_or("REQUEST_METHOD", ""); }       },
        {"racl",
         [] {
                load_acl(confidx);
                return acl_list[JOIN_RIGHT];
            }                                                                  },
        {"rfpprompt",     [] { return RFPPROMPT; }                             },
        {"rsep",          [] { return RSEP; }                                  },
        {"replysep",      [] { return REPLYSEP; }                              },
        {"subject",       [] { return SUBJECT; }                               },
        {"scribbled",     [] { return SCRIBBLED; }                             },
        {"scribok",       [] { return SCRIBOK; }                               },
        {"shell",         [] { return getenv_or("SHELL", SHELL); }             },
        {"status",        [] { return std::format("0x{:x}", status); }         },
        {"seenresp",
         [] { return std::to_string(abs(part[st_glob.i_current - 1].nr)); }    },
        {"sysop",         [] { return get_sysop_login(); }                     },
        {"ticket",        [] { return get_ticket(0, login); }                  },
        {"text",          [] { return TEXT; }                                  },
        {"txtsep",        [] { return TXTSEP; }                                },
        {"totalnewresp",  [] { return std::to_string(st_glob.r_totalnewresp); }},
        {"unseen",        [] { return std::to_string(st_glob.i_unseen); }      },
        {"userdbm",       [] { return get_conf_param("userdbm", USERDBM); }    },
        {"uid",           [] { return std::to_string(uid); }                   },
        {"userfile",      [] { return get_userfile(login); }                   },
        {"visual",
         [] {
                if (const auto *vis = getenv("VISUAL"); vis != nullptr)
                    return std::string(vis);
                return expand("editor", DM_VAR);
            }                                                                  },
        {"verifyemail",
         [] { return get_conf_param("verifyemail", VERIFY_EMAIL); }            },
        {"version",       [] { return VERSION; }                               },
        {"wacl",
         [] {
                load_acl(confidx);
                return acl_list[RESPOND_RIGHT];
            }                                                                  },
        {"wellmsg",       [] { return WELLMSG; }                               },
        {"work",          [] { return work; }                                  },
        {"wwwdir",
         [] {
                const auto wwwdef = get_conf_param("bbsdir", BBSDIR) + "/www";
                return get_conf_param("wwwdir", wwwdef);
            }                                                                  },
        {"zsep",          [] { return ZSEP; }                                  },
};

} // anonymous namespace

std::optional<std::reference_wrapper<macro_t>>
find_macro(const std::string_view &name, mask_t mask)
{
    for (auto &m : macros) {
        if ((mask & m.mask) != 0 && match(name, m.name))
            return m;
    }
    return {};
}

/******************************************************************************/
/* EXPAND A MACRO                                                             */
/* UNK Could put hashing in later to speed things up if needed                */
/******************************************************************************/
/* ARGUMENTS:                       */
/* Macro name to expand          */
/* Type of macro (see macro.h)   */
std::string
expand(const std::string_view &macro, mask_t mask)
{
    mask &= ~DM_CONSTANT;
    if (debug & DB_MACRO)
        std::println("expand: '{}' {}", macro, mask);
    const auto mac = find_macro(macro, mask);
    if (mac)
        return mac->get().value;
    if (mask & DM_VAR) {
        const auto entry = defaults.find(str::lowercase(macro));
        if (entry != defaults.end())
            return entry->second();
    }
    return "";
}

// ARGUMENTS:
// Macro name to expand
// Type of macro (see macro.h)
// Capitalize string if true
std::string
capexpand(const std::string_view &mac, mask_t mask, bool capitalize)
{
    auto expanded = expand(mac, mask);
    if (!expanded.empty() && capitalize)
        expanded[0] = toupper(expanded[0]);
    return expanded;
}

static int
print_macros(void)
{
    FILE *fp;

    /* Display current macros */
    open_pipe();
    if (status & S_PAGER)
        fp = st_glob.outp;
    else
        fp = stdout;
    std::println(fp, "What       Is Short For\n");
    for (const auto &m : std::views::reverse(macros)) {
        if ((status & S_INT) != 0)
            break;
        std::println(fp, "{:<10} {:3} {}", m.name, m.mask, m.value);
    }
    return 1;
}
/******************************************************************************/
/* PROCESS MACRO DEFINES AND UNDEFINES                                        */
/******************************************************************************/
int
define(int argc, char **argv)
{
    int con = 0;
    if (match(argv[0], "const_ant"))
        con = DM_CONSTANT;
    if (argc <= 1) /* Display current macros */
        return print_macros();
    const std::string_view name{argv[1], strlen(argv[1])};
    if (argc == 2) {
        /* remove name from macro table */
        undef_name(name);
        return 1;
    }
    if (argc == 3) {
        def_macro(name, DM_VAR | con, argv[2]);
        return 1;
    }
    std::string value(argv[3]);
    for (auto i = 4; i < argc; i++) {
        value.push_back(' ');
        value.append(argv[i]);
    }
    def_macro(name, atoi(argv[2]) | con, value);
    return 1;
}

// Defines or updates a macro definition.  Takes the
// alias/variable name, the macro type, and a string
// that the macro expands to.
void
def_macro(
    const std::string_view &name, mask_t mask, const std::string_view &val)
{
    if (mask == 0) {
        std::println("Bad mask value.");
        return;
    }
    if (rovars.contains(name)) {
        std::println("Variable '{}' is readonly.", name);
        return;
    }
    auto value = str::unquote(val);
    if ((mask & DM_ENVAR) != 0) {
        const std::string name0(name);
        setenv(name0.c_str(), value.c_str(), 1);
        return;
    }

    // Add name to macro table
    if (orig_stdin[stdin_stack_top].type & STD_SUPERSANE)
        mask |= DM_SUPERSANE;
    auto m = find_macro(name, mask);
    if (!m) {
        // Create new element
        macro_t nmac{};
        nmac.name.assign(name);
        nmac.mask = mask;
        nmac.value = value;
        macros.push_back(nmac);
        return;
    }
    // already defined
    auto &mac = m->get();
    if ((mac.mask & DM_CONSTANT) != 0) {
        if ((flags & O_QUIET) == 0 && !str::eq(mac.value, value))
            std::println("Can't redefine constant '{}'", mac.name);
        return;
    }
    if ((mac.mask & (DM_SANE | DM_SUPERSANE)) != 0 ||
        (mask & (DM_SANE | DM_SUPERSANE)) == 0) {

        mac.mask = mask;
        mac.value = value;
    }
}

// Removes all macros matching the given mask.
void
undefine(std::uint16_t mask)
{
    if (debug & DB_MACRO)
        std::println("undefine: mask={}", mask);
    std::erase_if(
        macros, [=](const auto &mac) { return (mac.mask & mask) != 0; });
}

void
undef_name(const std::string_view &name)
{
    if (debug & DB_MACRO)
        std::println("undef name={}", name);
    std::erase_if(
        macros, [&](const auto &mac) { return match(name, mac.name); });
}

std::string
conference(bool cap)
{
    return capexpand("conference", DM_VAR, cap);
}

std::string
fairwitness(bool cap)
{
    return capexpand("fairwitness", DM_VAR, cap);
}

std::string
topic(bool cap)
{
    return capexpand("item", DM_VAR, cap);
}

std::string
subject(bool cap)
{
    return capexpand("subject", DM_VAR, cap);
}
