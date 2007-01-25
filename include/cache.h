/* $Id: cache.h 6 2005-09-10 01:02:21Z nenolod $ */
#ifndef INCLUDED_CACHE_H
#define INCLUDED_CACHE_H

#include "client.h"
#include "tools.h"

#define HELP_MAX	100

#define CACHELINELEN	81
#define CACHEFILELEN	30
/* two servernames, a gecos, three spaces, ":1", '\0' */
#define LINKSLINELEN	(HOSTLEN + HOSTLEN + REALLEN + 6)

#define HELP_USER	0x001
#define HELP_OPER	0x002

struct Client;

struct cachefile
{
	char name[CACHEFILELEN];
	dlink_list contents;
	int flags;
};

struct cacheline
{
	char data[CACHELINELEN];
	dlink_node linenode;
};

extern struct cachefile *user_motd;
extern struct cachefile *oper_motd;
extern struct cacheline *emptyline;
extern dlink_list links_cache_list;

extern char user_motd_changed[MAX_DATE_STRING];

extern void init_cache(void);
extern struct cachefile *cache_file(const char *, const char *, int);
extern void cache_links(void *unused);
extern void free_cachefile(struct cachefile *);

extern void load_help(void);

extern void send_user_motd(struct Client *);
extern void send_oper_motd(struct Client *);

#endif

