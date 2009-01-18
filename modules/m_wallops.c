/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_wallops.c: Sends a message to all operators.
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
 *  $Id: m_wallops.c 1377 2006-05-20 13:48:37Z jilles $
 */

#include "stdinc.h"
#include "client.h"
#include "ircd.h"
#include "match.h"
#include "numeric.h"
#include "send.h"
#include "s_user.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_serv.h"

static int mo_operwall(struct Client *, struct Client *, int, const char **);
static int ms_operwall(struct Client *, struct Client *, int, const char **);
static int ms_wallops(struct Client *, struct Client *, int, const char **);

struct Message wallops_msgtab = {
	"WALLOPS", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, {ms_wallops, 2}, {ms_wallops, 2}, mg_ignore, {ms_wallops, 2}}
};
struct Message operwall_msgtab = {
	"OPERWALL", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, {ms_operwall, 2}, mg_ignore, mg_ignore, {mo_operwall, 2}}
};

mapi_clist_av1 wallops_clist[] = { &wallops_msgtab, &operwall_msgtab, NULL };
DECLARE_MODULE_AV1(wallops, NULL, NULL, wallops_clist, NULL, NULL, "$Revision: 1377 $");

/*
 * mo_operwall (write to *all* opers currently online)
 *      parv[1] = message text
 */
static int
mo_operwall(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(!IsOperOperwall(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "operwall");
		return 0;
	}

	sendto_wallops_flags(UMODE_OPERWALL, source_p, "OPERWALL - %s", parv[1]);
	sendto_server(client_p, NULL, CAP_TS6, NOCAPS, ":%s OPERWALL :%s", 
		      use_id(source_p), parv[1]);

	return 0;
}

/*
 * ms_operwall - OPERWALL message handler
 *  (write to *all* local opers currently online)
 *      parv[1] = message text
 */
static int
ms_operwall(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	sendto_server(client_p, NULL, CAP_TS6, NOCAPS, ":%s OPERWALL :%s",
		      use_id(source_p), parv[1]);
	sendto_wallops_flags(UMODE_OPERWALL, source_p, "OPERWALL - %s", parv[1]);

	return 0;
}

/*
 * ms_wallops (write to *all* opers currently online)
 *      parv[1] = message text
 */
static int
ms_wallops(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	const char *prefix = "";

	if (MyClient(source_p) && !IsOperMassNotice(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "mass_notice");
		return 0;
	}

	if (IsPerson(source_p))
	{
		if (!strncmp(parv[1], "OPERWALL - ", 11) ||
				!strncmp(parv[1], "LOCOPS - ", 9) ||
				!strncmp(parv[1], "SLOCOPS - ", 10) ||
				!strncmp(parv[1], "ADMINWALL - ", 12))
			prefix = "WALLOPS - ";
	}

	sendto_wallops_flags(UMODE_WALLOP, source_p, "%s%s", prefix, parv[1]);

	sendto_server(client_p, NULL, CAP_TS6, NOCAPS, ":%s WALLOPS :%s", 
		      use_id(source_p), parv[1]);

	return 0;
}

