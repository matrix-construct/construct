/*
 *  ircd-ratbox: A slightly useful ircd.
 *  match.h: A header for the ircd string functions.
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
 */

#pragma once
#define HAVE_IRCD_MATCH_H

#define EmptyString(x) ((x) == NULL || *(x) == '\0')

#ifdef __cplusplus
namespace ircd {

/*
 * match - compare name with mask, mask may contain * and ? as wildcards
 * match - returns 1 on successful match, 0 otherwise
 *
 * match_mask - like match() but a '?' in mask does not match a '*' in name
 * match_esc - compare with support for escaping chars
 * match_cidr - compares u!h@addr with u!h@addr/cidr
 * match_ips - compares addr with addr/cidr in ascii form
 */
extern int match(const char *mask, const char *name);
extern int match_mask(const char *oldmask, const char *newmask);
extern int match_esc(const char *mask, const char *name);
extern int match_cidr(const char *mask, const char *name);
extern int match_ips(const char *mask, const char *name);

/*
 * comp_with_mask - compares to IP address
 */
int comp_with_mask(void *addr, void *dest, unsigned int mask);
int comp_with_mask_sock(struct sockaddr *addr, struct sockaddr *dest, unsigned int mask);

/*
 * collapse - collapse a string in place, converts multiple adjacent *'s
 * into a single *.
 * collapse - modifies the contents of pattern
 *
 * collapse_esc() - collapse with support for escaping chars
 */
extern char *collapse(char *pattern);
extern char *collapse_esc(char *pattern);

/*
 * irccmp - case insensitive comparison of s1 and s2
 */
extern int irccmp(const char *s1, const char *s2);
/*
 * ircncmp - counted case insensitive comparison of s1 and s2
 */
extern int ircncmp(const char *s1, const char *s2, int n);

/* Below are used for radix trees and the like */
static inline void irccasecanon(char *str)
{
        while (*str)
        {
                *str = rfc1459::toupper(*str);
                str++;
        }
        return;
}

static inline void strcasecanon(char *str)
{
        while (*str)
        {
                *str = toupper((unsigned char)*str);
                str++;
        }
        return;
}

}      // namespace ircd
#endif // __cplusplus
