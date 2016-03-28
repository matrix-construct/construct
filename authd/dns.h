/* authd/dns.h - header for authd DNS functions
 * Copyright (c) 2016 William Pitcock <nenolod@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _AUTHD_DNS_H
#define _AUTHD_DNS_H

#define DNS_REQ_IDLEN 10

#include "stdinc.h"
#include "res.h"
#include "reslib.h"

typedef enum
{
	QUERY_INVALID = 0,
	QUERY_A = '4',
	QUERY_AAAA = '6',
	QUERY_PTR_A = 'R',
	QUERY_PTR_AAAA = 'S',
} query_type;

/* Similar to that in ircd */
typedef void (*DNSCB)(const char *res, bool status, query_type type, void *data);

struct dns_query
{
	struct DNSQuery query;
	query_type type;
	struct rb_sockaddr_storage addr;
	uint64_t id;

	DNSCB callback;
	void *data;
};

extern struct dns_query *lookup_hostname(const char *ip, DNSCB callback, void *data);
extern struct dns_query *lookup_ip(const char *host, int aftype, DNSCB callback, void *data);
extern void cancel_query(struct dns_query *query);

extern void handle_resolve_dns(int parc, char *parv[]);
extern void enumerate_nameservers(uint32_t rid, const char letter);
extern void reload_nameservers(const char letter);

#endif
