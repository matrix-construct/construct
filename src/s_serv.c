/*
 *  ircd-ratbox: A slightly useful ircd.
 *  s_serv.c: Server related functions.
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
 *  $Id: s_serv.c 3550 2007-08-09 06:47:26Z nenolod $
 */

#include "stdinc.h"

#ifdef HAVE_LIBCRYPTO
#include <openssl/rsa.h>
#endif

#include "s_serv.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "hash.h"
#include "match.h"
#include "ircd.h"
#include "ircd_defs.h"
#include "numeric.h"
#include "packet.h"
#include "res.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "logger.h"
#include "s_stats.h"
#include "s_user.h"
#include "scache.h"
#include "send.h"
#include "client.h"
#include "channel.h"		/* chcap_usage_counts stuff... */
#include "hook.h"
#include "msg.h"
#include "reject.h"
#include "sslproc.h"

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned int) 0xffffffff)
#endif

int MaxConnectionCount = 1;
int MaxClientCount = 1;
int refresh_user_links = 0;

static char buf[BUFSIZE];

/*
 * list of recognized server capabilities.  "TS" is not on the list
 * because all servers that we talk to already do TS, and the kludged
 * extra argument to "PASS" takes care of checking that.  -orabidoo
 */
struct Capability captab[] = {
/*  name     cap     */
	{ "QS",		CAP_QS },
	{ "EX",		CAP_EX },
	{ "CHW",	CAP_CHW},
	{ "IE", 	CAP_IE},
	{ "KLN",	CAP_KLN},
	{ "KNOCK",	CAP_KNOCK},
	{ "ZIP",	CAP_ZIP},
	{ "TB",		CAP_TB},
	{ "UNKLN",	CAP_UNKLN},
	{ "CLUSTER",	CAP_CLUSTER},
	{ "ENCAP",	CAP_ENCAP },
	{ "SERVICES",	CAP_SERVICE },
	{ "RSFNC",	CAP_RSFNC },
	{ "SAVE",	CAP_SAVE },
	{ "EUID",	CAP_EUID },
	{ "EOPMOD",	CAP_EOPMOD },
	{ "BAN",	CAP_BAN },
	{ "MLOCK",	CAP_MLOCK },
	{0, 0}
};

static CNCB serv_connect_callback;
static CNCB serv_connect_ssl_callback;

/*
 * hunt_server - Do the basic thing in delivering the message (command)
 *      across the relays to the specific server (server) for
 *      actions.
 *
 *      Note:   The command is a format string and *MUST* be
 *              of prefixed style (e.g. ":%s COMMAND %s ...").
 *              Command can have only max 8 parameters.
 *
 *      server  parv[server] is the parameter identifying the
 *              target server.
 *
 *      *WARNING*
 *              parv[server] is replaced with the pointer to the
 *              real servername from the matched client (I'm lazy
 *              now --msa).
 *
 *      returns: (see #defines)
 */
int
hunt_server(struct Client *client_p, struct Client *source_p,
	    const char *command, int server, int parc, const char *parv[])
{
	struct Client *target_p;
	int wilds;
	rb_dlink_node *ptr;
	const char *old;
	char *new;

	/*
	 * Assume it's me, if no server
	 */
	if(parc <= server || EmptyString(parv[server]) ||
	   match(parv[server], me.name) || (strcmp(parv[server], me.id) == 0))
		return (HUNTED_ISME);
	
	new = LOCAL_COPY(parv[server]);

	/*
	 * These are to pickup matches that would cause the following
	 * message to go in the wrong direction while doing quick fast
	 * non-matching lookups.
	 */
	if(MyClient(source_p))
		target_p = find_named_client(new);
	else
		target_p = find_client(new);

	if(target_p)
		if(target_p->from == source_p->from && !MyConnect(target_p))
			target_p = NULL;

	collapse(new);
	wilds = (strchr(new, '?') || strchr(new, '*'));

	/*
	 * Again, if there are no wild cards involved in the server
	 * name, use the hash lookup
	 */
	if(!target_p && wilds)
	{
		RB_DLINK_FOREACH(ptr, global_client_list.head)
		{
			if(match(new, ((struct Client *) (ptr->data))->name))
			{
				target_p = ptr->data;
				break;
			}
		}
	}

	if(target_p && !IsRegistered(target_p))
		target_p = NULL;

	if(target_p)
	{
		if(IsMe(target_p) || MyClient(target_p))
			return HUNTED_ISME;

		old = parv[server];
		parv[server] = get_id(target_p, target_p);

		sendto_one(target_p, command, get_id(source_p, target_p),
			   parv[1], parv[2], parv[3], parv[4], parv[5], parv[6], parv[7], parv[8]);
		parv[server] = old;
		return (HUNTED_PASS);
	}

	if(MyClient(source_p) || !IsDigit(parv[server][0]))
		sendto_one_numeric(source_p, ERR_NOSUCHSERVER,
				   form_str(ERR_NOSUCHSERVER), parv[server]);
	return (HUNTED_NOSUCH);
}

/*
 * try_connections - scan through configuration and try new connections.
 * Returns the calendar time when the next call to this
 * function should be made latest. (No harm done if this
 * is called earlier or later...)
 */
void
try_connections(void *unused)
{
	struct Client *client_p;
	struct server_conf *server_p = NULL;
	struct server_conf *tmp_p;
	struct Class *cltmp;
	rb_dlink_node *ptr;
	int connecting = FALSE;
	int confrq = 0;
	time_t next = 0;

	RB_DLINK_FOREACH(ptr, server_conf_list.head)
	{
		tmp_p = ptr->data;

		if(ServerConfIllegal(tmp_p) || !ServerConfAutoconn(tmp_p))
			continue;

		/* don't allow ssl connections if ssl isn't setup */
		if(ServerConfSSL(tmp_p) && (!ssl_ok || !get_ssld_count()))
			continue;

		cltmp = tmp_p->class;

		/*
		 * Skip this entry if the use of it is still on hold until
		 * future. Otherwise handle this entry (and set it on hold
		 * until next time). Will reset only hold times, if already
		 * made one successfull connection... [this algorithm is
		 * a bit fuzzy... -- msa >;) ]
		 */
		if(tmp_p->hold > rb_current_time())
		{
			if(next > tmp_p->hold || next == 0)
				next = tmp_p->hold;
			continue;
		}

		confrq = get_con_freq(cltmp);
		tmp_p->hold = rb_current_time() + confrq;

		/*
		 * Found a CONNECT config with port specified, scan clients
		 * and see if this server is already connected?
		 */
		client_p = find_server(NULL, tmp_p->name);

		if(!client_p && (CurrUsers(cltmp) < MaxUsers(cltmp)) && !connecting)
		{
			server_p = tmp_p;

			/* We connect only one at time... */
			connecting = TRUE;
		}

		if((next > tmp_p->hold) || (next == 0))
			next = tmp_p->hold;
	}

	/* TODO: change this to set active flag to 0 when added to event! --Habeeb */
	if(GlobalSetOptions.autoconn == 0)
		return;

	if(!connecting)
		return;

	/* move this connect entry to end.. */
	rb_dlinkDelete(&server_p->node, &server_conf_list);
	rb_dlinkAddTail(server_p, &server_p->node, &server_conf_list);

	/*
	 * We used to only print this if serv_connect() actually
	 * suceeded, but since rb_tcp_connect() can call the callback
	 * immediately if there is an error, we were getting error messages
	 * in the wrong order. SO, we just print out the activated line,
	 * and let serv_connect() / serv_connect_callback() print an
	 * error afterwards if it fails.
	 *   -- adrian
	 */
	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			"Connection to %s activated",
			server_p->name);

	serv_connect(server_p, 0);
}

int
check_server(const char *name, struct Client *client_p)
{
	struct server_conf *server_p = NULL;
	struct server_conf *tmp_p;
	rb_dlink_node *ptr;
	int error = -1;

	s_assert(NULL != client_p);
	if(client_p == NULL)
		return error;

	if(!(client_p->localClient->passwd))
		return -2;

	if(strlen(name) > HOSTLEN)
		return -4;

	RB_DLINK_FOREACH(ptr, server_conf_list.head)
	{
		tmp_p = ptr->data;

		if(ServerConfIllegal(tmp_p))
			continue;

		if(!match(tmp_p->name, name))
			continue;

		error = -3;

		/* XXX: Fix me for IPv6 */
		/* XXX sockhost is the IPv4 ip as a string */
		if(match(tmp_p->host, client_p->host) ||
		   match(tmp_p->host, client_p->sockhost))
		{
			error = -2;

			if(ServerConfEncrypted(tmp_p))
			{
				if(!strcmp(tmp_p->passwd, rb_crypt(client_p->localClient->passwd,
								tmp_p->passwd)))
				{
					server_p = tmp_p;
					break;
				}
			}
			else if(!strcmp(tmp_p->passwd, client_p->localClient->passwd))
			{
				server_p = tmp_p;
				break;
			}
		}
	}

	if(server_p == NULL)
		return error;

	if(ServerConfSSL(server_p) && client_p->localClient->ssl_ctl == NULL)
	{
		return -5;
	}

	attach_server_conf(client_p, server_p);

	/* clear ZIP/TB if they support but we dont want them */
#ifdef HAVE_LIBZ
	if(!ServerConfCompressed(server_p))
#endif
		ClearCap(client_p, CAP_ZIP);

	if(!ServerConfTb(server_p))
		ClearCap(client_p, CAP_TB);

	return 0;
}

/*
 * send_capabilities
 *
 * inputs	- Client pointer to send to
 *		- int flag of capabilities that this server has
 * output	- NONE
 * side effects	- send the CAPAB line to a server  -orabidoo
 *
 */
void
send_capabilities(struct Client *client_p, int cap_can_send)
{
	struct Capability *cap;
	char msgbuf[BUFSIZE];
	char *t;
	int tl;

	t = msgbuf;

	for (cap = captab; cap->name; ++cap)
	{
		if(cap->cap & cap_can_send)
		{
			tl = rb_sprintf(t, "%s ", cap->name);
			t += tl;
		}
	}

	t--;
	*t = '\0';

	sendto_one(client_p, "CAPAB :%s", msgbuf);
}

static void
burst_ban(struct Client *client_p)
{
	rb_dlink_node *ptr;
	struct ConfItem *aconf;
	const char *type, *oper;
	/* +5 for !,@,{,} and null */
	char operbuf[NICKLEN + USERLEN + HOSTLEN + HOSTLEN + 5];
	char *p;
	size_t melen;

	melen = strlen(me.name);
	RB_DLINK_FOREACH(ptr, prop_bans.head)
	{
		aconf = ptr->data;

		/* Skip expired stuff. */
		if(aconf->lifetime < rb_current_time())
			continue;
		switch(aconf->status & ~CONF_ILLEGAL)
		{
			case CONF_KILL: type = "K"; break;
			case CONF_DLINE: type = "D"; break;
			case CONF_XLINE: type = "X"; break;
			case CONF_RESV_NICK: type = "R"; break;
			case CONF_RESV_CHANNEL: type = "R"; break;
			default:
				continue;
		}
		oper = aconf->info.oper;
		if(aconf->flags & CONF_FLAGS_MYOPER)
		{
			/* Our operator{} names may not be meaningful
			 * to other servers, so rewrite to our server
			 * name.
			 */
			rb_strlcpy(operbuf, aconf->info.oper, sizeof buf);
			p = strrchr(operbuf, '{');
			if (p != NULL &&
					operbuf + sizeof operbuf - p > (ptrdiff_t)(melen + 2))
			{
				memcpy(p + 1, me.name, melen);
				p[melen + 1] = '}';
				p[melen + 2] = '\0';
				oper = operbuf;
			}
		}
		sendto_one(client_p, ":%s BAN %s %s %s %lu %d %d %s :%s%s%s",
				me.id,
				type,
				aconf->user ? aconf->user : "*", aconf->host,
				(unsigned long)aconf->created,
				(int)(aconf->hold - aconf->created),
				(int)(aconf->lifetime - aconf->created),
				oper,
				aconf->passwd,
				aconf->spasswd ? "|" : "",
				aconf->spasswd ? aconf->spasswd : "");
	}
}

/* burst_modes_TS6()
 *
 * input	- client to burst to, channel name, list to burst, mode flag
 * output	-
 * side effects - client is sent a list of +b, +e, or +I modes
 */
static void
burst_modes_TS6(struct Client *client_p, struct Channel *chptr, 
		rb_dlink_list *list, char flag)
{
	rb_dlink_node *ptr;
	struct Ban *banptr;
	char *t;
	int tlen;
	int mlen;
	int cur_len;

	cur_len = mlen = rb_sprintf(buf, ":%s BMASK %ld %s %c :",
				    me.id, (long) chptr->channelts, chptr->chname, flag);
	t = buf + mlen;

	RB_DLINK_FOREACH(ptr, list->head)
	{
		banptr = ptr->data;

		tlen = strlen(banptr->banstr) + 1;

		/* uh oh */
		if(cur_len + tlen > BUFSIZE - 3)
		{
			/* the one we're trying to send doesnt fit at all! */
			if(cur_len == mlen)
			{
				s_assert(0);
				continue;
			}

			/* chop off trailing space and send.. */
			*(t-1) = '\0';
			sendto_one(client_p, "%s", buf);
			cur_len = mlen;
			t = buf + mlen;
		}

		rb_sprintf(t, "%s ", banptr->banstr);
		t += tlen;
		cur_len += tlen;
	}

	/* cant ever exit the loop above without having modified buf,
	 * chop off trailing space and send.
	 */
	*(t-1) = '\0';
	sendto_one(client_p, "%s", buf);
}

/*
 * burst_TS6
 * 
 * inputs	- client (server) to send nick towards
 * 		- client to send nick for
 * output	- NONE
 * side effects	- NICK message is sent towards given client_p
 */
static void
burst_TS6(struct Client *client_p)
{
	static char ubuf[12];
	struct Client *target_p;
	struct Channel *chptr;
	struct membership *msptr;
	hook_data_client hclientinfo;
	hook_data_channel hchaninfo;
	rb_dlink_node *ptr;
	rb_dlink_node *uptr;
	char *t;
	int tlen, mlen;
	int cur_len = 0;

	hclientinfo.client = hchaninfo.client = client_p;

	RB_DLINK_FOREACH(ptr, global_client_list.head)
	{
		target_p = ptr->data;

		if(!IsPerson(target_p))
			continue;

		send_umode(NULL, target_p, 0, 0, ubuf);
		if(!*ubuf)
		{
			ubuf[0] = '+';
			ubuf[1] = '\0';
		}

		if(IsCapable(client_p, CAP_EUID))
			sendto_one(client_p, ":%s EUID %s %d %ld %s %s %s %s %s %s %s :%s",
				   target_p->servptr->id, target_p->name,
				   target_p->hopcount + 1, 
				   (long) target_p->tsinfo, ubuf,
				   target_p->username, target_p->host,
				   IsIPSpoof(target_p) ? "0" : target_p->sockhost,
				   target_p->id,
				   IsDynSpoof(target_p) ? target_p->orighost : "*",
				   EmptyString(target_p->user->suser) ? "*" : target_p->user->suser,
				   target_p->info);
		else
			sendto_one(client_p, ":%s UID %s %d %ld %s %s %s %s %s :%s",
				   target_p->servptr->id, target_p->name,
				   target_p->hopcount + 1, 
				   (long) target_p->tsinfo, ubuf,
				   target_p->username, target_p->host,
				   IsIPSpoof(target_p) ? "0" : target_p->sockhost,
				   target_p->id, target_p->info);

		if(!EmptyString(target_p->certfp))
			sendto_one(client_p, ":%s ENCAP * CERTFP :%s",
					use_id(target_p), target_p->certfp);

		if(!IsCapable(client_p, CAP_EUID))
		{
			if(IsDynSpoof(target_p))
				sendto_one(client_p, ":%s ENCAP * REALHOST %s",
						use_id(target_p), target_p->orighost);
			if(!EmptyString(target_p->user->suser))
				sendto_one(client_p, ":%s ENCAP * LOGIN %s",
						use_id(target_p), target_p->user->suser);
		}

		if(ConfigFileEntry.burst_away && !EmptyString(target_p->user->away))
			sendto_one(client_p, ":%s AWAY :%s",
				   use_id(target_p),
				   target_p->user->away);

		hclientinfo.target = target_p;
		call_hook(h_burst_client, &hclientinfo);
	}

	RB_DLINK_FOREACH(ptr, global_channel_list.head)
	{
		chptr = ptr->data;

		if(*chptr->chname != '#')
			continue;

		cur_len = mlen = rb_sprintf(buf, ":%s SJOIN %ld %s %s :", me.id,
				(long) chptr->channelts, chptr->chname,
				channel_modes(chptr, client_p));

		t = buf + mlen;

		RB_DLINK_FOREACH(uptr, chptr->members.head)
		{
			msptr = uptr->data;

			tlen = strlen(use_id(msptr->client_p)) + 1;
			if(is_chanop(msptr))
				tlen++;
			if(is_voiced(msptr))
				tlen++;

			if(cur_len + tlen >= BUFSIZE - 3)
			{
				*(t-1) = '\0';
				sendto_one(client_p, "%s", buf);
				cur_len = mlen;
				t = buf + mlen;
			}

			rb_sprintf(t, "%s%s ", find_channel_status(msptr, 1), 
				   use_id(msptr->client_p));

			cur_len += tlen;
			t += tlen;
		}

		if (rb_dlink_list_length(&chptr->members) > 0)
		{
			/* remove trailing space */
			*(t-1) = '\0';
		}
		sendto_one(client_p, "%s", buf);

		if(rb_dlink_list_length(&chptr->banlist) > 0)
			burst_modes_TS6(client_p, chptr, &chptr->banlist, 'b');

		if(IsCapable(client_p, CAP_EX) &&
		   rb_dlink_list_length(&chptr->exceptlist) > 0)
			burst_modes_TS6(client_p, chptr, &chptr->exceptlist, 'e');

		if(IsCapable(client_p, CAP_IE) &&
		   rb_dlink_list_length(&chptr->invexlist) > 0)
			burst_modes_TS6(client_p, chptr, &chptr->invexlist, 'I');

		if(rb_dlink_list_length(&chptr->quietlist) > 0)
			burst_modes_TS6(client_p, chptr, &chptr->quietlist, 'q');

		if(IsCapable(client_p, CAP_TB) && chptr->topic != NULL)
			sendto_one(client_p, ":%s TB %s %ld %s%s:%s",
				   me.id, chptr->chname, (long) chptr->topic_time,
				   ConfigChannel.burst_topicwho ? chptr->topic_info : "",
				   ConfigChannel.burst_topicwho ? " " : "",
				   chptr->topic);

		if(IsCapable(client_p, CAP_MLOCK))
			sendto_one(client_p, ":%s MLOCK %ld %s :%s",
				   me.id, (long) chptr->channelts, chptr->chname,
				   EmptyString(chptr->mode_lock) ? "" : chptr->mode_lock);

		hchaninfo.chptr = chptr;
		call_hook(h_burst_channel, &hchaninfo);
	}

	hclientinfo.target = NULL;
	call_hook(h_burst_finished, &hclientinfo);
}

/*
 * show_capabilities - show current server capabilities
 *
 * inputs       - pointer to an struct Client
 * output       - pointer to static string
 * side effects - build up string representing capabilities of server listed
 */
const char *
show_capabilities(struct Client *target_p)
{
	static char msgbuf[BUFSIZE];
	struct Capability *cap;

	if(has_id(target_p))
		rb_strlcpy(msgbuf, " TS6", sizeof(msgbuf));

	if(IsSSL(target_p))
		rb_strlcat(msgbuf, " SSL", sizeof(msgbuf));

	if(!IsServer(target_p) || !target_p->serv->caps)	/* short circuit if no caps */
		return msgbuf + 1;

	for (cap = captab; cap->cap; ++cap)
	{
		if(cap->cap & target_p->serv->caps)
			rb_snprintf_append(msgbuf, sizeof(msgbuf), " %s", cap->name);
	}

	return msgbuf + 1;
}

/*
 * server_estab
 *
 * inputs       - pointer to a struct Client
 * output       -
 * side effects -
 */
int
server_estab(struct Client *client_p)
{
	struct Client *target_p;
	struct server_conf *server_p;
	hook_data_client hdata;
	char *host;
	rb_dlink_node *ptr;
	char note[HOSTLEN + 15];

	s_assert(NULL != client_p);
	if(client_p == NULL)
		return -1;

	host = client_p->name;

	if((server_p = client_p->localClient->att_sconf) == NULL)
	{
		/* This shouldn't happen, better tell the ops... -A1kmm */
		sendto_realops_snomask(SNO_GENERAL, is_remote_connect(client_p) ? L_NETWIDE : L_ALL,
				     "Warning: Lost connect{} block for server %s!", host);
		return exit_client(client_p, client_p, client_p, "Lost connect{} block!");
	}

	/* We shouldn't have to check this, it should already done before
	 * server_estab is called. -A1kmm
	 */
	if(client_p->localClient->passwd)
	{
		memset(client_p->localClient->passwd, 0, strlen(client_p->localClient->passwd));
		rb_free(client_p->localClient->passwd);
		client_p->localClient->passwd = NULL;
	}

	/* Its got identd , since its a server */
	SetGotId(client_p);

	/* If there is something in the serv_list, it might be this
	 * connecting server..
	 */
	if(!ServerInfo.hub && serv_list.head)
	{
		if(client_p != serv_list.head->data || serv_list.head->next)
		{
			ServerStats.is_ref++;
			sendto_one(client_p, "ERROR :I'm a leaf not a hub");
			return exit_client(client_p, client_p, client_p, "I'm a leaf");
		}
	}

	if(IsUnknown(client_p))
	{
		/*
		 * jdc -- 1.  Use EmptyString(), not [0] index reference.
		 *        2.  Check ->spasswd, not ->passwd.
		 */
		if(!EmptyString(server_p->spasswd))
		{
			sendto_one(client_p, "PASS %s TS %d :%s", 
				   server_p->spasswd, TS_CURRENT, me.id);
		}

		/* pass info to new server */
		send_capabilities(client_p, default_server_capabs
				  | (ServerConfCompressed(server_p) ? CAP_ZIP_SUPPORTED : 0)
				  | (ServerConfTb(server_p) ? CAP_TB : 0));

		sendto_one(client_p, "SERVER %s 1 :%s%s",
			   me.name,
			   ConfigServerHide.hidden ? "(H) " : "",
			   (me.info[0]) ? (me.info) : "IRCers United");
	}

	if(!rb_set_buffers(client_p->localClient->F, READBUF_SIZE))
		ilog_error("rb_set_buffers failed for server");

	/* Enable compression now */
	if(IsCapable(client_p, CAP_ZIP))
	{
		start_zlib_session(client_p);
	}
	sendto_one(client_p, "SVINFO %d %d 0 :%ld", TS_CURRENT, TS_MIN, (long int)rb_current_time());

	client_p->servptr = &me;

	if(IsAnyDead(client_p))
		return CLIENT_EXITED;

	SetServer(client_p);

	/* Update the capability combination usage counts */
	set_chcap_usage_counts(client_p);

	rb_dlinkAdd(client_p, &client_p->lnode, &me.serv->servers);
	rb_dlinkMoveNode(&client_p->localClient->tnode, &unknown_list, &serv_list);
	rb_dlinkAddTailAlloc(client_p, &global_serv_list);

	if(has_id(client_p))
		add_to_id_hash(client_p->id, client_p);

	add_to_client_hash(client_p->name, client_p);
	/* doesnt duplicate client_p->serv if allocated this struct already */
	make_server(client_p);

	client_p->serv->caps = client_p->localClient->caps;

	if(client_p->localClient->fullcaps)
	{
		client_p->serv->fullcaps = rb_strdup(client_p->localClient->fullcaps);
		rb_free(client_p->localClient->fullcaps);
		client_p->localClient->fullcaps = NULL;
	}

	client_p->serv->nameinfo = scache_connect(client_p->name, client_p->info, IsHidden(client_p));
	client_p->localClient->firsttime = rb_current_time();
	/* fixing eob timings.. -gnp */

	if((rb_dlink_list_length(&lclient_list) + rb_dlink_list_length(&serv_list)) >
	   (unsigned long)MaxConnectionCount)
		MaxConnectionCount = rb_dlink_list_length(&lclient_list) + 
					rb_dlink_list_length(&serv_list);

	/* Show the real host/IP to admins */
	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			"Link with %s established: (%s) link",
			client_p->name,
			show_capabilities(client_p));

	ilog(L_SERVER, "Link with %s established: (%s) link",
	     log_client_name(client_p, SHOW_IP), show_capabilities(client_p));

	hdata.client = &me;
	hdata.target = client_p;
	call_hook(h_server_introduced, &hdata);

	rb_snprintf(note, sizeof(note), "Server: %s", client_p->name);
	rb_note(client_p->localClient->F, note);

	/*
	 ** Old sendto_serv_but_one() call removed because we now
	 ** need to send different names to different servers
	 ** (domain name matching) Send new server to other servers.
	 */
	RB_DLINK_FOREACH(ptr, serv_list.head)
	{
		target_p = ptr->data;

		if(target_p == client_p)
			continue;

		if(has_id(target_p) && has_id(client_p))
		{
			sendto_one(target_p, ":%s SID %s 2 %s :%s%s",
				   me.id, client_p->name, client_p->id,
				   IsHidden(client_p) ? "(H) " : "", client_p->info);

			if(IsCapable(target_p, CAP_ENCAP) &&
			   !EmptyString(client_p->serv->fullcaps))
				sendto_one(target_p, ":%s ENCAP * GCAP :%s",
					client_p->id, client_p->serv->fullcaps);
		}
		else
		{
			sendto_one(target_p, ":%s SERVER %s 2 :%s%s",
				   me.name, client_p->name,
				   IsHidden(client_p) ? "(H) " : "", client_p->info);

			if(IsCapable(target_p, CAP_ENCAP) &&
			   !EmptyString(client_p->serv->fullcaps))
				sendto_one(target_p, ":%s ENCAP * GCAP :%s",
					client_p->name, client_p->serv->fullcaps);
		}
	}

	/*
	 ** Pass on my client information to the new server
	 **
	 ** First, pass only servers (idea is that if the link gets
	 ** cancelled beacause the server was already there,
	 ** there are no NICK's to be cancelled...). Of course,
	 ** if cancellation occurs, all this info is sent anyway,
	 ** and I guess the link dies when a read is attempted...? --msa
	 ** 
	 ** Note: Link cancellation to occur at this point means
	 ** that at least two servers from my fragment are building
	 ** up connection this other fragment at the same time, it's
	 ** a race condition, not the normal way of operation...
	 **
	 ** ALSO NOTE: using the get_client_name for server names--
	 **    see previous *WARNING*!!! (Also, original inpath
	 **    is destroyed...)
	 */
	RB_DLINK_FOREACH(ptr, global_serv_list.head)
	{
		target_p = ptr->data;

		/* target_p->from == target_p for target_p == client_p */
		if(IsMe(target_p) || target_p->from == client_p)
			continue;

		/* presumption, if target has an id, so does its uplink */
		if(has_id(client_p) && has_id(target_p))
			sendto_one(client_p, ":%s SID %s %d %s :%s%s",
				   target_p->servptr->id, target_p->name,
				   target_p->hopcount + 1, target_p->id,
				   IsHidden(target_p) ? "(H) " : "", target_p->info);
		else
			sendto_one(client_p, ":%s SERVER %s %d :%s%s",
				   target_p->servptr->name,
				   target_p->name, target_p->hopcount + 1,
				   IsHidden(target_p) ? "(H) " : "", target_p->info);

		if(IsCapable(client_p, CAP_ENCAP) && 
		   !EmptyString(target_p->serv->fullcaps))
			sendto_one(client_p, ":%s ENCAP * GCAP :%s",
					get_id(target_p, client_p),
					target_p->serv->fullcaps);
	}

	if(IsCapable(client_p, CAP_BAN))
		burst_ban(client_p);

	burst_TS6(client_p);

	/* Always send a PING after connect burst is done */
	sendto_one(client_p, "PING :%s", get_id(&me, client_p));

	free_pre_client(client_p);

	send_pop_queue(client_p);

	return 0;
}

/*
 * New server connection code
 * Based upon the stuff floating about in s_bsd.c
 *   -- adrian
 */

static int
serv_connect_resolved(struct Client *client_p)
{
	struct rb_sockaddr_storage myipnum;
	char vhoststr[HOSTIPLEN];
	struct server_conf *server_p;
	uint16_t port;

	if((server_p = client_p->localClient->att_sconf) == NULL)
	{
		sendto_realops_snomask(SNO_GENERAL, is_remote_connect(client_p) ? L_NETWIDE : L_ALL, "Lost connect{} block for %s",
				client_p->name);
		exit_client(client_p, client_p, &me, "Lost connect{} block");
		return 0;
	}

#ifdef RB_IPV6
	if(client_p->localClient->ip.ss_family == AF_INET6)
		port = ntohs(((struct sockaddr_in6 *)&client_p->localClient->ip)->sin6_port);
	else
#endif
		port = ntohs(((struct sockaddr_in *)&client_p->localClient->ip)->sin_port);

	if(ServerConfVhosted(server_p))
	{
		memcpy(&myipnum, &server_p->my_ipnum, sizeof(myipnum));
		((struct sockaddr_in *)&myipnum)->sin_port = 0;
		myipnum.ss_family = server_p->aftype;
				
	}
	else if(server_p->aftype == AF_INET && ServerInfo.specific_ipv4_vhost)
	{
		memcpy(&myipnum, &ServerInfo.ip, sizeof(myipnum));
		((struct sockaddr_in *)&myipnum)->sin_port = 0;
		myipnum.ss_family = AF_INET;
		SET_SS_LEN(&myipnum, sizeof(struct sockaddr_in));
	}
	
#ifdef RB_IPV6
	else if((server_p->aftype == AF_INET6) && ServerInfo.specific_ipv6_vhost)
	{
		memcpy(&myipnum, &ServerInfo.ip6, sizeof(myipnum));
		((struct sockaddr_in6 *)&myipnum)->sin6_port = 0;
		myipnum.ss_family = AF_INET6;
		SET_SS_LEN(&myipnum, sizeof(struct sockaddr_in6));
	}
#endif
	else
	{
		/* log */
		ilog(L_SERVER, "Connecting to %s[%s] port %d (%s)", client_p->name, client_p->sockhost, port,
#ifdef RB_IPV6
				server_p->aftype == AF_INET6 ? "IPv6" :
#endif
				(server_p->aftype == AF_INET ? "IPv4" : "?"));

		if(ServerConfSSL(server_p))
		{
			rb_connect_tcp(client_p->localClient->F, (struct sockaddr *)&client_p->localClient->ip,
					 NULL, 0, serv_connect_ssl_callback, 
					 client_p, ConfigFileEntry.connect_timeout);
		}
		else
			rb_connect_tcp(client_p->localClient->F, (struct sockaddr *)&client_p->localClient->ip,
					 NULL, 0, serv_connect_callback, 
					 client_p, ConfigFileEntry.connect_timeout);
		 return 1;
	}

	/* log */
	rb_inet_ntop_sock((struct sockaddr *)&myipnum, vhoststr, sizeof vhoststr);
	ilog(L_SERVER, "Connecting to %s[%s] port %d (%s) (vhost %s)", client_p->name, client_p->sockhost, port,
#ifdef RB_IPV6
			server_p->aftype == AF_INET6 ? "IPv6" :
#endif
			(server_p->aftype == AF_INET ? "IPv4" : "?"), vhoststr);


	if(ServerConfSSL(server_p))
		rb_connect_tcp(client_p->localClient->F, (struct sockaddr *)&client_p->localClient->ip,
				 (struct sockaddr *) &myipnum,
				 GET_SS_LEN(&myipnum), serv_connect_ssl_callback, client_p,
				 ConfigFileEntry.connect_timeout);
	else
		rb_connect_tcp(client_p->localClient->F, (struct sockaddr *)&client_p->localClient->ip,
				 (struct sockaddr *) &myipnum,
				 GET_SS_LEN(&myipnum), serv_connect_callback, client_p,
				 ConfigFileEntry.connect_timeout);

	return 1;
}

static void
serv_connect_dns_callback(void *vptr, struct DNSReply *reply)
{
	struct Client *client_p = vptr;
	uint16_t port;

	rb_free(client_p->localClient->dnsquery);
	client_p->localClient->dnsquery = NULL;

	if (reply == NULL)
	{
		sendto_realops_snomask(SNO_GENERAL, is_remote_connect(client_p) ? L_NETWIDE : L_ALL, "Cannot resolve hostname for %s",
				client_p->name);
		ilog(L_SERVER, "Cannot resolve hostname for %s",
				log_client_name(client_p, HIDE_IP));
		exit_client(client_p, client_p, &me, "Cannot resolve hostname");
		return;
	}
#ifdef RB_IPV6
	if(reply->addr.ss_family == AF_INET6)
		port = ((struct sockaddr_in6 *)&client_p->localClient->ip)->sin6_port;
	else
#endif
		port = ((struct sockaddr_in *)&client_p->localClient->ip)->sin_port;
	memcpy(&client_p->localClient->ip, &reply->addr, sizeof(client_p->localClient->ip));
#ifdef RB_IPV6
	if(reply->addr.ss_family == AF_INET6)
		((struct sockaddr_in6 *)&client_p->localClient->ip)->sin6_port = port;
	else
#endif
		((struct sockaddr_in *)&client_p->localClient->ip)->sin_port = port;
	/* Set sockhost properly now -- jilles */
	rb_inet_ntop_sock((struct sockaddr *)&client_p->localClient->ip,
			client_p->sockhost, sizeof client_p->sockhost);
	serv_connect_resolved(client_p);
}

/*
 * serv_connect() - initiate a server connection
 *
 * inputs	- pointer to conf 
 *		- pointer to client doing the connet
 * output	-
 * side effects	-
 *
 * This code initiates a connection to a server. It first checks to make
 * sure the given server exists. If this is the case, it creates a socket,
 * creates a client, saves the socket information in the client, and
 * initiates a connection to the server through rb_connect_tcp(). The
 * completion of this goes through serv_completed_connection().
 *
 * We return 1 if the connection is attempted, since we don't know whether
 * it suceeded or not, and 0 if it fails in here somewhere.
 */
int
serv_connect(struct server_conf *server_p, struct Client *by)
{
	struct Client *client_p;
	struct rb_sockaddr_storage theiripnum;
	rb_fde_t *F;
	char note[HOSTLEN + 10];

	s_assert(server_p != NULL);
	if(server_p == NULL)
		return 0;

	/*
	 * Make sure this server isn't already connected
	 */
	if((client_p = find_server(NULL, server_p->name)))
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "Server %s already present from %s",
				     server_p->name, client_p->name);
		if(by && IsPerson(by) && !MyClient(by))
			sendto_one_notice(by, ":Server %s already present from %s",
					  server_p->name, client_p->name);
		return 0;
	}

	/* create a socket for the server connection */
	if((F = rb_socket(server_p->aftype, SOCK_STREAM, 0, NULL)) == NULL)
	{
		ilog_error("opening a stream socket");
		return 0;
	}

	rb_snprintf(note, sizeof note, "Server: %s", server_p->name);
	rb_note(F, note);

	/* Create a local client */
	client_p = make_client(NULL);

	/* Copy in the server, hostname, fd
	 * The sockhost may be a hostname, this will be corrected later
	 * -- jilles
	 */
	rb_strlcpy(client_p->name, server_p->name, sizeof(client_p->name));
	rb_strlcpy(client_p->host, server_p->host, sizeof(client_p->host));
	rb_strlcpy(client_p->sockhost, server_p->host, sizeof(client_p->sockhost));
	client_p->localClient->F = F;
	add_to_cli_fd_hash(client_p);

	/*
	 * Set up the initial server evilness, ripped straight from
	 * connect_server(), so don't blame me for it being evil.
	 *   -- adrian
	 */

	if(!rb_set_buffers(client_p->localClient->F, READBUF_SIZE))
	{
		ilog_error("setting the buffer size for a server connection");
	}

	/*
	 * Attach config entries to client here rather than in
	 * serv_connect_callback(). This to avoid null pointer references.
	 */
	attach_server_conf(client_p, server_p);

	/*
	 * at this point we have a connection in progress and C/N lines
	 * attached to the client, the socket info should be saved in the
	 * client and it should either be resolved or have a valid address.
	 *
	 * The socket has been connected or connect is in progress.
	 */
	make_server(client_p);
	if(by && IsPerson(by))
	{
		strcpy(client_p->serv->by, by->name);
		if(client_p->serv->user)
			free_user(client_p->serv->user, NULL);
		client_p->serv->user = by->user;
		by->user->refcnt++;
	}
	else
	{
		strcpy(client_p->serv->by, "AutoConn.");
		if(client_p->serv->user)
			free_user(client_p->serv->user, NULL);
		client_p->serv->user = NULL;
	}
	SetConnecting(client_p);
	rb_dlinkAddTail(client_p, &client_p->node, &global_client_list);

	if (rb_inet_pton_sock(server_p->host, (struct sockaddr *)&theiripnum) > 0)
	{
		memcpy(&client_p->localClient->ip, &theiripnum, sizeof(client_p->localClient->ip));
#ifdef RB_IPV6
		if(theiripnum.ss_family == AF_INET6)
			((struct sockaddr_in6 *)&client_p->localClient->ip)->sin6_port = htons(server_p->port);
		else
#endif
			((struct sockaddr_in *)&client_p->localClient->ip)->sin_port = htons(server_p->port);

		return serv_connect_resolved(client_p);
	}
	else
	{
#ifdef RB_IPV6
		if(theiripnum.ss_family == AF_INET6)
			((struct sockaddr_in6 *)&client_p->localClient->ip)->sin6_port = htons(server_p->port);
		else
#endif
			((struct sockaddr_in *)&client_p->localClient->ip)->sin_port = htons(server_p->port);

		client_p->localClient->dnsquery = rb_malloc(sizeof(struct DNSQuery));
		client_p->localClient->dnsquery->ptr = client_p;
		client_p->localClient->dnsquery->callback = serv_connect_dns_callback;
		gethost_byname_type(server_p->host, client_p->localClient->dnsquery,
#ifdef RB_IPV6
				server_p->aftype == AF_INET6 ? T_AAAA :
#endif
				T_A);
		return 1;
	}
}

static void
serv_connect_ssl_callback(rb_fde_t *F, int status, void *data)
{
	struct Client *client_p = data;
	rb_fde_t *xF[2];
	rb_connect_sockaddr(F, (struct sockaddr *)&client_p->localClient->ip, sizeof(client_p->localClient->ip));
	if(status != RB_OK)
	{
		/* Print error message, just like non-SSL. */
		serv_connect_callback(F, status, data);
		return;
	}
	if(rb_socketpair(AF_UNIX, SOCK_STREAM, 0, &xF[0], &xF[1], "Outgoing ssld connection") == -1)
	{
                ilog_error("rb_socketpair failed for server");
		serv_connect_callback(F, RB_ERROR, data);
		return;
		
	}
	del_from_cli_fd_hash(client_p);
	client_p->localClient->F = xF[0];
	add_to_cli_fd_hash(client_p);

	client_p->localClient->ssl_ctl = start_ssld_connect(F, xF[1], rb_get_fd(xF[0]));
	SetSSL(client_p);
	serv_connect_callback(client_p->localClient->F, RB_OK, client_p);
}

/*
 * serv_connect_callback() - complete a server connection.
 * 
 * This routine is called after the server connection attempt has
 * completed. If unsucessful, an error is sent to ops and the client
 * is closed. If sucessful, it goes through the initialisation/check
 * procedures, the capabilities are sent, and the socket is then
 * marked for reading.
 */
static void
serv_connect_callback(rb_fde_t *F, int status, void *data)
{
	struct Client *client_p = data;
	struct server_conf *server_p;
	char *errstr;

	/* First, make sure its a real client! */
	s_assert(client_p != NULL);
	s_assert(client_p->localClient->F == F);

	if(client_p == NULL)
		return;

	/* while we were waiting for the callback, its possible this already
	 * linked in.. --fl
	 */
	if(find_server(NULL, client_p->name) != NULL)
	{
		exit_client(client_p, client_p, &me, "Server Exists");
		return;
	}

	if(client_p->localClient->ssl_ctl == NULL)
		rb_connect_sockaddr(F, (struct sockaddr *)&client_p->localClient->ip, sizeof(client_p->localClient->ip));

	/* Check the status */
	if(status != RB_OK)
	{
		/* COMM_ERR_TIMEOUT wont have an errno associated with it,
		 * the others will.. --fl
		 */
		if(status == RB_ERR_TIMEOUT)
		{
			sendto_realops_snomask(SNO_GENERAL, is_remote_connect(client_p) ? L_NETWIDE : L_ALL,
					"Error connecting to %s[%s]: %s",
					client_p->name, 
					"255.255.255.255",
					rb_errstr(status));
			ilog(L_SERVER, "Error connecting to %s[%s]: %s",
				client_p->name, client_p->sockhost,
				rb_errstr(status));
		}
		else
		{
			errstr = strerror(rb_get_sockerr(F));
			sendto_realops_snomask(SNO_GENERAL, is_remote_connect(client_p) ? L_NETWIDE : L_ALL,
					"Error connecting to %s[%s]: %s (%s)",
					client_p->name,
					"255.255.255.255",
					rb_errstr(status), errstr);
			ilog(L_SERVER, "Error connecting to %s[%s]: %s (%s)",
				client_p->name, client_p->sockhost,
				rb_errstr(status), errstr);
		}

		exit_client(client_p, client_p, &me, rb_errstr(status));
		return;
	}

	/* COMM_OK, so continue the connection procedure */
	/* Get the C/N lines */
	if((server_p = client_p->localClient->att_sconf) == NULL)
	{
		sendto_realops_snomask(SNO_GENERAL, is_remote_connect(client_p) ? L_NETWIDE : L_ALL, "Lost connect{} block for %s",
				client_p->name);
		exit_client(client_p, client_p, &me, "Lost connect{} block");
		return;
	}

	/* Next, send the initial handshake */
	SetHandshake(client_p);

	if(!EmptyString(server_p->spasswd))
	{
		sendto_one(client_p, "PASS %s TS %d :%s", 
			   server_p->spasswd, TS_CURRENT, me.id);
	}

	/* pass my info to the new server */
	send_capabilities(client_p, default_server_capabs
			  | (ServerConfCompressed(server_p) ? CAP_ZIP_SUPPORTED : 0)
			  | (ServerConfTb(server_p) ? CAP_TB : 0));

	sendto_one(client_p, "SERVER %s 1 :%s%s",
		   me.name,
		   ConfigServerHide.hidden ? "(H) " : "", me.info);

	/* 
	 * If we've been marked dead because a send failed, just exit
	 * here now and save everyone the trouble of us ever existing.
	 */
	if(IsAnyDead(client_p))
	{
		sendto_realops_snomask(SNO_GENERAL, is_remote_connect(client_p) ? L_NETWIDE : L_ALL,
				     "%s went dead during handshake", client_p->name);
		exit_client(client_p, client_p, &me, "Went dead during handshake");
		return;
	}

	/* don't move to serv_list yet -- we haven't sent a burst! */

	/* If we get here, we're ok, so lets start reading some data */
	read_packet(F, client_p);
}
