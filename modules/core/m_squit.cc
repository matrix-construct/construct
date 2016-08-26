/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_squit.c: Makes a server quit.
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
 */

using namespace ircd;

static const char squit_desc[] = "Provides the SQUIT command to cause a server to quit";

static void ms_squit(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void mo_squit(struct MsgBuf *, client::client &, client::client &, int, const char **);

struct Message squit_msgtab = {
	"SQUIT", 0, 0, 0, 0,
	{mg_unreg, mg_not_oper, {ms_squit, 0}, {ms_squit, 0}, mg_ignore, {mo_squit, 2}}
};

mapi_clist_av1 squit_clist[] = { &squit_msgtab, NULL };

DECLARE_MODULE_AV2(squit, NULL, NULL, squit_clist, NULL, NULL, NULL, NULL, squit_desc);

struct squit_parms
{
	const char *server_name;
	client::client *target_p;
};

static struct squit_parms *find_squit(client::client &client,
				      client::client &source, const char *server);


/*
 * mo_squit - SQUIT message handler
 *      parv[1] = server name
 *      parv[2] = comment
 */
static void
mo_squit(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	struct squit_parms *found_squit;
	const char *comment = (parc > 2 && parv[2]) ? parv[2] : client.name;

	if((found_squit = find_squit(client, source, parv[1])))
	{
		if(my_connect(*found_squit->target_p))
		{
			sendto_realops_snomask(sno::GENERAL, L_ALL,
					     "Received SQUIT %s from %s (%s)",
					     found_squit->target_p->name,
					     get_client_name(&source, HIDE_IP), comment);
			ilog(L_SERVER, "Received SQUIT %s from %s (%s)",
			     found_squit->target_p->name, log_client_name(&source, HIDE_IP),
			     comment);
		}
		else if(!IsOperRemote(&source))
		{
			sendto_one(&source, form_str(ERR_NOPRIVS),
				   me.name, source.name, "remote");
			return;
		}

		exit_client(&client, found_squit->target_p, &source, comment);
		return;
	}
	else
	{
		sendto_one_numeric(&source, ERR_NOSUCHSERVER, form_str(ERR_NOSUCHSERVER), parv[1]);
	}
}

/*
 * ms_squit - SQUIT message handler
 *      parv[1] = server name
 *      parv[2] = comment
 */
static void
ms_squit(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	client::client *target_p;
	const char *comment = (parc > 2 && parv[2]) ? parv[2] : client.name;

	if(parc < 2)
		target_p = &client;
	else
	{
		if((target_p = find_server(NULL, parv[1])) == NULL)
			return;

		if(is_me(*target_p))
			target_p = &client;
		if(!is_server(*target_p))
			return;
	}

	/* Server is closing its link */
	if (target_p == &client)
	{
		sendto_realops_snomask(sno::GENERAL, L_ALL, "Server %s closing link (%s)",
				target_p->name, comment);
	}
	/*
	 **  Notify all opers, if my local link is remotely squitted
	 */
	else if(my_connect(*target_p))
	{
		sendto_wallops_flags(umode::WALLOP, &me,
				     "Remote SQUIT %s from %s (%s)",
				     target_p->name, source.name, comment);

		sendto_server(NULL, NULL, CAP_TS6, NOCAPS,
			      ":%s WALLOPS :Remote SQUIT %s from %s (%s)",
			      me.id, target_p->name, source.name, comment);

		ilog(L_SERVER, "SQUIT From %s : %s (%s)", source.name, target_p->name, comment);
	}
	exit_client(&client, target_p, &source, comment);
}


/*
 * find_squit
 * inputs	- local server connection
 *		-
 *		-
 * output	- pointer to struct containing found squit or none if not found
 * side effects	-
 */
static struct squit_parms *
find_squit(client::client &client, client::client &source, const char *server)
{
	static struct squit_parms found_squit;
	client::client *target_p = NULL;
	client::client *p;
	rb_dlink_node *ptr;

	/* must ALWAYS be reset */
	found_squit.target_p = NULL;
	found_squit.server_name = NULL;


	/*
	 ** The following allows wild cards in SQUIT. Only useful
	 ** when the command is issued by an oper.
	 */

	RB_DLINK_FOREACH(ptr, global_serv_list.head)
	{
		p = (client::client *)ptr->data;
		if(is_server(*p) || is_me(*p))
		{
			if(match(server, p->name))
			{
				target_p = p;
				break;
			}
		}
	}

	if(target_p == NULL)
		return NULL;

	found_squit.target_p = target_p;
	found_squit.server_name = server;

	if(is_me(*target_p))
	{
		if(is_client(client))
		{
			if(my(client))
				sendto_one_notice(&source, ":You are trying to squit me.");

			return NULL;
		}
		else
		{
			found_squit.target_p = &client;
			found_squit.server_name = client.name;
		}
	}

	if(found_squit.target_p != NULL)
		return &found_squit;
	else
		return (NULL);
}
