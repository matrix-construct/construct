/*
 *   IRC - Internet Relay Chat, contrib/m_findforwards.c
 *   Copyright (C) 2002 Hybrid Development Team
 *   Copyright (C) 2004 ircd-ratbox Development Team
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

using namespace ircd;

static const char findfowards_desc[] = "Allows operators to find forwards to a given channel";

static void m_findforwards(struct MsgBuf *msgbuf_p, client::client *client_p, client::client *source_p,
			int parc, const char *parv[]);

struct Message findforwards_msgtab = {
	"FINDFORWARDS", 0, 0, 0, 0,
	{mg_unreg, {m_findforwards, 2}, mg_ignore, mg_ignore, mg_ignore, {m_findforwards, 2}}
};

mapi_clist_av1 findforwards_clist[] = { &findforwards_msgtab, NULL };

DECLARE_MODULE_AV2(findforwards, NULL, NULL, findforwards_clist, NULL, NULL, NULL, NULL, findfowards_desc);

/*
** mo_findforwards
**      parv[1] = channel
*/
static void
m_findforwards(struct MsgBuf *msgbuf_p, client::client *client_p, client::client *source_p, int parc, const char *parv[])
{
	static time_t last_used = 0;
	chan::chan *chptr;
	chan::membership *msptr;
	rb_dlink_node *ptr;
	char buf[414];
	char *p = buf, *end = buf + sizeof buf - 1;
	*p = '\0';

	/* Allow ircops to search for forwards to nonexistent channels */
	if(!IsOper(source_p))
	{
		if((chptr = chan::get(parv[1], std::nothrow)) == NULL || (msptr = get(chptr->members, *source_p, std::nothrow)) == NULL)
		{
			sendto_one_numeric(source_p, ERR_NOTONCHANNEL,
					form_str(ERR_NOTONCHANNEL), parv[1]);
			return;
		}

		if(!is_chanop(msptr))
		{
			sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
					me.name, source_p->name, parv[1]);
			return;
		}

		if((last_used + ConfigFileEntry.pace_wait) > rb_current_time())
		{
			sendto_one(source_p, form_str(RPL_LOAD2HI),
					me.name, source_p->name, "FINDFORWARDS");
			return;
		}
		else
			last_used = rb_current_time();
	}

	for(const auto &pit : chan::chans)
	{
		chptr = pit.second.get();
		if(!irccmp(chptr->mode.forward, parv[1]))
		{
			if(p + chptr->name.size() >= end - 13)
			{
				strcpy(p, "<truncated> ");
				p += 12;
				break;
			}
			strcpy(p, chptr->name.c_str());
			p += chptr->name.size();
			*p++ = ' ';
		}
	}

	if(buf[0])
		*(--p) = '\0';

	sendto_one_notice(source_p, ":Forwards for %s: %s", parv[1], buf);
}
