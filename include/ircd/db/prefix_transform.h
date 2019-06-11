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
#define HAVE_IRCD_DB_PREFIX_TRANSFORM_H

namespace ircd::db
{
	struct prefix_transform;
}

struct ircd::db::prefix_transform
{
	std::string name;
	std::function<bool (const string_view &)> has;
	std::function<string_view (const string_view &)> get;
};
