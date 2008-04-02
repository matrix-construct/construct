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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 *  $Id: hash.c 25119 2008-03-13 16:57:05Z androsyn $
 */

#include "stdinc.h"
#include "s_conf.h"
#include "channel.h"
#include "client.h"
#include "hash.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "cache.h"
#include "s_newconf.h"

#define hash_nick(x) (fnv_hash_upper((const unsigned char *)(x), U_MAX_BITS, 0))
#define hash_id(x) (fnv_hash((const unsigned char *)(x), U_MAX_BITS, 0))
#define hash_channel(x) (fnv_hash_upper_len((const unsigned char *)(x), CH_MAX_BITS, 30))
#define hash_hostname(x) (fnv_hash_upper_len((const unsigned char *)(x), HOST_MAX_BITS, 30))
#define hash_resv(x) (fnv_hash_upper_len((const unsigned char *)(x), R_MAX_BITS, 30))
#define hash_cli_fd(x)	(x % CLI_FD_MAX)


static rb_dlink_list clientbyfdTable[U_MAX];
static rb_dlink_list clientTable[U_MAX];
static rb_dlink_list channelTable[CH_MAX];
static rb_dlink_list idTable[U_MAX];
rb_dlink_list resvTable[R_MAX];
static rb_dlink_list hostTable[HOST_MAX];
static rb_dlink_list helpTable[HELP_MAX];
rb_dlink_list ndTable[U_MAX];

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
	/* nothing to do here */
}


uint32_t
fnv_hash_upper(const unsigned char *s, unsigned int bits, unsigned int unused)
{
	uint32_t h = FNV1_32_INIT;
	bits = 32 - bits;
	while (*s)
	{
		h ^= ToUpper(*s++);
		h += (h<<1) + (h<<4) + (h<<7) + (h << 8) + (h << 24);
	}
	h = (h >> bits) ^ (h & ((2^bits)-1));
	return h;
}

uint32_t
fnv_hash(const unsigned char *s, unsigned int bits, unsigned int unused)
{
	uint32_t h = FNV1_32_INIT;
	bits = 32 - bits;
	while (*s)
	{
		h ^= *s++;
		h += (h<<1) + (h<<4) + (h<<7) + (h << 8) + (h << 24);
	}
	h = (h >> bits) ^ (h & ((2^bits)-1));
	return h;
}

uint32_t
fnv_hash_len(const unsigned char *s, unsigned int bits, unsigned int len)
{
	uint32_t h = FNV1_32_INIT;
	bits = 32 - bits;
	const unsigned char *x = s + len;
	while (*s && s < x)
	{
		h ^= *s++;
		h += (h<<1) + (h<<4) + (h<<7) + (h << 8) + (h << 24);
	}
	h = (h >> bits) ^ (h & ((2^bits)-1));
	return h;
}

uint32_t
fnv_hash_upper_len(const unsigned char *s, unsigned int bits, unsigned int len)
{
	uint32_t h = FNV1_32_INIT;
	bits = 32 - bits;
	const unsigned char *x = s + len;
	while (*s && s < x)
	{
		h ^= ToUpper(*s++);
		h += (h<<1) + (h<<4) + (h<<7) + (h << 8) + (h << 24);
	}
	h = (h >> bits) ^ (h & ((2^bits)-1));
	return h;
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

static struct _hash_function
{
	uint32_t (*func) (unsigned const char *, unsigned int, unsigned int);
	rb_dlink_list *table;
	unsigned int hashbits;
	unsigned int hashlen;
} hash_function[] = {
	{ fnv_hash_upper,	clientTable,	U_MAX_BITS,	0	},
	{ fnv_hash,		idTable,	U_MAX_BITS,	0	},
	{ fnv_hash_upper_len,	channelTable,	CH_MAX_BITS,	30	},
	{ fnv_hash_upper_len,	hostTable,	HOST_MAX_BITS,	30	},
	{ fnv_hash_upper_len,	resvTable,	R_MAX_BITS,	30	}
};

void
add_to_hash(hash_type type, const char *hashindex, void *pointer)
{
	rb_dlink_list *table = hash_function[type].table;
	unsigned int hashv;

	if(EmptyString(hashindex) || (pointer == NULL))
		return;

	hashv = (hash_function[type].func)((const unsigned char *) hashindex, 
					hash_function[type].hashbits, 
					hash_function[type].hashlen);
//	rb_dlinkAddAlloc(pointer, &hash_function[type].table[hashv]);
	rb_dlinkAddAlloc(pointer, &table[hashv]);
}

void
del_from_hash(hash_type type, const char *hashindex, void *pointer)
{
	rb_dlink_list *table = hash_function[type].table;
	unsigned int hashv;

	if(EmptyString(hashindex) || (pointer == NULL))
		return;

	hashv = (hash_function[type].func)((const unsigned char *) hashindex,
					hash_function[type].hashbits,
					hash_function[type].hashlen);
	rb_dlinkFindDestroy(pointer, &table[hashv]);
}

void
add_to_help_hash(const char *name, struct cachefile *hptr)
{
	unsigned int hashv;

	if(EmptyString(name) || hptr == NULL)
		return;

	hashv = hash_help(name);
	rb_dlinkAddAlloc(hptr, &helpTable[hashv]);
}

void
add_to_nd_hash(const char *name, struct nd_entry *nd)
{
	nd->hashv = hash_nick(name);
	rb_dlinkAdd(nd, &nd->hnode, &ndTable[nd->hashv]);
}

void
clear_help_hash(void)
{
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	int i;

	HASH_WALK_SAFE(i, HELP_MAX, ptr, next_ptr, helpTable)
	{
		free_cachefile(ptr->data);
		rb_dlinkDestroy(ptr, &helpTable[i]);
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
	rb_strlcpy(buf, name, sizeof(buf));

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

	/* wasnt found, look for a masked server */
	return hash_find_masked_server(source_p, name);
}

/* find_hostname()
 *
 * finds a hostname dlink list from the hostname hash table.
 * we return the full dlink list, because you can have multiple
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
		if(IsServer(client_p))
		{
			sendto_realops_flags(UMODE_DEBUG, L_ALL,
					     "*** Long channel name from %s (%d > %d): %s",
					     client_p->name, len, CHANNELLEN, s);
		}
		len = CHANNELLEN;
		s = LOCAL_COPY_N(s, CHANNELLEN);
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

struct cachefile *
hash_find_help(const char *name, int flags)
{
	struct cachefile *hptr;
	rb_dlink_node *ptr;
	unsigned int hashv;

	if(EmptyString(name))
		return NULL;

	hashv = hash_help(name);

	RB_DLINK_FOREACH(ptr, helpTable[hashv].head)
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
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	int i;

	HASH_WALK_SAFE(i, R_MAX, ptr, next_ptr, resvTable)
	{
		aconf = ptr->data;

		/* skip temp resvs */
		if(aconf->flags & CONF_FLAGS_TEMPORARY)
			continue;

		free_conf(ptr->data);
		rb_dlinkDestroy(ptr, &resvTable[i]);
	}
	HASH_WALK_END
}

struct nd_entry *
hash_find_nd(const char *name)
{
	struct nd_entry *nd;
	rb_dlink_node *ptr;
	unsigned int hashv;

	if(EmptyString(name))
		return NULL;

	hashv = hash_nick(name);

	RB_DLINK_FOREACH(ptr, ndTable[hashv].head)
	{
		nd = ptr->data;

		if(!irccmp(name, nd->name))
			return nd;
	}

	return NULL;
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
	char buf[128];
	int i;

	sendto_one_numeric(source_p, RPL_STATSDEBUG, "B :%s Hash Statistics", name);

	/* rb_snprintf which sendto_one_* uses doesn't support float formats */
#ifdef HAVE_SNPRINTF
	snprintf(buf, sizeof(buf), 
#else
	sprintf(buf, 
#endif
		"%.3f%%", (float) ((counts[0]*100) / (float) length));
	sendto_one_numeric(source_p, RPL_STATSDEBUG, "B :Size: %d Empty: %d (%s)",
			length, counts[0], buf);

	for(i = 1; i < 11; i++)
	{
		total += (counts[i] * i);
	}

	/* dont want to divide by 0! --fl */
	if(counts[0] != length) 
	{
#ifdef HAVE_SNPRINTF
		snprintf(buf, sizeof(buf),
#else
		sprintf(buf,
#endif
			"%.3f%%/%.3f%%", (float) (total / (length - counts[0])), 
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
	count_hash(source_p, channelTable, CH_MAX, "Channel");
	sendto_one_numeric(source_p, RPL_STATSDEBUG, "B :--");
	count_hash(source_p, clientTable, U_MAX, "Client");
	sendto_one_numeric(source_p, RPL_STATSDEBUG, "B :--");
	count_hash(source_p, idTable, U_MAX, "ID");
	sendto_one_numeric(source_p, RPL_STATSDEBUG, "B :--");
	count_hash(source_p, hostTable, HOST_MAX, "Hostname");
	sendto_one_numeric(source_p, RPL_STATSDEBUG, "B :--");
	count_hash(source_p, clientbyfdTable, CLI_FD_MAX, "Client by FD");
}	
