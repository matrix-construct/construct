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
/// Please note the exact mechanism of the return value from the closure. This
/// is an operation during a specific compaction, not a front-end operation on
/// the database.
///
/// - op::GET the source record is moved to the target as per normal
/// compaction.
///
/// - op::SET the new value is placed in the compaction target rather than
/// the source value. Specify the new value at `replace` in the args struct.
///
/// - op::DELETE a delete record is placed in the compaction target and the
/// source value will be forgotten.
///
/// - op::DELETE_RANGE skips moving the source record to the target. The
/// source record is simply forgotten without a delete record. User can set
/// `skip_until` in the args structure to apply this non-action to a range.
///
struct ircd::db::compactor
{
	struct args;
	using proto = db::op (const args &);
	using callback = std::function<proto>;

	callback value;
	callback merge;
};

/// The arguments presented to the callback are aggregated in this structure.
///
/// For each record iterated in the compaction we present const information
/// for examination by the user in the first part of this structure. It also
/// contains an interface for the compactor to mutate records during the
/// process.
///
struct ircd::db::compactor::args
{
	const int &level;
	const string_view key;
	const string_view val;

	std::string *const &replace;
	std::string *const &skip_until;
};
