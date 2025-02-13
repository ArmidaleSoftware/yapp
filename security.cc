// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

/*
 * Implements conference security with
 *  [rwca] [+-][all f:ulist f:observers fwlist originlist password sysop]
 */
#include "security.h"

#include <ctype.h>
#include <string.h>

#include <iterator>
#include <string_view>
#include <vector>

#include "conf.h"
#include "globals.h"
#include "lib.h"
#include "stats.h"
#include "str.h"
#include "struct.h"
#include "sysop.h"
#include "yapp.h"

static int acl_idx = -1;

std::string acl_list[NUM_RIGHTS];
static const std::string rightstr{"rwca"};

void
reinit_acl(void)
{
    acl_idx = -1;
}

void
load_acl(int idx)
{
    if (idx == acl_idx)
        return;
    for (auto it = std::begin(acl_list); it != std::end(acl_list); ++it)
        it->clear();
    auto lines =
        grab_file(conflist[idx].location, "acl", GF_IGNCMT | GF_SILENT);
    for (auto &acl : lines) {
        char *ptr{};
        for (ptr = acl.data(); isspace(*ptr); ptr++);
        const char *q = strchr(ptr, ' ');
        while (ptr < q) {
            const auto pos = rightstr.find(*ptr++);
            if (pos != std::string::npos)
                acl_list[pos] = std::string(q + 1);
        }
    }

    const auto no_join_acl = acl_list[JOIN_RIGHT].empty();
    const auto no_respond_acl = acl_list[RESPOND_RIGHT].empty();
    const auto no_enter_acl = acl_list[ENTER_RIGHT].empty();

    // If we hit eof without finding any relevant lines, revert to
    // conference security type default
    if (no_join_acl || no_respond_acl || no_enter_acl) {
        auto sec = security_type(get_config(idx), idx);
        auto basic_sec = (sec & CT_BASIC);

        std::string base{"+registered"};
        if (basic_sec == CT_PRESELECT || basic_sec == CT_PARANOID)
            base.append(" +f:ulist");
        if ((sec & CT_ORIGINLIST) != 0)
            base.append(" +originlist");

        if (no_respond_acl)
            acl_list[RESPOND_RIGHT] = str::concat({base, " -f:observers"});

        if (no_join_acl) {
            if ((sec & CT_READONLY) != 0)
                acl_list[JOIN_RIGHT] = "+all";
            else
                acl_list[JOIN_RIGHT] = base;

            if (basic_sec == CT_PASSWORD || basic_sec == CT_PARANOID)
                acl_list[JOIN_RIGHT].append(" +password");
        }
        if (no_enter_acl) {
            acl_list[ENTER_RIGHT] = acl_list[RESPOND_RIGHT];
            if ((sec & CT_NOENTER) != 0)
                acl_list[ENTER_RIGHT].append(" +fwlist");
        }
    }
    if (acl_list[CHACL_RIGHT].empty())
        acl_list[CHACL_RIGHT] = "+sysop";

    acl_idx = idx;
}

/* ARGUMENTS:                           */
/* IN: criteria to test                 */
/* IN: conference index                 */
static bool
check_field(std::string_view field, int idx)
{
    bool no = false;
    bool ok = false;

    const auto uidstr = std::to_string(uid);
    if (field[0] == '-') {
        no = true;
        field.remove_prefix(1);
    } else if (field[0] == '+') {
        field.remove_prefix(1);
    }
    if (tolower(field[0]) == 'f' && field[1] == ':') {
        field.remove_prefix(2);
        const std::string file(field);
        ok = is_inlistfile(idx, file.c_str());
    } else if (field == "fwlist")
        ok = is_fairwitness(idx);
    else if (field == "all")
        ok = 1;
    else if (field == "registered")
        ok = (status & S_NOAUTH) == 0;
    else if (field == "password")
        ok = check_password(idx);
    else if (field == "originlist")
        ok = is_validorigin(idx);
    else if (field == "sysop")
        ok = is_sysop(1);

    if (no)
        ok = !ok;

    return ok;
}

/* ARGUMENTS:                     */
/* IN: Right to check (r/w/c)     */
/* IN: Conference index           */
bool
check_acl(int right, int idx)
{
    load_acl(idx);
    const auto fields = str::split(acl_list[right], " ");

    /* Ok iff user fits every field.  */
    for (const auto &field : fields)
        if (!check_field(field, idx))
            return false;
    return true;
}
/******************************************************************************/
/* TEST TO SEE IF ULIST SHOULD BE UPDATED WITH NEW JOINERS                    */
/******************************************************************************/
bool
is_auto_ulist(/* ARGUMENTS:             */
    int idx   /* IN: conference index */
)
{
    load_acl(idx);
    // True if and only if the ulist file is not mentioned in any acls
    for (auto acl = std::begin(acl_list); acl != std::end(acl_list); ++acl)
        if (acl->find("ulist") != std::string::npos)
            return false;
    return true;
}
