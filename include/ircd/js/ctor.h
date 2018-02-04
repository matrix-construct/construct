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
#define HAVE_IRCD_JS_CTOR_H

namespace ircd {
namespace js   {

inline object
ctor(const object::handle &proto,
     const vector<value>::handle &args = {})
{
	return JS_New(*cx, proto, args);
}

inline object
ctor(trap &trap,
     const vector<value>::handle &args = {})
{
	return trap.construct(args);
}

} // namespace js
} // namespace ircd
