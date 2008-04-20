/*
 *  ircd-ratbox: A slightly useful ircd.
 *  irc_string.h: A header for the ircd string functions.
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
 *
 *  $Id: irc_string.h 3538 2007-07-26 14:21:57Z jilles $
 */

#ifndef INCLUDED_match_h
#define INCLUDED_match_h

#include "setup.h"
#include "ircd_defs.h"

/*
 * match - compare name with mask, mask may contain * and ? as wildcards
 * match - returns 1 on successful match, 0 otherwise
 *
 * mask_match - compare one mask to another
 * match_esc - compare with support for escaping chars
 * match_cidr - compares u!h@addr with u!h@addr/cidr
 * match_ips - compares addr with addr/cidr in ascii form
 */
extern int match(const char *mask, const char *name);
extern int mask_match(const char *oldmask, const char *newmask);
extern int match_esc(const char *mask, const char *name);
extern int match_cidr(const char *mask, const char *name);
extern int match_ips(const char *mask, const char *name);

/*
 * comp_with_mask - compares to IP address
 */
int comp_with_mask(void *addr, void *dest, u_int mask);
int comp_with_mask_sock(struct sockaddr *addr, struct sockaddr *dest, u_int mask);

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
/*
** canonize - reduce a string of duplicate list entries to contain
** only the unique items.
*/
#ifdef NO_DUPE_MULTI_MESSAGES
extern char *canonize(char *);
#endif

#define EmptyString(x) ((x) == NULL || *(x) == '\0')
#define CheckEmpty(x) EmptyString(x) ? "" : x

/*
 * character macros
 */
extern const unsigned char ToLowerTab[];
#define ToLower(c) (ToLowerTab[(unsigned char)(c)])

extern const unsigned char ToUpperTab[];
#define ToUpper(c) (ToUpperTab[(unsigned char)(c)])

extern const unsigned int CharAttrs[];

#define PRINT_C   0x001
#define CNTRL_C   0x002
#define ALPHA_C   0x004
#define PUNCT_C   0x008
#define DIGIT_C   0x010
#define SPACE_C   0x020
#define NICK_C    0x040
#define CHAN_C    0x080
#define KWILD_C   0x100
#define CHANPFX_C 0x200
#define USER_C    0x400
#define HOST_C    0x800
#define NONEOS_C 0x1000
#define SERV_C   0x2000
#define EOL_C    0x4000
#define MWILD_C  0x8000
#define LET_C   0x10000 /* an actual letter */
#define FCHAN_C 0x20000 /* a 'fake' channel char */

#define IsHostChar(c)   (CharAttrs[(unsigned char)(c)] & HOST_C)
#define IsUserChar(c)   (CharAttrs[(unsigned char)(c)] & USER_C)
#define IsChanPrefix(c) (CharAttrs[(unsigned char)(c)] & CHANPFX_C)
#define IsChanChar(c)   (CharAttrs[(unsigned char)(c)] & CHAN_C)
#define IsFakeChanChar(c)	(CharAttrs[(unsigned char)(c)] & FCHAN_C)
#define IsKWildChar(c)  (CharAttrs[(unsigned char)(c)] & KWILD_C)
#define IsMWildChar(c)  (CharAttrs[(unsigned char)(c)] & MWILD_C)
#define IsNickChar(c)   (CharAttrs[(unsigned char)(c)] & NICK_C)
#define IsServChar(c)   (CharAttrs[(unsigned char)(c)] & (NICK_C | SERV_C))
#define IsIdChar(c)	(CharAttrs[(unsigned char)(c)] & (DIGIT_C | LET_C))
#define IsLetter(c)	(CharAttrs[(unsigned char)(c)] & LET_C)
#define IsCntrl(c)      (CharAttrs[(unsigned char)(c)] & CNTRL_C)
#define IsAlpha(c)      (CharAttrs[(unsigned char)(c)] & ALPHA_C)
#define IsSpace(c)      (CharAttrs[(unsigned char)(c)] & SPACE_C)
#define IsLower(c)      (IsAlpha((c)) && ((unsigned char)(c) > 0x5f))
#define IsUpper(c)      (IsAlpha((c)) && ((unsigned char)(c) < 0x60))
#define IsDigit(c)      (CharAttrs[(unsigned char)(c)] & DIGIT_C)
#define IsXDigit(c) (IsDigit(c) || ('a' <= (c) && (c) <= 'f') || \
        ('A' <= (c) && (c) <= 'F'))
#define IsAlNum(c) (CharAttrs[(unsigned char)(c)] & (DIGIT_C | ALPHA_C))
#define IsPrint(c) (CharAttrs[(unsigned char)(c)] & PRINT_C)
#define IsAscii(c) ((unsigned char)(c) < 0x80)
#define IsGraph(c) (IsPrint((c)) && ((unsigned char)(c) != 0x32))
#define IsPunct(c) (!(CharAttrs[(unsigned char)(c)] & \
                                           (CNTRL_C | ALPHA_C | DIGIT_C)))

#define IsNonEOS(c) (CharAttrs[(unsigned char)(c)] & NONEOS_C)
#define IsEol(c) (CharAttrs[(unsigned char)(c)] & EOL_C)

#endif /* INCLUDED_match_h */
