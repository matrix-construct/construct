/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_knock.c: Requests to be invited to a channel.
 *
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

static const char knock_desc[] = "Provides the KNOCK command to ask for an invite to an invite-only channel";

static void m_knock(struct MsgBuf *, client::client &, client::client &, int, const char **);

struct Message knock_msgtab = {
	"KNOCK", 0, 0, 0, 0,
	{mg_unreg, {m_knock, 2}, {m_knock, 2}, mg_ignore, mg_ignore, {m_knock, 2}}
};

static int
_modinit(void)
{
	add_isupport("KNOCK", isupport_boolean, &ConfigChannel.use_knock);
	return 0;
}

static void
_moddeinit(void)
{
	delete_isupport("KNOCK");
}

mapi_clist_av1 knock_clist[] = { &knock_msgtab, NULL };

DECLARE_MODULE_AV2(knock, _modinit, _moddeinit, knock_clist, NULL, NULL, NULL, NULL, knock_desc);

/* m_knock
 *    parv[1] = channel
 *
 *  The KNOCK command has the following syntax:
 *   :<sender> KNOCK <channel>
 *
 *  If a user is not banned from the channel they can use the KNOCK
 *  command to have the server NOTICE the channel operators notifying
 *  they would like to join.  Helpful if the channel is invite-only, the
 *  key is forgotten, or the channel is full (INVITE can bypass each one
 *  of these conditions.  Concept by Dianora <db@db.net> and written by
 *  <anonymous>
 */
static void
m_knock(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	chan::chan *chptr;
	char *p, *name;

	if(my(source) && ConfigChannel.use_knock == 0)
	{
		sendto_one(&source, form_str(ERR_KNOCKDISABLED),
			   me.name, source.name);
		return;
	}

	name = LOCAL_COPY(parv[1]);

	/* dont allow one knock to multiple chans */
	if((p = strchr(name, ',')))
		*p = '\0';

	if((chptr = chan::get(name, std::nothrow)) == NULL)
	{
		sendto_one_numeric(&source, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), name);
		return;
	}

	if(is_member(chptr, &source))
	{
		if(my(source))
			sendto_one(&source, form_str(ERR_KNOCKONCHAN),
				   me.name, source.name, name);
		return;
	}

	if(!((chptr->mode.mode & chan::mode::INVITEONLY) || (*chptr->mode.key) ||
	     (chptr->mode.limit &&
	      size(chptr->members) >= (unsigned long)chptr->mode.limit)))
	{
		sendto_one_numeric(&source, ERR_CHANOPEN,
				   form_str(ERR_CHANOPEN), name);
		return;
	}

	/* cant knock to a +p channel */
	if(is_hidden(chptr))
	{
		sendto_one_numeric(&source, ERR_CANNOTSENDTOCHAN,
				   form_str(ERR_CANNOTSENDTOCHAN), name);
		return;
	}


	if(my(source))
	{
		// don't allow a knock if the user is banned
		if (check(*chptr, chan::mode::BAN, source) ||
		    check(*chptr, chan::mode::QUIET, source))
		{
			sendto_one_numeric(&source, ERR_CANNOTSENDTOCHAN, form_str(ERR_CANNOTSENDTOCHAN), name);
			return;
		}

		/* local flood protection:
		 * allow one knock per user per knock_delay
		 * allow one knock per channel per knock_delay_channel
		 */
		if(!IsOper(&source) &&
		   (source.localClient->last_knock + ConfigChannel.knock_delay) > rb_current_time())
		{
			sendto_one(&source, form_str(ERR_TOOMANYKNOCK),
					me.name, source.name, name, "user");
			return;
		}
		else if((chptr->last_knock + ConfigChannel.knock_delay_channel) > rb_current_time())
		{
			sendto_one(&source, form_str(ERR_TOOMANYKNOCK),
					me.name, source.name, name, "channel");
			return;
		}

		/* ok, we actually can send the knock, tell client */
		source.localClient->last_knock = rb_current_time();

		sendto_one(&source, form_str(RPL_KNOCKDLVR),
			   me.name, source.name, name);
	}

	chptr->last_knock = rb_current_time();

	if(ConfigChannel.use_knock)
		sendto_channel_local(chptr->mode.mode & chan::mode::FREEINVITE ? chan::ALL_MEMBERS : chan::ONLY_CHANOPS,
				     chptr, form_str(RPL_KNOCK),
				     me.name, name, name, source.name,
				     source.username, source.host);

	sendto_server(&client, chptr, CAP_KNOCK|CAP_TS6, NOCAPS,
		      ":%s KNOCK %s", use_id(&source), name);
	sendto_server(&client, chptr, CAP_KNOCK, CAP_TS6,
		      ":%s KNOCK %s", source.name, name);
}

