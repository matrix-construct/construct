/*
 *  ircd-ratbox: A slightly useful ircd.
 *  sprintf_rb_.h: The irc sprintf header.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 *  $Id: rb_snprintf.h 26092 2008-09-19 15:13:52Z androsyn $
 */

#ifndef RB_LIB_H
# error "Do not use snprintf.h directly"
#endif

#ifndef SPRINTF_IRC
#define SPRINTF_IRC

/*=============================================================================
 * Proto types
 */


/*
 * rb_sprintf - optimized sprintf
 */
#ifdef __GNUC__
int
rb_sprintf(char *str, const char *fmt, ...)
__attribute((format(printf, 2, 3)));
     int rb_snprintf(char *str, const size_t size, const char *, ...)
	__attribute__ ((format(printf, 3, 4)));
     int rb_sprintf_append(char *str, const char *format, ...) __attribute((format(printf, 2, 3)));
     int rb_snprintf_append(char *str, size_t len, const char *format, ...)
	__attribute__ ((format(printf, 3, 4)));
#else
int rb_sprintf(char *str, const char *format, ...);
int rb_snprintf(char *str, const size_t size, const char *, ...);
int rb_sprintf_append(char *str, const char *format, ...);
int rb_snprintf_append(char *str, const size_t size, const char *, ...);

#endif

int rb_vsnprintf(char *str, const size_t size, const char *fmt, va_list args);
int rb_vsprintf(char *str, const char *fmt, va_list args);
int rb_vsnprintf_append(char *str, const size_t size, const char *fmt, va_list args);
int rb_vsprintf_append(char *str, const char *fmt, va_list args);

#endif /* SPRINTF_IRC */
