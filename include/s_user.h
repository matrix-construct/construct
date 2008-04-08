/*
 *  ircd-ratbox: A slightly useful ircd.
 *  s_user.h: A header for the user functions.
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
 *  $Id: s_user.h 3201 2007-02-04 01:59:38Z jilles $
 */

#ifndef INCLUDED_s_user_h
#define INCLUDED_s_user_h

#include "config.h"

struct Client;
struct User;
struct oper_conf;
extern time_t LastUsedWallops;

extern int valid_hostname(const char *hostname);
extern int valid_username(const char *username);

extern int user_mode(struct Client *, struct Client *, int, const char **);
extern void send_umode(struct Client *, struct Client *, int, int, char *);
extern void send_umode_out(struct Client *, struct Client *, int);
extern int show_lusers(struct Client *source_p);
extern int register_local_user(struct Client *, struct Client *, const char *);

extern int introduce_client(struct Client *client_p, struct Client *source_p, 
			    struct User *user, const char *nick, int use_euid);

extern void change_nick_user_host(struct Client *target_p, const char *nick, const char *user,
				  const char *host, int newts, const char *format, ...);

extern int user_modes[256];
extern unsigned int find_umode_slot(void);
extern void construct_umodebuf(void);

extern int oper_up(struct Client *, struct oper_conf *);

#endif
