// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_STATS_H

namespace ircd::stats
{
	struct item;

	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, not_found)

	extern std::map<string_view, item *> items;
}

struct ircd::stats::item
{
	using callback = std::function<string_view (const mutable_buffer &)>;

	static const size_t NAME_MAX_LEN;

	json::strung feature_;
	json::object feature;
	string_view name;
	callback cb;

  public:
	item(const json::members &, callback);
	item(item &&) = delete;
	item(const item &) = delete;
	~item() noexcept;
};
