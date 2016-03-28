/*
 * res.h for referencing functions in res.c, reslib.c
 *
 */

#ifndef _CHARYBDIS_RES_H
#define _CHARYBDIS_RES_H

/* Maximum number of nameservers in /etc/resolv.conf we care about
 * In hybrid, this was 2 -- but in Charybdis, we want to track
 * a few more than that ;) --nenolod
 */
#define IRCD_MAXNS 10
#define RESOLVER_HOSTLEN 255

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
extern void gethost_byname_type(const char *, struct DNSQuery *, int);
extern void gethost_byaddr(const struct rb_sockaddr_storage *, struct DNSQuery *);
extern void build_rdns(char *, size_t, const struct rb_sockaddr_storage *, const char *);

#endif
