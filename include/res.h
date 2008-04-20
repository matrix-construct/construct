/*
 * res.h for referencing functions in res.c, reslib.c
 *
 * $Id: res.h 2023 2006-09-02 23:47:27Z jilles $
 */

#ifndef _CHARYBDIS_RES_H
#define _CHARYBDIS_RES_H

#include "ircd_defs.h"
#include "common.h"
#include "reslib.h"
#include "match.h"
#include "ircd.h"

/* Maximum number of nameservers in /etc/resolv.conf we care about 
 * In hybrid, this was 2 -- but in Charybdis, we want to track
 * a few more than that ;) --nenolod
 */
#define IRCD_MAXNS 10

struct DNSReply
{
  char *h_name;
  struct rb_sockaddr_storage addr;
};

struct DNSQuery
{
  void *ptr; /* pointer used by callback to identify request */
  void (*callback)(void* vptr, struct DNSReply *reply); /* callback to call */
};

extern struct rb_sockaddr_storage irc_nsaddr_list[];
extern int irc_nscount;

extern void init_resolver(void);
extern void restart_resolver(void);
extern void delete_resolver_queries(const struct DNSQuery *);
extern void gethost_byname_type(const char *, struct DNSQuery *, int);
extern void gethost_byaddr(const struct rb_sockaddr_storage *, struct DNSQuery *);
extern void add_local_domain(char *, size_t);
extern void report_dns_servers(struct Client *);

#endif
