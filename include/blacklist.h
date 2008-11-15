/*
 *  charybdis: A slightly useful ircd.
 *  blacklist.h: Manages DNS blacklist entries and lookups
 *
 *  Copyright (C) 2006 charybdis development team
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
 *  $Id: blacklist.h 2023 2006-09-02 23:47:27Z jilles $
 */

#ifndef _BLACKLIST_H_
#define _BLACKLIST_H_

/* A configured DNSBL */
struct Blacklist {
	unsigned int status;	/* If CONF_ILLEGAL, delete when no clients */
	int refcount;
	char host[IRCD_RES_HOSTLEN + 1];
	char reject_reason[IRCD_BUFSIZE];
	unsigned int hits;
	time_t lastwarning;
};

/* A lookup in progress for a particular DNSBL for a particular client */
struct BlacklistClient {
	struct Blacklist *blacklist;
	struct Client *client_p;
	struct DNSQuery dns_query;
	rb_dlink_node node;
};

/* public interfaces */
struct Blacklist *new_blacklist(char *host, char *reject_entry);
void lookup_blacklists(struct Client *client_p);
void abort_blacklist_queries(struct Client *client_p);
void unref_blacklist(struct Blacklist *blptr);
void destroy_blacklists(void);

extern rb_dlink_list blacklist_list;

#endif
