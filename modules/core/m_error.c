/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_error.c: Handles error messages from the other end.
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
 *  $Id: m_error.c 494 2006-01-15 16:08:28Z jilles $
 */

#include "stdinc.h"
#include "client.h"
#include "common.h"		/* FALSE */
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "msg.h"
#include "memory.h"
#include "modules.h"
#include "s_log.h"
#include "s_conf.h"

static int m_error(struct Client *, struct Client *, int, const char **);
static int ms_error(struct Client *, struct Client *, int, const char **);

struct Message error_msgtab = {
	"ERROR", 0, 0, 0, MFLG_SLOW | MFLG_UNREG,
	{{m_error, 0}, mg_ignore, mg_ignore, {ms_error, 0}, mg_ignore, mg_ignore}
};

mapi_clist_av1 error_clist[] = {
	&error_msgtab, NULL
};

DECLARE_MODULE_AV1(error, NULL, NULL, error_clist, NULL, NULL, "$Revision: 494 $");


/*
 * Note: At least at protocol level ERROR has only one parameter,
 * although this is called internally from other functions
 * --msa
 *
 *      parv[0] = sender prefix
 *      parv[*] = parameters
 */
int
m_error(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	const char *para;

	para = (parc > 1 && *parv[1] != '\0') ? parv[1] : "<>";

	ilog(L_SERVER, "Received ERROR message from %s: %s",
	     log_client_name(source_p, SHOW_IP), para);

	if(IsAnyServer(client_p) && ConfigFileEntry.hide_error_messages < 2)
	{
		sendto_realops_snomask(SNO_GENERAL, L_ADMIN,
				     "ERROR :from %s -- %s",
				     get_server_name(client_p, HIDE_IP), para);

		if(!ConfigFileEntry.hide_error_messages)
			sendto_realops_snomask(SNO_GENERAL, L_OPER,
					     "ERROR :from %s -- %s",
					     get_server_name(client_p, HIDE_IP), para);
	}

	exit_client(client_p, source_p, source_p, "ERROR");

	return 0;
}

static int
ms_error(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	const char *para;

	para = (parc > 1 && *parv[1] != '\0') ? parv[1] : "<>";

	ilog(L_SERVER, "Received ERROR message from %s: %s",
	     log_client_name(source_p, SHOW_IP), para);

	if(ConfigFileEntry.hide_error_messages == 2)
		return 0;

	if(client_p == source_p)
	{
		sendto_realops_snomask(SNO_GENERAL, L_ADMIN, "ERROR :from %s -- %s",
				     get_server_name(client_p, HIDE_IP), para);

		if(!ConfigFileEntry.hide_error_messages)
			sendto_realops_snomask(SNO_GENERAL, L_OPER,
					     "ERROR :from %s -- %s",
					     get_server_name(client_p, HIDE_IP), para);
	}
	else
	{
		sendto_realops_snomask(SNO_GENERAL, L_ADMIN, "ERROR :from %s via %s -- %s",
				     source_p->name, get_server_name(client_p, HIDE_IP), para);

		if(!ConfigFileEntry.hide_error_messages)
			sendto_realops_snomask(SNO_GENERAL, L_OPER,
					     "ERROR :from %s via %s -- %s",
					     source_p->name,
					     get_server_name(client_p, HIDE_IP), para);
	}

	return 0;
}
