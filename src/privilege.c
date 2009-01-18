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
#include "s_conf.h"
#include "privilege.h"
#include "numeric.h"

static rb_dlink_list privilegeset_list = {};

int
privilegeset_in_set(struct PrivilegeSet *set, const char *priv)
{
	s_assert(set != NULL);
	s_assert(priv != NULL);

	return strstr(set->privs, priv) != NULL;
}

static struct PrivilegeSet *
privilegeset_get_any(const char *name)
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
privilegeset_set_new(const char *name, const char *privs, PrivilegeFlags flags)
{
	struct PrivilegeSet *set;

	set = privilegeset_get_any(name);
	if (set != NULL)
	{
		if (!(set->status & CONF_ILLEGAL))
			ilog(L_MAIN, "Duplicate privset %s", name);
		set->status &= ~CONF_ILLEGAL;
		rb_free(set->privs);
	}
	else
	{
		set = rb_malloc(sizeof(struct PrivilegeSet));
		set->status = 0;
		set->refs = 0;
		set->name = rb_strdup(name);

		rb_dlinkAdd(set, &set->node, &privilegeset_list);
	}
	set->privs = rb_strdup(privs);
	set->flags = flags;

	return set;
}

struct PrivilegeSet *
privilegeset_extend(struct PrivilegeSet *parent, const char *name, const char *privs, PrivilegeFlags flags)
{
	struct PrivilegeSet *set;

	s_assert(parent != NULL);
	s_assert(name != NULL);
	s_assert(privs != NULL);

	set = privilegeset_get_any(name);
	if (set != NULL)
	{
		if (!(set->status & CONF_ILLEGAL))
			ilog(L_MAIN, "Duplicate privset %s", name);
		set->status &= ~CONF_ILLEGAL;
		rb_free(set->privs);
	}
	else
	{
		set = rb_malloc(sizeof(struct PrivilegeSet));
		set->status = 0;
		set->refs = 0;
		set->name = rb_strdup(name);

		rb_dlinkAdd(set, &set->node, &privilegeset_list);
	}
	set->flags = flags;
	set->privs = rb_malloc(strlen(parent->privs) + 1 + strlen(privs) + 1);
	strcpy(set->privs, parent->privs);
	strcat(set->privs, " ");
	strcat(set->privs, privs);

	return set;
}

struct PrivilegeSet *
privilegeset_get(const char *name)
{
	struct PrivilegeSet *set;

	set = privilegeset_get_any(name);
	if (set != NULL && set->status & CONF_ILLEGAL)
		set = NULL;
	return set;
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

	if (set->refs > 0)
		set->refs--;
	else
		ilog(L_MAIN, "refs on privset %s is already 0",
				set->name);
	if (set->refs == 0 && set->status & CONF_ILLEGAL)
	{
		rb_dlinkDelete(&set->node, &privilegeset_list);

		rb_free(set->name);
		rb_free(set->privs);
		rb_free(set);
	}
}

void
privilegeset_mark_all_illegal(void)
{
	rb_dlink_node *iter;

	RB_DLINK_FOREACH(iter, privilegeset_list.head)
	{
		struct PrivilegeSet *set = (struct PrivilegeSet *) iter->data;

		/* the "default" privset is special and must remain available */
		if (!strcmp(set->name, "default"))
			continue;

		set->status |= CONF_ILLEGAL;
		rb_free(set->privs);
		set->privs = rb_strdup("");
		/* but do not free it yet */
	}
}

void
privilegeset_delete_all_illegal(void)
{
	rb_dlink_node *iter, *next;

	RB_DLINK_FOREACH_SAFE(iter, next, privilegeset_list.head)
	{
		struct PrivilegeSet *set = (struct PrivilegeSet *) iter->data;

		privilegeset_ref(set);
		privilegeset_unref(set);
	}
}

void
privilegeset_report(struct Client *source_p)
{
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, privilegeset_list.head)
	{
		struct PrivilegeSet *set = ptr->data;

		/* use RPL_STATSDEBUG for now -- jilles */
		sendto_one_numeric(source_p, RPL_STATSDEBUG,
				"O :%s %s",
				set->name,
				set->privs);
	}
}
