/*
 * ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 * monitor.c - Code for server-side notify lists
 *
 * Copyright (C) 2005 Lee Hardy <lee -at- leeh.co.uk>
 * Copyright (C) 2005 ircd-ratbox development team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1.Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 2.Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3.The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "stdinc.h"
#include "client.h"
#include "monitor.h"
#include "hash.h"
#include "numeric.h"
#include "send.h"
#include "irc_radixtree.h"

static struct irc_radixtree *monitor_tree;

void
init_monitor(void)
{
	monitor_tree = irc_radixtree_create("monitor lists", irc_radixtree_irccasecanon);
}

struct monitor *
find_monitor(const char *name, int add)
{
	struct monitor *monptr;

	monptr = irc_radixtree_retrieve(monitor_tree, name);
	if (monptr != NULL)
		return monptr;

	if(add)
	{
		monptr = rb_malloc(sizeof(*monptr));
		rb_strlcpy(monptr->name, name, sizeof(monptr->name));
		irc_radixtree_add(monitor_tree, monptr->name, monptr);

		return monptr;
	}

	return NULL;
}

void
free_monitor(struct monitor *monptr)
{
	if (rb_dlink_list_length(&monptr->users) > 0)
		return;

	irc_radixtree_delete(monitor_tree, monptr->name);
	rb_free(monptr);
}

/* monitor_signon()
 *
 * inputs	- client who has just connected
 * outputs	-
 * side effects	- notifies any clients monitoring this nickname that it has
 * 		  connected to the network
 */
void
monitor_signon(struct Client *client_p)
{
	char buf[USERHOST_REPLYLEN];
	struct monitor *monptr = find_monitor(client_p->name, 0);

	/* noones watching this nick */
	if(monptr == NULL)
		return;

	snprintf(buf, sizeof(buf), "%s!%s@%s", client_p->name, client_p->username, client_p->host);

	sendto_monitor(monptr, form_str(RPL_MONONLINE), me.name, "*", buf);
}

/* monitor_signoff()
 *
 * inputs	- client who is exiting
 * outputs	-
 * side effects	- notifies any clients monitoring this nickname that it has
 * 		  left the network
 */
void
monitor_signoff(struct Client *client_p)
{
	struct monitor *monptr = find_monitor(client_p->name, 0);

	/* noones watching this nick */
	if(monptr == NULL)
		return;

	sendto_monitor(monptr, form_str(RPL_MONOFFLINE), me.name, "*",
			client_p->name);
}

void
clear_monitor(struct Client *client_p)
{
	struct monitor *monptr;
	rb_dlink_node *ptr, *next_ptr;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, client_p->localClient->monitor_list.head)
	{
		monptr = ptr->data;

		rb_dlinkFindDestroy(client_p, &monptr->users);
		rb_free_rb_dlink_node(ptr);

		free_monitor(monptr);
	}

	client_p->localClient->monitor_list.head = client_p->localClient->monitor_list.tail = NULL;
	client_p->localClient->monitor_list.length = 0;
}
