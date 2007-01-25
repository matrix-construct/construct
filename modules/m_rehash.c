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
 *
 *  $Id: m_rehash.c 932 2006-03-05 03:39:14Z nenolod $
 */

#include "stdinc.h"
#include "client.h"
#include "channel.h"
#include "common.h"
#include "irc_string.h"
#include "ircd.h"
#include "s_gline.h"
#include "s_serv.h"
#include "numeric.h"
#include "res.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "s_log.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "hostmask.h"
#include "reject.h"
#include "hash.h"
#include "cache.h"

static int mo_rehash(struct Client *, struct Client *, int, const char **);
static int me_rehash(struct Client *, struct Client *, int, const char **);

struct Message rehash_msgtab = {
	"REHASH", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_rehash, 0}, {mo_rehash, 0}}
};

mapi_clist_av1 rehash_clist[] = { &rehash_msgtab, NULL };
DECLARE_MODULE_AV1(rehash, NULL, NULL, rehash_clist, NULL, NULL, "$Revision: 932 $");

struct hash_commands
{
	const char *cmd;
	void (*handler) (struct Client * source_p);
};

static void
rehash_bans_loc(struct Client *source_p)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is rehashing bans",
				get_oper_name(source_p));

	rehash_bans(0);
}

static void
rehash_dns(struct Client *source_p)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is rehashing DNS", 
			     get_oper_name(source_p));

	/* reread /etc/resolv.conf and reopen res socket */
	restart_resolver();
}

static void
rehash_motd(struct Client *source_p)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			     "%s is forcing re-reading of MOTD file",
			     get_oper_name(source_p));

	free_cachefile(user_motd);
	user_motd = cache_file(MPATH, "ircd.motd", 0);
}

static void
rehash_omotd(struct Client *source_p)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			     "%s is forcing re-reading of OPER MOTD file",
			     get_oper_name(source_p));

	free_cachefile(oper_motd);
	oper_motd = cache_file(OPATH, "opers.motd", 0);
}

static void
rehash_glines(struct Client *source_p)
{
	struct ConfItem *aconf;
	dlink_node *ptr, *next_ptr;

	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is clearing G-lines",
				get_oper_name(source_p));

	DLINK_FOREACH_SAFE(ptr, next_ptr, glines.head)
	{
		aconf = ptr->data;

		delete_one_address_conf(aconf->host, aconf);
		dlinkDestroy(ptr, &glines);
	}
}

static void
rehash_pglines(struct Client *source_p)
{
	struct gline_pending *glp_ptr;
	dlink_node *ptr;
	dlink_node *next_ptr;

	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is clearing pending glines",
				get_oper_name(source_p));

	DLINK_FOREACH_SAFE(ptr, next_ptr, pending_glines.head)
	{
		glp_ptr = ptr->data;

		MyFree(glp_ptr->reason1);
		MyFree(glp_ptr->reason2);
		MyFree(glp_ptr);
		dlinkDestroy(ptr, &pending_glines);
	}
}

static void
rehash_tklines(struct Client *source_p)
{
	struct ConfItem *aconf;
	dlink_node *ptr, *next_ptr;
	int i;

	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is clearing temp klines",
				get_oper_name(source_p));

	for(i = 0; i < LAST_TEMP_TYPE; i++)
	{
		DLINK_FOREACH_SAFE(ptr, next_ptr, temp_klines[i].head)
		{
			aconf = ptr->data;

			delete_one_address_conf(aconf->host, aconf);
			dlinkDestroy(ptr, &temp_klines[i]);
		}
	}
}

static void
rehash_tdlines(struct Client *source_p)
{
	struct ConfItem *aconf;
	dlink_node *ptr, *next_ptr;
	int i;

	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is clearing temp dlines",
				get_oper_name(source_p));

	for(i = 0; i < LAST_TEMP_TYPE; i++)
	{
		DLINK_FOREACH_SAFE(ptr, next_ptr, temp_dlines[i].head)
		{
			aconf = ptr->data;

			delete_one_address_conf(aconf->host, aconf);
			dlinkDestroy(ptr, &temp_dlines[i]);
		}
	}
}

static void
rehash_txlines(struct Client *source_p)
{
	struct ConfItem *aconf;
	dlink_node *ptr;
	dlink_node *next_ptr;

	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is clearing temp xlines",
				get_oper_name(source_p));

	DLINK_FOREACH_SAFE(ptr, next_ptr, xline_conf_list.head)
	{
		aconf = ptr->data;

		if(!aconf->hold)
			continue;

		free_conf(aconf);
		dlinkDestroy(ptr, &xline_conf_list);
	}
}

static void
rehash_tresvs(struct Client *source_p)
{
	struct ConfItem *aconf;
	dlink_node *ptr;
	dlink_node *next_ptr;
	int i;

	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is clearing temp resvs",
				get_oper_name(source_p));

	HASH_WALK_SAFE(i, R_MAX, ptr, next_ptr, resvTable)
	{
		aconf = ptr->data;

		if(!aconf->hold)
			continue;

		free_conf(aconf);
		dlinkDestroy(ptr, &resvTable[i]);
	}
	HASH_WALK_END

	DLINK_FOREACH_SAFE(ptr, next_ptr, resv_conf_list.head)
	{
		aconf = ptr->data;

		if(!aconf->hold)
			continue;

		free_conf(aconf);
		dlinkDestroy(ptr, &resv_conf_list);
	}
}

static void
rehash_rejectcache(struct Client *source_p)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is clearing reject cache",
				get_oper_name(source_p));
	flush_reject();

}

static void
rehash_help(struct Client *source_p)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			     "%s is forcing re-reading of HELP files", 
			     get_oper_name(source_p));
	clear_help_hash();
	load_help();
}

static void
rehash_nickdelay(struct Client *source_p)
{
	struct nd_entry *nd;
	dlink_node *ptr;
	dlink_node *safe_ptr;

	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			     "%s is clearing the nick delay table",
			     get_oper_name(source_p));

	DLINK_FOREACH_SAFE(ptr, safe_ptr, nd_list.head)
	{
		nd = ptr->data;
	
		free_nd_entry(nd);
	}
}

/* *INDENT-OFF* */
static struct hash_commands rehash_commands[] =
{
	{"BANS",	rehash_bans_loc		},
	{"DNS", 	rehash_dns		},
	{"MOTD", 	rehash_motd		},
	{"OMOTD", 	rehash_omotd		},
	{"GLINES", 	rehash_glines		},
	{"PGLINES", 	rehash_pglines		},
	{"TKLINES", 	rehash_tklines		},
	{"TDLINES", 	rehash_tdlines		},
	{"TXLINES",	rehash_txlines		},
	{"TRESVS",	rehash_tresvs		},
	{"REJECTCACHE",	rehash_rejectcache	},
	{"HELP", 	rehash_help		},
	{"NICKDELAY",	rehash_nickdelay        },
	{NULL, 		NULL 			}
};
/* *INDENT-ON* */

static void
do_rehash(struct Client *source_p, const char *type)
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
				sendto_one(source_p, form_str(RPL_REHASHING), me.name,
					   source_p->name, rehash_commands[x].cmd);
				rehash_commands[x].handler(source_p);
				ilog(L_MAIN, "REHASH %s From %s[%s]", type,
				     get_oper_name(source_p), source_p->sockhost);
				return;
			}
		}

		/* We are still here..we didn't match */
		cmdbuf[0] = '\0';
		for (x = 0; rehash_commands[x].cmd != NULL && rehash_commands[x].handler != NULL;
		     x++)
		{
			strlcat(cmdbuf, " ", sizeof(cmdbuf));
			strlcat(cmdbuf, rehash_commands[x].cmd, sizeof(cmdbuf));
		}
		sendto_one(source_p, ":%s NOTICE %s :rehash one of:%s", me.name, source_p->name,
			   cmdbuf);
	}
	else
	{
		sendto_one(source_p, form_str(RPL_REHASHING), me.name, source_p->name,
			   ConfigFileEntry.configfile);
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "%s is rehashing server config file", get_oper_name(source_p));
		ilog(L_MAIN, "REHASH From %s[%s]", get_oper_name(source_p),
		     source_p->sockhost);
		rehash(0);
	}
}

/*
 * mo_rehash - REHASH message handler
 *
 * parv[1] = rehash type or destination
 * parv[2] = destination
 */
static int
mo_rehash(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	const char *type = NULL, *target_server = NULL;

	if(!IsOperRehash(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "rehash");
		return 0;
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
		if(!IsOperRemoteBan(source_p))
		{
			sendto_one(source_p, form_str(ERR_NOPRIVS),
				me.name, source_p->name, "remoteban");
			return 0;
		}
		sendto_match_servs(source_p, target_server,
				CAP_ENCAP, NOCAPS,
				"ENCAP %s REHASH %s",
				target_server, type != NULL ? type : "");
		if (match(target_server, me.name) == 0)
			return 0;
	}

	do_rehash(source_p, type);

	return 0;
}

static int
me_rehash(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{

	if (!IsPerson(source_p))
		return 0;

	if (!find_shared_conf(source_p->username, source_p->host,
				source_p->user->server, SHARED_REHASH))
		return 0;

	do_rehash(source_p, parc > 1 ? parv[1] : NULL);

	return 0;
}
