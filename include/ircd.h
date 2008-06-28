/*
 *  ircd-ratbox: A slightly useful ircd.
 *  ircd.h: A header for the ircd startup routines.
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
 *  $Id: ircd.h 1851 2006-08-24 17:16:53Z jilles $
 */

#ifndef INCLUDED_ircd_h
#define INCLUDED_ircd_h

#include "config.h"

struct Client;
struct rb_dlink_list;

struct SetOptions
{
	int maxclients;		/* max clients allowed */
	int autoconn;		/* autoconn enabled for all servers? */

	int floodcount;		/* Number of messages in 1 second */
	int ident_timeout;	/* timeout for identd lookups */

	int spam_num;
	int spam_time;

	char operstring[REALLEN];
	char adminstring[REALLEN];
};

struct Counter
{
	int oper;		/* Opers */
	int total;		/* total clients */
	int invisi;		/* invisible clients */
	int max_loc;		/* MAX local clients */
	int max_tot;		/* MAX global clients */
	unsigned long totalrestartcount;	/* Total client count ever */
};

extern struct SetOptions GlobalSetOptions;	/* defined in ircd.c */

extern const char *creation;
extern const char *generation;
extern const char *platform;
extern const char *infotext[];
extern const char *serno;
extern const char *ircd_version;
extern const char *logFileName;
extern const char *pidFileName;
extern int cold_start;
extern int dorehash;
extern int dorehashbans;
extern int doremotd;
extern int kline_queued;
extern int server_state_foreground;
extern int opers_see_all_users; /* sno_farconnect.so loaded, operspy without
				   accountability, etc */

extern struct Client me;
extern rb_dlink_list global_client_list;
extern struct Client *local[];
extern struct Counter Count;
extern int default_server_capabs;

extern time_t startup_time;

extern int splitmode;
extern int splitchecking;
extern int split_users;
extern int split_servers;
extern int eob_count;

extern rb_dlink_list unknown_list;
extern rb_dlink_list lclient_list;
extern rb_dlink_list serv_list;
extern rb_dlink_list global_serv_list;
extern rb_dlink_list local_oper_list;
extern rb_dlink_list oper_list;
extern rb_dlink_list dead_list;

extern int testing_conf;

extern struct ev_entry *check_splitmode_ev;

extern int ssl_ok;
extern int zlib_ok;
extern int maxconnections;

void ircd_shutdown(const char *reason);

#endif
