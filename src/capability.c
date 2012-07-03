/*
 * Copyright (c) 2012 William Pitcock <nenolod@dereferenced.org>.
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

#include "stdinc.h"
#include "capability.h"
#include "irc_dictionary.h"

static rb_dlink_list capability_indexes = { NULL, NULL, 0 };

struct CapabilityIndex {
	char *name;
	struct Dictionary *cap_dict;
	unsigned int highest_bit;
	rb_dlink_node node;
};

#define CAP_ORPHANED	0x1
#define CAP_REQUIRED	0x2

struct CapabilityEntry {
	char *cap;
	unsigned int value;
	unsigned int flags;
};

unsigned int
capability_get(struct CapabilityIndex *idx, const char *cap)
{
	struct CapabilityEntry *entry;

	s_assert(idx != NULL);

	entry = irc_dictionary_retrieve(idx->cap_dict, cap);
	if (entry != NULL && !(entry->flags & CAP_ORPHANED))
		return (1 << entry->value);

	return 0;
}

unsigned int
capability_put(struct CapabilityIndex *idx, const char *cap)
{
	struct CapabilityEntry *entry;

	s_assert(idx != NULL);
	if (!idx->highest_bit)
		return 0xFFFFFFFF;

	if ((entry = irc_dictionary_retrieve(idx->cap_dict, cap)) != NULL)
	{
		entry->flags &= ~CAP_ORPHANED;
		return (1 << entry->value);
	}

	entry = rb_malloc(sizeof(struct CapabilityEntry));
	entry->cap = rb_strdup(cap);
	entry->flags = 0;
	entry->value = idx->highest_bit;

	irc_dictionary_add(idx->cap_dict, entry->cap, entry);

	idx->highest_bit++;
	if (idx->highest_bit % (sizeof(unsigned int) * 8) == 0)
		idx->highest_bit = 0;

	return (1 << entry->value);
}

void
capability_orphan(struct CapabilityIndex *idx, const char *cap)
{
	struct CapabilityEntry *entry;

	s_assert(idx != NULL);

	entry = irc_dictionary_retrieve(idx->cap_dict, cap);
	if (entry != NULL)
	{
		entry->flags &= ~CAP_REQUIRED;
		entry->flags |= CAP_ORPHANED;
	}
}

void
capability_require(struct CapabilityIndex *idx, const char *cap)
{
	struct CapabilityEntry *entry;

	s_assert(idx != NULL);

	entry = irc_dictionary_retrieve(idx->cap_dict, cap);
	if (entry != NULL)
		entry->flags |= CAP_REQUIRED;
}

static void
capability_destroy(struct DictionaryElement *delem, void *privdata)
{
	s_assert(delem != NULL);

	rb_free(delem->data);
}

struct CapabilityIndex *
capability_index_create(const char *name)
{
	struct CapabilityIndex *idx;

	idx = rb_malloc(sizeof(struct CapabilityIndex));
	idx->name = rb_strdup(name);
	idx->cap_dict = irc_dictionary_create(strcasecmp);
	idx->highest_bit = 1;

	rb_dlinkAdd(idx, &idx->node, &capability_indexes);

	return idx;
}

void
capability_index_destroy(struct CapabilityIndex *idx)
{
	s_assert(idx != NULL);

	rb_dlinkDelete(&idx->node, &capability_indexes);

	irc_dictionary_destroy(idx->cap_dict, capability_destroy, NULL);
	rb_free(idx);
}

const char *
capability_index_list(struct CapabilityIndex *idx, unsigned int cap_mask)
{
	struct DictionaryIter iter;
	struct CapabilityEntry *entry;
	static char buf[BUFSIZE];
	char *t = buf;
	int tl;

	s_assert(idx != NULL);

	*t = '\0';

	DICTIONARY_FOREACH(entry, &iter, idx->cap_dict)
	{
		if ((1 << entry->value) & cap_mask)
		{
			tl = rb_sprintf(t, "%s ", entry->cap);
			t += tl;
		}
	}

	t--;
	*t = '\0';

	return buf;
}

unsigned int
capability_index_mask(struct CapabilityIndex *idx)
{
	struct DictionaryIter iter;
	struct CapabilityEntry *entry;
	unsigned int mask = 0;

	s_assert(idx != NULL);

	DICTIONARY_FOREACH(entry, &iter, idx->cap_dict)
	{
		if (!(entry->flags & CAP_ORPHANED))
			mask |= (1 << entry->value);
	}

	return mask;
}

unsigned int
capability_index_get_required(struct CapabilityIndex *idx)
{
	struct DictionaryIter iter;
	struct CapabilityEntry *entry;
	unsigned int mask = 0;

	s_assert(idx != NULL);

	DICTIONARY_FOREACH(entry, &iter, idx->cap_dict)
	{
		if (!(entry->flags & CAP_ORPHANED) && (entry->flags & CAP_REQUIRED))
			mask |= (1 << entry->value);
	}

	return mask;
}

void
capability_index_stats(void (*cb)(const char *line, void *privdata), void *privdata)
{
	rb_dlink_node *node;
	char buf[BUFSIZE];

	RB_DLINK_FOREACH(node, capability_indexes.head)
	{
		struct CapabilityIndex *idx = node->data;
		struct DictionaryIter iter;
		struct CapabilityEntry *entry;

		rb_snprintf(buf, sizeof buf, "'%s': allocated bits - %d", idx->name, (idx->highest_bit - 1));
		cb(buf, privdata);

		DICTIONARY_FOREACH(entry, &iter, idx->cap_dict)
		{
			rb_snprintf(buf, sizeof buf, "bit %d: '%s'", entry->value, entry->cap);
			cb(buf, privdata);
		}

		rb_snprintf(buf, sizeof buf, "'%s': remaining bits - %ld", idx->name,
			    (sizeof(unsigned int) * 8) - (idx->highest_bit - 1));
		cb(buf, privdata);
	}

	rb_snprintf(buf, sizeof buf, "%ld capability indexes", rb_dlink_list_length(&capability_indexes));
	cb(buf, privdata);
}
