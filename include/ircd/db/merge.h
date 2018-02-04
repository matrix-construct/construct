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
#define HAVE_IRCD_DB_MERGE_H

namespace ircd::db
{
	using merge_delta = std::pair<string_view, string_view>;
	using merge_closure = std::function<std::string (const string_view &key, const merge_delta &)>;
	using update_closure = std::function<std::string (const string_view &key, merge_delta &)>;

	std::string merge_operator(const string_view &, const std::pair<string_view, string_view> &);
}
