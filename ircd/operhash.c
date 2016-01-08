/* ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 * operhash.c - Hashes nick!user@host{oper}
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
 *
 * $Id: operhash.c 26094 2008-09-19 15:33:46Z androsyn $
 */
#include <ratbox_lib.h>
#include "stdinc.h"
#include "match.h"
#include "hash.h"
#include "operhash.h"

#define OPERHASH_MAX_BITS 7
#define OPERHASH_MAX (1<<OPERHASH_MAX_BITS)

#define hash_opername(x) fnv_hash_upper((const unsigned char *)(x), OPERHASH_MAX_BITS)

static rb_dlink_list operhash_table[OPERHASH_MAX];

const char *
operhash_add(const char *name)
{
	void *ohash_mem;
	char *ohash_copy;
	unsigned int hashv;
	rb_dlink_node *ptr;

	if(EmptyString(name))
		return NULL;

	hashv = hash_opername(name);

	RB_DLINK_FOREACH(ptr, operhash_table[hashv].head)
	{
		ohash_copy = ptr->data;

		if(!irccmp(ohash_copy, name))
		{
			ohash_mem = ohash_copy - 5;
			(*(int32_t *) ohash_mem)++;
			return ohash_copy;
		}
	}

	ohash_mem = rb_malloc(strlen(name) + 6);
	(*(int32_t *) ohash_mem) = 1;
	ohash_copy = ohash_mem + 5;
	ohash_copy[-1] = '@';
	strcpy(ohash_copy, name);

	rb_dlinkAddAlloc(ohash_copy, &operhash_table[hashv]);

	return ohash_copy;
}

const char *
operhash_find(const char *name)
{
	char *ohash_copy;
	unsigned int hashv;
	rb_dlink_node *ptr;

	if(EmptyString(name))
		return NULL;

	hashv = hash_opername(name);

	RB_DLINK_FOREACH(ptr, operhash_table[hashv].head)
	{
		ohash_copy = ptr->data;

		if(!irccmp(ohash_copy, name))
			return ohash_copy;
	}

	return NULL;
}

void
operhash_delete(const char *name)
{
	char *ohash_copy;
	void *ohash_mem;
	unsigned int hashv;
	rb_dlink_node *ptr;

	if(EmptyString(name))
		return;

	hashv = hash_opername(name);

	RB_DLINK_FOREACH(ptr, operhash_table[hashv].head)
	{
		ohash_copy = ptr->data;

		if(irccmp(ohash_copy, name))
			continue;

		ohash_mem = ohash_copy - 5;
		(*(int32_t *) ohash_mem)--;

		if((*(int32_t *) ohash_mem) == 0)
		{
			rb_dlinkDestroy(ptr, &operhash_table[hashv]);
			rb_free(ohash_mem);
			return;
		}
	}
}
