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
#define HAVE_IRCD_JS_VERSION_H

namespace ircd {
namespace js   {

const char *version(const JSVersion &v);
JSVersion version(const char *const &v);

} // namespace js
} // namespace ircd

inline JSVersion
ircd::js::version(const char *const &v)
{
	return JS_StringToVersion(v);
}

inline const char *
ircd::js::version(const JSVersion &v)
{
	return JS_VersionToString(v);
}
