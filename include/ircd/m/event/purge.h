// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_EVENT_PURGE_H

//XXX fwd decl
namespace ircd::m::dbs
{
	struct opts;
}

/// Erase an event from the database.
///
/// Purging an event will erase its data and metadata, including applying
/// reverse operations to restore the database state prior to the event's
/// acquisition. This allows the event to be reacquired without considering the
/// later eval to be a replay requiring an override. Take special care to
/// note that while the database will remain consistent after each purge, the
/// application logic may not, so be careful which events are purged. Further,
/// while the database will remain consistent after a later eval, such a
/// reevaluation is in fact a replay, and the extended effects of an event
/// were likely not reversed (nor cannot be reversed) by a purge, and its
/// revelation to clients may happen again unless prevented by the evaluator.
///
/// Overloads taking a db::txn will stage the erasure in the txn for the user
/// to commit later.
///
struct ircd::m::event::purge
:returns<bool>
{
	purge(db::txn &, const event::idx &, const event &, dbs::opts);
	purge(db::txn &, const event::idx &, const event &);
	purge(db::txn &, const event::idx &, dbs::opts);
	purge(db::txn &, const event::idx &);
	purge(const event::idx &, dbs::opts);
	purge(const event::idx &);
};
