/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kim Letkeman.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* 1999-02-01	Jean-Francois Bignolles: added option '-m' to display
 * 		monday as the first day of the week.
 * 1999-02-22 Arkadiusz Mi�kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 * 2000-09-01  Michael Charles Pruznick <dummy@netwiz.net> 
 *             Added "-3" option to print prev/next month with current.
 *             Added over-ridable default NUM_MONTHS and "-1" option to
 *             get traditional output when -3 is the default.  I hope that
 *             enough people will like -3 as the default that one day the
 *             product can be shipped that way.
 */

#include <sys/types.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <locale.h>
#include "errs.h"
#include "nls.h"
#include "../defines.h"

#ifdef HAVE_langinfo_h
# include <langinfo.h>
#else
# include <localeinfo.h>	/* libc4 only */
#endif

/* allow compile-time define to over-ride default */
#ifndef NUM_MONTHS
#define NUM_MONTHS 1
#endif

#if ( NUM_MONTHS != 1 && NUM_MONTHS !=3 )
#error NUM_MONTHS must be 1 or 3
#endif

#define	THURSDAY		4		/* for reformation */
#define	SATURDAY 		6		/* 1 Jan 1 was a Saturday */

#define	FIRST_MISSING_DAY 	639799		/* 3 Sep 1752 */
#define	NUMBER_MISSING_DAYS 	11		/* 11 day correction */

#define	MAXDAYS			43		/* max slots in a month array */
#define	SPACE			-1		/* used in day array */

static int days_in_month[2][13] = {
	{0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
	{0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
};

int sep1752[MAXDAYS] = {
	SPACE,	SPACE,	1,	2,	14,	15,	16,
	17,	18,	19,	20,	21,	22,	23,
	24,	25,	26,	27,	28,	29,	30,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE
}, j_sep1752[MAXDAYS] = {
	SPACE,	SPACE,	245,	246,	258,	259,	260,
	261,	262,	263,	264,	265,	266,	267,
	268,	269,	270,	271,	272,	273,	274,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE
}, empty[MAXDAYS] = {
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE
};

char day_headings[]   = " S  M Tu  W Th  F  S ";
/* week1stday = 1  =>   " M Tu  W Th  F  S  S " */
char j_day_headings[] = "Sun Mon Tue Wed Thu Fri Sat ";
/* week1stday = 1  =>   "  M  Tu   W  Th   F   S   S " */
const char *full_month[12];

/* leap year -- account for gregorian reformation in 1752 */
#define	leap_year(yr) \
	((yr) <= 1752 ? !((yr) % 4) : \
	(!((yr) % 4) && ((yr) % 100)) || !((yr) % 400))

/* number of centuries since 1700, not inclusive */
#define	centuries_since_1700(yr) \
	((yr) > 1700 ? (yr) / 100 - 17 : 0)

/* number of centuries since 1700 whose modulo of 400 is 0 */
#define	quad_centuries_since_1700(yr) \
	((yr) > 1600 ? ((yr) - 1600) / 400 : 0)

/* number of leap years between year 1 and this year, not inclusive */
#define	leap_years_since_year_1(yr) \
	((yr) / 4 - centuries_since_1700(yr) + quad_centuries_since_1700(yr))

/* 0 => sunday (default), 1 => monday */
int week1stday;
int julian;

#define FMT_ST_LINES 8
#define FMT_ST_CHARS 300	/* 90 suffices in most locales */
struct fmt_st
{
  char s[FMT_ST_LINES][FMT_ST_CHARS];
};

void	ascii_day __P((char *, int));
void	center __P((const char *, int, int));
void	day_array __P((int, int, int *));
int	day_in_week __P((int, int, int));
int	day_in_year __P((int, int, int));
void	j_yearly __P((int));
void	do_monthly __P((int, int, struct fmt_st*));
void	monthly __P((int, int));
void	monthly3 __P((int, int));
void	trim_trailing_spaces __P((char *));
void	usage __P((void));
void	yearly __P((int));
void    headers_init(void);
extern char *__progname;

int
main(int argc, char **argv) {
	struct tm *local_time;
	time_t now;
	int ch, month, year, yflag;
	char *progname, *p;
	int num_months = NUM_MONTHS;

	progname = argv[0];
	if ((p = strrchr(progname, '/')) != NULL)
		progname = p+1;
	__progname = progname;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	
	yflag = 0;
	while ((ch = getopt(argc, argv, "13mjyV")) != EOF)
		switch(ch) {
 		case '1':
 			num_months = 1;
 			break;
 		case '3':
 			num_months = 3;
 			break;
		case 'm':
			week1stday = 1;
			break;
		case 'j':
			julian = 1;
			break;
		case 'y':
			yflag = 1;
			break;
		case 'V':
			printf(_("%s from %s\n"),
			       progname, util_linux_version);
			return 0;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	month = year = 0;
	switch(argc) {
	case 2:
		if ((month = atoi(*argv++)) < 1 || month > 12)
			errx(1, _("illegal month value: use 1-12"));
		/* FALLTHROUGH */
	case 1:
		if ((year = atoi(*argv)) < 1 || year > 9999)
			errx(1, _("illegal year value: use 1-9999"));
		break;
	case 0:
		(void)time(&now);
		local_time = localtime(&now);
		year = local_time->tm_year + 1900;
		if (!yflag)
			month = local_time->tm_mon + 1;
		break;
	default:
		usage();
	}
	headers_init();

	if (month && num_months == 1)
		monthly(month, year);
	else if (month && num_months == 3)
		monthly3(month, year);
	else if (julian)
		j_yearly(year);
	else
		yearly(year);
	exit(0);
}

#define	DAY_LEN		3		/* 3 spaces per day */
#define	J_DAY_LEN	4		/* 4 spaces per day */
#define	WEEK_LEN	21		/* 7 days * 3 characters */
#define	J_WEEK_LEN	28		/* 7 days * 4 characters */
#define	HEAD_SEP	2		/* spaces between day headings */
#define	J_HEAD_SEP	2

void headers_init(void)
{
  int i, wd;

  strcpy(day_headings,"");
  strcpy(j_day_headings,"");

#ifdef HAVE_langinfo_h
# define weekday(wd)	nl_langinfo(ABDAY_1+wd)
#else
# define weekday(wd)	_time_info->abbrev_wkday[wd]
#endif
  
  for(i = 0 ; i < 7 ; i++ ) {
     wd = (i + week1stday) % 7;
     strncat(day_headings,weekday(wd),2);
     strncat(j_day_headings,weekday(wd),3);
     if (strlen(weekday(wd)) == 2)
	strcat(j_day_headings," ");
     strcat(day_headings," ");
     strcat(j_day_headings," ");
  }

#undef weekday
  
  for (i = 0; i < 12; i++) {
#ifdef HAVE_langinfo_h
     full_month[i] = nl_langinfo(MON_1+i);
#else
     full_month[i] = _time_info->full_month[i];
#endif
  }
}

void
do_monthly(month, year, out)
	int month, year;
	struct fmt_st* out;
{
	int col, row, len, days[MAXDAYS];
	char *p, lineout[300];

	day_array(month, year, days);
	len = sprintf(lineout, "%s %d", full_month[month - 1], year);
	(void)sprintf(out->s[0],"%*s%s",
	    ((julian ? J_WEEK_LEN : WEEK_LEN) - len) / 2, "", lineout );
	(void)sprintf(out->s[1],"%s",
	    julian ? j_day_headings : day_headings);
	for (row = 0; row < 6; row++) {
		for (col = 0, p = lineout; col < 7; col++,
		    p += julian ? J_DAY_LEN : DAY_LEN)
			ascii_day(p, days[row * 7 + col]);
		*p = '\0';
		trim_trailing_spaces(lineout);
		(void)sprintf(out->s[row+2],"%s", lineout);
	}
}

void
monthly(month, year)
	int month, year;
{
	int i;
	struct fmt_st out;

	do_monthly(month, year, &out);
	for ( i = 0; i < FMT_ST_LINES; i++ )
	{
	  printf("%s\n", out.s[i]);
	}
}

void
monthly3(month, year)
	int month, year;
{
	int i;
	int width;
	struct fmt_st out_prev;
	struct fmt_st out_curm;
	struct fmt_st out_next;
	int prev_month, prev_year;
	int next_month, next_year;

	if ( month == 1 )
	{
	  prev_month = 12;
	  prev_year  = year - 1;
	}
	else
	{
	  prev_month = month - 1;
	  prev_year  = year;
	}
	if ( month == 12 )
	{
	  next_month = 1;
	  next_year  = year + 1;
	}
	else
	{
	  next_month = month + 1;
	  next_year  = year;
	}

	do_monthly(prev_month, prev_year, &out_prev);
	do_monthly(month,      year,      &out_curm);
	do_monthly(next_month, next_year, &out_next);
        width = (julian ? J_WEEK_LEN : WEEK_LEN);
	for ( i = 0; i < FMT_ST_LINES; i++ )
	{
	  printf("%-*.*s %-*.*s %-*.*s\n", 
	      width, width, out_prev.s[i], 
	      width, width, out_curm.s[i], 
	      width, width, out_next.s[i] );
	}
}

void
j_yearly(year)
	int year;
{
	int col, *dp, i, month, row, which_cal;
	int days[12][MAXDAYS];
	char *p, lineout[80];

	(void)sprintf(lineout, "%d", year);
	center(lineout, J_WEEK_LEN * 2 + J_HEAD_SEP, 0);
	(void)printf("\n\n");
	for (i = 0; i < 12; i++)
		day_array(i + 1, year, days[i]);
	(void)memset(lineout, ' ', sizeof(lineout) - 1);
	lineout[sizeof(lineout) - 1] = '\0';
	for (month = 0; month < 12; month += 2) {
		center(full_month[month], J_WEEK_LEN, J_HEAD_SEP);
		center(full_month[month + 1], J_WEEK_LEN, 0);
		(void)printf("\n%s%*s%s\n", j_day_headings, J_HEAD_SEP, "",
		    j_day_headings);
		for (row = 0; row < 6; row++) {
			for (which_cal = 0; which_cal < 2; which_cal++) {
				p = lineout + which_cal * (J_WEEK_LEN + 2);
				dp = &days[month + which_cal][row * 7];
				for (col = 0; col < 7; col++, p += J_DAY_LEN)
					ascii_day(p, *dp++);
			}
			*p = '\0';
			trim_trailing_spaces(lineout);
			(void)printf("%s\n", lineout);
		}
	}
	(void)printf("\n");
}

void
yearly(year)
	int year;
{
	int col, *dp, i, month, row, which_cal;
	int days[12][MAXDAYS];
	char *p, lineout[80];

	(void)sprintf(lineout, "%d", year);
	center(lineout, WEEK_LEN * 3 + HEAD_SEP * 2, 0);
	(void)printf("\n\n");
	for (i = 0; i < 12; i++)
		day_array(i + 1, year, days[i]);
	(void)memset(lineout, ' ', sizeof(lineout) - 1);
	lineout[sizeof(lineout) - 1] = '\0';
	for (month = 0; month < 12; month += 3) {
		center(full_month[month], WEEK_LEN, HEAD_SEP);
		center(full_month[month + 1], WEEK_LEN, HEAD_SEP);
		center(full_month[month + 2], WEEK_LEN, 0);
		(void)printf("\n%s%*s%s%*s%s\n", day_headings, HEAD_SEP,
		    "", day_headings, HEAD_SEP, "", day_headings);
		for (row = 0; row < 6; row++) {
			for (which_cal = 0; which_cal < 3; which_cal++) {
				p = lineout + which_cal * (WEEK_LEN + 2);
				dp = &days[month + which_cal][row * 7];
				for (col = 0; col < 7; col++, p += DAY_LEN)
					ascii_day(p, *dp++);
			}
			*p = '\0';
			trim_trailing_spaces(lineout);
			(void)printf("%s\n", lineout);
		}
	}
	(void)printf("\n");
}

/*
 * day_array --
 *	Fill in an array of 42 integers with a calendar.  Assume for a moment
 *	that you took the (maximum) 6 rows in a calendar and stretched them
 *	out end to end.  You would have 42 numbers or spaces.  This routine
 *	builds that array for any month from Jan. 1 through Dec. 9999.
 */
void
day_array(month, year, days)
	int month, year;
	int *days;
{
	int day, dw, dm;
	int *d_sep1752;

	if (month == 9 && year == 1752) {
		d_sep1752 = julian ? j_sep1752 : sep1752;
		memcpy(days, d_sep1752 + week1stday, MAXDAYS * sizeof(int));
		return;
	}
	memcpy(days, empty, MAXDAYS * sizeof(int));
	dm = days_in_month[leap_year(year)][month];
	dw = (day_in_week(1, month, year) - week1stday + 7) % 7;
	day = julian ? day_in_year(1, month, year) : 1;
	while (dm--)
		days[dw++] = day++;
}

/*
 * day_in_year --
 *	return the 1 based day number within the year
 */
int
day_in_year(day, month, year)
	int day, month, year;
{
	int i, leap;

	leap = leap_year(year);
	for (i = 1; i < month; i++)
		day += days_in_month[leap][i];
	return (day);
}

/*
 * day_in_week
 *	return the 0 based day number for any date from 1 Jan. 1 to
 *	31 Dec. 9999.  Assumes the Gregorian reformation eliminates
 *	3 Sep. 1752 through 13 Sep. 1752.  Returns Thursday for all
 *	missing days.
 */
int
day_in_week(day, month, year)
	int day, month, year;
{
	long temp;

	temp = (long)(year - 1) * 365 + leap_years_since_year_1(year - 1)
	    + day_in_year(day, month, year);
	if (temp < FIRST_MISSING_DAY)
		return ((temp - 1 + SATURDAY) % 7);
	if (temp >= (FIRST_MISSING_DAY + NUMBER_MISSING_DAYS))
		return (((temp - 1 + SATURDAY) - NUMBER_MISSING_DAYS) % 7);
	return (THURSDAY);
}

void
ascii_day(p, day)
	char *p;
	int day;
{
	int display, val;
	static char *aday[] = {
		"",
		" 1", " 2", " 3", " 4", " 5", " 6", " 7",
		" 8", " 9", "10", "11", "12", "13", "14",
		"15", "16", "17", "18", "19", "20", "21",
		"22", "23", "24", "25", "26", "27", "28",
		"29", "30", "31",
	};

	if (day == SPACE) {
		memset(p, ' ', julian ? J_DAY_LEN : DAY_LEN);
		return;
	}
	if (julian) {
		if ((val = day / 100)) {
			day %= 100;
			*p++ = val + '0';
			display = 1;
		} else {
			*p++ = ' ';
			display = 0;
		}
		val = day / 10;
		if (val || display)
			*p++ = val + '0';
		else
			*p++ = ' ';
		*p++ = day % 10 + '0';
	} else {
		*p++ = aday[day][0];
		*p++ = aday[day][1];
	}
	*p = ' ';
}

void
trim_trailing_spaces(s)
	char *s;
{
	char *p;

	for (p = s; *p; ++p)
		continue;
	while (p > s && isspace(*--p))
		continue;
	if (p > s)
		++p;
	*p = '\0';
}

void
center(str, len, separate)
	const char *str;
	int len;
	int separate;
{

	len -= strlen(str);
	(void)printf("%*s%s%*s", len / 2, "", str, len / 2 + len % 2, "");
	if (separate)
		(void)printf("%*s", separate, "");
}

void
usage()
{

	(void)fprintf(stderr, _("usage: cal [-mjyV] [[month] year]\n"));
	exit(1);
}
