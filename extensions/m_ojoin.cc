/*   contrib/m_ojoin.c
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

static const char ojoin_desc[] = "Allow admins to forcibly join channels with the OJOIN command";

static void mo_ojoin(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[]);

struct Message ojoin_msgtab = {
	"OJOIN", 0, 0, 0, 0,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_ojoin, 2}}
};

mapi_clist_av1 ojoin_clist[] = { &ojoin_msgtab, NULL };

DECLARE_MODULE_AV2(ojoin, NULL, NULL, ojoin_clist, NULL, NULL, NULL, NULL, ojoin_desc);

/*
** mo_ojoin
**      parv[1] = channel
*/
static void
mo_ojoin(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	chan::chan *chptr;
	int move_me = 0;

	/* admins only */
	if(!IsOperAdmin(&source))
	{
		sendto_one(&source, form_str(ERR_NOPRIVS), me.name, source.name, "admin");
		return;
	}

	if(*parv[1] == '@' || *parv[1] == '+')
	{
		parv[1]++;
		move_me = 1;
	}

	if((chptr = chan::get(parv[1], std::nothrow)) == NULL)
	{
		sendto_one_numeric(&source, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), parv[1]);
		return;
	}

	if(is_member(chptr, &source))
	{
		sendto_one_notice(&source, ":Please part %s before using OJOIN", parv[1]);
		return;
	}

	if(move_me == 1)
		parv[1]--;

	sendto_wallops_flags(UMODE_WALLOP, &me,
			     "OJOIN called for %s by %s!%s@%s",
			     parv[1], source.name, source.username, source.host);
	ilog(L_MAIN, "OJOIN called for %s by %s",
	     parv[1], get_oper_name(&source));
	/* only sends stuff for #channels remotely */
	sendto_server(NULL, chptr, NOCAPS, NOCAPS,
			":%s WALLOPS :OJOIN called for %s by %s!%s@%s",
			me.name, parv[1],
			source.name, source.username, source.host);

	if(*parv[1] == '@')
	{
		add(*chptr, source, chan::CHANOP);
		sendto_server(&client, chptr, CAP_TS6, NOCAPS,
			      ":%s SJOIN %ld %s + :@%s",
			      me.id, (long) chptr->channelts, chptr->name.c_str(), source.id);
		send_join(*chptr, source);
		sendto_channel_local(chan::ALL_MEMBERS, chptr, ":%s MODE %s +o %s",
				     me.name, chptr->name.c_str(), source.name);

	}
	else if(*parv[1] == '+')
	{
		add(*chptr, source, chan::VOICE);
		sendto_server(&client, chptr, CAP_TS6, NOCAPS,
			      ":%s SJOIN %ld %s + :+%s",
			      me.id, (long) chptr->channelts, chptr->name.c_str(), source.id);
		send_join(*chptr, source);
		sendto_channel_local(chan::ALL_MEMBERS, chptr, ":%s MODE %s +v %s",
				     me.name, chptr->name.c_str(), source.name);
	}
	else
	{
		add(*chptr, source, chan::PEON);
		sendto_server(&client, chptr, CAP_TS6, NOCAPS,
			      ":%s JOIN %ld %s +",
			      source.id, (long) chptr->channelts, chptr->name.c_str());
		send_join(*chptr, source);
	}

	/* send the topic... */
	if(chptr->topic)
	{
		sendto_one(&source, form_str(RPL_TOPIC), me.name,
			   source.name, chptr->name.c_str(), chptr->topic.text.c_str());
		sendto_one(&source, form_str(RPL_TOPICWHOTIME), me.name,
			   source.name, chptr->name.c_str(), chptr->topic.info.c_str(), chptr->topic.time);
	}

	source.localClient->last_join_time = rb_current_time();
	channel_member_names(chptr, &source, 1);
}
