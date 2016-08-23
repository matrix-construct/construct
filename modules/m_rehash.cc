/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_rehash.c: Re-reads the configuration file.
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

static const char rehash_desc[] =
	"Provides the REHASH command to reload configuration and other files";

static void mo_rehash(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void me_rehash(struct MsgBuf *, client::client &, client::client &, int, const char **);

struct Message rehash_msgtab = {
	"REHASH", 0, 0, 0, 0,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_rehash, 0}, {mo_rehash, 0}}
};

mapi_clist_av1 rehash_clist[] = { &rehash_msgtab, NULL };

DECLARE_MODULE_AV2(rehash, NULL, NULL, rehash_clist, NULL, NULL, NULL, NULL, rehash_desc);

struct hash_commands
{
	const char *cmd;
	void (*handler) (client::client &source);
};

static void
rehash_bans_loc(client::client &source)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is rehashing bans",
				get_oper_name(&source));
	if (!MyConnect(&source))
		remote_rehash_oper_p = &source;

	rehash_bans();
}

static void
rehash_dns(client::client &source)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is rehashing DNS",
			     get_oper_name(&source));
	if (!MyConnect(&source))
		remote_rehash_oper_p = &source;

	reload_nameservers();
}

static void
rehash_ssld(client::client &source)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is restarting ssld",
				get_oper_name(&source));

	restart_ssld();
}

static void
rehash_motd(client::client &source)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			     "%s is forcing re-reading of MOTD file",
			     get_oper_name(&source));
	if (!MyConnect(&source))
		remote_rehash_oper_p = &source;

	cache::motd::cache_user();
}

static void
rehash_omotd(client::client &source)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			     "%s is forcing re-reading of OPER MOTD file",
			     get_oper_name(&source));
	if (!MyConnect(&source))
		remote_rehash_oper_p = &source;

	cache::motd::cache_oper();
}

static void
rehash_tklines(client::client &source)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr, *next_ptr;
	int i;

	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is clearing temp klines",
				get_oper_name(&source));
	if (!MyConnect(&source))
		remote_rehash_oper_p = &source;

	for(i = 0; i < LAST_TEMP_TYPE; i++)
	{
		RB_DLINK_FOREACH_SAFE(ptr, next_ptr, temp_klines[i].head)
		{
			aconf = (ConfItem *)ptr->data;

			delete_one_address_conf(aconf->host, aconf);
			rb_dlinkDestroy(ptr, &temp_klines[i]);
		}
	}
}

static void
rehash_tdlines(client::client &source)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr, *next_ptr;
	int i;

	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is clearing temp dlines",
				get_oper_name(&source));
	if (!MyConnect(&source))
		remote_rehash_oper_p = &source;

	for(i = 0; i < LAST_TEMP_TYPE; i++)
	{
		RB_DLINK_FOREACH_SAFE(ptr, next_ptr, temp_dlines[i].head)
		{
			aconf = (ConfItem *)ptr->data;

			delete_one_address_conf(aconf->host, aconf);
			rb_dlinkDestroy(ptr, &temp_dlines[i]);
		}
	}
}

static void
rehash_txlines(client::client &source)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;

	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is clearing temp xlines",
				get_oper_name(&source));
	if (!MyConnect(&source))
		remote_rehash_oper_p = &source;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, xline_conf_list.head)
	{
		aconf = (ConfItem *)ptr->data;

		if(!aconf->hold || aconf->lifetime)
			continue;

		free_conf(aconf);
		rb_dlinkDestroy(ptr, &xline_conf_list);
	}
}

static void
rehash_tresvs(client::client &source)
{
	struct ConfItem *aconf;
	rb_radixtree_iteration_state iter;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;

	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is clearing temp resvs",
				get_oper_name(&source));
	if (!MyConnect(&source))
		remote_rehash_oper_p = &source;

	void *elem;
	RB_RADIXTREE_FOREACH(elem, &iter, resv_tree)
	{
		const auto aconf(reinterpret_cast<ConfItem *>(elem));
		if(!aconf->hold || aconf->lifetime)
			continue;

		rb_radixtree_delete(resv_tree, aconf->host);
		free_conf(aconf);
	}

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, resv_conf_list.head)
	{
		aconf = (ConfItem *)ptr->data;

		if(!aconf->hold || aconf->lifetime)
			continue;

		free_conf(aconf);
		rb_dlinkDestroy(ptr, &resv_conf_list);
	}
}

static void
rehash_rejectcache(client::client &source)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is clearing reject cache",
				get_oper_name(&source));
	if (!MyConnect(&source))
		remote_rehash_oper_p = &source;
	flush_reject();

}

static void
rehash_throttles(client::client &source)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is clearing throttles",
				get_oper_name(&source));
	if (!MyConnect(&source))
		remote_rehash_oper_p = &source;
	flush_throttle();

}

static void
rehash_help(client::client &source)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			     "%s is forcing re-reading of HELP files",
			     get_oper_name(&source));
	if (!MyConnect(&source))
		remote_rehash_oper_p = &source;
	cache::help::load();
}

static void
rehash_nickdelay(client::client &source)
{
	struct nd_entry *nd;
	rb_dlink_node *ptr;
	rb_dlink_node *safe_ptr;

	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			     "%s is clearing the nick delay table",
			     get_oper_name(&source));
	if (!MyConnect(&source))
		remote_rehash_oper_p = &source;

	RB_DLINK_FOREACH_SAFE(ptr, safe_ptr, nd_list.head)
	{
		nd = (nd_entry *)ptr->data;

		free_nd_entry(nd);
	}
}

/* *INDENT-OFF* */
static struct hash_commands rehash_commands[] =
{
	{"BANS",	rehash_bans_loc		},
	{"DNS", 	rehash_dns		},
	{"SSLD", 	rehash_ssld		},
	{"MOTD", 	rehash_motd		},
	{"OMOTD", 	rehash_omotd		},
	{"TKLINES", 	rehash_tklines		},
	{"TDLINES", 	rehash_tdlines		},
	{"TXLINES",	rehash_txlines		},
	{"TRESVS",	rehash_tresvs		},
	{"REJECTCACHE",	rehash_rejectcache	},
	{"THROTTLES",	rehash_throttles	},
	{"HELP", 	rehash_help		},
	{"NICKDELAY",	rehash_nickdelay        },
	{NULL, 		NULL 			}
};
/* *INDENT-ON* */

static void
do_rehash(client::client &source, const char *type)
{
	if (type != NULL)
	{
		int x;
		char cmdbuf[100];

		for (x = 0; rehash_commands[x].cmd != NULL && rehash_commands[x].handler != NULL;
		     x++)
		{
			if(irccmp(type, rehash_commands[x].cmd) == 0)
			{
				sendto_one(&source, form_str(RPL_REHASHING), me.name,
					   source.name, rehash_commands[x].cmd);
				ilog(L_MAIN, "REHASH %s From %s[%s]", type,
				     get_oper_name(&source), source.sockhost);
				rehash_commands[x].handler(source);
				remote_rehash_oper_p = NULL;
				return;
			}
		}

		/* We are still here..we didn't match */
		cmdbuf[0] = '\0';
		for (x = 0; rehash_commands[x].cmd != NULL && rehash_commands[x].handler != NULL;
		     x++)
		{
			rb_strlcat(cmdbuf, " ", sizeof(cmdbuf));
			rb_strlcat(cmdbuf, rehash_commands[x].cmd, sizeof(cmdbuf));
		}
		sendto_one_notice(&source, ":rehash one of:%s", cmdbuf);
	}
	else
	{
		sendto_one(&source, form_str(RPL_REHASHING), me.name, source.name,
			   ConfigFileEntry.configfile);
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "%s is rehashing server config file", get_oper_name(&source));
		if (!MyConnect(&source))
			remote_rehash_oper_p = &source;
		ilog(L_MAIN, "REHASH From %s[%s]", get_oper_name(&source),
		     source.sockhost);
		rehash(false);
		remote_rehash_oper_p = NULL;
	}
}

/*
 * mo_rehash - REHASH message handler
 *
 * parv[1] = rehash type or destination
 * parv[2] = destination
 */
static void
mo_rehash(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	const char *type = NULL, *target_server = NULL;

	if(!IsOperRehash(&source))
	{
		sendto_one(&source, form_str(ERR_NOPRIVS),
			   me.name, source.name, "rehash");
		return;
	}

	if (parc > 2)
		type = parv[1], target_server = parv[2];
	else if (parc > 1 && (strchr(parv[1], '.') || strchr(parv[1], '?') || strchr(parv[1], '*')))
		type = NULL, target_server = parv[1];
	else if (parc > 1)
		type = parv[1], target_server = NULL;
	else
		type = NULL, target_server = NULL;

	if (target_server != NULL)
	{
		if(!IsOperRemoteBan(&source))
		{
			sendto_one(&source, form_str(ERR_NOPRIVS),
				me.name, source.name, "remoteban");
			return;
		}
		sendto_match_servs(&source, target_server,
				CAP_ENCAP, NOCAPS,
				"ENCAP %s REHASH %s",
				target_server, type != NULL ? type : "");
		if (match(target_server, me.name) == 0)
			return;
	}

	do_rehash(source, type);
}

static void
me_rehash(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{

	if (!IsPerson(&source))
		return;

	if (!find_shared_conf(source.username, source.host,
				source.servptr->name, SHARED_REHASH))
		return;

	do_rehash(source, parc > 1 ? parv[1] : NULL);
}
