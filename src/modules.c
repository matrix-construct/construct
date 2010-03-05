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



/* -TimeMr14C:
 * I have moved the dl* function definitions and
 * the two functions (load_a_module / unload_a_module) to the
 * file dynlink.c 
 * And also made the necessary changes to those functions
 * to comply with shl_load and friends.
 * In this file, to keep consistency with the makefile, 
 * I added the ability to load *.sl files, too.
 * 27/02/2002
 */

#ifndef STATIC_MODULES

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

struct Message modload_msgtab = {
	"MODLOAD", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_modload, 2}}
};

struct Message modunload_msgtab = {
	"MODUNLOAD", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_modunload, 2}}
};

struct Message modreload_msgtab = {
	"MODRELOAD", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_modreload, 2}}
};

struct Message modlist_msgtab = {
	"MODLIST", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_modlist, 0}}
};

struct Message modrestart_msgtab = {
	"MODRESTART", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_modrestart, 0}}
};

void
modules_init(void)
{
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
static struct module_path *
mod_find_path(const char *path)
{
	rb_dlink_node *ptr;
	struct module_path *mpath;

	RB_DLINK_FOREACH(ptr, mod_paths.head)
	{
		mpath = ptr->data;

		if(!strcmp(path, mpath->path))
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
	struct module_path *pathst;

	if(mod_find_path(path))
		return;

	pathst = rb_malloc(sizeof(struct module_path));

	strcpy(pathst->path, path);
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

	modlist = (struct module **) rb_malloc(sizeof(struct module) * (MODS_INCREMENT));

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
		if((len > 3) && !strcmp(ldirent->d_name+len-3, SHARED_SUFFIX))
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
	char module_name[MAXPATHLEN];
	int i;


	for (i = 0; core_module_table[i]; i++)
	{
		rb_snprintf(module_name, sizeof(module_name), "%s/%s%s", MODPATH,
			    core_module_table[i], SHARED_SUFFIX);

		if(load_a_module(module_name, warn, 1) == -1)
		{
			ilog(L_MAIN,
			     "Error loading core module %s%s: terminating ircd",
			     core_module_table[i], SHARED_SUFFIX);
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
	char modpath[MAXPATHLEN];
	rb_dlink_node *pathst;
	struct module_path *mpath;

	struct stat statbuf;

	if (server_state_foreground == 1)
		inotice("loading module %s ...", path);	

	RB_DLINK_FOREACH(pathst, mod_paths.head)
	{
		mpath = pathst->data;

		rb_snprintf(modpath, sizeof(modpath), "%s/%s", mpath->path, path);
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
	char *m_bn;

	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "admin");
		return 0;
	}

	m_bn = rb_basename(parv[1]);

	if(findmodule_byname(m_bn) != -1)
	{
		sendto_one_notice(source_p, ":Module %s is already loaded", m_bn);
		rb_free(m_bn);
		return 0;
	}

	load_one_module(parv[1], 0);

	rb_free(m_bn);

	return 0;
}


/* unload a module .. */
static int
mo_modunload(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	char *m_bn;
	int modindex;

	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "admin");
		return 0;
	}

	m_bn = rb_basename(parv[1]);

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
	char *m_bn;
	int modindex;
	int check_core;

	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "admin");
		return 0;
	}

	m_bn = rb_basename(parv[1]);

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
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
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
	int i;

	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "admin");
		return 0;
	}

	for (i = 0; i < num_mods; i++)
	{
		if(parc > 1)
		{
			if(match(parv[1], modlist[i]->name))
			{
				sendto_one(source_p, form_str(RPL_MODLIST),
					   me.name, source_p->name,
					   modlist[i]->name,
					   modlist[i]->address,
					   modlist[i]->version, modlist[i]->core ? "(core)" : "");
			}
		}
		else
		{
			sendto_one(source_p, form_str(RPL_MODLIST),
				   me.name, source_p->name, modlist[i]->name,
				   modlist[i]->address, modlist[i]->version,
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
	int modnum;

	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "admin");
		return 0;
	}

	sendto_one_notice(source_p, ":Reloading all modules");

	modnum = num_mods;
	while (num_mods)
		unload_one_module(modlist[0]->name, 0);

	load_all_modules(0);
	load_core_modules(0);
	rehash(0);

	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			     "Module Restart: %d modules unloaded, %d modules loaded",
			     modnum, num_mods);
	ilog(L_MAIN, "Module Restart: %d modules unloaded, %d modules loaded", modnum, num_mods);
	return 0;
}



#ifndef RTLD_NOW
#define RTLD_NOW RTLD_LAZY	/* openbsd deficiency */
#endif

#ifndef RTLD_LOCAL
#define RTLD_LOCAL 0
#endif

#ifdef CHARYBDIS_PROFILE
# ifndef RTLD_PROFILE
#  warning libdl may not support profiling, sucks. :(
#  define RTLD_PROFILE 0
# endif
#endif

static void increase_modlist(void);

#define MODS_INCREMENT 10

static char unknown_ver[] = "<unknown>";

/* This file contains the core functions to use dynamic libraries.
 * -TimeMr14C
 */


#ifdef HAVE_MACH_O_DYLD_H
/*
** jmallett's dl*(3) shims for NSModule(3) systems.
*/
#include <mach-o/dyld.h>

#ifndef HAVE_DLOPEN
#ifndef	RTLD_LAZY
#define RTLD_LAZY 2185		/* built-in dl*(3) don't care */
#endif

void undefinedErrorHandler(const char *);
NSModule multipleErrorHandler(NSSymbol, NSModule, NSModule);
void linkEditErrorHandler(NSLinkEditErrors, int, const char *, const char *);
char *dlerror(void);
void *dlopen(char *, int);
int dlclose(void *);
void *dlsym(void *, char *);

static int firstLoad = TRUE;
static int myDlError;
static char *myErrorTable[] = { "Loading file as object failed\n",
	"Loading file as object succeeded\n",
	"Not a valid shared object\n",
	"Architecture of object invalid on this architecture\n",
	"Invalid or corrupt image\n",
	"Could not access object\n",
	"NSCreateObjectFileImageFromFile failed\n",
	NULL
};

void
undefinedErrorHandler(const char *symbolName)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "Undefined symbol: %s", symbolName);
	ilog(L_MAIN, "Undefined symbol: %s", symbolName);
	return;
}

NSModule
multipleErrorHandler(NSSymbol s, NSModule old, NSModule new)
{
	/* XXX
	 ** This results in substantial leaking of memory... Should free one
	 ** module, maybe?
	 */
	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			     "Symbol `%s' found in `%s' and `%s'",
			     NSNameOfSymbol(s), NSNameOfModule(old), NSNameOfModule(new));
	ilog(L_MAIN, "Symbol `%s' found in `%s' and `%s'",
	     NSNameOfSymbol(s), NSNameOfModule(old), NSNameOfModule(new));
	/* We return which module should be considered valid, I believe */
	return new;
}

void
linkEditErrorHandler(NSLinkEditErrors errorClass, int errnum,
		     const char *fileName, const char *errorString)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			     "Link editor error: %s for %s", errorString, fileName);
	ilog(L_MAIN, "Link editor error: %s for %s", errorString, fileName);
	return;
}

char *
dlerror(void)
{
	return myDlError == NSObjectFileImageSuccess ? NULL : myErrorTable[myDlError % 7];
}

void *
dlopen(char *filename, int unused)
{
	NSObjectFileImage myImage;
	NSModule myModule;

	if(firstLoad)
	{
		/*
		 ** If we are loading our first symbol (huzzah!) we should go ahead
		 ** and install link editor error handling!
		 */
		NSLinkEditErrorHandlers linkEditorErrorHandlers;

		linkEditorErrorHandlers.undefined = undefinedErrorHandler;
		linkEditorErrorHandlers.multiple = multipleErrorHandler;
		linkEditorErrorHandlers.linkEdit = linkEditErrorHandler;
		NSInstallLinkEditErrorHandlers(&linkEditorErrorHandlers);
		firstLoad = FALSE;
	}
	myDlError = NSCreateObjectFileImageFromFile(filename, &myImage);
	if(myDlError != NSObjectFileImageSuccess)
	{
		return NULL;
	}
	myModule = NSLinkModule(myImage, filename, NSLINKMODULE_OPTION_PRIVATE);
	return (void *) myModule;
}

int
dlclose(void *myModule)
{
	NSUnLinkModule(myModule, FALSE);
	return 0;
}

void *
dlsym(void *myModule, char *mySymbolName)
{
	NSSymbol mySymbol;

	mySymbol = NSLookupSymbolInModule((NSModule) myModule, mySymbolName);
	return NSAddressOfSymbol(mySymbol);
}
#endif
#endif


/*
 * HPUX dl compat functions
 */
#if defined(HAVE_SHL_LOAD) && !defined(HAVE_DLOPEN)
#define RTLD_LAZY BIND_DEFERRED
#define RTLD_GLOBAL DYNAMIC_PATH
#define dlopen(file,mode) (void *)shl_load((file), (mode), (long) 0)
#define dlclose(handle) shl_unload((shl_t)(handle))
#define dlsym(handle,name) hpux_dlsym(handle,name)
#define dlerror() strerror(errno)

static void *
hpux_dlsym(void *handle, char *name)
{
	void *sym_addr;
	if(!shl_findsym((shl_t *) & handle, name, TYPE_UNDEFINED, &sym_addr))
		return sym_addr;
	return NULL;
}

#endif

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

	dlclose(modlist[modindex]->address);

	rb_free(modlist[modindex]->name);
	memmove(&modlist[modindex], &modlist[modindex + 1],
	       sizeof(struct module) * ((num_mods - 1) - modindex));

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
	void *tmpptr = NULL;

	char *mod_basename;
	const char *ver;

	int *mapi_version;

	mod_basename = rb_basename(path);

#ifdef CHARYBDIS_PROFILE
	tmpptr = dlopen(path, RTLD_NOW | RTLD_LOCAL | RTLD_PROFILE);
#else
	tmpptr = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif

	if(tmpptr == NULL)
	{
		const char *err = dlerror();

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
	mapi_version = (int *) (uintptr_t) dlsym(tmpptr, "_mheader");
	if((mapi_version == NULL
	    && (mapi_version = (int *) (uintptr_t) dlsym(tmpptr, "__mheader")) == NULL)
	   || MAPI_MAGIC(*mapi_version) != MAPI_MAGIC_HDR)
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "Data format error: module %s has no MAPI header.",
				     mod_basename);
		ilog(L_MAIN, "Data format error: module %s has no MAPI header.", mod_basename);
		(void) dlclose(tmpptr);
		rb_free(mod_basename);
		return -1;
	}

	switch (MAPI_VERSION(*mapi_version))
	{
	case 1:
		{
			struct mapi_mheader_av1 *mheader = (struct mapi_mheader_av1 *) mapi_version;	/* see above */
			if(mheader->mapi_register && (mheader->mapi_register() == -1))
			{
				ilog(L_MAIN, "Module %s indicated failure during load.",
				     mod_basename);
				sendto_realops_snomask(SNO_GENERAL, L_ALL,
						     "Module %s indicated failure during load.",
						     mod_basename);
				dlclose(tmpptr);
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
		dlclose(tmpptr);
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

	new_modlist = (struct module **) rb_malloc(sizeof(struct module) *
						  (max_mods + MODS_INCREMENT));
	memcpy((void *) new_modlist, (void *) modlist, sizeof(struct module) * num_mods);

	rb_free(modlist);
	modlist = new_modlist;
	max_mods += MODS_INCREMENT;
}

#else /* STATIC_MODULES */

/* load_all_modules()
 *
 * input        -
 * output       -
 * side effects - all the msgtabs are added for static modules
 */
void
load_all_modules(int warn)
{
	load_static_modules();
}

#endif /* STATIC_MODULES */
