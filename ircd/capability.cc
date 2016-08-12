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
#include <rb/format.h>

static std::list<CapabilityIndex *> capability_indexes;

CapabilityIndex::CapabilityIndex(std::string name_) : name(name_), highest_bit(1)
{
	capability_indexes.push_back(this);
}

CapabilityIndex::~CapabilityIndex()
{
	for (auto it = capability_indexes.begin(); it != capability_indexes.end(); it++)
	{
		if (*it == this)
		{
			capability_indexes.erase(it);
			break;
		}
	}
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
	if (entry != NULL && !entry->orphan)
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
		entry->orphan = false;
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
		entry->require = false;
		entry->orphan = true;
		entry->ownerdata = NULL;
	}
}

void
CapabilityIndex::require(std::string cap)
{
	std::shared_ptr<CapabilityEntry> entry;

	entry = cap_dict[cap];
	if (entry != NULL)
		entry->require = true;
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
		if (!entry->orphan)
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
		if (!entry->orphan && entry->require)
			mask |= (1 << entry->value);
	}

	return mask;
}

void
CapabilityIndex::stats(void (*cb)(std::string &line, void *privdata), void *privdata)
{
	std::string out = fmt::format("'{0}': {1} allocated:", name, highest_bit - 1);

	for (auto it = cap_dict.begin(); it != cap_dict.end(); it++)
		out += fmt::format(" {0}<{1}>", it->first, it->second->value);

	cb(out, privdata);
}

void
capability_stats(void (*cb)(std::string &line, void *privdata), void *privdata)
{
	for (auto it = capability_indexes.begin(); it != capability_indexes.end(); it++)
		(*it)->stats(cb, privdata);
}
