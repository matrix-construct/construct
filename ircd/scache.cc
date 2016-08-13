/*
 *  charybdis
 *  scache.c: Server names cache.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
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

#include <ircd/stdinc.h>
#include <ircd/client.h>
#include <ircd/match.h>
#include <ircd/ircd.h>
#include <ircd/numeric.h>
#include <ircd/send.h>
#include <ircd/scache.h>
#include <ircd/s_conf.h>
#include <ircd/s_assert.h>

namespace ircd {

/*
 * ircd used to store full servernames in anUser as well as in the
 * whowas info.  there can be some 40k such structures alive at any
 * given time, while the number of unique server names a server sees
 * in its lifetime is at most a few hundred.  by tokenizing server
 * names internally, the server can easily save 2 or 3 megs of RAM.
 * -orabidoo
 *
 * reworked to serve for flattening/delaying /links also
 * -- jilles
 *
 * reworked to make use of rb_radixtree.
 * -- kaniini
 */

#define SC_ONLINE 1
#define SC_HIDDEN 2

struct scache_entry
{
	char name[HOSTLEN + 1];
	char info[REALLEN + 1];
	int flags;
	time_t known_since;
	time_t last_connect;
	time_t last_split;
};

static rb_radixtree *scache_tree = NULL;

void
clear_scache_hash_table(void)
{
	scache_tree = rb_radixtree_create("server names cache", irccasecanon);
}

static struct scache_entry *
find_or_add(const char *name)
{
	struct scache_entry *ptr;

	ptr = (scache_entry *)rb_radixtree_retrieve(scache_tree, name);
	if (ptr != NULL)
		return ptr;

	ptr = (struct scache_entry *) rb_malloc(sizeof(struct scache_entry));
	s_assert(0 != ptr);

	rb_strlcpy(ptr->name, name, sizeof(ptr->name));
	ptr->info[0] = '\0';
	ptr->flags = 0;
	ptr->known_since = rb_current_time();
	ptr->last_connect = 0;
	ptr->last_split = 0;

	rb_radixtree_add(scache_tree, ptr->name, ptr);

	return ptr;
}

struct scache_entry *
scache_connect(const char *name, const char *info, int hidden)
{
	struct scache_entry *ptr;

	ptr = find_or_add(name);
	rb_strlcpy(ptr->info, info, sizeof(ptr->info));
	ptr->flags |= SC_ONLINE;
	if (hidden)
		ptr->flags |= SC_HIDDEN;
	else
		ptr->flags &= ~SC_HIDDEN;
	ptr->last_connect = rb_current_time();
	return ptr;
}

void
scache_split(struct scache_entry *ptr)
{
	if (ptr == NULL)
		return;
	ptr->flags &= ~SC_ONLINE;
	ptr->last_split = rb_current_time();
}

const char *scache_get_name(struct scache_entry *ptr)
{
	return ptr->name;
}

/* scache_send_flattened_links()
 *
 * inputs	- client to send to
 * outputs	- the cached links, us, and RPL_ENDOFLINKS
 * side effects	-
 */
void
scache_send_flattened_links(struct Client *source_p)
{
	struct scache_entry *scache_ptr;
	rb_radixtree_iteration_state iter;
	int show;

	void *elem;
	RB_RADIXTREE_FOREACH(elem, &iter, scache_tree)
	{
		scache_ptr = (scache_entry *)elem;

		if (!irccmp(scache_ptr->name, me.name))
			show = FALSE;
		else if (scache_ptr->flags & SC_HIDDEN &&
				!ConfigServerHide.disable_hidden)
			show = FALSE;
		else if (scache_ptr->flags & SC_ONLINE)
			show = scache_ptr->known_since < rb_current_time() - ConfigServerHide.links_delay;
		else
			show = scache_ptr->last_split > rb_current_time() - ConfigServerHide.links_delay && scache_ptr->last_split - scache_ptr->known_since > ConfigServerHide.links_delay;
		if (show)
			sendto_one_numeric(source_p, RPL_LINKS, form_str(RPL_LINKS),
					   scache_ptr->name, me.name, 1, scache_ptr->info);
	}
	sendto_one_numeric(source_p, RPL_LINKS, form_str(RPL_LINKS),
			   me.name, me.name, 0, me.info);

	sendto_one_numeric(source_p, RPL_ENDOFLINKS, form_str(RPL_ENDOFLINKS), "*");
}

#define MISSING_TIMEOUT 86400

/* scache_send_missing()
 *
 * inputs	- client to send to
 * outputs	- recently split servers
 * side effects	-
 */
void
scache_send_missing(struct Client *source_p)
{
	struct scache_entry *scache_ptr;
	rb_radixtree_iteration_state iter;

	void *elem;
	RB_RADIXTREE_FOREACH(elem, &iter, scache_tree)
	{
		scache_ptr = (scache_entry *)elem;

		if (!(scache_ptr->flags & SC_ONLINE) && scache_ptr->last_split > rb_current_time() - MISSING_TIMEOUT)
			sendto_one_numeric(source_p, RPL_MAP, "** %s (recently split)",
					   scache_ptr->name);
	}
}

/*
 * count_scache
 * inputs	- pointer to where to leave number of servers cached
 *		- pointer to where to leave total memory usage
 * output	- NONE
 * side effects	-
 */
void
count_scache(size_t * number_servers_cached, size_t * mem_servers_cached)
{
	struct scache_entry *scache_ptr;
	rb_radixtree_iteration_state iter;

	*number_servers_cached = 0;
	*mem_servers_cached = 0;

	void *elem;
	RB_RADIXTREE_FOREACH(elem, &iter, scache_tree)
	{
		scache_ptr = (scache_entry *)elem;

		*number_servers_cached = *number_servers_cached + 1;
		*mem_servers_cached = *mem_servers_cached +
			sizeof(struct scache_entry);
	}
}


} // namespace ircd
