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

struct CapabilityEntry {
	unsigned int orphaned;
	unsigned int value;
};

unsigned int
capability_get(struct CapabilityIndex *index, const char *cap)
{
	struct CapabilityEntry *entry;

	s_assert(index != NULL);

	entry = irc_dictionary_retrieve(index->cap_dict, cap);
	if (entry != NULL && !entry->orphaned)
		return entry->value;

	return 0xFFFFFFFF;
}

unsigned int
capability_put(struct CapabilityIndex *index, const char *cap)
{
	struct CapabilityEntry *entry;

	s_assert(index != NULL);

	if ((entry = irc_dictionary_retrieve(index->cap_dict, cap)) != NULL)
	{
		entry->orphaned = 0;
		return entry->value;
	}

	entry = rb_malloc(sizeof(struct CapabilityEntry));
	entry->orphaned = 0;
	entry->value = index->highest_bit;

	irc_dictionary_add(index->cap_dict, cap, entry);

	index->highest_bit <<= 1;

	/* hmm... not sure what to do here, so i guess we will abort for now... --nenolod */
	if (index->highest_bit == 0)
		abort();

	return entry->value;
}

void
capability_orphan(struct CapabilityIndex *index, const char *cap)
{
	struct CapabilityEntry *entry;

	s_assert(index != NULL);

	entry = irc_dictionary_retrieve(index->cap_dict, cap);
	if (entry != NULL)
		entry->orphaned = 1;
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
			tl = rb_sprintf(t, "%s ", iter.cur->key);
			t += tl;
		}
	}

	t--;
	*t = '\0';

	return buf;
}
