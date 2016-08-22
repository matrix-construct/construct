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
 */

namespace ircd {

rb_dictionary *client_connid_tree = NULL;
rb_radixtree *client_id_tree = NULL;
rb_radixtree *client_name_tree = NULL;

rb_radixtree *resv_tree = NULL;
rb_radixtree *hostname_tree = NULL;

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
	client_connid_tree = rb_dictionary_create("client connid", rb_uint32cmp);
	client_id_tree = rb_radixtree_create("client id", NULL);
	client_name_tree = rb_radixtree_create("client name", irccasecanon);

	resv_tree = rb_radixtree_create("resv", irccasecanon);

	hostname_tree = rb_radixtree_create("hostname", irccasecanon);
}

uint32_t
fnv_hash_upper(const unsigned char *s, int bits)
{
	uint32_t h = FNV1_32_INIT;

	while (*s)
	{
		h ^= rfc1459::toupper(*s++);
		h += (h<<1) + (h<<4) + (h<<7) + (h << 8) + (h << 24);
	}
	if (bits < 32)
		h = ((h >> bits) ^ h) & ((1<<bits)-1);
	return h;
}

uint32_t
fnv_hash(const unsigned char *s, int bits)
{
	uint32_t h = FNV1_32_INIT;

	while (*s)
	{
		h ^= *s++;
		h += (h<<1) + (h<<4) + (h<<7) + (h << 8) + (h << 24);
	}
	if (bits < 32)
		h = ((h >> bits) ^ h) & ((1<<bits)-1);
	return h;
}

uint32_t
fnv_hash_len(const unsigned char *s, int bits, int len)
{
	uint32_t h = FNV1_32_INIT;
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

uint32_t
fnv_hash_upper_len(const unsigned char *s, int bits, int len)
{
	uint32_t h = FNV1_32_INIT;
	const unsigned char *x = s + len;
	while (*s && s < x)
	{
		h ^= rfc1459::toupper(*s++);
		h += (h<<1) + (h<<4) + (h<<7) + (h << 8) + (h << 24);
	}
	if (bits < 32)
		h = ((h >> bits) ^ h) & ((1<<bits)-1);
	return h;
}

/* add_to_id_hash()
 *
 * adds an entry to the id hash table
 */
void
add_to_id_hash(const char *name, client::client *client_p)
{
	if(EmptyString(name) || (client_p == NULL))
		return;

	rb_radixtree_add(client_id_tree, name, client_p);
}

/* add_to_client_hash()
 *
 * adds an entry (client/server) to the client hash table
 */
void
add_to_client_hash(const char *name, client::client *client_p)
{
	s_assert(name != NULL);
	s_assert(client_p != NULL);
	if(EmptyString(name) || (client_p == NULL))
		return;

	rb_radixtree_add(client_name_tree, name, client_p);
}

/* add_to_hostname_hash()
 *
 * adds a client entry to the hostname hash table
 */
void
add_to_hostname_hash(const char *hostname, client::client *client_p)
{
	rb_dlink_list *list;

	s_assert(hostname != NULL);
	s_assert(client_p != NULL);
	if(EmptyString(hostname) || (client_p == NULL))
		return;

	list = (rb_dlink_list *)rb_radixtree_retrieve(hostname_tree, hostname);
	if (list != NULL)
	{
		rb_dlinkAddAlloc(client_p, list);
		return;
	}

	list = (rb_dlink_list *)rb_malloc(sizeof(*list));
	rb_radixtree_add(hostname_tree, hostname, list);
	rb_dlinkAddAlloc(client_p, list);
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

	rb_radixtree_add(resv_tree, name, aconf);
}

/* del_from_id_hash()
 *
 * removes an id from the id hash table
 */
void
del_from_id_hash(const char *id, client::client *client_p)
{
	s_assert(id != NULL);
	s_assert(client_p != NULL);
	if(EmptyString(id) || client_p == NULL)
		return;

	rb_radixtree_delete(client_id_tree, id);
}

/* del_from_client_hash()
 *
 * removes a client/server from the client hash table
 */
void
del_from_client_hash(const char *name, client::client *client_p)
{
	/* no s_asserts, this can happen when removing a client that
	 * is unregistered.
	 */
	if(EmptyString(name) || client_p == NULL)
		return;

	rb_radixtree_delete(client_name_tree, name);
}

/* del_from_hostname_hash()
 *
 * removes a client entry from the hostname hash table
 */
void
del_from_hostname_hash(const char *hostname, client::client *client_p)
{
	rb_dlink_list *list;

	if(hostname == NULL || client_p == NULL)
		return;

	list = (rb_dlink_list *)rb_radixtree_retrieve(hostname_tree, hostname);
	if (list == NULL)
		return;

	rb_dlinkFindDestroy(client_p, list);

	if (rb_dlink_list_length(list) == 0)
	{
		rb_radixtree_delete(hostname_tree, hostname);
		rb_free(list);
	}
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

	rb_radixtree_delete(resv_tree, name);
}

/* find_id()
 *
 * finds a client entry from the id hash table
 */
client::client *
find_id(const char *name)
{
	if(EmptyString(name))
		return NULL;

	return (client::client *)rb_radixtree_retrieve(client_id_tree, name);
}

/* find_client()
 *
 * finds a client/server entry from the client hash table
 */
client::client *
find_client(const char *name)
{
	s_assert(name != NULL);
	if(EmptyString(name))
		return NULL;

	/* hunting for an id, not a nick */
	if(rfc1459::is_digit(*name))
		return (find_id(name));

	return (client::client *)rb_radixtree_retrieve(client_name_tree, name);
}

/* find_named_client()
 *
 * finds a client/server entry from the client hash table
 */
client::client *
find_named_client(const char *name)
{
	s_assert(name != NULL);
	if(EmptyString(name))
		return NULL;

	return (client::client *)rb_radixtree_retrieve(client_name_tree, name);
}

/* find_server()
 *
 * finds a server from the client hash table
 */
client::client *
find_server(client::client *source_p, const char *name)
{
	client::client *target_p;

	if(EmptyString(name))
		return NULL;

	if((source_p == NULL || !MyClient(source_p)) &&
	   rfc1459::is_digit(*name) && strlen(name) == 3)
	{
		target_p = find_id(name);
      		return(target_p);
	}

	target_p = (client::client *)rb_radixtree_retrieve(client_name_tree, name);
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
	rb_dlink_list *hlist;

	if(EmptyString(hostname))
		return NULL;

	hlist = (rb_dlink_list *)rb_radixtree_retrieve(hostname_tree, hostname);
	if (hlist == NULL)
		return NULL;

	return hlist->head;
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

	aconf = (ConfItem *)rb_radixtree_retrieve(resv_tree, name);
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
	rb_radixtree_iteration_state iter;

	void *elem;
	RB_RADIXTREE_FOREACH(elem, &iter, resv_tree)
	{
		aconf = (ConfItem *)elem;
		/* skip temp resvs */
		if(aconf->hold)
			continue;

		rb_radixtree_delete(resv_tree, aconf->host);
		free_conf(aconf);
	}
}

void
add_to_cli_connid_hash(client::client *client_p, uint32_t id)
{
	rb_dictionary_add(client_connid_tree, RB_UINT_TO_POINTER(id), client_p);
}

void
del_from_cli_connid_hash(uint32_t id)
{
	rb_dictionary_delete(client_connid_tree, RB_UINT_TO_POINTER(id));
}

client::client *
find_cli_connid_hash(uint32_t connid)
{
	return (client::client *)rb_dictionary_retrieve(client_connid_tree, RB_UINT_TO_POINTER(connid));
}

} // namespace ircd
