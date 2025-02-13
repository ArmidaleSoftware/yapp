// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include "range.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctime>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include "dates.h"
#include "globals.h"
#include "lib.h"
#include "macro.h"
#include "stats.h"
#include "str.h"
#include "struct.h"
#include "sum.h"
#include "yapp.h"

/*
         if (part[i].nr < sum[i].nr) st->i_newresp++;
 * the problem with the following (^^^?) is that we may have recently read A
   response, but not ALL the responses -- Ok acc to Russ
 *
 * Latest from refresh_stats:
 *  else if (part[i].last < sum[i].last) st->i_newresp++;
 */
/*
Changed per Russ's suggestion:
The motivation is so that your own responses can advance the
timestamp but not the last-seen response number, thus showing
your own response to you only *after* an additional response
has been made (altering the timestamp again).  If you do not
have this feature, you have the (rather ugly) alternatives
of items instantly becoming "new" again when you respond
to them [if you advance neither], or of the system not showing you
your own response to establish the context for the followup [if
you advance the # resp].

Not:
   if ((spec & OF_NEWRESP) && p->nr && (abs(p->nr) < s->nr)) return 1;

Problem is that item comes up as new when there are no new responses.
*/

/* The problem with this one is that items come up as new when
 * some response is censored or scribbled.
 * As Marcus conceived it, this is a feature, not a bug.  If a response
 * is censored (especially by an authority), it should call attention
 * to itself.  Such is the idea as I remember it.  --Russ
 * if (p->nr && (p->last < s->last)) return 1;
 */

/*
 * Test to see if an item is newresp.  If SENSITIVE, then censored/scribbled
 * items come up new also, even if there are no new responses.
 */
int
is_newresp(partentry_t *p, sumentry_t *s)
{
    if ((flags & O_SENSITIVE) && p->nr && (p->last < s->last))
        return 1;
    if (p->nr && (abs(p->nr) < s->nr) && (p->last < s->last))
        return 1;
    return 0;
}
/*
 * if (part[i].last < sum[i].last) *
 * if (st->parttime < sum[i].last) before change new partfile *
 * Latest from refresh_stats:
 *  if (part[i].last < sum[i].last)
 */

/* Test to see if an item is brandnew */
int
is_brandnew(partentry_t *p, sumentry_t *s)
{
    return ((!p->nr && s->nr) && (p->last < s->last));
}
/*****************************************************************************/
/* TEST WHETHER ITEM IS COVERED BY THE SPECIFIED SUBSET PARAMETERS           */
/*****************************************************************************/
char
cover(                 /* ARGUMENTS: */
    int i,             /* IN : Item number                  */
    int idx,           /* IN : Conference index             */
    int spec,          /* IN : Specifiers                   */
    int act,           /* IN : Action flag                  */
    sumentry_t *sum,   /* Item summary                 */
    partentry_t *part, /* User participation info      */
    status_t *st)
{
    sumentry_t *s;
    partentry_t *p;
    /* if (spec & OF_NONE) return 0; */
    if (act == A_SKIP)
        return 0;

    /* I'm guessing the following refresh is so we catch changes while
     * reading items. -dgt 8/22/97 */
    refresh_sum(i, idx, sum, part, st); /* added for sno */

    s = &(sum[i - 1]);
    p = &(part[i - 1]);
    if (!s->flags)
        return 0;
    if (!st->string.empty()) {
        auto newstring = st->string;
        lower_case(newstring);
        std::string subjstring = get_subj(idx, i - 1, sum);
        lower_case(subjstring);
        if (subjstring.find(newstring) == std::string::npos)
            return 0;
    }
    if (!st->author.empty() && !str::eq(get_auth(idx, i - 1, sum), st->author))
        return 0;
    if (st->since > s->last)
        return 0;
    if (st->before < s->last && st->before > 0)
        return 0;
    if (st->rng_flags && !(s->flags & st->rng_flags))
        return 0;
    if (act == A_FORCE) {
        /* force takes precedence over forgotten */
        return 1;
        /*
        if (s->flags & IF_FORGOTTEN)
            std::print("<{}:{}:{}:}{}",
                i, spec & (OF_NOFORGET|OF_FORGOTTEN|OF_RETIRED),
                flags & O_FORGET,
                s->flags & (IF_FORGOTTEN|IF_RETIRED));
        */
    }
    if (!(spec & (OF_NOFORGET | OF_FORGOTTEN)) && (flags & O_FORGET) &&
        (s->flags & IF_FORGOTTEN))
        return 0;
    if (!(spec & (OF_NOFORGET | OF_RETIRED)) && (s->flags & IF_RETIRED))
        return 0;
    if (!(spec & (OF_NOFORGET | OF_EXPIRED)) && (s->flags & IF_EXPIRED))
        return 0;

    /* Process NEW limitations */
    if ((spec & OF_NEWRESP) && is_newresp(p, s))
        return 1;
    if ((spec & OF_BRANDNEW) && is_brandnew(p, s))
        return 1;
    if (spec & (OF_NEWRESP | OF_BRANDNEW))
        return 0;
    if ((spec & OF_UNSEEN) && p->nr)
        return 0;
    if ((spec & OF_FORGOTTEN) && !(s->flags & IF_FORGOTTEN))
        return 0;
    if ((spec & OF_RETIRED) && !(s->flags & IF_RETIRED))
        return 0;

    /* if (spec & OF_NEXT) spec |= OF_NONE; */
    return 1;
}
/*****************************************************************************/
/* MARK A RANGE OF ITEMS TO BE ACTED UPON                                    */
/*****************************************************************************/
static void
markrange(                     /* ARGUMENTS: */
    int bg,                    /* Beginning of range */
    int nd,                    /* End of range */
    char act[MAX_ITEMS],       /* Action array to fill in */
    sumentry_t sum[MAX_ITEMS], /* Summary of item info    */
    status_t *st,              /* Conference statistics   */
    int val                    /* Action value to set */
)
{
    if (bg < st->i_first)
        std::println(
            "{} #{} is too small (first {})", topic(1), bg, st->i_first);
    else if (nd > st->i_last)
        std::println("{} #{} is too big (last {})", topic(1), nd, st->i_last);
    else {
        for (auto j = bg; j <= nd; j++)
            if (sum[j - 1].flags)
                act[j - 1] = val;
        if (bg == nd && !sum[bg - 1].flags)
            std::println("No such {}!", topic());
    }
}
/*****************************************************************************/
/* CONVERT A TOKEN TO AN INTEGER                                             */
/*****************************************************************************/
short
get_number(                        /* ARGUMENTS:                */
    const std::string_view &token, /* Field to process      */
    status_t *st                   /* Conference statistics */
)
{
    short a, b;
    if (match(token, "fi_rst") || match(token, "^"))
        return st->i_first;
    if (match(token, "l_ast") || match(token, "$"))
        return st->i_last;
    if (match(token, "th_is") || match(token, "cu_rrent") || match(token, "."))
        return st->i_current;
    std::string tok(token);
    if (sscanf(tok.c_str(), "%hd.%hd", &a, &b) == 2) {
        if (b >= 0 && b < sum[a - 1].nr)
            st->r_first = b;
        return a;
    }
    return str::toi(tok);
}

static void
rangearray(const std::vector<std::string> &args, std::size_t start,
    short *fl,                 /* Flags to use */
    char act[MAX_ITEMS],       /* Action array to fill in */
    sumentry_t sum[MAX_ITEMS], /* Item summary info array */
    status_t *st               /* Conference statistics */
)
{
    size_t i = start;
    if (start >= args.size())
        return;
    for (auto it = args.begin() + start; it != args.end(); ++it) {
        const auto &arg = *it;
        if (match(arg, "si_nce") || match(arg, "S="))
            st->since = since(args, &i);
        else if (match(arg, "before") || match(arg, "B="))
            st->before = since(args, &i);
        else if (match(arg, "by") || match(arg, "A=")) {
            if (it + 1 == args.end())
                std::println("Invalid author specified.");
            else
                st->author = arg;
        } else if (!match(arg, "F=")) /* skip "F=" */
            rangetoken(arg.c_str(), fl, act, sum, st);
        i++;
    }
}

/*****************************************************************************/
/* PARSE ONE FIELD OF A RANGE SPECIFICATION                                  */
/*****************************************************************************/
void
rangetoken(                    /* ARGUMENTS: */
    const char *token,         /* Field to process */
    short *flg,                /* Flags to use */
    char act[MAX_ITEMS],       /* Action array to fill in */
    sumentry_t sum[MAX_ITEMS], /* Item summary info array */
    status_t *st               /* Conference statistics */
)
{
    short fl, a, b, c;
    char buff[MAX_LINE_LENGTH];

    if (debug & DB_RANGE)
        std::println("rangetoken: '{}'", token);
    const auto bp = expand(token, DM_PARAM);
    if (!bp.empty()) {
        const auto arr = str::splits(bp, " ");
        rangearray(arr, 0, flg, act, sum, st);
        return;
    }
    fl = *flg;

    if (match(token, "a_ll") || match(token, "*")) {
        markrange(st->i_first, st->i_last, act, sum, st, A_COVER);
        fl |= OF_RANGE;                 /* items specified */
    } else if (match(token, "nex_t")) { /* fl |=  OF_NEXT; */
        markrange(
            (short)(st->i_current + 1), st->i_last, act, sum, st, A_COVER);
        fl |= OF_RANGE; /* KKK */
    } else if (match(token, "pr_evious")) {
        fl |= /* OF_NEXT | */ OF_REVERSE;
        markrange(
            st->i_first, (short)(st->i_current - 1), act, sum, st, A_COVER);
        fl |= OF_RANGE; /* KKK */
    } else if (match(token, "n_ew")) {
        fl |= OF_BRANDNEW | OF_NEWRESP;
    } else if (match(token, "nof_orget")) {
        fl |= OF_NOFORGET;
    } else if (match(token, "p_ass")) {
        fl |= OF_PASS;
    } else if (match(token, "d_ate")) {
        fl |= OF_DATE;
    } else if (match(token, "nor_esponse")) {
        fl |= OF_NORESPONSE;
    } else if (match(token, "u_id")) {
        fl |= OF_UID;
    } else if (match(token, "nod_ate")) {
        fl &= ~OF_DATE;
    } else if (match(token, "nou_id")) {
        fl &= ~OF_UID;
    } else if (match(token, "bra_ndnew")) {
        fl |= OF_BRANDNEW;
    } else if (match(token, "newr_esponse")) {
        fl |= OF_NEWRESP;
    } else if (match(token, "r_everse")) {
        fl |= OF_REVERSE;
    } else if (match(token, "s_hort")) {
        fl |= OF_int;
    } else if (match(token, "nop_ass")) {
        fl &= ~OF_PASS;
    } else if (match(token, "nu_mbered")) {
        fl |= OF_NUMBERED;
    } else if (match(token, "nonu_mbered")) {
        fl &= ~OF_NUMBERED;
    } else if (match(token, "unn_umbered")) {
        fl &= ~OF_NUMBERED;
    } else if (match(token, "o_ld")) { /* xxxj */
    } else if (match(token, "exp_ired")) {
        fl |= OF_EXPIRED;
    } else if (match(token, "ret_ired")) {
        fl |= OF_RETIRED;
    } else if (match(token, "for_gotten")) {
        fl |= OF_FORGOTTEN;
    } else if (match(token, "un_seen")) {
        fl |= OF_UNSEEN;
    } else if (match(token, "linked")) {
        st->rng_flags |= IF_LINKED;
    } else if (match(token, "frozen")) {
        st->rng_flags |= IF_FROZEN;
    } else if (match(token, "force_response") ||
               match(token, "force_respond")) {                   /* KKK */
    } else if (match(token, "respond")) {                         /* KKK */
    } else if (match(token, "form_feed") || match(token, "ff")) { /* KKK */
    } else if (match(token, "lo_ng")) {
        fl &= ~OF_int;
    } else if (token[0] == '"') { /* "string" */
        st->string = token + 1;
        if (st->string[strlen(token) - 2] == '"')
            st->string.erase(strlen(token) - 2);
    } else if (strchr(token, ',')) {
        const auto arr = str::splits(token, ",");
        rangearray(arr, 0, flg, act, sum, st);
        return;
    } else if (token[0] == '-') {
        a = get_number(token + 1, st);
        markrange(st->i_first, a, act, sum, st, A_COVER);
        fl |= OF_RANGE; /* range specified */
    } else if (token[strlen(token) - 1] == '-') {
        strcpy(buff, token);
        buff[strlen(buff) - 1] = '\0';
        a = get_number(buff, st);
        markrange(a, st->i_last, act, sum, st, A_COVER);
        fl |= OF_RANGE;
    } else if (auto *bp = strchr(token, '-'); bp != NULL) {
        strncpy(buff, token, bp - token);
        buff[bp - token] = '\0';
        a = get_number(buff, st);
        b = get_number(bp + 1, st);
        if (b < a) {
            c = b;
            b = a;
            a = c;
            fl ^= OF_REVERSE;
        }
        markrange(a, b, act, sum, st, A_COVER);
        fl |= OF_RANGE;
        /*
        } else if (sscanf(token,"-%hd",&a)==1) {
                markrange(st->i_first,a,act,sum,st,A_COVER);
                fl |= OF_RANGE;
        } else if (sscanf(token,"%hd-%hd",&a,&b)==2) {
                if (b<a) { c=b; b=a; a=c; fl ^= OF_REVERSE; }
                markrange(a,b,act,sum,st,A_COVER);
                fl |= OF_RANGE;
        } else if (sscanf(token,"%hd%c",&a,&ch)==2 && ch=='-') {
                markrange(a,st->i_last,act,sum,st,A_COVER);
                fl |= OF_RANGE;
        } else if (sscanf(token,"%hd",&a)==1) {
                markrange(a,a,act,sum,st,A_FORCE);
                fl |= OF_RANGE;
        */
    } else if ((a = get_number(token, st)) != 0) {
        markrange(a, a, act, sum, st, A_FORCE);
        fl |= OF_RANGE;
    } else {
        st->string = token;
        /* KKK std::println("Bad token type in getrange"); */
    }
    *flg = fl;
}

/* args -- IN : fields holding date spec */
/* ip -- IN/OUT: prev field #          */
time_t
since(const std::vector<std::string> &args, size_t *ip)
{
    time_t t = 0;
    auto i = ((ip == nullptr) ? 0 : *ip) + 1;
    if (i >= args.size()) {
        std::println("Bad date near \"<newline>\"");
        return LONG_MAX; /* process nothing */
    }
    if (args[i][0] == '"') {
        std::string arg(args[i].begin() + 1, args[i].end());
        if (args[i].back() == '"')
            arg.pop_back();
        do_getdate(&t, arg.c_str() + 1);
    } else {
        size_t where[MAX_ARGS];
        std::string concat;
        const char *sep = "";
        for (auto j = i; j < args.size(); j++) {
            where[j] = concat.size();
            concat.append(sep);
            concat.append(args[j]);
            sep = " ";
        }
        const char *cstr = concat.c_str();
        const char *ptr = do_getdate(&t, cstr);
        while (ptr - cstr > where[i] && i < args.size()) ++i;
        i--;
    }
    if (debug & DB_RANGE)
        std::print("Since set to {}", ctime(&t));
    if (ip != nullptr)
        *ip = i;
    return t;
}

void
rangeinit(status_t *st, char act[MAX_ITEMS])
{
    short i;
    st->string.clear();
    st->author.clear();
    st->since = st->before = st->opt_flags = 0;
    st->rng_flags = 0;
    /* flags |= O_FORGET;  commented out 8/18 since it breaks 'set
     * noforget;r' */
    for (i = 0; i < MAX_ITEMS; i++) act[i] = 0;
}
/*****************************************************************************/
/* PARSE ONE FIELD OF A RANGE SPECIFICATION                                  */
/* Note that we need sum passed in because linkfrom does ranges in other     */
/* conference                                                                */
/*****************************************************************************/
void
range(                         /* ARGUMENTS: */
    int argc,                  /* Number of arguments */
    char **argv,               /* Argument list       */
    short *fl,                 /* Flags to use */
    char act[MAX_ITEMS],       /* Action array to fill in */
    sumentry_t sum[MAX_ITEMS], /* Item summary info array */
    status_t *st,              /* Conference statistics */
    int bef)
{
    std::vector<std::string> args;
    for (auto i = 0; i < argc; i++) args.push_back(argv[i]);
    rangearray(args, bef + 1, fl, act, sum, st);
}
