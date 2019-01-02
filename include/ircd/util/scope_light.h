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
#define HAVE_IRCD_UTIL_SCOPE_LIGHT_H

namespace ircd::util
{
	struct scope_light;
};

/// A simple boiler-plate  for setting a bool to true when constructed and
/// resetting it to its previous value when destructed. This takes a runtime
/// reference to that bool. This is not intended to be a replacement for the
/// reentrance_assertion utility or ctx::mutex et al.
struct ircd::util::scope_light
{
	bool *light {nullptr};
	bool theirs {false};

	scope_light(bool &light);
	scope_light(const scope_light &) = delete;
	scope_light(scope_light &&) noexcept = delete;
	~scope_light() noexcept;
};

inline
ircd::util::scope_light::scope_light(bool &light)
:light{&light}
,theirs{light}
{
	light = true;
}

inline
ircd::util::scope_light::~scope_light()
noexcept
{
	assert(light);
	*light = theirs;
}
