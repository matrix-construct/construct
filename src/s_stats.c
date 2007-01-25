/*
 *  ircd-ratbox: A slightly useful ircd.
 *  s_stats.c: Statistics related functions
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
 *  $Id: s_stats.c 1409 2006-05-21 14:46:17Z jilles $
 */

#include "stdinc.h"
#include "s_stats.h"
#include "client.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "commio.h"
#include "send.h"
#include "memory.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "whowas.h"
#include "hash.h"
#include "scache.h"
#include "reject.h"

/*
 * stats stuff
 */
static struct ServerStatistics ircst;
struct ServerStatistics *ServerStats = &ircst;

void
init_stats()
{
	memset(&ircst, 0, sizeof(ircst));
}

/*
 * tstats
 *
 * inputs	- client to report to
 * output	- NONE 
 * side effects	-
 */
void
tstats(struct Client *source_p)
{
	struct Client *target_p;
	struct ServerStatistics *sp;
	struct ServerStatistics tmp;
	dlink_node *ptr;

	sp = &tmp;
	memcpy(sp, ServerStats, sizeof(struct ServerStatistics));

	DLINK_FOREACH(ptr, serv_list.head)
	{
		target_p = ptr->data;

		sp->is_sbs += target_p->localClient->sendB;
		sp->is_sbr += target_p->localClient->receiveB;
		sp->is_sks += target_p->localClient->sendK;
		sp->is_skr += target_p->localClient->receiveK;
		sp->is_sti += CurrentTime - target_p->localClient->firsttime;
		sp->is_sv++;
		if(sp->is_sbs > 1023)
		{
			sp->is_sks += (sp->is_sbs >> 10);
			sp->is_sbs &= 0x3ff;
		}
		if(sp->is_sbr > 1023)
		{
			sp->is_skr += (sp->is_sbr >> 10);
			sp->is_sbr &= 0x3ff;
		}
	}

	DLINK_FOREACH(ptr, lclient_list.head)
	{
		target_p = ptr->data;

		sp->is_cbs += target_p->localClient->sendB;
		sp->is_cbr += target_p->localClient->receiveB;
		sp->is_cks += target_p->localClient->sendK;
		sp->is_ckr += target_p->localClient->receiveK;
		sp->is_cti += CurrentTime - target_p->localClient->firsttime;
		sp->is_cl++;
		if(sp->is_cbs > 1023)
		{
			sp->is_cks += (sp->is_cbs >> 10);
			sp->is_cbs &= 0x3ff;
		}
		if(sp->is_cbr > 1023)
		{
			sp->is_ckr += (sp->is_cbr >> 10);
			sp->is_cbr &= 0x3ff;
		}

	}

	DLINK_FOREACH(ptr, unknown_list.head)
	{
		sp->is_ni++;
	}

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :accepts %u refused %u", sp->is_ac, sp->is_ref);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			"T :rejected %u delaying %lu", 
			sp->is_rej, dlink_list_length(&delay_exit));
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			"T :nicks being delayed %lu",
			get_nd_count());
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :unknown commands %u prefixes %u",
			   sp->is_unco, sp->is_unpf);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :nick collisions %u saves %u unknown closes %u",
			   sp->is_kill, sp->is_save, sp->is_ni);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :wrong direction %u empty %u", 
			   sp->is_wrdi, sp->is_empt);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :numerics seen %u", sp->is_num);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :auth successes %u fails %u",
			   sp->is_asuc, sp->is_abad);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :sasl successes %u fails %u",
			   sp->is_ssuc, sp->is_sbad);
	sendto_one_numeric(source_p, RPL_STATSDEBUG, "T :Client Server");
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :connected %u %u", sp->is_cl, sp->is_sv);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :bytes sent %d.%uK %d.%uK",
			   (int) sp->is_cks, sp->is_cbs, 
			   (int) sp->is_sks, sp->is_sbs);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :bytes recv %d.%uK %d.%uK",
			   (int) sp->is_ckr, sp->is_cbr, 
			   (int) sp->is_skr, sp->is_sbr);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :time connected %d %d",
			   (int) sp->is_cti, (int) sp->is_sti);
}

void
count_memory(struct Client *source_p)
{
	struct Client *target_p;
	struct Channel *chptr;
	struct Ban *actualBan;
	dlink_node *dlink;
	dlink_node *ptr;
	int channel_count = 0;
	int local_client_conf_count = 0;	/* local client conf links */
	int users_counted = 0;	/* user structs */

	int channel_users = 0;
	int channel_invites = 0;
	int channel_bans = 0;
	int channel_except = 0;
	int channel_invex = 0;
	int channel_quiets = 0;

	size_t wwu = 0;		/* whowas users */
	int class_count = 0;	/* classes */
	int conf_count = 0;	/* conf lines */
	int users_invited_count = 0;	/* users invited */
	int user_channels = 0;	/* users in channels */
	int aways_counted = 0;
	size_t number_servers_cached;	/* number of servers cached by scache */

	size_t channel_memory = 0;
	size_t channel_ban_memory = 0;
	size_t channel_except_memory = 0;
	size_t channel_invex_memory = 0;
	size_t channel_quiet_memory = 0;

	size_t away_memory = 0;	/* memory used by aways */
	size_t wwm = 0;		/* whowas array memory used */
	size_t conf_memory = 0;	/* memory used by conf lines */
	size_t mem_servers_cached;	/* memory used by scache */

	size_t linebuf_count = 0;
	size_t linebuf_memory_used = 0;

	size_t total_channel_memory = 0;
	size_t totww = 0;

	size_t local_client_count = 0;
	size_t local_client_memory_used = 0;

	size_t remote_client_count = 0;
	size_t remote_client_memory_used = 0;

	size_t total_memory = 0;

	count_whowas_memory(&wwu, &wwm);

	DLINK_FOREACH(ptr, global_client_list.head)
	{
		target_p = ptr->data;
		if(MyConnect(target_p))
		{
			local_client_conf_count++;
		}

		if(target_p->user)
		{
			users_counted++;
			users_invited_count += dlink_list_length(&target_p->user->invited);
			user_channels += dlink_list_length(&target_p->user->channel);
			if(target_p->user->away)
			{
				aways_counted++;
				away_memory += (strlen(target_p->user->away) + 1);
			}
		}
	}

	/* Count up all channels, ban lists, except lists, Invex lists */
	DLINK_FOREACH(ptr, global_channel_list.head)
	{
		chptr = ptr->data;
		channel_count++;
		channel_memory += (strlen(chptr->chname) + sizeof(struct Channel));

		channel_users += dlink_list_length(&chptr->members);
		channel_invites += dlink_list_length(&chptr->invites);

		DLINK_FOREACH(dlink, chptr->banlist.head)
		{
			actualBan = dlink->data;
			channel_bans++;

			channel_ban_memory += sizeof(dlink_node) + sizeof(struct Ban);
		}

		DLINK_FOREACH(dlink, chptr->exceptlist.head)
		{
			actualBan = dlink->data;
			channel_except++;

			channel_except_memory += (sizeof(dlink_node) + sizeof(struct Ban));
		}

		DLINK_FOREACH(dlink, chptr->invexlist.head)
		{
			actualBan = dlink->data;
			channel_invex++;

			channel_invex_memory += (sizeof(dlink_node) + sizeof(struct Ban));
		}

		DLINK_FOREACH(dlink, chptr->quietlist.head)
		{
			actualBan = dlink->data;
			channel_quiets++;

			channel_quiet_memory += (sizeof(dlink_node) + sizeof(struct Ban));
		}
	}

	/* count up all classes */

	class_count = dlink_list_length(&class_list) + 1;

	count_linebuf_memory(&linebuf_count, &linebuf_memory_used);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Users %u(%lu) Invites %u(%lu)",
			   users_counted,
			   (unsigned long) users_counted * sizeof(struct User),
			   users_invited_count, 
			   (unsigned long) users_invited_count * sizeof(dlink_node));

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :User channels %u(%lu) Aways %u(%d)",
			   user_channels,
			   (unsigned long) user_channels * sizeof(dlink_node),
			   aways_counted, (int) away_memory);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Attached confs %u(%lu)",
			   local_client_conf_count,
			   (unsigned long) local_client_conf_count * sizeof(dlink_node));

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Conflines %u(%d)", conf_count, (int) conf_memory);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Classes %u(%lu)",
			   class_count, 
			   (unsigned long) class_count * sizeof(struct Class));

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Channels %u(%d)",
			   channel_count, (int) channel_memory);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Bans %u(%d) Exceptions %u(%d) Invex %u(%d) Quiets %u(%d)",
			   channel_bans, (int) channel_ban_memory,
			   channel_except, (int) channel_except_memory,
			   channel_invex, (int) channel_invex_memory,
			   channel_quiets, (int) channel_quiet_memory);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Channel members %u(%lu) invite %u(%lu)",
			   channel_users,
			   (unsigned long) channel_users * sizeof(dlink_node),
			   channel_invites, 
			   (unsigned long) channel_invites * sizeof(dlink_node));

	total_channel_memory = channel_memory +
		channel_ban_memory +
		channel_users * sizeof(dlink_node) + channel_invites * sizeof(dlink_node);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Whowas users %ld(%ld)",
			   (long)wwu, (long) (wwu * sizeof(struct User)));

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Whowas array %u(%d)",
			   NICKNAMEHISTORYLENGTH, (int) wwm);

	totww = wwu * sizeof(struct User) + wwm;

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Hash: client %u(%ld) chan %u(%ld)",
			   U_MAX, (long)(U_MAX * sizeof(dlink_list)), 
			   CH_MAX, (long)(CH_MAX * sizeof(dlink_list)));

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :linebuf %ld(%ld)",
			   (long)linebuf_count, (long)linebuf_memory_used);

	count_scache(&number_servers_cached, &mem_servers_cached);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :scache %ld(%ld)",
			   (long)number_servers_cached, (long)mem_servers_cached);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :hostname hash %d(%ld)",
			   HOST_MAX, (long)HOST_MAX * sizeof(dlink_list));

	total_memory = totww + total_channel_memory + conf_memory +
		class_count * sizeof(struct Class);

	total_memory += mem_servers_cached;
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Total: whowas %d channel %d conf %d", 
			   (int) totww, (int) total_channel_memory, 
			   (int) conf_memory);

	count_local_client_memory(&local_client_count, &local_client_memory_used);
	total_memory += local_client_memory_used;

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Local client Memory in use: %ld(%ld)",
			   (long)local_client_count, (long)local_client_memory_used);


	count_remote_client_memory(&remote_client_count, &remote_client_memory_used);
	total_memory += remote_client_memory_used;

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Remote client Memory in use: %ld(%ld)",
			   (long)remote_client_count,
			   (long)remote_client_memory_used);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :TOTAL: %d Available:  Current max RSS: %lu",
			   (int) total_memory, get_maxrss());
}
