/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_connect.c: Connects to a remote IRC server.
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
 *  $Id: m_connect.c 3161 2007-01-25 07:23:01Z nenolod $
 */

#include "stdinc.h"
#include "client.h"
#include "ircd.h"
#include "match.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "logger.h"
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "hash.h"
#include "modules.h"
#include "sslproc.h"

static int mo_connect(struct Client *, struct Client *, int, const char **);
static int ms_connect(struct Client *, struct Client *, int, const char **);

struct Message connect_msgtab = {
	"CONNECT", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, {ms_connect, 4}, {ms_connect, 4}, mg_ignore, {mo_connect, 2}}
};

mapi_clist_av1 connect_clist[] = { &connect_msgtab, NULL };
DECLARE_MODULE_AV1(connect, NULL, NULL, connect_clist, NULL, NULL, "$Revision: 3161 $");

/*
 * mo_connect - CONNECT command handler
 * 
 * Added by Jto 11 Feb 1989
 *
 * m_connect
 *      parv[1] = servername
 *      parv[2] = port number
 *      parv[3] = remote server
 */
static int
mo_connect(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	int port;
	int tmpport;
	struct server_conf *server_p;
	struct Client *target_p;

	/* always privileged with handlers */

	if(MyConnect(source_p) && !IsOperRemote(source_p) && parc > 3)
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "remote");
		return 0;
	}

	if(hunt_server(client_p, source_p, ":%s CONNECT %s %s :%s", 3, parc, parv) != HUNTED_ISME)
		return 0;

	if((target_p = find_server(source_p, parv[1])))
	{
		sendto_one_notice(source_p, ":Connect: Server %s already exists from %s.", parv[1], 
			target_p->from->name);
		return 0;
	}

	/*
	 * try to find the name, then host, if both fail notify ops and bail
	 */
	if((server_p = find_server_conf(parv[1])) == NULL)
	{
		sendto_one_notice(source_p, ":Connect: Host %s not listed in ircd.conf", parv[1]);
		return 0;
	}

	if(ServerConfSSL(server_p) && (!ssl_ok || !get_ssld_count()))
	{
		sendto_one_notice(source_p,
				  ":Connect: Server %s is set to use SSL/TLS but SSL/TLS is not configured.",
				  parv[1]);
		return 0;
	}

	/*
	 * Get port number from user, if given. If not specified,
	 * use the default form configuration structure. If missing
	 * from there, then use the precompiled default.
	 */
	tmpport = port = server_p->port;
	if(parc > 2 && !EmptyString(parv[2]))
	{
		if((port = atoi(parv[2])) <= 0)
		{
			sendto_one_notice(source_p, ":Connect: Illegal port number");
			return 0;
		}
	}
	else if(port <= 0 && (port = PORTNUM) <= 0)
	{
		sendto_one_notice(source_p, ":Connect: missing port number");
		return 0;
	}
	/*
	 * Notify all operators about remote connect requests
	 */

	ilog(L_SERVER, "CONNECT From %s : %s %s", source_p->name, parv[1], parc > 2 ? parv[2] : "");

	server_p->port = port;
	/*
	 * at this point we should be calling connect_server with a valid
	 * C:line and a valid port in the C:line
	 */
	if(serv_connect(server_p, source_p))
	{
			sendto_one_notice(source_p, ":*** Connecting to %s.%d",
				server_p->name, server_p->port);
	}
	else
	{
		sendto_one_notice(source_p, ":*** Couldn't connect to %s.%d",
			server_p->name, server_p->port);
	}

	/*
	 * client is either connecting with all the data it needs or has been
	 * destroyed
	 */
	server_p->port = tmpport;
	return 0;
}

/*
 * ms_connect - CONNECT command handler
 * 
 * Added by Jto 11 Feb 1989
 *
 * m_connect
 *      parv[1] = servername
 *      parv[2] = port number
 *      parv[3] = remote server
 */
static int
ms_connect(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	int port;
	int tmpport;
	struct server_conf *server_p;
	struct Client *target_p;

	if(hunt_server(client_p, source_p, ":%s CONNECT %s %s :%s", 3, parc, parv) != HUNTED_ISME)
		return 0;

	if((target_p = find_server(NULL, parv[1])))
	{
		sendto_one_notice(source_p, ":Connect: Server %s already exists from %s.",
				  parv[1], target_p->from->name);
		return 0;
	}

	/*
	 * try to find the name, then host, if both fail notify ops and bail
	 */
	if((server_p = find_server_conf(parv[1])) == NULL)
	{
		sendto_one_notice(source_p, ":Connect: Host %s not listed in ircd.conf",
				  parv[1]);
		return 0;
	}

	if(ServerConfSSL(server_p) && (!ssl_ok || !get_ssld_count()))
	{
		sendto_one_notice(source_p,
				  ":Connect: Server %s is set to use SSL/TLS but SSL/TLS is not configured.",
				  parv[1]);
		return 0;
	}

	/*
	 * Get port number from user, if given. If not specified,
	 * use the default form configuration structure. If missing
	 * from there, then use the precompiled default.
	 */
	tmpport = server_p->port;

	port = atoi(parv[2]);

	/* if someone sends port 0, and we have a config port.. use it */
	if(port == 0 && server_p->port)
		port = server_p->port;
	else if(port <= 0)
	{
		sendto_one_notice(source_p, ":Connect: Illegal port number");
		return 0;
	}

	/*
	 * Notify all operators about remote connect requests
	 */
	sendto_wallops_flags(UMODE_WALLOP, &me,
			     "Remote CONNECT %s %d from %s", 
			     parv[1], port, source_p->name);
	sendto_server(NULL, NULL, CAP_TS6, NOCAPS,
		      ":%s WALLOPS :Remote CONNECT %s %d from %s",
		      me.id, parv[1], port, source_p->name);

	ilog(L_SERVER, "CONNECT From %s : %s %d", source_p->name, parv[1], port);

	server_p->port = port;
	/*
	 * at this point we should be calling connect_server with a valid
	 * C:line and a valid port in the C:line
	 */
	if(serv_connect(server_p, source_p))
		sendto_one_notice(source_p, ":*** Connecting to %s.%d",
				  server_p->name, server_p->port);
	else
		sendto_one_notice(source_p, ":*** Couldn't connect to %s.%d",
				  server_p->name, server_p->port);
	/*
	 * client is either connecting with all the data it needs or has been
	 * destroyed
	 */
	server_p->port = tmpport;
	return 0;
}
