/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_user.c: Sends username information.
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

static const char user_desc[] =
	"Provides the USER command to register a new connection";

static void mr_user(struct MsgBuf *, client::client &, client::client &, int, const char **);

struct Message user_msgtab = {
	"USER", 0, 0, 0, 0,
	{{mr_user, 5}, mg_reg, mg_ignore, mg_ignore, mg_ignore, mg_reg}
};

mapi_clist_av1 user_clist[] = { &user_msgtab, NULL };
DECLARE_MODULE_AV2(user, NULL, NULL, user_clist, NULL, NULL, NULL, NULL, user_desc);

static void do_local_user(client::client &client, client::client &source,
		const char *username, const char *realname);

/* mr_user()
 *      parv[1] = username (login name, account)
 *      parv[2] = client host name (ignored)
 *      parv[3] = server host name (ignored)
 *      parv[4] = users gecos
 */
static void
mr_user(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	static char buf[BUFSIZE];
	char *p;

	if (strlen(client.id) == 3)
	{
		exit_client(&client, &client, &client, "Mixing client and server protocol");
		return;
	}

	if(source.flags & FLAGS_SENTUSER)
		return;

	if((p = (char *)strchr(parv[1], '@')))
		*p = '\0';

	snprintf(buf, sizeof(buf), "%s %s", parv[2], parv[3]);
	rb_free(source.localClient->fullcaps);
	source.localClient->fullcaps = rb_strdup(buf);

	do_local_user(client, source, parv[1], parv[4]);
}

static void
do_local_user(client::client &client, client::client &source,
	      const char *username, const char *realname)
{
	s_assert(NULL != &source);
	s_assert(source.username != username);

	make_user(source);

	source.flags |= FLAGS_SENTUSER;

	rb_strlcpy(source.info, realname, sizeof(source.info));

	if(!IsGotId(&source))
		rb_strlcpy(source.username, username, sizeof(source.username));

	if(source.name[0])
	{
		/* NICK already received, now I have USER... */
		register_local_user(&client, &source);
	}
}
