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

struct CapabilityIndex {
	struct Dictionary *cap_dict;
	unsigned int highest_bit;
};

#define CAP_ORPHANED	0x1
#define CAP_REQUIRED	0x2

struct CapabilityEntry {
	char *cap;
	unsigned int value;
	unsigned int flags;
};

unsigned int
capability_get(struct CapabilityIndex *index, const char *cap)
{
	struct CapabilityEntry *entry;

	s_assert(index != NULL);

	entry = irc_dictionary_retrieve(index->cap_dict, cap);
	if (entry != NULL && !(entry->flags & CAP_ORPHANED))
		return (1 << entry->value);

	return 0xFFFFFFFF;
}

unsigned int
capability_put(struct CapabilityIndex *index, const char *cap)
{
	struct CapabilityEntry *entry;

	s_assert(index != NULL);
	if (!index->highest_bit)
		return 0xFFFFFFFF;

	if ((entry = irc_dictionary_retrieve(index->cap_dict, cap)) != NULL)
	{
		entry->flags &= ~CAP_ORPHANED;
		return (1 << entry->value);
	}

	entry = rb_malloc(sizeof(struct CapabilityEntry));
	entry->cap = rb_strdup(cap);
	entry->flags = 0;
	entry->value = index->highest_bit;

	irc_dictionary_add(index->cap_dict, entry->cap, entry);

	index->highest_bit++;
	if (index->highest_bit % (sizeof(unsigned int) * 8) == 0)
		index->highest_bit = 0;

	return (1 << entry->value);
}

void
capability_orphan(struct CapabilityIndex *index, const char *cap)
{
	struct CapabilityEntry *entry;

	s_assert(index != NULL);

	entry = irc_dictionary_retrieve(index->cap_dict, cap);
	if (entry != NULL)
	{
		entry->flags &= ~CAP_REQUIRED;
		entry->flags |= CAP_ORPHANED;
	}
}

void
capability_require(struct CapabilityIndex *index, const char *cap)
{
	struct CapabilityEntry *entry;

	s_assert(index != NULL);

	entry = irc_dictionary_retrieve(index->cap_dict, cap);
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
capability_index_create(void)
{
	struct CapabilityIndex *index;

	index = rb_malloc(sizeof(struct CapabilityIndex));
	index->cap_dict = irc_dictionary_create(strcasecmp);
	index->highest_bit = 1;

	return index;
}

void
capability_index_destroy(struct CapabilityIndex *index)
{
	s_assert(index != NULL);

	irc_dictionary_destroy(index->cap_dict, capability_destroy, NULL);
	rb_free(index);
}

const char *
capability_index_list(struct CapabilityIndex *index, unsigned int cap_mask)
{
	struct DictionaryIter iter;
	struct CapabilityEntry *entry;
	static char buf[BUFSIZE];
	char *t = buf;
	int tl;

	s_assert(index != NULL);

	*t = '\0';

	DICTIONARY_FOREACH(entry, &iter, index->cap_dict)
	{
		if (entry->value & cap_mask)
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
capability_index_mask(struct CapabilityIndex *index)
{
	struct DictionaryIter iter;
	struct CapabilityEntry *entry;
	unsigned int mask = 0;

	s_assert(index != NULL);

	DICTIONARY_FOREACH(entry, &iter, index->cap_dict)
	{
		if (!(entry->flags & CAP_ORPHANED))
			mask |= (1 << entry->value);
	}

	return mask;
}

unsigned int
capability_index_get_required(struct CapabilityIndex *index)
{
	struct DictionaryIter iter;
	struct CapabilityEntry *entry;
	unsigned int mask = 0;

	s_assert(index != NULL);

	DICTIONARY_FOREACH(entry, &iter, index->cap_dict)
	{
		if (!(entry->flags & CAP_ORPHANED) && (entry->flags & CAP_REQUIRED))
			mask |= (1 << entry->value);
	}

	return mask;
}
