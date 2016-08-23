/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_whois.c: Shows who a user is.
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

static const char whois_desc[] =
	"Provides the WHOIS command to display information about a user";

static void do_whois(client::client &client, client::client &source, int parc, const char *parv[]);
static void single_whois(client::client &source, client::client *target_p, int operspy);

static void m_whois(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void ms_whois(struct MsgBuf *, client::client &, client::client &, int, const char **);

struct Message whois_msgtab = {
	"WHOIS", 0, 0, 0, 0,
	{mg_unreg, {m_whois, 2}, {ms_whois, 2}, mg_ignore, mg_ignore, {m_whois, 2}}
};

int doing_whois_hook;
int doing_whois_global_hook;
int doing_whois_channel_visibility_hook;

mapi_clist_av1 whois_clist[] = { &whois_msgtab, NULL };
mapi_hlist_av1 whois_hlist[] = {
	{ "doing_whois",			&doing_whois_hook },
	{ "doing_whois_global",			&doing_whois_global_hook },
	{ "doing_whois_channel_visibility",	&doing_whois_channel_visibility_hook },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(whois, NULL, NULL, whois_clist, whois_hlist, NULL, NULL, NULL, whois_desc);

/*
 * m_whois
 *      parv[1] = nickname masklist
 */
static void
m_whois(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	static time_t last_used = 0;

	if(parc > 2)
	{
		if(EmptyString(parv[2]))
		{
			sendto_one(&source, form_str(ERR_NONICKNAMEGIVEN),
					me.name, source.name);
			return;
		}

		if(!is(source, umode::OPER))
		{
			/* seeing as this is going across servers, we should limit it */
			if((last_used + ConfigFileEntry.pace_wait_simple) > rb_current_time() || !ratelimit_client(&source, 2))
			{
				sendto_one(&source, form_str(RPL_LOAD2HI),
					   me.name, source.name, "WHOIS");
				sendto_one_numeric(&source, RPL_ENDOFWHOIS,
						   form_str(RPL_ENDOFWHOIS), parv[2]);
				return;
			}
			else
				last_used = rb_current_time();
		}

		if(hunt_server(&client, &source, ":%s WHOIS %s :%s", 1, parc, parv) !=
		   HUNTED_ISME)
			return;

		parv[1] = parv[2];

	}
	do_whois(client, source, parc, parv);
}

/*
 * ms_whois
 *      parv[1] = server to reply
 *      parv[2] = nickname to whois
 */
static void
ms_whois(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	client::client *target_p;

	/* note: early versions of ratbox allowed users to issue a remote
	 * whois with a blank parv[2], so we cannot treat it as a protocol
	 * violation. --anfl
	 */
	if(parc < 3 || EmptyString(parv[2]))
	{
		sendto_one(&source, form_str(ERR_NONICKNAMEGIVEN),
				me.name, source.name);
		return;
	}

	/* check if parv[1] exists */
	if((target_p = find_client(parv[1])) == NULL)
	{
		sendto_one_numeric(&source, ERR_NOSUCHSERVER,
				   form_str(ERR_NOSUCHSERVER),
				   rfc1459::is_digit(parv[1][0]) ? "*" : parv[1]);
		return;
	}

	/* if parv[1] isnt my client, or me, someone else is supposed
	 * to be handling the request.. so send it to them
	 */
	if(!my(*target_p) && !is_me(*target_p))
	{
		sendto_one(target_p, ":%s WHOIS %s :%s",
			   get_id(&source, target_p),
			   get_id(target_p, target_p), parv[2]);
		return;
	}

	/* ok, the target is either us, or a client on our server, so perform the whois
	 * but first, parv[1] == server to perform the whois on, parv[2] == person
	 * to whois, so make parv[1] = parv[2] so do_whois is ok -- fl_
	 */
	parv[1] = parv[2];
	do_whois(client, source, parc, parv);
}

/* do_whois
 *
 * inputs	- pointer to
 * output	-
 * side effects -
 */
static void
do_whois(client::client &client, client::client &source, int parc, const char *parv[])
{
	client::client *target_p;
	char *nick;
	char *p = NULL;
	int operspy = 0;

	nick = LOCAL_COPY(parv[1]);
	if((p = strchr(nick, ',')))
		*p = '\0';

	if(IsOperSpy(&source) && *nick == '!')
	{
		operspy = 1;
		nick++;
	}

	target_p = client::find_named_person(nick);
	if(target_p != NULL)
	{
		if(operspy)
		{
			char buffer[BUFSIZE];

			snprintf(buffer, sizeof(buffer), "%s!%s@%s %s",
				target_p->name, target_p->username,
				target_p->host, target_p->servptr->name);
			report_operspy(&source, "WHOIS", buffer);
		}

		single_whois(source, target_p, operspy);
	}
	else
		sendto_one_numeric(&source, ERR_NOSUCHNICK,
				   form_str(ERR_NOSUCHNICK),
				   nick);

	sendto_one_numeric(&source, RPL_ENDOFWHOIS,
			   form_str(RPL_ENDOFWHOIS), parv[1]);
}

/*
 * single_whois()
 *
 * Inputs	- &source client to report to
 *		- target_p client to report on
 * Output	- if found return 1
 * Side Effects	- do a single whois on given client
 * 		  writing results to &source
 */
static void
single_whois(client::client &source, client::client *target_p, int operspy)
{
	char buf[BUFSIZE];
	rb_dlink_node *ptr;
	chan::membership *msptr;
	chan::chan *chptr;
	int cur_len = 0;
	int mlen;
	char *t;
	int tlen;
	hook_data_client hdata;
	int extra_space = 0;
#ifdef RB_IPV6
	struct sockaddr_in ip4;
#endif

	if(target_p->user == NULL)
	{
		s_assert(0);
		return;
	}

	sendto_one_numeric(&source, RPL_WHOISUSER, form_str(RPL_WHOISUSER),
			   target_p->name, target_p->username,
			   target_p->host, target_p->info);

	cur_len = mlen = sprintf(buf, form_str(RPL_WHOISCHANNELS),
				    get_id(&me, &source), get_id(&source, &source),
				    target_p->name);

	/* Make sure it won't overflow when sending it to the client
	 * in full names; note that serverhiding may require more space
	 * for a different server name (not done here) -- jilles
	 */
	if (!my_connect(source))
	{
		extra_space = strlen(source.name) - 9;
		if (extra_space < 0)
			extra_space = 0;
		extra_space += strlen(me.name) - 2; /* make sure >= 0 */
		cur_len += extra_space;
	}

	t = buf + mlen;

	hdata.client = &source;
	hdata.target = target_p;

	if (!is(*target_p, umode::SERVICE))
	{
		for(const auto &pit : chans(user(*target_p)))
		{
			auto &chptr(pit.first);
			auto &msptr(pit.second);

			hdata.chptr = chptr;

			hdata.approved = can_show(chptr, &source);
			call_hook(doing_whois_channel_visibility_hook, &hdata);

			if(hdata.approved || operspy)
			{
				if((cur_len + strlen(chptr->name.c_str()) + 3) > (BUFSIZE - 5))
				{
					sendto_one(&source, "%s", buf);
					cur_len = mlen + extra_space;
					t = buf + mlen;
				}

				tlen = sprintf(t, "%s%s%s ",
						hdata.approved ? "" : "!",
						find_status(msptr, 1),
						chptr->name.c_str());
				t += tlen;
				cur_len += tlen;
			}
		}
	}

	if(cur_len > mlen + extra_space)
		sendto_one(&source, "%s", buf);

	sendto_one_numeric(&source, RPL_WHOISSERVER, form_str(RPL_WHOISSERVER),
			   target_p->name, target_p->servptr->name,
			   target_p->servptr->info);

	if(away(user(*target_p)).size())
		sendto_one_numeric(&source, RPL_AWAY, form_str(RPL_AWAY),
				   target_p->name, away(user(*target_p)).c_str());

	if(is(*target_p, umode::OPER) && (!ConfigFileEntry.hide_opers_in_whois || is(source, umode::OPER)))
	{
		sendto_one_numeric(&source, RPL_WHOISOPERATOR, form_str(RPL_WHOISOPERATOR),
				   target_p->name,
				   is(*target_p, umode::SERVICE) ? ConfigFileEntry.servicestring :
				   (is(*target_p, umode::ADMIN) ? GlobalSetOptions.adminstring :
				    GlobalSetOptions.operstring));
	}

	if(my(*target_p) && !EmptyString(target_p->localClient->opername) && is(source, umode::OPER))
	{
		char buf[512];
		snprintf(buf, sizeof(buf), "is opered as %s, privset %s",
			    target_p->localClient->opername, target_p->localClient->privset->name);
		sendto_one_numeric(&source, RPL_WHOISSPECIAL, form_str(RPL_WHOISSPECIAL),
				   target_p->name, buf);
	}

	if(is(*target_p, umode::SSLCLIENT))
	{
		char cbuf[256] = "is using a secure connection";

		if (my(*target_p) && target_p->localClient->cipher_string != NULL)
			rb_snprintf_append(cbuf, sizeof(cbuf), " [%s]", target_p->localClient->cipher_string);

		sendto_one_numeric(&source, RPL_WHOISSECURE, form_str(RPL_WHOISSECURE),
				   target_p->name, cbuf);
		if((&source == target_p || is(source, umode::OPER)) &&
				target_p->certfp != NULL)
			sendto_one_numeric(&source, RPL_WHOISCERTFP,
					form_str(RPL_WHOISCERTFP),
					target_p->name, target_p->certfp);
	}

	if(my(*target_p))
	{
		if (is_dyn_spoof(*target_p) && (is(source, umode::OPER) || &source == target_p))
		{
			/* trick here: show a nonoper their own IP if
			 * dynamic spoofed but not if auth{} spoofed
			 * -- jilles */
			clear_dyn_spoof(*target_p);
			sendto_one_numeric(&source, RPL_WHOISHOST,
					   form_str(RPL_WHOISHOST),
					   target_p->name, target_p->orighost,
					   show_ip(&source, target_p) ? target_p->sockhost : "255.255.255.255");
			set_dyn_spoof(*target_p);
		}
		else if(ConfigFileEntry.use_whois_actually && show_ip(&source, target_p))
			sendto_one_numeric(&source, RPL_WHOISACTUALLY,
					   form_str(RPL_WHOISACTUALLY),
					   target_p->name, target_p->sockhost);

#ifdef RB_IPV6
		if (GET_SS_FAMILY(&target_p->localClient->ip) == AF_INET6 &&
				(show_ip(&source, target_p) ||
				 (&source == target_p && !is_ip_spoof(*target_p))) &&
				rb_ipv4_from_ipv6((struct sockaddr_in6 *)&target_p->localClient->ip, &ip4))
		{
			rb_inet_ntop_sock((struct sockaddr *)&ip4,
					buf, sizeof buf);
			sendto_one_numeric(&source, RPL_WHOISTEXT,
					"%s :Underlying IPv4 is %s",
					target_p->name, buf);
		}
#endif /* RB_IPV6 */

		sendto_one_numeric(&source, RPL_WHOISIDLE, form_str(RPL_WHOISIDLE),
				   target_p->name,
				   (long)(rb_current_time() - target_p->localClient->last),
				   (unsigned long)target_p->localClient->firsttime);
	}
	else
	{
		if (is_dyn_spoof(*target_p) && (is(source, umode::OPER) || &source == target_p))
		{
			clear_dyn_spoof(*target_p);
			sendto_one_numeric(&source, RPL_WHOISHOST,
					   form_str(RPL_WHOISHOST),
					   target_p->name, target_p->orighost,
					   show_ip(&source, target_p) && !EmptyString(target_p->sockhost) && strcmp(target_p->sockhost, "0")? target_p->sockhost : "255.255.255.255");
			set_dyn_spoof(*target_p);
		}
		else if(ConfigFileEntry.use_whois_actually && show_ip(&source, target_p) &&
		   !EmptyString(target_p->sockhost) && strcmp(target_p->sockhost, "0"))
		{
			sendto_one_numeric(&source, RPL_WHOISACTUALLY,
					   form_str(RPL_WHOISACTUALLY),
					   target_p->name, target_p->sockhost);

		}
	}

	/* doing_whois_hook must only be called for local clients,
	 * doing_whois_global_hook must only be called for local targets
	 */
	/* it is important that these are called *before* RPL_ENDOFWHOIS is
	 * sent, services compatibility code depends on it. --anfl
	 */
	if(my(source))
		call_hook(doing_whois_hook, &hdata);
	else
		call_hook(doing_whois_global_hook, &hdata);
}
