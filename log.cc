// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include "log.h"

#include <string.h>

#include <format>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include "driver.h"
#include "globals.h"
#include "lib.h"
#include "macro.h"
#include "sep.h"
#include "str.h"
#include "yapp.h"

std::string
expand_sep(const std::string_view &str, int fl, int type)
{
    const auto tmp_status = status;
    const std::string oldeval(evalbuf);
    evalbuf[0] = '\0';
    status |= S_EXECUTE;
    if (type == M_RFP)
        itemsep(str, fl);
    else
        confsep(str, confidx, &st_glob, part, fl);
    status &= ~S_EXECUTE;
    std::string sepbuf(evalbuf);
    strlcpy(evalbuf, oldeval.c_str(), sizeof(evalbuf));
    status = tmp_status;

    return sepbuf;
}

/* IN : event name */
/* OUT: log file name */
static std::string
find_event(const std::string_view &event, std::string &logfile)
{
    /* Look up variables: <event>log, <event>logsep */
    auto var = str::concat({event, "log"});
    const auto exp = expand(var, DM_VAR);
    if (exp.empty())
        return "";
    logfile = exp;
    var.append("sep");
    return expand(var, DM_VAR);
}

void
custom_log(const char *event, int type)
{
    std::string logfile;
    const auto sepstr = find_event(event, logfile);
    if (sepstr.empty())
        return;
    const auto file = expand_sep(logfile, 1, type);
    const auto str = expand_sep(sepstr, 0, type);
    if (!file.empty() && !str.empty())
        write_file(file, str);
}

/******************************************************************************/
int
logevent(       /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    if (argc != 4) {
        std::println("usage: log <event> <filename> <sepstring>");
        return 1;
    }

    const char *q = (argv[2][0] == '"') ? "" : "\"";
    const auto logconst =
        std::format("constant {}log {}{}{}", argv[1], q, argv[2], q);
    command(logconst.c_str(), 0);
    q = (argv[3][0] == '"') ? "" : "\"";
    const auto logsep =
        std::format("constant {}logsep {}{}{}", argv[1], q, argv[3], q);
    command(logsep.c_str(), 0);

    return 1;
}
