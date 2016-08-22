/*
 * ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 * monitor.h: Code for server-side notify lists.
 *
 * Copyright (C) 2005 Lee Hardy <lee -at- leeh.co.uk>
 * Copyright (C) 2005 ircd-ratbox development team
 */

#pragma once
#define HAVE_IRCD_MONITOR_H

struct rb_bh;

#ifdef __cplusplus
namespace ircd {

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
void clear_monitor(client::client *);

void monitor_signon(client::client *);
void monitor_signoff(client::client *);

}      // namespace ircd
#endif // __cplusplus
