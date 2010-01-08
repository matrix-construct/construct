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
 *  $Id: hash.c 3177 2007-02-01 00:19:14Z jilles $
 */

#include "stdinc.h"
#include "ircd_defs.h"
#include "s_conf.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "hash.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "msg.h"
#include "cache.h"
#include "s_newconf.h"

#define hash_cli_fd(x)	(x % CLI_FD_MAX)

static rb_dlink_list clientbyfdTable[U_MAX];

rb_dlink_list *clientTable;
rb_dlink_list *channelTable;
rb_dlink_list *idTable;
rb_dlink_list *resvTable;
rb_dlink_list *hostTable; 

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
	clientTable = rb_malloc(sizeof(rb_dlink_list) * U_MAX);
	idTable = rb_malloc(sizeof(rb_dlink_list) * U_MAX);
	channelTable = rb_malloc(sizeof(rb_dlink_list) * CH_MAX);
	hostTable = rb_malloc(sizeof(rb_dlink_list) * HOST_MAX);
	resvTable = rb_malloc(sizeof(rb_dlink_list) * R_MAX);
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
	if (bits < 32)
		h = ((h >> bits) ^ h) & ((1<<bits)-1);
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
	if (bits < 32)
		h = ((h >> bits) ^ h) & ((1<<bits)-1);
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
	if (bits < 32)
		h = ((h >> bits) ^ h) & ((1<<bits)-1);
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
	if (bits < 32)
		h = ((h >> bits) ^ h) & ((1<<bits)-1);
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
	rb_dlinkAddAlloc(client_p, &idTable[hashv]);
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
	rb_dlinkAddAlloc(client_p, &clientTable[hashv]);
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
	rb_dlinkAddAlloc(client_p, &hostTable[hashv]);
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
	rb_dlinkAddAlloc(aconf, &resvTable[hashv]);
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
	rb_dlinkFindDestroy(client_p, &idTable[hashv]);
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
	rb_dlinkFindDestroy(client_p, &clientTable[hashv]);
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
	rb_dlinkFindDestroy(chptr, &channelTable[hashv]);
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

	rb_dlinkFindDestroy(client_p, &hostTable[hashv]);
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

	rb_dlinkFindDestroy(aconf, &resvTable[hashv]);
}

/* find_id()
 *
 * finds a client entry from the id hash table
 */
struct Client *
find_id(const char *name)
{
	struct Client *target_p;
	rb_dlink_node *ptr;
	unsigned int hashv;

	if(EmptyString(name))
		return NULL;

	hashv = hash_id(name);

	RB_DLINK_FOREACH(ptr, idTable[hashv].head)
	{
		target_p = ptr->data;

		if(strcmp(name, target_p->id) == 0)
			return target_p;
	}

	return NULL;
}

/* find_client()
 *
 * finds a client/server entry from the client hash table
 */
struct Client *
find_client(const char *name)
{
	struct Client *target_p;
	rb_dlink_node *ptr;
	unsigned int hashv;

	s_assert(name != NULL);
	if(EmptyString(name))
		return NULL;

	/* hunting for an id, not a nick */
	if(IsDigit(*name))
		return (find_id(name));

	hashv = hash_nick(name);

	RB_DLINK_FOREACH(ptr, clientTable[hashv].head)
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
	rb_dlink_node *ptr;
	unsigned int hashv;

	s_assert(name != NULL);
	if(EmptyString(name))
		return NULL;

	hashv = hash_nick(name);

	RB_DLINK_FOREACH(ptr, clientTable[hashv].head)
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
	rb_dlink_node *ptr;
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

	RB_DLINK_FOREACH(ptr, clientTable[hashv].head)
	{
		target_p = ptr->data;

		if((IsServer(target_p) || IsMe(target_p)) &&
		   irccmp(name, target_p->name) == 0)
				return target_p;
	}

	return NULL;
}

/* find_hostname()
 *
 * finds a hostname rb_dlink list from the hostname hash table.
 * we return the full rb_dlink list, because you can have multiple
 * entries with the same hostname
 */
rb_dlink_node *
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
	rb_dlink_node *ptr;
	unsigned int hashv;

	s_assert(name != NULL);
	if(EmptyString(name))
		return NULL;

	hashv = hash_channel(name);

	RB_DLINK_FOREACH(ptr, channelTable[hashv].head)
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
	rb_dlink_node *ptr;
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

	RB_DLINK_FOREACH(ptr, channelTable[hashv].head)
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

	rb_dlinkAdd(chptr, &chptr->node, &global_channel_list);

	chptr->channelts = rb_current_time();	/* doesn't hurt to set it here */

	rb_dlinkAddAlloc(chptr, &channelTable[hashv]);

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
	rb_dlink_node *ptr;
	unsigned int hashv;

	s_assert(name != NULL);
	if(EmptyString(name))
		return NULL;

	hashv = hash_resv(name);

	RB_DLINK_FOREACH(ptr, resvTable[hashv].head)
	{
		aconf = ptr->data;

		if(!irccmp(name, aconf->host))
		{
			aconf->port++;
			return aconf;
		}
	}

	return NULL;
}

void
clear_resv_hash(void)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	int i;

	HASH_WALK_SAFE(i, R_MAX, ptr, next_ptr, resvTable)
	{
		aconf = ptr->data;

		/* skip temp resvs */
		if(aconf->hold)
			continue;

		free_conf(ptr->data);
		rb_dlinkDestroy(ptr, &resvTable[i]);
	}
	HASH_WALK_END
}

void
add_to_cli_fd_hash(struct Client *client_p)
{
	rb_dlinkAddAlloc(client_p, &clientbyfdTable[hash_cli_fd(rb_get_fd(client_p->localClient->F))]);
}


void
del_from_cli_fd_hash(struct Client *client_p)
{
	unsigned int hashv;
	hashv = hash_cli_fd(rb_get_fd(client_p->localClient->F));
	rb_dlinkFindDestroy(client_p, &clientbyfdTable[hashv]);
}

struct Client *
find_cli_fd_hash(int fd)
{
	struct Client *target_p;
	rb_dlink_node *ptr;
	unsigned int hashv;
	hashv = hash_cli_fd(fd);
	RB_DLINK_FOREACH(ptr, clientbyfdTable[hashv].head)
	{
		target_p = ptr->data;
		if(rb_get_fd(target_p->localClient->F) == fd)
			return target_p;
	}
	return  NULL;	
}

static void
output_hash(struct Client *source_p, const char *name, int length, int *counts, int deepest)
{
	unsigned long total = 0;
	int i;
	char buf[128];

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			"B :%s Hash Statistics", name);

	snprintf(buf, sizeof buf, "%.3f%%",
			(float) ((counts[0]*100) / (float) length));
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			"B :Size: %d Empty: %d (%s)",
			length, counts[0], buf);

	for(i = 1; i < 11; i++)
	{
		total += (counts[i] * i);
	}

	/* dont want to divide by 0! --fl */
	if(counts[0] != length)
	{
		snprintf(buf, sizeof buf, "%.3f/%.3f",
				(float) (total / (length - counts[0])),
				(float) (total / length));
		sendto_one_numeric(source_p, RPL_STATSDEBUG,
				"B :Average depth: %s Highest depth: %d",
				buf, deepest);
	}

	for(i = 0; i < 11; i++)
	{
		sendto_one_numeric(source_p, RPL_STATSDEBUG,
				"B :Nodes with %d entries: %d",
				i, counts[i]);
	}
}
	

static void
count_hash(struct Client *source_p, rb_dlink_list *table, int length, const char *name)
{
	int counts[11];
	int deepest = 0;
	int i;

	memset(counts, 0, sizeof(counts));
	
	for(i = 0; i < length; i++)
	{
		if(rb_dlink_list_length(&table[i]) >= 10)
			counts[10]++;
		else
			counts[rb_dlink_list_length(&table[i])]++;

		if(rb_dlink_list_length(&table[i]) > deepest)
			deepest = rb_dlink_list_length(&table[i]);
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
