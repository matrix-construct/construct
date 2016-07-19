/* charybdis
 * operhash.c - Hashes nick!user@host{oper}
 *
 * Copyright (C) 2005 Lee Hardy <lee -at- leeh.co.uk>
 * Copyright (C) 2005-2016 ircd-ratbox development team
 * Copyright (C) 2016 William Pitcock <nenolod@dereferenced.org>
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

#include <rb_lib.h>
#include "stdinc.h"
#include "match.h"
#include "hash.h"
#include "operhash.h"
#include "rb_radixtree.h"

static rb_radixtree *operhash_tree = NULL;

struct operhash_entry
{
	unsigned int refcount;
	char name[];
};

void
init_operhash(void)
{
	operhash_tree = rb_radixtree_create("operhash", NULL);
}

const char *
operhash_add(const char *name)
{
	struct operhash_entry *ohash;
	size_t len;

	if(EmptyString(name))
		return NULL;

	if((ohash = (struct operhash_entry *) rb_radixtree_retrieve(operhash_tree, name)) != NULL)
	{
		ohash->refcount++;
		return ohash->name;
	}

	len = strlen(name) + 1;
	ohash = rb_malloc(sizeof(struct operhash_entry) + len);
	ohash->refcount = 1;
	memcpy(ohash->name, name, len);
	rb_radixtree_add(operhash_tree, ohash->name, ohash);
	return ohash->name;
}

const char *
operhash_find(const char *name)
{
	return rb_radixtree_retrieve(operhash_tree, name);
}

void
operhash_delete(const char *name)
{
	struct operhash_entry *ohash = rb_radixtree_retrieve(operhash_tree, name);

	if(ohash == NULL)
		return;

	ohash->refcount--;
	if(ohash->refcount == 0)
	{
		rb_radixtree_delete(operhash_tree, ohash->name);
		rb_free(ohash);
	}
}
