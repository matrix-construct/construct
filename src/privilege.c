/*
 * charybdis: an advanced ircd.
 * privilege.c: Dynamic privileges API.
 *
 * Copyright (c) 2008 William Pitcock <nenolod@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
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

#include <stdinc.h>
#include "privilege.h"

static rb_dlink_list privilegeset_list = {};

int
privilegeset_in_set(struct PrivilegeSet *set, const char *priv)
{
	s_assert(set != NULL);
	s_assert(priv != NULL);

	return strstr(set->privs, priv) != NULL;
}

struct PrivilegeSet *
privilegeset_set_new(const char *name, const char *privs, PrivilegeFlags flags)
{
	struct PrivilegeSet *set;

	s_assert(privilegeset_get(name) == NULL);

	set = rb_malloc(sizeof(struct PrivilegeSet));
	set->refs = 1;
	set->name = rb_strdup(name);
	set->privs = rb_strdup(privs);
	set->flags = flags;

	rb_dlinkAdd(set, &set->node, &privilegeset_list);

	return set;
}

struct PrivilegeSet *
privilegeset_extend(struct PrivilegeSet *parent, const char *name, const char *privs, PrivilegeFlags flags)
{
	struct PrivilegeSet *set;

	s_assert(parent != NULL);
	s_assert(name != NULL);
	s_assert(privs != NULL);
	s_assert(privilegeset_get(name) == NULL);

	set = rb_malloc(sizeof(struct PrivilegeSet));
	set->refs = 1;
	set->name = rb_strdup(name);
	set->flags = flags;
	set->privs = rb_malloc(strlen(parent->privs) + 1 + strlen(privs) + 1);
	strcpy(set->privs, parent->privs);
	strcat(set->privs, " ");
	strcat(set->privs, privs);

	rb_dlinkAdd(set, &set->node, &privilegeset_list);

	return set;
}

struct PrivilegeSet *
privilegeset_get(const char *name)
{
	rb_dlink_node *iter;

	s_assert(name != NULL);

	RB_DLINK_FOREACH(iter, privilegeset_list.head)
	{
		struct PrivilegeSet *set = (struct PrivilegeSet *) iter->data;

		if (!strcasecmp(set->name, name))
			return set;
	}

	return NULL;
}

struct PrivilegeSet *
privilegeset_ref(struct PrivilegeSet *set)
{
	s_assert(set != NULL);

	set->refs++;

	return set;
}

void
privilegeset_unref(struct PrivilegeSet *set)
{
	s_assert(set != NULL);

	if (--set->refs == 0)
	{
		rb_dlinkDelete(&set->node, &privilegeset_list);

		rb_free(set->name);
		rb_free(set->privs);
		rb_free(set);
	}
}
