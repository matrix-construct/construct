/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  m_stats.c: Sends the user statistics or config information.
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

static const char stats_desc[] =
	"Provides the STATS command to inspect various server/network information";

static void m_stats (struct MsgBuf *, client::client &, client::client &, int, const char **);

struct Message stats_msgtab = {
	"STATS", false, false, false, false,
	{mg_unreg, {m_stats, 2}, {m_stats, 3}, mg_ignore, mg_ignore, {m_stats, 2}}
};

int doing_stats_hook;
int doing_stats_p_hook;

mapi_clist_av1 stats_clist[] = { &stats_msgtab, NULL };
mapi_hlist_av1 stats_hlist[] = {
	{ "doing_stats",	&doing_stats_hook },
	{ "doing_stats_p",	&doing_stats_p_hook },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(stats, NULL, NULL, stats_clist, stats_hlist, NULL, NULL, NULL, stats_desc);

const char *Lformat = "%s %u %u %u %u %u :%u %u %s";

static void stats_l_list(client::client &s, const char *, bool, bool, rb_dlink_list *, char,
				bool (*check_fn)(client::client *target_p));
static void stats_l_client(client::client &source, client::client *target_p,
				char statchar);

static int stats_spy(client::client &, char, const char *);
static void stats_p_spy(client::client &);

typedef void (*handler_t)(client::client &source);
typedef void (*handler_parv_t)(client::client &source, int parc, const char *parv[]);

struct stats_cmd
{
	union
	{
		handler_t handler;
		handler_parv_t handler_parv;
	};
	bool need_parv;
	bool need_oper;
	bool need_admin;

	stats_cmd(const handler_t &handler = nullptr,
	          const bool &need_oper = true,
	          const bool &need_admin = true)
	:handler{handler}
	,need_parv{false}
	,need_oper{need_oper}
	,need_admin{need_admin}
	{
	}

	stats_cmd(const handler_parv_t &handler_parv,
	          const bool &need_oper,
	          const bool &need_admin)
	:handler_parv{handler_parv}
	,need_parv{true}
	,need_oper{need_oper}
	,need_admin{need_admin}
	{
	}
};

static void stats_dns_servers(client::client &);
static void stats_delay(client::client &);
static void stats_hash(client::client &);
static void stats_connect(client::client &);
static void stats_tdeny(client::client &);
static void stats_deny(client::client &);
static void stats_exempt(client::client &);
static void stats_events(client::client &);
static void stats_prop_klines(client::client &);
static void stats_hubleaf(client::client &);
static void stats_auth(client::client &);
static void stats_tklines(client::client &);
static void stats_klines(client::client &);
static void stats_messages(client::client &);
static void stats_dnsbl(client::client &);
static void stats_oper(client::client &);
static void stats_privset(client::client &);
static void stats_operedup(client::client &);
static void stats_ports(client::client &);
static void stats_tresv(client::client &);
static void stats_resv(client::client &);
static void stats_ssld(client::client &);
static void stats_usage(client::client &);
static void stats_tstats(client::client &);
static void stats_uptime(client::client &);
static void stats_shared(client::client &);
static void stats_servers(client::client &);
static void stats_tgecos(client::client &);
static void stats_gecos(client::client &);
static void stats_class(client::client &);
static void stats_memory(client::client &);
static void stats_servlinks(client::client &);
static void stats_ltrace(client::client &, int, const char **);
static void stats_ziplinks(client::client &);
static void stats_comm(client::client &);
static void stats_capability(client::client &);


/* This table contains the possible stats items, in order:
 * stats letter,  function to call, operonly? adminonly? --fl_
 *
 * Previously in this table letters were a column. I fixed it to use modern
 * C initalisers so we don't have to iterate anymore
 * --Elizafox
 */
std::array<stats_cmd, 256> stats_cmd_table =
[]{
	std::array<stats_cmd, 256> ret;

	//letter               handler              oper      admin
	ret['a'] = stats_cmd { stats_dns_servers,   true,     true  };
	ret['a'] = stats_cmd { stats_dns_servers,   true,     true  };
	ret['A'] = stats_cmd { stats_dns_servers,   true,     true  };
	ret['b'] = stats_cmd { stats_delay,         true,     true  };
	ret['B'] = stats_cmd { stats_hash,          true,     true  };
	ret['c'] = stats_cmd { stats_connect,       false,    false };
	ret['C'] = stats_cmd { stats_capability,    true,     false };
	ret['d'] = stats_cmd { stats_tdeny,         true,     false };
	ret['D'] = stats_cmd { stats_deny,          true,     false };
	ret['e'] = stats_cmd { stats_exempt,        true,     false };
	ret['E'] = stats_cmd { stats_events,        true,     true  };
	ret['f'] = stats_cmd { stats_comm,          true,     true  };
	ret['F'] = stats_cmd { stats_comm,          true,     true  };
	ret['g'] = stats_cmd { stats_prop_klines,   true,     false };
	ret['h'] = stats_cmd { stats_hubleaf,       false,    false };
	ret['H'] = stats_cmd { stats_hubleaf,       false,    false };
	ret['i'] = stats_cmd { stats_auth,          false,    false };
	ret['I'] = stats_cmd { stats_auth,          false,    false };
	ret['k'] = stats_cmd { stats_tklines,       false,    false };
	ret['K'] = stats_cmd { stats_klines,        false,    false };
	ret['l'] = stats_cmd { stats_ltrace,        false,    false };
	ret['L'] = stats_cmd { stats_ltrace,        false,    false };
	ret['m'] = stats_cmd { stats_messages,      false,    false };
	ret['M'] = stats_cmd { stats_messages,      false,    false };
	ret['n'] = stats_cmd { stats_dnsbl,         false,    false };
	ret['o'] = stats_cmd { stats_oper,          false,    false };
	ret['O'] = stats_cmd { stats_privset,       true,     false };
	ret['p'] = stats_cmd { stats_operedup,      false,    false };
	ret['P'] = stats_cmd { stats_ports,         false,    false };
	ret['q'] = stats_cmd { stats_tresv,         true,     false };
	ret['Q'] = stats_cmd { stats_resv,          true,     false };
	ret['r'] = stats_cmd { stats_usage,         true,     false };
	ret['R'] = stats_cmd { stats_usage,         true,     false };
	ret['s'] = stats_cmd { stats_ssld,          true,     true  };
	ret['S'] = stats_cmd { stats_ssld,          true,     true  };
	ret['t'] = stats_cmd { stats_tstats,        true,     false };
	ret['T'] = stats_cmd { stats_tstats,        true,     false };
	ret['u'] = stats_cmd { stats_uptime,        false,    false };
	ret['U'] = stats_cmd { stats_shared,        true,     false };
	ret['v'] = stats_cmd { stats_servers,       false,    false };
	ret['V'] = stats_cmd { stats_servers,       false,    false };
	ret['x'] = stats_cmd { stats_tgecos,        true,     false };
	ret['X'] = stats_cmd { stats_gecos,         true,     false };
	ret['y'] = stats_cmd { stats_class,         false,    false };
	ret['Y'] = stats_cmd { stats_class,         false,    false };
	ret['z'] = stats_cmd { stats_memory,        true,     false };
	ret['Z'] = stats_cmd { stats_ziplinks,      true,     false };
	ret['?'] = stats_cmd { stats_servlinks,     false,    false };

	return ret;
}();


/*
 * m_stats by fl_
 * Modified heavily by Elizafox
 *      parv[1] = stat letter/command
 *      parv[2] = (if present) server/mask in stats L, or target
 *
 * This will search the tables for the appropriate stats letter,
 * if found execute it.
 */
static void
m_stats(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	static time_t last_used = 0;
	struct stats_cmd *cmd;
	unsigned char statchar;
	int did_stats = 0;

	statchar = parv[1][0];

	if(my(source) && !is(source, umode::OPER))
	{
		/* Check the user is actually allowed to do /stats, and isnt flooding */
		if((last_used + ConfigFileEntry.pace_wait) > rb_current_time())
		{
			/* safe enough to give this on a local connect only */
			sendto_one(&source, form_str(RPL_LOAD2HI),
				   me.name, source.name, "STATS");
			sendto_one_numeric(&source, RPL_ENDOFSTATS,
					   form_str(RPL_ENDOFSTATS), statchar);
			return;
		}
		else
			last_used = rb_current_time();
	}

	if(hunt_server (&client, &source, ":%s STATS %s :%s", 2, parc, parv) != HUNTED_ISME)
		return;

	if(tolower(statchar) != 'l')
		/* FIXME */
		did_stats = stats_spy(source, statchar, NULL);

	/* if did_stats is true, a module grabbed this STATS request */
	if(did_stats)
		goto stats_out;

	/* Look up */
	cmd = &stats_cmd_table[statchar];
	if(cmd->handler != NULL)
	{
		/* The stats table says what privs are needed, so check --fl_ */
		/* Called for remote clients and for local opers, so check need_admin
		 * and need_oper
		 */
		if(cmd->need_admin && !IsOperAdmin(&source))
		{
			sendto_one(&source, form_str(ERR_NOPRIVS),
				   me.name, source.name, "admin");
			goto stats_out;
		}
		if(cmd->need_oper && !is(source, umode::OPER))
		{
			sendto_one_numeric(&source, ERR_NOPRIVILEGES,
					   form_str (ERR_NOPRIVILEGES));
			goto stats_out;
		}

		if(cmd->need_parv)
			cmd->handler_parv(source, parc, parv);
		else
			cmd->handler(source);
	}

stats_out:
	/* Send the end of stats notice, and the stats_spy */
	sendto_one_numeric(&source, RPL_ENDOFSTATS,
			   form_str(RPL_ENDOFSTATS), statchar);
}

static void
stats_dns_servers (client::client &source)
{
	rb_dlink_node *n;

	RB_DLINK_FOREACH(n, nameservers.head)
	{
		sendto_one_numeric(&source, RPL_STATSDEBUG, "A %s", (char *)n->data);
	}
}

static void
stats_delay(client::client &source)
{
	struct nd_entry *nd;
	rb_dictionary_iter iter;

	void *elem;
	RB_DICTIONARY_FOREACH(elem, &iter, nd_dict)
	{
		nd = (nd_entry *)elem;
		sendto_one_notice(&source, ":Delaying: %s for %ld",
				nd->name, (long) nd->expire);
	}
}

static void
stats_hash_cb(const char *buf, void *client)
{
	sendto_one_numeric((client::client *)client, RPL_STATSDEBUG, "B :%s", buf);
}

static void
stats_hash(client::client &source)
{
	sendto_one_numeric(&source, RPL_STATSDEBUG, "B :%-30s %-15s %-10s %-10s %-10s %-10s",
		"NAME", "TYPE", "OBJECTS", "DEPTH SUM", "AVG DEPTH", "MAX DEPTH");

	rb_dictionary_stats_walk(stats_hash_cb, &source);
	rb_radixtree_stats_walk(stats_hash_cb, &source);
}

static void
stats_connect(client::client &source)
{
	static char buf[5];
	struct server_conf *server_p;
	char *s;
	rb_dlink_node *ptr;

	if((ConfigFileEntry.stats_c_oper_only ||
	    (ConfigServerHide.flatten_links && !is_exempt_shide(source))) &&
	    !is(source, umode::OPER))
	{
		sendto_one_numeric(&source, ERR_NOPRIVILEGES,
				   form_str(ERR_NOPRIVILEGES));
		return;
	}

	RB_DLINK_FOREACH(ptr, server_conf_list.head)
	{
		server_p = (server_conf *)ptr->data;

		if(ServerConfIllegal(server_p))
			continue;

		s = buf;

		if(is(source, umode::OPER))
		{
			if(ServerConfAutoconn(server_p))
				*s++ = 'A';
			if(ServerConfSSL(server_p))
				*s++ = 'S';
			if(ServerConfTb(server_p))
				*s++ = 'T';
			if(ServerConfCompressed(server_p))
				*s++ = 'Z';
		}

		if(s == buf)
			*s++ = '*';

		*s = '\0';

		sendto_one_numeric(&source, RPL_STATSCLINE,
				form_str(RPL_STATSCLINE),
				"*@127.0.0.1",
				buf, server_p->name,
				server_p->port, server_p->class_name,
				server_p->certfp ? server_p->certfp : "*");
	}
}

/* stats_tdeny()
 *
 * input	- client to report to
 * output	- none
 * side effects - client is given temp dline list.
 */
static void
stats_tdeny (client::client &source)
{
	char *host, *pass, *user, *oper_reason;
	struct AddressRec *arec;
	struct ConfItem *aconf;
	int i;

	for (i = 0; i < ATABLE_SIZE; i++)
	{
		for (arec = atable[i]; arec; arec = arec->next)
		{
			if(arec->type == CONF_DLINE)
			{
				aconf = arec->aconf;

				if(!(aconf->flags & CONF_FLAGS_TEMPORARY))
					continue;

				get_printable_kline(&source, aconf, &host, &pass, &user, &oper_reason);

				sendto_one_numeric(&source, RPL_STATSDLINE,
						   form_str (RPL_STATSDLINE),
						   'd', host, pass,
						   oper_reason ? "|" : "",
						   oper_reason ? oper_reason : "");
			}
		}
	}
}

/* stats_deny()
 *
 * input	- client to report to
 * output	- none
 * side effects - client is given dline list.
 */
static void
stats_deny (client::client &source)
{
	char *host, *pass, *user, *oper_reason;
	struct AddressRec *arec;
	struct ConfItem *aconf;
	int i;

	for (i = 0; i < ATABLE_SIZE; i++)
	{
		for (arec = atable[i]; arec; arec = arec->next)
		{
			if(arec->type == CONF_DLINE)
			{
				aconf = arec->aconf;

				if(aconf->flags & CONF_FLAGS_TEMPORARY)
					continue;

				get_printable_kline(&source, aconf, &host, &pass, &user, &oper_reason);

				sendto_one_numeric(&source, RPL_STATSDLINE,
						   form_str (RPL_STATSDLINE),
						   'D', host, pass,
						   oper_reason ? "|" : "",
						   oper_reason ? oper_reason : "");
			}
		}
	}
}


/* stats_exempt()
 *
 * input	- client to report to
 * output	- none
 * side effects - client is given list of exempt blocks
 */
static void
stats_exempt(client::client &source)
{
	char *name, *host, *user, *classname;
	const char *pass;
	struct AddressRec *arec;
	struct ConfItem *aconf;
	int i, port;

	if(ConfigFileEntry.stats_e_disabled)
	{
		sendto_one_numeric(&source, ERR_DISABLED,
				   form_str(ERR_DISABLED), "STATS e");
		return;
	}

	for (i = 0; i < ATABLE_SIZE; i++)
	{
		for (arec = atable[i]; arec; arec = arec->next)
		{
			if(arec->type == CONF_EXEMPTDLINE)
			{
				aconf = arec->aconf;
				get_printable_conf (aconf, &name, &host, &pass,
						    &user, &port, &classname);

				sendto_one_numeric(&source, RPL_STATSDLINE,
						   form_str(RPL_STATSDLINE),
						   'e', host, pass, "", "");
			}
		}
	}}


static void
stats_events_cb(char *str, void *ptr)
{
	sendto_one_numeric((client::client *)ptr, RPL_STATSDEBUG, "E :%s", str);
}

static void
stats_events (client::client &source)
{
	rb_dump_events(stats_events_cb, &source);
}

static void
stats_prop_klines(client::client &source)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr;
	char *user, *host, *pass, *oper_reason;

	RB_DLINK_FOREACH(ptr, prop_bans.head)
	{
		aconf = (ConfItem *)ptr->data;

		/* Skip non-klines and deactivated klines. */
		if(aconf->status != CONF_KILL)
			continue;

		get_printable_kline(&source, aconf, &host, &pass,
				&user, &oper_reason);

		sendto_one_numeric(&source, RPL_STATSKLINE,
				form_str(RPL_STATSKLINE),
				'g', host, user, pass,
				oper_reason ? "|" : "",
				oper_reason ? oper_reason : "");
	}
}

static void
stats_hubleaf(client::client &source)
{
	struct remote_conf *hub_p;
	rb_dlink_node *ptr;

	if((ConfigFileEntry.stats_h_oper_only ||
	    (ConfigServerHide.flatten_links && !is_exempt_shide(source))) &&
	    !is(source, umode::OPER))
	{
		sendto_one_numeric(&source, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));
		return;
	}

	RB_DLINK_FOREACH(ptr, hubleaf_conf_list.head)
	{
		hub_p = (remote_conf *)ptr->data;

		if(hub_p->flags & CONF_HUB)
			sendto_one_numeric(&source, RPL_STATSHLINE,
					form_str(RPL_STATSHLINE),
					hub_p->host, hub_p->server);
		else
			sendto_one_numeric(&source, RPL_STATSLLINE,
					form_str(RPL_STATSLLINE),
					hub_p->host, hub_p->server);
	}
}


static void
stats_auth (client::client &source)
{
	/* Oper only, if unopered, return ERR_NOPRIVS */
	if((ConfigFileEntry.stats_i_oper_only == 2) && !is(source, umode::OPER))
		sendto_one_numeric(&source, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));

	/* If unopered, Only return matching auth blocks */
	else if((ConfigFileEntry.stats_i_oper_only == 1) && !is(source, umode::OPER))
	{
		struct ConfItem *aconf;
		char *name, *host, *user, *classname;
		const char *pass = "*";
		int port;

		if(my_connect(source))
			aconf = find_conf_by_address (source.host, source.sockhost, NULL,
						      (struct sockaddr *)&source.localClient->ip,
						      CONF_CLIENT,
						      GET_SS_FAMILY(&source.localClient->ip),
						      source.username, NULL);
		else
			aconf = find_conf_by_address (source.host, NULL, NULL, NULL, CONF_CLIENT,
						      0, source.username, NULL);

		if(aconf == NULL)
			return;

		get_printable_conf (aconf, &name, &host, &pass, &user, &port, &classname);
		if(!EmptyString(aconf->spasswd))
			pass = aconf->spasswd;

		sendto_one_numeric(&source, RPL_STATSILINE, form_str(RPL_STATSILINE),
				   name, pass, show_iline_prefix(&source, aconf, user),
				   host, port, classname);
	}

	/* Theyre opered, or allowed to see all auth blocks */
	else
		report_auth (&source);
}


static void
stats_tklines(client::client &source)
{
	/* Oper only, if unopered, return ERR_NOPRIVS */
	if((ConfigFileEntry.stats_k_oper_only == 2) && !is(source, umode::OPER))
		sendto_one_numeric(&source, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));

	/* If unopered, Only return matching klines */
	else if((ConfigFileEntry.stats_k_oper_only == 1) && !is(source, umode::OPER))
	{
		struct ConfItem *aconf;
		char *host, *pass, *user, *oper_reason;

		if(my_connect(source))
			aconf = find_conf_by_address (source.host, source.sockhost, NULL,
						      (struct sockaddr *)&source.localClient->ip,
						      CONF_KILL,
						      GET_SS_FAMILY(&source.localClient->ip),
						      source.username, NULL);
		else
			aconf = find_conf_by_address (source.host, NULL, NULL, NULL, CONF_KILL,
						      0, source.username, NULL);

		if(aconf == NULL)
			return;

		/* dont report a permanent kline as a tkline */
		if((aconf->flags & CONF_FLAGS_TEMPORARY) == 0)
			return;

		get_printable_kline(&source, aconf, &host, &pass, &user, &oper_reason);

		sendto_one_numeric(&source, RPL_STATSKLINE,
				   form_str(RPL_STATSKLINE), aconf->flags & CONF_FLAGS_TEMPORARY ? 'k' : 'K',
				   host, user, pass, oper_reason ? "|" : "",
				   oper_reason ? oper_reason : "");
	}
	/* Theyre opered, or allowed to see all klines */
	else
	{
		struct ConfItem *aconf;
		rb_dlink_node *ptr;
		int i;
		char *user, *host, *pass, *oper_reason;

		for(i = 0; i < LAST_TEMP_TYPE; i++)
		{
			RB_DLINK_FOREACH(ptr, temp_klines[i].head)
			{
				aconf = (ConfItem *)ptr->data;

				get_printable_kline(&source, aconf, &host, &pass,
							&user, &oper_reason);

				sendto_one_numeric(&source, RPL_STATSKLINE,
						   form_str(RPL_STATSKLINE),
						   'k', host, user, pass,
						   oper_reason ? "|" : "",
						   oper_reason ? oper_reason : "");
			}
		}
	}
}

/* report_Klines()
 *
 * inputs       - Client to report to, mask
 * outputs      -
 * side effects - Reports configured K-lines to &client.
 */
static void
report_Klines(client::client &source)
{
	char *host, *pass, *user, *oper_reason;
	struct AddressRec *arec;
	struct ConfItem *aconf = NULL;
	int i;

	for (i = 0; i < ATABLE_SIZE; i++)
	{
		for (arec = atable[i]; arec; arec = arec->next)
		{
			if(arec->type == CONF_KILL)
			{
				aconf = arec->aconf;

				/* its a tempkline, theyre reported elsewhere */
				if(aconf->flags & CONF_FLAGS_TEMPORARY)
					continue;

				get_printable_kline(&source, aconf, &host, &pass, &user, &oper_reason);
				sendto_one_numeric(&source, RPL_STATSKLINE,
						   form_str(RPL_STATSKLINE),
						   'K', host, user, pass,
						   oper_reason ? "|" : "",
						   oper_reason ? oper_reason : "");
			}
		}
	}
}

static void
stats_klines(client::client &source)
{
	/* Oper only, if unopered, return ERR_NOPRIVS */
	if((ConfigFileEntry.stats_k_oper_only == 2) && !is(source, umode::OPER))
		sendto_one_numeric(&source, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));

	/* If unopered, Only return matching klines */
	else if((ConfigFileEntry.stats_k_oper_only == 1) && !is(source, umode::OPER))
	{
		struct ConfItem *aconf;
		char *host, *pass, *user, *oper_reason;

		/* search for a kline */
		if(my_connect(source))
			aconf = find_conf_by_address (source.host, source.sockhost, NULL,
						      (struct sockaddr *)&source.localClient->ip,
						      CONF_KILL,
						      GET_SS_FAMILY(&source.localClient->ip),
						      source.username, NULL);
		else
			aconf = find_conf_by_address (source.host, NULL, NULL, NULL, CONF_KILL,
						      0, source.username, NULL);

		if(aconf == NULL)
			return;

		get_printable_kline(&source, aconf, &host, &pass, &user, &oper_reason);

		sendto_one_numeric(&source, RPL_STATSKLINE, form_str(RPL_STATSKLINE),
				   aconf->flags & CONF_FLAGS_TEMPORARY ? 'k' : 'K',
				   host, user, pass, oper_reason ? "|" : "",
				   oper_reason ? oper_reason : "");
	}
	/* Theyre opered, or allowed to see all klines */
	else
		report_Klines (source);
}

static void
stats_messages(client::client &source)
{
	for (auto it = cmd_dict.begin(); it != cmd_dict.end(); it++)
	{
		const auto msg = it->second;
		s_assert(msg->cmd != NULL);

		sendto_one_numeric(&source, RPL_STATSCOMMANDS,
				   form_str(RPL_STATSCOMMANDS),
				   msg->cmd, msg->count,
				   msg->bytes, msg->rcount);
	}
}

static void
stats_dnsbl(client::client &source)
{
	rb_dictionary_iter iter;
	struct BlacklistStats *stats;

	if(bl_stats == NULL)
		return;

	void *elem;
	RB_DICTIONARY_FOREACH(elem, &iter, bl_stats)
	{
		stats = (BlacklistStats *)elem;

		/* use RPL_STATSDEBUG for now -- jilles */
		sendto_one_numeric(&source, RPL_STATSDEBUG, "n :%d %s",
				stats->hits, (const char *)iter.cur->key);
	}
}

static void
stats_oper(client::client &source)
{
	struct oper_conf *oper_p;
	rb_dlink_node *ptr;

	if(!is(source, umode::OPER) && ConfigFileEntry.stats_o_oper_only)
	{
		sendto_one_numeric(&source, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));
		return;
	}

	RB_DLINK_FOREACH(ptr, oper_conf_list.head)
	{
		oper_p = (oper_conf *)ptr->data;

		sendto_one_numeric(&source, RPL_STATSOLINE,
				form_str(RPL_STATSOLINE),
				oper_p->username, oper_p->host, oper_p->name,
				is(source, umode::OPER) ? oper_p->privset->name : "0", "-1");
	}
}

static void
stats_capability_walk(const std::string &line, void *data)
{
	client::client *client = (client::client *)data;

	sendto_one_numeric(client, RPL_STATSDEBUG, "C :%s", line.c_str());
}

static void
stats_capability(client::client &client)
{
	capability::stats(stats_capability_walk, &client);
}

static void
stats_privset(client::client &source)
{
	privilegeset_report(&source);
}

/* stats_operedup()
 *
 * input	- client pointer
 * output	- none
 * side effects - client is shown a list of active opers
 */
static void
stats_operedup (client::client &source)
{
	client::client *target_p;
	rb_dlink_node *oper_ptr;
	unsigned int count = 0;

	RB_DLINK_FOREACH (oper_ptr, oper_list.head)
	{
		target_p = (client::client *)oper_ptr->data;

		if(IsOperInvis(target_p) && !is(source, umode::OPER))
			continue;

		if(away(user(*target_p)).size())
			continue;

		count++;

		sendto_one_numeric(&source, RPL_STATSDEBUG,
				   "p :%s (%s@%s)",
				   target_p->name, target_p->username,
				   target_p->host);
	}

	sendto_one_numeric(&source, RPL_STATSDEBUG,
				"p :%u staff members", count);

	stats_p_spy (source);
}

static void
stats_ports (client::client &source)
{
	if(!is(source, umode::OPER) && ConfigFileEntry.stats_P_oper_only)
		sendto_one_numeric(&source, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));
	else
		show_ports (&source);
}

static void
stats_tresv(client::client &source)
{
	struct ConfItem *aconf;
	rb_radixtree_iteration_state state;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, resv_conf_list.head)
	{
		aconf = (ConfItem *)ptr->data;
		if(aconf->hold)
			sendto_one_numeric(&source, RPL_STATSQLINE,
					form_str(RPL_STATSQLINE),
					'q', aconf->port, aconf->host, aconf->passwd);
	}

	void *elem;
	RB_RADIXTREE_FOREACH(elem, &state, resv_tree)
	{
		aconf = (ConfItem *)elem;
		if(aconf->hold)
			sendto_one_numeric(&source, RPL_STATSQLINE,
					form_str(RPL_STATSQLINE),
					'q', aconf->port, aconf->host, aconf->passwd);
	}
}


static void
stats_resv(client::client &source)
{
	struct ConfItem *aconf;
	rb_radixtree_iteration_state state;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, resv_conf_list.head)
	{
		aconf = (ConfItem *)ptr->data;
		if(!aconf->hold)
			sendto_one_numeric(&source, RPL_STATSQLINE,
					form_str(RPL_STATSQLINE),
					'Q', aconf->port, aconf->host, aconf->passwd);
	}

	void *elem;
	RB_RADIXTREE_FOREACH(elem, &state, resv_tree)
	{
		aconf = (ConfItem *)elem;
		if(!aconf->hold)
			sendto_one_numeric(&source, RPL_STATSQLINE,
					form_str(RPL_STATSQLINE),
					'Q', aconf->port, aconf->host, aconf->passwd);
	}
}

static void
stats_ssld_foreach(void *data, pid_t pid, int cli_count, enum ssld_status status, const char *version)
{
	client::client *source = (client::client *)data;

	sendto_one_numeric(source, RPL_STATSDEBUG,
			"S :%u %c %u :%s",
			pid,
			status == SSLD_DEAD ? 'D' : (status == SSLD_SHUTDOWN ? 'S' : 'A'),
			cli_count,
			version);
}

static void
stats_ssld(client::client &source)
{
	ssld_foreach_info(stats_ssld_foreach, &source);
}

static void
stats_usage (client::client &source)
{
#ifndef _WIN32
	struct rusage rus;
	time_t secs;
	time_t rup;
#ifdef  hz
# define hzz hz
#else
# ifdef HZ
#  define hzz HZ
# else
	int hzz = 1;
# endif
#endif

	if(getrusage(RUSAGE_SELF, &rus) == -1)
	{
		sendto_one_notice(&source, ":Getruseage error: %s.",
				  strerror(errno));
		return;
	}
	secs = rus.ru_utime.tv_sec + rus.ru_stime.tv_sec;
	if(0 == secs)
		secs = 1;

	rup = (rb_current_time() - info::startup_time) * hzz;
	if(0 == rup)
		rup = 1;

	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "R :CPU Secs %d:%02d User %d:%02d System %d:%02d",
			   (int) (secs / 60), (int) (secs % 60),
			   (int) (rus.ru_utime.tv_sec / 60),
			   (int) (rus.ru_utime.tv_sec % 60),
			   (int) (rus.ru_stime.tv_sec / 60),
			   (int) (rus.ru_stime.tv_sec % 60));
	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "R :RSS %ld ShMem %ld Data %ld Stack %ld",
			   rus.ru_maxrss, (rus.ru_ixrss / rup),
			   (rus.ru_idrss / rup), (rus.ru_isrss / rup));
	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "R :Swaps %d Reclaims %d Faults %d",
			   (int) rus.ru_nswap, (int) rus.ru_minflt, (int) rus.ru_majflt);
	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "R :Block in %d out %d",
			   (int) rus.ru_inblock, (int) rus.ru_oublock);
	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "R :Msg Rcv %d Send %d",
			   (int) rus.ru_msgrcv, (int) rus.ru_msgsnd);
	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "R :Signals %d Context Vol. %d Invol %d",
			   (int) rus.ru_nsignals, (int) rus.ru_nvcsw,
			   (int) rus.ru_nivcsw);
#endif
}

static void
stats_tstats (client::client &source)
{
	client::client *target_p;
	struct ServerStatistics sp;
	rb_dlink_node *ptr;

	memcpy(&sp, &ServerStats, sizeof(struct ServerStatistics));

	RB_DLINK_FOREACH(ptr, serv_list.head)
	{
		target_p = (client::client *)ptr->data;

		sp.is_sbs += target_p->localClient->sendB;
		sp.is_sbr += target_p->localClient->receiveB;
		sp.is_sti += (unsigned long long)(rb_current_time() - target_p->localClient->firsttime);
		sp.is_sv++;
	}

	RB_DLINK_FOREACH(ptr, lclient_list.head)
	{
		target_p = (client::client *)ptr->data;

		sp.is_cbs += target_p->localClient->sendB;
		sp.is_cbr += target_p->localClient->receiveB;
		sp.is_cti += (unsigned long long)(rb_current_time() - target_p->localClient->firsttime);
		sp.is_cl++;
	}

	RB_DLINK_FOREACH(ptr, unknown_list.head)
	{
		sp.is_ni++;
	}

	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "T :accepts %u refused %u", sp.is_ac, sp.is_ref);
	sendto_one_numeric(&source, RPL_STATSDEBUG,
			"T :rejected %u delaying %lu",
			sp.is_rej, delay_exit_length());
	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "T :throttled refused %u throttle list size %lu", sp.is_thr, throttle_size());
	sendto_one_numeric(&source, RPL_STATSDEBUG,
			"T :nicks being delayed %lu",
			get_nd_count());
	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "T :unknown commands %u prefixes %u",
			   sp.is_unco, sp.is_unpf);
	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "T :nick collisions %u saves %u unknown closes %u",
			   sp.is_kill, sp.is_save, sp.is_ni);
	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "T :wrong direction %u empty %u",
			   sp.is_wrdi, sp.is_empt);
	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "T :numerics seen %u", sp.is_num);
	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "T :tgchange blocked msgs %u restricted addrs %lu",
			   sp.is_tgch, rb_dlink_list_length(&tgchange_list));
	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "T :ratelimit blocked commands %u", sp.is_rl);
	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "T :auth successes %u fails %u",
			   sp.is_asuc, sp.is_abad);
	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "T :sasl successes %u fails %u",
			   sp.is_ssuc, sp.is_sbad);
	sendto_one_numeric(&source, RPL_STATSDEBUG, "T :Client Server");
	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "T :connected %u %u", sp.is_cl, sp.is_sv);
	sendto_one_numeric(&source, RPL_STATSDEBUG,
				"T :bytes sent %lluK %lluK",
				sp.is_cbs / 1024,
				sp.is_sbs / 1024);
	sendto_one_numeric(&source, RPL_STATSDEBUG,
				"T :bytes recv %lluK %lluK",
				sp.is_cbr / 1024,
				sp.is_sbr / 1024);
	sendto_one_numeric(&source, RPL_STATSDEBUG,
				"T :time connected %llu %llu",
				sp.is_cti, sp.is_sti);
}

static void
stats_uptime (client::client &source)
{
	time_t now;

	now = rb_current_time() - info::startup_time;
	sendto_one_numeric(&source, RPL_STATSUPTIME,
			   form_str (RPL_STATSUPTIME),
			   (int)(now / 86400), (int)((now / 3600) % 24),
			   (int)((now / 60) % 60), (int)(now % 60));
	sendto_one_numeric(&source, RPL_STATSCONN,
			   form_str (RPL_STATSCONN),
			   MaxConnectionCount, MaxClientCount,
			   Count.totalrestartcount);
}

struct shared_flags
{
	int flag;
	char letter;
};
static struct shared_flags shared_flagtable[] =
{
	{ SHARED_PKLINE,	'K' },
	{ SHARED_TKLINE,	'k' },
	{ SHARED_UNKLINE,	'U' },
	{ SHARED_PXLINE,	'X' },
	{ SHARED_TXLINE,	'x' },
	{ SHARED_UNXLINE,	'Y' },
	{ SHARED_PRESV,		'Q' },
	{ SHARED_TRESV,		'q' },
	{ SHARED_UNRESV,	'R' },
	{ SHARED_LOCOPS,	'L' },
	{ SHARED_REHASH,	'H' },
	{ SHARED_TDLINE,	'd' },
	{ SHARED_PDLINE,	'D' },
	{ SHARED_UNDLINE,	'E' },
	{ SHARED_GRANT,		'G' },
	{ SHARED_DIE,		'I' },
	{ 0,			'\0'}
};


static void
stats_shared (client::client &source)
{
	struct remote_conf *shared_p;
	rb_dlink_node *ptr;
	char buf[sizeof(shared_flagtable)/sizeof(shared_flagtable[0])];
	char *p;
	int i;

	RB_DLINK_FOREACH(ptr, shared_conf_list.head)
	{
		shared_p = (remote_conf *)ptr->data;

		p = buf;

		*p++ = 'c';

		for(i = 0; shared_flagtable[i].flag != 0; i++)
		{
			if(shared_p->flags & shared_flagtable[i].flag)
				*p++ = shared_flagtable[i].letter;
		}

		*p = '\0';

		sendto_one_numeric(&source, RPL_STATSULINE,
					form_str(RPL_STATSULINE),
					shared_p->server, shared_p->username,
					shared_p->host, buf);
	}

	RB_DLINK_FOREACH(ptr, cluster_conf_list.head)
	{
		shared_p = (remote_conf *)ptr->data;

		p = buf;

		*p++ = 'C';

		for(i = 0; shared_flagtable[i].flag != 0; i++)
		{
			if(shared_p->flags & shared_flagtable[i].flag)
				*p++ = shared_flagtable[i].letter;
		}

		*p = '\0';

		sendto_one_numeric(&source, RPL_STATSULINE,
					form_str(RPL_STATSULINE),
					shared_p->server, "*", "*", buf);
	}
}

/* stats_servers()
 *
 * input	- client pointer
 * output	- none
 * side effects - client is shown lists of who connected servers
 */
static void
stats_servers (client::client &source)
{
	client::client *target_p;
	rb_dlink_node *ptr;
	time_t seconds;
	int days, hours, minutes;
	int j = 0;

	if(ConfigServerHide.flatten_links && !is(source, umode::OPER) &&
	   !is_exempt_shide(source))
	{
		sendto_one_numeric(&source, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));
		return;
	}

	RB_DLINK_FOREACH (ptr, serv_list.head)
	{
		target_p = (client::client *)ptr->data;

		j++;
		seconds = rb_current_time() - target_p->localClient->firsttime;

		days = (int) (seconds / 86400);
		seconds %= 86400;
		hours = (int) (seconds / 3600);
		seconds %= 3600;
		minutes = (int) (seconds / 60);
		seconds %= 60;

		sendto_one_numeric(&source, RPL_STATSDEBUG,
				   "V :%s (%s!*@*) Idle: %d SendQ: %d "
				   "Connected: %d day%s, %d:%02d:%02d",
				   target_p->name,
				   (by(serv(*target_p)).size()? by(serv(*target_p)).c_str() : "Remote."),
				   (int) (rb_current_time() - target_p->localClient->lasttime),
				   (int) rb_linebuf_len (&target_p->localClient->buf_sendq),
				   days, (days == 1) ? "" : "s", hours, minutes,
				   (int) seconds);
	}

	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "V :%d Server(s)", j);
}

static void
stats_tgecos(client::client &source)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, xline_conf_list.head)
	{
		aconf = (ConfItem *)ptr->data;

		if(aconf->hold)
			sendto_one_numeric(&source, RPL_STATSXLINE,
					form_str(RPL_STATSXLINE),
					'x', aconf->port, aconf->host,
					aconf->passwd);
	}
}

static void
stats_gecos(client::client &source)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, xline_conf_list.head)
	{
		aconf = (ConfItem *)ptr->data;

		if(!aconf->hold)
			sendto_one_numeric(&source, RPL_STATSXLINE,
					form_str(RPL_STATSXLINE),
					'X', aconf->port, aconf->host,
					aconf->passwd);
	}
}

static void
stats_class(client::client &source)
{
	if(ConfigFileEntry.stats_y_oper_only && !is(source, umode::OPER))
		sendto_one_numeric(&source, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));
	else
		report_classes(&source);
}

static void
stats_memory (client::client &source)
{
	client::client *target_p;
	chan::chan *chptr;
	rb_dlink_node *rb_dlink;
	rb_dlink_node *ptr;
	int channel_count = 0;
	int local_client_conf_count = 0;	/* local client conf links */
	int users_counted = 0;	/* user structs */

	int channel_users = 0;
	int channel_invites = 0;
	int channel_bans = 0;
	int channel_except = 0;
	int channel_invex = 0;
	int channel_quiets = 0;

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
	size_t ww = 0;		/* whowas array count */
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

	whowas_memory_usage(&ww, &wwm);

	RB_DLINK_FOREACH(ptr, global_client_list.head)
	{
		target_p = (client::client *)ptr->data;
		if(my_connect(*target_p))
		{
			local_client_conf_count++;
		}

		if(target_p->user)
		{
			users_counted++;
			users_invited_count += invites(user(*target_p)).size();
			user_channels += chans(user(*target_p)).size();
			if(away(user(*target_p)).size())
			{
				aways_counted++;
				away_memory += away(user(*target_p)).size() + 1;
			}
		}
	}

	/* Count up all channels, ban lists, except lists, Invex lists */
	for(const auto &pit : chan::chans)
	{
		chptr = pit.second.get();
		channel_count++;
		channel_memory += (strlen(chptr->name.c_str()) + sizeof(chan::chan));

		channel_users += size(chptr->members);
		channel_invites += chptr->invites.size();

		channel_bans += size(*chptr, chan::mode::BAN);
		channel_ban_memory += size(*chptr, chan::mode::BAN) * sizeof(chan::ban);

		channel_except += size(*chptr, chan::mode::EXCEPTION);
		channel_except_memory += size(*chptr, chan::mode::EXCEPTION) * sizeof(chan::ban);

		channel_invex += size(*chptr, chan::mode::INVEX);
		channel_invex_memory += size(*chptr, chan::mode::INVEX) * sizeof(chan::ban);

		channel_quiets += size(*chptr, chan::mode::QUIET);
		channel_quiet_memory += size(*chptr, chan::mode::QUIET) * sizeof(chan::ban);
	}

	/* count up all classes */

	class_count = rb_dlink_list_length(&class_list) + 1;

	rb_count_rb_linebuf_memory(&linebuf_count, &linebuf_memory_used);

	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "z :Users %u(%lu) Invites %u(%lu)",
			   users_counted,
			   (unsigned long) users_counted * 1,  //TODO: XXX:
			   users_invited_count,
			   (unsigned long) users_invited_count * sizeof(rb_dlink_node));

	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "z :User channels %u(%lu) Aways %u(%d)",
			   user_channels,
			   (unsigned long) user_channels * sizeof(rb_dlink_node),
			   aways_counted, (int) away_memory);

	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "z :Attached confs %u(%lu)",
			   local_client_conf_count,
			   (unsigned long) local_client_conf_count * sizeof(rb_dlink_node));

	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "z :Conflines %u(%d)", conf_count, (int) conf_memory);

	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "z :Classes %u(%lu)",
			   class_count,
			   (unsigned long) class_count * sizeof(struct Class));

	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "z :Channels %u(%d)",
			   channel_count, (int) channel_memory);

	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "z :Bans %u(%d) Exceptions %u(%d) Invex %u(%d) Quiets %u(%d)",
			   channel_bans, (int) channel_ban_memory,
			   channel_except, (int) channel_except_memory,
			   channel_invex, (int) channel_invex_memory,
			   channel_quiets, (int) channel_quiet_memory);

	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "z :Channel members %u(%lu) invite %u(%lu)",
			   channel_users,
			   (unsigned long) channel_users * sizeof(rb_dlink_node),
			   channel_invites,
			   (unsigned long) channel_invites * sizeof(rb_dlink_node));

	total_channel_memory = channel_memory +
		channel_ban_memory +
		channel_users * sizeof(rb_dlink_node) + channel_invites * sizeof(rb_dlink_node);

	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "z :Whowas array %ld(%ld)",
			   (long)ww, (long)wwm);

	totww = wwm;

	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "z :Hash: client %u(%ld) chan %u(%ld)",
			   U_MAX, (long)(U_MAX * sizeof(rb_dlink_list)),
			   CH_MAX, (long)(CH_MAX * sizeof(rb_dlink_list)));

	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "z :linebuf %ld(%ld)",
			   (long)linebuf_count, (long)linebuf_memory_used);

	count_scache(&number_servers_cached, &mem_servers_cached);

	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "z :scache %ld(%ld)",
			   (long)number_servers_cached, (long)mem_servers_cached);

	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "z :hostname hash %d(%ld)",
			   HOST_MAX, (long)HOST_MAX * sizeof(rb_dlink_list));

	total_memory = totww + total_channel_memory + conf_memory +
		class_count * sizeof(struct Class);

	total_memory += mem_servers_cached;
	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "z :Total: whowas %d channel %d conf %d",
			   (int) totww, (int) total_channel_memory,
			   (int) conf_memory);

	client::count_local_client_memory(&local_client_count, &local_client_memory_used);
	total_memory += local_client_memory_used;

	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "z :Local client Memory in use: %ld(%ld)",
			   (long)local_client_count, (long)local_client_memory_used);


	client::count_remote_client_memory(&remote_client_count, &remote_client_memory_used);
	total_memory += remote_client_memory_used;

	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "z :Remote client Memory in use: %ld(%ld)",
			   (long)remote_client_count,
			   (long)remote_client_memory_used);
}

static void
stats_ziplinks (client::client &source)
{
	rb_dlink_node *ptr;
	client::client *target_p;
	struct ZipStats *zipstats;
	int sent_data = 0;
	char buf[128], buf1[128];
	RB_DLINK_FOREACH (ptr, serv_list.head)
	{
		target_p = (client::client *)ptr->data;
		if(IsCapable (target_p, CAP_ZIP))
		{
			zipstats = target_p->localClient->zipstats;
			sprintf(buf, "%.2f%%", zipstats->out_ratio);
			sprintf(buf1, "%.2f%%", zipstats->in_ratio);
			sendto_one_numeric(&source, RPL_STATSDEBUG,
					    "Z :ZipLinks stats for %s send[%s compression "
					    "(%llu kB data/%llu kB wire)] recv[%s compression "
					    "(%llu kB data/%llu kB wire)]",
					    target_p->name,
					    buf, zipstats->out >> 10,
					    zipstats->out_wire >> 10, buf1,
					    zipstats->in >> 10, zipstats->in_wire >> 10);
			sent_data++;
		}
	}

	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "Z :%u ziplink(s)", sent_data);
}

static void
stats_servlinks (client::client &source)
{
	static char Sformat[] = ":%s %d %s %s %u %u %u %u %u :%u %u %s";
	long uptime, sendK, receiveK;
	client::client *target_p;
	rb_dlink_node *ptr;
	int j = 0;
	char buf[128];

	if(ConfigServerHide.flatten_links && !is(source, umode::OPER) &&
	   !is_exempt_shide(source))
	{
		sendto_one_numeric(&source, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));
		return;
	}

	sendK = receiveK = 0;

	RB_DLINK_FOREACH (ptr, serv_list.head)
	{
		target_p = (client::client *)ptr->data;

		j++;
		sendK += target_p->localClient->sendK;
		receiveK += target_p->localClient->receiveK;

		sendto_one(&source, Sformat,
			get_id(&me, &source), RPL_STATSLINKINFO, get_id(&source, &source),
			target_p->name,
			(int) rb_linebuf_len (&target_p->localClient->buf_sendq),
			(int) target_p->localClient->sendM,
			(int) target_p->localClient->sendK,
			(int) target_p->localClient->receiveM,
			(int) target_p->localClient->receiveK,
			rb_current_time() - target_p->localClient->firsttime,
			(rb_current_time() > target_p->localClient->lasttime) ?
			 (rb_current_time() - target_p->localClient->lasttime) : 0,
			is(source, umode::OPER) ? show_capabilities (target_p) : "TS");
	}

	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "? :%u total server(s)", j);

	snprintf(buf, sizeof buf, "%7.2f", _GMKv ((sendK)));
	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "? :Sent total : %s %s",
			   buf, _GMKs (sendK));
	snprintf(buf, sizeof buf, "%7.2f", _GMKv ((receiveK)));
	sendto_one_numeric(&source, RPL_STATSDEBUG,
			   "? :Recv total : %s %s",
			   buf, _GMKs (receiveK));

	uptime = rb_current_time() - info::startup_time;
	snprintf(buf, sizeof buf, "%7.2f %s (%4.1f K/s)",
			   _GMKv (me.localClient->sendK),
			   _GMKs (me.localClient->sendK),
			   (float) ((float) me.localClient->sendK / (float) uptime));
	sendto_one_numeric(&source, RPL_STATSDEBUG, "? :Server send: %s", buf);
	snprintf(buf, sizeof buf, "%7.2f %s (%4.1f K/s)",
			   _GMKv (me.localClient->receiveK),
			   _GMKs (me.localClient->receiveK),
			   (float) ((float) me.localClient->receiveK / (float) uptime));
	sendto_one_numeric(&source, RPL_STATSDEBUG, "? :Server recv: %s", buf);
}

static inline bool
stats_l_should_show_oper(client::client *target_p)
{
	return (!IsOperInvis(target_p));
}

static void
stats_ltrace(client::client &source, int parc, const char *parv[])
{
	bool doall = false;
	bool wilds = false;
	const char *name;
	char statchar = parv[1][0];

	/* this is def targeted at us somehow.. */
	if(parc > 2 && !EmptyString(parv[2]))
	{
		/* directed at us generically? */
		if(match(parv[2], me.name) ||
		   (!my(source) && !irccmp(parv[2], me.id)))
		{
			name = me.name;
			doall = true;
		}
		else
		{
			name = parv[2];
			wilds = strchr(name, '*') || strchr(name, '?');
		}

		/* must be directed at a specific person thats not us */
		if(!doall && !wilds)
		{
			client::client *target_p;

			if(my(source))
				target_p = client::find_named_person(name);
			else
				target_p = client::find_person(name);

			if(target_p != NULL)
			{
				stats_spy(source, statchar, target_p->name);
				stats_l_client(source, target_p, statchar);
			}
			else
				sendto_one_numeric(&source, ERR_NOSUCHSERVER,
						form_str(ERR_NOSUCHSERVER),
						name);

			return;
		}
	}
	else
	{
		name = me.name;
		doall = true;
	}

	stats_spy(source, statchar, name);

	if(doall)
	{
		/* local opers get everyone */
		if(my_oper(source))
		{
			stats_l_list(source, name, doall, wilds, &unknown_list, statchar, NULL);
			stats_l_list(source, name, doall, wilds, &lclient_list, statchar, NULL);
		}
		else
		{
			/* they still need themselves if theyre local.. */
			if(my(source))
				stats_l_client(source, &source, statchar);

			stats_l_list(source, name, doall, wilds, &local_oper_list, statchar, stats_l_should_show_oper);
		}

		if (!ConfigServerHide.flatten_links || is(source, umode::OPER) ||
				is_exempt_shide(source))
			stats_l_list(source, name, doall, wilds, &serv_list, statchar, NULL);

		return;
	}

	/* ok, at this point theyre looking for a specific client whos on
	 * our server.. but it contains a wildcard.  --fl
	 */
	stats_l_list(source, name, doall, wilds, &lclient_list, statchar, NULL);

	return;
}

static void
stats_l_list(client::client &source, const char *name, bool doall, bool wilds,
	     rb_dlink_list * list, char statchar, bool (*check_fn)(client::client *target_p))
{
	rb_dlink_node *ptr;
	client::client *target_p;

	/* send information about connections which match.  note, we
	 * dont need tests for is_invisible(*), because non-opers will
	 * never get here for normal clients --fl
	 */
	RB_DLINK_FOREACH(ptr, list->head)
	{
		target_p = (client::client *)ptr->data;

		if(!doall && wilds && !match(name, target_p->name))
			continue;

		if (check_fn == NULL || check_fn(target_p))
			stats_l_client(source, target_p, statchar);
	}
}

void
stats_l_client(client::client &source, client::client *target_p,
		char statchar)
{
	if(is_any_server(*target_p))
	{
		sendto_one_numeric(&source, RPL_STATSLINKINFO, Lformat,
				target_p->name,
				(int) rb_linebuf_len(&target_p->localClient->buf_sendq),
				(int) target_p->localClient->sendM,
				(int) target_p->localClient->sendK,
				(int) target_p->localClient->receiveM,
				(int) target_p->localClient->receiveK,
				rb_current_time() - target_p->localClient->firsttime,
				(rb_current_time() > target_p->localClient->lasttime) ?
				 (rb_current_time() - target_p->localClient->lasttime) : 0,
				is(source, umode::OPER) ? show_capabilities(target_p) : "-");
	}

	else
	{
		sendto_one_numeric(&source, RPL_STATSLINKINFO, Lformat,
				   show_ip(&source, target_p) ?
				    (rfc1459::is_upper(statchar) ?
				     get_client_name(target_p, SHOW_IP) :
				     get_client_name(target_p, HIDE_IP)) :
				    get_client_name(target_p, MASK_IP),
				    (int) rb_linebuf_len(&target_p->localClient->buf_sendq),
				    (int) target_p->localClient->sendM,
				    (int) target_p->localClient->sendK,
				    (int) target_p->localClient->receiveM,
				    (int) target_p->localClient->receiveK,
				    rb_current_time() - target_p->localClient->firsttime,
				    (rb_current_time() > target_p->localClient->lasttime) ?
				     (rb_current_time() - target_p->localClient->lasttime) : 0,
				    "-");
	}
}

static void
rb_dump_fd_callback(int fd, const char *desc, void *data)
{
	client::client *source = (client::client *)data;
	sendto_one_numeric(source, RPL_STATSDEBUG, "F :fd %-3d desc '%s'", fd, desc);
}

static void
stats_comm(client::client &source)
{
	rb_dump_fd(rb_dump_fd_callback, &source);
}

/*
 * stats_spy
 *
 * inputs	- pointer to client doing the /stats
 *		- char letter they are doing /stats on
 * output	- none
 * side effects -
 * This little helper function reports to opers if configured.
 */
static int
stats_spy(client::client &source, char statchar, const char *name)
{
	hook_data_int data;

	data.client = &source;
	data.arg1 = name;
	data.arg2 = (int) statchar;
	data.result = 0;

	call_hook(doing_stats_hook, &data);

	return data.result;
}

/* stats_p_spy()
 *
 * input	- pointer to client doing stats
 * ouput	-
 * side effects - call hook doing_stats_p
 */
static void
stats_p_spy (client::client &source)
{
	hook_data data;

	data.client = &source;
	data.arg1 = data.arg2 = NULL;

	call_hook(doing_stats_p_hook, &data);
}
