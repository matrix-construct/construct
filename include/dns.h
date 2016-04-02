/*
 *  charybdis
 *  dns.h: A header with the DNS functions.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2012 ircd-ratbox development team
 *  Copyright (C) 2016 William Pitcock <nenolod@dereferenced.org>
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
 */

#ifndef CHARYBDIS_DNS_H
#define CHARYBDIS_DNS_H

#include "stdinc.h"
#include "authproc.h"

extern rb_dlink_list nameservers;

typedef void (*DNSCB)(const char *res, int status, int aftype, void *data);
typedef void (*DNSLISTCB)(int resc, const char *resv[], int status, void *data);

uint32_t lookup_hostname(const char *hostname, int aftype, DNSCB callback, void *data);
uint32_t lookup_ip(const char *hostname, int aftype, DNSCB callback, void *data);
void cancel_lookup(uint32_t xid);
void cancel_dns_stats(uint32_t xid);

void dns_results_callback(const char *callid, const char *status, const char *aftype, const char *results);
void dns_stats_results_callback(const char *callid, const char *status, int resc, const char *resv[]);

void init_dns(void);
void reload_nameservers(void);

#endif
