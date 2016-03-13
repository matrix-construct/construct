/* authd/dns.c - authd DNS functions
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

#include "authd.h"
#include "dns.h"
#include "res.h"

static void handle_lookup_ip_reply(void *data, struct DNSReply *reply);
static void handle_lookup_hostname_reply(void *data, struct DNSReply *reply);

uint64_t query_count = 0;

/* A bit different from ircd... you just get a dns_query object.
 *
 * It gets freed whenever the res code gets back to us.
 */
struct dns_query *
lookup_ip(const char *host, int aftype, DNSCB callback, void *data)
{
	struct dns_query *query = rb_malloc(sizeof(struct dns_query));
	int g_type;

	if(aftype == AF_INET)
	{
		query->type = QUERY_A;
		g_type = T_A;
	}
#ifdef RB_IPV6
	else if(aftype == AF_INET6)
	{
		query->type = QUERY_AAAA;
		g_type = T_AAAA;
	}
#endif
	else
	{
		rb_free(query);
		return NULL;
	}

	query->id = query_count++;
	query->callback = callback;
	query->data = data;

	query->query.ptr = query;
	query->query.callback = handle_lookup_ip_reply;

	gethost_byname_type(host, &query->query, g_type);

	return query;
}

/* See lookup_ip's comment */
struct dns_query *
lookup_hostname(const char *ip, int aftype, DNSCB callback, void *data)
{
	struct dns_query *query = rb_malloc(sizeof(struct dns_query));

	if(!rb_inet_pton_sock(ip, (struct sockaddr *)&query->addr))
	{
		rb_free(query);
		return NULL;
	}

	if(aftype == AF_INET)
		query->type = QUERY_PTR_A;
#ifdef RB_IPV6
	else if(aftype == AF_INET6)
		query->type = QUERY_PTR_AAAA;
#endif
	else
	{
		rb_free(query);
		return NULL;
	}

	query->id = query_count++;
	query->callback = callback;
	query->data = data;

	query->query.ptr = query;
	query->query.callback = handle_lookup_hostname_reply;

	gethost_byaddr(&query->addr, &query->query);

	return query;
}

/* Cancel a pending query */
void
cancel_query(struct dns_query *query)
{
	query->callback = query->data = NULL;
}

/* Callback from gethost_byname_type */
static void
handle_lookup_ip_reply(void *data, struct DNSReply *reply)
{
	struct dns_query *query = data;
	char ip[64] = "*";
	query_type type = QUERY_INVALID;

	if(!query)
		/* Shouldn't happen */
		exit(2);
	
	type = query->type;

	if(!reply)
		goto end;

	switch(query->type)
	{
	case QUERY_A:
		if(GET_SS_FAMILY(&reply->addr) == AF_INET)
			rb_inet_ntop_sock((struct sockaddr *)&reply->addr, ip, sizeof(ip));
		break;
#ifdef RB_IPV6
	case QUERY_AAAA:
		if(GET_SS_FAMILY(&reply->addr) == AF_INET6)
		{
			rb_inet_ntop_sock((struct sockaddr *)&reply->addr, ip, sizeof(ip));
			if(ip[0] == ':')
			{
				memmove(&ip[1], ip, strlen(ip));
				ip[0] = '0';
			}
		}
		break;
#endif
	default:
		exit(3);
	}

end:
	if(query->callback)
		query->callback(ip, ip[0] != '*', type, query->data);

	rb_free(query);
}

/* Callback from gethost_byaddr */
static void
handle_lookup_hostname_reply(void *data, struct DNSReply *reply)
{
	struct dns_query *query = data;
	char *hostname = NULL;
	query_type type = QUERY_INVALID;

	if(!query)
		/* Shouldn't happen */
		exit(4);

	type = query->type;

	if(!reply)
		goto end;

	if(query->type == QUERY_PTR_A)
	{
		struct sockaddr_in *ip, *ip_fwd;
		ip = (struct sockaddr_in *) &query->addr;
		ip_fwd = (struct sockaddr_in *) &reply->addr;

		if(ip->sin_addr.s_addr == ip_fwd->sin_addr.s_addr && strlen(reply->h_name) < 63)
			hostname = reply->h_name;
	}
#ifdef RB_IPV6
	else if(query->type == QUERY_PTR_AAAA)
	{
		struct sockaddr_in6 *ip, *ip_fwd;
		ip = (struct sockaddr_in6 *) &query->addr;
		ip_fwd = (struct sockaddr_in6 *) &reply->addr;

		if(memcmp(&ip->sin6_addr, &ip_fwd->sin6_addr, sizeof(struct in6_addr)) == 0 && strlen(reply->h_name) < 63)
			hostname = reply->h_name;
	}
#endif
	else
		/* Shouldn't happen */
		exit(5);
end:
	if(query->callback)
		query->callback(hostname, hostname != NULL, query->type, query->data);

	rb_free(query);
}

static void
submit_dns_answer(const char *reply, bool status, query_type type, void *data)
{
	char *id = data;

	if(!id || type == QUERY_INVALID)
		exit(6);

	if(!reply || !status)
	{
		rb_helper_write(authd_helper, "E %s E %c *", id, type);
		rb_free(id);
		return;
	}

	rb_helper_write(authd_helper, "E %s O %c %s", id, type, reply);
	rb_free(id);
}

void
resolve_dns(int parc, char *parv[])
{
	char *id = rb_strdup(parv[1]);
	char qtype = *parv[2];
	char *record = parv[3];
	int aftype = AF_INET;

	switch(qtype)
	{
#ifdef RB_IPV6
	case '6':
		aftype = AF_INET6;
#endif
	case '4':
		if(!lookup_ip(record, aftype, submit_dns_answer, id))
			submit_dns_answer(NULL, false, qtype, NULL);
		break;
#ifdef RB_IPV6
	case 'S':
		aftype = AF_INET6;
#endif
	case 'R':
		if(!lookup_hostname(record, aftype, submit_dns_answer, id))
			submit_dns_answer(NULL, false, qtype, NULL);
		break;
	default:
		exit(7);
	}
}

void
enumerate_nameservers(const char *rid, const char letter)
{
	char buf[40 * IRCD_MAXNS]; /* Plenty */
	char *c = buf;
	int i;

	if (!irc_nscount)
	{
		/* Shouldn't happen */
		rb_helper_write(authd_helper, "X %s %c NONAMESERVERS", rid, letter);
		return;
	}

	for(i = 0; i < irc_nscount; i++)
	{
		char addr[40];
		int ret;

		rb_inet_ntop_sock((struct sockaddr *)&irc_nsaddr_list[i], addr, sizeof(addr));

		if (!addr[0])
		{
			/* Shouldn't happen */
			rb_helper_write(authd_helper, "X %s %c INVALIDNAMESERVER", rid, letter);
			return;
		}

		ret = snprintf(c, 40, "%s ", addr);
		c += (size_t)ret;
	}

	*(--c) = '\0';

	rb_helper_write(authd_helper, "Y %s %c %s", rid, letter, buf);
}

void
reload_nameservers(const char letter)
{
	/* Not a whole lot to it */
	restart_resolver();
}
