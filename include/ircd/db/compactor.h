// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_DB_COMPACTOR_H

namespace ircd::db
{
	struct compactor;
}

/// Compaction callback
///
/// return db::op::GET (0) from callback for no-op
/// return db::op::DELETE from callback to delete this kv.
/// return db::op::SET from callback if replacement modified.
/// return db::op::DELETE_RANGE from callback if skip_until modified.
///
struct ircd::db::compactor
{
	struct args;
	using proto = db::op (const args &);
	using callback = std::function<proto>;

	callback value;
	callback merge;
};

struct ircd::db::compactor::args
{
	const int &level;

	const string_view key;
	const string_view val;

	std::string *const &replace;
	std::string *const &skip_until;
};
