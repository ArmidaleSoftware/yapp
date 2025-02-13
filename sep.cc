// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

/* This module takes care of most of the fancy output, and allows
 * users and administrators to completely customize the output
 * of many functions.  A "separator" string is passed to confsep
 * or itemsep, which break it up and generate output based on the
 * codes therein.  For more information, do "help separators"
 * from within the program.
 */

#include "sep.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <algorithm>
#include <iostream>
#include <print>
#include <string>
#include <vector>

#include "change.h"
#include "conf.h"
#include "driver.h"
#include "globals.h"
#include "item.h"
#include "lib.h"
#include "macro.h"
#include "main.h"
#include "mem.h"
#include "news.h"
#include "range.h"
#include "stats.h"
#include "str.h"
#include "struct.h"
#include "sum.h"
#include "user.h"
#include "yapp.h"

static const char *lchars = "ntvbrface";
static const char *rchars = "\n\t\v\b\r\f\007\377\033";
static int lastnum = 0, show[100], depth = 0, num = 0, tabs = 1;
static int newline, qfail, once, zero;
extern char *cfiles[];

void
init_show(void)
{
    show[0] = 1; /* was: show[depth]=1; */
}

/******************************************************************************/
/* OUTPUT A STRING TO THE DESIRED FORMAT                                      */
/******************************************************************************/
static void
putstring(                     /* ARGUMENTS:                   */
    const std::string_view &b, /* String to output          */
    FILE *fp                   /* File pointer to output to */
)
{
    /* LOCAL VARIABLES:             */
    char buf[MAX_LINE_LENGTH]; /* Formatted output          */

    if (!show[depth])
        return;
    auto len = std::min(b.size(), sizeof(buf) - 1);
    if (0 < num && num < len)
        len = num;
    memcpy(buf, b.data(), len);
    buf[len] = '\0';
    wfputs(buf, fp);
}

namespace y
{

constexpr auto WHITESPACE = " \t\v\f\r\n";

// Gets the column number we should wrap at, or 0.
std::size_t
wrapcolumn(void)
{
    const auto wrapstr = expand("wrap", DM_VAR);
    if (wrapstr.empty())
        return 0;
    return str::toi(wrapstr);
}

// Returns a new view over the given string, with trailing
// whitespace removed.
std::string_view
rtrim(std::string_view s)
{
    const auto lpos = s.find_last_not_of(WHITESPACE);
    if (lpos != std::string_view::npos)
        s.remove_suffix(s.size() - lpos - 1);
    return s;
}

// Writes the given string to the given output,
// wrapping at some column length.
//
// Wrapping attempts to honor whitespace where
// it can.
void
wrapout(std::ostream &out, std::string_view str)
{
    const auto wrapcol = wrapcolumn();
    if (wrapcol == 0) {
        std::print(out, "{}", str);
        return;
    }
    while (!str.empty()) {
        if (str.size() < wrapcol) {
            std::println(out, "{}", str);
            break;
        }
        auto len = wrapcol;
        auto prefix = str.substr(0, len);
        auto pos = prefix.find_first_of("\r\n");
        if (pos != std::string_view::npos) {
            auto line = prefix.substr(0, pos);
            std::println(out, "{}", rtrim(line));
            auto c = prefix[pos];
            str.remove_prefix(pos + 1);
            if ((c == '\r' && str.starts_with("\n")) ||
                (c == '\n' && str.starts_with("\r")))
                str.remove_prefix(1);
            continue;
        }
        pos = prefix.find_last_of(WHITESPACE);
        if (pos != std::string_view::npos) {
            prefix.remove_suffix(prefix.size() - pos - 1);
            prefix = rtrim(prefix);
            len = pos + 1;
        }
        std::println(out, "{}", prefix);
        str.remove_prefix(len);
    }
}
} // namespace y

/*
 * This routine does:   std::println(fp, "{}", str);
 * except that it wraps output at WRAPCOL columns
 */
static void
wrapout(FILE *fp, const char *str)
{
    int wrapcol;
    char *line, *ptr;

    if (!show[depth])
        return;

    const auto wrap = expand("wrap", DM_VAR);
    if (wrap.empty() || (wrapcol = str::toi(wrap)) < 1) {
        putstring(str, fp);
        return;
    }
    ptr = line = (char *)emalloc(wrapcol + 2);

    const char *p;
    for (p = str; *p; p++) {
        if (*p == '\n' || *p == '\r') {
            (*ptr) = '\0';
            if (ptr > line)
                wfputs(line, fp);
            ptr = line;
            wfputc(*p, fp);
            continue;
        }
        if (ptr - line == wrapcol) {
            if (*p == ' ') {
                (*ptr) = '\0';
                wfputs(line, fp);
                ptr = line;

                wfputc('\n', fp);
                continue;
            } else {
                /* Find previous space */
                char *s;
                for (s = ptr - 1; s >= line && !isspace(*s); s--);
                if (s < line) {
                    (*ptr) = '\0';
                    wfputs(line, fp);
                    while (*p && !isspace(*p)) wfputc(*p++, fp);
                    wfputc('\n', fp);
                    ptr = line;
                    if (!*p)
                        p--;
                    continue;
                } else {
                    (*s++) = (*ptr) = '\0';
                    wfputs(line, fp);
                    wfputc('\n', fp);
                    strcpy(line, s);
                    ptr -= s - line;
                }
            }
        }
        *ptr++ = *p;
    }

    /* Once we hit end of line, dump the rest but don't append a newline */
    if (ptr > line) {
        (*ptr) = '\0';
        wfputs(line, fp);
    }
    free(line);
}
/******************************************************************************/
/* OUTPUT A NUMBER TO THE DESIRED FORMAT                                      */
/******************************************************************************/
static void
number(      /* ARGUMENTS:           */
    int b,   /* Number to output  */
    FILE *fp /* Stream to send to */
)
{
    if (!show[depth])
        return;
    if (b == 0 && zero)
        wfputs(std::format("{:c}o", "nN"[zero - 1]), fp);
    else if (num != 0)
        wfputs(std::format("{:{}d}", b, num), fp);
    else
        wfputs(std::format("{}", b), fp);
    lastnum = b;
}

entity_t *
get_entity(const char **spp)
{
    static entity_t ent;
    char buff[80];
    const char *sp;
    char *sub;

    sp = *spp;

    if (*sp == '"') { /* Get string */
        sp++;         /* skip start quote */
        for (sub = buff; *sp && *sp != '"'; sp++, sub++) *sub = *sp;
        (*sub) = '\0';
        if (*sp == '"') /* skip ending quote */
            sp++;
        ent.type = ET_STRING;
        ent.val.s = estrdup(buff);

    } else if (isdigit(*sp) || *sp == '-') { /* Get int    */
        sub = buff;
        if (*sp == '-')
            *sub++ = *sp++;
        while (*sp && isdigit(*sp)) *sub++ = *sp++;
        *sub = '\0';
        ent.type = ET_INTEGER;
        ent.val.i = atoi(buff);
    } else { /* Get macro */
        for (sub = buff; *sp && isalnum(*sp); sp++, sub++) *sub = *sp;
        (*sub) = '\0';
        auto str = expand(buff, DM_VAR);
        if (!str.empty() && (isdigit(str[0]) || str[0] == '-')) {
            ent.type = ET_INTEGER;
            ent.val.i = str::toi(str);
        } else {
            ent.type = ET_STRING;
            ent.val.s = estrdup(str.c_str());
        }
    }

    *spp = sp;
    return &ent;
}

void
dest_entity(entity_t *ent)
{
    if (ent->type == ET_STRING)
        free(ent->val.s);
}
/*
 * Convert an operator string to integer format, which some OR'ed combination
 * of the following flags:
 */
#define OP_GT 0x001
#define OP_EQ 0x010
#define OP_LT 0x100
#define OP_IN 0x1000
#define OP_NOT 0x2000
int
opstr2int(    /* ARGUMENTS: */
    char *str /* Operator string, e.g. "<=", "==", etc */
)
{
    char *p;
    int no_t = 0;
    int ret = 0;
    for (p = str; *p; p++) {
        switch (*p) {
        case '~':
            ret |= OP_IN;
            break;
        case '=':
            ret |= OP_EQ;
            break;
        case '>':
            ret |= OP_GT;
            break;
        case '<':
            ret |= OP_LT;
            break;
        case '!':
            no_t = 1 - no_t;
            break;
        default:
            return 0;
        }
    }

    if (no_t) {
        if (!(ret & OP_IN))
            ret = 0x111 - ret;
        else
            ret |= OP_NOT;
    }
    return ret;
}

int
opcompare(          /* ARGUMENTS: */
    entity_t *left, /* left operand */
    int op,         /* operator: OR'ing of operand flags */
    entity_t *right /* right operand */
)
{
    const char *lstr = nullptr, *rstr = nullptr;
    int lint = 0, rint = 0, typ;
    std::string buff, buff2;
    /* Promote int to string if needed */
    if ((op && op != 0x111) && left->type == ET_STRING &&
        right->type == ET_INTEGER) {
        lstr = left->val.s;
        buff = std::format("{}", right->val.i);
        rstr = buff.c_str();
        typ = ET_STRING;
    } else if ((op && op != 0x111) && left->type == ET_INTEGER &&
               right->type == ET_STRING) {
        rstr = right->val.s;
        buff = std::format("{}", left->val.i);
        lstr = buff.c_str();
        typ = ET_STRING;
    } else if (left->type == ET_STRING) { /* both strings */
        typ = ET_STRING;
        lstr = left->val.s;
        rstr = right->val.s;
    } else {              /* both int's */
        if (op & OP_IN) { /* must promote both to
                           * strings */
            typ = ET_STRING;
            buff = std::format("{}", left->val.i);
            lstr = buff.c_str();
            buff2 = std::format("{}", right->val.i);
            rstr = buff2.c_str();
        } else {
            typ = ET_INTEGER;
            lint = left->val.i;
            rint = right->val.i;
        }
    }

    if (typ == ET_INTEGER) {
        if (op == 0x000) /* single operand */
            return lint;
        if (op == 0x111) /* single operand */
            return !lint;

        /* Two operands */
        return ((lint > rint && (op & OP_GT)) ||
                (lint == rint && (op & OP_EQ)) ||
                (lint < rint && (op & OP_LT)));
    } else {             /* ET_STRING */
        if (op == 0x000) /* single operand */
            return (lstr != NULL);
        if (op == 0x111) /* single operand */
            return (lstr == NULL);

        /* Check for containment in list... */
        if (op & OP_IN) {
            const auto little = std::format(" {} ", rstr);
            const auto big = std::format(" {} ", lstr);
            const auto p = big.find(little) != std::string::npos;
            return (op & OP_NOT) ? !p : p;
        }

        /* Two operands */
        return ((strcmp(lstr, rstr) > 0 && (op & OP_GT)) ||
                (strcmp(lstr, rstr) == 0 && (op & OP_EQ)) ||
                (strcmp(lstr, rstr) < 0 && (op & OP_LT)));
    }
}
/******************************************************************************/
/* PROCESS CONDITIONS FOR BOTH ITEM/CONF SEPS                                 */
/******************************************************************************/
static char
misccond(            /* ARGUMENTS: */
    const char **spp /* Separator string */
)
{
    const char *sp = *spp;
    char ret = 0, no_t = 0;

    if (*sp == '!' || *sp == '~') {
        no_t = !no_t;
        sp++;
    }
    switch (*(sp++)) {
    case 'P':
        ret = (lastnum != 1);
        break;
    case 'S':
        ret = (st_glob.c_status & CS_FW);
        break;
    case 'l':
        ret = (st_glob.c_status & CS_NORESPONSE);
        break;
    case '(': {
        char *buff;
        entity_t left, right;
        int op;
        /* Skip whitespace */
        while (isspace(*sp)) sp++;

        /* Get left operand */
        memcpy(&left, get_entity(&sp), sizeof(left));

        /* Skip whitespace */
        while (isspace(*sp)) sp++;

        /* Get operator */
        size_t oplen = strspn(sp, "<>=!~");
        buff = estrndup(sp, oplen);
        sp += oplen;
        op = opstr2int(buff);
        free(buff);

        /* Skip whitespace */
        while (isspace(*sp)) sp++;

        /* Get right operand */
        memcpy(&right, get_entity(&sp), sizeof(right));

        /* Skip whitespace */
        while (isspace(*sp)) sp++;

        /* Get right paren */
        if (*sp == ')')
            sp++;

        ret = opcompare(&left, op, &right);
        dest_entity(&left);
        dest_entity(&right);
        break;
    }
    default:
        ret = 0;
        break; /* don't show */
    }

    *spp = sp;
    if (!show[depth])
        return 0;
    return (ret != 0) ^ no_t;
}

static sumentry_t oldsum[MAX_ITEMS];
static short oldconfidx;
/* Display new responses since last call */
void
announce_new_responses(FILE *fp /* IN: Stream to send to */
)
{
    int newr = 0, i;
    refresh_sum(0, confidx, sum, part, &st_glob);
    if (confidx == oldconfidx) {
        for (i = st_glob.i_first; i < st_glob.i_last; i++) {
            if (sum[i].nr > oldsum[i].nr && sum[i].last > part[i].last) {
                if (!newr) {
                    putstring("New responses were just posted to "
                              "item(s):\n",
                        fp);
                    newr = 1;
                }
                putstring(" ", fp);
                number(i + 1, fp);
                oldsum[i].nr = sum[i].nr;
            }
        }
        if (newr)
            putstring("\n", fp);
    } else {
        oldconfidx = confidx;
        memcpy(oldsum, sum, MAX_ITEMS * sizeof(sumentry_t));
    }
}
/* Mark given response as not new, so our own responses don't trigger
 * the message above
 */
void
skip_new_response(int c, /* IN: conference index */
    int i,               /* IN: item number */
    int nr               /* IN: response count */
)
{
    if (c == oldconfidx) {
        oldsum[i].nr = nr;
    } else {
        oldconfidx = c;
        refresh_sum(0, c, sum, part, &st_glob);
        memcpy(oldsum, sum, MAX_ITEMS * sizeof(sumentry_t));
    }
}
/******************************************************************************/
/* PROCESS SEPS FOR BOTH ITEM/CONF SEPS                                       */
/******************************************************************************/
void
miscsep(              /* ARGUMENTS: */
    const char **spp, /* Separator string */
    FILE *fp          /* Stream to send to */
)
{
    const char *sp;
    short i;
    char *sub;

    sp = *spp;
    switch (*(sp++)) {

        /* Customization separators */
    case '%':
        putstring("%", fp);
        break;
    case 'E':
        if (!depth || show[depth - 1])
            show[depth] = !show[depth];
        break;
    case 'c':
        newline = 0;
        break;
        /* case 'R': refresh_sum(0,confidx,sum,part,&st_glob);
         * break; */
    case 'R':
        announce_new_responses(fp);
        break;
    case 'S':
        if (lastnum != 1)
            putstring("s", fp);
        break;
    case 'T':
        tabs = num;
        break;
    case 'X':
        for (i = 0; i < tabs; i++)
            if (show[depth])
                wfputc(' ', fp);
        break;
    case ')':
        /*
        for (i=0; i<depth; i++) std::print("   ");
        std::println("---");
        */
        depth--;
        break;
    case 'D':
        if (show[depth])
            wfputs(get_date(time((time_t *)0), num), fp);
        break;
    case '`': /* Execute a command */
    {
        int tmp = once; /* save once */
        int lvl = stdin_stack_top;
        const char *tick = strchr(sp, '`');
        if (tick == NULL)
            tick = sp + strlen(sp);
        size_t len = tick - sp;
        sub = estrndup(sp, len);
        sp += len;
        if (*sp == '`')
            sp++;
        once = 0;
        command(sub, 1);
        once = tmp;
        free(sub);
        while (stdin_stack_top > lvl) pop_stdin();
    } break;

    default:
        break; /* do nothing */
    }

    *spp = sp;
}
/******************************************************************************/
/* PROCESS CONDITIONS FOR ITEM SEPS ONLY                                      */
/******************************************************************************/
char
itemcond(             /* ARGUMENTS:               */
    const char **spp, /* Separator string      */
    long fl           /* Sep flags             */
)
{
    const char *sp;
    char ret = 0, no_t = 0;
    response_t *cre;
    sp = *spp;
    cre = &(re[st_glob.r_current]);

    if (*sp == '!' || *sp == '~') {
        no_t = !no_t;
        sp++;
    }
    for (num = 0; isdigit(*sp); sp++) num = num * 10 + (*sp - '0');

    switch (*(sp++)) {
    case 'B':
        ret = ((once & IS_START) > 0);
        once &= ~IS_START;
        break;
    case 'D':
        ret = ((once & IS_DATE) > 0);
        break;
#ifdef NEWS
    case 'E':
        ret = ((cre->flags & RF_EXPIRED) > 0);
        break;
#endif
    case 'F':
        ret = ((fl & OF_NUMBERED) || (flags & O_NUMBERED));
        break;
        /* case 'I': (see 'O') */
    case 'L':
        ret = (st_glob.l_current >= 0 && st_glob.l_current < cre->text.size() &&
               !cre->text[st_glob.l_current].empty());
        break;
    case 'N':
        ret = (st_glob.r_current > 0);
        /*           ret= (!(!part[st_glob.i_current-1].nr &&
           sum[st_glob.i_current-1].nr)
                          && (part[st_glob.i_current-1].nr  <
           sum[st_glob.i_current-1].nr));
        */
        break;
    case 'I': /* ret=(!part[st_glob.i_current-1].nr &&
               * sum[st_glob.i_current-1].nr); break; fall
               * through into O */
    case 'O':
        ret = ((once & IS_ITEM) > 0);
        once &= ~IS_ITEM;
        break;
    case 'p':
        ret = (cre->parent > 0);
        once &= ~IS_PARENT;
        break;
    case 'r':
        ret = (st_glob.r_current >= 0);
        break;
    case 'R':
        ret = ((once & (IS_ITEM | IS_RESP)) > 0);
        once &= ~(IS_ITEM | IS_RESP);
        /*
                     ret= ((!part[st_glob.i_current-1].nr &&
           sum[st_glob.i_current-1].nr)
                         || (part[st_glob.i_current-1].nr  <
           sum[st_glob.i_current-1].nr));
        */
        break;
    case 'T':
        ret = ((fl & OF_FORMFEED) > 0);
        break;
    case 'U':
        ret = ((once & IS_UID) > 0);
        break;
    case 'V':
        ret = ((cre->flags & RF_CENSORED) > 0);
        break;
    case 'W':
        ret = ((cre->flags & RF_SCRIBBLED) > 0);
        break;
    case 'X':
        ret = ((sum[st_glob.i_current - 1].flags & IF_RETIRED) > 0);
        once &= ~IS_RETIRED;
        break;
    case 'x':
        ret = (once & num); /* once &= ~num; */
        break;
    case 'Y':
        ret = ((sum[st_glob.i_current - 1].flags & IF_FORGOTTEN) > 0);
        once &= ~IS_FORGOTTEN;
        break;
    case 'Z':
        ret = ((sum[st_glob.i_current - 1].flags & IF_FROZEN) > 0);
        once &= ~IS_FROZEN;
        break;
    default:
        return misccond(spp);
    }

    *spp = sp;
    if (!show[depth])
        return 0;
    return (ret != 0) ^ no_t;
}
/******************************************************************************/
/* PROCESS SEPS FOR ITEM SEPS ONLY                                            */
/* This works only for the current conference                                 */
/******************************************************************************/
/* RETURNS: (nothing) */
/* ARGUMENTS: */
/*    Separator string */
/*    Flags (see sep.h) */
/*    Stream to output to */
void
itemsep2(const char **spp, short *fl, FILE *fp)
{
    const char *sp;
    char *sub, neg = 0;
    response_t *cre;
    int cap = 0;
    cre = &(re[st_glob.r_current]);
    sp = *spp;
    num = 0;

    /* Get number */
    zero = 0;
    if (*sp == '^') {
        cap = 1;
        sp++;
    }
    if (*sp == 'z') {
        zero = 1;
        sp++;
    } else if (*sp == 'Z') {
        zero = 2;
        sp++;
    }
    if (*sp == '-') {
        neg = 1;
        sp++;
    }
    while (isdigit(*sp)) {
        num = num * 10 + (*sp - '0');
        sp++;
    }
    if (neg)
        num = -num;

    switch (*(sp++)) {

        /* Item Function Codes */
    case 'a':
        if (!cre->fullname.empty())
            putstring(cre->fullname, fp);
        break;
    case 'C':
        if (confidx >= 0)
            putstring(compress(conflist[confidx].name), fp);
        break;
    case 'h':
        putstring(get_subj(confidx, st_glob.i_current - 1, sum), fp);
        break;
    case 'i':
        number(st_glob.i_current, fp);
        break;
    case 'l':
        if (!cre->login.empty())
            putstring(cre->login, fp);
        break;
    case 'e': {
        uid_t tuid = 0;
        std::string tlogin;
        std::string temail;
        std::string tfullname;
        std::string thome;
        if (!cre->login.empty()) {
            tlogin = cre->login;
            const auto at = tlogin.find('@');
            if (at != std::string::npos &&
                str::eq(std::string_view(tlogin).substr(at + 1), hostname))
                tlogin.erase(at);
            tuid = cre->uid;
            if (get_user(&tuid, tlogin, tfullname, thome, temail))
                putstring(temail, fp);
            else
                putstring(cre->login, fp);
        } else
            temail = email;
        break;
    }
    case 'L':
        wrapout(fp, cre->text[st_glob.l_current].c_str());
        break;
#ifdef NEWS
    case 'm':
        putstring(message_id(compress(conflist[confidx].name),
                      st_glob.i_current, st_glob.r_current, re),
            fp);
        break;
#endif
    case 'n':
        number(sum[st_glob.i_current - 1].nr - 1, fp);
        break;
    case 'N':
        number(st_glob.l_current + 1, fp);
        break;
    case 'r':
        number(st_glob.r_current, fp);
        break;
    case 's':
        number(
            (cre->flags & (RF_SCRIBBLED | RF_EXPIRED)) ? 0 : cre->text.size(),
            fp);
        break;
    case 'k':
        number((cre->numchars + 1023) / 1024, fp);
        break;
    case 'q':
        number(cre->numchars, fp);
        break;
    case 'K': /* KKK */
        break;
    case 'Q': /* KKK */
        break;
    case 'u':
        number((short)cre->uid, fp);
        /* *fl &= ~OF_UID; */
        once &= ~IS_UID;
        break;
    case 'd':
        if (show[depth])
            wfputs(get_date(cre->date, num ? num : 1), fp);
        /* *fl &= ~OF_DATE; */
        once &= ~IS_DATE;
        break;
    case 't':
        if (show[depth])
            wfputs(get_date(cre->date, num), fp);
        /* *fl &= ~OF_DATE; */
        once &= ~IS_DATE;
        break;
    case 'p':
        number((short)cre->parent - 1, fp);
        once &= ~IS_PARENT;
        break;
    case '<': {
        int tmp = once; /* save once */
        const char *angle = strchr(sp, '>');
        if (angle == NULL)
            angle = sp + strlen(sp);
        size_t len = angle - sp;
        sub = estrndup(sp, len);
        sp += len;
        if (*sp == '>')
            sp++;
        once = 0;
        fitemsep(fp, capexpand(sub, DM_VAR, cap), 1);
        free(sub);
        once = tmp;
    } break;
    case '{': {
        int tmp = once; /* save once */
        const char *brace = strchr(sp, '}');
        if (brace == NULL)
            brace = sp + strlen(sp);
        size_t len = brace - sp;
        sub = estrndup(sp, len);
        sp += len;
        if (*sp == '}')
            sp++;
        once = 0;
        /* itemsep(capexpand(sub,DM_VAR,cap),1); */
        fitemsep(fp, capexpand(sub, DM_VAR, cap), 1);
        free(sub);
        once = tmp;
    } break;
    case '(':
        show[depth + 1] = itemcond(&sp, *fl);
        depth++;
        break; /* ) */

    default:
        *spp = sp - 1;
        miscsep(spp, fp);
        return;
    }

    *spp = sp;
}
/******************************************************************************/
/* PROCESS CONDITIONS FOR CONF SEPS ONLY                                      */
/******************************************************************************/
char
confcond(             /* ARGUMENTS:          */
    const char **spp, /* Separator string */
    int idx,          /* Conference index */
    status_t *st)
{
    struct stat stt;
    const char *sp;
    char ret = 0, no_t = 0;

    sp = *spp;
    if (*sp == '!' || *sp == '~') {
        no_t = !no_t;
        sp++;
    }
    for (num = 0; isdigit(*sp); sp++) num = num * 10 + (*sp - '0');
    /*
    for (int i = 0; i < depth; i++) {
        std::print("   ");
        std::print("{:1d}: {:c} ", i, *sp);
    }
    */
    switch (*(sp++)) {
    case 'y':
        lastnum = st->i_unseen;
        ret = lastnum;
        break;
    case 'n':
        lastnum = st->i_brandnew + st->i_newresp;
        ret = lastnum;
        break;
    case 'b':
        lastnum = st->i_brandnew;
        ret = lastnum;
        break;
    case 'r':
        lastnum = st->i_newresp;
        ret = lastnum;
        break;
    case 'm':
        ret = (status & S_MAIL);
        break;
    case 'x':
        ret = (once & num); /* once &= ~num; */
        break;
    case 'N':
        if (num >= 0 && num < CF_PUBLIC && idx >= 0) {
            const auto path =
                str::join("/", {conflist[idx].location, compress(cfiles[num])});
            if (stat(path.c_str(), &stt) || stt.st_size <= 0)
                ret = 0;
            else if (st->c_status & CS_JUSTJOINED)
                ret = 1;
            else
                ret = (stt.st_mtime > st->parttime);
        }
        break;
    case 'F':
        if (num >= 0 && num < CF_PUBLIC && idx >= 0) {
            const auto path =
                str::join("/", {conflist[idx].location, compress(cfiles[num])});
            ret = !stat(path.c_str(), &stt);
        }
        break;
    case 'O':
        ret = (st->c_status & CS_OTHERCONF) ? 1 : 0;
        break;
    case 'C':
        ret = (idx >= 0);
        break;
    case 'i':
        ret = (st->i_first <= st->i_last);
        break;
    case 's':
        ret = (st->c_status & CS_FW);
        break;
    case 'f':
        if (num >= 0 && idx >= 0) {
            const auto path = str::concat({conflist[idx].location, "/sum"});
            ret = !stat(path.c_str(), &stt);
        }
        break;
    case 'j':
        ret = (st->c_status & CS_JUSTJOINED) ? 1 : 0;
        break;
        /* case 'l': ret= (st->c_status & CS_NORESPONSE); break;
         */
    case 'B':
        ret = (idx == confidx);
        break;
    case 'k':
        ret = (once & IS_CFIDX); /* once &= ~IS_CFIDX; */
        break;

    default:
        return misccond(spp);
    }
    /*
    std::println("{}", ret);
    */
    *spp = sp;
    if (!show[depth])
        return 0;
    return (ret != 0) ^ no_t;
}

/******************************************************************************/
/* PROCESS SEPS FOR CONF SEPS ONLY                                            */
/******************************************************************************/
void
confsep2(                            /* ARGUMENTS: */
    const char **spp,                /* Separator string */
    int idx,                         /* Conference index */
    status_t *st, partentry_t *part, /* User participation info */
    FILE *fp                         /* Stream to output to */
)
{
    char *sub, neg = 0;
    const char *sp;
    time_t t;
    int cap = 0;
    sp = *spp;
    num = 0;

    /* Get number */
    zero = 0;
    if (*sp == '^') {
        cap = 1;
        sp++;
    }
    if (*sp == 'z') {
        zero = 1;
        sp++;
    } else if (*sp == 'Z') {
        zero = 2;
        sp++;
    }
    if (*sp == '-') {
        neg = 1;
        sp++;
    }
    while (isdigit(*sp)) {
        num = num * 10 + (*sp - '0');
        sp++;
    }
    if (neg)
        num = -num;

    switch (*(sp++)) {

        /* Conference separators */
#ifdef NEWS
    case 'A':
        number(st->c_article, fp);
        break;
#endif
    case 'y':
        number(st->i_unseen, fp);
        break;
    case 'n':
        number(st->i_brandnew + st->i_newresp, fp);
        break;
    case 'b':
        number(st->i_brandnew, fp);
        break;
    case 'C':
        putstring(st->string, fp);
        break;
    case 'r':
        number(st->i_newresp, fp);
        break;
    case 'N':
        number(st->r_totalnewresp, fp);
        break;
    case 'k':
        number(st->count, fp);
        break;
    case 'u':
        putstring(fullname_in_conference(st), fp);
        break;
    case 'v':
        putstring(login, fp);
        break;
    case 'w':
        putstring(work, fp);
        break;
    case 'f':
        number(st->i_first, fp);
        break;
    case 'L':
        if (idx >= 0)
            putstring(get_desc(compress(conflist[idx].name)), fp);
        break;
    case 'l':
        number(st->i_last, fp);
        break;
    case 'Q':
        if (idx < 0) {
            putstring("Not in a conference!", fp);
            qfail = 1;
        }
        break;
    case 'i':
        number(st->i_numitems, fp);
        break;
    case 't':
        number((short)st->c_security & CT_VISIBLE, fp);
        break;
    case 's':
        if (idx >= 0)
            putstring(compress(conflist[idx].name), fp);
        break;
    case 'p': {
        const auto config = get_config(idx);
        if (config.size() > CF_PARTFILE)
            putstring(config[CF_PARTFILE], fp);
        break;
    }
    case 'd':
        if (idx >= 0)
            putstring(conflist[idx].location, fp);
        break;
    case 'q':
        if (idx >= 0) {
            const char *sh = conflist[idx].location.c_str();
            const char *sh2;
            for (sh2 = sh + strlen(sh) - 1; sh2 >= sh && *sh2 != '/'; sh2--);
            putstring(sh2 + 1, fp);
        }
        break;
    case 'o':
        if (show[depth])
            wfputs(get_date(st->parttime, num), fp);
        break;
    case 'm': /* NEW: lastmod of sum file, if any */
        /*
        if (idx < 0)
            t = 0;
        else {
            const auto path = str::concat({conflist[idx].location, "/sum"});
            t = 0;
            if (stat(path.c_str(), &stt) == 0)
                t = stt.st_mtime;
        }
        */
        t = st->sumtime;
        if (show[depth])
            wfputs(get_date(t, num), fp);
        break;
    case 'g':
        if (num >= 0 && num < CF_PUBLIC && show[depth] && idx >= 0)
            more(conflist[idx].location, compress(cfiles[num]));
        break;
    case '<': {
        int tmp = once; /* save once */
        const char *angle = strchr(sp, '>');
        if (angle == NULL)
            angle = sp + strlen(sp);
        size_t len = angle - sp;
        sub = estrndup(sp, len);
        sp += len;
        if (*sp == '>')
            sp++;
        once = 0;
        confsep(capexpand(sub, DM_VAR, cap), idx, st, part, 1);
        free(sub);
        once = tmp;
    } break;
    case '{': {
        int tmp = once; /* save once */
        const char *brace = strchr(sp, '}');
        if (brace == NULL)
            brace = sp + strlen(sp);
        size_t len = brace - sp;
        sub = estrndup(sp, len);
        sp += len;
        if (*sp == '}')
            sp++;
        once = 0;
        confsep(capexpand(sub, DM_VAR, cap), idx, st, part, 1);
        free(sub);
        once = tmp;
    } break;
    case '(': /* Get number */
        /* for (num=0; isdigit(*sp); sp++) num=
         * num*10+(*sp-'0'); */
        show[depth + 1] = confcond(&sp, idx, st); /* for ultrix */
        depth++;
        break; /* ) */

    default:
        *spp = sp - 1;
        miscsep(spp, fp);
        return;
    }

    *spp = sp;
}
/******************************************************************************/
/* SET "ONCE-ONLY" FLAGS VALUE                                                */
/******************************************************************************/
void
sepinit(  /* ARGUMENTS:         */
    int x /* Flags to set    */
)
{
    once |= x;
}

/* sep: Separator variable */
/* fl: Force %c? */
void
fitemsep(FILE *fp, const std::string_view &sep, int fl)
{
    const char *sp, *tp;
    response_t *cre;
    int start_depth = depth, start_show = show[depth];
    int start_newline = newline;
    char str[1024];

    if (sep.empty())
        return;

    const auto len = std::min(sizeof(str) - 1, sep.size());
    memcpy(str, sep.data(), len);
    str[len] = 0;

    /* Force %c */
    if (fl)
        strlcat(str, "%c", sizeof(str));

    /* get status without trashing subj's in memory */
    cre = &(re[st_glob.r_current]);

    init_show();
    newline = 1;
    sp = str;

    for (;;) {
        switch (*sp) {
        case '$': {
            int cap = 0;
            if (sp[1] == '^') {
                cap = 1;
                sp++;
            }
            if (sp[1] != '{') {
                if (show[depth])
                    wfputc(*sp++, fp);
                else
                    sp++;
            } else {
                sp += 2;
                const char *brace = strchr(sp, '}');
                if (brace == NULL)
                    brace = sp + strlen(brace);
                size_t len = brace - sp;
                char *sub = estrndup(sp, len);
                sp += len;
                if (*sp == '}')
                    sp++;
                if (show[depth])
                    wfputs(capexpand(sub, DM_VAR, cap), fp);
                free(sub);
            }
            break;
        }
        case '%':
            sp++;
            itemsep2(&sp, &st_glob.opt_flags, fp);
            break;
        case '\0':
            if ((once & IS_UID) &&
                ((st_glob.opt_flags & OF_UID) || (flags & O_UID)))
                std::print(fp, " uid {}", cre->uid);
            if ((once & IS_DATE) &&
                ((st_glob.opt_flags & OF_DATE) || (flags & O_DATE)))
                std::print(fp, " on {:.24}", get_date(cre->date, 0));
            if ((once & IS_RETIRED) &&
                (sum[st_glob.i_current - 1].flags & IF_RETIRED))
                std::print(fp, "\n   <{} is retired>", topic());
            if ((once & IS_FORGOTTEN) &&
                (sum[st_glob.i_current - 1].flags & IF_FORGOTTEN))
                std::print(fp, "\n   <{} is forgotten>", topic());
            if ((once & IS_FROZEN) &&
                (sum[st_glob.i_current - 1].flags & IF_FROZEN))
                std::print(fp, "\n   <{} is frozen>", topic());
            if ((once & IS_LINKED) &&
                (sum[st_glob.i_current - 1].flags & IF_LINKED))
                std::print(fp, "\n   <linked {}>", topic());
            if ((once & IS_PARENT) && (cre->parent > 0))
                std::print(fp, "   <response to #{}>", cre->parent - 1);

            if (once & IS_CENSORED) {
                if (cre->flags & RF_EXPIRED)
                    wfputs("   <expired>", fp);
                else if (cre->flags & RF_SCRIBBLED) {
                    if (cre->numchars > 8 && !cre->text.empty() &&
                        (flags & O_SCRIBBLER)) {
                        char buff[9];
                        int i;
                        for (i = 0; i < 8 && cre->text[0][i] != ' '; i++)
                            buff[i] = cre->text[0][i];
                        buff[i] = '\0';
                        std::println(
                            fp, "   <censored & scribbled by {}>", buff);
                    } else {
                        int tmp = once; /* save once */
                        once = 0;
                        itemsep(expand("scribbled", DM_VAR), 1);
                        once = tmp;
                        /* wfputs(expand("scribbled", DM_VAR),fp); */
                    }
                } else if (cre->flags & RF_CENSORED) {
                    int tmp = once; /* save once */
                    once = 0;
                    itemsep(expand("censored", DM_VAR), 1);
                    once = tmp;
                    /* wfputs(expand("censored", DM_VAR),fp); */
                }
            }
            if (newline)
                wfputc('\n', fp);
            once = 0;
            if (fp)
                fflush(fp);      /* flush when done with
                                  * wfput stuff */
            depth = start_depth; /* restore original depth */
            show[depth] = start_show;
            newline = start_newline;
            return;
        case '\\': /* Translate lchar into rchar */
            sp++;
            tp = strchr(lchars, *sp);
            if (tp) { /* if *sp is 0 byte, will insert a 0
                       * byte */
                if (show[depth])
                    wfputc(rchars[tp - lchars], fp);
                sp++;
                break;
            } /* else fall through into default */
        default:
            if (show[depth])
                wfputc(*sp++, fp);
            else
                sp++;
        }
    }
}
/******************************************************************************/
/* PROCESS ITEMSEP STRING                                                     */
/* Output to pipe, if one is open, else to stdout                             */
/******************************************************************************/
void
itemsep(                         /* ARGUMENTS: */
    const std::string_view &sep, /* Separator variable */
    int fl                       /* Force %c? */
)
{
    FILE *fp;

    if (status & S_EXECUTE)
        fp = 0;
    else if (status & S_PAGER)
        fp = st_glob.outp;
    else
        fp = stdout;
    fitemsep(fp, sep, fl);
}
/******************************************************************************/
/* PROCESS CONFSEP STRING                                                     */
/******************************************************************************/
void
confsep(                             /* ARGUMENTS:                         */
    const std::string_view &sep,     /* Sep string to process              */
    int idx,                         /* Index of which cf we're processing */
    status_t *st, partentry_t *part, /* User participation info */
    int fl                           /* Force %c? */
)
{
    const char *sp, *tp;
    FILE *fp;
    char str[1024];
    int start_depth = depth; /* save original depth */
    int start_show = show[depth];
    int start_newline = newline;

    /*
      str = expand(sep,DM_VAR);
      if (!str) str=sep;
    */
    if (sep.empty())
        return;

    auto len = std::min(sizeof(str) - 1, sep.size());
    memcpy(str, sep.data(), len);
    str[len] = 0;

    /* Compatibility: force "...prompt" to end in \c */
    if (fl)
        strlcat(str, "%c", sizeof(str));

    if (status & S_EXECUTE)
        fp = 0;
    else if (status & S_PAGER)
        fp = st_glob.outp;
    else
        fp = stdout;

    init_show();
    newline = 1;
    qfail = 0;
    sp = str;

    while (!qfail) {
        switch (*sp) {
        case '$': {
            char *sub;
            int cap = 0;
            if (sp[1] == '^') {
                cap = 1;
                sp++;
            }
            if (sp[1] != '{') {
                if (show[depth])
                    wfputc(*sp++, fp);
                else
                    sp++;
            } else {
                sp += 2;
                const char *brace = strchr(sp, '}');
                if (brace == NULL)
                    brace = brace + strlen(sp);
                size_t len = brace - sp;
                sub = estrndup(sp, len);
                sp += len;
                if (*sp == '}')
                    sp++;
                if (show[depth])
                    wfputs(capexpand(sub, DM_VAR, cap), fp);
                free(sub);
            }
            break;
        }
        case '%':
            sp++;
            confsep2(&sp, idx, st, part, fp);
            break;
        case '\0':
            if (newline)
                wfputc('\n', fp);
            once = 0;
            if (fp)
                fflush(fp);      /* flush when done with
                                  * wfput stuff */
            depth = start_depth; /* restore original depth */
            show[depth] = start_show;
            newline = start_newline;
            return;
        case '\\': /* Translate lchar into rchar */
            sp++;
            tp = strchr(lchars, *sp);
            if (tp) { /* if *sp is 0 byte, will insert a 0
                       * byte */
                if (show[depth])
                    wfputc(rchars[tp - lchars], fp);
                sp++;
                break;
            } /* else fall through into default */
        default:
            if (show[depth])
                wfputc(*sp++, fp);
            else
                sp++;
            break;
        }
    }
    if (newline)
        wfputc('\n', fp);
    if (fp)
        fflush(fp);
    depth = start_depth; /* restore original depth */
    show[depth] = start_show;
    newline = start_newline;
}

char *
get_sep(const char **pEptr)
{
    static char buff[MAX_LINE_LENGTH];
    char oldeval[MAX_LINE_LENGTH];

    int tmp_status;
    tmp_status = status;
    strcpy(oldeval, evalbuf);
    switch (mode) {
    case M_RFP:
    case M_TEXT:
    case M_EDB:
        evalbuf[0] = '\0';
        status |= S_EXECUTE;
        itemsep2(pEptr, &st_glob.opt_flags, NULL);
        status &= ~S_EXECUTE;
        break;

    case M_OK:
    case M_JOQ:
    default:
        evalbuf[0] = '\0';
        status |= S_EXECUTE;
        confsep2(pEptr, confidx, &st_glob, part, NULL);
        status &= ~S_EXECUTE;
        break;
    }
    strcpy(buff, evalbuf);
    strcpy(evalbuf, oldeval);
    status = tmp_status;

    return buff;
}
