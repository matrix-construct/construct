/*
 *  ircd-ratbox: A slightly useful ircd.
 *  class.h: The ircd class management header.
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
 *  $Id: class.h 6 2005-09-10 01:02:21Z nenolod $
 */

#ifndef INCLUDED_class_h
#define INCLUDED_class_h

struct ConfItem;
struct Client;
struct _patricia_tree_t;

struct Class
{
	struct Class *next;
	char *class_name;
	int max_total;
	int max_local;
	int max_global;
	int max_ident;
	int max_sendq;
	int con_freq;
	int ping_freq;
	int total;
	rb_patricia_tree_t *ip_limits;
	int cidr_ipv4_bitlen;
	int cidr_ipv6_bitlen;
	int cidr_amount;

};

extern rb_dlink_list class_list;
extern struct Class *default_class;

#define ClassName(x)	((x)->class_name)
#define ConFreq(x)      ((x)->con_freq)
#define MaxLocal(x)	((x)->max_local)
#define MaxGlobal(x)	((x)->max_global)
#define MaxIdent(x)	((x)->max_ident)
#define MaxUsers(x)	((x)->max_total)
#define PingFreq(x)     ((x)->ping_freq)
#define MaxSendq(x)     ((x)->max_sendq)
#define CurrUsers(x)    ((x)->total)
#define IpLimits(x)     ((x)->ip_limits)
#define CidrIpv4Bitlen(x)   ((x)->cidr_ipv4_bitlen)
#define CidrIpv6Bitlen(x)   ((x)->cidr_ipv6_bitlen)
#define CidrAmount(x)	((x)->cidr_amount)
#define ClassPtr(x)      ((x)->c_class)

#define ConfClassName(x) (ClassPtr(x)->class_name)
#define ConfConFreq(x)   (ClassPtr(x)->con_freq)
#define ConfMaxLocal(x)  (ClassPtr(x)->max_local)
#define ConfMaxGlobal(x) (ClassPtr(x)->max_global)
#define ConfMaxIdent(x)  (ClassPtr(x)->max_ident)
#define ConfMaxUsers(x)  (ClassPtr(x)->max_total)
#define ConfPingFreq(x)  (ClassPtr(x)->ping_freq)
#define ConfMaxSendq(x)  (ClassPtr(x)->max_sendq)
#define ConfCurrUsers(x) (ClassPtr(x)->total)
#define ConfIpLimits(x) (ClassPtr(x)->ip_limits)
#define ConfCidrAmount(x) (ClassPtr(x)->cidr_amount)
#define ConfCidrIpv4Bitlen(x) (ClassPtr(x)->cidr_ipv4_bitlen)
#define ConfCidrIpv6Bitlen(x) (ClassPtr(x)->cidr_ipv6_bitlen)

void add_class(struct Class *);

struct Class *make_class(void);

extern long get_sendq(struct Client *);
extern int get_con_freq(struct Class *);
extern struct Class *find_class(const char *);
extern const char *get_client_class(struct Client *);
extern int get_client_ping(struct Client *);
extern void check_class(void);
extern void initclass(void);
extern void free_class(struct Class *);
extern void fix_class(struct ConfItem *, struct ConfItem *);
extern void report_classes(struct Client *);

#endif /* INCLUDED_class_h */
