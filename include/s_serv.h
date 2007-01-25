/*
 *  ircd-ratbox: A slightly useful ircd.
 *  s_serv.h: A header for the server functions.
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
 *  $Id: s_serv.h 1863 2006-08-27 13:40:37Z jilles $
 */

#ifndef INCLUDED_serv_h
#define INCLUDED_serv_h

#include "config.h"

/*
 * The number of seconds between calls to try_connections(). Fiddle with
 * this ONLY if you KNOW what you're doing!
 */
#define TRY_CONNECTIONS_TIME	60

/* collect ziplinks compression ratios/etc every minute */
#define ZIPSTATS_TIME           60

/*
 * number of seconds to wait after server starts up, before
 * starting try_connections()
 * TOO SOON and you can nick collide like crazy. 
 */
#define STARTUP_CONNECTIONS_TIME 60

struct Client;
struct server_conf;
struct Channel;

/* Capabilities */
struct Capability
{
	const char *name;	/* name of capability */
	unsigned int cap;	/* mask value */
};

#define CAP_CAP         0x00001	/* received a CAP to begin with */
#define CAP_QS          0x00002	/* Can handle quit storm removal */
#define CAP_EX          0x00004	/* Can do channel +e exemptions */
#define CAP_CHW         0x00008	/* Can do channel wall @# */
#define CAP_IE          0x00010	/* Can do invite exceptions */
#define CAP_KLN	        0x00040	/* Can do KLINE message */
#define CAP_GLN	        0x00080	/* Can do GLINE message */
#define CAP_ZIP         0x00100	/* Can do ZIPlinks */
#define CAP_KNOCK	0x00400	/* supports KNOCK */
#define CAP_TB		0x00800	/* supports TBURST */
#define CAP_UNKLN       0x01000	/* supports remote unkline */
#define CAP_CLUSTER     0x02000	/* supports cluster stuff */
#define CAP_ENCAP	0x04000	/* supports ENCAP */
#define CAP_TS6		0x08000 /* supports TS6 or above */
#define CAP_SERVICE	0x10000
#define CAP_RSFNC	0x20000 /* rserv FNC */
#define CAP_SAVE	0x40000 /* supports SAVE (nick collision FNC) */
#define CAP_EUID	0x80000 /* supports EUID (ext UID + nonencap CHGHOST) */

#define CAP_MASK        (CAP_QS  | CAP_EX   | CAP_CHW  | \
                         CAP_IE  | CAP_KLN  | CAP_SERVICE |\
                         CAP_GLN | CAP_CLUSTER | CAP_ENCAP | \
                         CAP_ZIP  | CAP_KNOCK  | CAP_UNKLN | \
			 CAP_RSFNC | CAP_SAVE | CAP_EUID)

#ifdef HAVE_LIBZ
#define CAP_ZIP_SUPPORTED       CAP_ZIP
#else
#define CAP_ZIP_SUPPORTED       0
#endif

/*
 * Capability macros.
 */
#define IsCapable(x, cap)       (((x)->localClient->caps & (cap)) == cap)
#define NotCapable(x, cap)	(((x)->localClient->caps & (cap)) == 0)
#define ClearCap(x, cap)        ((x)->localClient->caps &= ~(cap))

#define SLINKCMD_SET_ZIP_OUT_LEVEL           1	/* data */
#define SLINKCMD_START_ZIP_OUT               2
#define SLINKCMD_START_ZIP_IN                3
#define SLINKCMD_INJECT_RECVQ                4	/* data */
#define SLINKCMD_INJECT_SENDQ                5	/* data */
#define SLINKCMD_INIT                        6
#define SLINKCMD_ZIPSTATS                    7

#ifndef HAVE_SOCKETPAIR
#define LAST_SLINK_FD   7
#else
#define LAST_SLINK_FD   5
#endif

#define SLINKRPL_FLAG_DATA      0x0001	/* reply has data following */
#define SLINKRPL_ERROR          1
#define SLINKRPL_ZIPSTATS       2

#define MAX_SLINKRPL            2

typedef void SlinkRplHnd(unsigned int replyid, unsigned int datalen,
			 unsigned char *data, struct Client *client_p);
struct SlinkRplDef
{
	unsigned int replyid;
	SlinkRplHnd *handler;
	unsigned int flags;
};

extern struct SlinkRplDef slinkrpltab[];

/*
 * Globals
 *
 *
 * list of recognized server capabilities.  "TS" is not on the list
 * because all servers that we talk to already do TS, and the kludged
 * extra argument to "PASS" takes care of checking that.  -orabidoo
 */
extern struct Capability captab[];

extern int MaxClientCount;	/* GLOBAL - highest number of clients */
extern int MaxConnectionCount;	/* GLOBAL - highest number of connections */

extern int refresh_user_links;

/*
 * return values for hunt_server() 
 */
#define HUNTED_NOSUCH   (-1)	/* if the hunted server is not found */
#define HUNTED_ISME     0	/* if this server should execute the command */
#define HUNTED_PASS     1	/* if message passed onwards successfully */


extern int hunt_server(struct Client *client_pt,
		       struct Client *source_pt,
		       const char *command, int server, int parc, const char **parv);
extern void send_capabilities(struct Client *, int);
extern const char *show_capabilities(struct Client *client);
extern void try_connections(void *unused);
extern void start_collect_zipstats(void);
extern void collect_zipstats(void *unused);

extern int check_server(const char *name, struct Client *server);
extern int server_estab(struct Client *client_p);

extern int serv_connect(struct server_conf *, struct Client *);

#endif /* INCLUDED_s_serv_h */
