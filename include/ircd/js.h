// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

/* This is the 'stdinc header' which begins the ircd::js namespace, but does not
 * include any 3rd party symbols. This header is included automatically in stdinc.h.
 *
 * If the API in this header is unsatisfactory: get dirty with the real jsapi by
 * manually including <ircd/js/js.h>
 */

#pragma once
#define HAVE_IRCD_JS_H

/// JavaScript Embedded Machine
namespace ircd::js
{
	// Root exception for this subsystem. The exception hierarchy integrating
	// with JS itself inherits from this and is defined in TODO: _________
	IRCD_EXCEPTION(ircd::error, error)

	// Specific logging facility for this subsystem with snomask
	extern struct log::log log;
	extern conf::item<bool> enable;

	// Fetch version information
	enum class ver
	{
		IMPLEMENTATION,
	};
	const char *version(const ver &ver);

	struct init;
}

// Initialize the subsystem (singleton held by IRCd main context only)
struct ircd::js::init
{
	init();
	~init() noexcept;
};

#ifndef RB_ENABLE_JS
//
// Stub definitions for when JS isn't enabled because the definition
// file is not compiled at all.
//

inline
ircd::js::init::init()
{
}

inline
ircd::js::init::~init()
{
}

inline
const char *ircd::js::version(const ver &ver)
{
	return "DISABLED";
}

#endif // !RB_ENABLE_JS
