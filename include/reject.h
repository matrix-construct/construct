/*
 *  ircd-ratbox: A slightly useful ircd
 *  reject.h: header to a file which rejects users with prejudice
 *
 *  Copyright (C) 2003 Aaron Sethman <androsyn@ratbox.org>
 *  Copyright (C) 2003-2005 ircd-ratbox development team
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 *
 *  $Id: reject.h 25056 2008-02-06 20:47:40Z androsyn $
 */
#ifndef INCLUDED_reject_h
#define INCLUDED_reject_h

/* amount of time to delay a rejected clients exit */
#define DELAYED_EXIT_TIME	10

void init_reject(void);
int check_reject(rb_fde_t *F, struct sockaddr *addr);
void add_reject(struct Client *);
void flush_reject(void);
int remove_reject(const char *ip);
struct ConfItem *find_dline(struct sockaddr *addr);
struct ConfItem *find_dline_exact(struct sockaddr *addr, unsigned int bitlen);
void remove_dline(struct ConfItem *aconf);
int add_dline(struct ConfItem *aconf);
int add_eline(struct ConfItem *aconf);
void report_dlines(struct Client *);
void report_tdlines(struct Client *);
void report_elines(struct Client *);
unsigned long delay_exit_length(void);

int throttle_add(struct sockaddr *addr);

int inc_global_cidr_count(struct Client *client_p);
void dec_global_cidr_count(struct Client *client_p);
int check_global_cidr_count(struct Client *client_p);
void rehash_global_cidr_tree(void);


#endif

