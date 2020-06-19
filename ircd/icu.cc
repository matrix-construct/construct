// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#if __has_include(<unicode/uversion.h>)
	#include <unicode/uversion.h>
#endif

#if __has_include(<unicode/uchar.h>)
	#include <unicode/uchar.h>
#endif

decltype(ircd::icu::version_api)
ircd::icu::version_api
{
	"icu", info::versions::API, 0,
	#if __has_include(<unicode/uversion.h>)
	{
		U_ICU_VERSION_MAJOR_NUM,
		U_ICU_VERSION_MINOR_NUM,
		U_ICU_VERSION_PATCHLEVEL_NUM,
	},
	U_ICU_VERSION
	#endif
};

decltype(ircd::icu::version_abi)
ircd::icu::version_abi
{
	"icu", info::versions::ABI, 0, {0}, []
	(auto &v, const mutable_buffer &s)
	{
		#if __has_include(<unicode/uversion.h>)
		UVersionInfo info;
		u_getVersion(info);
		u_versionToString(info, data(s));
		#endif
	}
};

decltype(ircd::icu::unicode_version_api)
ircd::icu::unicode_version_api
{
	"unicode", info::versions::API, 0, {0},
	#if __has_include(<unicode/uchar.h>)
	U_UNICODE_VERSION
	#endif
};

decltype(ircd::icu::unicode_version_abi)
ircd::icu::unicode_version_abi
{
	"unicode", info::versions::ABI, 0, {0}, []
	(auto &v, const mutable_buffer &s)
	{
		#if __has_include(<unicode/uchar.h>)
		UVersionInfo info;
		u_getUnicodeVersion(info);
		u_versionToString(info, data(s));
		#endif
	}
};
