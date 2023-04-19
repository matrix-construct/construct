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
#define HAVE_IRCD_JSON_TOOL_H

// Convenience toolset for higher level operations.
namespace ircd::json
{
	strung append(const array &, const string_view &val);
	strung prepend(const array &, const string_view &val);

	void merge(stack::object &out, const vector &);

	strung remove(const object &, const string_view &key);
	strung remove(const object &, const size_t &index);

	strung insert(const object &, const member &);

	strung replace(const object &,  const members &);
	strung replace(const object &, const member &);
}
