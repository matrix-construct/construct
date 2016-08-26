/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_kill.c: Kills a user.
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

static const char kill_desc[] = "Provides the KILL command to remove a user from the network";

static int h_can_kill;
static char buf[BUFSIZE];

static void ms_kill(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void mo_kill(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void relay_kill(client::client &, client::client &, client::client *,
		       const char *, const char *);

struct Message kill_msgtab = {
	"KILL", 0, 0, 0, 0,
	{mg_unreg, mg_not_oper, {ms_kill, 2}, {ms_kill, 2}, mg_ignore, {mo_kill, 2}}
};

mapi_clist_av1 kill_clist[] = { &kill_msgtab, NULL };

mapi_hlist_av1 kill_hlist[] = {
	{ "can_kill", &h_can_kill },
	{ NULL, NULL},
};

DECLARE_MODULE_AV2(kill, NULL, NULL, kill_clist, kill_hlist, NULL, NULL, NULL, kill_desc);

/*
** mo_kill
**      parv[1] = kill victim
**      parv[2] = kill path
*/
static void
mo_kill(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	client::client *target_p;
	const char *inpath = client.name;
	const char *user;
	const char *reason;
	hook_data_client_approval moduledata;

	user = parv[1];

	if(!IsOperLocalKill(&source))
	{
		sendto_one(&source, form_str(ERR_NOPRIVS), me.name, source.name, "local_kill");
		return;
	}

	if(!EmptyString(parv[2]))
	{
		char *s;
		s = LOCAL_COPY(parv[2]);
		if(strlen(s) > (size_t) KILLLEN)
			s[KILLLEN] = '\0';
		reason = s;
	}
	else
		reason = "<No reason given>";

	if((target_p = client::find_named_person(user)) == NULL)
	{
		/*
		 ** If the user has recently changed nick, automatically
		 ** rewrite the KILL for this new nickname--this keeps
		 ** servers in synch when nick change and kill collide
		 */
		const auto history(whowas::history(user, KILLCHASETIMELIMIT, true));
		if(history.empty())
		{
			if (strchr(user, '.'))
				sendto_one_numeric(&source, ERR_CANTKILLSERVER, form_str(ERR_CANTKILLSERVER));
			else
				sendto_one_numeric(&source, ERR_NOSUCHNICK, form_str(ERR_NOSUCHNICK),
				                   user);
			return;
		}

		target_p = history.back()->online;
		sendto_one_notice(&source, ":KILL changed from %s to %s",
		                  user,
		                  target_p->name);
	}

	if(!my_connect(*target_p) && (!IsOperGlobalKill(&source)))
	{
		sendto_one_notice(&source, ":Nick %s is not on your server "
				            "and you do not have the global_kill flag",
				target_p->name);
		return;
	}

	/* Last chance to stop the kill */
	moduledata.client = &source;
	moduledata.target = target_p;
	moduledata.approved = 1;
	call_hook(h_can_kill, &moduledata);

	if (moduledata.approved == 0)
		/* The callee should have sent a message. */
		return;

	if(my_connect(*target_p))
		sendto_one(target_p, ":%s!%s@%s KILL %s :%s",
			   source.name, source.username, source.host,
			   target_p->name, reason);

	/* Do not change the format of this message.  There's no point in changing messages
	 * that have been around for ever, for no reason.. */
	sendto_realops_snomask(sno::GENERAL, L_ALL,
			     "Received KILL message for %s!%s@%s. From %s Path: %s (%s)",
			     target_p->name, target_p->username, target_p->orighost,
			     source.name, me.name, reason);

	ilog(L_KILL, "%c %s %s!%s@%s %s %s",
	     my_connect(*target_p) ? 'L' : 'G', get_oper_name(&source),
	     target_p->name, target_p->username, target_p->host, target_p->servptr->name, reason);

	/*
	 ** And pass on the message to other servers. Note, that if KILL
	 ** was changed, the message has to be sent to all links, also
	 ** back.
	 ** Suicide kills are NOT passed on --SRB
	 */
	if(!my_connect(*target_p))
	{
		relay_kill(client, source, target_p, inpath, reason);
		/*
		 ** Set FLAGS_KILLED. This prevents exit_one_client from sending
		 ** the unnecessary QUIT for this. (This flag should never be
		 ** set in any other place)
		 */
		target_p->flags |= client::flags::KILLED;
	}

	sprintf(buf, "Killed (%s (%s))", source.name, reason);

	exit_client(&client, target_p, &source, buf);
}

/*
 * ms_kill
 *      parv[1] = kill victim
 *      parv[2] = kill path and reason
 */
static void
ms_kill(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	client::client *target_p;
	const char *user;
	const char *reason;
	char default_reason[] = "<No reason given>";
	const char *path;

	*buf = '\0';

	user = parv[1];

	if(EmptyString(parv[2]))
	{
		reason = default_reason;

		/* hyb6 takes the nick of the killer from the path *sigh* --fl_ */
		path = source.name;
	}
	else
	{
		char *s = LOCAL_COPY(parv[2]), *t;
		t = strchr(s, ' ');

		if(t)
		{
			*t = '\0';
			t++;
			reason = t;
		}
		else
			reason = default_reason;

		path = s;
	}

	if((target_p = client::find_person(user)) == NULL)
	{
		/*
		 * If the user has recently changed nick, but only if its
		 * not an uid, automatically rewrite the KILL for this new nickname.
		 * --this keeps servers in synch when nick change and kill collide
		 */
		if(rfc1459::is_digit(*user))
		{
			sendto_one_numeric(&source, ERR_NOSUCHNICK, form_str(ERR_NOSUCHNICK), "*");
			return;
		}

		const auto history(whowas::history(user, KILLCHASETIMELIMIT, true));
		if(history.empty())
		{
			sendto_one_numeric(&source, ERR_NOSUCHNICK, form_str(ERR_NOSUCHNICK), user);
			return;
		}

		target_p = history.back()->online;
		sendto_one_notice(&source, ":KILL changed from %s to %s", user, target_p->name);
	}

	if(is_server(*target_p) || is_me(*target_p))
	{
		sendto_one_numeric(&source, ERR_CANTKILLSERVER, form_str(ERR_CANTKILLSERVER));
		return;
	}

	if(my_connect(*target_p))
	{
		if(is_server(source))
		{
			sendto_one(target_p, ":%s KILL %s :%s",
				   source.name, target_p->name, reason);
		}
		else
			sendto_one(target_p, ":%s!%s@%s KILL %s :%s",
				   source.name, source.username, source.host,
				   target_p->name, reason);
	}

	/* Be warned, this message must be From %s, or it confuses clients
	 * so dont change it to From: or the case or anything! -- fl -- db */
	/* path must contain at least 2 !'s, or bitchx falsely declares it
	 * local --fl
	 */
	if(is(source, umode::OPER))	/* send it normally */
	{
		sendto_realops_snomask(is(source, umode::SERVICE) ? sno::SKILL : sno::GENERAL, L_ALL,
				     "Received KILL message for %s!%s@%s. From %s Path: %s!%s!%s!%s %s",
				     target_p->name, target_p->username, target_p->orighost, source.name,
				     source.servptr->name, source.host, source.username,
				     source.name, reason);

		ilog(L_KILL, "%c %s %s!%s@%s %s %s",
		     my_connect(*target_p) ? 'O' : 'R', get_oper_name(&source),
		     target_p->name, target_p->username, target_p->host,
		     target_p->servptr->name, reason);
	}
	else
	{
		sendto_realops_snomask(sno::SKILL, L_ALL,
				     "Received KILL message for %s!%s@%s. From %s %s",
				     target_p->name, target_p->username, target_p->orighost,
				     source.name, reason);

		ilog(L_KILL, "S %s %s!%s@%s %s %s",
		     source.name, target_p->name, target_p->username,
		     target_p->host, target_p->servptr->name, reason);
	}

	relay_kill(client, source, target_p, path, reason);

	/* FLAGS_KILLED prevents a quit being sent out */
	target_p->flags |= client::flags::KILLED;

	sprintf(buf, "Killed (%s %s)", source.name, reason);

	exit_client(&client, target_p, &source, buf);
}

static void
relay_kill(client::client &one, client::client &source,
	   client::client *target_p, const char *inpath, const char *reason)
{
	client::client *client;
	rb_dlink_node *ptr;
	char buffer[BUFSIZE];

	if(my(source))
		snprintf(buffer, sizeof(buffer),
			    "%s!%s!%s!%s (%s)",
			    me.name, source.host, source.username, source.name, reason);
	else
		snprintf(buffer, sizeof(buffer), "%s %s", inpath, reason);

	RB_DLINK_FOREACH(ptr, serv_list.head)
	{
		client = (client::client *)ptr->data;

		if(!client || client == &one)
			continue;

		sendto_one(client, ":%s KILL %s :%s",
			   get_id(&source, client), get_id(target_p, client), buffer);
	}
}
