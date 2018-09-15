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
#define HAVE_IRCD_UTIL_ENV_H

namespace ircd::util
{
	string_view getenv(const string_view &);
}

inline ircd::string_view
ircd::util::getenv(const string_view &key)
{
	if(unlikely(size(key) > 127))
		throw std::runtime_error
		{
			"getenv(): variable key is too long."
		};

	// Ensure the key is null terminated for the std:: call.
	char keystr[128];
	const size_t len{copy(keystr, key)};
	keystr[std::min(len, sizeof(keystr) - 1)] = '\0';
	const string_view var
	{
		std::getenv(keystr)
	};

	return var;
}
