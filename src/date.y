%{
/*
 * Portable OpenCVS changes only; original notices below remain in force.
 *
 * Copyright (c) 2026, David Uhden Collado <david@uhden.dev>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*	$OpenBSD: date.y,v 1.27 2023/03/08 04:43:10 guenther Exp $	*/

/*
**  Originally written by Steven M. Bellovin <smb@research.att.com> while
**  at the University of North Carolina at Chapel Hill.  Later tweaked by
**  a couple of people on Usenet.  Completely overhauled by Rich $alz
**  <rsalz@bbn.com> and Jim Berets <jberets@bbn.com> in August, 1990;
**
**  This grammar has 10 shift/reduce conflicts.
**
**  This code is in the public domain and has no copyright.
*/
/* SUPPRESS 287 on yaccpar_sccsid *//* Unused static variable */
/* SUPPRESS 288 on yyerrlab *//* Label unused */

#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>
#include <time.h>

#include "cvs.h"

#define YEAR_EPOCH	1970
#define YEAR_TMORIGIN	1900
#define HOUR(x)		((time_t)(x) * 60)
#define SECSPERDAY	(24L * 60L * 60L)


/* An entry in the lexical lookup table */
typedef struct _TABLE {
	char	*name;
	int	type;
	time_t	value;
} TABLE;


/*  Daylight-savings mode:  on, off, or not yet known. */
typedef enum _DSTMODE {
	DSTon, DSToff, DSTmaybe
} DSTMODE;

/*  Meridian:  am, pm, or 24-hour style. */
typedef enum _MERIDIAN {
	MERam, MERpm, MER24
} MERIDIAN;


/*
 *  Global variables.  We could get rid of most of these by using a good
 *  union as the yacc stack.  (This routine was originally written before
 *  yacc had the %union construct.)  Maybe someday; right now we only use
 *  the %union very rarely.
 */
static const char	*yyInput;
static DSTMODE	yyDSTmode;
static time_t	yyDayOrdinal;
static time_t	yyDayNumber;
static int	yyHaveDate;
static int	yyHaveDay;
static int	yyHaveRel;
static int	yyHaveTime;
static int	yyHaveZone;
static time_t	yyTimezone;
static time_t	yyDay;
static time_t	yyHour;
static time_t	yyMinutes;
static time_t	yyMonth;
static time_t	yySeconds;
static time_t	yyYear;
static MERIDIAN	yyMeridian;
static time_t	yyRelMonth;
static time_t	yyRelSeconds;
static int yyDateError;


static int	yyerror(const char *);
static int	yylex(void);
static int	yyparse(void);
static int	lookup(char *);

/* All supported targets provide a signed integral time_t no wider than intmax_t. */
typedef char pc_date_signed_time[((time_t)-1 < 0) ? 1 : -1];
typedef char pc_date_time_width[(sizeof(time_t) <= sizeof(intmax_t)) ? 1 : -1];
#define DATE_TIME_MAX ((intmax_t)(UINTMAX_MAX >> \
    ((sizeof(uintmax_t) - sizeof(time_t)) * CHAR_BIT + 1)))
#define DATE_TIME_MIN (-DATE_TIME_MAX - 1)

static time_t
date_failure(void)
{
	yyDateError = 1;
	return 0;
}

static time_t
date_add(time_t left, time_t right)
{
	intmax_t a = (intmax_t)left, b = (intmax_t)right;

	if ((b > 0 && a > DATE_TIME_MAX - b) ||
	    (b < 0 && a < DATE_TIME_MIN - b))
		return date_failure();
	return (time_t)(a + b);
}

static time_t
date_subtract(time_t left, time_t right)
{
	intmax_t a = (intmax_t)left, b = (intmax_t)right;

	if ((b > 0 && a < DATE_TIME_MIN + b) ||
	    (b < 0 && a > DATE_TIME_MAX + b))
		return date_failure();
	return (time_t)(a - b);
}

static time_t
date_multiply(time_t left, time_t right)
{
	intmax_t a = (intmax_t)left, b = (intmax_t)right;

	if ((a > 0 && ((b > 0 && a > DATE_TIME_MAX / b) ||
	    (b < 0 && b < DATE_TIME_MIN / a))) ||
	    (a < 0 && ((b > 0 && a < DATE_TIME_MIN / b) ||
	    (b < 0 && a < DATE_TIME_MAX / b))))
		return date_failure();
	return (time_t)(a * b);
}

static time_t
date_negate(time_t value)
{
	return date_subtract(0, value);
}

/* Signed numeric zones are HHMM; reject invalid minute/hour components. */
static time_t
date_zone(time_t value)
{
	time_t magnitude = value < 0 ? date_negate(value) : value;

	if (yyDateError || magnitude > 2359 || magnitude % 100 > 59)
		return date_failure();
	return date_negate(date_add(value % 100,
	    date_multiply(value / 100, 60)));
}

%}

%union {
	time_t		Number;
	enum _MERIDIAN	Meridian;
}

%token	tAGO tDAY tDAYZONE tID tMERIDIAN tMINUTE_UNIT tMONTH tMONTH_UNIT
%token	tSEC_UNIT tSNUMBER tUNUMBER tZONE tDST

%type	<Number>	tDAY tDAYZONE tMINUTE_UNIT tMONTH tMONTH_UNIT
%type	<Number>	tSEC_UNIT tSNUMBER tUNUMBER tZONE
%type	<Meridian>	tMERIDIAN o_merid

%%

spec	: /* NULL */
	| spec item
	;

item	: time {
		yyHaveTime = 1 + (yyHaveTime != 0);
	}
	| zone {
		yyHaveZone = 1 + (yyHaveZone != 0);
	}
	| date {
		yyHaveDate = 1 + (yyHaveDate != 0);
	}
	| day {
		yyHaveDay = 1 + (yyHaveDay != 0);
	}
	| rel {
		yyHaveRel = 1;
	}
	| number
	;

time	: tUNUMBER tMERIDIAN {
		yyHour = $1;
		yyMinutes = 0;
		yySeconds = 0;
		yyMeridian = $2;
	}
	| tUNUMBER ':' tUNUMBER o_merid {
		yyHour = $1;
		yyMinutes = $3;
		yySeconds = 0;
		yyMeridian = $4;
	}
	| tUNUMBER ':' tUNUMBER tSNUMBER {
		yyHour = $1;
		yyMinutes = $3;
		yyMeridian = MER24;
		yyDSTmode = DSToff;
		yyTimezone = date_zone($4);
	}
	| tUNUMBER ':' tUNUMBER ':' tUNUMBER o_merid {
		yyHour = $1;
		yyMinutes = $3;
		yySeconds = $5;
		yyMeridian = $6;
	}
	| tUNUMBER ':' tUNUMBER ':' tUNUMBER tSNUMBER {
		yyHour = $1;
		yyMinutes = $3;
		yySeconds = $5;
		yyMeridian = MER24;
		yyDSTmode = DSToff;
		yyTimezone = date_zone($6);
	}
	;

zone	: tZONE {
		yyTimezone = $1;
		yyDSTmode = DSToff;
	}
	| tDAYZONE {
		yyTimezone = $1;
		yyDSTmode = DSTon;
	}
	| tZONE tDST {
		yyTimezone = $1;
		yyDSTmode = DSTon;
	}
	;

day	: tDAY {
		yyDayOrdinal = 1;
		yyDayNumber = $1;
	}
	| tDAY ',' {
		yyDayOrdinal = 1;
		yyDayNumber = $1;
	}
	| tUNUMBER tDAY {
		yyDayOrdinal = $1;
		yyDayNumber = $2;
	}
	;

date	: tUNUMBER '/' tUNUMBER {
		yyMonth = $1;
		yyDay = $3;
	}
	| tUNUMBER '/' tUNUMBER '/' tUNUMBER {
		if ($1 >= 100) {
			yyYear = $1;
			yyMonth = $3;
			yyDay = $5;
		} else {
			yyMonth = $1;
			yyDay = $3;
			yyYear = $5;
		}
	}
	| tUNUMBER tSNUMBER tSNUMBER {
		/* ISO 8601 format.  yyyy-mm-dd.  */
		yyYear = $1;
		yyMonth = date_negate($2);
		yyDay = date_negate($3);
	}
	| tUNUMBER tMONTH tSNUMBER {
		/* e.g. 17-JUN-1992.  */
		yyDay = $1;
		yyMonth = $2;
		yyYear = date_negate($3);
	}
	| tMONTH tUNUMBER {
		yyMonth = $1;
		yyDay = $2;
	}
	| tMONTH tUNUMBER ',' tUNUMBER {
		yyMonth = $1;
		yyDay = $2;
		yyYear = $4;
	}
	| tUNUMBER tMONTH {
		yyMonth = $2;
		yyDay = $1;
	}
	| tUNUMBER tMONTH tUNUMBER {
		yyMonth = $2;
		yyDay = $1;
		yyYear = $3;
	}
	;

rel	: relunit tAGO {
		yyRelSeconds = date_negate(yyRelSeconds);
		yyRelMonth = date_negate(yyRelMonth);
	}
	| relunit
	;

relunit	: tUNUMBER tMINUTE_UNIT {
		yyRelSeconds = date_add(yyRelSeconds,
		    date_multiply(date_multiply($1, $2), 60));
	}
	| tSNUMBER tMINUTE_UNIT {
		yyRelSeconds = date_add(yyRelSeconds,
		    date_multiply(date_multiply($1, $2), 60));
	}
	| tMINUTE_UNIT {
		yyRelSeconds = date_add(yyRelSeconds, date_multiply($1, 60));
	}
	| tSNUMBER tSEC_UNIT {
		yyRelSeconds = date_add(yyRelSeconds, $1);
	}
	| tUNUMBER tSEC_UNIT {
		yyRelSeconds = date_add(yyRelSeconds, $1);
	}
	| tSEC_UNIT {
		yyRelSeconds = date_add(yyRelSeconds, 1);
	}
	| tSNUMBER tMONTH_UNIT {
		yyRelMonth = date_add(yyRelMonth, date_multiply($1, $2));
	}
	| tUNUMBER tMONTH_UNIT {
		yyRelMonth = date_add(yyRelMonth, date_multiply($1, $2));
	}
	| tMONTH_UNIT {
		yyRelMonth = date_add(yyRelMonth, $1);
	}
	;

number	: tUNUMBER {
		if (yyHaveTime && yyHaveDate && !yyHaveRel)
			yyYear = $1;
		else {
			if ($1 > 10000) {
				yyHaveDate = 1 + (yyHaveDate != 0);
				yyDay= ($1)%100;
				yyMonth= ($1/100)%100;
				yyYear = $1/10000;
			} else {
				yyHaveTime = 1 + (yyHaveTime != 0);
				if ($1 < 100) {
					yyHour = $1;
					yyMinutes = 0;
				} else {
					yyHour = $1 / 100;
					yyMinutes = $1 % 100;
				}
				yySeconds = 0;
				yyMeridian = MER24;
			}
		}
	}
	;

o_merid	: /* NULL */ {
		$$ = MER24;
	}
	| tMERIDIAN {
		$$ = $1;
	}
	;

%%

/* Month and day table. */
static TABLE const MonthDayTable[] = {
	{ "january",	tMONTH,	1 },
	{ "february",	tMONTH,	2 },
	{ "march",	tMONTH,	3 },
	{ "april",	tMONTH,	4 },
	{ "may",	tMONTH,	5 },
	{ "june",	tMONTH,	6 },
	{ "july",	tMONTH,	7 },
	{ "august",	tMONTH,	8 },
	{ "september",	tMONTH,	9 },
	{ "sept",	tMONTH,	9 },
	{ "october",	tMONTH,	10 },
	{ "november",	tMONTH,	11 },
	{ "december",	tMONTH,	12 },
	{ "sunday",	tDAY,	0 },
	{ "monday",	tDAY,	1 },
	{ "tuesday",	tDAY,	2 },
	{ "tues",	tDAY,	2 },
	{ "wednesday",	tDAY,	3 },
	{ "wednes",	tDAY,	3 },
	{ "thursday",	tDAY,	4 },
	{ "thur",	tDAY,	4 },
	{ "thurs",	tDAY,	4 },
	{ "friday",	tDAY,	5 },
	{ "saturday",	tDAY,	6 },
	{ NULL, 0, 0 }
};

/* Time units table. */
static TABLE const UnitsTable[] = {
	{ "year",	tMONTH_UNIT,	12 },
	{ "month",	tMONTH_UNIT,	1 },
	{ "fortnight",	tMINUTE_UNIT,	14 * 24 * 60 },
	{ "week",	tMINUTE_UNIT,	7 * 24 * 60 },
	{ "day",	tMINUTE_UNIT,	1 * 24 * 60 },
	{ "hour",	tMINUTE_UNIT,	60 },
	{ "minute",	tMINUTE_UNIT,	1 },
	{ "min",	tMINUTE_UNIT,	1 },
	{ "second",	tSEC_UNIT,	1 },
	{ "sec",	tSEC_UNIT,	1 },
	{ NULL, 0, 0 }
};

/* Assorted relative-time words. */
static TABLE const OtherTable[] = {
	{ "tomorrow",	tMINUTE_UNIT,	1 * 24 * 60 },
	{ "yesterday",	tMINUTE_UNIT,	-1 * 24 * 60 },
	{ "today",	tMINUTE_UNIT,	0 },
	{ "now",	tMINUTE_UNIT,	0 },
	{ "last",	tUNUMBER,	-1 },
	{ "this",	tMINUTE_UNIT,	0 },
	{ "next",	tUNUMBER,	2 },
	{ "first",	tUNUMBER,	1 },
/*  { "second",		tUNUMBER,	2 }, */
	{ "third",	tUNUMBER,	3 },
	{ "fourth",	tUNUMBER,	4 },
	{ "fifth",	tUNUMBER,	5 },
	{ "sixth",	tUNUMBER,	6 },
	{ "seventh",	tUNUMBER,	7 },
	{ "eighth",	tUNUMBER,	8 },
	{ "ninth",	tUNUMBER,	9 },
	{ "tenth",	tUNUMBER,	10 },
	{ "eleventh",	tUNUMBER,	11 },
	{ "twelfth",	tUNUMBER,	12 },
	{ "ago",	tAGO,	1 },
	{ NULL, 0, 0 }
};

/* The timezone table. */
/* Some of these are commented out because a time_t can't store a float. */
static TABLE const TimezoneTable[] = {
	{ "gmt",	tZONE,     HOUR( 0) },	/* Greenwich Mean */
	{ "ut",		tZONE,     HOUR( 0) },	/* Universal (Coordinated) */
	{ "utc",	tZONE,     HOUR( 0) },
	{ "wet",	tZONE,     HOUR( 0) },	/* Western European */
	{ "bst",	tDAYZONE,  HOUR( 0) },	/* British Summer */
	{ "wat",	tZONE,     HOUR( 1) },	/* West Africa */
	{ "at",		tZONE,     HOUR( 2) },	/* Azores */
#if	0
	/* For completeness.  BST is also British Summer, and GST is
	 * also Guam Standard. */
	{ "bst",	tZONE,     HOUR( 3) },	/* Brazil Standard */
	{ "gst",	tZONE,     HOUR( 3) },	/* Greenland Standard */
#endif
#if 0
	{ "nft",	tZONE,     HOUR(3.5) },	/* Newfoundland */
	{ "nst",	tZONE,     HOUR(3.5) },	/* Newfoundland Standard */
	{ "ndt",	tDAYZONE,  HOUR(3.5) },	/* Newfoundland Daylight */
#endif
	{ "ast",	tZONE,     HOUR( 4) },	/* Atlantic Standard */
	{ "adt",	tDAYZONE,  HOUR( 4) },	/* Atlantic Daylight */
	{ "est",	tZONE,     HOUR( 5) },	/* Eastern Standard */
	{ "edt",	tDAYZONE,  HOUR( 5) },	/* Eastern Daylight */
	{ "cst",	tZONE,     HOUR( 6) },	/* Central Standard */
	{ "cdt",	tDAYZONE,  HOUR( 6) },	/* Central Daylight */
	{ "mst",	tZONE,     HOUR( 7) },	/* Mountain Standard */
	{ "mdt",	tDAYZONE,  HOUR( 7) },	/* Mountain Daylight */
	{ "pst",	tZONE,     HOUR( 8) },	/* Pacific Standard */
	{ "pdt",	tDAYZONE,  HOUR( 8) },	/* Pacific Daylight */
	{ "yst",	tZONE,     HOUR( 9) },	/* Yukon Standard */
	{ "ydt",	tDAYZONE,  HOUR( 9) },	/* Yukon Daylight */
	{ "hst",	tZONE,     HOUR(10) },	/* Hawaii Standard */
	{ "hdt",	tDAYZONE,  HOUR(10) },	/* Hawaii Daylight */
	{ "cat",	tZONE,     HOUR(10) },	/* Central Alaska */
	{ "ahst",	tZONE,     HOUR(10) },	/* Alaska-Hawaii Standard */
	{ "nt",		tZONE,     HOUR(11) },	/* Nome */
	{ "idlw",	tZONE,     HOUR(12) },	/* International Date Line West */
	{ "cet",	tZONE,     -HOUR(1) },	/* Central European */
	{ "met",	tZONE,     -HOUR(1) },	/* Middle European */
	{ "mewt",	tZONE,     -HOUR(1) },	/* Middle European Winter */
	{ "mest",	tDAYZONE,  -HOUR(1) },	/* Middle European Summer */
	{ "swt",	tZONE,     -HOUR(1) },	/* Swedish Winter */
	{ "sst",	tDAYZONE,  -HOUR(1) },	/* Swedish Summer */
	{ "fwt",	tZONE,     -HOUR(1) },	/* French Winter */
	{ "fst",	tDAYZONE,  -HOUR(1) },	/* French Summer */
	{ "eet",	tZONE,     -HOUR(2) },	/* Eastern Europe, USSR Zone 1 */
	{ "bt",		tZONE,     -HOUR(3) },	/* Baghdad, USSR Zone 2 */
#if 0
	{ "it",		tZONE,     -HOUR(3.5) },/* Iran */
#endif
	{ "zp4",	tZONE,     -HOUR(4) },	/* USSR Zone 3 */
	{ "zp5",	tZONE,     -HOUR(5) },	/* USSR Zone 4 */
#if 0
	{ "ist",	tZONE,     -HOUR(5.5) },/* Indian Standard */
#endif
	{ "zp6",	tZONE,     -HOUR(6) },	/* USSR Zone 5 */
#if	0
	/* For completeness.  NST is also Newfoundland Stanard, and SST is
	 * also Swedish Summer. */
	{ "nst",	tZONE,     -HOUR(6.5) },/* North Sumatra */
	{ "sst",	tZONE,     -HOUR(7) },	/* South Sumatra, USSR Zone 6 */
#endif	/* 0 */
	{ "wast",	tZONE,     -HOUR(7) },	/* West Australian Standard */
	{ "wadt",	tDAYZONE,  -HOUR(7) },	/* West Australian Daylight */
#if 0
	{ "jt",		tZONE,     -HOUR(7.5) },/* Java (3pm in Cronusland!) */
#endif
	{ "cct",	tZONE,     -HOUR(8) },	/* China Coast, USSR Zone 7 */
	{ "jst",	tZONE,     -HOUR(9) },	/* Japan Standard, USSR Zone 8 */
#if 0
	{ "cast",	tZONE,     -HOUR(9.5) },/* Central Australian Standard */
	{ "cadt",	tDAYZONE,  -HOUR(9.5) },/* Central Australian Daylight */
#endif
	{ "east",	tZONE,     -HOUR(10) },	/* Eastern Australian Standard */
	{ "eadt",	tDAYZONE,  -HOUR(10) },	/* Eastern Australian Daylight */
	{ "gst",	tZONE,     -HOUR(10) },	/* Guam Standard, USSR Zone 9 */
	{ "nzt",	tZONE,     -HOUR(12) },	/* New Zealand */
	{ "nzst",	tZONE,     -HOUR(12) },	/* New Zealand Standard */
	{ "nzdt",	tDAYZONE,  -HOUR(12) },	/* New Zealand Daylight */
	{ "idle",	tZONE,     -HOUR(12) },	/* International Date Line East */
	{ NULL, 0, 0 }
};

/* Military timezone table. */
static TABLE const MilitaryTable[] = {
	{ "a",	tZONE,	HOUR(  1) },
	{ "b",	tZONE,	HOUR(  2) },
	{ "c",	tZONE,	HOUR(  3) },
	{ "d",	tZONE,	HOUR(  4) },
	{ "e",	tZONE,	HOUR(  5) },
	{ "f",	tZONE,	HOUR(  6) },
	{ "g",	tZONE,	HOUR(  7) },
	{ "h",	tZONE,	HOUR(  8) },
	{ "i",	tZONE,	HOUR(  9) },
	{ "k",	tZONE,	HOUR( 10) },
	{ "l",	tZONE,	HOUR( 11) },
	{ "m",	tZONE,	HOUR( 12) },
	{ "n",	tZONE,	HOUR(- 1) },
	{ "o",	tZONE,	HOUR(- 2) },
	{ "p",	tZONE,	HOUR(- 3) },
	{ "q",	tZONE,	HOUR(- 4) },
	{ "r",	tZONE,	HOUR(- 5) },
	{ "s",	tZONE,	HOUR(- 6) },
	{ "t",	tZONE,	HOUR(- 7) },
	{ "u",	tZONE,	HOUR(- 8) },
	{ "v",	tZONE,	HOUR(- 9) },
	{ "w",	tZONE,	HOUR(-10) },
	{ "x",	tZONE,	HOUR(-11) },
	{ "y",	tZONE,	HOUR(-12) },
	{ "z",	tZONE,	HOUR(  0) },
	{ NULL, 0, 0 }
};


static int
yyerror(const char *s)
{
#if !defined(TEST)
	char *str;

	(void)xasprintf(&str, "parsing date string: %s", s);
	cvs_log(LP_ERR, "%s", str);
	free(str);
#endif
	return (0);
}


static time_t
ToSeconds(time_t Hours, time_t Minutes, time_t	Seconds, MERIDIAN Meridian)
{
	if (Minutes < 0 || Minutes > 59 || Seconds < 0 || Seconds > 59)
		return (-1);

	switch (Meridian) {
	case MER24:
		if (Hours < 0 || Hours > 23)
			return (-1);
		return (Hours * 60L + Minutes) * 60L + Seconds;
	case MERam:
		if (Hours < 1 || Hours > 12)
			return (-1);
		if (Hours == 12)
			Hours = 0;
		return (Hours * 60L + Minutes) * 60L + Seconds;
	case MERpm:
		if (Hours < 1 || Hours > 12)
			return (-1);
		if (Hours == 12)
			Hours = 0;
		return ((Hours + 12) * 60L + Minutes) * 60L + Seconds;
	default:
		return (-1);
	}
	/* NOTREACHED */
}


/* Year is either
 * A negative number, which means to use its absolute value (why?)
 * A number from 0 to 99, which means a year from 1900 to 1999, or
 * The actual year (>=100).
 */
static time_t
Convert(time_t Month, time_t Day, time_t Year, time_t Hours, time_t Minutes,
    time_t Seconds, MERIDIAN Meridian, DSTMODE DSTmode)
{
	static int DaysInMonth[12] = {
		31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
	};
	time_t	tod;
	time_t	julian;
	int	i;
	struct tm local = { 0 };

	/* Bound the supported calendar before negation and year iteration. */
	if (Year < -9999 || Year > 9999)
		return (-1);
	if (Year < 0)
		Year = -Year;
	if (Year < 69)
		Year += 2000;
	else if (Year < 100) {
		Year += 1900;
		if (Year < YEAR_EPOCH)
			Year += 100;
	}
	DaysInMonth[1] = Year % 4 == 0 && (Year % 100 != 0 || Year % 400 == 0)
	    ? 29 : 28;
	/* XXX Sloppily check for 2038 if time_t is 32 bits */
	if (Year < YEAR_EPOCH ||
	    (sizeof(time_t) == sizeof(int) && Year > 2038) ||
	    Month < 1 || Month > 12 ||
	    /* Lint fluff:  "conversion from long may lose accuracy" */
	     Day < 1 || Day > DaysInMonth[(int)--Month])
		return (-1);

	for (julian = Day - 1, i = 0; i < Month; i++)
		julian += DaysInMonth[i];

	for (i = YEAR_EPOCH; i < Year; i++)
		julian += 365 + (i % 4 == 0 && (i % 100 != 0 || i % 400 == 0));
	julian = date_multiply(julian, SECSPERDAY);
	julian = date_add(julian, date_multiply(yyTimezone, 60));

	if ((tod = ToSeconds(Hours, Minutes, Seconds, Meridian)) < 0)
		return (-1);
	julian = date_add(julian, tod);
	if (DSTmode == DSTmaybe && localtime_r(&julian, &local) == NULL)
		return (-1);
	if (DSTmode == DSTon || (DSTmode == DSTmaybe && local.tm_isdst))
		julian = date_subtract(julian, 60 * 60);
	return (yyDateError ? (time_t)-1 : julian);
}


static time_t
DSTcorrect(time_t Start, time_t Future)
{
	time_t	StartDay;
	time_t	FutureDay;
	struct tm start_tm, future_tm;

	if (localtime_r(&Start, &start_tm) == NULL ||
	    localtime_r(&Future, &future_tm) == NULL)
		return date_failure();
	StartDay = (start_tm.tm_hour + 1) % 24;
	FutureDay = (future_tm.tm_hour + 1) % 24;
	return date_add(date_subtract(Future, Start),
	    date_multiply(StartDay - FutureDay, 60 * 60));
}


static time_t
RelativeDate(time_t Start, time_t DayOrdinal, time_t DayNumber)
{
	struct tm value, *tm = &value;
	time_t	now;

	now = Start;
	if (localtime_r(&now, tm) == NULL)
		return date_failure();
	now = date_add(now,
	    date_multiply(SECSPERDAY, (DayNumber - tm->tm_wday + 7) % 7));
	now = date_add(now, date_multiply(7 * SECSPERDAY,
	    DayOrdinal <= 0 ? DayOrdinal : date_subtract(DayOrdinal, 1)));
	return DSTcorrect(Start, now);
}


static time_t
RelativeMonth(time_t Start, time_t RelMonth)
{
	struct tm value, *tm = &value;
	time_t	Month;
	time_t	Year, future;

	if (RelMonth == 0)
		return (0);
	if (localtime_r(&Start, tm) == NULL)
		return date_failure();
	Month = date_add(date_add(date_multiply(12,
	    date_add((time_t)tm->tm_year, 1900)), (time_t)tm->tm_mon), RelMonth);
	Year = Month / 12;
	Month = Month % 12 + 1;
	future = Convert(Month, (time_t)tm->tm_mday, Year,
	    (time_t)tm->tm_hour, (time_t)tm->tm_min, (time_t)tm->tm_sec,
	    MER24, DSTmaybe);
	if (future < 0 || yyDateError)
		return date_failure();
	return DSTcorrect(Start, future);
}


static int
lookup(char *buff)
{
	size_t		len;
	char		*p, *q;
	int		i, abbrev;
	const TABLE	*tp;

	/* Make it lowercase. */
	for (p = buff; *p; p++)
		if (isupper((unsigned char)*p))
			*p = (char)tolower((unsigned char)*p);

	if (strcmp(buff, "am") == 0 || strcmp(buff, "a.m.") == 0) {
		yylval.Meridian = MERam;
		return (tMERIDIAN);
	}
	if (strcmp(buff, "pm") == 0 || strcmp(buff, "p.m.") == 0) {
		yylval.Meridian = MERpm;
		return (tMERIDIAN);
	}

	len = strlen(buff);
	/* See if we have an abbreviation for a month. */
	if (len == 3)
		abbrev = 1;
	else if (len == 4 && buff[3] == '.') {
		abbrev = 1;
		buff[3] = '\0';
		--len;
	} else
		abbrev = 0;

	for (tp = MonthDayTable; tp->name; tp++) {
		if (abbrev) {
			if (strncmp(buff, tp->name, 3) == 0) {
				yylval.Number = tp->value;
				return (tp->type);
			}
		} else if (strcmp(buff, tp->name) == 0) {
			yylval.Number = tp->value;
			return (tp->type);
		}
	}

	for (tp = TimezoneTable; tp->name; tp++)
		if (strcmp(buff, tp->name) == 0) {
			yylval.Number = tp->value;
			return (tp->type);
		}

	if (strcmp(buff, "dst") == 0)
		return (tDST);

	for (tp = UnitsTable; tp->name; tp++)
		if (strcmp(buff, tp->name) == 0) {
			yylval.Number = tp->value;
			return (tp->type);
		}

	/* Strip off any plural and try the units table again. */
	if (len != 0 && buff[len - 1] == 's') {
		buff[len - 1] = '\0';
		for (tp = UnitsTable; tp->name; tp++)
			if (strcmp(buff, tp->name) == 0) {
				yylval.Number = tp->value;
				return (tp->type);
			}
		buff[len - 1] = 's';	/* Put back for "this" in OtherTable. */
	}

	for (tp = OtherTable; tp->name; tp++)
		if (strcmp(buff, tp->name) == 0) {
			yylval.Number = tp->value;
			return (tp->type);
		}

	/* Military timezones. */
	if (len == 1 && isalpha((unsigned char)*buff)) {
		for (tp = MilitaryTable; tp->name; tp++)
			if (strcmp(buff, tp->name) == 0) {
				yylval.Number = tp->value;
				return (tp->type);
			}
	}

	/* Drop out any periods and try the timezone table again. */
	for (i = 0, p = q = buff; *q; q++)
		if (*q != '.')
			*p++ = *q;
		else
			i++;
	*p = '\0';
	if (i)
		for (tp = TimezoneTable; tp->name; tp++)
			if (strcmp(buff, tp->name) == 0) {
				yylval.Number = tp->value;
				return (tp->type);
			}

	return (tID);
}


static int
yylex(void)
{
	char	c, *p, buff[20];
	int	count, sign;

	for (;;) {
		while (isspace((unsigned char)*yyInput))
			yyInput++;

		if (isdigit((unsigned char)(c = *yyInput)) || c == '-' || c == '+') {
			if (c == '-' || c == '+') {
				sign = c == '-' ? -1 : 1;
				if (!isdigit((unsigned char)*++yyInput))
					/* skip the '-' sign */
					continue;
			}
			else
				sign = 0;

			for (yylval.Number = 0; isdigit((unsigned char)(c = *yyInput++)); )
				yylval.Number = date_add(date_multiply(yylval.Number, 10),
				    (time_t)(c - '0'));
			yyInput--;
			if (sign < 0)
				yylval.Number = date_negate(yylval.Number);
			return sign ? tSNUMBER : tUNUMBER;
		}

		if (isalpha((unsigned char)c)) {
			for (p = buff; isalpha((unsigned char)(c = *yyInput++)) || c == '.'; ) {
				if (p < &buff[sizeof buff - 1])
					*p++ = c;
				else
					yyDateError = 1;
			}
			*p = '\0';
			yyInput--;
			return lookup(buff);
		}
		if (c == '\0')
			return 0;
		if (c != '(')
			return (unsigned char)*yyInput++;

		count = 0;
		do {
			c = *yyInput++;
			if (c == '\0') {
				yyInput--;
				yyDateError = 1;
				return 0;
			}
			if (c == '(') {
				if (count == INT_MAX) {
					yyDateError = 1;
					return 0;
				}
				count++;
			}
			else if (c == ')')
				count--;
		} while (count > 0);
	}
}

/* Yield A - B, measured in seconds.  */
static time_t
difftm(struct tm *a, struct tm *b)
{
	time_t ay, by, days, seconds;

	ay = date_add((time_t)a->tm_year, YEAR_TMORIGIN - 1);
	by = date_add((time_t)b->tm_year, YEAR_TMORIGIN - 1);
	days = date_subtract((time_t)a->tm_yday, (time_t)b->tm_yday);
	days = date_add(days, date_subtract(ay / 4, by / 4));
	days = date_subtract(days, date_subtract(ay / 100, by / 100));
	days = date_add(days, date_subtract(ay / 400, by / 400));
	days = date_add(days, date_multiply(date_subtract(ay, by), 365));
	seconds = date_add(date_multiply(days, 24),
	    (time_t)(a->tm_hour - b->tm_hour));
	seconds = date_add(date_multiply(seconds, 60),
	    (time_t)(a->tm_min - b->tm_min));
	return date_add(date_multiply(seconds, 60),
	    (time_t)(a->tm_sec - b->tm_sec));
}

/*
 * date_parse()
 *
 * Returns the number of seconds since the Epoch corresponding to the date.
 */
time_t
date_parse(const char *p)
{
	struct tm	gmt, tm;
	time_t		Start, tod, nowtime, tz;

	yyDateError = 0;
	if (p == NULL)
		return -1;
	yyInput = p;

	if (time(&nowtime) == -1 || !gmtime_r(&nowtime, &gmt) ||
	    !localtime_r(&nowtime, &tm))
		return -1;

	tz = difftm(&gmt, &tm) / 60;

	if (tm.tm_isdst)
		tz = date_add(tz, 60);

	yyYear = date_add((time_t)tm.tm_year, 1900);
	yyMonth = tm.tm_mon + 1;
	yyDay = tm.tm_mday;
	yyTimezone = tz;
	yyDSTmode = DSTmaybe;
	yyHour = 0;
	yyMinutes = 0;
	yySeconds = 0;
	yyMeridian = MER24;
	yyRelSeconds = 0;
	yyRelMonth = 0;
	yyHaveDate = 0;
	yyHaveDay = 0;
	yyHaveRel = 0;
	yyHaveTime = 0;
	yyHaveZone = 0;

	if (yyparse() || yyDateError || yyHaveTime > 1 || yyHaveZone > 1 ||
	    yyHaveDate > 1 || yyHaveDay > 1)
		return (-1);

	if (yyHaveDate || yyHaveTime || yyHaveDay) {
		Start = Convert(yyMonth, yyDay, yyYear, yyHour, yyMinutes,
		    yySeconds, yyMeridian, yyDSTmode);
		if (Start < 0)
			return (-1);
	} else {
		Start = nowtime;
		if (!yyHaveRel)
			Start = date_subtract(Start,
			    ((tm.tm_hour * 60L + tm.tm_min) * 60L) + tm.tm_sec);
	}

	Start = date_add(Start, yyRelSeconds);
	Start = date_add(Start, RelativeMonth(Start, yyRelMonth));

	if (yyHaveDay && !yyHaveDate) {
		tod = RelativeDate(Start, yyDayOrdinal, yyDayNumber);
		Start = date_add(Start, tod);
	}

	return yyDateError ? (time_t)-1 : Start;
}

#if defined(TEST)
int
main(int argc, char **argv)
{
	char	buff[128];
	time_t	d;

	(void)printf("Enter date, or blank line to exit.\n\t> ");
	(void)fflush(stdout);
	while (fgets(buff, sizeof(buff), stdin) && buff[0]) {
		d = date_parse(buff);
		if (d == -1)
			(void)printf("Bad format - couldn't convert.\n");
		else
			(void)printf("%s", ctime(&d));
		(void)printf("\t> ");
		(void)fflush(stdout);
	}

	return (0);
}
#endif	/* defined(TEST) */
