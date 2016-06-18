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
 */

#ifndef INCLUDED_modules_h
#define INCLUDED_modules_h
#include "serno.h"
#include "defaults.h"
#include "setup.h"
#include "parse.h"

#define MAPI_CHARYBDIS 2

#include <ltdl.h>

#include "msg.h"
#include "hook.h"

struct module
{
	char *name;
	const char *version;
	const char *description;
	lt_dlhandle address;
	int core;	/* This is int for backwards compat reasons */
	int origin;	/* Ditto */
	int mapi_version;
	void *mapi_header; /* actually struct mapi_mheader_av<mapi_version> */
	rb_dlink_node node;
};

#define MAPI_MAGIC_HDR	0x4D410000

#define MAPI_V1		(MAPI_MAGIC_HDR | 0x1)
#define MAPI_V2		(MAPI_MAGIC_HDR | 0x2)

#define MAPI_MAGIC(x)	((x) & 0xffff0000)
#define MAPI_VERSION(x)	((x) & 0x0000ffff)

typedef struct Message* mapi_clist_av1;

typedef struct
{
	const char *hapi_name;
	int *hapi_id;
} mapi_hlist_av1;

typedef struct
{
	const char *hapi_name;
	hookfn fn;
} mapi_hfn_list_av1;


#define MAPI_CAP_CLIENT		1
#define MAPI_CAP_SERVER		2

typedef struct
{
	int cap_index;		/* Which cap index does this belong to? */
	const char *cap_name;	/* Capability name */
	void *cap_ownerdata;	/* Not used much but why not... */
	unsigned int *cap_id;	/* May be set to non-NULL to store cap id */
} mapi_cap_list_av2;

struct mapi_mheader_av1
{
	int mapi_version;			/* Module API version */
	int (*mapi_register)(void);		/* Register function; ret -1 = failure (unload) */
	void (*mapi_unregister)(void);		/* Unregister function.	*/
	mapi_clist_av1 *mapi_command_list;	/* List of commands to add. */
	mapi_hlist_av1 *mapi_hook_list;		/* List of hooks to add. */
	mapi_hfn_list_av1 *mapi_hfn_list;	/* List of hook_add_hook's to do */
	const char *mapi_module_version;	/* Module's version (freeform) */
};

#define MAPI_ORIGIN_UNKNOWN	0		/* Unknown provenance (AV1 etc.) */
#define MAPI_ORIGIN_EXTENSION	1		/* Charybdis extension */
#define MAPI_ORIGIN_CORE	2		/* Charybdis core module */

struct mapi_mheader_av2
{
	int mapi_version;			/* Module API version */
	int (*mapi_register)(void);		/* Register function; ret -1 = failure (unload) */
	void (*mapi_unregister)(void);		/* Unregister function.	*/
	mapi_clist_av1 *mapi_command_list;	/* List of commands to add. */
	mapi_hlist_av1 *mapi_hook_list;		/* List of hooks to add. */
	mapi_hfn_list_av1 *mapi_hfn_list;	/* List of hook_add_hook's to do */
	mapi_cap_list_av2 *mapi_cap_list;	/* List of CAPs to add */
	const char *mapi_module_version;	/* Module's version (freeform), replaced with ircd version if NULL */
	const char *mapi_module_description;	/* Module's description (freeform) */
	unsigned long int mapi_datecode;	/* Unix timestamp of module's build */
};

#define DECLARE_MODULE_AV1(name, reg, unreg, cl, hl, hfnlist, v) \
	struct mapi_mheader_av1 _mheader = { MAPI_V1, reg, unreg, cl, hl, hfnlist, v}

#define DECLARE_MODULE_AV2(name, reg, unreg, cl, hl, hfnlist, caplist, v, desc) \
	struct mapi_mheader_av2 _mheader = { MAPI_V2, reg, unreg, cl, hl, hfnlist, caplist, v, desc, DATECODE}

/* add a path */
void mod_add_path(const char *path);
void mod_clear_paths(void);

/* load a module */
extern void load_module(char *path);

/* load all modules */
extern void load_all_modules(bool warn);

/* load core modules */
extern void load_core_modules(bool);

extern bool unload_one_module(const char *, bool);
extern bool load_one_module(const char *, int, bool);
extern bool load_a_module(const char *, bool, int, bool);
extern struct module *findmodule_byname(const char *);
extern void init_modules(void);

extern rb_dlink_list module_list;
extern rb_dlink_list mod_paths;

#endif /* INCLUDED_modules_h */
