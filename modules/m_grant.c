/*
 * Copyright (C) 2006 Jilles Tjoelker
 * Copyright (C) 2006 Stephen Bennett <spb@gentoo.org>
 */

#include "stdinc.h"
#include "modules.h"
#include "numeric.h"
#include "client.h"
#include "ircd.h"
#include "send.h"
#include "s_user.h"
#include "s_serv.h"
#include "s_conf.h"
#include "s_newconf.h"

static int mo_grant(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static int me_grant(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);

static int do_grant(struct Client *source_p, struct Client *target_p, const char *new_privset);

struct Message grant_msgtab = {
  "GRANT", 0, 0, 0, 0,
  { mg_ignore, mg_not_oper, mg_ignore, mg_ignore, {me_grant, 3}, {mo_grant, 3}}
};

mapi_clist_av1 grant_clist[] = { &grant_msgtab, NULL };

DECLARE_MODULE_AV2(grant, NULL, NULL, grant_clist, NULL, NULL, NULL, NULL, NULL);

static int
mo_grant(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;

	if(!HasPrivilege(source_p, "oper:grant"))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "grant");
		return 0;
	}

	target_p = find_named_person(parv[1]);
	if (target_p == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHNICK,
				form_str(ERR_NOSUCHNICK), parv[1]);
		return 0;
	}

	if (MyClient(target_p))
	{
		do_grant(source_p, target_p, parv[2]);
	}
	else
	{
		sendto_one(target_p, ":%s ENCAP %s GRANT %s %s",
				get_id(source_p, target_p), target_p->servptr->name,
				get_id(target_p, target_p), parv[2]);
	}

	return 0;
}

static int me_grant(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;

	target_p = find_person(parv[1]);
	if (target_p == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHNICK,
				form_str(ERR_NOSUCHNICK), parv[1]);
		return 0;
	}

	if(!find_shared_conf(source_p->username, source_p->host,
				source_p->servptr->name, SHARED_GRANT))
	{
		sendto_one(source_p, ":%s NOTICE %s :You don't have an appropriate shared"
			"block to grant privilege on this server.", me.name, source_p->name);
		return 0;
	}

	do_grant(source_p, target_p, parv[2]);

	return 0;
}


static int do_grant(struct Client *source_p, struct Client *target_p, const char *new_privset)
{
	int dooper = 0, dodeoper = 0;
	struct PrivilegeSet *privset = 0;

	if (!strcmp(new_privset, "deoper"))
	{
		if (!IsOper(target_p))
		{
			sendto_one_notice(source_p, ":You can't deoper someone who isn't an oper.");
			return 0;
		}
		new_privset = "default";
		dodeoper = 1;

		sendto_one_notice(target_p, ":%s is deopering you.", source_p->name);
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "%s is deopering %s.", get_oper_name(source_p), target_p->name);
	}
	else
	{
		if (!(privset = privilegeset_get(new_privset)))
		{
			sendto_one_notice(source_p, ":There is no privilege set named '%s'.", new_privset);
			return 0;
		}

		if (privset == target_p->localClient->privset)
		{
			sendto_one_notice(source_p, ":%s already has privilege set %s.", target_p->name, target_p->localClient->privset->name);
			return 0;
		}
	}

	if (!dodeoper)
	{
		if (!IsOper(target_p))
		{
			sendto_one_notice(target_p, ":%s is opering you with privilege set %s", source_p->name, privset->name);
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "%s is opering %s with privilege set %s", get_oper_name(source_p), target_p->name, privset->name);
			dooper = 1;
		}
		else
		{
			sendto_one_notice(target_p, ":%s is changing your privilege set to %s", source_p->name, privset->name);
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "%s is changing the privilege set of %s to %s", get_oper_name(source_p), target_p->name, privset->name);
		}
	}

	if (dodeoper)
	{
		const char *modeparv[4];
		modeparv[0] = modeparv[1] = target_p->name;
		modeparv[2] = "-o";
		modeparv[3] = NULL;
		user_mode(target_p, target_p, 3, modeparv);
	}

	if (dooper)
	{
		struct oper_conf oper;
		oper.name = "<grant>";
		oper.umodes = 0;
		oper.snomask = 0;
		oper.privset = privset;

		oper_up(target_p, &oper);
	}

	target_p->localClient->privset = privset;
	const char *modeparv[4];
	modeparv[0] = modeparv[1] = target_p->name;
	modeparv[2] = "+";
	modeparv[3] = NULL;
	user_mode(target_p, target_p, 3, modeparv);

	return 0;
}
