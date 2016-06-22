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
#include "capability.h"

#include <ltdl.h>

#ifndef LT_MODULE_EXT
#	error "Charybdis requires loadable module support."
#endif

rb_dlink_list module_list;
rb_dlink_list mod_paths;

static const char *core_module_table[] = {
	"m_ban",
	"m_die",
	"m_error",
	"m_join",
	"m_kick",
	"m_kill",
	"m_message",
	"m_mode",
	"m_modules",
	"m_nick",
	"m_part",
	"m_quit",
	"m_server",
	"m_squit",
	NULL
};

#define MOD_WARN_DELTA (90 * 86400)	/* time in seconds, 86400 seconds in a day */

void
init_modules(void)
{
	if(lt_dlinit())
	{
		ilog(L_MAIN, "lt_dlinit failed");
		exit(EXIT_FAILURE);
	}

	/* Add the default paths we look in to the module system --nenolod */
	mod_add_path(ircd_paths[IRCD_PATH_MODULES]);
	mod_add_path(ircd_paths[IRCD_PATH_AUTOLOAD_MODULES]);
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
 * input        - module to load
 * output       - index of module on success, -1 on failure
 * side effects - none
 */
struct module *
findmodule_byname(const char *name)
{
	rb_dlink_node *ptr;
	char name_ext[PATH_MAX + 1];

	rb_strlcpy(name_ext, name, sizeof name_ext);
	rb_strlcat(name_ext, LT_MODULE_EXT, sizeof name_ext);

	RB_DLINK_FOREACH(ptr, module_list.head)
	{
		struct module *mod = ptr->data;

		if(!irccmp(mod->name, name))
			return mod;

		if(!irccmp(mod->name, name_ext))
			return mod;
	}

	return NULL;
}

/* load_all_modules()
 *
 * input        -
 * output       -
 * side effects -
 */
void
load_all_modules(bool warn)
{
	DIR *system_module_dir = NULL;
	struct dirent *ldirent = NULL;
	char module_fq_name[PATH_MAX + 1];
	size_t module_ext_len = strlen(LT_MODULE_EXT);

	system_module_dir = opendir(ircd_paths[IRCD_PATH_AUTOLOAD_MODULES]);

	if(system_module_dir == NULL)
	{
		ilog(L_MAIN, "Could not load modules from %s: %s", ircd_paths[IRCD_PATH_AUTOLOAD_MODULES], strerror(errno));
		return;
	}

	while ((ldirent = readdir(system_module_dir)) != NULL)
	{
		size_t len = strlen(ldirent->d_name);

		if(len > module_ext_len &&
			rb_strncasecmp(ldirent->d_name + (len - module_ext_len), LT_MODULE_EXT, module_ext_len) == 0)
		{
			(void) snprintf(module_fq_name, sizeof(module_fq_name), "%s%c%s",
					ircd_paths[IRCD_PATH_AUTOLOAD_MODULES], RB_PATH_SEPARATOR, ldirent->d_name);
			(void) load_a_module(module_fq_name, warn, MAPI_ORIGIN_CORE, false);
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
load_core_modules(bool warn)
{
	char module_name[PATH_MAX];
	int i;


	for (i = 0; core_module_table[i]; i++)
	{
		snprintf(module_name, sizeof(module_name), "%s%c%s", ircd_paths[IRCD_PATH_MODULES], RB_PATH_SEPARATOR,
			    core_module_table[i]);

		if(load_a_module(module_name, warn, MAPI_ORIGIN_CORE, true) == false)
		{
			ilog(L_MAIN,
			     "Error loading core module %s: terminating ircd",
			     core_module_table[i]);
			exit(EXIT_FAILURE);
		}
	}
}

/* load_one_module()
 *
 * input        -
 * output       -
 * side effects -
 */
bool
load_one_module(const char *path, int origin, bool coremodule)
{
	char modpath[PATH_MAX];
	rb_dlink_node *pathst;

	if (server_state_foreground)
		inotice("loading module %s ...", path);

	if(coremodule)
		origin = MAPI_ORIGIN_CORE;

	RB_DLINK_FOREACH(pathst, mod_paths.head)
	{
		struct stat statbuf;
		const char *mpath = pathst->data;

		snprintf(modpath, sizeof(modpath), "%s%c%s%s", mpath, RB_PATH_SEPARATOR, path, LT_MODULE_EXT);
		if((strstr(modpath, "../") == NULL) && (strstr(modpath, "/..") == NULL))
		{
			if(stat(modpath, &statbuf) == 0 && S_ISREG(statbuf.st_mode))
			{
				/* Regular files only please */
				return load_a_module(modpath, true, origin, coremodule);
			}

		}
	}

	sendto_realops_snomask(SNO_GENERAL, L_ALL, "Cannot locate module %s", path);
	return false;
}

static char unknown_ver[] = "<unknown>";
static char unknown_description[] = "<none>";

/* unload_one_module()
 *
 * inputs	- name of module to unload
 *		- true to say modules unloaded, false to not
 * output	- true if successful, false if error
 * side effects	- module is unloaded
 */
bool
unload_one_module(const char *name, bool warn)
{
	struct module *mod;

	if((mod = findmodule_byname(name)) == NULL)
		return false;

	if(mod->core)
		return false;

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
	switch (mod->mapi_version)
	{
	case 1:
		{
			struct mapi_mheader_av1 *mheader = mod->mapi_header;
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
	case 2:
		{
			struct mapi_mheader_av2 *mheader = mod->mapi_header;

			/* XXX duplicate code :( */
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

			if(mheader->mapi_cap_list)
			{
				mapi_cap_list_av2 *m;
				for (m = mheader->mapi_cap_list; m->cap_name; ++m)
				{
					struct CapabilityIndex *idx;

					switch (m->cap_index)
					{
					case MAPI_CAP_CLIENT:
						idx = cli_capindex;
						break;
					case MAPI_CAP_SERVER:
						idx = serv_capindex;
						break;
					default:
						sendto_realops_snomask(SNO_GENERAL, L_ALL,
							"Unknown/unsupported CAP index found of type %d on capability %s when unloading %s",
							m->cap_index, m->cap_name, mod->name);
						ilog(L_MAIN,
							"Unknown/unsupported CAP index found of type %d on capability %s when unloading %s",
							m->cap_index, m->cap_name, mod->name);
						continue;
					}

					if (m->cap_id != NULL)
					{
						capability_orphan(idx, m->cap_name);
						sendto_local_clients_with_capability(CLICAP_CAP_NOTIFY, ":%s CAP * DEL :%s", me.name, m->cap_name);
					}
				}
			}
			break;
		}
	default:
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "Unknown/unsupported MAPI version %d when unloading %s!",
				     mod->mapi_version, mod->name);
		ilog(L_MAIN, "Unknown/unsupported MAPI version %d when unloading %s!",
		     mod->mapi_version, mod->name);
		break;
	}

	lt_dlclose(mod->address);

	rb_dlinkDelete(&mod->node, &module_list);
	rb_free(mod->name);
	rb_free(mod);

	if(warn)
	{
		ilog(L_MAIN, "Module %s unloaded", name);
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "Module %s unloaded", name);
	}

	return true;
}

/*
 * load_a_module()
 *
 * inputs	- path name of module, bool to notice, int of origin, bool if core
 * output	- false if error true if success
 * side effects - loads a module if successful
 */
bool
load_a_module(const char *path, bool warn, int origin, bool core)
{
	struct module *mod;
	lt_dlhandle tmpptr;
	char *mod_displayname, *c;
	const char *ver, *description = NULL;
	size_t module_ext_len = strlen(LT_MODULE_EXT);

	int *mapi_version;

	mod_displayname = rb_basename(path);

	/* Trim off the ending for the display name if we have to */
	if((c = rb_strcasestr(mod_displayname, LT_MODULE_EXT)) != NULL)
		*c = '\0';

	tmpptr = lt_dlopenext(path);

	if(tmpptr == NULL)
	{
		const char *err = lt_dlerror();

		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "Error loading module %s: %s", mod_displayname, err);
		ilog(L_MAIN, "Error loading module %s: %s", mod_displayname, err);
		rb_free(mod_displayname);
		return false;
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
				     mod_displayname);
		ilog(L_MAIN, "Data format error: module %s has no MAPI header.", mod_displayname);
		(void) lt_dlclose(tmpptr);
		rb_free(mod_displayname);
		return false;
	}

	switch (MAPI_VERSION(*mapi_version))
	{
	case 1:
		{
			struct mapi_mheader_av1 *mheader = (struct mapi_mheader_av1 *)(void *)mapi_version;	/* see above */
			if(mheader->mapi_register && (mheader->mapi_register() == -1))
			{
				ilog(L_MAIN, "Module %s indicated failure during load.",
				     mod_displayname);
				sendto_realops_snomask(SNO_GENERAL, L_ALL,
						     "Module %s indicated failure during load.",
						     mod_displayname);
				lt_dlclose(tmpptr);
				rb_free(mod_displayname);
				return false;
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
	case 2:
		{
			struct mapi_mheader_av2 *mheader = (struct mapi_mheader_av2 *)(void *)mapi_version;     /* see above */

			/* XXX duplicated code :( */
			if(mheader->mapi_register && (mheader->mapi_register() == -1))
			{
				ilog(L_MAIN, "Module %s indicated failure during load.",
					mod_displayname);
				sendto_realops_snomask(SNO_GENERAL, L_ALL,
						     "Module %s indicated failure during load.",
						     mod_displayname);
				lt_dlclose(tmpptr);
				rb_free(mod_displayname);
				return false;
			}

			/* Basic date code checks
			 *
			 * Don't make them fatal, but do complain about differences within a certain time frame.
			 * Later on if there are major API changes we can add fatal checks.
			 * -- Elizafox
			 */
			if(mheader->mapi_datecode != datecode && mheader->mapi_datecode > 0)
			{
				long int delta = datecode - mheader->mapi_datecode;
				if (delta > MOD_WARN_DELTA)
				{
					delta /= 86400;
					iwarn("Module %s build date is out of sync with ircd build date by %ld days, expect problems",
						mod_displayname, delta);
					sendto_realops_snomask(SNO_GENERAL, L_ALL,
						"Module %s build date is out of sync with ircd build date by %ld days, expect problems",
						mod_displayname, delta);
				}
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

			/* New in MAPI v2 - version replacement */
			ver = mheader->mapi_module_version ? mheader->mapi_module_version : ircd_version;
			description = mheader->mapi_module_description;

			if(mheader->mapi_cap_list)
			{
				mapi_cap_list_av2 *m;
				for (m = mheader->mapi_cap_list; m->cap_name; ++m)
				{
					struct CapabilityIndex *idx;
					int result;

					switch (m->cap_index)
					{
					case MAPI_CAP_CLIENT:
						idx = cli_capindex;
						break;
					case MAPI_CAP_SERVER:
						idx = serv_capindex;
						break;
					default:
						sendto_realops_snomask(SNO_GENERAL, L_ALL,
							"Unknown/unsupported CAP index found of type %d on capability %s when loading %s",
							m->cap_index, m->cap_name, mod_displayname);
						ilog(L_MAIN,
							"Unknown/unsupported CAP index found of type %d on capability %s when loading %s",
							m->cap_index, m->cap_name, mod_displayname);
						continue;
					}

					result = capability_put(idx, m->cap_name, m->cap_ownerdata);
					if (m->cap_id != NULL)
					{
						*(m->cap_id) = result;
						sendto_local_clients_with_capability(CLICAP_CAP_NOTIFY, ":%s CAP * ADD :%s", me.name, m->cap_name);
					}
				}
			}
		}

		break;
	default:
		ilog(L_MAIN, "Module %s has unknown/unsupported MAPI version %d.",
		     mod_displayname, MAPI_VERSION(*mapi_version));
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "Module %s has unknown/unsupported MAPI version %d.",
				     mod_displayname, *mapi_version);
		lt_dlclose(tmpptr);
		rb_free(mod_displayname);
		return false;
	}

	if(ver == NULL)
		ver = unknown_ver;

	if(description == NULL)
		description = unknown_description;

	mod = rb_malloc(sizeof(struct module));
	mod->address = tmpptr;
	mod->version = ver;
	mod->description = description;
	mod->core = core;
	mod->name = rb_strdup(mod_displayname);
	mod->mapi_header = mapi_version;
	mod->mapi_version = MAPI_VERSION(*mapi_version);
	mod->origin = origin;
	rb_dlinkAdd(mod, &mod->node, &module_list);

	if(warn)
	{
		const char *o;

		switch (origin)
		{
		case MAPI_ORIGIN_EXTENSION:
			o = "extension";
			break;
		case MAPI_ORIGIN_CORE:
			o = "core";
			break;
		default:
			o = "unknown";
			break;
		}

		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "Module %s [version: %s; MAPI version: %d; origin: %s; description: \"%s\"] loaded at %p",
				     mod_displayname, ver, MAPI_VERSION(*mapi_version), o, description,
				     (void *) tmpptr);
		ilog(L_MAIN, "Module %s [version: %s; MAPI version: %d; origin: %s; description: \"%s\"] loaded at %p",
		     mod_displayname, ver, MAPI_VERSION(*mapi_version), o, description, (void *) tmpptr);
	}
	rb_free(mod_displayname);
	return true;
}
