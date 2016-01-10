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
#include "s_assert.h"
#include "irc_dictionary.h"
#include "irc_radixtree.h"

rb_dlink_list *hostTable;

struct Dictionary *client_connid_tree = NULL;
struct Dictionary *client_zconnid_tree = NULL;
struct irc_radixtree *client_id_tree = NULL;
struct irc_radixtree *client_name_tree = NULL;

struct irc_radixtree *channel_tree = NULL;
struct irc_radixtree *resv_tree = NULL;

/*
 * look in whowas.c for the missing ...[WW_MAX]; entry
 */

/* init_hash()
 *
 * clears the various hashtables
 */
void
init_hash(void)
{
	hostTable = rb_malloc(sizeof(rb_dlink_list) * HOST_MAX);

	client_connid_tree = irc_dictionary_create("client connid", irc_uint32cmp);
	client_zconnid_tree = irc_dictionary_create("client zconnid", irc_uint32cmp);
	client_id_tree = irc_radixtree_create("client id", NULL);
	client_name_tree = irc_radixtree_create("client name", irc_radixtree_irccasecanon);

	channel_tree = irc_radixtree_create("channel", irc_radixtree_irccasecanon);
	resv_tree = irc_radixtree_create("resv", irc_radixtree_irccasecanon);
}

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

/* add_to_id_hash()
 *
 * adds an entry to the id hash table
 */
void
add_to_id_hash(const char *name, struct Client *client_p)
{
	if(EmptyString(name) || (client_p == NULL))
		return;

	irc_radixtree_add(client_id_tree, name, client_p);
}

/* add_to_client_hash()
 *
 * adds an entry (client/server) to the client hash table
 */
void
add_to_client_hash(const char *name, struct Client *client_p)
{
	s_assert(name != NULL);
	s_assert(client_p != NULL);
	if(EmptyString(name) || (client_p == NULL))
		return;

	irc_radixtree_add(client_name_tree, name, client_p);
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
	s_assert(!EmptyString(name));
	s_assert(aconf != NULL);
	if(EmptyString(name) || aconf == NULL)
		return;

	irc_radixtree_add(resv_tree, name, aconf);
}

/* del_from_id_hash()
 *
 * removes an id from the id hash table
 */
void
del_from_id_hash(const char *id, struct Client *client_p)
{
	s_assert(id != NULL);
	s_assert(client_p != NULL);
	if(EmptyString(id) || client_p == NULL)
		return;

	irc_radixtree_delete(client_id_tree, id);
}

/* del_from_client_hash()
 *
 * removes a client/server from the client hash table
 */
void
del_from_client_hash(const char *name, struct Client *client_p)
{
	/* no s_asserts, this can happen when removing a client that
	 * is unregistered.
	 */
	if(EmptyString(name) || client_p == NULL)
		return;

	irc_radixtree_delete(client_name_tree, name);
}

/* del_from_channel_hash()
 *
 * removes a channel from the channel hash table
 */
void
del_from_channel_hash(const char *name, struct Channel *chptr)
{
	s_assert(name != NULL);
	s_assert(chptr != NULL);

	if(EmptyString(name) || chptr == NULL)
		return;

	irc_radixtree_delete(channel_tree, name);
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
	s_assert(name != NULL);
	s_assert(aconf != NULL);
	if(EmptyString(name) || aconf == NULL)
		return;

	irc_radixtree_delete(resv_tree, name);
}

/* find_id()
 *
 * finds a client entry from the id hash table
 */
struct Client *
find_id(const char *name)
{
	if(EmptyString(name))
		return NULL;

	return irc_radixtree_retrieve(client_id_tree, name);
}

/* find_client()
 *
 * finds a client/server entry from the client hash table
 */
struct Client *
find_client(const char *name)
{
	s_assert(name != NULL);
	if(EmptyString(name))
		return NULL;

	/* hunting for an id, not a nick */
	if(IsDigit(*name))
		return (find_id(name));

	return irc_radixtree_retrieve(client_name_tree, name);
}

/* find_named_client()
 *
 * finds a client/server entry from the client hash table
 */
struct Client *
find_named_client(const char *name)
{
	s_assert(name != NULL);
	if(EmptyString(name))
		return NULL;

	return irc_radixtree_retrieve(client_name_tree, name);
}

/* find_server()
 *
 * finds a server from the client hash table
 */
struct Client *
find_server(struct Client *source_p, const char *name)
{
	struct Client *target_p;

	if(EmptyString(name))
		return NULL;

	if((source_p == NULL || !MyClient(source_p)) &&
	   IsDigit(*name) && strlen(name) == 3)
	{
		target_p = find_id(name);
      		return(target_p);
	}

	target_p = irc_radixtree_retrieve(client_name_tree, name);
	if (target_p != NULL)
	{
		if(IsServer(target_p) || IsMe(target_p))
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
	s_assert(name != NULL);
	if(EmptyString(name))
		return NULL;

	return irc_radixtree_retrieve(channel_tree, name);
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

	chptr = irc_radixtree_retrieve(channel_tree, s);
	if (chptr != NULL)
	{
		if (isnew != NULL)
			*isnew = 0;
		return chptr;
	}

	if(isnew != NULL)
		*isnew = 1;

	chptr = allocate_channel(s);
	chptr->channelts = rb_current_time();	/* doesn't hurt to set it here */

	rb_dlinkAdd(chptr, &chptr->node, &global_channel_list);
	irc_radixtree_add(channel_tree, chptr->chname, chptr);

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

	s_assert(name != NULL);
	if(EmptyString(name))
		return NULL;

	aconf = irc_radixtree_retrieve(resv_tree, name);
	if (aconf != NULL)
	{
		aconf->port++;
		return aconf;
	}

	return NULL;
}

void
clear_resv_hash(void)
{
	struct ConfItem *aconf;
	struct irc_radixtree_iteration_state iter;

	IRC_RADIXTREE_FOREACH(aconf, &iter, resv_tree)
	{
		/* skip temp resvs */
		if(aconf->hold)
			continue;

		irc_radixtree_delete(resv_tree, aconf->host);
		free_conf(aconf);
	}
}

void
add_to_zconnid_hash(struct Client *client_p)
{
	irc_dictionary_add(client_zconnid_tree, IRC_UINT_TO_POINTER(client_p->localClient->zconnid), client_p);
}

void
del_from_zconnid_hash(struct Client *client_p)
{
	irc_dictionary_delete(client_zconnid_tree, IRC_UINT_TO_POINTER(client_p->localClient->zconnid));
}

void
add_to_cli_connid_hash(struct Client *client_p)
{
	irc_dictionary_add(client_connid_tree, IRC_UINT_TO_POINTER(client_p->localClient->connid), client_p);
}

void
del_from_cli_connid_hash(struct Client *client_p)
{
	irc_dictionary_delete(client_connid_tree, IRC_UINT_TO_POINTER(client_p->localClient->connid));
}

struct Client *
find_cli_connid_hash(uint32_t connid)
{
	struct Client *target_p;

	target_p = irc_dictionary_retrieve(client_connid_tree, IRC_UINT_TO_POINTER(connid));
	if (target_p != NULL)
		return target_p;

	target_p = irc_dictionary_retrieve(client_zconnid_tree, IRC_UINT_TO_POINTER(connid));
	if (target_p != NULL)
		return target_p;

	return NULL;
}

static void
output_hash(struct Client *source_p, const char *name, int length, int *counts, unsigned long deepest)
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
				"B :Average depth: %s Highest depth: %lu",
				buf, deepest);
	}
}


static void
count_hash(struct Client *source_p, rb_dlink_list *table, int length, const char *name)
{
	int counts[11];
	unsigned long deepest = 0;
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
	count_hash(source_p, hostTable, HOST_MAX, "Hostname");
}
