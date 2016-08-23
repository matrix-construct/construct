/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_mode.c: Sets a user or channel mode.
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

static const char mode_desc[] =
	"Provides the MODE and MLOCK client and server commands, and TS6 server-to-server TMODE and BMASK commands";

static void m_mode(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void ms_mode(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void ms_tmode(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void ms_mlock(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void ms_bmask(struct MsgBuf *, client::client &, client::client &, int, const char **);

struct Message mode_msgtab = {
	"MODE", 0, 0, 0, 0,
	{mg_unreg, {m_mode, 2}, {m_mode, 3}, {ms_mode, 3}, mg_ignore, {m_mode, 2}}
};
struct Message tmode_msgtab = {
	"TMODE", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, {ms_tmode, 4}, {ms_tmode, 4}, mg_ignore, mg_ignore}
};
struct Message mlock_msgtab = {
	"MLOCK", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, {ms_mlock, 3}, {ms_mlock, 3}, mg_ignore, mg_ignore}
};
struct Message bmask_msgtab = {
	"BMASK", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, {ms_bmask, 5}, mg_ignore, mg_ignore}
};

mapi_clist_av1 mode_clist[] = { &mode_msgtab, &tmode_msgtab, &mlock_msgtab, &bmask_msgtab, NULL };

DECLARE_MODULE_AV2(mode, NULL, NULL, mode_clist, NULL, NULL, NULL, NULL, mode_desc);

/*
 * m_mode - MODE command handler
 * parv[1] - channel
 */
static void
m_mode(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	chan::chan *chptr = NULL;
	chan::membership *msptr;
	int n = 2;
	const char *dest;
	int operspy = 0;

	dest = parv[1];

	if(IsOperSpy(&source) && *dest == '!')
	{
		dest++;
		operspy = 1;

		if(EmptyString(dest))
		{
			sendto_one(&source, form_str(ERR_NEEDMOREPARAMS),
				   me.name, source.name, "MODE");
			return;
		}
	}

	/* Now, try to find the channel in question */
	if(!rfc1459::is_chan_prefix(*dest))
	{
		/* if here, it has to be a non-channel name */
		user_mode(&client, &source, parc, parv);
		return;
	}

	if(!chan::valid_name(dest))
	{
		sendto_one_numeric(&source, ERR_BADCHANNAME, form_str(ERR_BADCHANNAME), parv[1]);
		return;
	}

	chptr = chan::get(dest, std::nothrow);

	if(chptr == NULL)
	{
		sendto_one_numeric(&source, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), parv[1]);
		return;
	}

	/* Now know the channel exists */
	if(parc < n + 1)
	{
		if(operspy)
			report_operspy(&source, "MODE", chptr->name.c_str());

		sendto_one(&source, form_str(RPL_CHANNELMODEIS),
			   me.name, source.name, parv[1],
			   operspy ? channel_modes(chptr, &me) : channel_modes(chptr, &source));

		sendto_one(&source, form_str(RPL_CREATIONTIME),
			   me.name, source.name, parv[1], chptr->channelts);
	}
	else
	{
		msptr = get(chptr->members, source, std::nothrow);

		/* Finish the flood grace period... */
		if(MyClient(&source) && !IsFloodDone(&source))
		{
			if(!((parc == 3) && (parv[2][0] == 'b' || parv[2][0] == 'q') && (parv[2][1] == '\0')))
				flood_endgrace(&source);
		}

		set_channel_mode(&client, &source, chptr, msptr, parc - n, parv + n);
	}
}

static void
ms_mode(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	chan::chan *chptr;

	chptr = chan::get(parv[1], std::nothrow);

	if(chptr == NULL)
	{
		sendto_one_numeric(&source, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), parv[1]);
		return;
	}

	set_channel_mode(&client, &source, chptr, NULL, parc - 2, parv + 2);
}

static void
ms_tmode(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	chan::chan *chptr = NULL;
	chan::membership *msptr;

	/* Now, try to find the channel in question */
	if(!rfc1459::is_chan_prefix(parv[2][0]) || !chan::valid_name(parv[2]))
	{
		sendto_one_numeric(&source, ERR_BADCHANNAME, form_str(ERR_BADCHANNAME), parv[2]);
		return;
	}

	chptr = chan::get(parv[2], std::nothrow);

	if(chptr == NULL)
	{
		sendto_one_numeric(&source, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), parv[2]);
		return;
	}

	/* TS is higher, drop it. */
	if(atol(parv[1]) > chptr->channelts)
		return;

	if(IsServer(&source))
	{
		set_channel_mode(&client, &source, chptr, NULL, parc - 3, parv + 3);
	}
	else
	{
		msptr = get(chptr->members, source, std::nothrow);

		set_channel_mode(&client, &source, chptr, msptr, parc - 3, parv + 3);
	}
}

static void
ms_mlock(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	chan::chan *chptr = NULL;

	/* Now, try to find the channel in question */
	if(!rfc1459::is_chan_prefix(parv[2][0]) || !chan::valid_name(parv[2]))
	{
		sendto_one_numeric(&source, ERR_BADCHANNAME, form_str(ERR_BADCHANNAME), parv[2]);
		return;
	}

	chptr = chan::get(parv[2], std::nothrow);

	if(chptr == NULL)
	{
		sendto_one_numeric(&source, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), parv[2]);
		return;
	}

	/* TS is higher, drop it. */
	if(atol(parv[1]) > chptr->channelts)
		return;

	if(IsServer(&source))
		set_channel_mlock(&client, &source, chptr, parv[3], true);
}

static void
possibly_remove_lower_forward(client::client *fakesource,
                              const int &mems,
                              chan::chan &chan,
                              chan::list &list,
                              const int &mchar,
                              const char *mask,
                              const char *forward)
{
	for (auto it(begin(list)); it != end(list); ++it)
	{
		const auto &ban(*it);

		if (irccmp(ban.banstr, mask) != 0)
			continue;

		if (ban.forward.size() && irccmp(ban.forward, forward) >= 0)
			continue;

		sendto_channel_local(mems, &chan,
		                     ":%s MODE %s -%c %s%s%s",
		                     fakesource->name,
		                     chan.name.c_str(),
		                     mchar,
		                     ban.banstr.c_str(),
		                     ban.forward.size()? "$" : "",
		                     ban.forward.size()? ban.forward.c_str() : "");
		list.erase(it);
		break;
	}
}

static void
ms_bmask(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	static char modebuf[BUFSIZE];
	static char parabuf[BUFSIZE];
	chan::chan *chptr;
	chan::list *list;
	chan::mode::type mode_type;
	char *s, *forward;
	char *t;
	char *mbuf;
	char *pbuf;
	int mlen;
	int plen = 0;
	int tlen;
	int arglen;
	int modecount = 0;
	int needcap = NOCAPS;
	int mems;
	client::client *fakesource;

	if(!rfc1459::is_chan_prefix(parv[2][0]) || !chan::valid_name(parv[2]))
		return;

	if((chptr = chan::get(parv[2], std::nothrow)) == NULL)
		return;

	/* TS is higher, drop it. */
	if(atol(parv[1]) > chptr->channelts)
		return;

	switch (parv[3][0])
	{
	case 'b':
		mode_type = chan::mode::BAN;
		mems = chan::ALL_MEMBERS;
		break;

	case 'e':
		mode_type = chan::mode::EXCEPTION;
		needcap = CAP_EX;
		mems = chan::ONLY_CHANOPS;
		break;

	case 'I':
		mode_type = chan::mode::INVEX;
		needcap = CAP_IE;
		mems = chan::ONLY_CHANOPS;
		break;

	case 'q':
		mode_type = chan::mode::QUIET;
		mems = chan::ALL_MEMBERS;
		break;

		/* maybe we should just blindly propagate this? */
	default:
		return;
	}

	list = &get(*chptr, mode_type);

	parabuf[0] = '\0';
	s = LOCAL_COPY(parv[4]);

	/* Hide connecting server on netburst -- jilles */
	if (ConfigServerHide.flatten_links && !HasSentEob(&source))
		fakesource = &me;
	else
		fakesource = &source;
	mlen = sprintf(modebuf, ":%s MODE %s +", fakesource->name, chptr->name.c_str());
	mbuf = modebuf + mlen;
	pbuf = parabuf;

	while(*s == ' ')
		s++;

	/* next char isnt a space, point t to the next one */
	if((t = strchr(s, ' ')) != NULL)
	{
		*t++ = '\0';

		/* double spaces will break the parser */
		while(*t == ' ')
			t++;
	}

	/* couldve skipped spaces and got nothing.. */
	while(!EmptyString(s))
	{
		/* ban with a leading ':' -- this will break the protocol */
		if(*s == ':')
			goto nextban;

		tlen = strlen(s);

		/* I dont even want to begin parsing this.. */
		if(tlen > chan::mode::BUFLEN)
			break;

		if((forward = strchr(s+1, '$')) != NULL)
		{
			*forward++ = '\0';
			if(*forward == '\0')
				tlen--, forward = NULL;
			else
				possibly_remove_lower_forward(fakesource,
						mems, *chptr, *list,
						parv[3][0], s, forward);
		}

		if (add(*chptr, mode_type, s, *fakesource, forward))
		{
			/* this new one wont fit.. */
			if(mlen + chan::mode::MAXPARAMS + plen + tlen > BUFSIZE - 5 ||
			   modecount >= chan::mode::MAXPARAMS)
			{
				*mbuf = '\0';
				*(pbuf - 1) = '\0';
				sendto_channel_local(mems, chptr, "%s %s", modebuf, parabuf);

				mbuf = modebuf + mlen;
				pbuf = parabuf;
				plen = modecount = 0;
			}

			if (forward != NULL)
				forward[-1] = '$';

			*mbuf++ = parv[3][0];
			arglen = sprintf(pbuf, "%s ", s);
			pbuf += arglen;
			plen += arglen;
			modecount++;
		}

	      nextban:
		s = t;

		if(s != NULL)
		{
			if((t = strchr(s, ' ')) != NULL)
			{
				*t++ = '\0';

				while(*t == ' ')
					t++;
			}
		}
	}

	if(modecount)
	{
		*mbuf = '\0';
		*(pbuf - 1) = '\0';
		sendto_channel_local(mems, chptr, "%s %s", modebuf, parabuf);
	}

	sendto_server(&client, chptr, CAP_TS6 | needcap, NOCAPS, ":%s BMASK %ld %s %s :%s",
		      source.id, (long) chptr->channelts, chptr->name.c_str(), parv[3], parv[4]);
}
