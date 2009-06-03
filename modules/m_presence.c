/*
 *  charybdis: an advanced ircd.
 *  m_presence.c: IRC presence protocol implementation
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
 *  Copyright (c) 2009 William Pitcock <nenolod@dereferenced.org>
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
 *  $Id: m_away.c 3370 2007-04-03 10:15:39Z nenolod $
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
#include "s_conf.h"
#include "s_serv.h"
#include "packet.h"

static int m_presence(struct Client *, struct Client *, int, const char **);
static int me_presence(struct Client *, struct Client *, int, const char **);

struct Message presence_msgtab = {
	"PRESENCE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_presence, 2}, {m_presence, 2}, mg_ignore, {me_presence, 2}, {m_presence, 2}}
};

mapi_clist_av1 presence_clist[] = { &presence_msgtab, NULL };
DECLARE_MODULE_AV1(presence, NULL, NULL, presence_clist, NULL, NULL, "$Revision$");

/*
** m_presence
**      parv[1] = key
**      parv[2] = setting
*/
static int
m_presence(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	const char *val;

	if(MyClient(source_p) && !IsFloodDone(source_p))
		flood_endgrace(source_p);

	if(!IsClient(source_p))
		return 0;

	if (!irccmp(parv[1], "away"))
	{
		sendto_one_notice(source_p, ":Please use /AWAY to change your away status");
		return 0;
	}

	if((parc < 3 || EmptyString(parv[2])) && !EmptyString(parv[1]))
	{
		if ((val = get_metadata(source_p, parv[1])) != NULL)
		{
			delete_metadata(source_p, parv[1]);

			sendto_server(client_p, NULL, CAP_TS6, NOCAPS,
					":%s ENCAP * PRESENCE %s", use_id(source_p), parv[1]);
		}
		if (MyConnect(source_p))
			sendto_one_numeric(source_p, RPL_METADATAREM, form_str(RPL_METADATAREM), parv[1]);
		return 0;
	}

	if (strlen(parv[1]) >= METADATAKEYLEN)
	{
		sendto_one_notice(source_p, ":Metadata key too long");
		return 0;
	}

	if ((val = get_metadata(source_p, parv[1])) != NULL)
	{
		if (!strcmp(parv[2], val))
			return 0;
	}

	set_metadata(source_p, parv[1], parv[2]);
	sendto_server(client_p, NULL, CAP_TS6, NOCAPS, 
		      ":%s ENCAP * PRESENCE %s :%s", use_id(source_p), parv[1], parv[2]);

	if(MyConnect(source_p))
		sendto_one_numeric(source_p, RPL_METADATASET, form_str(RPL_METADATASET), parv[1]);

	return 0;
}

/*
** me_presence
**      parv[1] = key
**      parv[2] = setting
*/
static int
me_presence(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	const char *val;

	if(!IsClient(source_p))
		return 0;

	if((parc < 3 || EmptyString(parv[2])) && !EmptyString(parv[1]))
	{
		delete_metadata(source_p, parv[1]);
		return 0;
	}

	if (strlen(parv[1]) >= METADATAKEYLEN)
		return 0;

	if ((val = get_metadata(source_p, parv[1])) != NULL)
	{
		if (!strcmp(parv[2], val))
			return 0;
	}

	set_metadata(source_p, parv[1], parv[2]);

	return 0;
}
