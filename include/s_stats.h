/*
 *  ircd-ratbox: A slightly useful ircd.
 *  s_stats.h: A header for the statistics functions.
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
 *  $Id: s_stats.h 1409 2006-05-21 14:46:17Z jilles $
 */

#ifndef INCLUDED_s_stats_h
#define INCLUDED_s_stats_h

#define _1MEG     (1024.0)
#define _1GIG     (1024.0*1024.0)
#define _1TER     (1024.0*1024.0*1024.0)
#define _GMKs(x)  ( (x > _1TER) ? "Terabytes" : ((x > _1GIG) ? "Gigabytes" : \
                  ((x > _1MEG) ? "Megabytes" : "Kilobytes")))
#define _GMKv(x)  ( (x > _1TER) ? (float)(x/_1TER) : ((x > _1GIG) ? \
                  (float)(x/_1GIG) : ((x > _1MEG) ? (float)(x/_1MEG) : (float)x)))

struct Client;

/*
 * statistics structures
 */
struct ServerStatistics
{
	unsigned int is_cl;	/* number of client connections */
	unsigned int is_sv;	/* number of server connections */
	unsigned int is_ni;	/* connection but no idea who it was */
	unsigned long long int is_cbs;	/* bytes sent to clients */
	unsigned long long int is_cbr;	/* bytes received to clients */
	unsigned long long int is_sbs;	/* bytes sent to servers */
	unsigned long long int is_sbr;	/* bytes received to servers */
	unsigned long long int is_cti;	/* time spent connected by clients */
	unsigned long long int is_sti;	/* time spent connected by servers */
	unsigned int is_ac;	/* connections accepted */
	unsigned int is_ref;	/* accepts refused */
	unsigned int is_unco;	/* unknown commands */
	unsigned int is_wrdi;	/* command going in wrong direction */
	unsigned int is_unpf;	/* unknown prefix */
	unsigned int is_empt;	/* empty message */
	unsigned int is_num;	/* numeric message */
	unsigned int is_kill;	/* number of kills generated on collisions */
	unsigned int is_save;	/* number of saves generated on collisions */
	unsigned int is_asuc;	/* successful auth requests */
	unsigned int is_abad;	/* bad auth requests */
	unsigned int is_rej;	/* rejected from cache */
	unsigned int is_thr;	/* number of throttled connections */
	unsigned int is_ssuc;	/* successful sasl authentications */
	unsigned int is_sbad;	/* failed sasl authentications */
	unsigned int is_tgch;	/* messages blocked due to target change */
};

extern struct ServerStatistics ServerStats;

#endif /* INCLUDED_s_stats_h */
