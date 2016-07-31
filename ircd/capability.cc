/*
 * Copyright (c) 2012 - 2016 William Pitcock <nenolod@dereferenced.org>.
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

#include <ircd/stdinc.h>
#include <ircd/capability.h>
#include <ircd/s_assert.h>

#include <list>

// static std::list<CapabilityIndex *> capability_indexes;

CapabilityIndex::CapabilityIndex(std::string name_) : name(name_), highest_bit(1)
{
//	capability_indexes.push_back(this);
}

CapabilityIndex::~CapabilityIndex()
{
//	capability_indexes.erase(this);
}

std::shared_ptr<CapabilityEntry>
CapabilityIndex::find(std::string cap)
{
	return cap_dict[cap];
}

unsigned int
CapabilityIndex::get(std::string cap, void **ownerdata)
{
	std::shared_ptr<CapabilityEntry> entry;

	entry = find(cap);
	if (entry != NULL && !(entry->flags & CAP_ORPHANED))
	{
		if (ownerdata != NULL)
			*ownerdata = entry->ownerdata;
		return (1 << entry->value);
	}

	return 0;
}

unsigned int
CapabilityIndex::put(std::string cap, void *ownerdata)
{
	std::shared_ptr<CapabilityEntry> entry;

	if (highest_bit == 0)
		return 0xFFFFFFFF;

	if ((entry = find(cap)) != NULL)
	{
		entry->flags &= ~CAP_ORPHANED;
		return (1 << entry->value);
	}

	entry = cap_dict[cap] = std::make_shared<CapabilityEntry>(cap, highest_bit, ownerdata);

	highest_bit++;
	if (highest_bit % (sizeof(unsigned int) * 8) == 0)
		highest_bit = 0;

	return (1 << entry->value);
}

unsigned int
CapabilityIndex::put_anonymous()
{
	unsigned int value;

	if (!highest_bit)
		return 0xFFFFFFFF;

	value = 1 << highest_bit;
	highest_bit++;
	if (highest_bit % (sizeof(unsigned int) * 8) == 0)
		highest_bit = 0;

	return value;
}

void
CapabilityIndex::orphan(std::string cap)
{
	std::shared_ptr<CapabilityEntry> entry;

	entry = cap_dict[cap];
	if (entry != NULL)
	{
		entry->flags &= ~CAP_REQUIRED;
		entry->flags |= CAP_ORPHANED;
		entry->ownerdata = NULL;
	}
}

void
CapabilityIndex::require(std::string cap)
{
	std::shared_ptr<CapabilityEntry> entry;

	entry = cap_dict[cap];
	if (entry != NULL)
		entry->flags |= CAP_REQUIRED;
}

const char *
CapabilityIndex::list(unsigned int cap_mask)
{
	static std::string buf;

	buf.clear();

	for (auto it = cap_dict.begin(); it != cap_dict.end(); it++)
	{
		auto entry = it->second;

		if ((1 << entry->value) & cap_mask)
		{
			buf += entry->cap + " ";
		}
	}

	return buf.c_str();
}

unsigned int
CapabilityIndex::mask()
{
	unsigned int mask = 0;

	for (auto it = cap_dict.begin(); it != cap_dict.end(); it++)
	{
		auto entry = it->second;
		if (!(entry->flags & CAP_ORPHANED))
			mask |= (1 << entry->value);
	}

	return mask;
}

unsigned int
CapabilityIndex::required()
{
	unsigned int mask = 0;

	for (auto it = cap_dict.begin(); it != cap_dict.end(); it++)
	{
		auto entry = it->second;
		if (!(entry->flags & CAP_ORPHANED) && (entry->flags & CAP_REQUIRED))
			mask |= (1 << entry->value);
	}

	return mask;
}

void
capability_index_stats(void (*cb)(const char *line, void *privdata), void *privdata)
{
#if 0
	rb_dlink_node *node;
	char buf[BUFSIZE];

	RB_DLINK_FOREACH(node, capability_indexes.head)
	{
		struct CapabilityIndex *idx = (CapabilityIndex *)node->data;
		rb_dictionary_iter iter;
		struct CapabilityEntry *entry;

		snprintf(buf, sizeof buf, "'%s': allocated bits - %d", idx->name, (idx->highest_bit - 1));
		cb(buf, privdata);

		void *elem;
		RB_DICTIONARY_FOREACH(elem, &iter, idx->cap_dict)
		{
			entry = (CapabilityEntry *)elem;
			snprintf(buf, sizeof buf, "bit %d: '%s'", entry->value, entry->cap);
			cb(buf, privdata);
		}

		snprintf(buf, sizeof buf, "'%s': remaining bits - %u", idx->name,
			    (unsigned int)((sizeof(unsigned int) * 8) - (idx->highest_bit - 1)));
		cb(buf, privdata);
	}

	snprintf(buf, sizeof buf, "%ld capability indexes", rb_dlink_list_length(&capability_indexes));
	cb(buf, privdata);
#endif
}
