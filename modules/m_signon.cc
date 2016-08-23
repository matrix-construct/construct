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
 */

using namespace ircd;

static const char signon_desc[] = "Provides account login/logout support for services";

static void me_svslogin(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void ms_signon(struct MsgBuf *, client::client &, client::client &, int, const char **);

static void send_signon(client::client *, client::client &, const char *, const char *, const char *, unsigned int, const char *);

struct Message svslogin_msgtab = {
	"SVSLOGIN", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, mg_ignore, {me_svslogin, 6}, mg_ignore}
};
struct Message signon_msgtab = {
	"SIGNON", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, {ms_signon, 6}, mg_ignore, mg_ignore, mg_ignore}
};

mapi_clist_av1 signon_clist[] = {
	&svslogin_msgtab, &signon_msgtab, NULL
};

DECLARE_MODULE_AV2(signon, NULL, NULL, signon_clist, NULL, NULL, NULL, NULL, signon_desc);

#define NICK_VALID	1
#define USER_VALID	2
#define HOST_VALID	4

static bool
clean_username(const char *username)
{
	int len = 0;

	for (; *username; username++)
	{
		len++;

		if(!rfc1459::is_user(*username))
			return false;
	}

	if(len > USERLEN)
		return false;

	return true;
}

static bool
clean_host(const char *host)
{
	int len = 0;

	for (; *host; host++)
	{
		len++;

		if(!rfc1459::is_host(*host))
			return false;
	}

	if(len > HOSTLEN)
		return false;

	return true;
}

static void
me_svslogin(struct MsgBuf *msgbuf_p, client::client &client, client::client &source,
	int parc, const char *parv[])
{
	client::client *target_p, *exist_p;
	char nick[NICKLEN+1], login[NICKLEN+1];
	char user[USERLEN+1], host[HOSTLEN+1];
	int valid = 0;

	if(!(source.flags & client::flags::SERVICE))
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
			"Non-service server %s attempting to execute services-only command SVSLOGIN", source.name);
		return;
	}

	if((target_p = find_client(parv[1])) == NULL)
		return;

	if(!my(*target_p) && !is_unknown(*target_p))
		return;

	if(client::clean_nick(parv[2], 0))
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
			rb_strlcpy(login, suser(ircd::user(*target_p)).c_str(), NICKLEN + 1);
		else
			login[0] = '\0';
	}
	else if(!strcmp(parv[5], "0"))
		login[0] = '\0';
	else
		rb_strlcpy(login, parv[5], NICKLEN + 1);

	/* Login (mostly) follows nick rules. */
	if(*login && !client::clean_nick(login, 0))
		return;

	if((exist_p = client::find_person(nick)) && target_p != exist_p)
	{
		char buf[BUFSIZE];

		if(my(*exist_p))
			sendto_one(exist_p, ":%s KILL %s :(Nickname regained by services)",
				me.name, exist_p->name);

		exist_p->flags |= client::flags::KILLED;
		kill_client_serv_butone(NULL, exist_p, "%s (Nickname regained by services)",
					me.name);
		sendto_realops_snomask(SNO_SKILL, L_ALL,
				"Nick collision due to SVSLOGIN on %s",
				nick);

		snprintf(buf, sizeof(buf), "Killed (%s (Nickname regained by services))",
			me.name);
		exit_client(NULL, exist_p, &me, buf);
	}
	else if((exist_p = find_client(nick)) && is_unknown(*exist_p) && exist_p != target_p)
	{
		exit_client(NULL, exist_p, &me, "Overridden");
	}

	if(*login)
	{
		/* Strip leading digits, unless it's purely numeric. */
		const char *p = login;
		while(rfc1459::is_digit(*p))
			p++;
		if(!*p)
			p = login;

		sendto_one(target_p, form_str(RPL_LOGGEDIN), me.name, EmptyString(target_p->name) ? "*" : target_p->name,
				nick, user, host, p, p);
	}
	else
		sendto_one(target_p, form_str(RPL_LOGGEDOUT), me.name, EmptyString(target_p->name) ? "*" : target_p->name,
				nick, user, host);

	if(is_unknown(*target_p))
	{
		if(valid & NICK_VALID)
			rb_strlcpy(target_p->preClient->spoofnick, nick, sizeof(target_p->preClient->spoofnick));

		if(valid & USER_VALID)
			rb_strlcpy(target_p->preClient->spoofuser, user, sizeof(target_p->preClient->spoofuser));

		if(valid & HOST_VALID)
			rb_strlcpy(target_p->preClient->spoofhost, host, sizeof(target_p->preClient->spoofhost));

		make_user(*target_p);
	}
	else
	{
		char note[NICKLEN + 10];

		send_signon(NULL, *target_p, nick, user, host, rb_current_time(), login);

		snprintf(note, NICKLEN + 10, "Nick: %s", target_p->name);
		rb_note(target_p->localClient->F, note);
	}
}

static void
ms_signon(struct MsgBuf *msgbuf_p, client::client &client, client::client &source,
	int parc, const char *parv[])
{
	client::client *target_p;
	int newts, sameuser;
	char login[NICKLEN+1];

	if(!client::clean_nick(parv[1], 0))
	{
		ServerStats.is_kill++;
		sendto_realops_snomask(SNO_DEBUG, L_ALL,
				"Bad Nick from SIGNON: %s From: %s(via %s)",
				parv[1], source.servptr->name, client.name);
		/* if &source has an id, kill_client_serv_butone() will
		 * send a kill to &client, otherwise do it here */
		if (!has_id(&source))
			sendto_one(&client, ":%s KILL %s :%s (Bad nickname from SIGNON)",
				get_id(&me, &client), parv[1], me.name);
		kill_client_serv_butone(&client, &source, "%s (Bad nickname from SIGNON)",
				me.name);
		source.flags |= client::flags::KILLED;
		exit_client(NULL, &source, &me, "Bad nickname from SIGNON");
		return;
	}

	if(!clean_username(parv[2]) || !clean_host(parv[3]))
	{
		ServerStats.is_kill++;
		sendto_realops_snomask(SNO_DEBUG, L_ALL,
				"Bad user@host from SIGNON: %s@%s From: %s(via %s)",
				parv[2], parv[3], source.servptr->name, client.name);
		/* if &source has an id, kill_client_serv_butone() will
		 * send a kill to &client, otherwise do it here */
		if (!has_id(&source))
			sendto_one(&client, ":%s KILL %s :%s (Bad user@host from SIGNON)",
				get_id(&me, &client), parv[1], me.name);
		kill_client_serv_butone(&client, &source, "%s (Bad user@host from SIGNON)",
				me.name);
		source.flags |= client::flags::KILLED;
		exit_client(NULL, &source, &me, "Bad user@host from SIGNON");
		return;
	}

	newts = atol(parv[4]);

	if(!strcmp(parv[5], "0"))
		login[0] = '\0';
	else if(*parv[5] != '*')
	{
		if (client::clean_nick(parv[5], 0))
			rb_strlcpy(login, parv[5], NICKLEN + 1);
		else
			return;
	}
	else
		login[0] = '\0';

	target_p = find_named_client(parv[1]);
	if(target_p != NULL && target_p != &source)
	{
 		/* In case of collision, follow NICK rules. */
		/* XXX this is duplicated code and does not do SAVE */
		if(is_unknown(*target_p))
			exit_client(NULL, target_p, &me, "Overridden");
		else
		{
			if(!newts || !target_p->tsinfo || (newts == target_p->tsinfo) || !source.user)
			{
				sendto_realops_snomask(SNO_GENERAL, L_ALL,
						     "Nick change collision from SIGNON from %s to %s(%s <- %s)(both killed)",
						     source.name, target_p->name, target_p->from->name,
						     client.name);

				ServerStats.is_kill++;
				sendto_one_numeric(target_p, ERR_NICKCOLLISION,
						   form_str(ERR_NICKCOLLISION), target_p->name);

				kill_client_serv_butone(NULL, &source, "%s (Nick change collision)", me.name);

				ServerStats.is_kill++;

				kill_client_serv_butone(NULL, target_p, "%s (Nick change collision)", me.name);

				target_p->flags |= client::flags::KILLED;
				exit_client(NULL, target_p, &me, "Nick collision(new)");
				source.flags |= client::flags::KILLED;
				exit_client(&client, &source, &me, "Nick collision(old)");
				return;
			}
			else
			{
				sameuser = !irccmp(target_p->username, source.username) &&
					!irccmp(target_p->host, source.host);

				if((sameuser && newts < target_p->tsinfo) ||
				   (!sameuser && newts > target_p->tsinfo))
				{
					if(sameuser)
						sendto_realops_snomask(SNO_GENERAL, L_ALL,
								     "Nick change collision from SIGNON from %s to %s(%s <- %s)(older killed)",
								     source.name, target_p->name,
								     target_p->from->name, client.name);
					else
						sendto_realops_snomask(SNO_GENERAL, L_ALL,
								     "Nick change collision from SIGNON from %s to %s(%s <- %s)(newer killed)",
								     source.name, target_p->name,
								     target_p->from->name, client.name);

					ServerStats.is_kill++;

					sendto_one_numeric(target_p, ERR_NICKCOLLISION,
							   form_str(ERR_NICKCOLLISION), target_p->name);

					/* kill the client issuing the nickchange */
					kill_client_serv_butone(&client, &source,
								"%s (Nick change collision)", me.name);

					source.flags |= client::flags::KILLED;

					if(sameuser)
						exit_client(&client, &source, &me, "Nick collision(old)");
					else
						exit_client(&client, &source, &me, "Nick collision(new)");
					return;
				}
				else
				{
					if(sameuser)
						sendto_realops_snomask(SNO_GENERAL, L_ALL,
								     "Nick collision from SIGNON on %s(%s <- %s)(older killed)",
								     target_p->name, target_p->from->name,
								     client.name);
					else
						sendto_realops_snomask(SNO_GENERAL, L_ALL,
								     "Nick collision from SIGNON on %s(%s <- %s)(newer killed)",
								     target_p->name, target_p->from->name,
								     client.name);

					sendto_one_numeric(target_p, ERR_NICKCOLLISION,
							   form_str(ERR_NICKCOLLISION), target_p->name);

					/* kill the client who existed before hand */
					kill_client_serv_butone(&client, target_p,
							"%s (Nick collision)", me.name);

					ServerStats.is_kill++;

					target_p->flags |= client::flags::KILLED;
					(void) exit_client(&client, target_p, &me, "Nick collision");
				}
			}

		}
	}

	send_signon(&client, source, parv[1], parv[2], parv[3], newts, login);
}

static void
send_signon(client::client *client, client::client &target,
		const char *nick, const char *username, const char *host,
		unsigned int newts, const char *login)
{
	sendto_server(client, NULL, CAP_TS6, NOCAPS, ":%s SIGNON %s %s %s %ld %s",
			use_id(&target), nick, username, host,
			long(target.tsinfo), *login ? login : "0");

	suser(user(target)) = login;

	change_nick_user_host(&target, nick, username, host, newts, "Signing %s (%s)", *login ?  "in" : "out", nick);
}
