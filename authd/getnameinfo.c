/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Issues to be discussed:
 * - Thread safe-ness must be checked
 * - RFC2553 says that we should raise error on short buffer.  X/Open says
 *   we need to truncate the result.  We obey RFC2553 (and X/Open should be
 *   modified).	 ipngwg rough consensus seems to follow RFC2553.
 * - What is "local" in NI_FQDN?
 * - NI_NAMEREQD and NI_NUMERICHOST conflict with each other.
 * - (KAME extension) always attach textual scopeid (fe80::1%lo0), if
 *   sin6_scope_id is filled - standardization status?
 *   XXX breaks backward compat for code that expects no scopeid.
 *   beware on merge.
 */

#ifdef _WIN32
#include <rb_lib.h>
#include "getaddrinfo.h"
#include "getnameinfo.h"

static const struct afd {
  int a_af;
  int a_addrlen;
  rb_socklen_t a_socklen;
  int a_off;
} afdl [] = {
#ifdef IPV6
    {PF_INET6, sizeof(struct in6_addr), sizeof(struct sockaddr_in6),
	     offsetof(struct sockaddr_in6, sin6_addr)},
#endif
    {PF_INET, sizeof(struct in_addr), sizeof(struct sockaddr_in),
	    offsetof(struct sockaddr_in, sin_addr)},
    {0, 0, 0, 0},
};

struct sockinet
{
  unsigned char si_len;
  unsigned char si_family;
  unsigned short si_port;
};

#ifdef IPV6
static int ip6_parsenumeric(const struct sockaddr *, const char *, char *,
    size_t, int);
#endif

int
rb_getnameinfo(const struct sockaddr *sa, rb_socklen_t salen, char *host,
		size_t hostlen, char *serv, size_t servlen, int flags)
{
  const struct afd *afd;
  struct servent *sp;
  unsigned short port;
  int family, i;
  const char *addr;
  uint32_t v4a;
  char numserv[512];
  char numaddr[512];

  if (sa == NULL)
    return EAI_FAIL;

/*	if (sa->sa_len != salen)
		return EAI_FAIL;
*/
	family = sa->sa_family;
	for (i = 0; afdl[i].a_af; i++)
		if (afdl[i].a_af == family) {
			afd = &afdl[i];
			goto found;
		}
	return EAI_FAMILY;

 found:
	if (salen != afd->a_socklen)
		return EAI_FAIL;

	/* network byte order */
	port = ((const struct sockinet *)sa)->si_port;
	addr = (const char *)sa + afd->a_off;

	if (serv == NULL || servlen == 0) {
		/*
		 * do nothing in this case.
		 * in case you are wondering if "&&" is more correct than
		 * "||" here: rfc2553bis-03 says that serv == NULL OR
		 * servlen == 0 means that the caller does not want the result.
		 */
	} else {
		if (flags & NI_NUMERICSERV)
			sp = NULL;
		else {
			sp = getservbyport(port,
				(flags & NI_DGRAM) ? "udp" : "tcp");
		}
		if (sp) {
			if (strlen(sp->s_name) + 1 > servlen)
				return EAI_MEMORY;
			rb_strlcpy(serv, sp->s_name, servlen);
		} else {
			snprintf(numserv, sizeof(numserv), "%u", ntohs(port));
			if (strlen(numserv) + 1 > servlen)
				return EAI_MEMORY;
			rb_strlcpy(serv, numserv, servlen);
		}
	}

	switch (sa->sa_family) {
	case AF_INET:
		v4a = (uint32_t)
		    ntohl(((const struct sockaddr_in *)sa)->sin_addr.s_addr);
		if (IN_MULTICAST(v4a) || IN_EXPERIMENTAL(v4a))
			flags |= NI_NUMERICHOST;
		v4a >>= IN_CLASSA_NSHIFT;
		if (v4a == 0)
			flags |= NI_NUMERICHOST;
		break;
#ifdef IPV6
	case AF_INET6:
	    {
		const struct sockaddr_in6 *sin6;
		sin6 = (const struct sockaddr_in6 *)sa;
		switch (sin6->sin6_addr.s6_addr[0]) {
		case 0x00:
			if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
				;
			else if (IN6_IS_ADDR_LOOPBACK(&sin6->sin6_addr))
				;
			else
				flags |= NI_NUMERICHOST;
			break;
		default:
			if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
				flags |= NI_NUMERICHOST;
			}
			else if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr))
				flags |= NI_NUMERICHOST;
			break;
		}
	    }
		break;
#endif
	}
	if (host == NULL || hostlen == 0) {
		/*
		 * do nothing in this case.
		 * in case you are wondering if "&&" is more correct than
		 * "||" here: rfc2553bis-03 says that host == NULL or
		 * hostlen == 0 means that the caller does not want the result.
		 */
	} else if (flags & NI_NUMERICHOST) {
		size_t numaddrlen;

		/* NUMERICHOST and NAMEREQD conflicts with each other */
		if (flags & NI_NAMEREQD)
			return EAI_NONAME;

		switch(afd->a_af) {
#ifdef IPV6
		case AF_INET6:
		{
			int error;

			if ((error = ip6_parsenumeric(sa, addr, host,
						      hostlen, flags)) != 0)
				return(error);
			break;
		}
#endif
		default:
			if (rb_inet_ntop(afd->a_af, addr, numaddr, sizeof(numaddr))
			    == NULL)
				return EAI_SYSTEM;
			numaddrlen = strlen(numaddr);
			if (numaddrlen + 1 > hostlen) /* don't forget terminator */
				return EAI_MEMORY;
			rb_strlcpy(host, numaddr, hostlen);
			break;
		}
	}
	return(0);
}

#ifdef IPV6
static int
ip6_parsenumeric(const struct sockaddr *sa, const char *addr,
		 char *host, size_t hostlen, int flags)
{
  size_t numaddrlen;
  char numaddr[512];

  if (rb_inet_ntop(AF_INET6, addr, numaddr, sizeof(numaddr)) == NULL)
    return(EAI_SYSTEM);

  numaddrlen = strlen(numaddr);

  if (numaddrlen + 1 > hostlen) /* don't forget terminator */
    return(EAI_MEMORY);

  if (*numaddr == ':')
  {
    *host = '0';
    rb_strlcpy(host+1, numaddr, hostlen-1);
  }
  else
    rb_strlcpy(host, numaddr, hostlen);

  return(0);
}
#endif
#endif
