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

#include <rb/format.h>

using namespace ircd::capability;
using namespace ircd;


std::list<capability::index *> indexes;


entry::entry(const std::string &cap,
             const uint32_t &value,
             void *const &ownerdata)
:cap(cap)
,value(value)
,require(false)
,orphan(false)
,ownerdata(ownerdata)
{
}


index::index(const std::string &name)
:name(name)
,highest_bit(1)
,indexes_it(indexes.emplace(end(indexes), this))
{
}

index::~index()
{
	indexes.erase(indexes_it);
}

uint32_t
index::put(const std::string &cap,
           void *ownerdata)
{
	if (highest_bit == 0)
		return 0xFFFFFFFF;

	auto it(caps.lower_bound(cap));
	if (it != end(caps) && it->second->cap == cap)
	{
		auto &entry(it->second);
		entry->orphan = false;
		return 1 << entry->value;
	}

	auto entry(std::make_shared<entry>(cap, highest_bit, ownerdata));
	caps.emplace_hint(it, cap, entry);

	highest_bit++;
	if (highest_bit % (sizeof(uint32_t) * 8) == 0)
		highest_bit = 0;

	return 1 << entry->value;
}

uint32_t
index::put_anonymous()
{
	if (!highest_bit)
		return 0xFFFFFFFF;

	const uint32_t value(1 << highest_bit);
	highest_bit++;
	if (highest_bit % (sizeof(uint32_t) * 8) == 0)
		highest_bit = 0;

	return value;
}

uint32_t
index::get(const std::string &cap,
           void **ownerdata)
try
{
	auto &entry(caps.at(cap));
	if (entry->orphan)
		return 0;

	if (ownerdata != NULL)
		*ownerdata = entry->ownerdata;

	return 1 << entry->value;
}
catch(const std::out_of_range &e)
{
	return 0;
}

bool
index::require(const std::string &cap)
{
	const auto it(caps.find(cap));
	if (it == end(caps))
		return false;

	auto &entry(it->second);
	entry->require = true;
	return true;
}

bool
index::orphan(const std::string &cap)
{
	const auto it(caps.find(cap));
	if (it == end(caps))
		return false;

	auto &entry(it->second);
	entry->require = false;
	entry->orphan = true;
	entry->ownerdata = NULL;
	return true;
}

uint32_t
index::mask()
{
	uint32_t mask(0);
	for (const auto &it : caps)
	{
		const auto &entry(it.second);
		if (!entry->orphan)
			mask |= (1 << entry->value);
	}

	return mask;
}

uint32_t
index::required()
{
	uint32_t mask(0);
	for (const auto &it : caps)
	{
		const auto &entry(it.second);
		if (!entry->orphan && entry->require)
			mask |= (1 << entry->value);
	}

	return mask;
}

std::string
index::list(const uint32_t &cap_mask)
{
	std::stringstream buf;
	for (const auto &it : caps)
	{
		const auto &entry(it.second);
		if ((1 << entry->value) & cap_mask)
			buf << entry->cap << " ";
	}

	return buf.str();
}

void
index::stats(void (*cb)(const std::string &line, void *privdata), void *privdata)
const
{
	std::string out(fmt::format("'{0}': {1} allocated:", name, highest_bit - 1));
	for (const auto &it : caps)
		out += fmt::format(" {0}<{1}>", it.first, it.second->value);

	cb(out, privdata);
}

void
capability::stats(void (*cb)(const std::string &line, void *privdata), void *privdata)
{
	for (const auto &index : indexes)
		index->stats(cb, privdata);
}
