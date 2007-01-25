/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_join.c: Joins a channel.
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
 *  $Id: m_join.c 3131 2007-01-21 15:36:31Z jilles $
 */

#include "stdinc.h"
#include "tools.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "s_serv.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "sprintf_irc.h"
#include "packet.h"

static int m_join(struct Client *, struct Client *, int, const char **);
static int ms_join(struct Client *, struct Client *, int, const char **);

static int h_can_create_channel;
static int h_channel_join;

struct Message join_msgtab = {
	"JOIN", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_join, 2}, {ms_join, 2}, mg_ignore, mg_ignore, {m_join, 2}}
};

mapi_clist_av1 join_clist[] = { &join_msgtab, NULL };

mapi_hlist_av1 join_hlist[] = {
	{ "can_create_channel", &h_can_create_channel },
	{ "channel_join", &h_channel_join },
	{ NULL, NULL },
};

DECLARE_MODULE_AV1(join, NULL, NULL, join_clist, join_hlist, NULL, "$Revision: 3131 $");

static void do_join_0(struct Client *client_p, struct Client *source_p);
static int check_channel_name_loc(struct Client *source_p, const char *name);

static void set_final_mode(struct Mode *mode, struct Mode *oldmode);
static void remove_our_modes(struct Channel *chptr, struct Client *source_p);

static char modebuf[MODEBUFLEN];
static char parabuf[MODEBUFLEN];
static char *mbuf;

/* Check what we will forward to, without sending any notices to the user
 * -- jilles
 */
static struct Channel *
check_forward(struct Client *source_p, struct Channel *chptr,
		char *key)
{
	int depth = 0, i;

	/* User is +Q */
	if (IsNoForward(source_p))
		return NULL;

	while (depth < 16)
	{
		chptr = find_channel(chptr->mode.forward);
		/* Can only forward to existing channels */
		if (chptr == NULL)
			return NULL;
		/* Already on there, show original error message */
		if (IsMember(source_p, chptr))
			return NULL;
		/* Juped. Sending a warning notice would be unfair */
		if (hash_find_resv(chptr->chname))
			return NULL;
		/* Don't forward to +Q channel */
		if (chptr->mode.mode & MODE_DISFORWARD)
			return NULL;
		i = can_join(source_p, chptr, key);
		if (i == 0)
			return chptr;
		if (i != ERR_INVITEONLYCHAN && i != ERR_NEEDREGGEDNICK && i != ERR_THROTTLE && i != ERR_CHANNELISFULL)
			return NULL;
		depth++;
	}

	return NULL;
}

/*
 * m_join
 *      parv[0] = sender prefix
 *      parv[1] = channel
 *      parv[2] = channel password (key)
 */
static int
m_join(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static char jbuf[BUFSIZE];
	struct Channel *chptr = NULL;
	struct ConfItem *aconf;
	char *name;
	char *key = NULL;
	int i, flags = 0;
	char *p = NULL, *p2 = NULL;
	char *chanlist;
	char *mykey;
	int successful_join_count = 0;	/* Number of channels successfully joined */

	jbuf[0] = '\0';

	/* rebuild the list of channels theyre supposed to be joining.
	 * this code has a side effect of losing keys, but..
	 */
	chanlist = LOCAL_COPY(parv[1]);
	for(name = strtoken(&p, chanlist, ","); name; name = strtoken(&p, NULL, ","))
	{
		/* check the length and name of channel is ok */
		if(!check_channel_name_loc(source_p, name) || (strlen(name) > LOC_CHANNELLEN))
		{
			sendto_one_numeric(source_p, ERR_BADCHANNAME,
					   form_str(ERR_BADCHANNAME), (unsigned char *) name);
			continue;
		}

		/* join 0 parts all channels */
		if(*name == '0' && !atoi(name))
		{
			(void) strcpy(jbuf, "0");
			continue;
		}

		/* check it begins with # or &, and local chans are disabled */
		else if(!IsChannelName(name))
		{
			sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
					   form_str(ERR_NOSUCHCHANNEL), name);
			continue;
		}

		/* see if its resv'd */
		if(!IsExemptResv(source_p) && (aconf = hash_find_resv(name)))
		{
			sendto_one_numeric(source_p, ERR_BADCHANNAME,
					   form_str(ERR_BADCHANNAME), name);

			/* dont warn for opers */
			if(!IsExemptJupe(source_p) && !IsOper(source_p))
				sendto_realops_snomask(SNO_SPY, L_NETWIDE,
						     "User %s (%s@%s) is attempting to join locally juped channel %s (%s)",
						     source_p->name, source_p->username,
						     source_p->host, name, aconf->passwd);
			/* dont update tracking for jupe exempt users, these
			 * are likely to be spamtrap leaves
			 */
			else if(IsExemptJupe(source_p))
				aconf->port--;

			continue;
		}

		if(splitmode && !IsOper(source_p) && (*name != '&') &&
		   ConfigChannel.no_join_on_split)
		{
			sendto_one(source_p, form_str(ERR_UNAVAILRESOURCE),
				   me.name, source_p->name, name);
			continue;
		}

		if(*jbuf)
			(void) strcat(jbuf, ",");
		(void) strlcat(jbuf, name, sizeof(jbuf));
	}

	if(parc > 2)
	{
		mykey = LOCAL_COPY(parv[2]);
		key = strtoken(&p2, mykey, ",");
	}

	for(name = strtoken(&p, jbuf, ","); name;
	    key = (key) ? strtoken(&p2, NULL, ",") : NULL, name = strtoken(&p, NULL, ","))
	{
		hook_data_channel_activity hook_info;

		/* JOIN 0 simply parts all channels the user is in */
		if(*name == '0' && !atoi(name))
		{
			if(source_p->user->channel.head == NULL)
				continue;

			do_join_0(&me, source_p);
			continue;
		}

		/* look for the channel */
		if((chptr = find_channel(name)) != NULL)
		{
			if(IsMember(source_p, chptr))
				continue;

			flags = 0;
		}
		else
		{
			hook_data_client_approval moduledata;

			moduledata.client = source_p;
			moduledata.approved = 0;

			call_hook(h_can_create_channel, &moduledata);

			if(moduledata.approved != 0 && !IsOper(source_p))
			{
				sendto_one(source_p, form_str(moduledata.approved),
					   me.name, source_p->name, name);
				continue;
			}

			if(splitmode && !IsOper(source_p) && (*name != '&') &&
			   ConfigChannel.no_create_on_split)
			{
				sendto_one(source_p, form_str(ERR_UNAVAILRESOURCE),
					   me.name, source_p->name, name);
				continue;
			}

			flags = CHFL_CHANOP;
		}

		if((dlink_list_length(&source_p->user->channel) >=
		    (unsigned long) ConfigChannel.max_chans_per_user) &&
		   (!IsOper(source_p) ||
		    (dlink_list_length(&source_p->user->channel) >=
		     (unsigned long) ConfigChannel.max_chans_per_user * 3)))
		{
			sendto_one(source_p, form_str(ERR_TOOMANYCHANNELS),
				   me.name, source_p->name, name);
			if(successful_join_count)
				source_p->localClient->last_join_time = CurrentTime;
			return 0;
		}

		if(flags == 0)	/* if channel doesn't exist, don't penalize */
			successful_join_count++;

		if(chptr == NULL)	/* If I already have a chptr, no point doing this */
		{
			chptr = get_or_create_channel(source_p, name, NULL);

			if(chptr == NULL)
			{
				sendto_one(source_p, form_str(ERR_UNAVAILRESOURCE),
					   me.name, source_p->name, name);
				if(successful_join_count > 0)
					successful_join_count--;
				continue;
			}
		}

		if(!IsOper(source_p) && !IsExemptSpambot(source_p))
			check_spambot_warning(source_p, name);

		/* can_join checks for +i key, bans etc */
		if((i = can_join(source_p, chptr, key)))
		{
			if ((i != ERR_NEEDREGGEDNICK && i != ERR_THROTTLE && i != ERR_INVITEONLYCHAN && i != ERR_CHANNELISFULL) ||
			    (!ConfigChannel.use_forward || (chptr = check_forward(source_p, chptr, key)) == NULL))
			{
				sendto_one(source_p, form_str(i), me.name, source_p->name, name);
				if(successful_join_count > 0)
					successful_join_count--;
				continue;
			}
			sendto_one_numeric(source_p, ERR_LINKCHANNEL, form_str(ERR_LINKCHANNEL), name, chptr->chname);
		}

		/* add the user to the channel */
		add_user_to_channel(chptr, source_p, flags);
		if (chptr->mode.join_num &&
			CurrentTime - chptr->join_delta >= chptr->mode.join_time)
		{
			chptr->join_count = 0;
			chptr->join_delta = CurrentTime;
		}
		chptr->join_count++;

		/* we send the user their join here, because we could have to
		 * send a mode out next.
		 */
		sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s JOIN :%s",
				     source_p->name,
				     source_p->username, source_p->host, chptr->chname);

		/* its a new channel, set +nt and burst. */
		if(flags & CHFL_CHANOP)
		{
			chptr->channelts = CurrentTime;
			chptr->mode.mode |= MODE_TOPICLIMIT;
			chptr->mode.mode |= MODE_NOPRIVMSGS;

			sendto_channel_local(ONLY_CHANOPS, chptr, ":%s MODE %s +nt",
					     me.name, chptr->chname);

			if(*chptr->chname == '#')
			{
				sendto_server(client_p, chptr, CAP_TS6, NOCAPS,
					      ":%s SJOIN %ld %s +nt :@%s",
					      me.id, (long) chptr->channelts,
					      chptr->chname, source_p->id);
				sendto_server(client_p, chptr, NOCAPS, CAP_TS6,
					      ":%s SJOIN %ld %s +nt :@%s",
					      me.name, (long) chptr->channelts,
					      chptr->chname, source_p->name);
			}
		}
		else
		{
			const char *modes = channel_modes(chptr, &me);

			sendto_server(client_p, chptr, CAP_TS6, NOCAPS,
				      ":%s JOIN %ld %s %s",
				      use_id(source_p), (long) chptr->channelts,
				      chptr->chname, modes);

			sendto_server(client_p, chptr, NOCAPS, CAP_TS6,
				      ":%s SJOIN %ld %s %s :%s",
				      me.name, (long) chptr->channelts,
				      chptr->chname, modes, source_p->name);
		}

		del_invite(chptr, source_p);

		if(chptr->topic != NULL)
		{
			sendto_one(source_p, form_str(RPL_TOPIC), me.name,
				   source_p->name, chptr->chname, chptr->topic);

			sendto_one(source_p, form_str(RPL_TOPICWHOTIME),
				   me.name, source_p->name, chptr->chname,
				   chptr->topic_info, chptr->topic_time);
		}

		channel_member_names(chptr, source_p, 1);

		if(successful_join_count)
			source_p->localClient->last_join_time = CurrentTime;

		hook_info.client = source_p;
		hook_info.chptr = chptr;
		hook_info.key = key;
		call_hook(h_channel_join, &hook_info);
	}

	return 0;
}

/*
 * ms_join
 *
 * inputs	-
 * output	- none
 * side effects	- handles remote JOIN's sent by servers. In TSora
 *		  remote clients are joined using SJOIN, hence a 
 *		  JOIN sent by a server on behalf of a client is an error.
 *		  here, the initial code is in to take an extra parameter
 *		  and use it for the TimeStamp on a new channel.
 */
static int
ms_join(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Channel *chptr;
	static struct Mode mode, *oldmode;
	const char *s;
	const char *modes;
	time_t oldts;
	time_t newts;
	int isnew;
	int args = 0;
	int keep_our_modes = YES;
	int keep_new_modes = YES;
	dlink_node *ptr, *next_ptr;

	/* special case for join 0 */
	if((parv[1][0] == '0') && (parv[1][1] == '\0') && parc == 2)
	{
		do_join_0(client_p, source_p);
		return 0;
	}

	if(parc < 4)
		return 0;

	if(!IsChannelName(parv[2]) || !check_channel_name(parv[2]))
		return 0;

	/* joins for local channels cant happen. */
	if(parv[2][0] == '&')
		return 0;

	mbuf = modebuf;
	mode.key[0] = mode.forward[0] = '\0';
	mode.mode = mode.limit = mode.join_num = mode.join_time = 0;

	s = parv[3];
	while(*s)
	{
		switch (*(s++))
		{
		case 'i':
			mode.mode |= MODE_INVITEONLY;
			break;
		case 'n':
			mode.mode |= MODE_NOPRIVMSGS;
			break;
		case 'p':
			mode.mode |= MODE_PRIVATE;
			break;
		case 's':
			mode.mode |= MODE_SECRET;
			break;
		case 'm':
			mode.mode |= MODE_MODERATED;
			break;
		case 't':
			mode.mode |= MODE_TOPICLIMIT;
			break;
		case 'r':
			mode.mode |= MODE_REGONLY;
			break;
		case 'L':
			mode.mode |= MODE_EXLIMIT;
			break;
		case 'P':
			mode.mode |= MODE_PERMANENT;
			break;
		case 'c':
			mode.mode |= MODE_NOCOLOR;
			break;
		case 'g':
			mode.mode |= MODE_FREEINVITE;
			break;
		case 'z':
			mode.mode |= MODE_OPMODERATE;
			break;
		case 'F':
			mode.mode |= MODE_FREETARGET;
			break;
		case 'Q':
			mode.mode |= MODE_DISFORWARD;
			break;
		case 'f':
			if(parc < 5 + args)
				return 0;
			strlcpy(mode.forward, parv[4 + args], sizeof(mode.forward));
			args++;
			break;
		case 'j':
			/* sent a +j without an arg. */
			if(parc < 5 + args)
				return 0;
			sscanf(parv[4 + args], "%d:%d", &mode.join_num, &mode.join_time);
			args++;
			break;
		case 'k':
			/* sent a +k without a key, eek. */
			if(parc < 5 + args)
				return 0;
			strlcpy(mode.key, parv[4 + args], sizeof(mode.key));
			args++;
			break;
		case 'l':
			/* sent a +l without a limit. */
			if(parc < 5 + args)
				return 0;
			mode.limit = atoi(parv[4 + args]);
			args++;
			break;
		}
	}

	if((chptr = get_or_create_channel(source_p, parv[2], &isnew)) == NULL)
		return 0;

	newts = atol(parv[1]);
	oldts = chptr->channelts;
	oldmode = &chptr->mode;

#ifdef IGNORE_BOGUS_TS
	if(newts < 800000000)
	{
		sendto_realops_snomask(SNO_DEBUG, L_ALL,
				     "*** Bogus TS %ld on %s ignored from %s",
				     (long) newts, chptr->chname, client_p->name);
		newts = (oldts == 0) ? oldts : 800000000;
	}
#else
	/* making a channel TS0 */
	if(!isnew && !newts && oldts)
	{
		sendto_channel_local(ALL_MEMBERS, chptr,
				     ":%s NOTICE %s :*** Notice -- TS for %s changed from %ld to 0",
				     me.name, chptr->chname, chptr->chname, (long) oldts);
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "Server %s changing TS on %s from %ld to 0",
				     source_p->name, chptr->chname, (long) oldts);
	}
#endif

	if(isnew)
		chptr->channelts = newts;
	else if(newts == 0 || oldts == 0)
		chptr->channelts = 0;
	else if(newts == oldts)
		;
	else if(newts < oldts)
	{
		keep_our_modes = NO;
		chptr->channelts = newts;
	}
	else
		keep_new_modes = NO;

	if(!keep_new_modes)
		mode = *oldmode;
	else if(keep_our_modes)
	{
		mode.mode |= oldmode->mode;
		if(oldmode->limit > mode.limit)
			mode.limit = oldmode->limit;
		if(strcmp(mode.key, oldmode->key) < 0)
			strcpy(mode.key, oldmode->key);
		if(oldmode->join_num > mode.join_num ||
				(oldmode->join_num == mode.join_num &&
				 oldmode->join_time > mode.join_time))
		{
			mode.join_num = oldmode->join_num;
			mode.join_time = oldmode->join_time;
		}
		if(irccmp(mode.forward, oldmode->forward) < 0)
			strcpy(mode.forward, oldmode->forward);
	}
	else
	{
		/* If setting -j, clear join throttle state -- jilles */
		if (!mode.join_num)
			chptr->join_count = chptr->join_delta = 0;
	}

	set_final_mode(&mode, oldmode);
	chptr->mode = mode;

	/* Lost the TS, other side wins, so remove modes on this side */
	if(!keep_our_modes)
	{
		remove_our_modes(chptr, source_p);
		DLINK_FOREACH_SAFE(ptr, next_ptr, chptr->invites.head)
		{
			del_invite(chptr, ptr->data);
		}
		sendto_channel_local(ALL_MEMBERS, chptr,
				     ":%s NOTICE %s :*** Notice -- TS for %s changed from %ld to %ld",
				     me.name, chptr->chname, chptr->chname,
				     (long) oldts, (long) newts);
	}

	if(*modebuf != '\0')
		sendto_channel_local(ALL_MEMBERS, chptr, ":%s MODE %s %s %s",
				     source_p->user->server, chptr->chname, modebuf, parabuf);

	*modebuf = *parabuf = '\0';

	if(!IsMember(source_p, chptr))
	{
		add_user_to_channel(chptr, source_p, CHFL_PEON);
		if (chptr->mode.join_num &&
			CurrentTime - chptr->join_delta >= chptr->mode.join_time)
		{
			chptr->join_count = 0;
			chptr->join_delta = CurrentTime;
		}
		chptr->join_count++;
		sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s JOIN :%s",
				     source_p->name, source_p->username,
				     source_p->host, chptr->chname);
	}

	modes = channel_modes(chptr, client_p);
	sendto_server(client_p, chptr, CAP_TS6, NOCAPS,
		      ":%s JOIN %ld %s %s",
		      source_p->id, (long) chptr->channelts, chptr->chname, modes);
	sendto_server(client_p, chptr, NOCAPS, CAP_TS6,
		      ":%s SJOIN %ld %s %s :%s",
		      source_p->user->server, (long) chptr->channelts,
		      chptr->chname, modes, source_p->name);
	return 0;
}

/*
 * do_join_0
 *
 * inputs	- pointer to client doing join 0
 * output	- NONE
 * side effects	- Use has decided to join 0. This is legacy
 *		  from the days when channels were numbers not names. *sigh*
 *		  There is a bunch of evilness necessary here due to
 * 		  anti spambot code.
 */
static void
do_join_0(struct Client *client_p, struct Client *source_p)
{
	struct membership *msptr;
	struct Channel *chptr = NULL;
	dlink_node *ptr;

	/* Finish the flood grace period... */
	if(MyClient(source_p) && !IsFloodDone(source_p))
		flood_endgrace(source_p);


	sendto_server(client_p, NULL, NOCAPS, NOCAPS, ":%s JOIN 0", source_p->name);

	if(source_p->user->channel.head && MyConnect(source_p) &&
	   !IsOper(source_p) && !IsExemptSpambot(source_p))
		check_spambot_warning(source_p, NULL);

	while((ptr = source_p->user->channel.head))
	{
		msptr = ptr->data;
		chptr = msptr->chptr;
		sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s PART %s",
				     source_p->name,
				     source_p->username, source_p->host, chptr->chname);
		remove_user_from_channel(msptr);
	}
}

static int
check_channel_name_loc(struct Client *source_p, const char *name)
{
	s_assert(name != NULL);
	if(EmptyString(name))
		return 0;

	if(ConfigFileEntry.disable_fake_channels && !IsOper(source_p))
	{
		for(; *name; ++name)
		{
			if(!IsChanChar(*name) || IsFakeChanChar(*name))
				return 0;
		}
	}
	else
	{
		for(; *name; ++name)
		{
			if(!IsChanChar(*name))
				return 0;
		}
	}

	return 1;
}

struct mode_letter
{
	int mode;
	char letter;
};

static struct mode_letter flags[] = {
	{MODE_NOPRIVMSGS, 'n'},
	{MODE_TOPICLIMIT, 't'},
	{MODE_SECRET, 's'},
	{MODE_MODERATED, 'm'},
	{MODE_INVITEONLY, 'i'},
	{MODE_PRIVATE, 'p'},
	{MODE_REGONLY, 'r'},
	{MODE_EXLIMIT, 'L'},
	{MODE_PERMANENT, 'P'},
	{MODE_NOCOLOR, 'c'},
	{MODE_FREEINVITE, 'g'},
	{MODE_OPMODERATE, 'z'},
	{MODE_FREETARGET, 'F'},
	{MODE_DISFORWARD, 'Q'},
	{0, 0}
};

static void
set_final_mode(struct Mode *mode, struct Mode *oldmode)
{
	int dir = MODE_QUERY;
	char *pbuf = parabuf;
	int len;
	int i;

	/* ok, first get a list of modes we need to add */
	for(i = 0; flags[i].letter; i++)
	{
		if((mode->mode & flags[i].mode) && !(oldmode->mode & flags[i].mode))
		{
			if(dir != MODE_ADD)
			{
				*mbuf++ = '+';
				dir = MODE_ADD;
			}
			*mbuf++ = flags[i].letter;
		}
	}

	/* now the ones we need to remove. */
	for(i = 0; flags[i].letter; i++)
	{
		if((oldmode->mode & flags[i].mode) && !(mode->mode & flags[i].mode))
		{
			if(dir != MODE_DEL)
			{
				*mbuf++ = '-';
				dir = MODE_DEL;
			}
			*mbuf++ = flags[i].letter;
		}
	}

	if(oldmode->limit && !mode->limit)
	{
		if(dir != MODE_DEL)
		{
			*mbuf++ = '-';
			dir = MODE_DEL;
		}
		*mbuf++ = 'l';
	}
	if(oldmode->key[0] && !mode->key[0])
	{
		if(dir != MODE_DEL)
		{
			*mbuf++ = '-';
			dir = MODE_DEL;
		}
		*mbuf++ = 'k';
		len = ircsprintf(pbuf, "%s ", oldmode->key);
		pbuf += len;
	}
	if(oldmode->join_num && !mode->join_num)
	{
		if(dir != MODE_DEL)
		{
			*mbuf++ = '-';
			dir = MODE_DEL;
		}
		*mbuf++ = 'j';
	}
	if(oldmode->forward[0] && !mode->forward[0])
	{
		if(dir != MODE_DEL)
		{
			*mbuf++ = '-';
			dir = MODE_DEL;
		}
		*mbuf++ = 'f';
	}
	if(mode->limit && oldmode->limit != mode->limit)
	{
		if(dir != MODE_ADD)
		{
			*mbuf++ = '+';
			dir = MODE_ADD;
		}
		*mbuf++ = 'l';
		len = ircsprintf(pbuf, "%d ", mode->limit);
		pbuf += len;
	}
	if(mode->key[0] && strcmp(oldmode->key, mode->key))
	{
		if(dir != MODE_ADD)
		{
			*mbuf++ = '+';
			dir = MODE_ADD;
		}
		*mbuf++ = 'k';
		len = ircsprintf(pbuf, "%s ", mode->key);
		pbuf += len;
	}
	if(mode->join_num && (oldmode->join_num != mode->join_num || oldmode->join_time != mode->join_time))
	{
		if(dir != MODE_ADD)
		{
			*mbuf++ = '+';
			dir = MODE_ADD;
		}
		*mbuf++ = 'j';
		len = ircsprintf(pbuf, "%d:%d ", mode->join_num, mode->join_time);
		pbuf += len;
	}
	if(mode->forward[0] && strcmp(oldmode->forward, mode->forward) && ConfigChannel.use_forward)
	{
		if(dir != MODE_ADD)
		{
			*mbuf++ = '+';
			dir = MODE_ADD;
		}
		*mbuf++ = 'f';
		len = ircsprintf(pbuf, "%s ", mode->forward);
		pbuf += len;
	}
	*mbuf = '\0';
}

/*
 * remove_our_modes
 *
 * inputs	-
 * output	- 
 * side effects	- 
 */
static void
remove_our_modes(struct Channel *chptr, struct Client *source_p)
{
	struct membership *msptr;
	dlink_node *ptr;
	char lmodebuf[MODEBUFLEN];
	char *lpara[MAXMODEPARAMS];
	int count = 0;
	int i;

	mbuf = lmodebuf;
	*mbuf++ = '-';

	for(i = 0; i < MAXMODEPARAMS; i++)
		lpara[i] = NULL;

	DLINK_FOREACH(ptr, chptr->members.head)
	{
		msptr = ptr->data;

		if(is_chanop(msptr))
		{
			msptr->flags &= ~CHFL_CHANOP;
			lpara[count++] = msptr->client_p->name;
			*mbuf++ = 'o';

			/* +ov, might not fit so check. */
			if(is_voiced(msptr))
			{
				if(count >= MAXMODEPARAMS)
				{
					*mbuf = '\0';
					sendto_channel_local(ALL_MEMBERS, chptr,
							     ":%s MODE %s %s %s %s %s %s",
							     me.name, chptr->chname,
							     lmodebuf, lpara[0], lpara[1],
							     lpara[2], lpara[3]);

					/* preserve the initial '-' */
					mbuf = lmodebuf;
					*mbuf++ = '-';
					count = 0;

					for(i = 0; i < MAXMODEPARAMS; i++)
						lpara[i] = NULL;
				}

				msptr->flags &= ~CHFL_VOICE;
				lpara[count++] = msptr->client_p->name;
				*mbuf++ = 'v';
			}
		}
		else if(is_voiced(msptr))
		{
			msptr->flags &= ~CHFL_VOICE;
			lpara[count++] = msptr->client_p->name;
			*mbuf++ = 'v';
		}
		else
			continue;

		if(count >= MAXMODEPARAMS)
		{
			*mbuf = '\0';
			sendto_channel_local(ALL_MEMBERS, chptr,
					     ":%s MODE %s %s %s %s %s %s",
					     me.name, chptr->chname, lmodebuf,
					     lpara[0], lpara[1], lpara[2], lpara[3]);
			mbuf = lmodebuf;
			*mbuf++ = '-';
			count = 0;

			for(i = 0; i < MAXMODEPARAMS; i++)
				lpara[i] = NULL;
		}
	}

	if(count != 0)
	{
		*mbuf = '\0';
		sendto_channel_local(ALL_MEMBERS, chptr,
				     ":%s MODE %s %s %s %s %s %s",
				     me.name, chptr->chname, lmodebuf,
				     EmptyString(lpara[0]) ? "" : lpara[0],
				     EmptyString(lpara[1]) ? "" : lpara[1],
				     EmptyString(lpara[2]) ? "" : lpara[2],
				     EmptyString(lpara[3]) ? "" : lpara[3]);

	}
}
