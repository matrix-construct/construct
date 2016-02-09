/*
 *  ircd-ratbox: A slightly useful ircd.
 *  modules.c: A module loader.
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
 *  $Id: modules.c 3161 2007-01-25 07:23:01Z nenolod $
 */

#include "stdinc.h"
#include "modules.h"
#include "logger.h"
#include "ircd.h"
#include "client.h"
#include "send.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "numeric.h"
#include "parse.h"
#include "ircd_defs.h"
#include "match.h"
#include "s_serv.h"

#include <ltdl.h>

struct module **modlist = NULL;

static const char *core_module_table[] = {
	"m_ban",
	"m_die",
	"m_error",
	"m_join",
	"m_kick",
	"m_kill",
	"m_message",
	"m_mode",
	"m_nick",
	"m_part",
	"m_quit",
	"m_server",
	"m_squit",
	NULL
};

#define MODS_INCREMENT 10
int num_mods = 0;
int max_mods = MODS_INCREMENT;

static rb_dlink_list mod_paths;

static int mo_modload(struct Client *, struct Client *, int, const char **);
static int mo_modlist(struct Client *, struct Client *, int, const char **);
static int mo_modreload(struct Client *, struct Client *, int, const char **);
static int mo_modunload(struct Client *, struct Client *, int, const char **);
static int mo_modrestart(struct Client *, struct Client *, int, const char **);

static int me_modload(struct Client *, struct Client *, int, const char **);
static int me_modlist(struct Client *, struct Client *, int, const char **);
static int me_modreload(struct Client *, struct Client *, int, const char **);
static int me_modunload(struct Client *, struct Client *, int, const char **);
static int me_modrestart(struct Client *, struct Client *, int, const char **);

static int do_modload(struct Client *, const char *);
static int do_modunload(struct Client *, const char *);
static int do_modreload(struct Client *, const char *);
static int do_modlist(struct Client *, const char *);
static int do_modrestart(struct Client *);

struct Message modload_msgtab = {
	"MODLOAD", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_modload, 2}, {mo_modload, 2}}
};

struct Message modunload_msgtab = {
	"MODUNLOAD", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_modunload, 2}, {mo_modunload, 2}}
};

struct Message modreload_msgtab = {
	"MODRELOAD", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_modreload, 2}, {mo_modreload, 2}}
};

struct Message modlist_msgtab = {
	"MODLIST", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_modlist, 0}, {mo_modlist, 0}}
};

struct Message modrestart_msgtab = {
	"MODRESTART", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_modrestart, 0}, {mo_modrestart, 0}}
};

void
modules_init(void)
{
	if(lt_dlinit())
	{
		ilog(L_MAIN, "lt_dlinit failed");
		exit(0);
	}

	mod_add_cmd(&modload_msgtab);
	mod_add_cmd(&modunload_msgtab);
	mod_add_cmd(&modreload_msgtab);
	mod_add_cmd(&modlist_msgtab);
	mod_add_cmd(&modrestart_msgtab);

	/* Add the default paths we look in to the module system --nenolod */
	mod_add_path(MODPATH);
	mod_add_path(AUTOMODPATH);
}

/* mod_find_path()
 *
 * input	- path
 * output	- none
 * side effects - returns a module path from path
 */
static char *
mod_find_path(const char *path)
{
	rb_dlink_node *ptr;
	char *mpath;

	RB_DLINK_FOREACH(ptr, mod_paths.head)
	{
		mpath = ptr->data;

		if(!strcmp(path, mpath))
			return mpath;
	}

	return NULL;
}

/* mod_add_path
 *
 * input	- path
 * ouput	-
 * side effects - adds path to list
 */
void
mod_add_path(const char *path)
{
	char *pathst;

	if(mod_find_path(path))
		return;

	pathst = rb_strdup(path);
	rb_dlinkAddAlloc(pathst, &mod_paths);
}

/* mod_clear_paths()
 *
 * input	-
 * output	-
 * side effects - clear the lists of paths
 */
void
mod_clear_paths(void)
{
	rb_dlink_node *ptr, *next_ptr;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, mod_paths.head)
	{
		rb_free(ptr->data);
		rb_free_rb_dlink_node(ptr);
	}

	mod_paths.head = mod_paths.tail = NULL;
	mod_paths.length = 0;
}

/* findmodule_byname
 *
 * input        -
 * output       -
 * side effects -
 */

int
findmodule_byname(const char *name)
{
	int i;

	for (i = 0; i < num_mods; i++)
	{
		if(!irccmp(modlist[i]->name, name))
			return i;
	}
	return -1;
}

/* load_all_modules()
 *
 * input        -
 * output       -
 * side effects -
 */
void
load_all_modules(int warn)
{
	DIR *system_module_dir = NULL;
	struct dirent *ldirent = NULL;
	char module_fq_name[PATH_MAX + 1];
	int len;

	modules_init();

	modlist = (struct module **) rb_malloc(sizeof(struct module *) * (MODS_INCREMENT));

	max_mods = MODS_INCREMENT;

	system_module_dir = opendir(AUTOMODPATH);

	if(system_module_dir == NULL)
	{
		ilog(L_MAIN, "Could not load modules from %s: %s", AUTOMODPATH, strerror(errno));
		return;
	}

	while ((ldirent = readdir(system_module_dir)) != NULL)
	{
        	len = strlen(ldirent->d_name);
		if((len > 3) && !strcmp(ldirent->d_name+len-3, ".la"))
                {
		 	(void) rb_snprintf(module_fq_name, sizeof(module_fq_name), "%s/%s", AUTOMODPATH, ldirent->d_name);
		 	(void) load_a_module(module_fq_name, warn, 0);
                }

	}
	(void) closedir(system_module_dir);
}

/* load_core_modules()
 *
 * input        -
 * output       -
 * side effects - core modules are loaded, if any fail, kill ircd
 */
void
load_core_modules(int warn)
{
	char module_name[PATH_MAX];
	int i;


	for (i = 0; core_module_table[i]; i++)
	{
		rb_snprintf(module_name, sizeof(module_name), "%s/%s%s", MODPATH,
			    core_module_table[i], ".la");

		if(load_a_module(module_name, warn, 1) == -1)
		{
			ilog(L_MAIN,
			     "Error loading core module %s%s: terminating ircd",
			     core_module_table[i], ".la");
			exit(0);
		}
	}
}

/* load_one_module()
 *
 * input        -
 * output       -
 * side effects -
 */
int
load_one_module(const char *path, int coremodule)
{
	char modpath[PATH_MAX];
	rb_dlink_node *pathst;
	const char *mpath;

	struct stat statbuf;

	if (server_state_foreground == 1)
		inotice("loading module %s ...", path);

	RB_DLINK_FOREACH(pathst, mod_paths.head)
	{
		mpath = pathst->data;

		rb_snprintf(modpath, sizeof(modpath), "%s/%s", mpath, path);
		if((strstr(modpath, "../") == NULL) && (strstr(modpath, "/..") == NULL))
		{
			if(stat(modpath, &statbuf) == 0)
			{
				if(S_ISREG(statbuf.st_mode))
				{
					/* Regular files only please */
					if(coremodule)
						return load_a_module(modpath, 1, 1);
					else
						return load_a_module(modpath, 1, 0);
				}
			}

		}
	}

	sendto_realops_snomask(SNO_GENERAL, L_ALL, "Cannot locate module %s", path);
	return -1;
}


/* load a module .. */
static int
mo_modload(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "admin");
		return 0;
	}

	if(parc > 2)
	{
		sendto_match_servs(source_p, parv[2], CAP_ENCAP, NOCAPS,
				"ENCAP %s MODLOAD %s", parv[2], parv[1]);
		if (match(parv[2], me.name) == 0)
			return 0;
	}

	return do_modload(source_p, parv[1]);
}

static int
me_modload(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!find_shared_conf(source_p->username, source_p->host, source_p->servptr->name, SHARED_MODULE))
	{
		sendto_one_notice(source_p, ":*** You do not have an appropriate shared block "
				"to load modules on this server.");
		return 0;
	}

	return do_modload(source_p, parv[1]);
}

static int
do_modload(struct Client *source_p, const char *module)
{
	char *m_bn = rb_basename(module);

	if(findmodule_byname(m_bn) != -1)
	{
		sendto_one_notice(source_p, ":Module %s is already loaded", m_bn);
		rb_free(m_bn);
		return 0;
	}

	load_one_module(module, 0);

	rb_free(m_bn);

	return 0;
}


/* unload a module .. */
static int
mo_modunload(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "admin");
		return 0;
	}

	if(parc > 2)
	{
		sendto_match_servs(source_p, parv[2], CAP_ENCAP, NOCAPS,
				"ENCAP %s MODUNLOAD %s", parv[2], parv[1]);
		if (match(parv[2], me.name) == 0)
			return 0;
	}

	return do_modunload(source_p, parv[1]);
}

static int
me_modunload(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!find_shared_conf(source_p->username, source_p->host, source_p->servptr->name, SHARED_MODULE))
	{
		sendto_one_notice(source_p, ":*** You do not have an appropriate shared block "
				"to load modules on this server.");
		return 0;
	}

	return do_modunload(source_p, parv[1]);
}

static int
do_modunload(struct Client *source_p, const char *module)
{
	int modindex;
	char *m_bn = rb_basename(module);

	if((modindex = findmodule_byname(m_bn)) == -1)
	{
		sendto_one_notice(source_p, ":Module %s is not loaded", m_bn);
		rb_free(m_bn);
		return 0;
	}

	if(modlist[modindex]->core == 1)
	{
		sendto_one_notice(source_p, ":Module %s is a core module and may not be unloaded", m_bn);
		rb_free(m_bn);
		return 0;
	}

	if(unload_one_module(m_bn, 1) == -1)
	{
		sendto_one_notice(source_p, ":Module %s is not loaded", m_bn);
	}

	rb_free(m_bn);
	return 0;
}

/* unload and load in one! */
static int
mo_modreload(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "admin");
		return 0;
	}

	if(parc > 2)
	{
		sendto_match_servs(source_p, parv[2], CAP_ENCAP, NOCAPS,
				"ENCAP %s MODRELOAD %s", parv[2], parv[1]);
		if (match(parv[2], me.name) == 0)
			return 0;
	}

	return do_modreload(source_p, parv[1]);
}

static int
me_modreload(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!find_shared_conf(source_p->username, source_p->host, source_p->servptr->name, SHARED_MODULE))
	{
		sendto_one_notice(source_p, ":*** You do not have an appropriate shared block "
				"to load modules on this server.");
		return 0;
	}

	return do_modreload(source_p, parv[1]);
}

static int
do_modreload(struct Client *source_p, const char *module)
{
	int modindex;
	int check_core;
	char *m_bn = rb_basename(module);

	if((modindex = findmodule_byname(m_bn)) == -1)
	{
		sendto_one_notice(source_p, ":Module %s is not loaded", m_bn);
		rb_free(m_bn);
		return 0;
	}

	check_core = modlist[modindex]->core;

	if(unload_one_module(m_bn, 1) == -1)
	{
		sendto_one_notice(source_p, ":Module %s is not loaded", m_bn);
		rb_free(m_bn);
		return 0;
	}

	if((load_one_module(m_bn, check_core) == -1) && check_core)
	{
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
				     "Error reloading core module: %s: terminating ircd", m_bn);
		ilog(L_MAIN, "Error loading core module %s: terminating ircd", m_bn);
		exit(0);
	}

	rb_free(m_bn);
	return 0;
}

/* list modules .. */
static int
mo_modlist(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "admin");
		return 0;
	}

	if(parc > 2)
	{
		sendto_match_servs(source_p, parv[2], CAP_ENCAP, NOCAPS,
				"ENCAP %s MODLIST %s", parv[2], parv[1]);
		if (match(parv[2], me.name) == 0)
			return 0;
	}

	return do_modlist(source_p, parc > 1 ? parv[1] : 0);
}

static int
me_modlist(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!find_shared_conf(source_p->username, source_p->host, source_p->servptr->name, SHARED_MODULE))
	{
		sendto_one_notice(source_p, ":*** You do not have an appropriate shared block "
				"to load modules on this server.");
		return 0;
	}

	return do_modlist(source_p, parv[1]);
}

static int
do_modlist(struct Client *source_p, const char *pattern)
{
	int i;

	for (i = 0; i < num_mods; i++)
	{
		if(pattern)
		{
			if(match(pattern, modlist[i]->name))
			{
				sendto_one(source_p, form_str(RPL_MODLIST),
					   me.name, source_p->name,
					   modlist[i]->name,
					   (unsigned long)(uintptr_t)modlist[i]->address,
					   modlist[i]->version, modlist[i]->core ? "(core)" : "");
			}
		}
		else
		{
			sendto_one(source_p, form_str(RPL_MODLIST),
				   me.name, source_p->name, modlist[i]->name,
				   (unsigned long)(uintptr_t)modlist[i]->address, modlist[i]->version,
				   modlist[i]->core ? "(core)" : "");
		}
	}

	sendto_one(source_p, form_str(RPL_ENDOFMODLIST), me.name, source_p->name);
	return 0;
}

/* unload and reload all modules */
static int
mo_modrestart(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "admin");
		return 0;
	}

	if(parc > 1)
	{
		sendto_match_servs(source_p, parv[1], CAP_ENCAP, NOCAPS,
				"ENCAP %s MODRESTART", parv[1]);
		if (match(parv[1], me.name) == 0)
			return 0;
	}

	return do_modrestart(source_p);
}

static int
me_modrestart(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!find_shared_conf(source_p->username, source_p->host, source_p->servptr->name, SHARED_MODULE))
	{
		sendto_one_notice(source_p, ":*** You do not have an appropriate shared block "
				"to load modules on this server.");
		return 0;
	}

	return do_modrestart(source_p);
}

static int
do_modrestart(struct Client *source_p)
{
	int modnum;

	sendto_one_notice(source_p, ":Reloading all modules");

	modnum = num_mods;
	while (num_mods)
		unload_one_module(modlist[0]->name, 0);

	load_all_modules(0);
	load_core_modules(0);
	rehash(0);

	sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
			     "Module Restart: %d modules unloaded, %d modules loaded",
			     modnum, num_mods);
	ilog(L_MAIN, "Module Restart: %d modules unloaded, %d modules loaded", modnum, num_mods);
	return 0;
}


static void increase_modlist(void);

#define MODS_INCREMENT 10

static char unknown_ver[] = "<unknown>";

/* unload_one_module()
 *
 * inputs	- name of module to unload
 *		- 1 to say modules unloaded, 0 to not
 * output	- 0 if successful, -1 if error
 * side effects	- module is unloaded
 */
int
unload_one_module(const char *name, int warn)
{
	int modindex;

	if((modindex = findmodule_byname(name)) == -1)
		return -1;

	/*
	 ** XXX - The type system in C does not allow direct conversion between
	 ** data and function pointers, but as it happens, most C compilers will
	 ** safely do this, however it is a theoretical overlow to cast as we
	 ** must do here.  I have library functions to take care of this, but
	 ** despite being more "correct" for the C language, this is more
	 ** practical.  Removing the abuse of the ability to cast ANY pointer
	 ** to and from an integer value here will break some compilers.
	 **          -jmallett
	 */
	/* Left the comment in but the code isn't here any more         -larne */
	switch (modlist[modindex]->mapi_version)
	{
	case 1:
		{
			struct mapi_mheader_av1 *mheader = modlist[modindex]->mapi_header;
			if(mheader->mapi_command_list)
			{
				struct Message **m;
				for (m = mheader->mapi_command_list; *m; ++m)
					mod_del_cmd(*m);
			}

			/* hook events are never removed, we simply lose the
			 * ability to call them --fl
			 */
			if(mheader->mapi_hfn_list)
			{
				mapi_hfn_list_av1 *m;
				for (m = mheader->mapi_hfn_list; m->hapi_name; ++m)
					remove_hook(m->hapi_name, m->fn);
			}

			if(mheader->mapi_unregister)
				mheader->mapi_unregister();
			break;
		}
	default:
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "Unknown/unsupported MAPI version %d when unloading %s!",
				     modlist[modindex]->mapi_version, modlist[modindex]->name);
		ilog(L_MAIN, "Unknown/unsupported MAPI version %d when unloading %s!",
		     modlist[modindex]->mapi_version, modlist[modindex]->name);
		break;
	}

	lt_dlclose(modlist[modindex]->address);

	rb_free(modlist[modindex]->name);
	rb_free(modlist[modindex]);
	memmove(&modlist[modindex], &modlist[modindex + 1],
	       sizeof(struct module *) * ((num_mods - 1) - modindex));

	if(num_mods != 0)
		num_mods--;

	if(warn == 1)
	{
		ilog(L_MAIN, "Module %s unloaded", name);
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "Module %s unloaded", name);
	}

	return 0;
}


/*
 * load_a_module()
 *
 * inputs	- path name of module, int to notice, int of core
 * output	- -1 if error 0 if success
 * side effects - loads a module if successful
 */
int
load_a_module(const char *path, int warn, int core)
{
	lt_dlhandle tmpptr;
	char *mod_basename;
	const char *ver;

	int *mapi_version;

	mod_basename = rb_basename(path);

	tmpptr = lt_dlopen(path);

	if(tmpptr == NULL)
	{
		const char *err = lt_dlerror();

		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "Error loading module %s: %s", mod_basename, err);
		ilog(L_MAIN, "Error loading module %s: %s", mod_basename, err);
		rb_free(mod_basename);
		return -1;
	}


	/*
	 * _mheader is actually a struct mapi_mheader_*, but mapi_version
	 * is always the first member of this structure, so we treate it
	 * as a single int in order to determine the API version.
	 *      -larne.
	 */
	mapi_version = (int *) (uintptr_t) lt_dlsym(tmpptr, "_mheader");
	if((mapi_version == NULL
	    && (mapi_version = (int *) (uintptr_t) lt_dlsym(tmpptr, "__mheader")) == NULL)
	   || MAPI_MAGIC(*mapi_version) != MAPI_MAGIC_HDR)
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "Data format error: module %s has no MAPI header.",
				     mod_basename);
		ilog(L_MAIN, "Data format error: module %s has no MAPI header.", mod_basename);
		(void) lt_dlclose(tmpptr);
		rb_free(mod_basename);
		return -1;
	}

	switch (MAPI_VERSION(*mapi_version))
	{
	case 1:
		{
			struct mapi_mheader_av1 *mheader = (struct mapi_mheader_av1 *)(void *)mapi_version;	/* see above */
			if(mheader->mapi_register && (mheader->mapi_register() == -1))
			{
				ilog(L_MAIN, "Module %s indicated failure during load.",
				     mod_basename);
				sendto_realops_snomask(SNO_GENERAL, L_ALL,
						     "Module %s indicated failure during load.",
						     mod_basename);
				lt_dlclose(tmpptr);
				rb_free(mod_basename);
				return -1;
			}
			if(mheader->mapi_command_list)
			{
				struct Message **m;
				for (m = mheader->mapi_command_list; *m; ++m)
					mod_add_cmd(*m);
			}

			if(mheader->mapi_hook_list)
			{
				mapi_hlist_av1 *m;
				for (m = mheader->mapi_hook_list; m->hapi_name; ++m)
					*m->hapi_id = register_hook(m->hapi_name);
			}

			if(mheader->mapi_hfn_list)
			{
				mapi_hfn_list_av1 *m;
				for (m = mheader->mapi_hfn_list; m->hapi_name; ++m)
					add_hook(m->hapi_name, m->fn);
			}

			ver = mheader->mapi_module_version;
			break;
		}

	default:
		ilog(L_MAIN, "Module %s has unknown/unsupported MAPI version %d.",
		     mod_basename, MAPI_VERSION(*mapi_version));
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "Module %s has unknown/unsupported MAPI version %d.",
				     mod_basename, *mapi_version);
		lt_dlclose(tmpptr);
		rb_free(mod_basename);
		return -1;
	}

	if(ver == NULL)
		ver = unknown_ver;

	increase_modlist();

	modlist[num_mods] = rb_malloc(sizeof(struct module));
	modlist[num_mods]->address = tmpptr;
	modlist[num_mods]->version = ver;
	modlist[num_mods]->core = core;
	modlist[num_mods]->name = rb_strdup(mod_basename);
	modlist[num_mods]->mapi_header = mapi_version;
	modlist[num_mods]->mapi_version = MAPI_VERSION(*mapi_version);
	num_mods++;

	if(warn == 1)
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "Module %s [version: %s; MAPI version: %d] loaded at 0x%lx",
				     mod_basename, ver, MAPI_VERSION(*mapi_version),
				     (unsigned long) tmpptr);
		ilog(L_MAIN, "Module %s [version: %s; MAPI version: %d] loaded at 0x%lx",
		     mod_basename, ver, MAPI_VERSION(*mapi_version), (unsigned long) tmpptr);
	}
	rb_free(mod_basename);
	return 0;
}

/*
 * increase_modlist
 *
 * inputs	- NONE
 * output	- NONE
 * side effects	- expand the size of modlist if necessary
 */
static void
increase_modlist(void)
{
	struct module **new_modlist = NULL;

	if((num_mods + 1) < max_mods)
		return;

	new_modlist = (struct module **) rb_malloc(sizeof(struct module *) *
						  (max_mods + MODS_INCREMENT));
	memcpy((void *) new_modlist, (void *) modlist, sizeof(struct module *) * num_mods);

	rb_free(modlist);
	modlist = new_modlist;
	max_mods += MODS_INCREMENT;
}
