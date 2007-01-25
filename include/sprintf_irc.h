/*
 *  ircd-ratbox: A slightly useful ircd.
 *  sprintf_irc.h: The irc sprintf header.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 *  $Id: sprintf_irc.h 6 2005-09-10 01:02:21Z nenolod $
 */

#ifndef SPRINTF_IRC
#define SPRINTF_IRC

#include <stdarg.h>

/*=============================================================================
 * Proto types
 */


/*
 * ircsprintf - optimized sprintf
 */
#ifdef __GNUC__
int ircsprintf(char *str, const char *fmt, ...) __attribute((format(printf, 2, 3)));
int ircsnprintf(char *str, const size_t size, const char *, ...) __attribute__ ((format(printf, 3, 4)));
#else
int ircsprintf(char *str, const char *format, ...);
int ircsnprintf(char *str, const size_t size, const char *, ...);
#endif

int ircvsnprintf(char *str, const size_t size, const char *fmt, va_list args);
int ircvsprintf(char *str, const char *fmt, va_list args);

#endif /* SPRINTF_IRC */
