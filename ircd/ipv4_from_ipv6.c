/*
 * charybdis: A slightly useful ircd.
 * ipv4_from_ipv6.c: Finds IPv4 addresses behind IPv6 transition technologies
 *
 * Copyright (C) 2012 Jilles Tjoelker
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 */

#include "stdinc.h"
#include "ipv4_from_ipv6.h"

#ifdef RB_IPV6
int
ipv4_from_ipv6(const struct sockaddr_in6 *restrict ip6,
		struct sockaddr_in *restrict ip4)
{
	int i;

	if (!memcmp(ip6->sin6_addr.s6_addr, "\x20\x02", 2))
	{
		/* 6to4 and similar */
		memcpy(&ip4->sin_addr, ip6->sin6_addr.s6_addr + 2, 4);
	}
	else if (!memcmp(ip6->sin6_addr.s6_addr, "\x20\x01\x00\x00", 4))
	{
		/* Teredo */
		for (i = 0; i < 4; i++)
			((uint8_t *)&ip4->sin_addr)[i] = 0xFF ^
				ip6->sin6_addr.s6_addr[12 + i];
	}
	else
		return 0;
	SET_SS_LEN(ip4, sizeof(struct sockaddr_in));
	ip4->sin_family = AF_INET;
	ip4->sin_port = 0;
	return 1;
}
#endif /* RB_IPV6 */
