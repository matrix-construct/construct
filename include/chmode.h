/*
 *  charybdis: An advanced ircd.
 *  chmode.h: The ircd channel header.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
 *  Copyright (C) 2008 charybdis development team
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
 *  $Id$
 */

#ifndef INCLUDED_chmode_h
#define INCLUDED_chmode_h

/* something not included in messages.tab
 * to change some hooks behaviour when needed
 * -- dwr
 */
#define ERR_CUSTOM 1000

extern int chmode_flags[256];

extern void chm_nosuch(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type);
extern void chm_orphaned(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type);
extern void chm_simple(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type);
extern void chm_ban(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type);
extern void chm_staff(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type);
extern void chm_forward(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type);
extern void chm_throttle(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type);
extern void chm_key(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type);
extern void chm_limit(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type);
extern void chm_regonly(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type);
extern void chm_op(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type);
extern void chm_voice(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type);

extern unsigned int cflag_add(char c, ChannelModeFunc function);
extern void cflag_orphan(char c);
extern void construct_cflags_strings(void);
extern char cflagsbuf[256];
extern char cflagsmyinfo[256];

#endif
