/*
 *  ircd-ratbox: A slightly useful ircd.
 *  hash.c: Maintains hashtables.
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
 *  $Id: hash.c 1321 2006-05-13 23:49:14Z nenolod $
 */

#include "stdinc.h"
#include "ircd_defs.h"
#include "tools.h"
#include "s_conf.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "hash.h"
#include "irc_string.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "memory.h"
#include "msg.h"
#include "cache.h"
#include "s_newconf.h"

dlink_list *clientTable;
dlink_list *channelTable;
dlink_list *idTable;
dlink_list *resvTable;
dlink_list *hostTable;
dlink_list *helpTable;
dlink_list *ndTable;

/*
 * look in whowas.c for the missing ...[WW_MAX]; entry
 */

/*
 * Hashing.
 *
 *   The server uses a chained hash table to provide quick and efficient
 * hash table maintenance (providing the hash function works evenly over
 * the input range).  The hash table is thus not susceptible to problems
 * of filling all the buckets or the need to rehash.
 *    It is expected that the hash table would look something like this
 * during use:
 *                   +-----+    +-----+    +-----+   +-----+
 *                ---| 224 |----| 225 |----| 226 |---| 227 |---
 *                   +-----+    +-----+    +-----+   +-----+
 *                      |          |          |
 *                   +-----+    +-----+    +-----+
 *                   |  A  |    |  C  |    |  D  |
 *                   +-----+    +-----+    +-----+
 *                      |
 *                   +-----+
 *                   |  B  |
 *                   +-----+
 *
 * A - GOPbot, B - chang, C - hanuaway, D - *.mu.OZ.AU
 *
 * The order shown above is just one instant of the server. 
 *
 *
 * The hash functions currently used are based Fowler/Noll/Vo hashes
 * which work amazingly well and have a extremely low collision rate
 * For more info see http://www.isthe.com/chongo/tech/comp/fnv/index.html
 *
 * 
 */

/* init_hash()
 *
 * clears the various hashtables
 */
void
init_hash(void)
{
	clientTable = MyMalloc(sizeof(dlink_list) * U_MAX);
	idTable = MyMalloc(sizeof(dlink_list) * U_MAX);
	ndTable = MyMalloc(sizeof(dlink_list) * U_MAX);
	channelTable = MyMalloc(sizeof(dlink_list) * CH_MAX);
	hostTable = MyMalloc(sizeof(dlink_list) * HOST_MAX);
	resvTable = MyMalloc(sizeof(dlink_list) * R_MAX);
	helpTable = MyMalloc(sizeof(dlink_list) * HELP_MAX);
}

#ifndef RICER_HASHING
u_int32_t
fnv_hash_upper(const unsigned char *s, int bits)
{
 	u_int32_t h = FNV1_32_INIT;

	while (*s)
	{
         	h ^= ToUpper(*s++);
		h += (h<<1) + (h<<4) + (h<<7) + (h << 8) + (h << 24);
	}
        h = (h >> bits) ^ (h & ((2^bits)-1));
	return h;
}

u_int32_t
fnv_hash(const unsigned char *s, int bits)
{
 	u_int32_t h = FNV1_32_INIT;

	while (*s)
	{
		h ^= *s++;
		h += (h<<1) + (h<<4) + (h<<7) + (h << 8) + (h << 24);
	}
        h = (h >> bits) ^ (h & ((2^bits)-1));
	return h;
}

u_int32_t
fnv_hash_len(const unsigned char *s, int bits, int len)
{
 	u_int32_t h = FNV1_32_INIT;
	const unsigned char *x = s + len;
	while (*s && s < x)
	{
		h ^= *s++;
		h += (h<<1) + (h<<4) + (h<<7) + (h << 8) + (h << 24);
	}
        h = (h >> bits) ^ (h & ((2^bits)-1));
	return h;
}

u_int32_t
fnv_hash_upper_len(const unsigned char *s, int bits, int len)
{
 	u_int32_t h = FNV1_32_INIT;
	const unsigned char *x = s + len;
	while (*s && s < x)
	{
         	h ^= ToUpper(*s++);
		h += (h<<1) + (h<<4) + (h<<7) + (h << 8) + (h << 24);
	}
        h = (h >> bits) ^ (h & ((2^bits)-1));
	return h;
}
#endif

/* hash_nick()
 *
 * hashes a nickname, first converting to lowercase
 */
static u_int32_t
hash_nick(const char *name)
{
	return fnv_hash_upper((const unsigned char *) name, U_MAX_BITS);
}

/* hash_id()
 *
 * hashes an id, case is kept
 */
static u_int32_t
hash_id(const char *name)
{
	return fnv_hash((const unsigned char *) name, U_MAX_BITS);
}

/* hash_channel()
 *
 * hashes a channel name, based on first 30 chars only for efficiency
 */
static u_int32_t
hash_channel(const char *name)
{
	return fnv_hash_upper_len((const unsigned char *) name, CH_MAX_BITS, 30);
}

/* hash_hostname()
 *
 * hashes a hostname, based on first 30 chars only, as thats likely to
 * be more dynamic than rest.
 */
static u_int32_t
hash_hostname(const char *name)
{
	return fnv_hash_upper_len((const unsigned char *) name, HOST_MAX_BITS, 30);
}

/* hash_resv()
 *
 * hashes a resv channel name, based on first 30 chars only
 */
static u_int32_t
hash_resv(const char *name)
{
	return fnv_hash_upper_len((const unsigned char *) name, R_MAX_BITS, 30);
}

static unsigned int
hash_help(const char *name)
{
	unsigned int h = 0;

	while(*name)
	{
		h += (unsigned int) (ToLower(*name++) & 0xDF);
	}

	return (h % HELP_MAX);
}

/* add_to_id_hash()
 *
 * adds an entry to the id hash table
 */
void
add_to_id_hash(const char *name, struct Client *client_p)
{
	unsigned int hashv;

	if(EmptyString(name) || (client_p == NULL))
		return;

	hashv = hash_id(name);
	dlinkAddAlloc(client_p, &idTable[hashv]);
}

/* add_to_client_hash()
 *
 * adds an entry (client/server) to the client hash table
 */
void
add_to_client_hash(const char *name, struct Client *client_p)
{
	unsigned int hashv;

	s_assert(name != NULL);
	s_assert(client_p != NULL);
	if(EmptyString(name) || (client_p == NULL))
		return;

	hashv = hash_nick(name);
	dlinkAddAlloc(client_p, &clientTable[hashv]);
}

/* add_to_hostname_hash()
 *
 * adds a client entry to the hostname hash table
 */
void
add_to_hostname_hash(const char *hostname, struct Client *client_p)
{
	unsigned int hashv;

	s_assert(hostname != NULL);
	s_assert(client_p != NULL);
	if(EmptyString(hostname) || (client_p == NULL))
		return;

	hashv = hash_hostname(hostname);
	dlinkAddAlloc(client_p, &hostTable[hashv]);
}

/* add_to_resv_hash()
 *
 * adds a resv channel entry to the resv hash table
 */
void
add_to_resv_hash(const char *name, struct ConfItem *aconf)
{
	unsigned int hashv;

	s_assert(!EmptyString(name));
	s_assert(aconf != NULL);
	if(EmptyString(name) || aconf == NULL)
		return;

	hashv = hash_resv(name);
	dlinkAddAlloc(aconf, &resvTable[hashv]);
}

void
add_to_help_hash(const char *name, struct cachefile *hptr)
{
	unsigned int hashv;

	if(EmptyString(name) || hptr == NULL)
		return;

	hashv = hash_help(name);
	dlinkAddAlloc(hptr, &helpTable[hashv]);
}

void
add_to_nd_hash(const char *name, struct nd_entry *nd)
{
	nd->hashv = hash_nick(name);
	dlinkAdd(nd, &nd->hnode, &ndTable[nd->hashv]);
}

/* del_from_id_hash()
 *
 * removes an id from the id hash table
 */
void
del_from_id_hash(const char *id, struct Client *client_p)
{
	unsigned int hashv;

	s_assert(id != NULL);
	s_assert(client_p != NULL);
	if(EmptyString(id) || client_p == NULL)
		return;

	hashv = hash_id(id);
	dlinkFindDestroy(client_p, &idTable[hashv]);
}

/* del_from_client_hash()
 *
 * removes a client/server from the client hash table
 */
void
del_from_client_hash(const char *name, struct Client *client_p)
{
	unsigned int hashv;

	/* no s_asserts, this can happen when removing a client that
	 * is unregistered.
	 */
	if(EmptyString(name) || client_p == NULL)
		return;

	hashv = hash_nick(name);
	dlinkFindDestroy(client_p, &clientTable[hashv]);
}

/* del_from_channel_hash()
 * 
 * removes a channel from the channel hash table
 */
void
del_from_channel_hash(const char *name, struct Channel *chptr)
{
	unsigned int hashv;

	s_assert(name != NULL);
	s_assert(chptr != NULL);

	if(EmptyString(name) || chptr == NULL)
		return;

	hashv = hash_channel(name);
	dlinkFindDestroy(chptr, &channelTable[hashv]);
}

/* del_from_hostname_hash()
 *
 * removes a client entry from the hostname hash table
 */
void
del_from_hostname_hash(const char *hostname, struct Client *client_p)
{
	unsigned int hashv;

	if(hostname == NULL || client_p == NULL)
		return;

	hashv = hash_hostname(hostname);

	dlinkFindDestroy(client_p, &hostTable[hashv]);
}

/* del_from_resv_hash()
 *
 * removes a resv entry from the resv hash table
 */
void
del_from_resv_hash(const char *name, struct ConfItem *aconf)
{
	unsigned int hashv;

	s_assert(name != NULL);
	s_assert(aconf != NULL);
	if(EmptyString(name) || aconf == NULL)
		return;

	hashv = hash_resv(name);

	dlinkFindDestroy(aconf, &resvTable[hashv]);
}

void
clear_help_hash(void)
{
	dlink_node *ptr;
	dlink_node *next_ptr;
	int i;

	HASH_WALK_SAFE(i, HELP_MAX, ptr, next_ptr, helpTable)
	{
		free_cachefile(ptr->data);
		dlinkDestroy(ptr, &helpTable[i]);
	}
	HASH_WALK_END
}

/* find_id()
 *
 * finds a client entry from the id hash table
 */
struct Client *
find_id(const char *name)
{
	struct Client *target_p;
	dlink_node *ptr;
	unsigned int hashv;

	if(EmptyString(name))
		return NULL;

	hashv = hash_id(name);

	DLINK_FOREACH(ptr, idTable[hashv].head)
	{
		target_p = ptr->data;

		if(strcmp(name, target_p->id) == 0)
			return target_p;
	}

	return NULL;
}

/* hash_find_masked_server()
 * 
 * Whats happening in this next loop ? Well, it takes a name like
 * foo.bar.edu and proceeds to earch for *.edu and then *.bar.edu.
 * This is for checking full server names against masks although
 * it isnt often done this way in lieu of using matches().
 *
 * Rewrote to do *.bar.edu first, which is the most likely case,
 * also made const correct
 * --Bleep
 */
static struct Client *
hash_find_masked_server(struct Client *source_p, const char *name)
{
	char buf[HOSTLEN + 1];
	char *p = buf;
	char *s;
	struct Client *server;

	if('*' == *name || '.' == *name)
		return NULL;

	/* copy it across to give us a buffer to work on */
	strlcpy(buf, name, sizeof(buf));

	while ((s = strchr(p, '.')) != 0)
	{
		*--s = '*';
		/*
		 * Dont need to check IsServer() here since nicknames cant
		 * have *'s in them anyway.
		 */
		if((server = find_server(source_p, s)))
			return server;
		p = s + 2;
	}

	return NULL;
}

/* find_any_client()
 *
 * finds a client/server/masked server entry from the hash
 */
struct Client *
find_any_client(const char *name)
{
	struct Client *target_p;
	dlink_node *ptr;
	unsigned int hashv;

	s_assert(name != NULL);
	if(EmptyString(name))
		return NULL;

	/* hunting for an id, not a nick */
	if(IsDigit(*name))
		return (find_id(name));

	hashv = hash_nick(name);

	DLINK_FOREACH(ptr, clientTable[hashv].head)
	{
		target_p = ptr->data;

		if(irccmp(name, target_p->name) == 0)
			return target_p;
	}

	/* wasnt found, look for a masked server */
	return hash_find_masked_server(NULL, name);
}

/* find_client()
 *
 * finds a client/server entry from the client hash table
 */
struct Client *
find_client(const char *name)
{
	struct Client *target_p;
	dlink_node *ptr;
	unsigned int hashv;

	s_assert(name != NULL);
	if(EmptyString(name))
		return NULL;

	/* hunting for an id, not a nick */
	if(IsDigit(*name))
		return (find_id(name));

	hashv = hash_nick(name);

	DLINK_FOREACH(ptr, clientTable[hashv].head)
	{
		target_p = ptr->data;

		if(irccmp(name, target_p->name) == 0)
			return target_p;
	}

	return NULL;
}

/* find_named_client()
 *
 * finds a client/server entry from the client hash table
 */
struct Client *
find_named_client(const char *name)
{
	struct Client *target_p;
	dlink_node *ptr;
	unsigned int hashv;

	s_assert(name != NULL);
	if(EmptyString(name))
		return NULL;

	hashv = hash_nick(name);

	DLINK_FOREACH(ptr, clientTable[hashv].head)
	{
		target_p = ptr->data;

		if(irccmp(name, target_p->name) == 0)
			return target_p;
	}

	return NULL;
}

/* find_server()
 *
 * finds a server from the client hash table
 */
struct Client *
find_server(struct Client *source_p, const char *name)
{
	struct Client *target_p;
	dlink_node *ptr;
	unsigned int hashv;
  
	if(EmptyString(name))
		return NULL;

	if((source_p == NULL || !MyClient(source_p)) && 
	   IsDigit(*name) && strlen(name) == 3)
	{
		target_p = find_id(name);
      		return(target_p);
	}

	hashv = hash_nick(name);

	DLINK_FOREACH(ptr, clientTable[hashv].head)
	{
		target_p = ptr->data;

		if((IsServer(target_p) || IsMe(target_p)) &&
		   irccmp(name, target_p->name) == 0)
				return target_p;
	}

	/* wasnt found, look for a masked server */
	return hash_find_masked_server(source_p, name);
}

/* find_hostname()
 *
 * finds a hostname dlink list from the hostname hash table.
 * we return the full dlink list, because you can have multiple
 * entries with the same hostname
 */
dlink_node *
find_hostname(const char *hostname)
{
	unsigned int hashv;

	if(EmptyString(hostname))
		return NULL;

	hashv = hash_hostname(hostname);

	return hostTable[hashv].head;
}

/* find_channel()
 *
 * finds a channel from the channel hash table
 */
struct Channel *
find_channel(const char *name)
{
	struct Channel *chptr;
	dlink_node *ptr;
	unsigned int hashv;

	s_assert(name != NULL);
	if(EmptyString(name))
		return NULL;

	hashv = hash_channel(name);

	DLINK_FOREACH(ptr, channelTable[hashv].head)
	{
		chptr = ptr->data;

		if(irccmp(name, chptr->chname) == 0)
			return chptr;
	}

	return NULL;
}

/*
 * get_or_create_channel
 * inputs       - client pointer
 *              - channel name
 *              - pointer to int flag whether channel was newly created or not
 * output       - returns channel block or NULL if illegal name
 *		- also modifies *isnew
 *
 *  Get Channel block for chname (and allocate a new channel
 *  block, if it didn't exist before).
 */
struct Channel *
get_or_create_channel(struct Client *client_p, const char *chname, int *isnew)
{
	struct Channel *chptr;
	dlink_node *ptr;
	unsigned int hashv;
	int len;
	const char *s = chname;

	if(EmptyString(s))
		return NULL;

	len = strlen(s);
	if(len > CHANNELLEN)
	{
		char *t;
		if(IsServer(client_p))
		{
			sendto_realops_snomask(SNO_DEBUG, L_ALL,
					     "*** Long channel name from %s (%d > %d): %s",
					     client_p->name, len, CHANNELLEN, s);
		}
		len = CHANNELLEN;
		t = LOCAL_COPY(s);
		*(t + CHANNELLEN) = '\0';
		s = t;
	}

	hashv = hash_channel(s);

	DLINK_FOREACH(ptr, channelTable[hashv].head)
	{
		chptr = ptr->data;

		if(irccmp(s, chptr->chname) == 0)
		{
			if(isnew != NULL)
				*isnew = 0;
			return chptr;
		}
	}

	if(isnew != NULL)
		*isnew = 1;

	chptr = allocate_channel(s);

	dlinkAdd(chptr, &chptr->node, &global_channel_list);

	chptr->channelts = CurrentTime;	/* doesn't hurt to set it here */

	dlinkAddAlloc(chptr, &channelTable[hashv]);

	return chptr;
}

/* hash_find_resv()
 *
 * hunts for a resv entry in the resv hash table
 */
struct ConfItem *
hash_find_resv(const char *name)
{
	struct ConfItem *aconf;
	dlink_node *ptr;
	unsigned int hashv;

	s_assert(name != NULL);
	if(EmptyString(name))
		return NULL;

	hashv = hash_resv(name);

	DLINK_FOREACH(ptr, resvTable[hashv].head)
	{
		aconf = ptr->data;

		if(!irccmp(name, aconf->name))
		{
			aconf->port++;
			return aconf;
		}
	}

	return NULL;
}

struct cachefile *
hash_find_help(const char *name, int flags)
{
	struct cachefile *hptr;
	dlink_node *ptr;
	unsigned int hashv;

	if(EmptyString(name))
		return NULL;

	hashv = hash_help(name);

	DLINK_FOREACH(ptr, helpTable[hashv].head)
	{
		hptr = ptr->data;

		if((irccmp(name, hptr->name) == 0) &&
		   (hptr->flags & flags))
			return hptr;
	}

	return NULL;
}

void
clear_resv_hash(void)
{
	struct ConfItem *aconf;
	dlink_node *ptr;
	dlink_node *next_ptr;
	int i;

	HASH_WALK_SAFE(i, R_MAX, ptr, next_ptr, resvTable)
	{
		aconf = ptr->data;

		/* skip temp resvs */
		if(aconf->hold)
			continue;

		free_conf(ptr->data);
		dlinkDestroy(ptr, &resvTable[i]);
	}
	HASH_WALK_END
}

struct nd_entry *
hash_find_nd(const char *name)
{
	struct nd_entry *nd;
	dlink_node *ptr;
	unsigned int hashv;

	if(EmptyString(name))
		return NULL;

	hashv = hash_nick(name);

	DLINK_FOREACH(ptr, ndTable[hashv].head)
	{
		nd = ptr->data;

		if(!irccmp(name, nd->name))
			return nd;
	}

	return NULL;
}

static void
output_hash(struct Client *source_p, const char *name, int length, int *counts, int deepest)
{
	unsigned long total = 0;
	int i;

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			"B :%s Hash Statistics", name);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			"B :Size: %d Empty: %d (%.3f%%)",
			length, counts[0], 
			(float) ((counts[0]*100) / (float) length));

	for(i = 1; i < 11; i++)
	{
		total += (counts[i] * i);
	}

	/* dont want to divide by 0! --fl */
	if(counts[0] != length)
		sendto_one_numeric(source_p, RPL_STATSDEBUG,
				"B :Average depth: %.3f/%.3f Highest depth: %d",
				(float) (total / (length - counts[0])),
				(float) (total / length), deepest);

	for(i = 0; i < 11; i++)
	{
		sendto_one_numeric(source_p, RPL_STATSDEBUG,
				"B :Nodes with %d entries: %d",
				i, counts[i]);
	}
}
	

static void
count_hash(struct Client *source_p, dlink_list *table, int length, const char *name)
{
	int counts[11];
	int deepest = 0;
	int i;

	memset(counts, 0, sizeof(counts));
	
	for(i = 0; i < length; i++)
	{
		if(dlink_list_length(&table[i]) >= 10)
			counts[10]++;
		else
			counts[dlink_list_length(&table[i])]++;

		if(dlink_list_length(&table[i]) > deepest)
			deepest = dlink_list_length(&table[i]);
	}

	output_hash(source_p, name, length, counts, deepest);
}

void
hash_stats(struct Client *source_p)
{
	count_hash(source_p, channelTable, CH_MAX, "Channel");
	sendto_one_numeric(source_p, RPL_STATSDEBUG, "B :--");
	count_hash(source_p, clientTable, U_MAX, "Client");
	sendto_one_numeric(source_p, RPL_STATSDEBUG, "B :--");
	count_hash(source_p, idTable, U_MAX, "ID");
	sendto_one_numeric(source_p, RPL_STATSDEBUG, "B :--");
	count_hash(source_p, hostTable, HOST_MAX, "Hostname");
}	
