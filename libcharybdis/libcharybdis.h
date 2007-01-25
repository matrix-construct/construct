/*
 *  charybdis: A slightly useful ircd.
 *  libcharybdis.h: library entrypoint
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
 *  Copyright (C) 2005 William Pitcock and Jilles Tjoelker
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
 *  $Id: libcharybdis.h 388 2005-12-07 16:34:40Z nenolod $
 */

#ifndef _LIBCHARYBDIS_H
#define _LIBCHARYBDIS_H

#include "stdinc.h"
#include "res.h"
#include "numeric.h"
#include "tools.h"
#include "memory.h"
#include "balloc.h"
#include "linebuf.h"
#include "sprintf_irc.h"
#include "commio.h"
#include "event.h"

extern void libcharybdis_log(const char *str, ...);
extern void libcharybdis_restart(const char *str, ...);
extern void libcharybdis_die(const char *str, ...);
extern void libcharybdis_init(void (*)(const char *),
	void (*)(const char *), void (*)(const char *));

#endif
