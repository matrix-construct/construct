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

#include "authd.h"
#include "dns.h"
#include "res.h"

void
format_address(struct rb_sockaddr_storage *addr, char *buffer, size_t length)
{
	if(GET_SS_FAMILY(addr) == AF_INET)
	{
		rb_inet_ntop_sock((struct sockaddr *)addr, buffer, length);
	}
#ifdef RB_IPV6
	else if(GET_SS_FAMILY(addr) == AF_INET6)
	{
		char tmpbuf[length];

		rb_inet_ntop_sock((struct sockaddr *)addr, tmpbuf, length);

		if(*tmpbuf == ':')
		{
			rb_strlcpy(buffer, "0", length);
			rb_strlcat(buffer, tmpbuf, length);
		}
		else
			rb_strlcpy(buffer, tmpbuf, length);
	}
#endif
}

bool
sockcmp(struct rb_sockaddr_storage *addr, struct rb_sockaddr_storage *addr2, int family)
{
	if(family == AF_INET)
	{
		struct sockaddr_in *ip, *ip2;
		ip = (struct sockaddr_in *)addr;
		ip2 = (struct sockaddr_in *)addr2;

		return ip->sin_addr.s_addr == ip2->sin_addr.s_addr;
	}
#ifdef RB_IPV6
	else if(family == AF_INET6)
	{
		struct sockaddr_in6 *ip, *ip2;
		ip = (struct sockaddr_in6 *) addr;
		ip2 = (struct sockaddr_in6 *) addr2;

		return(memcmp(&ip->sin6_addr, &ip2->sin6_addr, sizeof(struct in6_addr)) == 0);
	}
#endif

	return false;
}

static void
submit_dns_answer(void *userdata, struct DNSReply *reply)
{
	struct dns_request *req = userdata;
	char response[64] = "*";
	char status = 'E';
	int family = AF_INET;

	if (reply == NULL)
	{
		rb_helper_write(authd_helper, "E %s E %c *", req->reqid, req->type);
		goto cleanup;
	}

	switch (req->type)
	{
	case '4':
#ifdef RB_IPV6
	case '6':
		format_address(&reply->addr, response, sizeof(response));
		break;
#endif
#ifdef RB_IPV6
	case 'S':
		family = AF_INET6;
#endif
	case 'R':
		if(sockcmp(&req->addr, &reply->addr, family) && strlen(reply->h_name) < 63)
		{
			rb_strlcpy(response, reply->h_name, sizeof(response));
			status = 'O';
		}
		break;
	default:
		exit(7);
	}

	rb_helper_write(authd_helper, "E %s %c %c %s", req->reqid, status, req->type, response);
cleanup:
	rb_free(req);
}

void
resolve_dns(int parc, char *parv[])
{
	struct dns_request *req;
	char *requestid = parv[1];
	char *qtype = parv[2];
	char *rec = parv[3];
	int type;

	req = rb_malloc(sizeof(*req));
	rb_strlcpy(req->reqid, requestid, sizeof(req->reqid));
	req->type = *qtype;

	switch (req->type)
	{
	case '4':
		type = T_A;
		break;
	case '6':
		type = T_AAAA;
		break;
	case 'R':
	case 'S':
		if(!rb_inet_pton_sock(rec, (struct sockaddr *) &req->addr))
			exit(6);
		type = T_PTR;
		break;
	}

	req->query.ptr = req;
	req->query.callback = submit_dns_answer;

	if (type != T_PTR)
		gethost_byname_type(rec, &req->query, type);
	else
		gethost_byaddr(&req->addr, &req->query);
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
