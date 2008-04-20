/*
 *  ircd-ratbox: A slightly useful ircd.
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
 *
 *  $Id: scache.c 6 2005-09-10 01:02:21Z nenolod $
 */

#include "stdinc.h"
#include "client.h"
#include "common.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "scache.h"
#include "s_conf.h"


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
 */


#define SCACHE_HASH_SIZE 257

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
	struct scache_entry *next;
};

static struct scache_entry *scache_hash[SCACHE_HASH_SIZE];

void
clear_scache_hash_table(void)
{
	memset(scache_hash, 0, sizeof(scache_hash));
}

static int
sc_hash(const char *string)
{
	int hash_value;

	hash_value = 0;
	while (*string)
	{
		hash_value += ToLower(*string++);
	}

	return hash_value % SCACHE_HASH_SIZE;
}

static struct scache_entry *
find_or_add(const char *name)
{
	int hash_index;
	struct scache_entry *ptr;

	ptr = scache_hash[hash_index = sc_hash(name)];
	for (; ptr; ptr = ptr->next)
	{
		if(!irccmp(ptr->name, name))
			return ptr;
	}

	ptr = (struct scache_entry *) rb_malloc(sizeof(struct scache_entry));
	s_assert(0 != ptr);

	rb_strlcpy(ptr->name, name, sizeof(ptr->name));
	ptr->info[0] = '\0';
	ptr->flags = 0;
	ptr->known_since = rb_current_time();
	ptr->last_connect = 0;
	ptr->last_split = 0;

	ptr->next = scache_hash[hash_index];
	scache_hash[hash_index] = ptr;
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
	int i;
	int show;

	for (i = 0; i < SCACHE_HASH_SIZE; i++)
	{
		scache_ptr = scache_hash[i];
		while (scache_ptr)
		{
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

			scache_ptr = scache_ptr->next;
		}
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
	int i;

	for (i = 0; i < SCACHE_HASH_SIZE; i++)
	{
		scache_ptr = scache_hash[i];
		while (scache_ptr)
		{
			if (!(scache_ptr->flags & SC_ONLINE) && scache_ptr->last_split > rb_current_time() - MISSING_TIMEOUT)
				sendto_one_numeric(source_p, RPL_MAP, "** %s (recently split)", 
						   scache_ptr->name);

			scache_ptr = scache_ptr->next;
		}
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
	int i;

	*number_servers_cached = 0;
	*mem_servers_cached = 0;

	for (i = 0; i < SCACHE_HASH_SIZE; i++)
	{
		scache_ptr = scache_hash[i];
		while (scache_ptr)
		{
			*number_servers_cached = *number_servers_cached + 1;
			*mem_servers_cached = *mem_servers_cached +
				sizeof(struct scache_entry );

			scache_ptr = scache_ptr->next;
		}
	}
}
