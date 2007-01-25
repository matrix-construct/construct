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
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "scache.h"
#include "memory.h"


/*
 * ircd used to store full servernames in anUser as well as in the 
 * whowas info.  there can be some 40k such structures alive at any
 * given time, while the number of unique server names a server sees
 * in its lifetime is at most a few hundred.  by tokenizing server
 * names internally, the server can easily save 2 or 3 megs of RAM.  
 * -orabidoo
 */


#define SCACHE_HASH_SIZE 257

typedef struct scache_entry
{
	char name[HOSTLEN + 1];
	struct scache_entry *next;
}
SCACHE;

static SCACHE *scache_hash[SCACHE_HASH_SIZE];

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

/*
 * this takes a server name, and returns a pointer to the same string
 * (up to case) in the server name token list, adding it to the list if
 * it's not there.  care must be taken not to call this with
 * user-supplied arguments that haven't been verified to be a valid,
 * existing, servername.  use the hash in list.c for those.  -orabidoo
 */

const char *
find_or_add(const char *name)
{
	int hash_index;
	SCACHE *ptr;

	ptr = scache_hash[hash_index = sc_hash(name)];
	for (; ptr; ptr = ptr->next)
	{
		if(!irccmp(ptr->name, name))
			return (ptr->name);
	}

	ptr = (SCACHE *) MyMalloc(sizeof(SCACHE));
	s_assert(0 != ptr);

	strlcpy(ptr->name, name, sizeof(ptr->name));

	ptr->next = scache_hash[hash_index];
	scache_hash[hash_index] = ptr;
	return ptr->name;
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
	SCACHE *scache_ptr;
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
				(strlen(scache_ptr->name) + sizeof(SCACHE *));

			scache_ptr = scache_ptr->next;
		}
	}
}
