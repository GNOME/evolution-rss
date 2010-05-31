/* Copyright (C) 2002, 2004, 2005, 2007 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifdef WIN32

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#ifdef localtime_r
#undef localtime_r
#endif
#define localtime_r(tp,tmp) memcpy(tmp,localtime(tp),sizeof(struct tm))

#define match_char(ch1, ch2) if (ch1 != ch2) return NULL

#define match_string(cs1, s2) \
  (strncasecmp ((cs1), (s2), strlen (cs1)) ? 0 : ((s2) += strlen (cs1), 1))

/* We intentionally do not use isdigit() for testing because this will
   lead to problems with the wide character version.  */
#define get_number(from, to, n) \
  do {                                        \
    int __n = n;                                  \
    val = 0;                                      \
    while (*rp == ' ')                                \
      ++rp;                                   \
    if (*rp < '0' || *rp > '9')                           \
      return NULL;                                \
    do {                                      \
      val *= 10;                                  \
      val += *rp++ - '0';                             \
    } while (--__n > 0 && val * 10 <= to && *rp >= '0' && *rp <= '9');        \
    if (val < from || val > to)                           \
      return NULL;                                \
  } while (0)
#ifdef _NL_CURRENT
#error what is this
# define get_alt_number(from, to, n) \
  ({                                          \
     __label__ do_normal;                             \
                                          \
     if (s.decided != raw)                            \
       {                                      \
     val = _nl_parse_alt_digit (&rp);           \
     if (val == -1 && s.decided != loc)                   \
       {                                      \
         s.decided = loc;                             \
         goto do_normal;                              \
       }                                      \
    if (val < from || val > to)                       \
      return NULL;                                \
       }                                      \
     else                                     \
       {                                      \
       do_normal:                                 \
     get_number (from, to, n);                        \
       }                                      \
    0;                                        \
  })
#else
# define get_alt_number(from, to, n) \
  /* We don't have the alternate representation.  */                  \
  get_number(from, to, n)
#endif
#define recursive(new_fmt) \
  (*(new_fmt) != '\0'                                 \
   && (rp = __strptime_internal (rp, (new_fmt), tm, &s)) != NULL)


static char const weekday_name[][10] =
  {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
  };
static char const ab_weekday_name[][4] =
  {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
  };
static char const month_name[][10] =
  {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
  };
static char const ab_month_name[][4] =
  {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
# define HERE_D_T_FMT "%a %b %e %H:%M:%S %Y"
# define HERE_D_FMT "%m/%d/%y"
# define HERE_AM_STR "AM"
# define HERE_PM_STR "PM"
# define HERE_T_FMT_AMPM "%I:%M:%S %p"
# define HERE_T_FMT "%H:%M:%S"

static const unsigned short int __mon_yday[2][13] =
  {
    /* Normal years.  */
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
    /* Leap years.  */
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
  };

#ifndef __isleap
/* Nonzero if YEAR is a leap year (every 4 years,
   except every 100th isn't, and every 400th is).  */
# define __isleap(year) \
  ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))
#endif

/* Compute the day of the week.  */
static void
day_of_the_week (struct tm *tm)
{
  /* We know that January 1st 1970 was a Thursday (= 4).  Compute the
     the difference between this data in the one on TM and so determine
     the weekday.  */
  int corr_year = 1900 + tm->tm_year - (tm->tm_mon < 2);
  int wday = (-473
          + (365 * (tm->tm_year - 70))
          + (corr_year / 4)
          - ((corr_year / 4) / 25) + ((corr_year / 4) % 25 < 0)
          + (((corr_year / 4) / 25) / 4)
          + __mon_yday[0][tm->tm_mon]
          + tm->tm_mday - 1);
  tm->tm_wday = ((wday % 7) + 7) % 7;
}

/* Compute the day of the year.  */
static void
day_of_the_year (struct tm *tm)
{
  tm->tm_yday = (__mon_yday[__isleap (1900 + tm->tm_year)][tm->tm_mon]
         + (tm->tm_mday - 1));
}


static char *
__strptime_internal (const char *rp, const char *fmt, struct tm *tmp, void *statep)
{

  const char *rp_backup;
  const char *rp_longest;
  int cnt;
  int cnt_longest;
  size_t val;
#ifdef _NL_CURRENT
  size_t num_eras;
  struct era_entry *era = NULL;
#endif
  enum ptime_locale_status { nott, loc, raw } decided_longest;
  struct __strptime_state
  {
    unsigned int have_I : 1;
    unsigned int have_wday : 1;
    unsigned int have_yday : 1;
    unsigned int have_mon : 1;
    unsigned int have_mday : 1;
    unsigned int have_uweek : 1;
    unsigned int have_wweek : 1;
    unsigned int is_pm : 1;
    unsigned int want_century : 1;
    unsigned int want_era : 1;
    unsigned int want_xday : 1;
    enum ptime_locale_status decided : 2;
    signed char week_no;
    signed char century;
    int era_cnt;
  } s;
  struct tm tmb;
  struct tm *tm;

  if (statep == NULL)
    {
      memset (&s, 0, sizeof (s));
      s.century = -1;
      s.era_cnt = -1;
#ifdef _NL_CURRENT
      s.decided = nott;
#else
      s.decided = raw;
#endif
      tm = tmp;
    }
  else
    {
      s = *(struct __strptime_state *) statep;
      tmb = *tmp;
      tm = &tmb;
    }

  while (*fmt != '\0')
    {
      /* A white space in the format string matches 0 more or white
     space in the input string.  */
      if (isspace (*fmt))
    {
      while (isspace (*rp))
        ++rp;
      ++fmt;
      continue;
    }

      /* Any character but `%' must be matched by the same character
     in the iput string.  */
      if (*fmt != '%')
    {
      match_char (*fmt++, *rp++);
      continue;
    }

      ++fmt;
      if (statep != NULL)
    {
      /* In recursive calls silently discard strftime modifiers.  */
      while (*fmt == '-' || *fmt == '_' || *fmt == '0'
         || *fmt == '^' || *fmt == '#')
        ++fmt;

      /* And field width.  */
      while (*fmt >= '0' && *fmt <= '9')
        ++fmt;
    }

#ifndef _NL_CURRENT
      /* We need this for handling the `E' modifier.  */
    start_over:
#endif

      /* Make back up of current processing pointer.  */
      rp_backup = rp;

      switch (*fmt++)
    {
    case '%':
      /* Match the `%' character itself.  */
      match_char ('%', *rp++);
      break;
    case 'a':
    case 'A':
      /* Match day of week.  */
      rp_longest = NULL;
      decided_longest = s.decided;
      cnt_longest = -1;
      for (cnt = 0; cnt < 7; ++cnt)
        {
          const char *trp;
#ifdef _NL_CURRENT
          if (s.decided !=raw)
        {
          trp = rp;
          if (match_string (_NL_CURRENT (LC_TIME, DAY_1 + cnt), trp)
              && trp > rp_longest)
            {
              rp_longest = trp;
              cnt_longest = cnt;
              if (s.decided == nott
              && strcmp (_NL_CURRENT (LC_TIME, DAY_1 + cnt),
                     weekday_name[cnt]))
            decided_longest = loc;
            }
          trp = rp;
          if (match_string (_NL_CURRENT (LC_TIME, ABDAY_1 + cnt), trp)
              && trp > rp_longest)
            {
              rp_longest = trp;
              cnt_longest = cnt;
              if (s.decided == nott
              && strcmp (_NL_CURRENT (LC_TIME, ABDAY_1 + cnt),
                     ab_weekday_name[cnt]))
            decided_longest = loc;
            }
        }
#endif
          if (s.decided != loc
          && (((trp = rp, match_string (weekday_name[cnt], trp))
               && trp > rp_longest)
              || ((trp = rp, match_string (ab_weekday_name[cnt], rp))
              && trp > rp_longest)))
        {
          rp_longest = trp;
          cnt_longest = cnt;
          decided_longest = raw;
        }
        }
      if (rp_longest == NULL)
        /* Does not match a weekday name.  */
        return NULL;
      rp = rp_longest;
      s.decided = decided_longest;
      tm->tm_wday = cnt_longest;
      s.have_wday = 1;
      break;
    case 'b':
    case 'B':
    case 'h':
      /* Match month name.  */
      rp_longest = NULL;
      decided_longest = s.decided;
      cnt_longest = -1;
      for (cnt = 0; cnt < 12; ++cnt)
        {
          const char *trp;
#ifdef _NL_CURRENT
          if (s.decided !=raw)
        {
          trp = rp;
          if (match_string (_NL_CURRENT (LC_TIME, MON_1 + cnt), trp)
              && trp > rp_longest)
            {
              rp_longest = trp;
              cnt_longest = cnt;
              if (s.decided == nott
              && strcmp (_NL_CURRENT (LC_TIME, MON_1 + cnt),
                     month_name[cnt]))
            decided_longest = loc;
            }
          trp = rp;
          if (match_string (_NL_CURRENT (LC_TIME, ABMON_1 + cnt), trp)
              && trp > rp_longest)
            {
              rp_longest = trp;
              cnt_longest = cnt;
              if (s.decided == nott
              && strcmp (_NL_CURRENT (LC_TIME, ABMON_1 + cnt),
                     ab_month_name[cnt]))
            decided_longest = loc;
            }
        }
#endif
          if (s.decided != loc
          && (((trp = rp, match_string (month_name[cnt], trp))
               && trp > rp_longest)
              || ((trp = rp, match_string (ab_month_name[cnt], trp))
              && trp > rp_longest)))
        {
          rp_longest = trp;
          cnt_longest = cnt;
          decided_longest = raw;
        }
        }
      if (rp_longest == NULL)
        /* Does not match a month name.  */
        return NULL;
      rp = rp_longest;
      s.decided = decided_longest;
      tm->tm_mon = cnt_longest;
      s.have_mon = 1;
      s.want_xday = 1;
      break;
    case 'c':
      /* Match locale's date and time format.  */
#ifdef _NL_CURRENT
      if (s.decided != raw)
        {
          if (!recursive (_NL_CURRENT (LC_TIME, D_T_FMT)))
        {
          if (s.decided == loc)
            return NULL;
          else
            rp = rp_backup;
        }
          else
        {
          if (s.decided == nott &&
              strcmp (_NL_CURRENT (LC_TIME, D_T_FMT), HERE_D_T_FMT))
            s.decided = loc;
          s.want_xday = 1;
          break;
        }
          s.decided = raw;
        }
#endif
      if (!recursive (HERE_D_T_FMT))
        return NULL;
      s.want_xday = 1;
      break;
    case 'C':
      /* Match century number.  */
#ifdef _NL_CURRENT
    match_century:
#endif
      get_number (0, 99, 2);
      s.century = val;
      s.want_xday = 1;
      break;
    case 'd':
    case 'e':
      /* Match day of month.  */
      get_number (1, 31, 2);
      tm->tm_mday = val;
      s.have_mday = 1;
      s.want_xday = 1;
      break;
    case 'F':
      if (!recursive ("%Y-%m-%d"))
        return NULL;
      s.want_xday = 1;
      break;
    case 'x':
#ifdef _NL_CURRENT
      if (s.decided != raw)
        {
          if (!recursive (_NL_CURRENT (LC_TIME, D_FMT)))
        {
          if (s.decided == loc)
            return NULL;
          else
            rp = rp_backup;
        }
          else
        {
          if (s.decided == nott
              && strcmp (_NL_CURRENT (LC_TIME, D_FMT), HERE_D_FMT))
            s.decided = loc;
          s.want_xday = 1;
          break;
        }
          s.decided = raw;
        }
#endif
      /* Fall through.  */
    case 'D':
      /* Match standard day format.  */
      if (!recursive (HERE_D_FMT))
        return NULL;
      s.want_xday = 1;
      break;
    case 'k':
    case 'H':
      /* Match hour in 24-hour clock.  */
      get_number (0, 23, 2);
      tm->tm_hour = val;
      s.have_I = 0;
      break;
    case 'l':
      /* Match hour in 12-hour clock.  GNU extension.  */
    case 'I':
      /* Match hour in 12-hour clock.  */
      get_number (1, 12, 2);
      tm->tm_hour = val % 12;
      s.have_I = 1;
      break;
    case 'j':
      /* Match day number of year.  */
      get_number (1, 366, 3);
      tm->tm_yday = val - 1;
      s.have_yday = 1;
      break;
    case 'm':
      /* Match number of month.  */
      get_number (1, 12, 2);
      tm->tm_mon = val - 1;
      s.have_mon = 1;
      s.want_xday = 1;
      break;
    case 'M':
      /* Match minute.  */
      get_number (0, 59, 2);
      tm->tm_min = val;
      break;
    case 'n':
    case 't':
      /* Match any white space.  */
      while (isspace (*rp))
        ++rp;
      break;
    case 'p':
      /* Match locale's equivalent of AM/PM.  */
#ifdef _NL_CURRENT
      if (s.decided != raw)
        {
          if (match_string (_NL_CURRENT (LC_TIME, AM_STR), rp))
        {
          if (strcmp (_NL_CURRENT (LC_TIME, AM_STR), HERE_AM_STR))
            s.decided = loc;
          s.is_pm = 0;
          break;
        }
          if (match_string (_NL_CURRENT (LC_TIME, PM_STR), rp))
        {
          if (strcmp (_NL_CURRENT (LC_TIME, PM_STR), HERE_PM_STR))
            s.decided = loc;
          s.is_pm = 1;
          break;
        }
          s.decided = raw;
        }
#endif
      if (!match_string (HERE_AM_STR, rp))
        {
          if (match_string (HERE_PM_STR, rp))
        s.is_pm = 1;
          else
        return NULL;
        }
      else
        s.is_pm = 0;
      break;
    case 'r':
#ifdef _NL_CURRENT
      if (s.decided != raw)
        {
          if (!recursive (_NL_CURRENT (LC_TIME, T_FMT_AMPM)))
        {
          if (s.decided == loc)
            return NULL;
          else
            rp = rp_backup;
        }
          else
        {
          if (s.decided == nott &&
              strcmp (_NL_CURRENT (LC_TIME, T_FMT_AMPM),
                  HERE_T_FMT_AMPM))
            s.decided = loc;
          break;
        }
          s.decided = raw;
        }
#endif
      if (!recursive (HERE_T_FMT_AMPM))
        return NULL;
      break;
    case 'R':
      if (!recursive ("%H:%M"))
        return NULL;
      break;
    case 's':
      {
        /* The number of seconds may be very high so we cannot use
           the `get_number' macro.  Instead read the number
           character for character and construct the result while
           doing this.  */
        time_t secs = 0;
        if (*rp < '0' || *rp > '9')
          /* We need at least one digit.  */
          return NULL;

        do
          {
        secs *= 10;
        secs += *rp++ - '0';
          }
        while (*rp >= '0' && *rp <= '9');

        if (localtime_r (&secs, tm) == NULL)
          /* Error in function.  */
          return NULL;
      }
      break;
    case 'S':
      get_number (0, 61, 2);
      tm->tm_sec = val;
      break;
    case 'X':
#ifdef _NL_CURRENT
      if (s.decided != raw)
        {
          if (!recursive (_NL_CURRENT (LC_TIME, T_FMT)))
        {
          if (s.decided == loc)
            return NULL;
          else
            rp = rp_backup;
        }
          else
        {
          if (strcmp (_NL_CURRENT (LC_TIME, T_FMT), HERE_T_FMT))
            s.decided = loc;
          break;
        }
          s.decided = raw;
        }
#endif
      /* Fall through.  */
    case 'T':
      if (!recursive (HERE_T_FMT))
        return NULL;
      break;
    case 'u':
      get_number (1, 7, 1);
      tm->tm_wday = val % 7;
      s.have_wday = 1;
      break;
    case 'g':
      get_number (0, 99, 2);
      /* XXX This cannot determine any field in TM.  */
      break;
    case 'G':
      if (*rp < '0' || *rp > '9')
        return NULL;
      /* XXX Ignore the number since we would need some more
         information to compute a real date.  */
      do
        ++rp;
      while (*rp >= '0' && *rp <= '9');
      break;
    case 'U':
      get_number (0, 53, 2);
      s.week_no = val;
      s.have_uweek = 1;
      break;
    case 'W':
      get_number (0, 53, 2);
      s.week_no = val;
      s.have_wweek = 1;
      break;
    case 'V':
      get_number (0, 53, 2);
      /* XXX This cannot determine any field in TM without some
         information.  */
      break;
    case 'w':
      /* Match number of weekday.  */
      get_number (0, 6, 1);
      tm->tm_wday = val;
      s.have_wday = 1;
      break;
    case 'y':
#ifdef _NL_CURRENT
    match_year_in_century:
#endif
      /* Match year within century.  */
      get_number (0, 99, 2);
      /* The "Year 2000: The Millennium Rollover" paper suggests that
         values in the range 69-99 refer to the twentieth century.  */
      tm->tm_year = val >= 69 ? val : val + 100;
      /* Indicate that we want to use the century, if specified.  */
      s.want_century = 1;
      s.want_xday = 1;
      break;
    case 'Y':
      /* Match year including century number.  */
      get_number (0, 9999, 4);
      tm->tm_year = val - 1900;
      s.want_century = 0;
      s.want_xday = 1;
      break;
    case 'Z':
      /* XXX How to handle this?  */
      break;
    case 'z':
      /* We recognize two formats: if two digits are given, these
         specify hours.  If fours digits are used, minutes are
         also specified.  */
      {
        val = 0;
        while (*rp == ' ')
          ++rp;
        if (*rp != '+' && *rp != '-')
          return NULL;
        rp++;
        int n = 0;
        while (n < 4 && *rp >= '0' && *rp <= '9')
          {
        val = val * 10 + *rp++ - '0';
        ++n;
          }
        if (n == 2)
          val *= 100;
        else if (n != 4)
          /* Only two or four digits recognized.  */
          return NULL;
        else
          {
        /* We have to convert the minutes into decimal.  */
        if (val % 100 >= 60)
          return NULL;
        val = (val / 100) * 100 + ((val % 100) * 50) / 30;
          }
        if (val > 1200)
          return NULL;
      }
      break;
    case 'E':
#ifdef _NL_CURRENT
      switch (*fmt++)
        {
        case 'c':
          /* Match locale's alternate date and time format.  */
          if (s.decided != raw)
        {
          const char *fmt = _NL_CURRENT (LC_TIME, ERA_D_T_FMT);

          if (*fmt == '\0')
            fmt = _NL_CURRENT (LC_TIME, D_T_FMT);

          if (!recursive (fmt))
            {
              if (s.decided == loc)
            return NULL;
              else
            rp = rp_backup;
            }
          else
            {
              if (strcmp (fmt, HERE_D_T_FMT))
            s.decided = loc;
              s.want_xday = 1;
              break;
            }
          s.decided = raw;
        }
          /* The C locale has no era information, so use the
         normal representation.  */
          if (!recursive (HERE_D_T_FMT))
        return NULL;
          s.want_xday = 1;
          break;
        case 'C':
          if (s.decided != raw)
        {
          if (s.era_cnt >= 0)
            {
              era = _nl_select_era_entry (s.era_cnt);
              if (era != NULL && match_string (era->era_name, rp))
            {
              s.decided = loc;
              break;
            }
              else
            return NULL;
            }

          num_eras = _NL_CURRENT_WORD (LC_TIME,
                           _NL_TIME_ERA_NUM_ENTRIES);
          for (s.era_cnt = 0; s.era_cnt < (int) num_eras;
               ++s.era_cnt, rp = rp_backup)
            {
              era = _nl_select_era_entry (s.era_cnt);
              if (era != NULL && match_string (era->era_name, rp))
            {
              s.decided = loc;
              break;
            }
            }
          if (s.era_cnt != (int) num_eras)
            break;

          s.era_cnt = -1;
          if (s.decided == loc)
            return NULL;

          s.decided = raw;
        }
          /* The C locale has no era information, so use the
         normal representation.  */
          goto match_century;
        case 'y':
          if (s.decided != raw)
        {
          get_number(0, 9999, 4);
          tm->tm_year = val;
          s.want_era = 1;
          s.want_xday = 1;
          s.want_century = 1;

          if (s.era_cnt >= 0)
            {
              assert (s.decided == loc);

              era = _nl_select_era_entry (s.era_cnt);
              bool match = false;
              if (era != NULL)
            {
              int delta = ((tm->tm_year - era->offset)
                       * era->absolute_direction);
              match = (delta >= 0
                   && delta < (((int64_t) era->stop_date[0]
                        - (int64_t) era->start_date[0])
                           * era->absolute_direction));
            }
              if (! match)
            return NULL;

              break;
            }

          num_eras = _NL_CURRENT_WORD (LC_TIME,
                           _NL_TIME_ERA_NUM_ENTRIES);
          for (s.era_cnt = 0; s.era_cnt < (int) num_eras; ++s.era_cnt)
            {
              era = _nl_select_era_entry (s.era_cnt);
              if (era != NULL)
            {
              int delta = ((tm->tm_year - era->offset)
                       * era->absolute_direction);
              if (delta >= 0
                  && delta < (((int64_t) era->stop_date[0]
                       - (int64_t) era->start_date[0])
                      * era->absolute_direction))
                {
                  s.decided = loc;
                  break;
                }
            }
            }
          if (s.era_cnt != (int) num_eras)
            break;

          s.era_cnt = -1;
          if (s.decided == loc)
            return NULL;

          s.decided = raw;
        }

          goto match_year_in_century;
        case 'Y':
          if (s.decided != raw)
        {
          num_eras = _NL_CURRENT_WORD (LC_TIME,
                           _NL_TIME_ERA_NUM_ENTRIES);
          for (s.era_cnt = 0; s.era_cnt < (int) num_eras;
               ++s.era_cnt, rp = rp_backup)
            {
              era = _nl_select_era_entry (s.era_cnt);
              if (era != NULL && recursive (era->era_format))
            break;
            }
          if (s.era_cnt == (int) num_eras)
            {
              s.era_cnt = -1;
              if (s.decided == loc)
            return NULL;
              else
            rp = rp_backup;
            }
          else
            {
              s.decided = loc;
              s.era_cnt = -1;
              break;
            }

          s.decided = raw;
        }
          get_number (0, 9999, 4);
          tm->tm_year = val - 1900;
          s.want_century = 0;
          s.want_xday = 1;
          break;
        case 'x':
          if (s.decided != raw)
        {
          const char *fmt = _NL_CURRENT (LC_TIME, ERA_D_FMT);

          if (*fmt == '\0')
            fmt = _NL_CURRENT (LC_TIME, D_FMT);

          if (!recursive (fmt))
            {
              if (s.decided == loc)
            return NULL;
              else
            rp = rp_backup;
            }
          else
            {
              if (strcmp (fmt, HERE_D_FMT))
            s.decided = loc;
              break;
            }
          s.decided = raw;
        }
          if (!recursive (HERE_D_FMT))
        return NULL;
          break;
        case 'X':
          if (s.decided != raw)
        {
          const char *fmt = _NL_CURRENT (LC_TIME, ERA_T_FMT);

          if (*fmt == '\0')
            fmt = _NL_CURRENT (LC_TIME, T_FMT);

          if (!recursive (fmt))
            {
              if (s.decided == loc)
            return NULL;
              else
            rp = rp_backup;
            }
          else
            {
              if (strcmp (fmt, HERE_T_FMT))
            s.decided = loc;
              break;
            }
          s.decided = raw;
        }
          if (!recursive (HERE_T_FMT))
        return NULL;
          break;
        default:
          return NULL;
        }
      break;
#else
      /* We have no information about the era format.  Just use
         the normal format.  */
      if (*fmt != 'c' && *fmt != 'C' && *fmt != 'y' && *fmt != 'Y'
          && *fmt != 'x' && *fmt != 'X')
        /* This is an illegal format.  */
        return NULL;

      goto start_over;
#endif
    case 'O':
      switch (*fmt++)
        {
        case 'd':
        case 'e':
          /* Match day of month using alternate numeric symbols.  */
          get_alt_number (1, 31, 2);
          tm->tm_mday = val;
          s.have_mday = 1;
          s.want_xday = 1;
          break;
        case 'H':
          /* Match hour in 24-hour clock using alternate numeric
         symbols.  */
          get_alt_number (0, 23, 2);
          tm->tm_hour = val;
          s.have_I = 0;
          break;
        case 'I':
          /* Match hour in 12-hour clock using alternate numeric
         symbols.  */
          get_alt_number (1, 12, 2);
          tm->tm_hour = val % 12;
          s.have_I = 1;
          break;
        case 'm':
          /* Match month using alternate numeric symbols.  */
          get_alt_number (1, 12, 2);
          tm->tm_mon = val - 1;
          s.have_mon = 1;
          s.want_xday = 1;
          break;
        case 'M':
          /* Match minutes using alternate numeric symbols.  */
          get_alt_number (0, 59, 2);
          tm->tm_min = val;
          break;
        case 'S':
          /* Match seconds using alternate numeric symbols.  */
          get_alt_number (0, 61, 2);
          tm->tm_sec = val;
          break;
        case 'U':
          get_alt_number (0, 53, 2);
          s.week_no = val;
          s.have_uweek = 1;
          break;
        case 'W':
          get_alt_number (0, 53, 2);
          s.week_no = val;
          s.have_wweek = 1;
          break;
        case 'V':
          get_alt_number (0, 53, 2);
          /* XXX This cannot determine any field in TM without
         further information.  */
          break;
        case 'w':
          /* Match number of weekday using alternate numeric symbols.  */
          get_alt_number (0, 6, 1);
          tm->tm_wday = val;
          s.have_wday = 1;
          break;
        case 'y':
          /* Match year within century using alternate numeric symbols.  */
          get_alt_number (0, 99, 2);
          tm->tm_year = val >= 69 ? val : val + 100;
          s.want_xday = 1;
          break;
        default:
          return NULL;
        }
      break;
    default:
      return NULL;
    }
    }

  if (statep != NULL)
    {
      /* Recursive invocation, returning success, so
     update parent's struct tm and state.  */
      *(struct __strptime_state *) statep = s;
      *tmp = tmb;
      return (char *) rp;
    }

  if (s.have_I && s.is_pm)
    tm->tm_hour += 12;

  if (s.century != -1)
    {
      if (s.want_century)
    tm->tm_year = tm->tm_year % 100 + (s.century - 19) * 100;
      else
    /* Only the century, but not the year.  Strange, but so be it.  */
    tm->tm_year = (s.century - 19) * 100;
    }

  if (s.want_xday && !s.have_wday)
    {
      if ( !(s.have_mon && s.have_mday) && s.have_yday)
    {
      /* We don't have tm_mon and/or tm_mday, compute them.  */
      int t_mon = 0;
      while (__mon_yday[__isleap(1900 + tm->tm_year)][t_mon] <= tm->tm_yday)
          t_mon++;
      if (!s.have_mon)
          tm->tm_mon = t_mon - 1;
      if (!s.have_mday)
          tm->tm_mday =
        (tm->tm_yday
         - __mon_yday[__isleap(1900 + tm->tm_year)][t_mon - 1] + 1);
      s.have_mon = 1;
      s.have_mday = 1;
    }
      /* Don't crash in day_of_the_week if tm_mon is uninitialized.  */
      if (s.have_mon || (unsigned) tm->tm_mon <= 11)
    day_of_the_week (tm);
    }

  if (s.want_xday && !s.have_yday && (s.have_mon || (unsigned) tm->tm_mon <= 11))
    day_of_the_year (tm);

  if ((s.have_uweek || s.have_wweek) && s.have_wday)
    {
      int save_wday = tm->tm_wday;
      int save_mday = tm->tm_mday;
      int save_mon = tm->tm_mon;
      int w_offset = s.have_uweek ? 0 : 1;

      tm->tm_mday = 1;
      tm->tm_mon = 0;
      day_of_the_week (tm);
      if (s.have_mday)
    tm->tm_mday = save_mday;
      if (s.have_mon)
    tm->tm_mon = save_mon;

      if (!s.have_yday)
    tm->tm_yday = ((7 - (tm->tm_wday - w_offset)) % 7
               + (s.week_no - 1) *7
               + save_wday - w_offset);

      if (!s.have_mday || !s.have_mon)
    {
      int t_mon = 0;
      while (__mon_yday[__isleap(1900 + tm->tm_year)][t_mon]
         <= tm->tm_yday)
        t_mon++;
      if (!s.have_mon)
        tm->tm_mon = t_mon - 1;
      if (!s.have_mday)
          tm->tm_mday =
        (tm->tm_yday
         - __mon_yday[__isleap(1900 + tm->tm_year)][t_mon - 1] + 1);
    }

      tm->tm_wday = save_wday;
    }

  return (char *) rp;
}

char *
strptime (const char *buf, const char *format, struct tm *tm);

char *
strptime (const char *buf, const char *format, struct tm *tm)
{
  return __strptime_internal (buf, format, tm, NULL);
}

#endif
