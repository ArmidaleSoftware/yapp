// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include "arch.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <iterator>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include "globals.h"
#include "item.h"
#include "lib.h"
#include "mem.h"
#include "news.h"
#include "str.h"
#include "struct.h"
#include "sum.h"
#include "yapp.h"

#ifdef NEWS
void
check_news(response_t *re)
{
	/* News article? */
	if (!re->text.empty() && re->text[0].starts_with(",N")) {
		/* Get article # */
		if (sscanf(re->text[0].c_str() + 2, "%ld", &re->article) > 0 && re->article) {
			re->text.clear();
			get_article(re);
		}
		/* Get message id# */
		if (re->text.size() > 1 && re->text[1].starts_with(",M"))
			re->mid = re->text[1].substr(2);
	}
}
#endif

/******************************************************************************/
/* READ IN A SINGLE RESPONSE                                                  */
/* Starting at current file position, read in a response.  The ending file    */
/* position will be the start of the next response.  Also, allocates space    */
/* for text which needs to be freed.                                          */
/* Assumes (sum) that this is always done within the current conference       */
/******************************************************************************/
void
get_resp(           /* ARGUMENTS                      */
    FILE *fp,       /* Current file position       */
    response_t *re, /* Response to fill in         */
    int fast,       /* Don't save the actual text? */
    int num)
{
	std::string buff;
	char done = 0;

	/* Get response */
	if (re->offset >= 0 && re->numchars > 0 && fast == GR_ALL) {
		if (fseek(fp, re->textoff, 0)) {
			auto off = std::to_string(re->textoff);
			error("fseeking to ", off);
		}
		auto text = grab_more(fp, ",E", NULL);
		if ((flags & O_SIGNATURE) == 0 && (st_glob.c_security & CT_EMAIL) != 0) {
			auto it = std::find(std::begin(text), std::end(text), "--");
			text.erase(it, std::end(text));
		}
		re->text = text;
#ifdef NEWS
		check_news(re);
#endif
		return;
	}
	if (re->offset >= 0 && fseek(fp, re->offset, 0)) {
		error("fseeking to ", std::format("{}", re->textoff));
	}
	if (re->offset < 0) { /* Find start of response */
		short i, j;
		for (i = 1; i <= num && re[-i].endoff < 0; i++)
			; /* find prev offset */
		for (j = i - 1; j > 0; j--) {
			get_resp(fp, &(re[-j]), GR_OFFSET, num - j);
		}
		if (num && fseek(fp, re[-1].endoff, 0)) {
			error("fseeking to ", std::format("{}", re[-1].endoff));
		}
	}
	if (fast == GR_OFFSET) {
		re->offset = ftell(fp);
		while (ngets(buff, fp))
			if (buff.starts_with(",T"))
				break;
		re->textoff = ftell(fp);
		for (;;) {
			if (!ngets(buff, fp) || buff.starts_with(",E")) {
				re->endoff = ftell(fp);
				break;
			}
			if (buff.starts_with(",R")) {
				re->endoff = ftell(fp) - buff.size() - 1;
				break;
			}
		}
		re->numchars = -1;
	} else {

#ifdef NEWS
		re->mid.clear();
		re->article = 0;
#endif
		re->parent = 0;

		while (!done && !(status & S_INT)) {
			if (!ngets(buff, fp))
				break; /* UNK error */
			if (buff.size() < 2)
				continue;
			switch (buff[1]) {
			case 'A':
				re->fullname = buff.substr(2);
				break;
#if (SIZEOF_LONG == 8)
			case 'D':
				sscanf(buff.c_str() + 2, "%x", &(re->date));
				break;
#else
			case 'D':
				sscanf(buff.c_str() + 2, "%lx", &(re->date));
				break;
#endif
			case 'E':
				done = 1;
				re->endoff = ftell(fp);
				break;
			case 'R':
				re->offset = ftell(fp) - buff.size() - 1;
				sscanf(buff.c_str() + 2, "%hx", &(re->flags));
				break;
			/*
		        case 'M':
				re->mid = buff.substr(2);
				break;
			*/
			case 'P':
				sscanf(buff.c_str() + 2, "%hd", &(re->parent));
				re->parent++;
				break;
			case 'T':
				re->textoff = ftell(fp);
				re->numchars = 0;
				if (fast == GR_ALL) {
					size_t endlen;
					re->text = grab_more(fp, ",E", &endlen);
					re->numchars = /*-",E"*/
					    ftell(fp) - re->textoff - (endlen + 1);
				} else {
					while (ngets(buff, fp) &&
					       !buff.starts_with(",E") &&
					       !buff.starts_with(",R"))
						;
					re->text.clear(); /*-",E..." */
					re->numchars = ftell(fp) - re->textoff -
					               (buff.size() + 1);
				}
#ifdef NEWS
				check_news(re);
#endif
				done = 1;
				break;
			case 'U': {
				const auto who = str::split(buff.c_str() + 2, ",", false);
				const auto n = who.size();
				re->uid = 0;
				if (n > 0)
					re->uid = str::toi(who[0]);
				re->login = (n > 1) ? std::string(who[1]) : "Unknown";
				break;
			}
			}
		}
	}
	if (debug & DB_ARCH)
		std::println(
		    "get_resp: returning response author {} date {} flags {} textoff {}",
		    re->login, get_date(re->date, 0), re->flags, re->textoff);
}
/******************************************************************************/
/* READ IN INFORMATION SUMMARIZING ALL THE RESPONSES IN AN ITEM               */
/* Note that this is currently only used within the current cf, but could     */
/* easily be used for a remote cf by passing in the right sum & re, confidx   */
/******************************************************************************/
void
get_item(                         /* ARGUMENTS:                            */
    FILE *fp,                     /* File pointer to read from          */
    int n,                        /* Which item # we're reading         */
    response_t re[MAX_RESPONSES], /* Buffer array to hold response info */
    sumentry_t sum[MAX_ITEMS]     /* Buffer array holding info on items */
)
{
	short i;
	long offset = 0;
	/* For each response */
	for (i = 0; i < MAX_RESPONSES; i++) {
		re[i].offset = re[i].endoff = -1;
		re[i].date = 0;
	}

	/* Find EOF */
	fseek(fp, 0L, 2);
	offset = ftell(fp);
	rewind(fp);

	/* Get all responses, and fix sum file NR value */
	for (i = 0; !i || re[i - 1].endoff < offset; i++) {
		get_resp(fp, &(re[i]), (short)GR_OFFSET, i);
		if (debug & DB_ARCH)
			std::println("{:2} Offset = {:4} Textoff = {:4}", i,
			    re[i].offset, re[i].textoff);
	}
	if (sum[n - 1].nr != i) {
		sum[n - 1].nr = i;
		save_sum(sum, (short)(n - 1), confidx, &st_glob);
		dirty_sum(n - 1);
	}
}
