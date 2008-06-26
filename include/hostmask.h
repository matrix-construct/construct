/*
 *  charybdis: an advanced Internet Relay Chat Daemon(ircd).
 *  hostmask.h: A header for the hostmask code.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
 *  Copyright (C) 2005-2006 charybdis development team
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
 *  $Id: hostmask.h 2757 2006-11-10 22:58:15Z jilles $
 */

#ifndef INCLUDE_hostmask_h
#define INCLUDE_hostmask_h 1
enum
{
	HM_HOST,
	HM_IPV4
#ifdef RB_IPV6
		, HM_IPV6
#endif
};

int parse_netmask(const char *, struct sockaddr *, int *);
struct ConfItem *find_conf_by_address(const char *host, const char *sockhost,
				      const char *orighost, struct sockaddr *,
				      int, int, const char *, const char *);
struct ConfItem *find_exact_conf_by_address(const char *address, int type,
					    const char *username);
void add_conf_by_address(const char *, int, const char *, const char *, struct ConfItem *);
void delete_one_address_conf(const char *, struct ConfItem *);
void clear_out_address_conf(void);
void clear_out_address_conf_bans(void);
void init_host_hash(void);
struct ConfItem *find_address_conf(const char *host, const char *sockhost, 
				const char *, const char *, struct sockaddr *,
				int, char *);

struct ConfItem *find_dline(struct sockaddr *, int);

#define find_kline(x)	(find_conf_by_address((x)->host, (x)->sockhost, \
			 (x)->orighost, \
			 (struct sockaddr *)&(x)->localClient->ip, CONF_KILL,\
			 (x)->localClient->ip.ss_family, (x)->username, NULL))

void report_Klines(struct Client *);
void report_auth(struct Client *);
#ifdef RB_IPV6
int match_ipv6(struct sockaddr *, struct sockaddr *, int);
#endif
int match_ipv4(struct sockaddr *, struct sockaddr *, int);

/* Hashtable stuff... */
#define ATABLE_SIZE 0x1000 /* 2^12 */
#define ATABLE_BITS (32-12)

extern struct AddressRec *atable[ATABLE_SIZE];

struct AddressRec
{
	/* masktype: HM_HOST, HM_IPV4, HM_IPV6 -A1kmm */
	int masktype;

	union
	{
		struct
		{
			/* Pointer into ConfItem... -A1kmm */
			struct rb_sockaddr_storage addr;
			int bits;
		}
		ipa;

		/* Pointer into ConfItem... -A1kmm */
		const char *hostname;
	}
	Mask;

	/* type: CONF_CLIENT, CONF_DLINE, CONF_KILL etc... -A1kmm */
	int type;

	/* Higher precedences overrule lower ones... */
	unsigned long precedence;

	/* Only checked if !(type & 1)... */
	const char *username;
	/* Only checked if type == CONF_CLIENT */
	const char *auth_user;
	struct ConfItem *aconf;

	/* The next record in this hash bucket. */
	struct AddressRec *next;
};


#endif /* INCLUDE_hostmask_h */
