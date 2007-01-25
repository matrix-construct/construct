/* 
 * ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 * monitor.h: Code for server-side notify lists.
 *
 * Copyright (C) 2005 Lee Hardy <lee -at- leeh.co.uk>
 * Copyright (C) 2005 ircd-ratbox development team
 *
 * $Id: monitor.h 6 2005-09-10 01:02:21Z nenolod $
 */
#ifndef INCLUDED_monitor_h
#define INCLUDED_monitor_h

struct monitor
{
	struct monitor *hnext;
	char name[NICKLEN];
	dlink_list users;
};

extern BlockHeap *monitor_heap;

#define MONITOR_HASH_SIZE 65536
#define MONITOR_HASH_BITS 16

void init_monitor(void);
struct monitor *find_monitor(const char *name, int add);
void clear_monitor(struct Client *);

void monitor_signon(struct Client *);
void monitor_signoff(struct Client *);

#endif
