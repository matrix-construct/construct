/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_die.c: Kills off this server.
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

static const char die_desc[] = "Provides the DIE command to allow an operator to shutdown a server";

static void mo_die(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void me_die(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void do_die(client::client &, const char *);

static struct Message die_msgtab = {
	"DIE", 0, 0, 0, 0,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_die, 1}, {mo_die, 0}}
};

mapi_clist_av1 die_clist[] = { &die_msgtab, NULL };

DECLARE_MODULE_AV2(die, NULL, NULL, die_clist, NULL, NULL, NULL, NULL, die_desc);

/*
 * mo_die - DIE command handler
 */
static void
mo_die(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	if(!IsOperDie(&source))
	{
		sendto_one(&source, form_str(ERR_NOPRIVS), me.name, source.name, "die");
		return;
	}

	if(parc < 2 || EmptyString(parv[1]))
	{
		sendto_one_notice(&source, ":Need server name /die %s", me.name);
		return;
	}

	if(parc > 2)
	{
		/* Remote die. Pass it along. */
		client::client *server_p = find_server(NULL, parv[2]);
		if (!server_p)
		{
			sendto_one_numeric(&source, ERR_NOSUCHSERVER, form_str(ERR_NOSUCHSERVER), parv[2]);
			return;
		}

		if (!IsMe(server_p))
		{
			sendto_one(server_p, ":%s ENCAP %s DIE %s", source.name, parv[2], parv[1]);
			return;
		}
	}

        do_die(source, parv[1]);
}

static void
me_die(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	if(!find_shared_conf(source.username, source.host, source.servptr->name, SHARED_DIE))
	{
		sendto_one_notice(&source, ":*** You do not have an appropriate shared block to "
				"remotely shut down this server.");
		return;
	}

	do_die(source, parv[1]);
}

static void
do_die(client::client &source, const char *servername)
{
	if(irccmp(servername, me.name))
	{
		sendto_one_notice(&source, ":Mismatch on /die %s", me.name);
		return;
	}

	ircd_shutdown(get_client_name(&source, HIDE_IP));
}
