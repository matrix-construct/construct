/* $Id: cache.h 24250 2007-08-22 19:15:08Z androsyn $ */
#ifndef INCLUDED_CACHE_H
#define INCLUDED_CACHE_H

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
	rb_dlink_list contents;
	int flags;
};

struct cacheline
{
	char data[CACHELINELEN];
	rb_dlink_node linenode;
};

extern struct cachefile *user_motd;
extern struct cachefile *oper_motd;
extern struct cacheline *emptyline;

extern char user_motd_changed[MAX_DATE_STRING];
extern rb_dlink_list links_cache_list;

void init_cache(void);
struct cachefile *cache_file(const char *, const char *, int);
void cache_links(void *unused);
void free_cachefile(struct cachefile *);

void load_help(void);

void send_user_motd(struct Client *);
void send_oper_motd(struct Client *);
void cache_user_motd(void);

struct Dictionary;
extern struct Dictionary *help_dict_oper;
extern struct Dictionary *help_dict_user;
#endif

