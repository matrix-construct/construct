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

#include <ltdl.h>

namespace ircd {

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
	mod_add_path(fs::path::get(fs::path::MODULES));
	mod_add_path(fs::path::get(fs::path::AUTOLOAD_MODULES));
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
		mpath = (char *)ptr->data;

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
		struct module *mod = (struct module *)ptr->data;

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

	system_module_dir = opendir(fs::path::get(fs::path::AUTOLOAD_MODULES));

	if(system_module_dir == NULL)
	{
		ilog(L_MAIN, "Could not load modules from %s: %s", fs::path::get(fs::path::AUTOLOAD_MODULES), strerror(errno));
		return;
	}

	while ((ldirent = readdir(system_module_dir)) != NULL)
	{
		size_t len = strlen(ldirent->d_name);

		if(len > module_ext_len &&
			rb_strncasecmp(ldirent->d_name + (len - module_ext_len), LT_MODULE_EXT, module_ext_len) == 0)
		{
			(void) snprintf(module_fq_name, sizeof(module_fq_name), "%s%c%s",
					fs::path::get(fs::path::AUTOLOAD_MODULES), RB_PATH_SEPARATOR, ldirent->d_name);
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
		snprintf(module_name, sizeof(module_name), "%s%c%s", fs::path::get(fs::path::MODULES), RB_PATH_SEPARATOR,
			    core_module_table[i]);

		if(load_a_module(module_name, warn, MAPI_ORIGIN_CORE, true) == false)
		{
			ilog(L_MAIN,
			     "Error loading core module %s: terminating ircd",
			     core_module_table[i]);

			fprintf(stderr,
			     "Error loading core module %s (%s): terminating ircd\n",
			     core_module_table[i],
			     module_name);

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
		const char *mpath = (const char *)pathst->data;

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


void module_log(struct module *const mod,
                const char *const fmt,
                ...)
{
	va_list ap;
	va_start(ap, fmt);

	char buf[BUFSIZE];
	vsnprintf(buf, sizeof(buf), fmt, ap),
	slog(L_MAIN, SNO_GENERAL, "Module %s: %s", mod->name, buf);

	va_end(ap);
}



// ******************************************************************************
// INTERNAL API STACK
// driven by load_a_module() / unload_one_module() (bottom)

static
bool init_module_v1(struct module *const mod)
{
	struct mapi_mheader_av1 *mheader = (struct mapi_mheader_av1 *)mod->mapi_header;
	if(mheader->mapi_register && (mheader->mapi_register() == -1))
		return false;

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

	mod->version = mheader->mapi_module_version;
	return true;
}


static
void fini_module_v1(struct module *const mod)
{
	struct mapi_mheader_av1 *mheader = (mapi_mheader_av1 *)mod->mapi_header;
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
}


static
bool init_module__cap(struct module *const mod,
                      mapi_cap_list_av2 *const m)
{
	using capability::index;

	index *idx;
	switch(m->cap_index)
	{
		case MAPI_CAP_CLIENT:  idx = &cli_capindex;    break;
		case MAPI_CAP_SERVER:  idx = &serv_capindex;   break;
		default:
			slog(L_MAIN, SNO_GENERAL,
			     "Unknown/unsupported CAP index found of type %d on capability %s when loading %s",
			     m->cap_index,
			     m->cap_name,
			     mod->name);
			return false;
	}

	if(m->cap_id)
	{
		*(m->cap_id) = idx->put(m->cap_name, m->cap_ownerdata);
		sendto_local_clients_with_capability(CLICAP_CAP_NOTIFY, ":%s CAP * ADD :%s", me.name, m->cap_name);
	}

	return true;
}


static
void fini_module__cap(struct module *const mod,
                      mapi_cap_list_av2 *const m)
{
	using capability::index;

	index *idx;
	switch(m->cap_index)
	{
		case MAPI_CAP_CLIENT:  idx = &cli_capindex;    break;
		case MAPI_CAP_SERVER:  idx = &serv_capindex;   break;
		default:
			slog(L_MAIN, SNO_GENERAL,
			     "Unknown/unsupported CAP index found of type %d on capability %s when unloading %s",
			     m->cap_index,
			     m->cap_name,
			     mod->name);
			return;
	}

	if(m->cap_id)
	{
		idx->orphan(m->cap_name);
		sendto_local_clients_with_capability(CLICAP_CAP_NOTIFY, ":%s CAP * DEL :%s", me.name, m->cap_name);
	}
}


static
bool init_module_v2(struct module *const mod)
{
	struct mapi_mheader_av2 *mheader = (struct mapi_mheader_av2 *)mod->mapi_header;
	if(mheader->mapi_register && (mheader->mapi_register() == -1))
		return false;

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
				mod->name, delta);
			sendto_realops_snomask(SNO_GENERAL, L_ALL,
				"Module %s build date is out of sync with ircd build date by %ld days, expect problems",
				mod->name, delta);
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
	mod->version = mheader->mapi_module_version? mheader->mapi_module_version : ircd_version;
	mod->description = mheader->mapi_module_description;

	if(mheader->mapi_cap_list)
	{
		mapi_cap_list_av2 *m;
		for (m = mheader->mapi_cap_list; m->cap_name; ++m)
			if(!init_module__cap(mod, m))
				return false;
	}

	return true;
}


static
void fini_module_v2(struct module *const mod)
{
	struct mapi_mheader_av2 *mheader = (struct mapi_mheader_av2 *)mod->mapi_header;

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
			fini_module__cap(mod, m);
	}
}


static
bool require_value(struct module *const mod,
                   struct mapi_av3_attr *const attr)
{
	if(!attr->value)
	{
		module_log(mod, "key[%s] requires non-null value", attr->key);
		return false;
	}

	return true;
}

static
bool init_v3_module_attr(struct module *const mod,
                         struct mapi_mheader_av3 *const h,
                         struct mapi_av3_attr *const attr)
{
	if(EmptyString(attr->key))
	{
		module_log(mod, "Skipping a NULL or empty key (ignoring)");
		return true;
	}

	if(strncmp(attr->key, "time", MAPI_V3_KEY_MAXLEN) == 0)
	{
		//TODO: elizafox's warning
		return true;
	}

	if(strncmp(attr->key, "name", MAPI_V3_KEY_MAXLEN) == 0)
	{
		module_log(mod, "Changing the display unsupported (ignoring)");
		return true;
	}

	if(strncmp(attr->key, "mtab", MAPI_V3_KEY_MAXLEN) == 0)
	{
		if(!require_value(mod, attr))
			return false;

		struct Message *const v = (Message *)attr->value;
		mod_add_cmd(v);
		return true;
	}

	if(strncmp(attr->key, "hook", MAPI_V3_KEY_MAXLEN) == 0)
	{
		if(!require_value(mod, attr))
			return false;

		mapi_hlist_av1 *const v = (mapi_hlist_av1 *)attr->value;
		*v->hapi_id = register_hook(v->hapi_name);
		return true;
	}

	if(strncmp(attr->key, "hookfn", MAPI_V3_KEY_MAXLEN) == 0)
	{
		if(!require_value(mod, attr))
			return false;

		mapi_hfn_list_av1 *const v = (mapi_hfn_list_av1 *)attr->value;
		add_hook(v->hapi_name, v->fn);
		return true;
	}

	if(strncmp(attr->key, "cap", MAPI_V3_KEY_MAXLEN) == 0)
	{
		if(!require_value(mod, attr))
			return false;

		mapi_cap_list_av2 *const v = (mapi_cap_list_av2 *)attr->value;
		return init_module__cap(mod, v);
	}

	if(strncmp(attr->key, "description", MAPI_V3_KEY_MAXLEN) == 0)
	{
		mod->description = (const char *)attr->value;
		return true;
	}

	if(strncmp(attr->key, "version", MAPI_V3_KEY_MAXLEN) == 0)
	{
		mod->version = (const char *)attr->value;
		return true;
	}

	if(strncmp(attr->key, "init", MAPI_V3_KEY_MAXLEN) == 0)
		return attr->init();

	// Ignore fini on load
	if(strncmp(attr->key, "fini", MAPI_V3_KEY_MAXLEN) == 0)
		return true;

	// TODO: analysis.
	module_log(mod, "Unknown key [%s]. Host version does not yet support unknown keys.", attr->key);
	return false;
}


static
void fini_v3_module_attr(struct module *const mod,
                         struct mapi_mheader_av3 *const h,
                         struct mapi_av3_attr *const attr)
{
	if(EmptyString(attr->key))
	{
		module_log(mod, "Skipping a NULL or empty key (ignoring)");
		return;
	}

	if(strncmp(attr->key, "mtab", MAPI_V3_KEY_MAXLEN) == 0)
	{
		if(attr->value)
			mod_del_cmd((Message *)attr->value);

		return;
	}

	if(strncmp(attr->key, "hook", MAPI_V3_KEY_MAXLEN) == 0)
	{
		// ???
		return;
	}

	if(strncmp(attr->key, "hookfn", MAPI_V3_KEY_MAXLEN) == 0)
	{
		if(!attr->value)
			return;

		mapi_hfn_list_av1 *const v = (mapi_hfn_list_av1 *)attr->value;
		remove_hook(v->hapi_name, v->fn);
		return;
	}

	if(strncmp(attr->key, "cap", MAPI_V3_KEY_MAXLEN) == 0)
	{
		if(attr->value)
			fini_module__cap(mod, (mapi_cap_list_av2 *)attr->value);

		return;
	}

	if(strncmp(attr->key, "fini", MAPI_V3_KEY_MAXLEN) == 0)
	{
		if(attr->value)
			attr->fini();

		return;
	}
}


static
void fini_module_v3(struct module *const mod)
{
	struct mapi_mheader_av3 *const h = (struct mapi_mheader_av3 *)mod->mapi_header;
	if(!h->attrs)
	{
		module_log(mod, "(unload) has no attribute vector!");
		return;
	}

	ssize_t i = -1;
	struct mapi_av3_attr *attr;
	for(attr = h->attrs[++i]; attr; attr = h->attrs[++i]);
	for(attr = h->attrs[--i]; i > -1; attr = h->attrs[--i])
		fini_v3_module_attr(mod, h, attr);
}


static
bool init_module_v3(struct module *const mod)
{
	struct mapi_mheader_av3 *const h = (struct mapi_mheader_av3 *)mod->mapi_header;
	if(!h->attrs)
	{
		module_log(mod, "has no attribute vector!");
		return false;
	}

	size_t i = 0;
	for(struct mapi_av3_attr *attr = h->attrs[i]; attr; attr = h->attrs[++i])
	{
		if(!init_v3_module_attr(mod, h, attr))
		{
			h->attrs[i] = NULL;
			fini_module_v3(mod);
			return false;
		}
	}

	return true;
}


static
bool init_module(struct module *const mod)
{
	mod->mapi_header = lt_dlsym(mod->address, "_mheader");
	if(!mod->mapi_header)
	{
		module_log(mod, "has no MAPI header. (%s)", lt_dlerror());
		return false;
	}

	const int version_magic = *(const int *)mod->mapi_header;
	if(MAPI_MAGIC(version_magic) != MAPI_MAGIC_HDR)
	{
		module_log(mod, "has an invalid header (magic is [%x] mismatches [%x]).",
		           MAPI_MAGIC(version_magic),
		           MAPI_MAGIC_HDR);
		return false;
	}

	mod->mapi_version = MAPI_VERSION(version_magic);
	switch(mod->mapi_version)
	{
		case 1:  return init_module_v1(mod);
		case 2:  return init_module_v2(mod);
		case 3:  return init_module_v3(mod);
		default:
			module_log(mod, "has unknown/unsupported MAPI version %d.", mod->mapi_version);
			return false;
	}
}


static
const char *reflect_origin(const int origin)
{
	switch(origin)
	{
		case MAPI_ORIGIN_EXTENSION:  return "extension";
		case MAPI_ORIGIN_CORE:       return "core";
		default:                     return "unknown";
	}
}


static
void free_module(struct module **ptr)
{
	if(!ptr || !*ptr)
		return;

	struct module *mod = *ptr;
	if(mod->name)
		rb_free(mod->name);

	if(mod->path)
		rb_free(mod->path);

	rb_free(mod);
}


static
void close_handle(lt_dlhandle *handle)
{
	if(handle && *handle)
		lt_dlclose(*handle);
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
	char *const name RB_AUTO_PTR = rb_basename(path);

	/* Trim off the ending for the display name if we have to */
	char *c;
	if((c = rb_strcasestr(name, LT_MODULE_EXT)) != NULL)
		*c = '\0';

	lt_dlhandle handle RB_UNIQUE_PTR(close_handle) = lt_dlopenext(path);
	if(handle == NULL)
	{
		slog(L_MAIN, SNO_GENERAL, "Error loading module %s: %s", name, lt_dlerror());
		return false;
	}

	struct module *mod RB_UNIQUE_PTR(free_module) = (module *)rb_malloc(sizeof(struct module));
	mod->name = rb_strdup(name);
	mod->path = rb_strdup(path);
	mod->address = handle;
	mod->origin = origin;
	mod->core = core;

	if(!init_module(mod))
	{
		slog(L_MAIN, SNO_GENERAL, "Loading module %s aborted.", name);
		return false;
	}

	if(!mod->version)
		mod->version = "<unknown>";

	if(!mod->description)
		mod->description = "<no description>";

	if(warn)
		slog(L_MAIN, SNO_GENERAL,
		     "Module %s [version: %s; MAPI version: %d; origin: %s; description: \"%s\"] loaded from [%s] to %p",
		     name,
		     mod->version,
		     mod->mapi_version,
		     reflect_origin(mod->origin),
		     mod->description,
		     mod->path,
		     (const void *)mod->address);

	// NULL the acquired resources after commitment to list ownership
	rb_dlinkAdd(mod, &mod->node, &module_list);
	mod = NULL;
	handle = NULL;
	return true;
}


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
		case 1:  fini_module_v1(mod);  break;
		case 2:  fini_module_v2(mod);  break;
		case 3:  fini_module_v3(mod);  break;
		default:
			slog(L_MAIN, SNO_GENERAL,
			     "Unknown/unsupported MAPI version %d when unloading %s!",
			     mod->mapi_version,
			     mod->name);
		break;
	}

	rb_dlinkDelete(&mod->node, &module_list);
	close_handle(&mod->address);

	if(warn)
		slog(L_MAIN, SNO_GENERAL, "Module %s unloaded", name);

	// free after the unload message in case *name came from the mod struct.
	free_module(&mod);
	return true;
}


} // namespace ircd
