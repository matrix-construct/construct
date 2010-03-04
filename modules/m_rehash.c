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
 *  $Id: m_rehash.c 3161 2007-01-25 07:23:01Z nenolod $
 */

#include "stdinc.h"
#include "client.h"
#include "channel.h"
#include "common.h"
#include "match.h"
#include "ircd.h"
#include "s_serv.h"
#include "numeric.h"
#include "res.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "logger.h"
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
DECLARE_MODULE_AV1(rehash, NULL, NULL, rehash_clist, NULL, NULL, "$Revision: 3161 $");

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
	if (!MyConnect(source_p))
		remote_rehash_oper_p = source_p;

	rehash_bans(0);
}

static void
rehash_dns(struct Client *source_p)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is rehashing DNS", 
			     get_oper_name(source_p));
	if (!MyConnect(source_p))
		remote_rehash_oper_p = source_p;

	/* reread /etc/resolv.conf and reopen res socket */
	restart_resolver();
}

static void
rehash_motd(struct Client *source_p)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			     "%s is forcing re-reading of MOTD file",
			     get_oper_name(source_p));
	if (!MyConnect(source_p))
		remote_rehash_oper_p = source_p;

	cache_user_motd();
}

static void
rehash_omotd(struct Client *source_p)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			     "%s is forcing re-reading of OPER MOTD file",
			     get_oper_name(source_p));
	if (!MyConnect(source_p))
		remote_rehash_oper_p = source_p;

	free_cachefile(oper_motd);
	oper_motd = cache_file(OPATH, "opers.motd", 0);
}

static void
rehash_tklines(struct Client *source_p)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr, *next_ptr;
	int i;

	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is clearing temp klines",
				get_oper_name(source_p));
	if (!MyConnect(source_p))
		remote_rehash_oper_p = source_p;

	for(i = 0; i < LAST_TEMP_TYPE; i++)
	{
		RB_DLINK_FOREACH_SAFE(ptr, next_ptr, temp_klines[i].head)
		{
			aconf = ptr->data;

			delete_one_address_conf(aconf->host, aconf);
			rb_dlinkDestroy(ptr, &temp_klines[i]);
		}
	}
}

static void
rehash_tdlines(struct Client *source_p)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr, *next_ptr;
	int i;

	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is clearing temp dlines",
				get_oper_name(source_p));
	if (!MyConnect(source_p))
		remote_rehash_oper_p = source_p;

	for(i = 0; i < LAST_TEMP_TYPE; i++)
	{
		RB_DLINK_FOREACH_SAFE(ptr, next_ptr, temp_dlines[i].head)
		{
			aconf = ptr->data;

			delete_one_address_conf(aconf->host, aconf);
			rb_dlinkDestroy(ptr, &temp_dlines[i]);
		}
	}
}

static void
rehash_txlines(struct Client *source_p)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;

	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is clearing temp xlines",
				get_oper_name(source_p));
	if (!MyConnect(source_p))
		remote_rehash_oper_p = source_p;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, xline_conf_list.head)
	{
		aconf = ptr->data;

		if(!aconf->hold || aconf->lifetime)
			continue;

		free_conf(aconf);
		rb_dlinkDestroy(ptr, &xline_conf_list);
	}
}

static void
rehash_tresvs(struct Client *source_p)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	int i;

	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is clearing temp resvs",
				get_oper_name(source_p));
	if (!MyConnect(source_p))
		remote_rehash_oper_p = source_p;

	HASH_WALK_SAFE(i, R_MAX, ptr, next_ptr, resvTable)
	{
		aconf = ptr->data;

		if(!aconf->hold || aconf->lifetime)
			continue;

		free_conf(aconf);
		rb_dlinkDestroy(ptr, &resvTable[i]);
	}
	HASH_WALK_END

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, resv_conf_list.head)
	{
		aconf = ptr->data;

		if(!aconf->hold || aconf->lifetime)
			continue;

		free_conf(aconf);
		rb_dlinkDestroy(ptr, &resv_conf_list);
	}
}

static void
rehash_rejectcache(struct Client *source_p)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is clearing reject cache",
				get_oper_name(source_p));
	if (!MyConnect(source_p))
		remote_rehash_oper_p = source_p;
	flush_reject();

}

static void
rehash_throttles(struct Client *source_p)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s is clearing throttles",
				get_oper_name(source_p));
	if (!MyConnect(source_p))
		remote_rehash_oper_p = source_p;
	flush_throttle();

}

static void
rehash_help(struct Client *source_p)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			     "%s is forcing re-reading of HELP files", 
			     get_oper_name(source_p));
	if (!MyConnect(source_p))
		remote_rehash_oper_p = source_p;
	load_help();
}

static void
rehash_nickdelay(struct Client *source_p)
{
	struct nd_entry *nd;
	rb_dlink_node *ptr;
	rb_dlink_node *safe_ptr;

	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			     "%s is clearing the nick delay table",
			     get_oper_name(source_p));
	if (!MyConnect(source_p))
		remote_rehash_oper_p = source_p;

	RB_DLINK_FOREACH_SAFE(ptr, safe_ptr, nd_list.head)
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
				ilog(L_MAIN, "REHASH %s From %s[%s]", type,
				     get_oper_name(source_p), source_p->sockhost);
				rehash_commands[x].handler(source_p);
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
		sendto_one_notice(source_p, ":rehash one of:%s", cmdbuf);
	}
	else
	{
		sendto_one(source_p, form_str(RPL_REHASHING), me.name, source_p->name,
			   ConfigFileEntry.configfile);
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "%s is rehashing server config file", get_oper_name(source_p));
		if (!MyConnect(source_p))
			remote_rehash_oper_p = source_p;
		ilog(L_MAIN, "REHASH From %s[%s]", get_oper_name(source_p),
		     source_p->sockhost);
		rehash(0);
		remote_rehash_oper_p = NULL;
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
				source_p->servptr->name, SHARED_REHASH))
		return 0;

	do_rehash(source_p, parc > 1 ? parv[1] : NULL);

	return 0;
}
