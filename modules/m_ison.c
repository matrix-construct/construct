/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_ison.c: Provides a single line answer of whether a user is online.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
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
 *  $Id: m_ison.c 254 2005-09-21 23:35:12Z nenolod $
 */

#include "stdinc.h"
#include "client.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_conf.h"		/* ConfigFileEntry */
#include "s_serv.h"		/* uplink/IsCapable */
#include "hash.h"

#include <string.h>

static int m_ison(struct Client *, struct Client *, int, const char **);

struct Message ison_msgtab = {
	"ISON", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_ison, 2}, mg_ignore, mg_ignore, mg_ignore, {m_ison, 2}}
};

mapi_clist_av1 ison_clist[] = { &ison_msgtab, NULL };
DECLARE_MODULE_AV1(ison, NULL, NULL, ison_clist, NULL, NULL, "$Revision: 254 $");

static char buf[BUFSIZE];
static char buf2[BUFSIZE];


/*
 * m_ison added by Darren Reed 13/8/91 to act as an efficent user indicator
 * with respect to cpu/bandwidth used. Implemented for NOTIFY feature in
 * clients. Designed to reduce number of whois requests. Can process
 * nicknames in batches as long as the maximum buffer length.
 *
 * format:
 * ISON :nicklist
 */
static int
m_ison(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	char *nick;
	char *p;
	char *current_insert_point, *current_insert_point2;
	int len;
	int i;
	int done = 0;

	current_insert_point2 = buf2;
	*buf2 = '\0';

	rb_sprintf(buf, form_str(RPL_ISON), me.name, source_p->name);
	len = strlen(buf);
	current_insert_point = buf + len;

	/* rfc1489 is ambigious about how to handle ISON
	 * this should handle both interpretations.
	 */
	for (i = 1; i < parc; i++)
	{
		char *cs = LOCAL_COPY(parv[i]);
		for (nick = rb_strtok_r(cs, " ", &p); nick; nick = rb_strtok_r(NULL, " ", &p))
		{
			target_p = find_named_client(nick);

			if(target_p != NULL)
			{
				len = strlen(target_p->name);
				if((current_insert_point + (len + 5)) < (buf + sizeof(buf)))
				{
					memcpy((void *) current_insert_point,
					       (void *) target_p->name, len);
					current_insert_point += len;
					*current_insert_point++ = ' ';
				}
				else
				{
					done = 1;
					break;
				}
			}
		}
		if(done)
			break;
	}

	/*  current_insert_point--;
	 *  Do NOT take out the trailing space, it breaks ircII
	 *  --Rodder */

	*current_insert_point = '\0';
	*current_insert_point2 = '\0';

	sendto_one(source_p, "%s", buf);

	return 0;
}
