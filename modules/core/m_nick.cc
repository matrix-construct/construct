/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_nick.c: Sets a users nick.
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

/* Give all UID nicks the same TS. This ensures nick TS is always the same on
 * all servers for each nick-user pair, also if a user with a UID nick changes
 * their nick but is collided again (the server detecting the collision will
 * not propagate the nick change further). -- jilles
 */
#define SAVE_NICKTS 100

static void mr_nick(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void m_nick(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void mc_nick(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void ms_nick(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void ms_uid(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void ms_euid(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void ms_save(struct MsgBuf *, client::client &, client::client &, int, const char **);
static bool can_save(client::client *);
static void save_user(client::client &, client::client &, client::client *);
static void bad_nickname(client::client &, const char *);
static void change_remote_nick(client::client &, client::client &, time_t,
			      const char *, int);
static bool clean_username(const char *);
static bool clean_host(const char *);
static bool clean_uid(const char *uid, const char *sid);

static void set_initial_nick(client::client &client, client::client &source, char *nick);
static void change_local_nick(client::client &client, client::client &source, char *nick, int);
static void register_client(client::client &client, client::client *server,
			   const char *nick, time_t newts, int parc, const char *parv[]);
static void perform_nick_collides(client::client &, client::client &,
				  client::client *, int, const char **,
				  time_t, const char *, const char *);
static void perform_nickchange_collides(client::client &, client::client &,
					client::client *, int, const char **, time_t, const char *);

struct Message nick_msgtab = {
	"NICK", 0, 0, 0, 0,
	{{mr_nick, 0}, {m_nick, 0}, {mc_nick, 3}, {ms_nick, 0}, mg_ignore, {m_nick, 0}}
};
struct Message uid_msgtab = {
	"UID", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, {ms_uid, 9}, mg_ignore, mg_ignore}
};
struct Message euid_msgtab = {
	"EUID", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, {ms_euid, 12}, mg_ignore, mg_ignore}
};
struct Message save_msgtab = {
	"SAVE", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, {ms_save, 3}, mg_ignore, mg_ignore}
};

mapi_clist_av1 nick_clist[] = { &nick_msgtab, &uid_msgtab, &euid_msgtab,
	&save_msgtab, NULL };

static const char nick_desc[] =
	"Provides the NICK client and server commands as well as the UID, EUID, and SAVE TS6 server commands";

DECLARE_MODULE_AV2(nick, NULL, NULL, nick_clist, NULL, NULL, NULL, NULL, nick_desc);

/* mr_nick()
 *       parv[1] = nickname
 */
static void
mr_nick(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	client::client *target_p;
	char nick[NICKLEN];

	if (strlen(client.id) == 3)
	{
		exit_client(&client, &client, &client, "Mixing client and server protocol");
		return;
	}

	if(parc < 2 || EmptyString(parv[1]))
	{
		sendto_one(&source, form_str(ERR_NONICKNAMEGIVEN),
			   me.name, EmptyString(source.name) ? "*" : source.name);
		return;
	}

	/* copy the nick and terminate it */
	rb_strlcpy(nick, parv[1], ConfigFileEntry.nicklen);

	/* check the nickname is ok */
	if(!client::clean_nick(nick, 1))
	{
		sendto_one(&source, form_str(ERR_ERRONEUSNICKNAME),
			   me.name, EmptyString(source.name) ? "*" : source.name, parv[1]);
		return;
	}

	/* check if the nick is resv'd */
	if(find_nick_resv(nick))
	{
		sendto_one(&source, form_str(ERR_ERRONEUSNICKNAME),
			   me.name, EmptyString(source.name) ? "*" : source.name, nick);
		return;
	}

	if(rb_dictionary_find(nd_dict, nick))
	{
		sendto_one(&source, form_str(ERR_UNAVAILRESOURCE),
			   me.name, EmptyString(source.name) ? "*" : source.name, nick);
		return;
	}

	if((target_p = find_named_client(nick)) == NULL)
		set_initial_nick(client, source, nick);
	else if(&source == target_p)
		rb_strlcpy(source.name, nick, sizeof(source.name));
	else
		sendto_one(&source, form_str(ERR_NICKNAMEINUSE), me.name, "*", nick);
}

/* m_nick()
 *     parv[1] = nickname
 */
static void
m_nick(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	client::client *target_p;
	char nick[NICKLEN];

	if(parc < 2 || EmptyString(parv[1]))
	{
		sendto_one(&source, form_str(ERR_NONICKNAMEGIVEN), me.name, source.name);
		return;
	}

	/* mark end of grace period, to prevent nickflooding */
	if(!is_flood_done(source))
		flood_endgrace(&source);

	/* terminate nick to NICKLEN, we dont want client::clean_nick() to error! */
	rb_strlcpy(nick, parv[1], ConfigFileEntry.nicklen);

	/* check the nickname is ok */
	if(!client::clean_nick(nick, 1))
	{
		sendto_one(&source, form_str(ERR_ERRONEUSNICKNAME), me.name, source.name, nick);
		return;
	}

	if(!is_exempt_resv(source) && find_nick_resv(nick))
	{
		sendto_one(&source, form_str(ERR_ERRONEUSNICKNAME), me.name, source.name, nick);
		return;
	}

	if(rb_dictionary_find(nd_dict, nick))
	{
		sendto_one(&source, form_str(ERR_UNAVAILRESOURCE),
			   me.name, EmptyString(source.name) ? "*" : source.name, nick);
		return;
	}

	if((target_p = find_named_client(nick)))
	{
		/* If(target_p == &source) the client is changing nicks between
		 * equivalent nicknames ie: [nick] -> {nick}
		 */
		if(target_p == &source)
		{
			/* check the nick isnt exactly the same */
			if(strcmp(target_p->name, nick))
				change_local_nick(client, source, nick, 1);

		}

		/* drop unregged client */
		else if(is_unknown(*target_p))
		{
			exit_client(NULL, target_p, &me, "Overridden");
			change_local_nick(client, source, nick, 1);
		}
		else
			sendto_one(&source, form_str(ERR_NICKNAMEINUSE), me.name, source.name, nick);

		return;
	}
	else
		change_local_nick(client, source, nick, 1);
}

/* mc_nick()
 *
 * server -> server nick change
 *    parv[1] = nickname
 *    parv[2] = TS when nick change
 */
static void
mc_nick(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	client::client *target_p;
	time_t newts = 0;

	/* if nicks erroneous, or too long, kill */
	if(!client::clean_nick(parv[1], 0))
	{
		bad_nickname(client, parv[1]);
		return;
	}

	newts = atol(parv[2]);
	target_p = find_named_client(parv[1]);

	/* if the nick doesnt exist, allow it and process like normal */
	if(target_p == NULL)
	{
		change_remote_nick(client, source, newts, parv[1], 1);
	}
	else if(is_unknown(*target_p))
	{
		exit_client(NULL, target_p, &me, "Overridden");
		change_remote_nick(client, source, newts, parv[1], 1);
	}
	else if(target_p == &source)
	{
		/* client changing case of nick */
		if(strcmp(target_p->name, parv[1]))
			change_remote_nick(client, source, newts, parv[1], 1);
	}
	/* we've got a collision! */
	else
		perform_nickchange_collides(source, client, target_p,
					    parc, parv, newts, parv[1]);
}

static void
ms_nick(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	const char *nick, *server;

	nick = parc > 1 ? parv[1] : "?";
	server = parc > 7 ? parv[7] : "?";

	sendto_wallops_flags(umode::WALLOP, &me,
			"Link %s cancelled, TS5 nickname %s on %s introduced (old server?)",
			client.name, nick, server);
	sendto_server(NULL, NULL, CAP_TS6, NOCAPS,
			":%s WALLOPS :Link %s cancelled, TS5 nickname %s on %s introduced (old server?)",
			me.id, client.name, nick, server);
	ilog(L_SERVER, "Link %s cancelled, TS5 nickname %s on %s introduced (old server?)",
			client.name, nick, server);

	exit_client(&client, &client, &me, "TS5 nickname introduced");
}

/* ms_uid()
 *     parv[1] - nickname
 *     parv[2] - hops
 *     parv[3] - TS
 *     parv[4] - umodes
 *     parv[5] - username
 *     parv[6] - hostname
 *     parv[7] - IP
 *     parv[8] - UID
 *     parv[9] - gecos
 */
static void
ms_uid(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	client::client *target_p;
	time_t newts = 0;
	char squitreason[120];

	newts = atol(parv[3]);

	if(parc != 10)
	{
		sendto_realops_snomask(sno::GENERAL, L_ALL,
				     "Dropping server %s due to (invalid) command 'UID' "
				     "with %d arguments (expecting 10)", client.name, parc);
		ilog(L_SERVER, "Excess parameters (%d) for command 'UID' from %s.",
		     parc, client.name);
		snprintf(squitreason, sizeof squitreason,
				"Excess parameters (%d) to %s command, expecting %d",
				parc, "UID", 10);
		exit_client(&client, &client, &client, squitreason);
		return;
	}

	/* if nicks erroneous, or too long, kill */
	if(!client::clean_nick(parv[1], 0))
	{
		bad_nickname(client, parv[1]);
		return;
	}

	if(!clean_uid(parv[8], source.id))
	{
		snprintf(squitreason, sizeof squitreason,
				"Invalid UID %s for nick %s on %s/%s",
				parv[8], parv[1], source.name, source.id);
		exit_client(&client, &client, &client, squitreason);
		return;
	}

	if(!clean_username(parv[5]) || !clean_host(parv[6]))
	{
		ServerStats.is_kill++;
		sendto_realops_snomask(sno::DEBUG, L_ALL,
				     "Bad user@host: %s@%s From: %s(via %s)",
				     parv[5], parv[6], source.name, client.name);
		sendto_one(&client, ":%s KILL %s :%s (Bad user@host)", me.id, parv[8], me.name);
		return;
	}

	/* check length of clients gecos */
	if(strlen(parv[9]) > REALLEN)
	{
		char *s = LOCAL_COPY(parv[9]);
		sendto_realops_snomask(sno::GENERAL, L_ALL, "Long realname from server %s for %s",
				     source.name, parv[1]);
		s[REALLEN] = '\0';
		parv[9] = s;
	}

	target_p = find_named_client(parv[1]);

	if(target_p == NULL)
	{
		register_client(client, &source, parv[1], newts, parc, parv);
	}
	else if(is_unknown(*target_p))
	{
		exit_client(NULL, target_p, &me, "Overridden");
		register_client(client, &source, parv[1], newts, parc, parv);
	}
	/* we've got a collision! */
	else
		perform_nick_collides(source, client, target_p, parc, parv,
				      newts, parv[1], parv[8]);
}

/* ms_euid()
 *     parv[1] - nickname
 *     parv[2] - hops
 *     parv[3] - TS
 *     parv[4] - umodes
 *     parv[5] - username
 *     parv[6] - hostname
 *     parv[7] - IP
 *     parv[8] - UID
 *     parv[9] - realhost
 *     parv[10] - account
 *     parv[11] - gecos
 */
static void
ms_euid(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	client::client *target_p;
	time_t newts = 0;
	char squitreason[120];

	newts = atol(parv[3]);

	if(parc != 12)
	{
		sendto_realops_snomask(sno::GENERAL, L_ALL,
				     "Dropping server %s due to (invalid) command 'EUID' "
				     "with %d arguments (expecting 12)", client.name, parc);
		ilog(L_SERVER, "Excess parameters (%d) for command 'EUID' from %s.",
		     parc, client.name);
		snprintf(squitreason, sizeof squitreason,
				"Excess parameters (%d) to %s command, expecting %d",
				parc, "EUID", 12);
		exit_client(&client, &client, &client, squitreason);
		return;
	}

	/* if nicks erroneous, or too long, kill */
	if(!client::clean_nick(parv[1], 0))
	{
		bad_nickname(client, parv[1]);
		return;
	}

	if(!clean_uid(parv[8], source.id))
	{
		snprintf(squitreason, sizeof squitreason,
				"Invalid UID %s for nick %s on %s/%s",
				parv[8], parv[1], source.name, source.id);
		exit_client(&client, &client, &client, squitreason);
		return;
	}

	if(!clean_username(parv[5]) || !clean_host(parv[6]))
	{
		ServerStats.is_kill++;
		sendto_realops_snomask(sno::DEBUG, L_ALL,
				     "Bad user@host: %s@%s From: %s(via %s)",
				     parv[5], parv[6], source.name, client.name);
		sendto_one(&client, ":%s KILL %s :%s (Bad user@host)", me.id, parv[8], me.name);
		return;
	}

	if(strcmp(parv[9], "*") && !clean_host(parv[9]))
	{
		ServerStats.is_kill++;
		sendto_realops_snomask(sno::DEBUG, L_ALL,
				     "Bad realhost: %s From: %s(via %s)",
				     parv[9], source.name, client.name);
		sendto_one(&client, ":%s KILL %s :%s (Bad user@host)", me.id, parv[8], me.name);
		return;
	}

	/* check length of clients gecos */
	if(strlen(parv[11]) > REALLEN)
	{
		char *s = LOCAL_COPY(parv[11]);
		sendto_realops_snomask(sno::GENERAL, L_ALL, "Long realname from server %s for %s",
				     source.name, parv[1]);
		s[REALLEN] = '\0';
		parv[11] = s;
	}

	target_p = find_named_client(parv[1]);

	if(target_p == NULL)
	{
		register_client(client, &source, parv[1], newts, parc, parv);
	}
	else if(is_unknown(*target_p))
	{
		exit_client(NULL, target_p, &me, "Overridden");
		register_client(client, &source, parv[1], newts, parc, parv);
	}
	/* we've got a collision! */
	else
		perform_nick_collides(source, client, target_p, parc, parv,
				      newts, parv[1], parv[8]);
}

/* ms_save()
 *   parv[1] - UID
 *   parv[2] - TS
 */
static void
ms_save(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	client::client *target_p;

	target_p = find_id(parv[1]);
	if (target_p == NULL)
		return;
	if (!is_person(*target_p))
		sendto_realops_snomask(sno::GENERAL, L_ALL,
				"Ignored SAVE message for non-person %s from %s",
				target_p->name, source.name);
	else if (rfc1459::is_digit(target_p->name[0]))
		sendto_realops_snomask(sno::DEBUG, L_ALL,
				"Ignored noop SAVE message for %s from %s",
				target_p->name, source.name);
	else if (target_p->tsinfo == atol(parv[2]))
		save_user(client, source, target_p);
	else
		sendto_realops_snomask(sno::SKILL, L_ALL,
				"Ignored SAVE message for %s from %s",
				target_p->name, source.name);
}

/* clean_username()
 *
 * input	- username to check
 * output	- false if erroneous, else true
 * side effects -
 */
static bool
clean_username(const char *username)
{
	int len = 0;

	for(; *username; username++)
	{
		len++;

		if(!rfc1459::is_user(*username))
			return false;
	}

	if(len > USERLEN)
		return false;

	return true;
}

/* clean_host()
 *
 * input	- host to check
 * output	- false if erroneous, else true
 * side effects -
 */
static bool
clean_host(const char *host)
{
	int len = 0;

	for(; *host; host++)
	{
		len++;

		if(!rfc1459::is_host(*host))
			return false;
	}

	if(len > HOSTLEN)
		return false;

	return true;
}

static bool
clean_uid(const char *uid, const char *sid)
{
	int len = 1;

	if(strncmp(uid, sid, strlen(sid)))
		return false;

	if(!rfc1459::is_digit(*uid++))
		return false;

	for(; *uid; uid++)
	{
		len++;

		if(!rfc1459::is_id(*uid))
			return false;
	}

	if(len != client::IDLEN - 1)
		return false;

	return true;
}

static void
set_initial_nick(client::client &client, client::client &source, char *nick)
{
	char note[NICKLEN + 10];

	/* This had to be copied here to avoid problems.. */
	source.tsinfo = rb_current_time();
	if(source.name[0])
		del_from_client_hash(source.name, &source);

	rb_strlcpy(source.name, nick, sizeof(source.name));
	add_to_client_hash(nick, &source);

	snprintf(note, sizeof(note), "Nick: %s", nick);
	rb_note(client.localClient->F, note);

	if(source.flags & client::flags::SENTUSER)
	{
		/* got user, heres nick. */
		register_local_user(&client, &source);
	}
}

static void
change_local_nick(client::client &client, client::client &source,
		char *nick, int dosend)
{
	client::client *target_p;
	rb_dlink_node *ptr, *next_ptr;
	chan::chan *chptr;
	char note[NICKLEN + 10];
	int samenick;

	if (dosend)
	{
		chptr = chan::find_bannickchange_channel(&source);
		if (chptr != NULL)
		{
			sendto_one_numeric(&source, ERR_BANNICKCHANGE,
			                   form_str(ERR_BANNICKCHANGE),
			                   nick,
			                   chptr->name.c_str());
			return;
		}
		if((source.localClient->last_nick_change + ConfigFileEntry.max_nick_time) < rb_current_time())
			source.localClient->number_of_nick_changes = 0;

		source.localClient->last_nick_change = rb_current_time();
		source.localClient->number_of_nick_changes++;

		if(ConfigFileEntry.anti_nick_flood && !is(source, umode::OPER) &&
				source.localClient->number_of_nick_changes > ConfigFileEntry.max_nick_changes)
		{
			sendto_one(&source, form_str(ERR_NICKTOOFAST),
					me.name, source.name, source.name,
					nick, ConfigFileEntry.max_nick_time);
			return;
		}
	}

	samenick = irccmp(source.name, nick) ? 0 : 1;

	/* dont reset TS if theyre just changing case of nick */
	if(!samenick)
	{
		/* force the TS to increase -- jilles */
		if (source.tsinfo >= rb_current_time())
			source.tsinfo++;
		else
			source.tsinfo = rb_current_time();
		monitor_signoff(&source);
		/* we only do bancache for local users -- jilles */
		if(source.user)
			chan::invalidate_bancache_user(&source);
	}

	sendto_realops_snomask(sno::NCHANGE, L_ALL,
			     "Nick change: From %s to %s [%s@%s]",
			     source.name, nick, source.username, source.host);

	/* send the nick change to the users channels */
	sendto_common_channels_local(&source, NOCAPS, NOCAPS, ":%s!%s@%s NICK :%s",
				     source.name, source.username, source.host, nick);

	/* send the nick change to servers.. */
	if(source.user)
	{
		whowas::add(source);

		if (dosend)
		{
			sendto_server(&client, NULL, CAP_TS6, NOCAPS, ":%s NICK %s :%ld",
					use_id(&source), nick, (long) source.tsinfo);
		}
	}

	/* Finally, add to hash */
	del_from_client_hash(source.name, &source);
	rb_strlcpy(source.name, nick, sizeof(source.name));
	add_to_client_hash(nick, &source);

	if(!samenick)
		monitor_signon(&source);

	/* Make sure everyone that has this client on its accept list
	 * loses that reference.
	 */
	/* we used to call del_all_accepts() here, but theres no real reason
	 * to clear a clients own list of accepted clients.  So just remove
	 * them from everyone elses list --anfl
	 */
	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, source.on_allow_list.head)
	{
		target_p = (client::client *)ptr->data;

		rb_dlinkFindDestroy(&source, &target_p->localClient->allow_list);
		rb_dlinkDestroy(ptr, &source.on_allow_list);
	}

	snprintf(note, sizeof(note), "Nick: %s", nick);
	rb_note(client.localClient->F, note);

	return;
}

/*
 * change_remote_nick()
 */
static void
change_remote_nick(client::client &client, client::client &source,
		   time_t newts, const char *nick, int dosend)
{
	struct nd_entry *nd;
	int samenick = irccmp(source.name, nick) ? 0 : 1;

	/* client changing their nick - dont reset ts if its same */
	if(!samenick)
	{
		source.tsinfo = newts ? newts : rb_current_time();
		monitor_signoff(&source);
	}

	sendto_common_channels_local(&source, NOCAPS, NOCAPS, ":%s!%s@%s NICK :%s",
				     source.name, source.username, source.host, nick);

	if(source.user)
	{
		whowas::add(source);
		if (dosend)
		{
			sendto_server(&client, NULL, CAP_TS6, NOCAPS, ":%s NICK %s :%ld",
					use_id(&source), nick, (long) source.tsinfo);
		}
	}

	del_from_client_hash(source.name, &source);

	/* invalidate nick delay when a remote client uses the nick.. */
	if((nd = (nd_entry *)rb_dictionary_retrieve(nd_dict, nick)))
		free_nd_entry(nd);

	rb_strlcpy(source.name, nick, sizeof(source.name));
	add_to_client_hash(nick, &source);

	if(!samenick)
		monitor_signon(&source);

	/* remove all accepts pointing to the client */
	del_all_accepts(&source);
}

static void
perform_nick_collides(client::client &source, client::client &client,
		      client::client *target_p, int parc, const char *parv[],
		      time_t newts, const char *nick, const char *uid)
{
	int sameuser;
	int use_save;
	const char *action;

	use_save = ConfigFileEntry.collision_fnc && can_save(target_p) &&
		uid != NULL && can_save(&source);
	action = use_save ? "saved" : "killed";

	/* if we dont have a ts, or their TS's are the same, kill both */
	if(!newts || !target_p->tsinfo || (newts == target_p->tsinfo))
	{
		sendto_realops_snomask(sno::SKILL, L_ALL,
				     "Nick collision on %s(%s <- %s)(both %s)",
				     target_p->name, target_p->from->name, client.name, action);

		if (use_save)
		{
			save_user(me, me, target_p);
			ServerStats.is_save++;
			sendto_one(&client, ":%s SAVE %s %ld", me.id,
					uid, (long)newts);
			register_client(client, &source,
					uid, SAVE_NICKTS, parc, parv);
		}
		else
		{
			sendto_one_numeric(target_p, ERR_NICKCOLLISION,
					form_str(ERR_NICKCOLLISION), target_p->name);

			/* if the new client being introduced has a UID, we need to
			 * issue a KILL for it..
			 */
			if(uid)
				sendto_one(&client, ":%s KILL %s :%s (Nick collision (new))",
						me.id, uid, me.name);

			/* we then need to KILL the old client everywhere */
			kill_client_serv_butone(NULL, target_p, "%s (Nick collision (new))", me.name);
			ServerStats.is_kill++;

			target_p->flags |= client::flags::KILLED;
			exit_client(&client, target_p, &me, "Nick collision (new)");
		}
		return;
	}
	/* the timestamps are different */
	else
	{
		sameuser = (target_p->user) && !irccmp(target_p->username, parv[5])
			&& !irccmp(target_p->host, parv[6]);

		if((sameuser && newts < target_p->tsinfo) ||
		   (!sameuser && newts > target_p->tsinfo))
		{
			/* if we have a UID, then we need to issue a KILL,
			 * otherwise we do nothing and hope that the other
			 * client will collide it..
			 */
			if (use_save)
			{
				sendto_one(&client, ":%s SAVE %s %ld", me.id,
						uid, (long)newts);
				register_client(client, &source,
						uid, SAVE_NICKTS, parc, parv);
			}
			else if(uid)
				sendto_one(&client,
					   ":%s KILL %s :%s (Nick collision (new))",
					   me.id, uid, me.name);
			return;
		}
		else
		{
			if(sameuser)
				sendto_realops_snomask(sno::SKILL, L_ALL,
						     "Nick collision on %s(%s <- %s)(older %s)",
						     target_p->name, target_p->from->name,
						     client.name, action);
			else
				sendto_realops_snomask(sno::SKILL, L_ALL,
						     "Nick collision on %s(%s <- %s)(newer %s)",
						     target_p->name, target_p->from->name,
						     client.name, action);

			if (use_save)
			{
				ServerStats.is_save++;
				save_user(me, me, target_p);
			}
			else
			{
				ServerStats.is_kill++;
				sendto_one_numeric(target_p, ERR_NICKCOLLISION,
						form_str(ERR_NICKCOLLISION), target_p->name);

				/* now we just need to kill the existing client */
				kill_client_serv_butone(&client, target_p,
						"%s (Nick collision (new))", me.name);

				target_p->flags |= client::flags::KILLED;
				(void) exit_client(&client, target_p, &me, "Nick collision");
			}

			register_client(client, &source,
					nick, newts, parc, parv);
		}
	}
}


static void
perform_nickchange_collides(client::client &source, client::client &client,
			    client::client *target_p, int parc,
			    const char *parv[], time_t newts, const char *nick)
{
	int sameuser;
	int use_save;
	const char *action;

	use_save = ConfigFileEntry.collision_fnc && can_save(target_p) &&
		can_save(&source);
	action = use_save ? "saved" : "killed";

	/* its a client changing nick and causing a collide */
	if(!newts || !target_p->tsinfo || (newts == target_p->tsinfo) || !source.user)
	{
		sendto_realops_snomask(sno::SKILL, L_ALL,
				     "Nick change collision from %s to %s(%s <- %s)(both %s)",
				     source.name, target_p->name, target_p->from->name,
				     client.name, action);

		if (use_save)
		{
			ServerStats.is_save += 2;
			save_user(me, me, target_p);
			sendto_one(&client, ":%s SAVE %s %ld", me.id,
					source.id, (long)newts);
			/* don't send a redundant nick change */
			if (!rfc1459::is_digit(source.name[0]))
				change_remote_nick(client, source, SAVE_NICKTS, source.id, 1);
		}
		else
		{
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
		}
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
				sendto_realops_snomask(sno::SKILL, L_ALL,
						     "Nick change collision from %s to %s(%s <- %s)(older %s)",
						     source.name, target_p->name,
						     target_p->from->name, client.name, action);
			else
				sendto_realops_snomask(sno::SKILL, L_ALL,
						     "Nick change collision from %s to %s(%s <- %s)(newer %s)",
						     source.name, target_p->name,
						     target_p->from->name, client.name, action);

			if (use_save)
			{
				ServerStats.is_save++;
				/* can't broadcast a SAVE because the
				 * nickchange has happened at &client
				 * but not in other directions -- jilles */
				sendto_one(&client, ":%s SAVE %s %ld", me.id,
						source.id, (long)newts);
				/* send a :<id> NICK <id> <ts> (!) */
				if (!rfc1459::is_digit(source.name[0]))
					change_remote_nick(client, source, SAVE_NICKTS, source.id, 1);
			}
			else
			{
				ServerStats.is_kill++;

				sendto_one_numeric(&source, ERR_NICKCOLLISION,
						form_str(ERR_NICKCOLLISION), source.name);

				/* kill the client issuing the nickchange */
				kill_client_serv_butone(&client, &source,
						"%s (Nick change collision)", me.name);

				source.flags |= client::flags::KILLED;

				if(sameuser)
					exit_client(&client, &source, &me, "Nick collision(old)");
				else
					exit_client(&client, &source, &me, "Nick collision(new)");
			}
			return;
		}
		else
		{
			if(sameuser)
				sendto_realops_snomask(sno::SKILL, L_ALL,
						     "Nick collision on %s(%s <- %s)(older %s)",
						     target_p->name, target_p->from->name,
						     client.name, action);
			else
				sendto_realops_snomask(sno::SKILL, L_ALL,
						     "Nick collision on %s(%s <- %s)(newer %s)",
						     target_p->name, target_p->from->name,
						     client.name, action);

			if (use_save)
			{
				ServerStats.is_save++;
				save_user(me, me, target_p);
			}
			else
			{
				sendto_one_numeric(target_p, ERR_NICKCOLLISION,
						form_str(ERR_NICKCOLLISION), target_p->name);

				/* kill the client who existed before hand */
				kill_client_serv_butone(&client, target_p, "%s (Nick collision)", me.name);

				ServerStats.is_kill++;

				target_p->flags |= client::flags::KILLED;
				(void) exit_client(&client, target_p, &me, "Nick collision");
			}
		}
	}

	change_remote_nick(client, source, newts, nick, 1);
}

static void
register_client(client::client &client, client::client *server,
		const char *nick, time_t newts, int parc, const char *parv[])
{
	client::client *source;
	struct nd_entry *nd;
	const char *m;
	int flag;

	source = make_client(&client);
	make_user(*source);
	rb_dlinkAddTail(source, &source->node, &global_client_list);

	source->hopcount = atoi(parv[2]);
	source->tsinfo = newts;

	rb_strlcpy(source->name, nick, sizeof(source->name));
	rb_strlcpy(source->username, parv[5], sizeof(source->username));
	rb_strlcpy(source->host, parv[6], sizeof(source->host));
	rb_strlcpy(source->orighost, source->host, sizeof(source->orighost));

	if(parc == 12)
	{
		rb_strlcpy(source->info, parv[11], sizeof(source->info));
		rb_strlcpy(source->sockhost, parv[7], sizeof(source->sockhost));
		rb_strlcpy(source->id, parv[8], sizeof(source->id));
		add_to_id_hash(source->id, source);
		if (strcmp(parv[9], "*"))
		{
			rb_strlcpy(source->orighost, parv[9], sizeof(source->orighost));
			if (irccmp(source->host, source->orighost))
				set_dyn_spoof(*source);
		}
		if (strcmp(parv[10], "*"))
			suser(user(*source)) = parv[10];
	}
	else if(parc == 10)
	{
		rb_strlcpy(source->info, parv[9], sizeof(source->info));
		rb_strlcpy(source->sockhost, parv[7], sizeof(source->sockhost));
		rb_strlcpy(source->id, parv[8], sizeof(source->id));
		add_to_id_hash(source->id, source);
	}
	else
	{
		s_assert(0);
	}

	/* remove any nd entries for this nick */
	if((nd = (nd_entry *)rb_dictionary_retrieve(nd_dict, nick)))
		free_nd_entry(nd);

	add_to_client_hash(nick, source);
	add_to_hostname_hash(source->orighost, source);
	monitor_signon(source);

	m = &parv[4][1];
	while(*m)
	{
		flag = user_modes[(unsigned char) *m];

		if(flag & umode::SERVICE)
		{
			int hit = 0;
			rb_dlink_node *ptr;

			RB_DLINK_FOREACH(ptr, service_list.head)
			{
				if(!irccmp((const char *) ptr->data, server->name))
				{
					hit++;
					break;
				}
			}

			if(!hit)
			{
				m++;
				continue;
			}
		}

		/* increment +i count if theyre invis */
		if(!(source->mode & umode::INVISIBLE) && (flag & umode::INVISIBLE))
			Count.invisi++;

		/* increment opered count if theyre opered */
		if(!(source->mode & umode::OPER) && (flag & umode::OPER))
			Count.oper++;

		source->mode |= umode(flag);
		m++;
	}

	if(is(*source, umode::OPER) && !is(*source, umode::SERVICE))
		rb_dlinkAddAlloc(source, &oper_list);

	set_remote_client(*source);

	if(++Count.total > Count.max_tot)
		Count.max_tot = Count.total;

	source->servptr = server;

	source->lnode = users(serv(*source)).emplace(end(users(serv(*source))), source);

	call_hook(h_new_remote_user, source);

	introduce_client(&client, source, nick, parc == 12);
}

/* Check if we can do SAVE. target_p can be a client to save or a
 * server introducing a client -- jilles */
static bool
can_save(client::client *target_p)
{
	client::client *serv_p;

	if (my(*target_p))
		return true;
	if (!has_id(target_p))
		return false;
	serv_p = is_server(*target_p) ? target_p : target_p->servptr;
	while (serv_p != NULL && serv_p != &me)
	{
		if (~caps(serv(*serv_p)) & CAP_SAVE)
			return false;
		serv_p = serv_p->servptr;
	}
	return serv_p == &me;
}

static void
save_user(client::client &client, client::client &source,
		client::client *target_p)
{
	if (!my_connect(*target_p) && (!has_id(target_p) || !IsCapable(target_p->from, CAP_SAVE)))
	{
		/* This shouldn't happen */
		/* Note we only need SAVE support in this direction */
		sendto_realops_snomask(sno::GENERAL, L_ALL,
				"Killed %s!%s@%s for nick collision detected by %s (%s does not support SAVE)",
				target_p->name, target_p->username, target_p->host, source.name, target_p->from->name);
		kill_client_serv_butone(NULL, target_p, "%s (Nick collision (no SAVE support))", me.name);
		ServerStats.is_kill++;

		target_p->flags |= client::flags::KILLED;
		(void) exit_client(NULL, target_p, &me, "Nick collision (no SAVE support)");
		return;
	}
	sendto_server(&client, NULL, CAP_SAVE|CAP_TS6, NOCAPS, ":%s SAVE %s %ld",
			source.id, target_p->id, (long)target_p->tsinfo);
	sendto_server(&client, NULL, CAP_TS6, CAP_SAVE, ":%s NICK %s :%ld",
			target_p->id, target_p->id, (long)SAVE_NICKTS);
	if (!is_me(client))
		sendto_realops_snomask(sno::SKILL, L_ALL,
				"Received SAVE message for %s from %s",
				target_p->name, source.name);
	if (my(*target_p))
	{
		sendto_one_numeric(target_p, RPL_SAVENICK,
				form_str(RPL_SAVENICK), target_p->id);
		change_local_nick(*target_p, *target_p, target_p->id, 0);
		target_p->tsinfo = SAVE_NICKTS;
	}
	else
		change_remote_nick(*target_p, *target_p, SAVE_NICKTS, target_p->id, 0);
}

static void bad_nickname(client::client &client, const char *nick)
{
	char squitreason[100];

	sendto_wallops_flags(umode::WALLOP, &me,
			"Squitting %s because of bad nickname %s (NICKLEN mismatch?)",
			client.name, nick);
	sendto_server(NULL, NULL, CAP_TS6, NOCAPS,
			":%s WALLOPS :Squitting %s because of bad nickname %s (NICKLEN mismatch?)",
			me.id, client.name, nick);
	ilog(L_SERVER, "Link %s cancelled, bad nickname %s sent (NICKLEN mismatch?)",
			client.name, nick);

	snprintf(squitreason, sizeof squitreason,
			"Bad nickname introduced [%s]", nick);
	exit_client(&client, &client, &me, squitreason);
}
