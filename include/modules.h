/*
 *  ircd-ratbox: A slightly useful ircd.
 *  modules.h: A header for the modules functions.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
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
 *  $Id: modules.h 6 2005-09-10 01:02:21Z nenolod $
 */

#ifndef INCLUDED_modules_h
#define INCLUDED_modules_h
#include "config.h"
#include "setup.h"
#include "parse.h"

#define MAPI_RATBOX 1

#if defined(HAVE_SHL_LOAD)
#include <dl.h>
#endif
#if !defined(STATIC_MODULES) && defined(HAVE_DLFCN_H)
#include <dlfcn.h>
#endif

#include "msg.h"
#include "hook.h"

struct module
{
	char *name;
	const char *version;
	void *address;
	int core;
	int mapi_version;
	void * mapi_header; /* actually struct mapi_mheader_av<mapi_version>	*/
};

struct module_path
{
	char path[MAXPATHLEN];
};

#define MAPI_MAGIC_HDR	0x4D410000

#define MAPI_V1		(MAPI_MAGIC_HDR | 0x1)

#define MAPI_MAGIC(x)	((x) & 0xffff0000)
#define MAPI_VERSION(x)	((x) & 0x0000ffff)

typedef struct Message* mapi_clist_av1;

typedef struct
{
	const char *	hapi_name;
	int *		hapi_id;
} mapi_hlist_av1;

typedef struct
{
	const char * 	hapi_name;
	hookfn 		fn;
} mapi_hfn_list_av1;

struct mapi_mheader_av1
{
	int		  mapi_version;				/* Module API version		*/
	int		(*mapi_register)	(void);		/* Register function;
								   ret -1 = failure (unload)	*/
	void		(*mapi_unregister)	(void);		/* Unregister function.		*/
	mapi_clist_av1	* mapi_command_list;			/* List of commands to add.	*/
	mapi_hlist_av1	* mapi_hook_list;			/* List of hooks to add.	*/
	mapi_hfn_list_av1 *mapi_hfn_list;			/* List of hook_add_hook's to do */
	const char *	  mapi_module_version;			/* Module's version (freeform)	*/
};

#ifndef STATIC_MODULES
# define DECLARE_MODULE_AV1(name,reg,unreg,cl,hl,hfnlist, v) \
	struct mapi_mheader_av1 _mheader = { MAPI_V1, reg, unreg, cl, hl, hfnlist, v}
#else
# define DECLARE_MODULE_AV1(name,reg,unreg,cl,hl,hfnlist, v) \
	struct mapi_mheader_av1 name ## _mheader = { MAPI_V1, reg, unreg, cl, hl, hfnlist, v}
void load_static_modules(void);
#endif

/* add a path */
void mod_add_path(const char *path);
void mod_clear_paths(void);

/* load a module */
extern void load_module(char *path);

/* load all modules */
extern void load_all_modules(int warn);

/* load core modules */
extern void load_core_modules(int);

extern int unload_one_module(const char *, int);
extern int load_one_module(const char *, int);
extern int load_a_module(const char *, int, int);
extern int findmodule_byname(const char *);
extern void modules_init(void);

#endif /* INCLUDED_modules_h */
