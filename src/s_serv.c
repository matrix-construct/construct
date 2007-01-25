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
 *  $Id: s_serv.c 2723 2006-11-09 23:35:48Z jilles $
 */

#include "stdinc.h"

#ifdef HAVE_LIBCRYPTO
#include <openssl/rsa.h>
#endif

#include "tools.h"
#include "s_serv.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "event.h"
#include "hash.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "ircd.h"
#include "ircd_defs.h"
#include "numeric.h"
#include "packet.h"
#include "res.h"
#include "commio.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "s_log.h"
#include "s_stats.h"
#include "s_user.h"
#include "scache.h"
#include "send.h"
#include "client.h"
#include "memory.h"
#include "channel.h"		/* chcap_usage_counts stuff... */
#include "hook.h"
#include "msg.h"

extern char *crypt();

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned int) 0xffffffff)
#endif

#ifndef HAVE_SOCKETPAIR
static int inet_socketpair(int d, int type, int protocol, int sv[2]);
#endif

int MaxConnectionCount = 1;
int MaxClientCount = 1;
int refresh_user_links = 0;

static char buf[BUFSIZE];

static void start_io(struct Client *server);

static SlinkRplHnd slink_error;
static SlinkRplHnd slink_zipstats;
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
	{ "GLN",	CAP_GLN},
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
	{0, 0}
};

struct SlinkRplDef slinkrpltab[] = {
	{SLINKRPL_ERROR, slink_error, SLINKRPL_FLAG_DATA},
	{SLINKRPL_ZIPSTATS, slink_zipstats, SLINKRPL_FLAG_DATA},
	{0, 0, 0},
};

static int fork_server(struct Client *client_p);

static CNCB serv_connect_callback;

void
slink_error(unsigned int rpl, unsigned int len, unsigned char *data, struct Client *server_p)
{
	char squitreason[256];

	s_assert(rpl == SLINKRPL_ERROR);

	s_assert(len < 256);
	data[len - 1] = '\0';

	sendto_realops_snomask(SNO_GENERAL, L_ALL, "SlinkError for %s: %s", server_p->name, data);
	snprintf(squitreason, sizeof squitreason, "servlink error: %s", data);
	exit_client(server_p, server_p, &me, squitreason);
}

void
slink_zipstats(unsigned int rpl, unsigned int len, unsigned char *data, struct Client *server_p)
{
	struct ZipStats zipstats;
	u_int32_t in = 0, in_wire = 0, out = 0, out_wire = 0;
	int i = 0;

	s_assert(rpl == SLINKRPL_ZIPSTATS);
	s_assert(len == 16);
	s_assert(IsCapable(server_p, CAP_ZIP));

	/* Yes, it needs to be done this way, no we cannot let the compiler
	 * work with the pointer to the structure.  This works around a GCC
	 * bug on SPARC that affects all versions at the time of this writing.
	 * I will feed you to the creatures living in RMS's beard if you do
	 * not leave this as is, without being sure that you are not causing
	 * regression for most of our installed SPARC base.
	 * -jmallett, 04/27/2002
	 */
	memcpy(&zipstats, &server_p->localClient->zipstats, sizeof(struct ZipStats));

	in |= (data[i++] << 24);
	in |= (data[i++] << 16);
	in |= (data[i++] << 8);
	in |= (data[i++]);

	in_wire |= (data[i++] << 24);
	in_wire |= (data[i++] << 16);
	in_wire |= (data[i++] << 8);
	in_wire |= (data[i++]);

	out |= (data[i++] << 24);
	out |= (data[i++] << 16);
	out |= (data[i++] << 8);
	out |= (data[i++]);

	out_wire |= (data[i++] << 24);
	out_wire |= (data[i++] << 16);
	out_wire |= (data[i++] << 8);
	out_wire |= (data[i++]);

	zipstats.in += in;
	zipstats.inK += zipstats.in >> 10;
	zipstats.in &= 0x03ff;

	zipstats.in_wire += in_wire;
	zipstats.inK_wire += zipstats.in_wire >> 10;
	zipstats.in_wire &= 0x03ff;

	zipstats.out += out;
	zipstats.outK += zipstats.out >> 10;
	zipstats.out &= 0x03ff;

	zipstats.out_wire += out_wire;
	zipstats.outK_wire += zipstats.out_wire >> 10;
	zipstats.out_wire &= 0x03ff;

	if(zipstats.inK > 0)
		zipstats.in_ratio =
			(((double) (zipstats.inK - zipstats.inK_wire) /
			  (double) zipstats.inK) * 100.00);
	else
		zipstats.in_ratio = 0;

	if(zipstats.outK > 0)
		zipstats.out_ratio =
			(((double) (zipstats.outK - zipstats.outK_wire) /
			  (double) zipstats.outK) * 100.00);
	else
		zipstats.out_ratio = 0;

	memcpy(&server_p->localClient->zipstats, &zipstats, sizeof(struct ZipStats));
}

void
collect_zipstats(void *unused)
{
	dlink_node *ptr;
	struct Client *target_p;

	DLINK_FOREACH(ptr, serv_list.head)
	{
		target_p = ptr->data;
		if(IsCapable(target_p, CAP_ZIP))
		{
			/* only bother if we haven't already got something queued... */
			if(!target_p->localClient->slinkq)
			{
				target_p->localClient->slinkq = MyMalloc(1);	/* sigh.. */
				target_p->localClient->slinkq[0] = SLINKCMD_ZIPSTATS;
				target_p->localClient->slinkq_ofs = 0;
				target_p->localClient->slinkq_len = 1;
				send_queued_slink_write(target_p->localClient->ctrlfd, target_p);
			}
		}
	}
}

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
	dlink_node *ptr;
	const char *old;
	char *new;

	/*
	 * Assume it's me, if no server
	 */
	if(parc <= server || EmptyString(parv[server]) ||
	   match(me.name, parv[server]) || match(parv[server], me.name) ||
	   (strcmp(parv[server], me.id) == 0))
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

	if(target_p == NULL && (target_p = find_server(source_p, new)))
		if(target_p->from == source_p->from && !MyConnect(target_p))
			target_p = NULL;

	collapse(new);
	wilds = (strchr(new, '?') || strchr(new, '*'));

	/*
	 * Again, if there are no wild cards involved in the server
	 * name, use the hash lookup
	 */
	if(!target_p)
	{
		if(!wilds)
		{
			if(MyClient(source_p) || !IsDigit(parv[server][0]))
				sendto_one_numeric(source_p, ERR_NOSUCHSERVER,
						   form_str(ERR_NOSUCHSERVER),
						   parv[server]);
			return (HUNTED_NOSUCH);
		}
		else
		{
			target_p = NULL;

			DLINK_FOREACH(ptr, global_client_list.head)
			{
				if(match(new, ((struct Client *) (ptr->data))->name))
				{
					target_p = ptr->data;
					break;
				}
			}
		}
	}

	if(target_p)
	{
		if(!IsRegistered(target_p))
		{
			sendto_one_numeric(source_p, ERR_NOSUCHSERVER,
					   form_str(ERR_NOSUCHSERVER),
					   parv[server]);
			return HUNTED_NOSUCH;
		}

		if(IsMe(target_p) || MyClient(target_p))
			return HUNTED_ISME;

		old = parv[server];
		parv[server] = get_id(target_p, target_p);

		sendto_one(target_p, command, get_id(source_p, target_p),
			   parv[1], parv[2], parv[3], parv[4], parv[5], parv[6], parv[7], parv[8]);
		parv[server] = old;
		return (HUNTED_PASS);
	}

	if(!IsDigit(parv[server][0]))
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
	dlink_node *ptr;
	int connecting = FALSE;
	int confrq = 0;
	time_t next = 0;

	DLINK_FOREACH(ptr, server_conf_list.head)
	{
		tmp_p = ptr->data;

		if(ServerConfIllegal(tmp_p) || !ServerConfAutoconn(tmp_p))
			continue;

		cltmp = tmp_p->class;

		/*
		 * Skip this entry if the use of it is still on hold until
		 * future. Otherwise handle this entry (and set it on hold
		 * until next time). Will reset only hold times, if already
		 * made one successfull connection... [this algorithm is
		 * a bit fuzzy... -- msa >;) ]
		 */
		if(tmp_p->hold > CurrentTime)
		{
			if(next > tmp_p->hold || next == 0)
				next = tmp_p->hold;
			continue;
		}

		confrq = get_con_freq(cltmp);
		tmp_p->hold = CurrentTime + confrq;

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
	dlinkDelete(&server_p->node, &server_conf_list);
	dlinkAddTail(server_p, &server_p->node, &server_conf_list);

	/*
	 * We used to only print this if serv_connect() actually
	 * suceeded, but since comm_tcp_connect() can call the callback
	 * immediately if there is an error, we were getting error messages
	 * in the wrong order. SO, we just print out the activated line,
	 * and let serv_connect() / serv_connect_callback() print an
	 * error afterwards if it fails.
	 *   -- adrian
	 */
#ifndef HIDE_SERVERS_IPS
	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			"Connection to %s[%s] activated.",
			server_p->name, server_p->host);
#else
	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			"Connection to %s activated",
			server_p->name);
#endif

	serv_connect(server_p, 0);
}

int
check_server(const char *name, struct Client *client_p)
{
	struct server_conf *server_p = NULL;
	struct server_conf *tmp_p;
	dlink_node *ptr;
	int error = -1;

	s_assert(NULL != client_p);
	if(client_p == NULL)
		return error;

	if(!(client_p->localClient->passwd))
		return -2;

	if(strlen(name) > HOSTLEN)
		return -4;

	DLINK_FOREACH(ptr, server_conf_list.head)
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
				if(!strcmp(tmp_p->passwd, crypt(client_p->localClient->passwd,
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
			tl = ircsprintf(t, "%s ", cap->name);
			t += tl;
		}
	}

	t--;
	*t = '\0';

	sendto_one(client_p, "CAPAB :%s", msgbuf);
}

/* burst_modes_TS5()
 *
 * input	- client to burst to, channel name, list to burst, mode flag
 * output	-
 * side effects - client is sent a list of +b, or +e, or +I modes
 */
static void
burst_modes_TS5(struct Client *client_p, char *chname, dlink_list *list, char flag)
{
	dlink_node *ptr;
	struct Ban *banptr;
	char mbuf[MODEBUFLEN];
	char pbuf[BUFSIZE];
	int tlen;
	int mlen;
	int cur_len;
	char *mp;
	char *pp;
	int count = 0;

	mlen = ircsprintf(buf, ":%s MODE %s +", me.name, chname);
	cur_len = mlen;

	mp = mbuf;
	pp = pbuf;

	DLINK_FOREACH(ptr, list->head)
	{
		banptr = ptr->data;
		tlen = strlen(banptr->banstr) + 3;

		/* uh oh */
		if(tlen > MODEBUFLEN)
			continue;

		if((count >= MAXMODEPARAMS) || ((cur_len + tlen + 2) > (BUFSIZE - 3)))
		{
			sendto_one(client_p, "%s%s %s", buf, mbuf, pbuf);

			mp = mbuf;
			pp = pbuf;
			cur_len = mlen;
			count = 0;
		}

		*mp++ = flag;
		*mp = '\0';
		pp += ircsprintf(pp, "%s ", banptr->banstr);
		cur_len += tlen;
		count++;
	}

	if(count != 0)
		sendto_one(client_p, "%s%s %s", buf, mbuf, pbuf);
}

/* burst_modes_TS6()
 *
 * input	- client to burst to, channel name, list to burst, mode flag
 * output	-
 * side effects - client is sent a list of +b, +e, or +I modes
 */
static void
burst_modes_TS6(struct Client *client_p, struct Channel *chptr, 
		dlink_list *list, char flag)
{
	dlink_node *ptr;
	struct Ban *banptr;
	char *t;
	int tlen;
	int mlen;
	int cur_len;

	cur_len = mlen = ircsprintf(buf, ":%s BMASK %ld %s %c :",
				    me.id, (long) chptr->channelts, chptr->chname, flag);
	t = buf + mlen;

	DLINK_FOREACH(ptr, list->head)
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

		ircsprintf(t, "%s ", banptr->banstr);
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
 * burst_TS5
 * 
 * inputs	- client (server) to send nick towards
 * 		- client to send nick for
 * output	- NONE
 * side effects	- NICK message is sent towards given client_p
 */
static void
burst_TS5(struct Client *client_p)
{
	static char ubuf[12];
	struct Client *target_p;
	struct Channel *chptr;
	struct membership *msptr;
	hook_data_client hclientinfo;
	hook_data_channel hchaninfo;
	dlink_node *ptr;
	dlink_node *uptr;
	char *t;
	int tlen, mlen;
	int cur_len = 0;

	hclientinfo.client = hchaninfo.client = client_p;

	DLINK_FOREACH(ptr, global_client_list.head)
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

		sendto_one(client_p, "NICK %s %d %ld %s %s %s %s :%s",
			   target_p->name, target_p->hopcount + 1,
			   (long) target_p->tsinfo, ubuf,
			   target_p->username, target_p->host,
			   target_p->user->server, target_p->info);

		if(IsDynSpoof(target_p))
			sendto_one(client_p, ":%s ENCAP * REALHOST %s",
					target_p->name, target_p->orighost);
		if(!EmptyString(target_p->user->suser))
			sendto_one(client_p, ":%s ENCAP * LOGIN %s",
					target_p->name, target_p->user->suser);

		if(ConfigFileEntry.burst_away && !EmptyString(target_p->user->away))
			sendto_one(client_p, ":%s AWAY :%s",
				   target_p->name, target_p->user->away);

		hclientinfo.target = target_p;
		call_hook(h_burst_client, &hclientinfo);
	}

	DLINK_FOREACH(ptr, global_channel_list.head)
	{
		chptr = ptr->data;

		if(*chptr->chname != '#')
			continue;

		cur_len = mlen = ircsprintf(buf, ":%s SJOIN %ld %s %s :", me.name,
				(long) chptr->channelts, chptr->chname, 
				channel_modes(chptr, client_p));

		t = buf + mlen;

		DLINK_FOREACH(uptr, chptr->members.head)
		{
			msptr = uptr->data;

			tlen = strlen(msptr->client_p->name) + 1;
			if(is_chanop(msptr))
				tlen++;
			if(is_voiced(msptr))
				tlen++;

			if(cur_len + tlen >= BUFSIZE - 3)
			{
				t--;
				*t = '\0';
				sendto_one(client_p, "%s", buf);
				cur_len = mlen;
				t = buf + mlen;
			}

			ircsprintf(t, "%s%s ", find_channel_status(msptr, 1), 
				   msptr->client_p->name);

			cur_len += tlen;
			t += tlen;
		}

		if (dlink_list_length(&chptr->members) > 0)
		{
			/* remove trailing space */
			t--;
			*t = '\0';
		}
		sendto_one(client_p, "%s", buf);

		burst_modes_TS5(client_p, chptr->chname, &chptr->banlist, 'b');

		if(IsCapable(client_p, CAP_EX))
			burst_modes_TS5(client_p, chptr->chname, &chptr->exceptlist, 'e');

		if(IsCapable(client_p, CAP_IE))
			burst_modes_TS5(client_p, chptr->chname, &chptr->invexlist, 'I');

		burst_modes_TS5(client_p, chptr->chname, &chptr->quietlist, 'q');

		if(IsCapable(client_p, CAP_TB) && chptr->topic != NULL)
			sendto_one(client_p, ":%s TB %s %ld %s%s:%s",
				   me.name, chptr->chname, (long) chptr->topic_time,
				   ConfigChannel.burst_topicwho ? chptr->topic_info : "",
				   ConfigChannel.burst_topicwho ? " " : "",
				   chptr->topic);

		hchaninfo.chptr = chptr;
		call_hook(h_burst_channel, &hchaninfo);
	}

	hclientinfo.target = NULL;
	call_hook(h_burst_finished, &hclientinfo);
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
	dlink_node *ptr;
	dlink_node *uptr;
	char *t;
	int tlen, mlen;
	int cur_len = 0;

	hclientinfo.client = hchaninfo.client = client_p;

	DLINK_FOREACH(ptr, global_client_list.head)
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

		if(has_id(target_p) && IsCapable(client_p, CAP_EUID))
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
		else if(has_id(target_p))
			sendto_one(client_p, ":%s UID %s %d %ld %s %s %s %s %s :%s",
				   target_p->servptr->id, target_p->name,
				   target_p->hopcount + 1, 
				   (long) target_p->tsinfo, ubuf,
				   target_p->username, target_p->host,
				   IsIPSpoof(target_p) ? "0" : target_p->sockhost,
				   target_p->id, target_p->info);
		else
			sendto_one(client_p, "NICK %s %d %ld %s %s %s %s :%s",
					target_p->name,
					target_p->hopcount + 1,
					(long) target_p->tsinfo,
					ubuf,
					target_p->username, target_p->host,
					target_p->user->server, target_p->info);

		if(!has_id(target_p) || !IsCapable(client_p, CAP_EUID))
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

	DLINK_FOREACH(ptr, global_channel_list.head)
	{
		chptr = ptr->data;

		if(*chptr->chname != '#')
			continue;

		cur_len = mlen = ircsprintf(buf, ":%s SJOIN %ld %s %s :", me.id,
				(long) chptr->channelts, chptr->chname,
				channel_modes(chptr, client_p));

		t = buf + mlen;

		DLINK_FOREACH(uptr, chptr->members.head)
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

			ircsprintf(t, "%s%s ", find_channel_status(msptr, 1), 
				   use_id(msptr->client_p));

			cur_len += tlen;
			t += tlen;
		}

		if (dlink_list_length(&chptr->members) > 0)
		{
			/* remove trailing space */
			*(t-1) = '\0';
		}
		sendto_one(client_p, "%s", buf);

		if(dlink_list_length(&chptr->banlist) > 0)
			burst_modes_TS6(client_p, chptr, &chptr->banlist, 'b');

		if(IsCapable(client_p, CAP_EX) &&
		   dlink_list_length(&chptr->exceptlist) > 0)
			burst_modes_TS6(client_p, chptr, &chptr->exceptlist, 'e');

		if(IsCapable(client_p, CAP_IE) &&
		   dlink_list_length(&chptr->invexlist) > 0)
			burst_modes_TS6(client_p, chptr, &chptr->invexlist, 'I');

		if(dlink_list_length(&chptr->quietlist) > 0)
			burst_modes_TS6(client_p, chptr, &chptr->quietlist, 'q');

		if(IsCapable(client_p, CAP_TB) && chptr->topic != NULL)
			sendto_one(client_p, ":%s TB %s %ld %s%s:%s",
				   me.id, chptr->chname, (long) chptr->topic_time,
				   ConfigChannel.burst_topicwho ? chptr->topic_info : "",
				   ConfigChannel.burst_topicwho ? " " : "",
				   chptr->topic);

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
	char *t;
	int tl;

	t = msgbuf;
	tl = ircsprintf(msgbuf, "TS ");
	t += tl;

	if(!IsServer(target_p) || !target_p->serv->caps)	/* short circuit if no caps */
	{
		msgbuf[2] = '\0';
		return msgbuf;
	}

	for (cap = captab; cap->cap; ++cap)
	{
		if(cap->cap & target_p->serv->caps)
		{
			tl = ircsprintf(t, "%s ", cap->name);
			t += tl;
		}
	}

	t--;
	*t = '\0';

	return msgbuf;
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
	dlink_node *ptr;

	s_assert(NULL != client_p);
	if(client_p == NULL)
		return -1;
	ClearAccess(client_p);

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
		MyFree(client_p->localClient->passwd);
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
			ServerStats->is_ref++;
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
			/* kludge, if we're not using TS6, dont ever send
			 * ourselves as being TS6 capable.
			 */
			if(ServerInfo.use_ts6)
				sendto_one(client_p, "PASS %s TS %d :%s", 
					   server_p->spasswd, TS_CURRENT, me.id);
			else
				sendto_one(client_p, "PASS %s :TS",
					   server_p->spasswd);
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

	if(!comm_set_buffers(client_p->localClient->fd, READBUF_SIZE))
		report_error(SETBUF_ERROR_MSG, 
			     get_server_name(client_p, SHOW_IP), 
			     log_client_name(client_p, SHOW_IP), errno);

	/* Hand the server off to servlink now */
	if(IsCapable(client_p, CAP_ZIP))
	{
		if(fork_server(client_p) < 0)
		{
			sendto_realops_snomask(SNO_GENERAL, is_remote_connect(client_p) ? L_NETWIDE : L_ALL,
					     "Warning: fork failed for server %s -- check servlink_path (%s)",
					     get_server_name(client_p, HIDE_IP),
					     ConfigFileEntry.servlink_path);
			return exit_client(client_p, client_p, client_p, "Fork failed");
		}
		start_io(client_p);
		SetServlink(client_p);
	}

	sendto_one(client_p, "SVINFO %d %d 0 :%ld", TS_CURRENT, TS_MIN, CurrentTime);

	client_p->servptr = &me;

	if(IsAnyDead(client_p))
		return CLIENT_EXITED;

	SetServer(client_p);

	/* Update the capability combination usage counts */
	set_chcap_usage_counts(client_p);

	dlinkAdd(client_p, &client_p->lnode, &me.serv->servers);
	dlinkMoveNode(&client_p->localClient->tnode, &unknown_list, &serv_list);
	dlinkAddTailAlloc(client_p, &global_serv_list);

	if(has_id(client_p))
		add_to_id_hash(client_p->id, client_p);

	add_to_client_hash(client_p->name, client_p);
	/* doesnt duplicate client_p->serv if allocated this struct already */
	make_server(client_p);
	client_p->serv->up = me.name;
	client_p->serv->upid = me.id;

	client_p->serv->caps = client_p->localClient->caps;

	if(client_p->localClient->fullcaps)
	{
		DupString(client_p->serv->fullcaps, client_p->localClient->fullcaps);
		MyFree(client_p->localClient->fullcaps);
		client_p->localClient->fullcaps = NULL;
	}

	/* add it to scache */
	find_or_add(client_p->name);
	client_p->localClient->firsttime = CurrentTime;
	/* fixing eob timings.. -gnp */

	/* Show the real host/IP to admins */
	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			"Link with %s established: (%s) link",
			get_server_name(client_p, SHOW_IP),
			show_capabilities(client_p));

	ilog(L_SERVER, "Link with %s established: (%s) link",
	     log_client_name(client_p, SHOW_IP), show_capabilities(client_p));

	hdata.client = &me;
	hdata.target = client_p;
	call_hook(h_server_introduced, &hdata);

	if(HasServlink(client_p))
	{
		/* we won't overflow FD_DESC_SZ here, as it can hold
		 * client_p->name + 64
		 */
		comm_note(client_p->localClient->fd, "slink data: %s", client_p->name);
		comm_note(client_p->localClient->ctrlfd, "slink ctrl: %s", client_p->name);
	}
	else
		comm_note(client_p->localClient->fd, "Server: %s", client_p->name);

	/*
	 ** Old sendto_serv_but_one() call removed because we now
	 ** need to send different names to different servers
	 ** (domain name matching) Send new server to other servers.
	 */
	DLINK_FOREACH(ptr, serv_list.head)
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
	DLINK_FOREACH(ptr, global_serv_list.head)
	{
		target_p = ptr->data;

		/* target_p->from == target_p for target_p == client_p */
		if(IsMe(target_p) || target_p->from == client_p)
			continue;

		/* presumption, if target has an id, so does its uplink */
		if(has_id(client_p) && has_id(target_p))
			sendto_one(client_p, ":%s SID %s %d %s :%s%s",
				   target_p->serv->upid, target_p->name,
				   target_p->hopcount + 1, target_p->id,
				   IsHidden(target_p) ? "(H) " : "", target_p->info);
		else
			sendto_one(client_p, ":%s SERVER %s %d :%s%s",
				   target_p->serv->up,
				   target_p->name, target_p->hopcount + 1,
				   IsHidden(target_p) ? "(H) " : "", target_p->info);

		if(IsCapable(client_p, CAP_ENCAP) && 
		   !EmptyString(target_p->serv->fullcaps))
			sendto_one(client_p, ":%s ENCAP * GCAP :%s",
					get_id(target_p, client_p),
					target_p->serv->fullcaps);
	}

	if(has_id(client_p))
		burst_TS6(client_p);
	else
		burst_TS5(client_p);

	/* Always send a PING after connect burst is done */
	sendto_one(client_p, "PING :%s", get_id(&me, client_p));

	free_pre_client(client_p);

	return 0;
}

static void
start_io(struct Client *server)
{
	unsigned char *iobuf;
	int c = 0;
	int linecount = 0;
	int linelen;

	iobuf = MyMalloc(256);	/* XXX: This seems arbitrary. Perhaps make it IRCD_BUFSIZE? --nenolod */

	if(IsCapable(server, CAP_ZIP))
	{
		/* ziplink */
		iobuf[c++] = SLINKCMD_SET_ZIP_OUT_LEVEL;
		iobuf[c++] = 0;	/* |          */
		iobuf[c++] = 1;	/* \ len is 1 */
		iobuf[c++] = ConfigFileEntry.compression_level;
		iobuf[c++] = SLINKCMD_START_ZIP_IN;
		iobuf[c++] = SLINKCMD_START_ZIP_OUT;
	}

	while (MyConnect(server))
	{
		linecount++;

		iobuf = MyRealloc(iobuf, (c + READBUF_SIZE + 64));

		/* store data in c+3 to allow for SLINKCMD_INJECT_RECVQ and len u16 */
		linelen = linebuf_get(&server->localClient->buf_recvq, (char *) (iobuf + c + 3), READBUF_SIZE, LINEBUF_PARTIAL, LINEBUF_RAW);	/* include partial lines */

		if(linelen)
		{
			iobuf[c++] = SLINKCMD_INJECT_RECVQ;
			iobuf[c++] = (linelen >> 8);
			iobuf[c++] = (linelen & 0xff);
			c += linelen;
		}
		else
			break;
	}

	while (MyConnect(server))
	{
		linecount++;

		iobuf = MyRealloc(iobuf, (c + BUF_DATA_SIZE + 64));

		/* store data in c+3 to allow for SLINKCMD_INJECT_RECVQ and len u16 */
		linelen = linebuf_get(&server->localClient->buf_sendq, 
				      (char *) (iobuf + c + 3), READBUF_SIZE, 
				      LINEBUF_PARTIAL, LINEBUF_PARSED);	/* include partial lines */

		if(linelen)
		{
			iobuf[c++] = SLINKCMD_INJECT_SENDQ;
			iobuf[c++] = (linelen >> 8);
			iobuf[c++] = (linelen & 0xff);
			c += linelen;
		}
		else
			break;
	}

	/* start io */
	iobuf[c++] = SLINKCMD_INIT;

	server->localClient->slinkq = iobuf;
	server->localClient->slinkq_ofs = 0;
	server->localClient->slinkq_len = c;

	/* schedule a write */
	send_queued_slink_write(server->localClient->ctrlfd, server);
}

/*
 * fork_server
 *
 * inputs       - struct Client *server
 * output       - success: 0 / failure: -1
 * side effect  - fork, and exec SERVLINK to handle this connection
 */
static int
fork_server(struct Client *server)
{
	int ret;
	int i;
	int ctrl_fds[2];
	int data_fds[2];

	char fd_str[4][6];
	char *kid_argv[7];
	char slink[] = "-slink";


	/* ctrl */
#ifdef HAVE_SOCKETPAIR
	if(socketpair(AF_UNIX, SOCK_STREAM, 0, ctrl_fds) < 0)
#else
	if(inet_socketpair(AF_INET,SOCK_STREAM, 0, ctrl_fds) < 0)
#endif
		goto fork_error;

	

	/* data */
#ifdef HAVE_SOCKETPAIR
	if(socketpair(AF_UNIX, SOCK_STREAM, 0, data_fds) < 0)
#else
	if(inet_socketpair(AF_INET,SOCK_STREAM, 0, data_fds) < 0)
#endif
		goto fork_error;


#ifdef __CYGWIN__
	if((ret = vfork()) < 0)
#else
	if((ret = fork()) < 0)
#endif
		goto fork_error;
	else if(ret == 0)
	{
		/* set our fds as non blocking and close everything else */
		for (i = 0; i < HARD_FDLIMIT; i++)
		{
				

			if((i == ctrl_fds[1]) || (i == data_fds[1]) || (i == server->localClient->fd)) 
			{
				comm_set_nb(i);
			}
			else
			{
#ifdef __CYGWIN__
				if(i > 2)	/* don't close std* */
#endif
					close(i);
			}
		}

		ircsnprintf(fd_str[0], sizeof(fd_str[0]), "%d", ctrl_fds[1]);
		ircsnprintf(fd_str[1], sizeof(fd_str[1]), "%d", data_fds[1]);
		ircsnprintf(fd_str[2], sizeof(fd_str[2]), "%d", server->localClient->fd);
		kid_argv[0] = slink;
		kid_argv[1] = fd_str[0];
		kid_argv[2] = fd_str[1];
		kid_argv[3] = fd_str[2];
		kid_argv[4] = NULL;

		/* exec servlink program */
		execv(ConfigFileEntry.servlink_path, kid_argv);

		/* We're still here, abort. */
		_exit(1);
	}
	else
	{
		comm_close(server->localClient->fd);

		/* close the childs end of the pipes */
		close(ctrl_fds[1]);
		close(data_fds[1]);
		
		s_assert(server->localClient);
		server->localClient->ctrlfd = ctrl_fds[0];
		server->localClient->fd = data_fds[0];

		if(!comm_set_nb(server->localClient->fd))
		{
			report_error(NONB_ERROR_MSG,
					get_server_name(server, SHOW_IP),
					log_client_name(server, SHOW_IP),
					errno);
		}

		if(!comm_set_nb(server->localClient->ctrlfd))
		{
			report_error(NONB_ERROR_MSG,
					get_server_name(server, SHOW_IP),
					log_client_name(server, SHOW_IP),
					errno);
		}

		comm_open(server->localClient->ctrlfd, FD_SOCKET, NULL);
		comm_open(server->localClient->fd, FD_SOCKET, NULL);

		read_ctrl_packet(server->localClient->ctrlfd, server);
		read_packet(server->localClient->fd, server);
	}

	return 0;

      fork_error:
	/* this is ugly, but nicer than repeating
	 * about 50 close() statements everywhre... */
	close(data_fds[0]);
	close(data_fds[1]);
	close(ctrl_fds[0]);
	close(ctrl_fds[1]);
	return -1;
}

/*
 * New server connection code
 * Based upon the stuff floating about in s_bsd.c
 *   -- adrian
 */

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
 * initiates a connection to the server through comm_connect_tcp(). The
 * completion of this goes through serv_completed_connection().
 *
 * We return 1 if the connection is attempted, since we don't know whether
 * it suceeded or not, and 0 if it fails in here somewhere.
 */
int
serv_connect(struct server_conf *server_p, struct Client *by)
{
	struct Client *client_p;
	struct irc_sockaddr_storage myipnum; 
	int fd;

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
				     server_p->name, get_server_name(client_p, SHOW_IP));
		if(by && IsPerson(by) && !MyClient(by))
			sendto_one_notice(by, ":Server %s already present from %s",
					  server_p->name, get_server_name(client_p, SHOW_IP));
		return 0;
	}

	/* create a socket for the server connection */
	if((fd = comm_socket(server_p->aftype, SOCK_STREAM, 0, NULL)) < 0)
	{
		/* Eek, failure to create the socket */
		report_error("opening stream socket to %s: %s", 
			     server_p->name, server_p->name, errno);
		return 0;
	}

	/* servernames are always guaranteed under HOSTLEN chars */
	comm_note(fd, "Server: %s", server_p->name);

	/* Create a local client */
	client_p = make_client(NULL);

	/* Copy in the server, hostname, fd
	 * The sockhost may be a hostname, this will be corrected later
	 * -- jilles
	 */
	strlcpy(client_p->name, server_p->name, sizeof(client_p->name));
	strlcpy(client_p->host, server_p->host, sizeof(client_p->host));
	strlcpy(client_p->sockhost, server_p->host, sizeof(client_p->sockhost));
	client_p->localClient->fd = fd;

	/*
	 * Set up the initial server evilness, ripped straight from
	 * connect_server(), so don't blame me for it being evil.
	 *   -- adrian
	 */

	if(!comm_set_buffers(client_p->localClient->fd, READBUF_SIZE))
	{
		report_error(SETBUF_ERROR_MSG,
				get_server_name(client_p, SHOW_IP),
				log_client_name(client_p, SHOW_IP),
				errno);
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
	client_p->serv->up = me.name;
	client_p->serv->upid = me.id;
	SetConnecting(client_p);
	dlinkAddTail(client_p, &client_p->node, &global_client_list);

	/* log */
	ilog(L_SERVER, "Connecting to %s[%s] port %d (%s)", server_p->name, server_p->host, server_p->port,
#ifdef IPV6
			server_p->aftype == AF_INET6 ? "IPv6" :
#endif
			(server_p->aftype == AF_INET ? "IPv4" : "?"));

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
		SET_SS_LEN(myipnum, sizeof(struct sockaddr_in));
	}
	
#ifdef IPV6
	else if((server_p->aftype == AF_INET6) && ServerInfo.specific_ipv6_vhost)
	{
		memcpy(&myipnum, &ServerInfo.ip6, sizeof(myipnum));
		((struct sockaddr_in6 *)&myipnum)->sin6_port = 0;
		myipnum.ss_family = AF_INET6;
		SET_SS_LEN(myipnum, sizeof(struct sockaddr_in6));
	}
#endif
	else
	{
		comm_connect_tcp(client_p->localClient->fd, server_p->host,
				 server_p->port, NULL, 0, serv_connect_callback, 
				 client_p, server_p->aftype, 
				 ConfigFileEntry.connect_timeout);
		 return 1;
	}

	comm_connect_tcp(client_p->localClient->fd, server_p->host,
			 server_p->port, (struct sockaddr *) &myipnum,
			 GET_SS_LEN(myipnum), serv_connect_callback, client_p,
			 myipnum.ss_family, ConfigFileEntry.connect_timeout);

	return 1;
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
serv_connect_callback(int fd, int status, void *data)
{
	struct Client *client_p = data;
	struct server_conf *server_p;
	char *errstr;

	/* First, make sure its a real client! */
	s_assert(client_p != NULL);
	s_assert(client_p->localClient->fd == fd);

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

	/* Next, for backward purposes, record the ip of the server */
	memcpy(&client_p->localClient->ip, &fd_table[fd].connect.hostaddr, sizeof client_p->localClient->ip);
	/* Set sockhost properly now -- jilles */
	inetntop_sock((struct sockaddr *)&fd_table[fd].connect.hostaddr,
			client_p->sockhost, sizeof client_p->sockhost);
	
	/* Check the status */
	if(status != COMM_OK)
	{
		/* COMM_ERR_TIMEOUT wont have an errno associated with it,
		 * the others will.. --fl
		 */
		if(status == COMM_ERR_TIMEOUT)
		{
			sendto_realops_snomask(SNO_GENERAL, is_remote_connect(client_p) ? L_NETWIDE : L_ALL,
					"Error connecting to %s[%s]: %s",
					client_p->name, 
#ifdef HIDE_SERVERS_IPS
					"255.255.255.255",
#else
					client_p->host,
#endif
					comm_errstr(status));
			ilog(L_SERVER, "Error connecting to %s[%s]: %s",
				client_p->name, client_p->sockhost,
				comm_errstr(status));
		}
		else
		{
			errstr = strerror(comm_get_sockerr(fd));
			sendto_realops_snomask(SNO_GENERAL, is_remote_connect(client_p) ? L_NETWIDE : L_ALL,
					"Error connecting to %s[%s]: %s (%s)",
					client_p->name,
#ifdef HIDE_SERVERS_IPS
					"255.255.255.255",
#else
					client_p->host,
#endif
					comm_errstr(status), errstr);
			ilog(L_SERVER, "Error connecting to %s[%s]: %s (%s)",
				client_p->name, client_p->sockhost,
				comm_errstr(status), errstr);
		}

		exit_client(client_p, client_p, &me, comm_errstr(status));
		return;
	}

	/* COMM_OK, so continue the connection procedure */
	/* Get the C/N lines */
	if((server_p = client_p->localClient->att_sconf) == NULL)
	{
		sendto_realops_snomask(SNO_GENERAL, is_remote_connect(client_p) ? L_NETWIDE : L_ALL, "Lost connect{} block for %s",
				get_server_name(client_p, HIDE_IP));
		exit_client(client_p, client_p, &me, "Lost connect{} block");
		return;
	}

	/* Next, send the initial handshake */
	SetHandshake(client_p);

	/* kludge, if we're not using TS6, dont ever send
	 * ourselves as being TS6 capable.
	 */
	if(!EmptyString(server_p->spasswd))
	{
		if(ServerInfo.use_ts6)
			sendto_one(client_p, "PASS %s TS %d :%s", 
				   server_p->spasswd, TS_CURRENT, me.id);
		else
			sendto_one(client_p, "PASS %s :TS",
				   server_p->spasswd);
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
	read_packet(fd, client_p);
}

#ifndef HAVE_SOCKETPAIR
static int
inet_socketpair(int d, int type, int protocol, int sv[2])
{
	struct sockaddr_in addr1, addr2, addr3;
	int addr3_len = sizeof(addr3);
	int fd, rc;
	int port_no = 20000;
	
	if(d != AF_INET || type != SOCK_STREAM || protocol)
	{
		errno = EAFNOSUPPORT;
		return -1;
	}
	if(((sv[0] = socket(AF_INET, SOCK_STREAM, 0)) < 0) || ((sv[1] = socket(AF_INET, SOCK_STREAM, 0)) < 0))
		return -1;
	
	addr1.sin_port = htons(port_no);
	addr1.sin_family = AF_INET;
	addr1.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	while ((rc = bind (sv[0], (struct sockaddr *) &addr1, sizeof (addr1))) < 0 && errno == EADDRINUSE)
		addr1.sin_port = htons(++port_no);
	
	if(rc < 0)
		return -1;
	
	if(listen(sv[0], 1) < 0)
	{
		close(sv[0]);
		close(sv[1]);
		return -1;
	}
	
	addr2.sin_port = htons(port_no);
	addr2.sin_family = AF_INET;
	addr2.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if(connect (sv[1], (struct sockaddr *) &addr2, sizeof (addr2)) < 0) 
	{
		close(sv[0]);
		close(sv[1]);
		return -1;
	}
	
	if((fd = accept(sv[1], (struct sockaddr *) &addr3, &addr3_len)) < 0)
	{
		close(sv[0]);
		close(sv[1]);
		return -1;
	}
	close(sv[0]);
	sv[0] = fd;
	
	return(0);

}
#endif
