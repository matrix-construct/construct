/*
 *  Charybdis: an advanced Internet Relay Chat Daemon(ircd).
 *  m_omode.c: allows oper mode hacking
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
 *  Copyright (C) 2006 Charybdis development team
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
 *  $Id: m_omode.c 3121 2007-01-02 13:23:04Z jilles $
 */

#include "stdinc.h"
#include "channel.h"
#include "client.h"
#include "hash.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "s_user.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "packet.h"

static int mo_omode(struct Client *, struct Client *, int, const char **);

struct Message omode_msgtab = {
	"OMODE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_omode, 3}}
};

mapi_clist_av1 omode_clist[] = { &omode_msgtab, NULL };

DECLARE_MODULE_AV1(omode, NULL, NULL, omode_clist, NULL, NULL, "$Revision: 3121 $");

/*
 * mo_omode - MODE command handler
 * parv[1] - channel
 */
static int
mo_omode(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Channel *chptr = NULL;
	struct membership *msptr;
	char params[512];
	int i;
	int wasonchannel;

	/* admins only */
	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "admin");
		return 0;
	}

	/* Now, try to find the channel in question */
	if(!IsChanPrefix(parv[1][0]) || !check_channel_name(parv[1]))
	{
		sendto_one_numeric(source_p, ERR_BADCHANNAME,
				form_str(ERR_BADCHANNAME), parv[1]);
		return 0;
	}

	chptr = find_channel(parv[1]);

	if(chptr == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
				form_str(ERR_NOSUCHCHANNEL), parv[1]);
		return 0;
	}

	/* Now know the channel exists */
	msptr = find_channel_membership(chptr, source_p);
	wasonchannel = msptr != NULL;

	if (is_chanop(msptr))
	{
		sendto_one_notice(source_p, ":Use a normal MODE you idiot");
		return 0;
	}

	params[0] = '\0';
	for (i = 2; i < parc; i++)
	{
		if (i != 2)
			rb_strlcat(params, " ", sizeof params);
		rb_strlcat(params, parv[i], sizeof params);
	}

	sendto_wallops_flags(UMODE_WALLOP, &me, 
			     "OMODE called for [%s] [%s] by %s!%s@%s",
			     parv[1], params, source_p->name, source_p->username, source_p->host);
	ilog(L_MAIN, "OMODE called for [%s] [%s] by %s",
	     parv[1], params, get_oper_name(source_p));

	if(*chptr->chname != '&')
		sendto_server(NULL, NULL, NOCAPS, NOCAPS, 
			      ":%s WALLOPS :OMODE called for [%s] [%s] by %s!%s@%s",
			      me.name, parv[1], params, source_p->name, source_p->username,
			      source_p->host);

#if 0
	set_channel_mode(client_p, source_p->servptr, chptr, msptr, 
			 parc - 2, parv + 2);
#else
	if (parc == 4 && !strcmp(parv[2], "+o") && !irccmp(parv[3], source_p->name))
	{
		/* Opping themselves */
		if (!wasonchannel)
		{
			sendto_one_numeric(source_p, ERR_USERNOTINCHANNEL,
					   form_str(ERR_USERNOTINCHANNEL), parv[3], chptr->chname);
			return 0;
		}
		sendto_channel_local(ALL_MEMBERS, chptr, ":%s MODE %s +o %s",
				me.name, parv[1], source_p->name);
		sendto_server(NULL, chptr, CAP_TS6, NOCAPS,
				":%s TMODE %ld %s +o %s",
				me.id, (long) chptr->channelts, parv[1],
				source_p->id);
		msptr->flags |= CHFL_CHANOP;
	}
	else
	{
		/* Hack it so set_channel_mode() will accept */
		if (wasonchannel)
			msptr->flags |= CHFL_CHANOP;
		else
		{
			add_user_to_channel(chptr, source_p, CHFL_CHANOP);
			msptr = find_channel_membership(chptr, source_p);
		}
		set_channel_mode(client_p, source_p, chptr, msptr, 
				parc - 2, parv + 2);
		/* We know they were not opped before and they can't have opped
		 * themselves as set_channel_mode() does not allow that
		 * -- jilles */
		if (wasonchannel)
			msptr->flags &= ~CHFL_CHANOP;
		else
			remove_user_from_channel(msptr);
	}
#endif
	return 0;
}
