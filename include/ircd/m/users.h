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
#define HAVE_IRCD_M_USERS_H

namespace ircd::m::users
{
	struct opts extern const opts_default;

	// Iterate the users
	bool for_each(const opts &, const user::closure_bool &);
	bool for_each(const user::closure_bool &);

	size_t count(const opts & = opts_default);
	bool exists(const opts & = opts_default);
}

/// Shape the query by matching users based on the options filled in.
struct ircd::m::users::opts
{
	/// Fill this in to match the localpart of an mxid. If this is empty then
	/// all localparts can be matched.
	string_view localpart;

	/// The results match if their localpart startswith the specified localpart.
	bool localpart_prefix {false};

	/// Fill this in to match the hostpart of an mxid, i.e the origin. If this
	/// is empty then all servers can be matched.
	string_view hostpart;

	/// The results match if their hostpart startswith the specified  hostpart
	bool hostpart_prefix {false};

	/// Construction from a single string; decomposes into the options
	/// based on ad hoc rules, see definition or use default ctor.
	opts(const string_view &query);
	opts() = default;
};
