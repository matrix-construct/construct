/* modules/m_signon.c
 *   Copyright (C) 2006 Michael Tharp <gxti@partiallystapled.com>
 *   Copyright (C) 2006 charybdis development team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1.Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 2.Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3.The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: m_signon.c 1192 2006-04-21 16:21:02Z jilles $
 */

#include "stdinc.h"

#include "send.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "config.h"
#include "ircd.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_serv.h"
#include "hash.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "whowas.h"
#include "monitor.h"
#include "s_stats.h"
#include "snomask.h"
#include "match.h"
#include "s_user.h"

static int me_svslogin(struct Client *, struct Client *, int, const char **);
static int ms_signon(struct Client *, struct Client *, int, const char **);

static void send_signon(struct Client *, struct Client *, const char *, const char *, const char *, unsigned int, const char *);

struct Message svslogin_msgtab = {
	"SVSLOGIN", 0, 0, 0, MFLG_SLOW,
	{mg_ignore, mg_ignore, mg_ignore, mg_ignore, {me_svslogin, 6}, mg_ignore}
};
struct Message signon_msgtab = {
	"SIGNON", 0, 0, 0, MFLG_SLOW,
	{mg_ignore, mg_ignore, {ms_signon, 6}, mg_ignore, mg_ignore, mg_ignore}
};

mapi_clist_av1 signon_clist[] = { 
	&svslogin_msgtab, &signon_msgtab, NULL
};

DECLARE_MODULE_AV1(signon, NULL, NULL, signon_clist, NULL, NULL, "$Revision: 1192 $");

#define NICK_VALID	1
#define USER_VALID	2
#define HOST_VALID	4

static int
clean_nick(const char *nick)
{
	int len = 0;

	if(*nick == '-')
		return 0;

	/* This is used to check logins, which are often
	 * numeric. Don't check for leading digits, if
	 * services wants to set someone's nick to something
	 * starting with a number, let it try.
	 * --gxti
	 */

	for (; *nick; nick++)
	{
		len++;
		if(!IsNickChar(*nick))
			return 0;
	}

	/* nicklen is +1 */
	if(len >= NICKLEN)
		return 0;

	return 1;
}

static int
clean_username(const char *username)
{
	int len = 0;

	for (; *username; username++)
	{
		len++;

		if(!IsUserChar(*username))
			return 0;
	}

	if(len > USERLEN)
		return 0;

	return 1;
}

static int
clean_host(const char *host)
{
	int len = 0;

	for (; *host; host++)
	{
		len++;

		if(!IsHostChar(*host))
			return 0;
	}

	if(len > HOSTLEN)
		return 0;

	return 1;
}

static int
me_svslogin(struct Client *client_p, struct Client *source_p,
	int parc, const char *parv[])
{
	struct Client *target_p, *exist_p;
	char nick[NICKLEN+1], login[NICKLEN+1];
	char user[USERLEN+1], host[HOSTLEN+1];
	int valid = 0;

	if(!(source_p->flags & FLAGS_SERVICE))
		return 0;

	if((target_p = find_client(parv[1])) == NULL)
		return 0;

	if(!MyClient(target_p) && !IsUnknown(target_p))
		return 0;

	if(clean_nick(parv[2]))
	{
		rb_strlcpy(nick, parv[2], NICKLEN + 1);
		valid |= NICK_VALID;
	}
	else if(*target_p->name)
		rb_strlcpy(nick, target_p->name, NICKLEN + 1);
	else
		strcpy(nick, "*");

	if(clean_username(parv[3]))
	{
		rb_strlcpy(user, parv[3], USERLEN + 1);
		valid |= USER_VALID;
	}
	else
		rb_strlcpy(user, target_p->username, USERLEN + 1);

	if(clean_host(parv[4]))
	{
		rb_strlcpy(host, parv[4], HOSTLEN + 1);
		valid |= HOST_VALID;
	}
	else
		rb_strlcpy(host, target_p->host, HOSTLEN + 1);

	if(*parv[5] == '*')
	{
		if(target_p->user)
			rb_strlcpy(login, target_p->user->suser, NICKLEN + 1);
		else
			login[0] = '\0';
	}
	else if(!strcmp(parv[5], "0"))
		login[0] = '\0';
	else
		rb_strlcpy(login, parv[5], NICKLEN + 1);

	/* Login (mostly) follows nick rules. */
	if(*login && !clean_nick(login))
		return 0;

	if((exist_p = find_person(nick)) && target_p != exist_p)
	{
		char buf[BUFSIZE];

		if(MyClient(exist_p))
			sendto_one(exist_p, ":%s KILL %s :(Nickname regained by services)",
				me.name, exist_p->name);

		exist_p->flags |= FLAGS_KILLED;
		kill_client_serv_butone(NULL, exist_p, "%s (Nickname regained by services)",
					me.name);

		rb_snprintf(buf, sizeof(buf), "Killed (%s (Nickname regained by services))",
			me.name);
		exit_client(NULL, exist_p, &me, buf);
	}else if((exist_p = find_client(nick)) && IsUnknown(exist_p) && exist_p != target_p) {
		exit_client(NULL, exist_p, &me, "Overridden");
	}

	if(*login)
	{
		/* Strip leading digits, unless it's purely numeric. */
		const char *p = login;
		while(IsDigit(*p))
			p++;
		if(!*p)
			p = login;

		sendto_one(target_p, form_str(RPL_LOGGEDIN), me.name, EmptyString(target_p->name) ? "*" : target_p->name,
				nick, user, host, p, p);
	}
	else
		sendto_one(target_p, form_str(RPL_LOGGEDOUT), me.name, EmptyString(target_p->name) ? "*" : target_p->name,
				nick, user, host);

	if(IsUnknown(target_p))
	{
		struct User *user_p = make_user(target_p);

		if(valid & NICK_VALID)
			strcpy(target_p->preClient->spoofnick, nick);

		if(valid & USER_VALID)
			strcpy(target_p->preClient->spoofuser, user);

		if(valid & HOST_VALID)
			strcpy(target_p->preClient->spoofhost, host);

		rb_strlcpy(user_p->suser, login, NICKLEN + 1);
	}
	else
	{
		char note[NICKLEN + 10];

		send_signon(NULL, target_p, nick, user, host, rb_current_time(), login);

		rb_snprintf(note, NICKLEN + 10, "Nick: %s", target_p->name);
		rb_note(target_p->localClient->F, note);
	}

	return 0;
}

static int
ms_signon(struct Client *client_p, struct Client *source_p,
	int parc, const char *parv[])
{
	struct Client *target_p;
	int newts, sameuser;
	char login[NICKLEN+1];

	if(!clean_nick(parv[1]))
	{
		ServerStats.is_kill++;
		sendto_realops_snomask(SNO_DEBUG, L_ALL,
				"Bad Nick from SIGNON: %s From: %s(via %s)",
				parv[1], source_p->servptr->name, client_p->name);
		/* if source_p has an id, kill_client_serv_butone() will
		 * send a kill to client_p, otherwise do it here */
		if (!has_id(source_p))
			sendto_one(client_p, ":%s KILL %s :%s (Bad nickname from SIGNON)",
				get_id(&me, client_p), parv[1], me.name);
		kill_client_serv_butone(client_p, source_p, "%s (Bad nickname from SIGNON)",
				me.name);
		source_p->flags |= FLAGS_KILLED;
		exit_client(NULL, source_p, &me, "Bad nickname from SIGNON");
		return 0;
	}

	if(!clean_username(parv[2]) || !clean_host(parv[3]))
	{
		ServerStats.is_kill++;
		sendto_realops_snomask(SNO_DEBUG, L_ALL,
				"Bad user@host from SIGNON: %s@%s From: %s(via %s)",
				parv[2], parv[3], source_p->servptr->name, client_p->name);
		/* if source_p has an id, kill_client_serv_butone() will
		 * send a kill to client_p, otherwise do it here */
		if (!has_id(source_p))
			sendto_one(client_p, ":%s KILL %s :%s (Bad user@host from SIGNON)",
				get_id(&me, client_p), parv[1], me.name);
		kill_client_serv_butone(client_p, source_p, "%s (Bad user@host from SIGNON)",
				me.name);
		source_p->flags |= FLAGS_KILLED;
		exit_client(NULL, source_p, &me, "Bad user@host from SIGNON");
		return 0;
	}

	newts = atol(parv[4]);

	if(!strcmp(parv[5], "0"))
		login[0] = '\0';
	else if(*parv[5] != '*')
	{
		if (clean_nick(parv[5]))
			rb_strlcpy(login, parv[5], NICKLEN + 1);
		else
			return 0;
	}

	target_p = find_named_client(parv[1]);
	if(target_p != NULL && target_p != source_p)
	{
 		/* In case of collision, follow NICK rules. */
		/* XXX this is duplicated code and does not do SAVE */
		if(IsUnknown(target_p))
			exit_client(NULL, target_p, &me, "Overridden");
		else
		{
			if(!newts || !target_p->tsinfo || (newts == target_p->tsinfo) || !source_p->user)
			{
				sendto_realops_snomask(SNO_GENERAL, L_ALL,
						     "Nick change collision from SIGNON from %s to %s(%s <- %s)(both killed)",
						     source_p->name, target_p->name, target_p->from->name,
						     client_p->name);
		
				ServerStats.is_kill++;
				sendto_one_numeric(target_p, ERR_NICKCOLLISION,
						   form_str(ERR_NICKCOLLISION), target_p->name);
		
				kill_client_serv_butone(NULL, source_p, "%s (Nick change collision)", me.name);
		
				ServerStats.is_kill++;
		
				kill_client_serv_butone(NULL, target_p, "%s (Nick change collision)", me.name);
		
				target_p->flags |= FLAGS_KILLED;
				exit_client(NULL, target_p, &me, "Nick collision(new)");
				source_p->flags |= FLAGS_KILLED;
				exit_client(client_p, source_p, &me, "Nick collision(old)");
				return 0;
			}
			else
			{
				sameuser = !irccmp(target_p->username, source_p->username) &&
					!irccmp(target_p->host, source_p->host);
		
				if((sameuser && newts < target_p->tsinfo) ||
				   (!sameuser && newts > target_p->tsinfo))
				{
					if(sameuser)
						sendto_realops_snomask(SNO_GENERAL, L_ALL,
								     "Nick change collision from SIGNON from %s to %s(%s <- %s)(older killed)",
								     source_p->name, target_p->name,
								     target_p->from->name, client_p->name);
					else
						sendto_realops_snomask(SNO_GENERAL, L_ALL,
								     "Nick change collision from SIGNON from %s to %s(%s <- %s)(newer killed)",
								     source_p->name, target_p->name,
								     target_p->from->name, client_p->name);
		
					ServerStats.is_kill++;
		
					sendto_one_numeric(target_p, ERR_NICKCOLLISION,
							   form_str(ERR_NICKCOLLISION), target_p->name);
		
					/* kill the client issuing the nickchange */
					kill_client_serv_butone(client_p, source_p,
								"%s (Nick change collision)", me.name);
		
					source_p->flags |= FLAGS_KILLED;
		
					if(sameuser)
						exit_client(client_p, source_p, &me, "Nick collision(old)");
					else
						exit_client(client_p, source_p, &me, "Nick collision(new)");
					return 0;
				}
				else
				{
					if(sameuser)
						sendto_realops_snomask(SNO_GENERAL, L_ALL,
								     "Nick collision from SIGNON on %s(%s <- %s)(older killed)",
								     target_p->name, target_p->from->name,
								     client_p->name);
					else
						sendto_realops_snomask(SNO_GENERAL, L_ALL,
								     "Nick collision from SIGNON on %s(%s <- %s)(newer killed)",
								     target_p->name, target_p->from->name,
								     client_p->name);
		
					sendto_one_numeric(target_p, ERR_NICKCOLLISION,
							   form_str(ERR_NICKCOLLISION), target_p->name);
		
					/* kill the client who existed before hand */
					kill_client_serv_butone(client_p, target_p, 
							"%s (Nick collision)", me.name);
		
					ServerStats.is_kill++;
		
					target_p->flags |= FLAGS_KILLED;
					(void) exit_client(client_p, target_p, &me, "Nick collision");
				}
			}
		
		}
	}

	send_signon(client_p, source_p, parv[1], parv[2], parv[3], newts, login);
	return 0;
}

static void
send_signon(struct Client *client_p, struct Client *target_p,
		const char *nick, const char *user, const char *host,
		unsigned int newts, const char *login)
{
	sendto_server(client_p, NULL, CAP_TS6, NOCAPS, ":%s SIGNON %s %s %s %ld %s",
			use_id(target_p), nick, user, host,
			(long) target_p->tsinfo, *login ? login : "0");

	strcpy(target_p->user->suser, login);

	change_nick_user_host(target_p, nick, user, host, newts, "Signing %s (%s)", *login ?  "in" : "out", nick);
}

