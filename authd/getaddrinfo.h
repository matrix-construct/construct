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
 *
 */

struct rb_addrinfo {
	int ai_flags;
	int ai_family;
	int ai_socktype;
	int ai_protocol;
	size_t ai_addrlen;
	char *ai_canonname;
	struct sockaddr *ai_addr;
	struct rb_addrinfo *ai_next;
};

#ifndef AI_PASSIVE
#define AI_PASSIVE      0x00000001 /* get address to use bind() */
#endif /* AI_PASSIVE */

#ifndef AI_NUMERICHOST
#define AI_NUMERICHOST  0x00000004 /* prevent name resolution */
#endif /* AI_NUMERICHOST */

#ifndef EAI_FAIL
#define EAI_FAIL         4      /* non-recoverable failure in name resolution */
#endif /* EAI_FAIL */

#ifndef EAI_FAMILY
#define EAI_FAMILY       5      /* ai_family not supported */
#endif /* EAI_FAMILY */

#ifndef EAI_MEMORY
#define EAI_MEMORY       6      /* memory allocation failure */
#endif /* EAI_MEMORY */

#ifndef EAI_NONAME
#define EAI_NONAME       8      /* hostname nor servname provided, or not known */
#endif /* EAI_NONAME */

#ifndef EAI_SYSTEM
#define EAI_SYSTEM      11      /* system error returned in errno */
#endif /* EAI_SYSTEM */

#ifndef NI_NUMERICHOST
#define NI_NUMERICHOST	0x00000002
#endif /* NI_NUMERICHOST */

#ifndef NI_NAMEREQD
#define NI_NAMEREQD     0x00000004
#endif /* NI_NAMEREQD */

#ifndef NI_NUMERICSERV
#define NI_NUMERICSERV  0x00000008
#endif /* NI_NUMERICSERV */

#ifndef NI_DGRAM
#define NI_DGRAM        0x00000010
#endif /* NI_DGRAM */

int rb_getaddrinfo(const char *hostname, const char *servname,
                           const struct rb_addrinfo *hints, struct rb_addrinfo **res);
void rb_freeaddrinfo(struct rb_addrinfo *ai);

#define SUCCESS 0
#define ANY 0

#undef EAI_ADDRFAMILY
#undef EAI_AGAIN
#undef EAI_BADFLAGS
#undef EAI_FAIL
#undef EAI_FAMILY
#undef EAI_MEMORY
#undef EAI_NODATA
#undef EAI_NONAME
#undef EAI_SERVICE
#undef EAI_SOCKTYPE
#undef EAI_SYSTEM
#undef EAI_BADHINTS
#undef EAI_PROTOCOL
#undef EAI_MAX
#undef AI_MASK

#define EAI_ADDRFAMILY   1  /* address family for hostname not supported */
#define EAI_AGAIN    2  /* temporary failure in name resolution */
#define EAI_BADFLAGS     3  /* invalid value for ai_flags */
#define EAI_FAIL     4  /* non-recoverable failure in name resolution */
#define EAI_FAMILY   5  /* ai_family not supported */
#define EAI_MEMORY   6  /* memory allocation failure */
#define EAI_NODATA   7  /* no address associated with hostname */
#define EAI_NONAME   8  /* hostname nor servname provided, or not known */
#define EAI_SERVICE  9  /* servname not supported for ai_socktype */
#define EAI_SOCKTYPE    10  /* ai_socktype not supported */
#define EAI_SYSTEM  11  /* system error returned in errno */
#define EAI_BADHINTS    12
#define EAI_PROTOCOL    13
#define EAI_MAX     14
#define AI_MASK (AI_PASSIVE | AI_NUMERICHOST)
