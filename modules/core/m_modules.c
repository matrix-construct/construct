/* modules/m_modules.c - module for module loading
 * Copyright (c) 2016 Elizabeth Myers <elizabeth@interlinked.me>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
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

#include "stdinc.h"
#include "client.h"
#include "parse.h"
#include "msg.h"
#include "modules.h"
#include "s_newconf.h"
#include "s_conf.h"
#include "s_serv.h"
#include "hash.h"
#include "ircd.h"
#include "match.h"
#include "numeric.h"
#include "send.h"
#include "packet.h"
#include "logger.h"

static const char modules_desc[] = "Provides module management commands";

static void mo_modload(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void mo_modlist(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void mo_modreload(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void mo_modunload(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void mo_modrestart(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

static void me_modload(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void me_modlist(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void me_modreload(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void me_modunload(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void me_modrestart(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

static void do_modload(struct Client *, const char *);
static void do_modunload(struct Client *, const char *);
static void do_modreload(struct Client *, const char *);
static void do_modlist(struct Client *, const char *);
static void do_modrestart(struct Client *);

struct Message modload_msgtab = {
	"MODLOAD", 0, 0, 0, 0,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_modload, 2}, {mo_modload, 2}}
};

struct Message modunload_msgtab = {
	"MODUNLOAD", 0, 0, 0, 0,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_modunload, 2}, {mo_modunload, 2}}
};

struct Message modreload_msgtab = {
	"MODRELOAD", 0, 0, 0, 0,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_modreload, 2}, {mo_modreload, 2}}
};

struct Message modlist_msgtab = {
	"MODLIST", 0, 0, 0, 0,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_modlist, 0}, {mo_modlist, 0}}
};

struct Message modrestart_msgtab = {
	"MODRESTART", 0, 0, 0, 0,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_modrestart, 0}, {mo_modrestart, 0}}
};

mapi_clist_av1 modules_clist[] = { &modload_msgtab, &modunload_msgtab, &modreload_msgtab, &modlist_msgtab, &modrestart_msgtab, NULL };

DECLARE_MODULE_AV2(modules, NULL, NULL, modules_clist, NULL, NULL, NULL, NULL, modules_desc);

/* load a module .. */
static void
mo_modload(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "admin");
		return;
	}

	if(parc > 2)
	{
		sendto_match_servs(source_p, parv[2], CAP_ENCAP, NOCAPS,
				"ENCAP %s MODLOAD %s", parv[2], parv[1]);
		if(match(parv[2], me.name) == 0)
			return;
	}

	do_modload(source_p, parv[1]);
}

static void
me_modload(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!find_shared_conf(source_p->username, source_p->host, source_p->servptr->name, SHARED_MODULE))
	{
		sendto_one_notice(source_p, ":*** You do not have an appropriate shared block "
				"to load modules on this server.");
		return;
	}

	do_modload(source_p, parv[1]);
}


/* unload a module .. */
static void
mo_modunload(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "admin");
		return;
	}

	if(parc > 2)
	{
		sendto_match_servs(source_p, parv[2], CAP_ENCAP, NOCAPS,
				"ENCAP %s MODUNLOAD %s", parv[2], parv[1]);
		if(match(parv[2], me.name) == 0)
			return;
	}

	do_modunload(source_p, parv[1]);
}

static void
me_modunload(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!find_shared_conf(source_p->username, source_p->host, source_p->servptr->name, SHARED_MODULE))
	{
		sendto_one_notice(source_p, ":*** You do not have an appropriate shared block "
				"to load modules on this server.");
		return;
	}

	do_modunload(source_p, parv[1]);
}

/* unload and load in one! */
static void
mo_modreload(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "admin");
		return;
	}

	if(parc > 2)
	{
		sendto_match_servs(source_p, parv[2], CAP_ENCAP, NOCAPS,
				"ENCAP %s MODRELOAD %s", parv[2], parv[1]);
		if(match(parv[2], me.name) == 0)
			return;
	}

	do_modreload(source_p, parv[1]);
}

static void
me_modreload(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!find_shared_conf(source_p->username, source_p->host, source_p->servptr->name, SHARED_MODULE))
	{
		sendto_one_notice(source_p, ":*** You do not have an appropriate shared block "
				"to load modules on this server.");
		return;
	}

	do_modreload(source_p, parv[1]);
}

/* list modules .. */
static void
mo_modlist(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "admin");
		return;
	}

	if(parc > 2)
	{
		sendto_match_servs(source_p, parv[2], CAP_ENCAP, NOCAPS,
				"ENCAP %s MODLIST %s", parv[2], parv[1]);
		if(match(parv[2], me.name) == 0)
			return;
	}

	do_modlist(source_p, parc > 1 ? parv[1] : 0);
}

static void
me_modlist(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!find_shared_conf(source_p->username, source_p->host, source_p->servptr->name, SHARED_MODULE))
	{
		sendto_one_notice(source_p, ":*** You do not have an appropriate shared block "
				"to load modules on this server.");
		return;
	}

	do_modlist(source_p, parv[1]);
}

/* unload and reload all modules */
static void
mo_modrestart(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "admin");
		return;
	}

	if(parc > 1)
	{
		sendto_match_servs(source_p, parv[1], CAP_ENCAP, NOCAPS,
				"ENCAP %s MODRESTART", parv[1]);
		if(match(parv[1], me.name) == 0)
			return;
	}

	do_modrestart(source_p);
}

static void
me_modrestart(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!find_shared_conf(source_p->username, source_p->host, source_p->servptr->name, SHARED_MODULE))
	{
		sendto_one_notice(source_p, ":*** You do not have an appropriate shared block "
				"to load modules on this server.");
		return;
	}

	do_modrestart(source_p);
}

static void
do_modload(struct Client *source_p, const char *module)
{
	char *m_bn = rb_basename(module);
	int origin;

	if(findmodule_byname(m_bn) != NULL)
	{
		sendto_one_notice(source_p, ":Module %s is already loaded", m_bn);
		rb_free(m_bn);
		return;
	}

	origin = strcmp(module, m_bn) == 0 ? MAPI_ORIGIN_CORE : MAPI_ORIGIN_EXTENSION;
	load_one_module(module, origin, false);

	rb_free(m_bn);
}

static void
do_modunload(struct Client *source_p, const char *module)
{
	struct module *mod;
	char *m_bn = rb_basename(module);

	if((mod = findmodule_byname(m_bn)) == NULL)
	{
		sendto_one_notice(source_p, ":Module %s is not loaded", m_bn);
		rb_free(m_bn);
		return;
	}

	if(mod->core)
	{
		sendto_one_notice(source_p, ":Module %s is a core module and may not be unloaded", m_bn);
		rb_free(m_bn);
		return;
	}

	if(unload_one_module(m_bn, true) == false)
		sendto_one_notice(source_p, ":Module %s is not loaded", m_bn);

	rb_free(m_bn);
}

static void
do_modreload(struct Client *source_p, const char *module)
{
	struct module *mod;
	int check_core;
	char *m_bn = rb_basename(module);

	if((mod = findmodule_byname(m_bn)) == NULL)
	{
		sendto_one_notice(source_p, ":Module %s is not loaded", m_bn);
		rb_free(m_bn);
		return;
	}

	check_core = mod->core;

	if(unload_one_module(m_bn, true) == false)
	{
		sendto_one_notice(source_p, ":Module %s is not loaded", m_bn);
		rb_free(m_bn);
		return;
	}

	if((load_one_module(m_bn, mod->origin, check_core) == false) && check_core)
	{
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
				     "Error reloading core module: %s: terminating ircd", m_bn);
		ilog(L_MAIN, "Error loading core module %s: terminating ircd", m_bn);
		exit(0);
	}

	rb_free(m_bn);
}

static void
do_modrestart(struct Client *source_p)
{
	unsigned int modnum = 0;
	rb_dlink_node *ptr, *nptr;

	sendto_one_notice(source_p, ":Reloading all modules");

	RB_DLINK_FOREACH_SAFE(ptr, nptr, module_list.head)
	{
		struct module *mod = ptr->data;
		unload_one_module(mod->name, false);
		modnum++;
	}

	load_all_modules(false);
	load_core_modules(false);
	rehash(false);

	sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
			     "Module Restart: %u modules unloaded, %lu modules loaded",
			     modnum, rb_dlink_list_length(&module_list));
	ilog(L_MAIN, "Module Restart: %u modules unloaded, %lu modules loaded", modnum, rb_dlink_list_length(&module_list));
}

static void
do_modlist(struct Client *source_p, const char *pattern)
{
	rb_dlink_node *ptr;
	int i;

	RB_DLINK_FOREACH(ptr, module_list.head)
	{
		struct module *mod = ptr->data;
		const char *origin;
		switch (mod->origin)
		{
		case MAPI_ORIGIN_EXTENSION:
			origin = "extension";
			break;
		case MAPI_ORIGIN_CORE:
			origin = "builtin";
			break;
		default:
			origin = "unknown";
			break;
		}

		if(pattern)
		{
			if(match(pattern, mod->name))
			{
				sendto_one(source_p, form_str(RPL_MODLIST),
					   me.name, source_p->name,
					   mod->name,
					   (unsigned long)(uintptr_t)mod->address, origin,
					   mod->core ? " (core)" : "", mod->version, mod->description);
			}
		}
		else
		{
			sendto_one(source_p, form_str(RPL_MODLIST),
				   me.name, source_p->name, mod->name,
				   (unsigned long)(uintptr_t)mod->address, origin,
				   mod->core ? " (core)" : "", mod->version, mod->description);
		}
	}

	sendto_one(source_p, form_str(RPL_ENDOFMODLIST), me.name, source_p->name);
}
