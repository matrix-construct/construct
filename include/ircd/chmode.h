/*
 *  charybdis: An advanced ircd.
 *  chmode.h: The ircd channel header.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
 *  Copyright (C) 2008 charybdis development team
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
#define HAVE_IRCD_CHMOD_H

/* something not included in messages.tab
 * to change some hooks behaviour when needed
 * -- dwr
 */
#define ERR_CUSTOM 1000

/* Maximum mode changes allowed per client, per server is different */
#define MAXMODEPARAMS      4
#define MAXMODEPARAMSSERV  10
#define MODEBUFLEN         200

#ifdef __cplusplus
namespace ircd {

/* Channel mode classification */
typedef enum
{
	CHM_A,    // Mode has a parameter apropos a list (or no param for xfer)
	CHM_B,    // Always has a parameter
	CHM_C,    // Only has a parameter on MODE_ADD
	CHM_D,    // Never has a parameter
}
ChmClass;

/* Channel mode mask */
#define MODE_PRIVATE        0x00000001
#define MODE_SECRET         0x00000002
#define MODE_MODERATED      0x00000004
#define MODE_TOPICLIMIT     0x00000008
#define MODE_INVITEONLY     0x00000010
#define MODE_NOPRIVMSGS     0x00000020
#define MODE_REGONLY        0x00000040
#define MODE_EXLIMIT        0x00000100  /* exempt from list limits, +b/+e/+I/+q */
#define MODE_PERMANENT      0x00000200  /* permanant channel, +P */
#define MODE_OPMODERATE     0x00000400  /* send rejected messages to ops */
#define MODE_FREEINVITE     0x00000800  /* allow free use of /invite */
#define MODE_FREETARGET     0x00001000  /* can be forwarded to without authorization */
#define MODE_DISFORWARD     0x00002000  /* disable channel forwarding */

#define CHFL_BAN        0x10000000	/* ban channel flag */
#define CHFL_EXCEPTION  0x20000000	/* exception to ban channel flag */
#define CHFL_INVEX      0x40000000
#define CHFL_QUIET      0x80000000

/* extban function results */
#define EXTBAN_INVALID -1   /* invalid mask, false even if negated */
#define EXTBAN_NOMATCH  0   /* valid mask, no match */
#define EXTBAN_MATCH    1   /* matches */

struct Client;
struct Channel;

typedef int (*ExtbanFunc)
        (const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);

typedef void (*ChmFunc)
        (struct Client *source_p, struct Channel *chptr,
        int alevel, int parc, int *parn, const char **parv, int *errors, int dir, char c, long mode_type);

struct Chm
{
	ChmFunc set_func;
	ChmClass mode_class;
	unsigned long mode_type;
};


extern struct Chm chmode_table[256];
extern ExtbanFunc extban_table[256];
extern char chmode_arity[2][256];
extern char chmode_class[4][256];
extern int chmode_flags[256];


#define CHM_FUNCTION(_NAME_)                                      \
void _NAME_(struct Client *source_p, struct Channel *chptr,       \
            int alevel, int parc, int *parn, const char **parv,   \
            int *errors, int dir, char c, long mode_type);

CHM_FUNCTION(chm_nosuch)
CHM_FUNCTION(chm_orphaned)
CHM_FUNCTION(chm_simple)
CHM_FUNCTION(chm_ban)
CHM_FUNCTION(chm_hidden)
CHM_FUNCTION(chm_staff)
CHM_FUNCTION(chm_forward)
CHM_FUNCTION(chm_throttle)
CHM_FUNCTION(chm_key)
CHM_FUNCTION(chm_limit)
CHM_FUNCTION(chm_op)
CHM_FUNCTION(chm_voice)

unsigned int cflag_add(const unsigned char c, const ChmClass chmclass, const ChmFunc function);
void cflag_orphan(const unsigned char c);

void chmode_init(void);

}      // namespace ircd
#endif // __cplusplus
