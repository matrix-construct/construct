/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_admin.c: Sends administrative information to a user.
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

const char admin_desc[] =
	"Provides the ADMIN command to show server administrator information";

static void m_admin(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void mr_admin(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void ms_admin(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void do_admin(client::client &source);
static void admin_spy(client::client &);

struct Message admin_msgtab = {
	"ADMIN", 0, 0, 0, 0,
	{{mr_admin, 0}, {m_admin, 0}, {ms_admin, 0}, mg_ignore, mg_ignore, {ms_admin, 0}}
};

int doing_admin_hook;

mapi_clist_av1 admin_clist[] = { &admin_msgtab, NULL };
mapi_hlist_av1 admin_hlist[] = {
	{ "doing_admin",	&doing_admin_hook },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(admin, NULL, NULL, admin_clist, admin_hlist, NULL, NULL, NULL, admin_desc);

/*
 * mr_admin - ADMIN command handler
 *      parv[1] = servername
 */
static void
mr_admin(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	static time_t last_used = 0L;

	if((last_used + ConfigFileEntry.pace_wait) > rb_current_time())
	{
		sendto_one(&source, form_str(RPL_LOAD2HI),
			   me.name,
			   EmptyString(source.name) ? "*" : source.name,
			   "ADMIN");
		return;
	}
	else
		last_used = rb_current_time();

	do_admin(source);
}

/*
 * m_admin - ADMIN command handler
 *      parv[1] = servername
 */
static void
m_admin(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	static time_t last_used = 0L;

	if(parc > 1)
	{
		if((last_used + ConfigFileEntry.pace_wait) > rb_current_time())
		{
			sendto_one(&source, form_str(RPL_LOAD2HI),
				   me.name, source.name, "ADMIN");
			return;
		}
		else
			last_used = rb_current_time();

		if(hunt_server(&client, &source, ":%s ADMIN :%s", 1, parc, parv) != HUNTED_ISME)
			return;
	}

	do_admin(source);
}


/*
 * ms_admin - ADMIN command handler, used for OPERS as well
 *      parv[1] = servername
 */
static void
ms_admin(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	if(hunt_server(&client, &source, ":%s ADMIN :%s", 1, parc, parv) != HUNTED_ISME)
		return;

	do_admin(source);
}


/*
 * do_admin
 *
 * inputs	- pointer to client to report to
 * output	- none
 * side effects	- admin info is sent to client given
 */
static void
do_admin(client::client &source)
{
	if(IsPerson(&source))
		admin_spy(source);

	sendto_one_numeric(&source, RPL_ADMINME, form_str(RPL_ADMINME), me.name);
	if(AdminInfo.name != NULL)
		sendto_one_numeric(&source, RPL_ADMINLOC1, form_str(RPL_ADMINLOC1), AdminInfo.name);
	if(AdminInfo.description != NULL)
		sendto_one_numeric(&source, RPL_ADMINLOC2, form_str(RPL_ADMINLOC2), AdminInfo.description);
	if(AdminInfo.email != NULL)
		sendto_one_numeric(&source, RPL_ADMINEMAIL, form_str(RPL_ADMINEMAIL), AdminInfo.email);
}

/* admin_spy()
 *
 * input	- pointer to client
 * output	- none
 * side effects - event doing_admin is called
 */
static void
admin_spy(client::client &source)
{
	hook_data hd;

	hd.client = &source;
	hd.arg1 = hd.arg2 = NULL;

	call_hook(doing_admin_hook, &hd);
}
