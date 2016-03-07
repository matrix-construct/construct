/*
 *  charybdis
 *  m_extendchans.c: Allow an oper or service to let a given user join more channels.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2006 ircd-ratbox development team
 *  Copyright (C) 2006-2016 ircd-seven development team
 *  Copyright (C) 2015-2016 ChatLounge IRC Network Development Team
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

#include "stdinc.h"
#include "client.h"
#include "modules.h"
#include "s_newconf.h"
#include "send.h"
#include "numeric.h"

static int mo_extendchans(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static int me_extendchans(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

struct Message extendchans_msgtab = {
	"EXTENDCHANS", 0, 0, 0, 0,
	{ mg_unreg, mg_ignore, mg_ignore, mg_ignore, {me_extendchans, 2}, {mo_extendchans, 2}}
};

mapi_clist_av1 extendchans_clist[] = { &extendchans_msgtab, NULL };

static const char extendchans_desc =
	"Allow an oper or service to let a given user join more channels";

DECLARE_MODULE_AV2(extendchans, NULL, NULL, extendchans_clist, NULL, NULL, NULL, NULL, extendchans_desc);

static int
mo_extendchans(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;

	if(!HasPrivilege(source_p, "oper:extendchans"))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "extendchans");
		return 0;
	}

	if(EmptyString(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "EXTENDCHANS");
		return 0;
	}

	if((target_p = find_chasing(source_p, parv[1], NULL)) == NULL)
		return 0;

	/* Is the target user local? */
	if(MyClient(target_p))
	{
		sendto_one_notice(target_p, ":*** %s (%s@%s) is extending your channel limit",
			source_p->name, source_p->username, source_p->host);
		SetExtendChans(target_p);
	}
	else /* Target user isn't local, so pass it on. */
	{
		struct Client *cptr = target_p->servptr;
		sendto_one(cptr, ":%s ENCAP %s EXTENDCHANS %s",
			get_id(source_p, cptr), cptr->name, get_id(target_p, cptr));
	}

	sendto_one_notice(source_p, ":You have extended the channel limit on: %s (%s@%s)",
		target_p->name, target_p->username, target_p->orighost);

	return 0;
}

static int
me_extendchans(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;

	target_p = find_person(parv[1]);
	if(target_p == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHNICK, form_str(ERR_NOSUCHNICK), parv[1]);
		return 0;
	}

	/* Is the target user local?  If not, pass it on. */
	if(!MyClient(target_p))
	{
		struct Client *cptr = target_p->servptr;
		sendto_one(cptr, ":%s ENCAP %s EXTENDCHANS %s",
			get_id(source_p, cptr), cptr->name, get_id(target_p, cptr));
		return 0;
	}

	sendto_one_notice(target_p, ":*** %s (%s@%s) is extending your channel limit",
		source_p->name, source_p->username, source_p->host);
	SetExtendChans(target_p);

	return 0;
}
