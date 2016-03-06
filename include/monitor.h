/*
 * ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 * monitor.h: Code for server-side notify lists.
 *
 * Copyright (C) 2005 Lee Hardy <lee -at- leeh.co.uk>
 * Copyright (C) 2005 ircd-ratbox development team
 */
#ifndef INCLUDED_monitor_h
#define INCLUDED_monitor_h

struct rb_bh;

struct monitor
{
	char name[NICKLEN];
	rb_dlink_list users;
	rb_dlink_node node;
	unsigned int hashv;
};

#define MONITOR_HASH_BITS 16
#define MONITOR_HASH_SIZE (1<<MONITOR_HASH_BITS)

void free_monitor(struct monitor *);

void init_monitor(void);
struct monitor *find_monitor(const char *name, int add);
void clear_monitor(struct Client *);

void monitor_signon(struct Client *);
void monitor_signoff(struct Client *);

#endif
