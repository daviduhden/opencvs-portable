/* original parser id follows */
/* yysccsid[] = "@(#)yaccpar	1.9 (Berkeley) 02/21/93" */
/* (use YYMAJOR/YYMINOR for ifdefs dependent on parser version) */

#define YYBYACC 1
#define YYMAJOR 2
#define YYMINOR 0
#define YYPATCH 20221106

#define YYEMPTY        (-1)
#define yyclearin      (yychar = YYEMPTY)
#define yyerrok        (yyerrflag = 0)
#define YYRECOVERING() (yyerrflag != 0)
#define YYENOMEM       (-2)
#define YYEOF          0
#undef YYBTYACC
#define YYBTYACC 0
#define YYDEBUGSTR YYPREFIX "debug"
#define YYPREFIX "yy"

#define YYPURE 0

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


static int	yyerror(const char *);
static int	yylex(void);
static int	yyparse(void);
static int	lookup(char *);

#ifdef YYSTYPE
#undef  YYSTYPE_IS_DECLARED
#define YYSTYPE_IS_DECLARED 1
#endif
#ifndef YYSTYPE_IS_DECLARED
#define YYSTYPE_IS_DECLARED 1
typedef union YYSTYPE {
	time_t		Number;
	enum _MERIDIAN	Meridian;
} YYSTYPE;
#endif /* !YYSTYPE_IS_DECLARED */

/* compatibility with bison */
#ifdef YYPARSE_PARAM
/* compatibility with FreeBSD */
# ifdef YYPARSE_PARAM_TYPE
#  define YYPARSE_DECL() yyparse(YYPARSE_PARAM_TYPE YYPARSE_PARAM)
# else
#  define YYPARSE_DECL() yyparse(void *YYPARSE_PARAM)
# endif
#else
# define YYPARSE_DECL() yyparse(void)
#endif

/* Parameters sent to lex. */
#ifdef YYLEX_PARAM
# define YYLEX_DECL() yylex(void *YYLEX_PARAM)
# define YYLEX yylex(YYLEX_PARAM)
#else
# define YYLEX_DECL() yylex(void)
# define YYLEX yylex()
#endif

#if !(defined(yylex) || defined(YYSTATE))
int YYLEX_DECL();
#endif

/* Parameters sent to yyerror. */
#ifndef YYERROR_DECL
#define YYERROR_DECL() yyerror(const char *s)
#endif
#ifndef YYERROR_CALL
#define YYERROR_CALL(msg) yyerror(msg)
#endif

extern int YYPARSE_DECL();

#define tAGO 257
#define tDAY 258
#define tDAYZONE 259
#define tID 260
#define tMERIDIAN 261
#define tMINUTE_UNIT 262
#define tMONTH 263
#define tMONTH_UNIT 264
#define tSEC_UNIT 265
#define tSNUMBER 266
#define tUNUMBER 267
#define tZONE 268
#define tDST 269
#define YYERRCODE 256
typedef int YYINT;
static const YYINT yylhs[] = {                           -1,
    0,    0,    2,    2,    2,    2,    2,    2,    3,    3,
    3,    3,    3,    4,    4,    4,    6,    6,    6,    5,
    5,    5,    5,    5,    5,    5,    5,    7,    7,    9,
    9,    9,    9,    9,    9,    9,    9,    9,    8,    1,
    1,
};
static const YYINT yylen[] = {                            2,
    0,    2,    1,    1,    1,    1,    1,    1,    2,    4,
    4,    6,    6,    1,    1,    2,    1,    2,    2,    3,
    5,    3,    3,    2,    4,    2,    3,    2,    1,    2,
    2,    1,    2,    2,    1,    2,    2,    1,    1,    0,
    1,
};
static const YYINT yydefred[] = {                         1,
    0,    0,   15,   32,    0,   38,   35,    0,    0,    0,
    2,    3,    4,    5,    6,    7,    8,    0,   18,    0,
   31,   36,   33,   19,    9,   30,    0,   37,   34,    0,
    0,    0,   16,   28,    0,   23,   27,   22,    0,    0,
   25,   41,   11,    0,   10,    0,    0,   21,   13,   12,
};
#if defined(YYDESTRUCT_CALL) || defined(YYSTYPE_TOSTRING)
static const YYINT yystos[] = {                           0,
  271,  258,  259,  262,  263,  264,  265,  266,  267,  268,
  273,  274,  275,  276,  277,  278,  279,  280,   44,  267,
  262,  264,  265,  258,  261,  262,  263,  264,  265,  266,
   58,   47,  269,  257,   44,  266,  267,  266,  267,  267,
  267,  261,  266,   58,  272,   47,  267,  267,  266,  272,
};
#endif /* YYDESTRUCT_CALL || YYSTYPE_TOSTRING */
static const YYINT yydgoto[] = {                          1,
   45,   11,   12,   13,   14,   15,   16,   17,   18,
};
static const YYINT yysindex[] = {                         0,
 -249,  -38,    0,    0, -260,    0,    0, -240,  -47, -248,
    0,    0,    0,    0,    0,    0,    0, -237,    0,  -18,
    0,    0,    0,    0,    0,    0, -262,    0,    0, -239,
 -238, -236,    0,    0, -235,    0,    0,    0,  -56,  -19,
    0,    0,    0, -234,    0, -232, -258,    0,    0,    0,
};
static const YYINT yyrindex[] = {                         0,
    0,    1,    0,    0,    0,    0,    0,    0,   69,   12,
    0,    0,    0,    0,    0,    0,    0,   23,    0,   34,
    0,    0,    0,    0,    0,    0,   67,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,   56,   45,
    0,    0,    0,    0,    0,    0,   56,    0,    0,    0,
};
#if YYBTYACC
static const YYINT yycindex[] = {                         0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
};
#endif
static const YYINT yygindex[] = {                         0,
  -17,    0,    0,    0,    0,    0,    0,    0,    0,
};
#define YYTABLESIZE 337
static const YYINT yytable[] = {                         32,
   17,   44,   42,   36,   37,   19,   20,   49,    2,    3,
   31,   14,    4,    5,    6,    7,    8,    9,   10,   34,
   33,   21,   29,   22,   23,   35,   38,   46,   39,   50,
   40,   41,   47,   24,   48,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   20,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   40,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,   26,    0,   39,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   42,    0,    0,    0,    0,   43,
   24,    0,    0,   25,   26,   27,   28,   29,   30,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,   17,   17,
    0,    0,   17,   17,   17,   17,   17,   17,   17,   14,
   14,    0,    0,   14,   14,   14,   14,   14,   14,   14,
   29,   29,    0,    0,   29,   29,   29,   29,   29,   29,
   29,   24,   24,    0,    0,   24,   24,   24,   24,   24,
   24,   24,   20,   20,    0,    0,   20,   20,   20,   20,
   20,   20,   20,   40,   40,    0,    0,   40,   40,   40,
   40,    0,   40,   40,   26,   26,    0,   39,   26,   26,
   26,   26,    0,    0,   26,   39,   39,
};
static const YYINT yycheck[] = {                         47,
    0,   58,  261,  266,  267,   44,  267,  266,  258,  259,
   58,    0,  262,  263,  264,  265,  266,  267,  268,  257,
  269,  262,    0,  264,  265,   44,  266,   47,  267,   47,
  267,  267,  267,    0,  267,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,    0,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,    0,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,    0,   -1,    0,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,  261,   -1,   -1,   -1,   -1,  266,
  258,   -1,   -1,  261,  262,  263,  264,  265,  266,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  258,  259,
   -1,   -1,  262,  263,  264,  265,  266,  267,  268,  258,
  259,   -1,   -1,  262,  263,  264,  265,  266,  267,  268,
  258,  259,   -1,   -1,  262,  263,  264,  265,  266,  267,
  268,  258,  259,   -1,   -1,  262,  263,  264,  265,  266,
  267,  268,  258,  259,   -1,   -1,  262,  263,  264,  265,
  266,  267,  268,  258,  259,   -1,   -1,  262,  263,  264,
  265,   -1,  267,  268,  258,  259,   -1,  259,  262,  263,
  264,  265,   -1,   -1,  268,  267,  268,
};
#if YYBTYACC
static const YYINT yyctable[] = {                        -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,
};
#endif
#define YYFINAL 1
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 269
#define YYUNDFTOKEN 281
#define YYTRANSLATE(a) ((a) > YYMAXTOKEN ? YYUNDFTOKEN : (a))
#if YYDEBUG
static const char *const yyname[] = {

"$end",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,"','",0,0,"'/'",0,0,0,0,0,0,0,0,0,0,"':'",0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"error","tAGO","tDAY",
"tDAYZONE","tID","tMERIDIAN","tMINUTE_UNIT","tMONTH","tMONTH_UNIT","tSEC_UNIT",
"tSNUMBER","tUNUMBER","tZONE","tDST","$accept","spec","o_merid","item","time",
"zone","date","day","rel","number","relunit","illegal-symbol",
};
static const char *const yyrule[] = {
"$accept : spec",
"spec :",
"spec : spec item",
"item : time",
"item : zone",
"item : date",
"item : day",
"item : rel",
"item : number",
"time : tUNUMBER tMERIDIAN",
"time : tUNUMBER ':' tUNUMBER o_merid",
"time : tUNUMBER ':' tUNUMBER tSNUMBER",
"time : tUNUMBER ':' tUNUMBER ':' tUNUMBER o_merid",
"time : tUNUMBER ':' tUNUMBER ':' tUNUMBER tSNUMBER",
"zone : tZONE",
"zone : tDAYZONE",
"zone : tZONE tDST",
"day : tDAY",
"day : tDAY ','",
"day : tUNUMBER tDAY",
"date : tUNUMBER '/' tUNUMBER",
"date : tUNUMBER '/' tUNUMBER '/' tUNUMBER",
"date : tUNUMBER tSNUMBER tSNUMBER",
"date : tUNUMBER tMONTH tSNUMBER",
"date : tMONTH tUNUMBER",
"date : tMONTH tUNUMBER ',' tUNUMBER",
"date : tUNUMBER tMONTH",
"date : tUNUMBER tMONTH tUNUMBER",
"rel : relunit tAGO",
"rel : relunit",
"relunit : tUNUMBER tMINUTE_UNIT",
"relunit : tSNUMBER tMINUTE_UNIT",
"relunit : tMINUTE_UNIT",
"relunit : tSNUMBER tSEC_UNIT",
"relunit : tUNUMBER tSEC_UNIT",
"relunit : tSEC_UNIT",
"relunit : tSNUMBER tMONTH_UNIT",
"relunit : tUNUMBER tMONTH_UNIT",
"relunit : tMONTH_UNIT",
"number : tUNUMBER",
"o_merid :",
"o_merid : tMERIDIAN",

};
#endif

#if YYDEBUG
int      yydebug;
#endif

int      yyerrflag;
int      yychar;
YYSTYPE  yyval;
YYSTYPE  yylval;
int      yynerrs;

#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
YYLTYPE  yyloc; /* position returned by actions */
YYLTYPE  yylloc; /* position from the lexer */
#endif

#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
#ifndef YYLLOC_DEFAULT
#define YYLLOC_DEFAULT(loc, rhs, n) \
do \
{ \
    if (n == 0) \
    { \
        (loc).first_line   = YYRHSLOC(rhs, 0).last_line; \
        (loc).first_column = YYRHSLOC(rhs, 0).last_column; \
        (loc).last_line    = YYRHSLOC(rhs, 0).last_line; \
        (loc).last_column  = YYRHSLOC(rhs, 0).last_column; \
    } \
    else \
    { \
        (loc).first_line   = YYRHSLOC(rhs, 1).first_line; \
        (loc).first_column = YYRHSLOC(rhs, 1).first_column; \
        (loc).last_line    = YYRHSLOC(rhs, n).last_line; \
        (loc).last_column  = YYRHSLOC(rhs, n).last_column; \
    } \
} while (0)
#endif /* YYLLOC_DEFAULT */
#endif /* defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED) */
#if YYBTYACC

#ifndef YYLVQUEUEGROWTH
#define YYLVQUEUEGROWTH 32
#endif
#endif /* YYBTYACC */

/* define the initial stack-sizes */
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH  YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 10000
#define YYMAXDEPTH  10000
#endif
#endif

#ifndef YYINITSTACKSIZE
#define YYINITSTACKSIZE 200
#endif

typedef struct {
    unsigned stacksize;
    YYINT    *s_base;
    YYINT    *s_mark;
    YYINT    *s_last;
    YYSTYPE  *l_base;
    YYSTYPE  *l_mark;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
    YYLTYPE  *p_base;
    YYLTYPE  *p_mark;
#endif
} YYSTACKDATA;
#if YYBTYACC

struct YYParseState_s
{
    struct YYParseState_s *save;    /* Previously saved parser state */
    YYSTACKDATA            yystack; /* saved parser stack */
    int                    state;   /* saved parser state */
    int                    errflag; /* saved error recovery status */
    int                    lexeme;  /* saved index of the conflict lexeme in the lexical queue */
    YYINT                  ctry;    /* saved index in yyctable[] for this conflict */
};
typedef struct YYParseState_s YYParseState;
#endif /* YYBTYACC */
/* variables for the parser stack */
static YYSTACKDATA yystack;
#if YYBTYACC

/* Current parser state */
static YYParseState *yyps = 0;

/* yypath != NULL: do the full parse, starting at *yypath parser state. */
static YYParseState *yypath = 0;

/* Base of the lexical value queue */
static YYSTYPE *yylvals = 0;

/* Current position at lexical value queue */
static YYSTYPE *yylvp = 0;

/* End position of lexical value queue */
static YYSTYPE *yylve = 0;

/* The last allocated position at the lexical value queue */
static YYSTYPE *yylvlim = 0;

#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
/* Base of the lexical position queue */
static YYLTYPE *yylpsns = 0;

/* Current position at lexical position queue */
static YYLTYPE *yylpp = 0;

/* End position of lexical position queue */
static YYLTYPE *yylpe = 0;

/* The last allocated position at the lexical position queue */
static YYLTYPE *yylplim = 0;
#endif

/* Current position at lexical token queue */
static YYINT  *yylexp = 0;

static YYINT  *yylexemes = 0;
#endif /* YYBTYACC */

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
	{ NULL }
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
	{ NULL }
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
	{ NULL }
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
	{  NULL  }
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
	{ NULL }
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
		julian += 365 + (i % 4 == 0);
	julian *= SECSPERDAY;
	julian += yyTimezone * 60L;

	if ((tod = ToSeconds(Hours, Minutes, Seconds, Meridian)) < 0)
		return (-1);
	julian += tod;
	if ((DSTmode == DSTon) ||
	    (DSTmode == DSTmaybe && localtime(&julian)->tm_isdst))
	julian -= 60 * 60;
	return (julian);
}


static time_t
DSTcorrect(time_t Start, time_t Future)
{
	time_t	StartDay;
	time_t	FutureDay;

	StartDay = (localtime(&Start)->tm_hour + 1) % 24;
	FutureDay = (localtime(&Future)->tm_hour + 1) % 24;
	return (Future - Start) + (StartDay - FutureDay) * 60L * 60L;
}


static time_t
RelativeDate(time_t Start, time_t DayOrdinal, time_t DayNumber)
{
	struct tm	*tm;
	time_t	now;

	now = Start;
	tm = localtime(&now);
	now += SECSPERDAY * ((DayNumber - tm->tm_wday + 7) % 7);
	now += 7 * SECSPERDAY * (DayOrdinal <= 0 ? DayOrdinal : DayOrdinal - 1);
	return DSTcorrect(Start, now);
}


static time_t
RelativeMonth(time_t Start, time_t RelMonth)
{
	struct tm	*tm;
	time_t	Month;
	time_t	Year;

	if (RelMonth == 0)
		return (0);
	tm = localtime(&Start);
	Month = 12 * (tm->tm_year + 1900) + tm->tm_mon + RelMonth;
	Year = Month / 12;
	Month = Month % 12 + 1;
	return DSTcorrect(Start,
	    Convert(Month, (time_t)tm->tm_mday, Year,
	    (time_t)tm->tm_hour, (time_t)tm->tm_min, (time_t)tm->tm_sec,
	    MER24, DSTmaybe));
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
		if (isupper(*p))
			*p = tolower(*p);

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
	if (len == 1 && isalpha(*buff)) {
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
		while (isspace(*yyInput))
			yyInput++;

		if (isdigit(c = *yyInput) || c == '-' || c == '+') {
			if (c == '-' || c == '+') {
				sign = c == '-' ? -1 : 1;
				if (!isdigit(*++yyInput))
					/* skip the '-' sign */
					continue;
			}
			else
				sign = 0;

			for (yylval.Number = 0; isdigit(c = *yyInput++); )
				yylval.Number = 10 * yylval.Number + c - '0';
			yyInput--;
			if (sign < 0)
				yylval.Number = -yylval.Number;
			return sign ? tSNUMBER : tUNUMBER;
		}

		if (isalpha(c)) {
			for (p = buff; isalpha(c = *yyInput++) || c == '.'; )
				if (p < &buff[sizeof buff - 1])
					*p++ = c;
			*p = '\0';
			yyInput--;
			return lookup(buff);
		}
		if (c != '(')
			return *yyInput++;

		count = 0;
		do {
			c = *yyInput++;
			if (c == '\0')
				return (c);
			if (c == '(')
				count++;
			else if (c == ')')
				count--;
		} while (count > 0);
	}
}

/* Yield A - B, measured in seconds.  */
static long
difftm(struct tm *a, struct tm *b)
{
	int ay = a->tm_year + (YEAR_TMORIGIN - 1);
	int by = b->tm_year + (YEAR_TMORIGIN - 1);
	int days = (
	    /* difference in day of year */
	    a->tm_yday - b->tm_yday
	    /* + intervening leap days */
	    +  ((ay >> 2) - (by >> 2))
	    -  (ay/100 - by/100)
	    +  ((ay/100 >> 2) - (by/100 >> 2))
	    /* + difference in years * 365 */
	    +  (long)(ay-by) * 365);
	return (60 * (60 * (24 * days + (a->tm_hour - b->tm_hour))
	    + (a->tm_min - b->tm_min)) + (a->tm_sec - b->tm_sec));
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

	yyInput = p;

	if (time(&nowtime) == -1 || !gmtime_r(&nowtime, &gmt) ||
	    !localtime_r(&nowtime, &tm))
		return -1;

	tz = difftm(&gmt, &tm) / 60;

	if (tm.tm_isdst)
		tz += 60;

	yyYear = tm.tm_year + 1900;
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

	if (yyparse() || yyHaveTime > 1 || yyHaveZone > 1 ||
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
			Start -= ((tm.tm_hour * 60L + tm.tm_min) * 60L) +
			    tm.tm_sec;
	}

	Start += yyRelSeconds;
	Start += RelativeMonth(Start, yyRelMonth);

	if (yyHaveDay && !yyHaveDate) {
		tod = RelativeDate(Start, yyDayOrdinal, yyDayNumber);
		Start += tod;
	}

	return Start;
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

/* For use in generated program */
#define yydepth (int)(yystack.s_mark - yystack.s_base)
#if YYBTYACC
#define yytrial (yyps->save)
#endif /* YYBTYACC */

#if YYDEBUG
#include <stdio.h>	/* needed for printf */
#endif

#include <stdlib.h>	/* needed for malloc, etc */
#include <string.h>	/* needed for memset */

/* allocate initial stack or double stack size, up to YYMAXDEPTH */
static int yygrowstack(YYSTACKDATA *data)
{
    int i;
    unsigned newsize;
    YYINT *newss;
    YYSTYPE *newvs;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
    YYLTYPE *newps;
#endif

    if ((newsize = data->stacksize) == 0)
        newsize = YYINITSTACKSIZE;
    else if (newsize >= YYMAXDEPTH)
        return YYENOMEM;
    else if ((newsize *= 2) > YYMAXDEPTH)
        newsize = YYMAXDEPTH;

    i = (int) (data->s_mark - data->s_base);
    newss = (YYINT *)realloc(data->s_base, newsize * sizeof(*newss));
    if (newss == 0)
        return YYENOMEM;

    data->s_base = newss;
    data->s_mark = newss + i;

    newvs = (YYSTYPE *)realloc(data->l_base, newsize * sizeof(*newvs));
    if (newvs == 0)
        return YYENOMEM;

    data->l_base = newvs;
    data->l_mark = newvs + i;

#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
    newps = (YYLTYPE *)realloc(data->p_base, newsize * sizeof(*newps));
    if (newps == 0)
        return YYENOMEM;

    data->p_base = newps;
    data->p_mark = newps + i;
#endif

    data->stacksize = newsize;
    data->s_last = data->s_base + newsize - 1;

#if YYDEBUG
    if (yydebug)
        fprintf(stderr, "%sdebug: stack size increased to %d\n", YYPREFIX, newsize);
#endif
    return 0;
}

#if YYPURE || defined(YY_NO_LEAKS)
static void yyfreestack(YYSTACKDATA *data)
{
    free(data->s_base);
    free(data->l_base);
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
    free(data->p_base);
#endif
    memset(data, 0, sizeof(*data));
}
#else
#define yyfreestack(data) /* nothing */
#endif /* YYPURE || defined(YY_NO_LEAKS) */
#if YYBTYACC

static YYParseState *
yyNewState(unsigned size)
{
    YYParseState *p = (YYParseState *) malloc(sizeof(YYParseState));
    if (p == NULL) return NULL;

    p->yystack.stacksize = size;
    if (size == 0)
    {
        p->yystack.s_base = NULL;
        p->yystack.l_base = NULL;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
        p->yystack.p_base = NULL;
#endif
        return p;
    }
    p->yystack.s_base    = (YYINT *) malloc(size * sizeof(YYINT));
    if (p->yystack.s_base == NULL) return NULL;
    p->yystack.l_base    = (YYSTYPE *) malloc(size * sizeof(YYSTYPE));
    if (p->yystack.l_base == NULL) return NULL;
    memset(p->yystack.l_base, 0, size * sizeof(YYSTYPE));
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
    p->yystack.p_base    = (YYLTYPE *) malloc(size * sizeof(YYLTYPE));
    if (p->yystack.p_base == NULL) return NULL;
    memset(p->yystack.p_base, 0, size * sizeof(YYLTYPE));
#endif

    return p;
}

static void
yyFreeState(YYParseState *p)
{
    yyfreestack(&p->yystack);
    free(p);
}
#endif /* YYBTYACC */

#define YYABORT  goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR  goto yyerrlab
#if YYBTYACC
#define YYVALID        do { if (yyps->save)            goto yyvalid; } while(0)
#define YYVALID_NESTED do { if (yyps->save && \
                                yyps->save->save == 0) goto yyvalid; } while(0)
#endif /* YYBTYACC */

int
YYPARSE_DECL()
{
    int yym, yyn, yystate, yyresult;
#if YYBTYACC
    int yynewerrflag;
    YYParseState *yyerrctx = NULL;
#endif /* YYBTYACC */
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
    YYLTYPE  yyerror_loc_range[3]; /* position of error start/end (0 unused) */
#endif
#if YYDEBUG
    const char *yys;

    if ((yys = getenv("YYDEBUG")) != 0)
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
    if (yydebug)
        fprintf(stderr, "%sdebug[<# of symbols on state stack>]\n", YYPREFIX);
#endif
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
    memset(yyerror_loc_range, 0, sizeof(yyerror_loc_range));
#endif

#if YYBTYACC
    yyps = yyNewState(0); if (yyps == 0) goto yyenomem;
    yyps->save = 0;
#endif /* YYBTYACC */
    yym = 0;
    /* yyn is set below */
    yynerrs = 0;
    yyerrflag = 0;
    yychar = YYEMPTY;
    yystate = 0;

#if YYPURE
    memset(&yystack, 0, sizeof(yystack));
#endif

    if (yystack.s_base == NULL && yygrowstack(&yystack) == YYENOMEM) goto yyoverflow;
    yystack.s_mark = yystack.s_base;
    yystack.l_mark = yystack.l_base;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
    yystack.p_mark = yystack.p_base;
#endif
    yystate = 0;
    *yystack.s_mark = 0;

yyloop:
    if ((yyn = yydefred[yystate]) != 0) goto yyreduce;
    if (yychar < 0)
    {
#if YYBTYACC
        do {
        if (yylvp < yylve)
        {
            /* we're currently re-reading tokens */
            yylval = *yylvp++;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
            yylloc = *yylpp++;
#endif
            yychar = *yylexp++;
            break;
        }
        if (yyps->save)
        {
            /* in trial mode; save scanner results for future parse attempts */
            if (yylvp == yylvlim)
            {   /* Enlarge lexical value queue */
                size_t p = (size_t) (yylvp - yylvals);
                size_t s = (size_t) (yylvlim - yylvals);

                s += YYLVQUEUEGROWTH;
                if ((yylexemes = (YYINT *)realloc(yylexemes, s * sizeof(YYINT))) == NULL) goto yyenomem;
                if ((yylvals   = (YYSTYPE *)realloc(yylvals, s * sizeof(YYSTYPE))) == NULL) goto yyenomem;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
                if ((yylpsns   = (YYLTYPE *)realloc(yylpsns, s * sizeof(YYLTYPE))) == NULL) goto yyenomem;
#endif
                yylvp   = yylve = yylvals + p;
                yylvlim = yylvals + s;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
                yylpp   = yylpe = yylpsns + p;
                yylplim = yylpsns + s;
#endif
                yylexp  = yylexemes + p;
            }
            *yylexp = (YYINT) YYLEX;
            *yylvp++ = yylval;
            yylve++;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
            *yylpp++ = yylloc;
            yylpe++;
#endif
            yychar = *yylexp++;
            break;
        }
        /* normal operation, no conflict encountered */
#endif /* YYBTYACC */
        yychar = YYLEX;
#if YYBTYACC
        } while (0);
#endif /* YYBTYACC */
        if (yychar < 0) yychar = YYEOF;
#if YYDEBUG
        if (yydebug)
        {
            if ((yys = yyname[YYTRANSLATE(yychar)]) == NULL) yys = yyname[YYUNDFTOKEN];
            fprintf(stderr, "%s[%d]: state %d, reading token %d (%s)",
                            YYDEBUGSTR, yydepth, yystate, yychar, yys);
#ifdef YYSTYPE_TOSTRING
#if YYBTYACC
            if (!yytrial)
#endif /* YYBTYACC */
                fprintf(stderr, " <%s>", YYSTYPE_TOSTRING(yychar, yylval));
#endif
            fputc('\n', stderr);
        }
#endif
    }
#if YYBTYACC

    /* Do we have a conflict? */
    if (((yyn = yycindex[yystate]) != 0) && (yyn += yychar) >= 0 &&
        yyn <= YYTABLESIZE && yycheck[yyn] == (YYINT) yychar)
    {
        YYINT ctry;

        if (yypath)
        {
            YYParseState *save;
#if YYDEBUG
            if (yydebug)
                fprintf(stderr, "%s[%d]: CONFLICT in state %d: following successful trial parse\n",
                                YYDEBUGSTR, yydepth, yystate);
#endif
            /* Switch to the next conflict context */
            save = yypath;
            yypath = save->save;
            save->save = NULL;
            ctry = save->ctry;
            if (save->state != yystate) YYABORT;
            yyFreeState(save);

        }
        else
        {

            /* Unresolved conflict - start/continue trial parse */
            YYParseState *save;
#if YYDEBUG
            if (yydebug)
            {
                fprintf(stderr, "%s[%d]: CONFLICT in state %d. ", YYDEBUGSTR, yydepth, yystate);
                if (yyps->save)
                    fputs("ALREADY in conflict, continuing trial parse.\n", stderr);
                else
                    fputs("Starting trial parse.\n", stderr);
            }
#endif
            save                  = yyNewState((unsigned)(yystack.s_mark - yystack.s_base + 1));
            if (save == NULL) goto yyenomem;
            save->save            = yyps->save;
            save->state           = yystate;
            save->errflag         = yyerrflag;
            save->yystack.s_mark  = save->yystack.s_base + (yystack.s_mark - yystack.s_base);
            memcpy (save->yystack.s_base, yystack.s_base, (size_t) (yystack.s_mark - yystack.s_base + 1) * sizeof(YYINT));
            save->yystack.l_mark  = save->yystack.l_base + (yystack.l_mark - yystack.l_base);
            memcpy (save->yystack.l_base, yystack.l_base, (size_t) (yystack.l_mark - yystack.l_base + 1) * sizeof(YYSTYPE));
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
            save->yystack.p_mark  = save->yystack.p_base + (yystack.p_mark - yystack.p_base);
            memcpy (save->yystack.p_base, yystack.p_base, (size_t) (yystack.p_mark - yystack.p_base + 1) * sizeof(YYLTYPE));
#endif
            ctry                  = yytable[yyn];
            if (yyctable[ctry] == -1)
            {
#if YYDEBUG
                if (yydebug && yychar >= YYEOF)
                    fprintf(stderr, "%s[%d]: backtracking 1 token\n", YYDEBUGSTR, yydepth);
#endif
                ctry++;
            }
            save->ctry = ctry;
            if (yyps->save == NULL)
            {
                /* If this is a first conflict in the stack, start saving lexemes */
                if (!yylexemes)
                {
                    yylexemes = (YYINT *) malloc((YYLVQUEUEGROWTH) * sizeof(YYINT));
                    if (yylexemes == NULL) goto yyenomem;
                    yylvals   = (YYSTYPE *) malloc((YYLVQUEUEGROWTH) * sizeof(YYSTYPE));
                    if (yylvals == NULL) goto yyenomem;
                    yylvlim   = yylvals + YYLVQUEUEGROWTH;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
                    yylpsns   = (YYLTYPE *) malloc((YYLVQUEUEGROWTH) * sizeof(YYLTYPE));
                    if (yylpsns == NULL) goto yyenomem;
                    yylplim   = yylpsns + YYLVQUEUEGROWTH;
#endif
                }
                if (yylvp == yylve)
                {
                    yylvp  = yylve = yylvals;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
                    yylpp  = yylpe = yylpsns;
#endif
                    yylexp = yylexemes;
                    if (yychar >= YYEOF)
                    {
                        *yylve++ = yylval;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
                        *yylpe++ = yylloc;
#endif
                        *yylexp  = (YYINT) yychar;
                        yychar   = YYEMPTY;
                    }
                }
            }
            if (yychar >= YYEOF)
            {
                yylvp--;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
                yylpp--;
#endif
                yylexp--;
                yychar = YYEMPTY;
            }
            save->lexeme = (int) (yylvp - yylvals);
            yyps->save   = save;
        }
        if (yytable[yyn] == ctry)
        {
#if YYDEBUG
            if (yydebug)
                fprintf(stderr, "%s[%d]: state %d, shifting to state %d\n",
                                YYDEBUGSTR, yydepth, yystate, yyctable[ctry]);
#endif
            if (yychar < 0)
            {
                yylvp++;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
                yylpp++;
#endif
                yylexp++;
            }
            if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack) == YYENOMEM)
                goto yyoverflow;
            yystate = yyctable[ctry];
            *++yystack.s_mark = (YYINT) yystate;
            *++yystack.l_mark = yylval;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
            *++yystack.p_mark = yylloc;
#endif
            yychar  = YYEMPTY;
            if (yyerrflag > 0) --yyerrflag;
            goto yyloop;
        }
        else
        {
            yyn = yyctable[ctry];
            goto yyreduce;
        }
    } /* End of code dealing with conflicts */
#endif /* YYBTYACC */
    if (((yyn = yysindex[yystate]) != 0) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == (YYINT) yychar)
    {
#if YYDEBUG
        if (yydebug)
            fprintf(stderr, "%s[%d]: state %d, shifting to state %d\n",
                            YYDEBUGSTR, yydepth, yystate, yytable[yyn]);
#endif
        if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack) == YYENOMEM) goto yyoverflow;
        yystate = yytable[yyn];
        *++yystack.s_mark = yytable[yyn];
        *++yystack.l_mark = yylval;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
        *++yystack.p_mark = yylloc;
#endif
        yychar = YYEMPTY;
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if (((yyn = yyrindex[yystate]) != 0) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == (YYINT) yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag != 0) goto yyinrecovery;
#if YYBTYACC

    yynewerrflag = 1;
    goto yyerrhandler;
    goto yyerrlab; /* redundant goto avoids 'unused label' warning */

yyerrlab:
    /* explicit YYERROR from an action -- pop the rhs of the rule reduced
     * before looking for error recovery */
    yystack.s_mark -= yym;
    yystate = *yystack.s_mark;
    yystack.l_mark -= yym;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
    yystack.p_mark -= yym;
#endif

    yynewerrflag = 0;
yyerrhandler:
    while (yyps->save)
    {
        int ctry;
        YYParseState *save = yyps->save;
#if YYDEBUG
        if (yydebug)
            fprintf(stderr, "%s[%d]: ERROR in state %d, CONFLICT BACKTRACKING to state %d, %d tokens\n",
                            YYDEBUGSTR, yydepth, yystate, yyps->save->state,
                    (int)(yylvp - yylvals - yyps->save->lexeme));
#endif
        /* Memorize most forward-looking error state in case it's really an error. */
        if (yyerrctx == NULL || yyerrctx->lexeme < yylvp - yylvals)
        {
            /* Free old saved error context state */
            if (yyerrctx) yyFreeState(yyerrctx);
            /* Create and fill out new saved error context state */
            yyerrctx                 = yyNewState((unsigned)(yystack.s_mark - yystack.s_base + 1));
            if (yyerrctx == NULL) goto yyenomem;
            yyerrctx->save           = yyps->save;
            yyerrctx->state          = yystate;
            yyerrctx->errflag        = yyerrflag;
            yyerrctx->yystack.s_mark = yyerrctx->yystack.s_base + (yystack.s_mark - yystack.s_base);
            memcpy (yyerrctx->yystack.s_base, yystack.s_base, (size_t) (yystack.s_mark - yystack.s_base + 1) * sizeof(YYINT));
            yyerrctx->yystack.l_mark = yyerrctx->yystack.l_base + (yystack.l_mark - yystack.l_base);
            memcpy (yyerrctx->yystack.l_base, yystack.l_base, (size_t) (yystack.l_mark - yystack.l_base + 1) * sizeof(YYSTYPE));
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
            yyerrctx->yystack.p_mark = yyerrctx->yystack.p_base + (yystack.p_mark - yystack.p_base);
            memcpy (yyerrctx->yystack.p_base, yystack.p_base, (size_t) (yystack.p_mark - yystack.p_base + 1) * sizeof(YYLTYPE));
#endif
            yyerrctx->lexeme         = (int) (yylvp - yylvals);
        }
        yylvp          = yylvals   + save->lexeme;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
        yylpp          = yylpsns   + save->lexeme;
#endif
        yylexp         = yylexemes + save->lexeme;
        yychar         = YYEMPTY;
        yystack.s_mark = yystack.s_base + (save->yystack.s_mark - save->yystack.s_base);
        memcpy (yystack.s_base, save->yystack.s_base, (size_t) (yystack.s_mark - yystack.s_base + 1) * sizeof(YYINT));
        yystack.l_mark = yystack.l_base + (save->yystack.l_mark - save->yystack.l_base);
        memcpy (yystack.l_base, save->yystack.l_base, (size_t) (yystack.l_mark - yystack.l_base + 1) * sizeof(YYSTYPE));
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
        yystack.p_mark = yystack.p_base + (save->yystack.p_mark - save->yystack.p_base);
        memcpy (yystack.p_base, save->yystack.p_base, (size_t) (yystack.p_mark - yystack.p_base + 1) * sizeof(YYLTYPE));
#endif
        ctry           = ++save->ctry;
        yystate        = save->state;
        /* We tried shift, try reduce now */
        if ((yyn = yyctable[ctry]) >= 0) goto yyreduce;
        yyps->save     = save->save;
        save->save     = NULL;
        yyFreeState(save);

        /* Nothing left on the stack -- error */
        if (!yyps->save)
        {
#if YYDEBUG
            if (yydebug)
                fprintf(stderr, "%sdebug[%d,trial]: trial parse FAILED, entering ERROR mode\n",
                                YYPREFIX, yydepth);
#endif
            /* Restore state as it was in the most forward-advanced error */
            yylvp          = yylvals   + yyerrctx->lexeme;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
            yylpp          = yylpsns   + yyerrctx->lexeme;
#endif
            yylexp         = yylexemes + yyerrctx->lexeme;
            yychar         = yylexp[-1];
            yylval         = yylvp[-1];
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
            yylloc         = yylpp[-1];
#endif
            yystack.s_mark = yystack.s_base + (yyerrctx->yystack.s_mark - yyerrctx->yystack.s_base);
            memcpy (yystack.s_base, yyerrctx->yystack.s_base, (size_t) (yystack.s_mark - yystack.s_base + 1) * sizeof(YYINT));
            yystack.l_mark = yystack.l_base + (yyerrctx->yystack.l_mark - yyerrctx->yystack.l_base);
            memcpy (yystack.l_base, yyerrctx->yystack.l_base, (size_t) (yystack.l_mark - yystack.l_base + 1) * sizeof(YYSTYPE));
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
            yystack.p_mark = yystack.p_base + (yyerrctx->yystack.p_mark - yyerrctx->yystack.p_base);
            memcpy (yystack.p_base, yyerrctx->yystack.p_base, (size_t) (yystack.p_mark - yystack.p_base + 1) * sizeof(YYLTYPE));
#endif
            yystate        = yyerrctx->state;
            yyFreeState(yyerrctx);
            yyerrctx       = NULL;
        }
        yynewerrflag = 1;
    }
    if (yynewerrflag == 0) goto yyinrecovery;
#endif /* YYBTYACC */

    YYERROR_CALL("syntax error");
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
    yyerror_loc_range[1] = yylloc; /* lookahead position is error start position */
#endif

#if !YYBTYACC
    goto yyerrlab; /* redundant goto avoids 'unused label' warning */
yyerrlab:
#endif
    ++yynerrs;

yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if (((yyn = yysindex[*yystack.s_mark]) != 0) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == (YYINT) YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    fprintf(stderr, "%s[%d]: state %d, error recovery shifting to state %d\n",
                                    YYDEBUGSTR, yydepth, *yystack.s_mark, yytable[yyn]);
#endif
                if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack) == YYENOMEM) goto yyoverflow;
                yystate = yytable[yyn];
                *++yystack.s_mark = yytable[yyn];
                *++yystack.l_mark = yylval;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
                /* lookahead position is error end position */
                yyerror_loc_range[2] = yylloc;
                YYLLOC_DEFAULT(yyloc, yyerror_loc_range, 2); /* position of error span */
                *++yystack.p_mark = yyloc;
#endif
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    fprintf(stderr, "%s[%d]: error recovery discarding state %d\n",
                                    YYDEBUGSTR, yydepth, *yystack.s_mark);
#endif
                if (yystack.s_mark <= yystack.s_base) goto yyabort;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
                /* the current TOS position is the error start position */
                yyerror_loc_range[1] = *yystack.p_mark;
#endif
#if defined(YYDESTRUCT_CALL)
#if YYBTYACC
                if (!yytrial)
#endif /* YYBTYACC */
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
                    YYDESTRUCT_CALL("error: discarding state",
                                    yystos[*yystack.s_mark], yystack.l_mark, yystack.p_mark);
#else
                    YYDESTRUCT_CALL("error: discarding state",
                                    yystos[*yystack.s_mark], yystack.l_mark);
#endif /* defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED) */
#endif /* defined(YYDESTRUCT_CALL) */
                --yystack.s_mark;
                --yystack.l_mark;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
                --yystack.p_mark;
#endif
            }
        }
    }
    else
    {
        if (yychar == YYEOF) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            if ((yys = yyname[YYTRANSLATE(yychar)]) == NULL) yys = yyname[YYUNDFTOKEN];
            fprintf(stderr, "%s[%d]: state %d, error recovery discarding token %d (%s)\n",
                            YYDEBUGSTR, yydepth, yystate, yychar, yys);
        }
#endif
#if defined(YYDESTRUCT_CALL)
#if YYBTYACC
        if (!yytrial)
#endif /* YYBTYACC */
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
            YYDESTRUCT_CALL("error: discarding token", yychar, &yylval, &yylloc);
#else
            YYDESTRUCT_CALL("error: discarding token", yychar, &yylval);
#endif /* defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED) */
#endif /* defined(YYDESTRUCT_CALL) */
        yychar = YYEMPTY;
        goto yyloop;
    }

yyreduce:
    yym = yylen[yyn];
#if YYDEBUG
    if (yydebug)
    {
        fprintf(stderr, "%s[%d]: state %d, reducing by rule %d (%s)",
                        YYDEBUGSTR, yydepth, yystate, yyn, yyrule[yyn]);
#ifdef YYSTYPE_TOSTRING
#if YYBTYACC
        if (!yytrial)
#endif /* YYBTYACC */
            if (yym > 0)
            {
                int i;
                fputc('<', stderr);
                for (i = yym; i > 0; i--)
                {
                    if (i != yym) fputs(", ", stderr);
                    fputs(YYSTYPE_TOSTRING(yystos[yystack.s_mark[1-i]],
                                           yystack.l_mark[1-i]), stderr);
                }
                fputc('>', stderr);
            }
#endif
        fputc('\n', stderr);
    }
#endif
    if (yym > 0)
        yyval = yystack.l_mark[1-yym];
    else
        memset(&yyval, 0, sizeof yyval);
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)

    /* Perform position reduction */
    memset(&yyloc, 0, sizeof(yyloc));
#if YYBTYACC
    if (!yytrial)
#endif /* YYBTYACC */
    {
        YYLLOC_DEFAULT(yyloc, &yystack.p_mark[-yym], yym);
        /* just in case YYERROR is invoked within the action, save
           the start of the rhs as the error start position */
        yyerror_loc_range[1] = yystack.p_mark[1-yym];
    }
#endif

    switch (yyn)
    {
case 3:
	{
		yyHaveTime++;
	}
break;
case 4:
	{
		yyHaveZone++;
	}
break;
case 5:
	{
		yyHaveDate++;
	}
break;
case 6:
	{
		yyHaveDay++;
	}
break;
case 7:
	{
		yyHaveRel++;
	}
break;
case 9:
	{
		yyHour = yystack.l_mark[-1].Number;
		yyMinutes = 0;
		yySeconds = 0;
		yyMeridian = yystack.l_mark[0].Meridian;
	}
break;
case 10:
	{
		yyHour = yystack.l_mark[-3].Number;
		yyMinutes = yystack.l_mark[-1].Number;
		yySeconds = 0;
		yyMeridian = yystack.l_mark[0].Meridian;
	}
break;
case 11:
	{
		yyHour = yystack.l_mark[-3].Number;
		yyMinutes = yystack.l_mark[-1].Number;
		yyMeridian = MER24;
		yyDSTmode = DSToff;
		yyTimezone = - (yystack.l_mark[0].Number % 100 + (yystack.l_mark[0].Number / 100) * 60);
	}
break;
case 12:
	{
		yyHour = yystack.l_mark[-5].Number;
		yyMinutes = yystack.l_mark[-3].Number;
		yySeconds = yystack.l_mark[-1].Number;
		yyMeridian = yystack.l_mark[0].Meridian;
	}
break;
case 13:
	{
		yyHour = yystack.l_mark[-5].Number;
		yyMinutes = yystack.l_mark[-3].Number;
		yySeconds = yystack.l_mark[-1].Number;
		yyMeridian = MER24;
		yyDSTmode = DSToff;
		yyTimezone = - (yystack.l_mark[0].Number % 100 + (yystack.l_mark[0].Number / 100) * 60);
	}
break;
case 14:
	{
		yyTimezone = yystack.l_mark[0].Number;
		yyDSTmode = DSToff;
	}
break;
case 15:
	{
		yyTimezone = yystack.l_mark[0].Number;
		yyDSTmode = DSTon;
	}
break;
case 16:
	{
		yyTimezone = yystack.l_mark[-1].Number;
		yyDSTmode = DSTon;
	}
break;
case 17:
	{
		yyDayOrdinal = 1;
		yyDayNumber = yystack.l_mark[0].Number;
	}
break;
case 18:
	{
		yyDayOrdinal = 1;
		yyDayNumber = yystack.l_mark[-1].Number;
	}
break;
case 19:
	{
		yyDayOrdinal = yystack.l_mark[-1].Number;
		yyDayNumber = yystack.l_mark[0].Number;
	}
break;
case 20:
	{
		yyMonth = yystack.l_mark[-2].Number;
		yyDay = yystack.l_mark[0].Number;
	}
break;
case 21:
	{
		if (yystack.l_mark[-4].Number >= 100) {
			yyYear = yystack.l_mark[-4].Number;
			yyMonth = yystack.l_mark[-2].Number;
			yyDay = yystack.l_mark[0].Number;
		} else {
			yyMonth = yystack.l_mark[-4].Number;
			yyDay = yystack.l_mark[-2].Number;
			yyYear = yystack.l_mark[0].Number;
		}
	}
break;
case 22:
	{
		/* ISO 8601 format.  yyyy-mm-dd.  */
		yyYear = yystack.l_mark[-2].Number;
		yyMonth = -yystack.l_mark[-1].Number;
		yyDay = -yystack.l_mark[0].Number;
	}
break;
case 23:
	{
		/* e.g. 17-JUN-1992.  */
		yyDay = yystack.l_mark[-2].Number;
		yyMonth = yystack.l_mark[-1].Number;
		yyYear = -yystack.l_mark[0].Number;
	}
break;
case 24:
	{
		yyMonth = yystack.l_mark[-1].Number;
		yyDay = yystack.l_mark[0].Number;
	}
break;
case 25:
	{
		yyMonth = yystack.l_mark[-3].Number;
		yyDay = yystack.l_mark[-2].Number;
		yyYear = yystack.l_mark[0].Number;
	}
break;
case 26:
	{
		yyMonth = yystack.l_mark[0].Number;
		yyDay = yystack.l_mark[-1].Number;
	}
break;
case 27:
	{
		yyMonth = yystack.l_mark[-1].Number;
		yyDay = yystack.l_mark[-2].Number;
		yyYear = yystack.l_mark[0].Number;
	}
break;
case 28:
	{
		yyRelSeconds = -yyRelSeconds;
		yyRelMonth = -yyRelMonth;
	}
break;
case 30:
	{
		yyRelSeconds += yystack.l_mark[-1].Number * yystack.l_mark[0].Number * 60L;
	}
break;
case 31:
	{
		yyRelSeconds += yystack.l_mark[-1].Number * yystack.l_mark[0].Number * 60L;
	}
break;
case 32:
	{
		yyRelSeconds += yystack.l_mark[0].Number * 60L;
	}
break;
case 33:
	{
		yyRelSeconds += yystack.l_mark[-1].Number;
	}
break;
case 34:
	{
		yyRelSeconds += yystack.l_mark[-1].Number;
	}
break;
case 35:
	{
		yyRelSeconds++;
	}
break;
case 36:
	{
		yyRelMonth += yystack.l_mark[-1].Number * yystack.l_mark[0].Number;
	}
break;
case 37:
	{
		yyRelMonth += yystack.l_mark[-1].Number * yystack.l_mark[0].Number;
	}
break;
case 38:
	{
		yyRelMonth += yystack.l_mark[0].Number;
	}
break;
case 39:
	{
		if (yyHaveTime && yyHaveDate && !yyHaveRel)
			yyYear = yystack.l_mark[0].Number;
		else {
			if (yystack.l_mark[0].Number > 10000) {
				yyHaveDate++;
				yyDay= (yystack.l_mark[0].Number)%100;
				yyMonth= (yystack.l_mark[0].Number/100)%100;
				yyYear = yystack.l_mark[0].Number/10000;
			} else {
				yyHaveTime++;
				if (yystack.l_mark[0].Number < 100) {
					yyHour = yystack.l_mark[0].Number;
					yyMinutes = 0;
				} else {
					yyHour = yystack.l_mark[0].Number / 100;
					yyMinutes = yystack.l_mark[0].Number % 100;
				}
				yySeconds = 0;
				yyMeridian = MER24;
			}
		}
	}
break;
case 40:
	{
		yyval.Meridian = MER24;
	}
break;
case 41:
	{
		yyval.Meridian = yystack.l_mark[0].Meridian;
	}
break;
    default:
        break;
    }
    yystack.s_mark -= yym;
    yystate = *yystack.s_mark;
    yystack.l_mark -= yym;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
    yystack.p_mark -= yym;
#endif
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
        {
            fprintf(stderr, "%s[%d]: after reduction, ", YYDEBUGSTR, yydepth);
#ifdef YYSTYPE_TOSTRING
#if YYBTYACC
            if (!yytrial)
#endif /* YYBTYACC */
                fprintf(stderr, "result is <%s>, ", YYSTYPE_TOSTRING(yystos[YYFINAL], yyval));
#endif
            fprintf(stderr, "shifting from state 0 to final state %d\n", YYFINAL);
        }
#endif
        yystate = YYFINAL;
        *++yystack.s_mark = YYFINAL;
        *++yystack.l_mark = yyval;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
        *++yystack.p_mark = yyloc;
#endif
        if (yychar < 0)
        {
#if YYBTYACC
            do {
            if (yylvp < yylve)
            {
                /* we're currently re-reading tokens */
                yylval = *yylvp++;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
                yylloc = *yylpp++;
#endif
                yychar = *yylexp++;
                break;
            }
            if (yyps->save)
            {
                /* in trial mode; save scanner results for future parse attempts */
                if (yylvp == yylvlim)
                {   /* Enlarge lexical value queue */
                    size_t p = (size_t) (yylvp - yylvals);
                    size_t s = (size_t) (yylvlim - yylvals);

                    s += YYLVQUEUEGROWTH;
                    if ((yylexemes = (YYINT *)realloc(yylexemes, s * sizeof(YYINT))) == NULL)
                        goto yyenomem;
                    if ((yylvals   = (YYSTYPE *)realloc(yylvals, s * sizeof(YYSTYPE))) == NULL)
                        goto yyenomem;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
                    if ((yylpsns   = (YYLTYPE *)realloc(yylpsns, s * sizeof(YYLTYPE))) == NULL)
                        goto yyenomem;
#endif
                    yylvp   = yylve = yylvals + p;
                    yylvlim = yylvals + s;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
                    yylpp   = yylpe = yylpsns + p;
                    yylplim = yylpsns + s;
#endif
                    yylexp  = yylexemes + p;
                }
                *yylexp = (YYINT) YYLEX;
                *yylvp++ = yylval;
                yylve++;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
                *yylpp++ = yylloc;
                yylpe++;
#endif
                yychar = *yylexp++;
                break;
            }
            /* normal operation, no conflict encountered */
#endif /* YYBTYACC */
            yychar = YYLEX;
#if YYBTYACC
            } while (0);
#endif /* YYBTYACC */
            if (yychar < 0) yychar = YYEOF;
#if YYDEBUG
            if (yydebug)
            {
                if ((yys = yyname[YYTRANSLATE(yychar)]) == NULL) yys = yyname[YYUNDFTOKEN];
                fprintf(stderr, "%s[%d]: state %d, reading token %d (%s)\n",
                                YYDEBUGSTR, yydepth, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == YYEOF) goto yyaccept;
        goto yyloop;
    }
    if (((yyn = yygindex[yym]) != 0) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == (YYINT) yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
    {
        fprintf(stderr, "%s[%d]: after reduction, ", YYDEBUGSTR, yydepth);
#ifdef YYSTYPE_TOSTRING
#if YYBTYACC
        if (!yytrial)
#endif /* YYBTYACC */
            fprintf(stderr, "result is <%s>, ", YYSTYPE_TOSTRING(yystos[yystate], yyval));
#endif
        fprintf(stderr, "shifting from state %d to state %d\n", *yystack.s_mark, yystate);
    }
#endif
    if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack) == YYENOMEM) goto yyoverflow;
    *++yystack.s_mark = (YYINT) yystate;
    *++yystack.l_mark = yyval;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
    *++yystack.p_mark = yyloc;
#endif
    goto yyloop;
#if YYBTYACC

    /* Reduction declares that this path is valid. Set yypath and do a full parse */
yyvalid:
    if (yypath) YYABORT;
    while (yyps->save)
    {
        YYParseState *save = yyps->save;
        yyps->save = save->save;
        save->save = yypath;
        yypath = save;
    }
#if YYDEBUG
    if (yydebug)
        fprintf(stderr, "%s[%d]: state %d, CONFLICT trial successful, backtracking to state %d, %d tokens\n",
                        YYDEBUGSTR, yydepth, yystate, yypath->state, (int)(yylvp - yylvals - yypath->lexeme));
#endif
    if (yyerrctx)
    {
        yyFreeState(yyerrctx);
        yyerrctx = NULL;
    }
    yylvp          = yylvals + yypath->lexeme;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
    yylpp          = yylpsns + yypath->lexeme;
#endif
    yylexp         = yylexemes + yypath->lexeme;
    yychar         = YYEMPTY;
    yystack.s_mark = yystack.s_base + (yypath->yystack.s_mark - yypath->yystack.s_base);
    memcpy (yystack.s_base, yypath->yystack.s_base, (size_t) (yystack.s_mark - yystack.s_base + 1) * sizeof(YYINT));
    yystack.l_mark = yystack.l_base + (yypath->yystack.l_mark - yypath->yystack.l_base);
    memcpy (yystack.l_base, yypath->yystack.l_base, (size_t) (yystack.l_mark - yystack.l_base + 1) * sizeof(YYSTYPE));
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
    yystack.p_mark = yystack.p_base + (yypath->yystack.p_mark - yypath->yystack.p_base);
    memcpy (yystack.p_base, yypath->yystack.p_base, (size_t) (yystack.p_mark - yystack.p_base + 1) * sizeof(YYLTYPE));
#endif
    yystate        = yypath->state;
    goto yyloop;
#endif /* YYBTYACC */

yyoverflow:
    YYERROR_CALL("yacc stack overflow");
#if YYBTYACC
    goto yyabort_nomem;
yyenomem:
    YYERROR_CALL("memory exhausted");
yyabort_nomem:
#endif /* YYBTYACC */
    yyresult = 2;
    goto yyreturn;

yyabort:
    yyresult = 1;
    goto yyreturn;

yyaccept:
#if YYBTYACC
    if (yyps->save) goto yyvalid;
#endif /* YYBTYACC */
    yyresult = 0;

yyreturn:
#if defined(YYDESTRUCT_CALL)
    if (yychar != YYEOF && yychar != YYEMPTY)
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
        YYDESTRUCT_CALL("cleanup: discarding token", yychar, &yylval, &yylloc);
#else
        YYDESTRUCT_CALL("cleanup: discarding token", yychar, &yylval);
#endif /* defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED) */

    {
        YYSTYPE *pv;
#if defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED)
        YYLTYPE *pp;

        for (pv = yystack.l_base, pp = yystack.p_base; pv <= yystack.l_mark; ++pv, ++pp)
             YYDESTRUCT_CALL("cleanup: discarding state",
                             yystos[*(yystack.s_base + (pv - yystack.l_base))], pv, pp);
#else
        for (pv = yystack.l_base; pv <= yystack.l_mark; ++pv)
             YYDESTRUCT_CALL("cleanup: discarding state",
                             yystos[*(yystack.s_base + (pv - yystack.l_base))], pv);
#endif /* defined(YYLTYPE) || defined(YYLTYPE_IS_DECLARED) */
    }
#endif /* defined(YYDESTRUCT_CALL) */

#if YYBTYACC
    if (yyerrctx)
    {
        yyFreeState(yyerrctx);
        yyerrctx = NULL;
    }
    while (yyps)
    {
        YYParseState *save = yyps;
        yyps = save->save;
        save->save = NULL;
        yyFreeState(save);
    }
    while (yypath)
    {
        YYParseState *save = yypath;
        yypath = save->save;
        save->save = NULL;
        yyFreeState(save);
    }
#endif /* YYBTYACC */
    yyfreestack(&yystack);
    return (yyresult);
}
