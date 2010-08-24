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
 *
 *  $Id: m_mode.c 1006 2006-03-09 15:32:14Z nenolod $
 */

#include "stdinc.h"
#include "channel.h"
#include "client.h"
#include "hash.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "s_user.h"
#include "s_conf.h"
#include "s_serv.h"
#include "logger.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "packet.h"
#include "s_newconf.h"

static int m_mode(struct Client *, struct Client *, int, const char **);
static int ms_mode(struct Client *, struct Client *, int, const char **);
static int ms_tmode(struct Client *, struct Client *, int, const char **);
static int ms_mlock(struct Client *, struct Client *, int, const char **);
static int ms_bmask(struct Client *, struct Client *, int, const char **);

struct Message mode_msgtab = {
	"MODE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_mode, 2}, {m_mode, 3}, {ms_mode, 3}, mg_ignore, {m_mode, 2}}
};
struct Message tmode_msgtab = {
	"TMODE", 0, 0, 0, MFLG_SLOW,
	{mg_ignore, mg_ignore, {ms_tmode, 4}, {ms_tmode, 4}, mg_ignore, mg_ignore}
};
struct Message mlock_msgtab = {
	"MLOCK", 0, 0, 0, MFLG_SLOW,
	{mg_ignore, mg_ignore, {ms_mlock, 3}, {ms_mlock, 3}, mg_ignore, mg_ignore}
};
struct Message bmask_msgtab = {
	"BMASK", 0, 0, 0, MFLG_SLOW,
	{mg_ignore, mg_ignore, mg_ignore, {ms_bmask, 5}, mg_ignore, mg_ignore}
};

mapi_clist_av1 mode_clist[] = { &mode_msgtab, &tmode_msgtab, &mlock_msgtab, &bmask_msgtab, NULL };

DECLARE_MODULE_AV1(mode, NULL, NULL, mode_clist, NULL, NULL, "$Revision: 1006 $");

/*
 * m_mode - MODE command handler
 * parv[1] - channel
 */
static int
m_mode(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Channel *chptr = NULL;
	struct membership *msptr;
	int n = 2;
	const char *dest;
	int operspy = 0;

	dest = parv[1];

	if(IsOperSpy(source_p) && *dest == '!')
	{
		dest++;
		operspy = 1;

		if(EmptyString(dest))
		{
			sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
				   me.name, source_p->name, "MODE");
			return 0;
		}
	}

	/* Now, try to find the channel in question */
	if(!IsChanPrefix(*dest))
	{
		/* if here, it has to be a non-channel name */
		user_mode(client_p, source_p, parc, parv);
		return 0;
	}

	if(!check_channel_name(dest))
	{
		sendto_one_numeric(source_p, ERR_BADCHANNAME, form_str(ERR_BADCHANNAME), parv[1]);
		return 0;
	}

	chptr = find_channel(dest);

	if(chptr == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), parv[1]);
		return 0;
	}

	/* Now know the channel exists */
	if(parc < n + 1)
	{
		if(operspy)
			report_operspy(source_p, "MODE", chptr->chname);

		sendto_one(source_p, form_str(RPL_CHANNELMODEIS),
			   me.name, source_p->name, parv[1],
			   operspy ? channel_modes(chptr, &me) : channel_modes(chptr, source_p));

		sendto_one(source_p, form_str(RPL_CREATIONTIME),
			   me.name, source_p->name, parv[1], chptr->channelts);
	}
	else
	{
		msptr = find_channel_membership(chptr, source_p);

		/* Finish the flood grace period... */
		if(MyClient(source_p) && !IsFloodDone(source_p))
		{
			if(!((parc == 3) && (parv[2][0] == 'b') && (parv[2][1] == '\0')))
				flood_endgrace(source_p);
		}

		set_channel_mode(client_p, source_p, chptr, msptr, parc - n, parv + n);
	}

	return 0;
}

static int
ms_mode(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Channel *chptr;

	chptr = find_channel(parv[1]);

	if(chptr == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), parv[1]);
		return 0;
	}

	set_channel_mode(client_p, source_p, chptr, NULL, parc - 2, parv + 2);

	return 0;
}

static int
ms_tmode(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Channel *chptr = NULL;
	struct membership *msptr;

	/* Now, try to find the channel in question */
	if(!IsChanPrefix(parv[2][0]) || !check_channel_name(parv[2]))
	{
		sendto_one_numeric(source_p, ERR_BADCHANNAME, form_str(ERR_BADCHANNAME), parv[2]);
		return 0;
	}

	chptr = find_channel(parv[2]);

	if(chptr == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), parv[2]);
		return 0;
	}

	/* TS is higher, drop it. */
	if(atol(parv[1]) > chptr->channelts)
		return 0;

	if(IsServer(source_p))
	{
		set_channel_mode(client_p, source_p, chptr, NULL, parc - 3, parv + 3);
	}
	else
	{
		msptr = find_channel_membership(chptr, source_p);

		set_channel_mode(client_p, source_p, chptr, msptr, parc - 3, parv + 3);
	}

	return 0;
}

static int
ms_mlock(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Channel *chptr = NULL;

	/* Now, try to find the channel in question */
	if(!IsChanPrefix(parv[2][0]) || !check_channel_name(parv[2]))
	{
		sendto_one_numeric(source_p, ERR_BADCHANNAME, form_str(ERR_BADCHANNAME), parv[2]);
		return 0;
	}

	chptr = find_channel(parv[2]);

	if(chptr == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), parv[2]);
		return 0;
	}

	/* TS is higher, drop it. */
	if(atol(parv[1]) > chptr->channelts)
		return 0;

	if(IsServer(source_p))
		set_channel_mlock(client_p, source_p, chptr, parv[3], TRUE);

	return 0;
}

static int
ms_bmask(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static char modebuf[BUFSIZE];
	static char parabuf[BUFSIZE];
	struct Channel *chptr;
	rb_dlink_list *banlist;
	const char *s;
	char *t;
	char *mbuf;
	char *pbuf;
	long mode_type;
	int mlen;
	int plen = 0;
	int tlen;
	int arglen;
	int modecount = 0;
	int needcap = NOCAPS;
	int mems;
	struct Client *fakesource_p;

	if(!IsChanPrefix(parv[2][0]) || !check_channel_name(parv[2]))
		return 0;

	if((chptr = find_channel(parv[2])) == NULL)
		return 0;

	/* TS is higher, drop it. */
	if(atol(parv[1]) > chptr->channelts)
		return 0;

	switch (parv[3][0])
	{
	case 'b':
		banlist = &chptr->banlist;
		mode_type = CHFL_BAN;
		mems = ALL_MEMBERS;
		break;

	case 'e':
		banlist = &chptr->exceptlist;
		mode_type = CHFL_EXCEPTION;
		needcap = CAP_EX;
		mems = ONLY_CHANOPS;
		break;

	case 'I':
		banlist = &chptr->invexlist;
		mode_type = CHFL_INVEX;
		needcap = CAP_IE;
		mems = ONLY_CHANOPS;
		break;

	case 'q':
		banlist = &chptr->quietlist;
		mode_type = CHFL_QUIET;
		mems = ALL_MEMBERS;
		break;

		/* maybe we should just blindly propagate this? */
	default:
		return 0;
	}

	parabuf[0] = '\0';
	s = LOCAL_COPY(parv[4]);

	/* Hide connecting server on netburst -- jilles */
	if (ConfigServerHide.flatten_links && !HasSentEob(source_p))
		fakesource_p = &me;
	else
		fakesource_p = source_p;
	mlen = rb_sprintf(modebuf, ":%s MODE %s +", fakesource_p->name, chptr->chname);
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
		if(tlen > MODEBUFLEN)
			break;

		if(add_id(fakesource_p, chptr, s, banlist, mode_type))
		{
			/* this new one wont fit.. */
			if(mlen + MAXMODEPARAMS + plen + tlen > BUFSIZE - 5 ||
			   modecount >= MAXMODEPARAMS)
			{
				*mbuf = '\0';
				*(pbuf - 1) = '\0';
				sendto_channel_local(mems, chptr, "%s %s", modebuf, parabuf);
				sendto_server(client_p, chptr, needcap, CAP_TS6,
					      "%s %s", modebuf, parabuf);

				mbuf = modebuf + mlen;
				pbuf = parabuf;
				plen = modecount = 0;
			}

			*mbuf++ = parv[3][0];
			arglen = rb_sprintf(pbuf, "%s ", s);
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
		sendto_server(client_p, chptr, needcap, CAP_TS6, "%s %s", modebuf, parabuf);
	}

	sendto_server(client_p, chptr, CAP_TS6 | needcap, NOCAPS, ":%s BMASK %ld %s %s :%s",
		      source_p->id, (long) chptr->channelts, chptr->chname, parv[3], parv[4]);
	return 0;
}

