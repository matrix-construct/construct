/*
 *  ircd-ratbox: A slightly useful ircd.
 *  send.h: A header for the message sending functions.
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
 */

#pragma once
#define HAVE_IRCD_SEND_H

#ifdef __cplusplus
namespace ircd {

struct monitor;

/* The nasty global also used in s_serv.c for server bursts */
extern unsigned long current_serial;

extern client::client *remote_rehash_oper_p;

extern void send_pop_queue(client::client *);

extern void send_queued(client::client *to);

extern void sendto_one(client::client *target_p, const char *, ...) AFP(2, 3);
extern void sendto_one_notice(client::client *target_p,const char *, ...) AFP(2, 3);
extern void sendto_one_prefix(client::client *target_p, client::client *source_p,
			      const char *command, const char *, ...) AFP(4, 5);
extern void sendto_one_numeric(client::client *target_p,
			       int numeric, const char *, ...) AFP(3, 4);

extern void sendto_server(client::client *one, chan::chan *chptr,
			  unsigned long caps, unsigned long nocaps,
			  const char *format, ...) AFP(5, 6);

extern void sendto_channel_flags(client::client *one, int type, client::client *source_p,
				 chan::chan *chptr, const char *, ...) AFP(5, 6);
extern void sendto_channel_opmod(client::client *one, client::client *source_p,
				 chan::chan *chptr, const char *command,
				 const char *text);

extern void sendto_channel_local(int type, chan::chan *, const char *, ...) AFP(3, 4);
extern void sendto_channel_local_butone(client::client *, int type, chan::chan *, const char *, ...) AFP(4, 5);

extern void sendto_channel_local_with_capability(int type, int caps, int negcaps, chan::chan *, const char *, ...) AFP(5, 6);
extern void sendto_channel_local_with_capability_butone(client::client *, int type, int caps, int negcaps, chan::chan *,
							const char *, ...) AFP(6, 7);

extern void sendto_common_channels_local(client::client *, int cap, int negcap, const char *, ...) AFP(4, 5);
extern void sendto_common_channels_local_butone(client::client *, int cap, int negcap, const char *, ...) AFP(4, 5);


extern void sendto_match_butone(client::client *, client::client *,
				const char *, int, const char *, ...) AFP(5, 6);
extern void sendto_match_servs(client::client *source_p, const char *mask,
				int capab, int, const char *, ...) AFP(5, 6);

extern void sendto_monitor(struct monitor *monptr, const char *, ...) AFP(2, 3);

extern void sendto_anywhere(client::client *, client::client *, const char *,
			    const char *, ...) AFP(4, 5);
extern void sendto_local_clients_with_capability(int cap, const char *pattern, ...) AFP(2, 3);

extern void sendto_realops_snomask(int, int, const char *, ...) AFP(3, 4);
extern void sendto_realops_snomask_from(int, int, client::client *, const char *, ...) AFP(4, 5);

extern void sendto_wallops_flags(int, client::client *, const char *, ...) AFP(3, 4);

extern void kill_client(client::client *client_p, client::client *diedie,
			 const char *pattern, ...) AFP(3, 4);
extern void kill_client_serv_butone(client::client *one, client::client *source_p,
				    const char *pattern, ...) AFP(3, 4);

#define L_ALL 	0
#define L_OPER 	1
#define L_ADMIN	2
#define L_NETWIDE 256 /* OR with L_ALL or L_OPER */

#define NOCAPS          0	/* no caps */

/* used when sending to #mask or $mask */
#define MATCH_SERVER  1
#define MATCH_HOST    2

}      // namespace ircd
#endif // __cplusplus
