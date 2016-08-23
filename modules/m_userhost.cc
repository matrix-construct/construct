/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_userhost.c: Shows a user's host.
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

static const char userhost_desc[] =
	"Provides the USERHOST command to show a user's host";

static char buf[BUFSIZE];

static void m_userhost(struct MsgBuf *, client::client &, client::client &, int, const char **);

struct Message userhost_msgtab = {
	"USERHOST", 0, 0, 0, 0,
	{mg_unreg, {m_userhost, 2}, mg_ignore, mg_ignore, mg_ignore, {m_userhost, 2}}
};

mapi_clist_av1 userhost_clist[] = { &userhost_msgtab, NULL };

DECLARE_MODULE_AV2(userhost, NULL, NULL, userhost_clist, NULL, NULL, NULL, NULL, userhost_desc);

/*
 * m_userhost added by Darren Reed 13/8/91 to aid clients and reduce
 * the need for complicated requests like WHOIS. It returns user/host
 * information only (no spurious AWAY labels or channels).
 */
static void
m_userhost(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	client::client *target_p;
	char response[NICKLEN * 2 + USERLEN + HOSTLEN + 30];
	char *t;
	int i;			/* loop counter */
	int cur_len;
	int rl;

	cur_len = sprintf(buf, form_str(RPL_USERHOST), me.name, source.name, "");
	t = buf + cur_len;

	for (i = 1; i <= 5; i++)
	{
		if(parc < i + 1)
			break;

		if((target_p = client::find_person(parv[i])) != NULL)
		{
			/*
			 * Show real IP for USERHOST on yourself.
			 * This is needed for things like mIRC, which do a server-based
			 * lookup (USERHOST) to figure out what the clients' local IP
			 * is.  Useful for things like NAT, and dynamic dial-up users.
			 */
			if(my(*target_p) && (target_p == &source))
			{
				rl = sprintf(response, "%s%s=%c%s@%s ",
						target_p->name,
						IsOper(target_p) ? "*" : "",
						(away(user(*target_p)).size())? '-' : '+',
						target_p->username,
						target_p->sockhost);
			}
			else
			{
				rl = sprintf(response, "%s%s=%c%s@%s ",
						target_p->name,
						IsOper(target_p) ? "*" : "",
						(away(user(*target_p)).size())? '-' : '+',
						target_p->username, target_p->host);
			}

			if((rl + cur_len) < (BUFSIZE - 10))
			{
				sprintf(t, "%s", response);
				t += rl;
				cur_len += rl;
			}
			else
				break;
		}
	}

	sendto_one(&source, "%s", buf);
}
