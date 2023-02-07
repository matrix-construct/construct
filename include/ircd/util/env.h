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

namespace ircd {
inline namespace util
{
	using env_closure = closure_bool<std::function, string_view, string_view>;

	// Iterate environment variables starting with prefix
	bool for_each_env(const string_view &, const env_closure &);

	// Iterate all environment variables
	bool for_each_env(const env_closure &);

	// Get one environment variable
	string_view getenv(const string_view &);
}}
