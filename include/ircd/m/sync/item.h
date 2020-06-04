// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_SYNC_ITEM_H

namespace ircd::m::sync
{
	struct item;
	struct data;

	using item_closure = std::function<void (item &)>;
	using item_closure_bool = std::function<bool (item &)>;

	bool for_each(const string_view &prefix, const item_closure_bool &);
	bool for_each(const item_closure_bool &);
}

/// A sync::item provides response content for a specific part of a /sync as
/// specified in Matrix c2s. Instances of this object act similarly to hook
/// handlers but specialized for /sync. Each instance registers itself to
/// handle a path. Two handlers are provided for an item: a polylog handler
/// and a linear handler.
/// 
struct ircd::m::sync::item
:instance_multimap<std::string, item, std::less<>>
{
	using handle = std::function<bool (data &)>;

	std::string conf_name[2];
	conf::item<bool> enable;
	conf::item<bool> stats_debug;
	handle _polylog;
	handle _linear;
	json::strung feature;
	json::object opts;
	bool phased;
	bool prefetch;

  public:
	string_view name() const;
	string_view member_name() const;
	size_t children() const;

	bool linear(data &);
	bool polylog(data &);

	item(std::string name,
	     handle polylog         = {},
	     handle linear          = {},
	     const json::members &  = {});

	item(item &&) = delete;
	item(const item &) = delete;
	~item() noexcept;
};
