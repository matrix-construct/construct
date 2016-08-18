/************************************************************************
 *   IRC - Internet Relay Chat, src/match.c
 *   Copyright (C) 1990 Jarkko Oikarinen
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

namespace ircd {

/*
 * Compare if a given string (name) matches the given
 * mask (which can contain wild cards: '*' - match any
 * number of chars, '?' - match any single character.
 *
 * return  1, if match
 *         0, if no match
 *
 *  Originally by Douglas A Lewis (dalewis@acsu.buffalo.edu)
 *  Rewritten by Timothy Vogelsang (netski), net@astrolink.org
 */

/** Check a string against a mask.
 * This test checks using traditional IRC wildcards only: '*' means
 * match zero or more characters of any type; '?' means match exactly
 * one character of any type.
 *
 * @param[in] mask Wildcard-containing mask.
 * @param[in] name String to check against \a mask.
 * @return Zero if \a mask matches \a name, non-zero if no match.
 */
int match(const char *mask, const char *name)
{
	const char *m = mask, *n = name;
	const char *m_tmp = mask, *n_tmp = name;
	int star_p;

	s_assert(mask != NULL);
	s_assert(name != NULL);

	for (;;)
	{
		switch (*m)
		{
		  case '\0':
			  if (!*n)
				  return 1;
		  backtrack:
			  if (m_tmp == mask)
				  return 0;
			  m = m_tmp;
			  n = ++n_tmp;
			  break;
		  case '*':
		  case '?':
			  for (star_p = 0;; m++)
			  {
				  if (*m == '*')
					  star_p = 1;
				  else if (*m == '?')
				  {
					  if (!*n++)
						  goto backtrack;
				  }
				  else
					  break;
			  }
			  if (star_p)
			  {
				  if (!*m)
					  return 1;
				  else
				  {
					  m_tmp = m;
					  for (n_tmp = n; *n && rfc1459::tolower(*n) != rfc1459::tolower(*m); n++);
				  }
			  }
			  /* and fall through */
		  default:
			  if (!*n)
				  return (*m != '\0' ? 0 : 1);
			  if (rfc1459::tolower(*m) != rfc1459::tolower(*n))
				  goto backtrack;
			  m++;
			  n++;
			  break;
		}
	}
}

/** Check a mask against a mask.
 * This test checks using traditional IRC wildcards only: '*' means
 * match zero or more characters of any type; '?' means match exactly
 * one character of any type.
 * The difference between match_mask() and match() is that in match_mask()
 * a '?' in mask does not match a '*' in name.
 *
 * @param[in] mask Existing wildcard-containing mask.
 * @param[in] name New wildcard-containing mask.
 * @return 1 if \a name is equal to or more specific than \a mask, 0 otherwise.
 */
int match_mask(const char *mask, const char *name)
{
	const char *m = mask, *n = name;
	const char *m_tmp = mask, *n_tmp = name;
	int star_p;

	s_assert(mask != NULL);
	s_assert(name != NULL);

	for (;;)
	{
		switch (*m)
		{
		  case '\0':
			  if (!*n)
				  return 1;
		  backtrack:
			  if (m_tmp == mask)
				  return 0;
			  m = m_tmp;
			  n = ++n_tmp;
			  break;
		  case '*':
		  case '?':
			  for (star_p = 0;; m++)
			  {
				  if (*m == '*')
					  star_p = 1;
				  else if (*m == '?')
				  {
					  /* changed for match_mask() */
					  if (*n == '*' || !*n)
						  goto backtrack;
					  n++;
				  }
				  else
					  break;
			  }
			  if (star_p)
			  {
				  if (!*m)
					  return 1;
				  else
				  {
					  m_tmp = m;
					  for (n_tmp = n; *n && rfc1459::tolower(*n) != rfc1459::tolower(*m); n++);
				  }
			  }
			  /* and fall through */
		  default:
			  if (!*n)
				  return (*m != '\0' ? 0 : 1);
			  if (rfc1459::tolower(*m) != rfc1459::tolower(*n))
				  goto backtrack;
			  m++;
			  n++;
			  break;
		}
	}
}


#define MATCH_MAX_CALLS 512	/* ACK! This dies when it's less that this
				   and we have long lines to parse */
/** Check a string against a mask.
 * This test checks using extended wildcards: '*' means match zero
 * or more characters of any type; '?' means match exactly one
 * character of any type; '#' means match exactly one character that
 * is a number; '@' means match exactly one character that is a
 * letter; '\s' means match a space.
 *
 * This function supports escaping, so that a wildcard may be matched
 * exactly.
 *
 * @param[in] mask Wildcard-containing mask.
 * @param[in] name String to check against \a mask.
 * @return Zero if \a mask matches \a name, non-zero if no match.
 */
int
match_esc(const char *mask, const char *name)
{
	const unsigned char *m = (const unsigned char *)mask;
	const unsigned char *n = (const unsigned char *)name;
	const unsigned char *ma = (const unsigned char *)mask;
	const unsigned char *na = (const unsigned char *)name;
	int wild = 0;
	int calls = 0;
	int quote = 0;
	int match1 = 0;

	s_assert(mask != NULL);
	s_assert(name != NULL);

	if(!mask || !name)
		return 0;

	/* if the mask is "*", it matches everything */
	if((*m == '*') && (*(m + 1) == '\0'))
		return 1;

	while(calls++ < MATCH_MAX_CALLS)
	{
		if(quote)
			quote++;
		if(quote == 3)
			quote = 0;
		if(*m == '\\' && !quote)
		{
			m++;
			quote = 1;
			continue;
		}
		if(!quote && *m == '*')
		{
			/*
			 * XXX - shouldn't need to spin here, the mask should have been
			 * collapsed before match is called
			 */
			while(*m == '*')
				m++;

			wild = 1;
			ma = m;
			na = n;

			if(*m == '\\')
			{
				m++;
				/* This means it is an invalid mask -A1kmm. */
				if(!*m)
					return 0;
				quote++;
				continue;
			}
		}

		if(!*m)
		{
			if(!*n)
				return 1;
			if(quote)
				return 0;
			for(m--; (m > (const unsigned char *)mask) && (*m == '?'); m--)
				;

			if(*m == '*' && (m > (const unsigned char *)mask))
				return 1;
			if(!wild)
				return 0;
			m = ma;
			n = ++na;
		}
		else if(!*n)
		{
			/*
			 * XXX - shouldn't need to spin here, the mask should have been
			 * collapsed before match is called
			 */
			if(quote)
				return 0;
			while(*m == '*')
				m++;
			return (*m == 0);
		}

		if(quote)
			match1 = *m == 's' ? *n == ' ' : rfc1459::tolower(*m) == rfc1459::tolower(*n);
		else if(*m == '?')
			match1 = 1;
		else if(*m == '@')
			match1 = rfc1459::is_letter(*n);
		else if(*m == '#')
			match1 = rfc1459::is_digit(*n);
		else
			match1 = rfc1459::tolower(*m) == rfc1459::tolower(*n);
		if(match1)
		{
			if(*m)
				m++;
			if(*n)
				n++;
		}
		else
		{
			if(!wild)
				return 0;
			m = ma;
			n = ++na;
		}
	}
	return 0;
}

int comp_with_mask(void *addr, void *dest, unsigned int mask)
{
	if (memcmp(addr, dest, mask / 8) == 0)
	{
		int n = mask / 8;
		int m = ((-1) << (8 - (mask % 8)));
		if (mask % 8 == 0 || (((unsigned char *) addr)[n] & m) == (((unsigned char *) dest)[n] & m))
		{
			return (1);
		}
	}
	return (0);
}

int comp_with_mask_sock(struct sockaddr *addr, struct sockaddr *dest, unsigned int mask)
{
	void *iaddr = NULL;
	void *idest = NULL;

	if (addr->sa_family == AF_INET)
	{
		iaddr = &((struct sockaddr_in *)(void *)addr)->sin_addr;
		idest = &((struct sockaddr_in *)(void *)dest)->sin_addr;
	}
#ifdef RB_IPV6
	else
	{
		iaddr = &((struct sockaddr_in6 *)(void *)addr)->sin6_addr;
		idest = &((struct sockaddr_in6 *)(void *)dest)->sin6_addr;

	}
#endif

	return (comp_with_mask(iaddr, idest, mask));
}

/*
 * match_ips()
 *
 * Input - cidr ip mask, address
 */
int match_ips(const char *s1, const char *s2)
{
	struct rb_sockaddr_storage ipaddr, maskaddr;
	char mask[BUFSIZE];
	char address[HOSTLEN + 1];
	char *len;
	void *ipptr, *maskptr;
	int cidrlen, aftype;

	rb_strlcpy(mask, s1, sizeof(mask));
	rb_strlcpy(address, s2, sizeof(address));

	len = strrchr(mask, '/');
	if (len == NULL)
		return 0;

	*len++ = '\0';

	cidrlen = atoi(len);
	if (cidrlen <= 0)
		return 0;

#ifdef RB_IPV6
	if (strchr(mask, ':') && strchr(address, ':'))
	{
		if (cidrlen > 128)
			return 0;

		aftype = AF_INET6;
		ipptr = &((struct sockaddr_in6 *)&ipaddr)->sin6_addr;
		maskptr = &((struct sockaddr_in6 *)&maskaddr)->sin6_addr;
	}
	else
#endif
	if (!strchr(mask, ':') && !strchr(address, ':'))
	{
		if (cidrlen > 32)
			return 0;

		aftype = AF_INET;
		ipptr = &((struct sockaddr_in *)&ipaddr)->sin_addr;
		maskptr = &((struct sockaddr_in *)&maskaddr)->sin_addr;
	}
	else
		return 0;

	if (rb_inet_pton(aftype, address, ipptr) <= 0)
		return 0;
	if (rb_inet_pton(aftype, mask, maskptr) <= 0)
		return 0;
	if (comp_with_mask(ipptr, maskptr, cidrlen))
		return 1;
	else
		return 0;
}

/* match_cidr()
 *
 * Input - mask, address
 * Ouput - 1 = Matched 0 = Did not match
 */

int match_cidr(const char *s1, const char *s2)
{
	struct rb_sockaddr_storage ipaddr, maskaddr;
	char mask[BUFSIZE];
	char address[NICKLEN + USERLEN + HOSTLEN + 6];
	char *ipmask;
	char *ip;
	char *len;
	void *ipptr, *maskptr;
	int cidrlen, aftype;

	rb_strlcpy(mask, s1, sizeof(mask));
	rb_strlcpy(address, s2, sizeof(address));

	ipmask = strrchr(mask, '@');
	if (ipmask == NULL)
		return 0;

	*ipmask++ = '\0';

	ip = strrchr(address, '@');
	if (ip == NULL)
		return 0;
	*ip++ = '\0';


	len = strrchr(ipmask, '/');
	if (len == NULL)
		return 0;

	*len++ = '\0';

	cidrlen = atoi(len);
	if (cidrlen <= 0)
		return 0;

#ifdef RB_IPV6
	if (strchr(ip, ':') && strchr(ipmask, ':'))
	{
		if (cidrlen > 128)
			return 0;

		aftype = AF_INET6;
		ipptr = &((struct sockaddr_in6 *)&ipaddr)->sin6_addr;
		maskptr = &((struct sockaddr_in6 *)&maskaddr)->sin6_addr;
	}
	else
#endif
	if (!strchr(ip, ':') && !strchr(ipmask, ':'))
	{
		if (cidrlen > 32)
			return 0;

		aftype = AF_INET;
		ipptr = &((struct sockaddr_in *)&ipaddr)->sin_addr;
		maskptr = &((struct sockaddr_in *)&maskaddr)->sin_addr;
	}
	else
		return 0;

	if (rb_inet_pton(aftype, ip, ipptr) <= 0)
		return 0;
	if (rb_inet_pton(aftype, ipmask, maskptr) <= 0)
		return 0;
	if (comp_with_mask(ipptr, maskptr, cidrlen) && match(mask, address))
		return 1;
	else
		return 0;
}

/* collapse()
 *
 * collapses a string containing multiple *'s.
 */
char *collapse(char *pattern)
{
	char *p = pattern, *po = pattern;
	char c;
	int f = 0;

	if (p == NULL)
		return NULL;

	while ((c = *p++))
	{
		if (c == '*')
		{
			if (!(f & 1))
				*po++ = '*';
			f |= 1;
		}
		else
		{
			*po++ = c;
			f &= ~1;
		}
	}
	*po++ = 0;

	return pattern;
}

/* collapse_esc()
 *
 * The collapse() function with support for escaping characters
 */
char *collapse_esc(char *pattern)
{
	char *p = pattern, *po = pattern;
	char c;
	int f = 0;

	if (p == NULL)
		return NULL;

	while ((c = *p++))
	{
		if (!(f & 2) && c == '*')
		{
			if (!(f & 1))
				*po++ = '*';
			f |= 1;
		}
		else if (!(f & 2) && c == '\\')
		{
			*po++ = '\\';
			f |= 2;
		}
		else
		{
			*po++ = c;
			f &= ~3;
		}
	}
	*po++ = 0;
	return pattern;
}

/*
 * irccmp - case insensitive comparison of two 0 terminated strings.
 *
 *      returns  0, if s1 equal to s2
 *              <0, if s1 lexicographically less than s2
 *              >0, if s1 lexicographically greater than s2
 */
int irccmp(const char *s1, const char *s2)
{
	const unsigned char *str1 = (const unsigned char *)s1;
	const unsigned char *str2 = (const unsigned char *)s2;
	int res;

	s_assert(s1 != NULL);
	s_assert(s2 != NULL);

	while ((res = rfc1459::toupper(*str1) - rfc1459::toupper(*str2)) == 0)
	{
		if (*str1 == '\0')
			return 0;
		str1++;
		str2++;
	}
	return (res);
}

int ircncmp(const char *s1, const char *s2, int n)
{
	const unsigned char *str1 = (const unsigned char *)s1;
	const unsigned char *str2 = (const unsigned char *)s2;
	int res;
	s_assert(s1 != NULL);
	s_assert(s2 != NULL);

	while ((res = rfc1459::toupper(*str1) - rfc1459::toupper(*str2)) == 0)
	{
		str1++;
		str2++;
		n--;
		if (n == 0 || (*str1 == '\0' && *str2 == '\0'))
			return 0;
	}
	return (res);
}


}
