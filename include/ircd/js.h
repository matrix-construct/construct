/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* This is the 'stdinc header' which begins the ircd::js namespace, but does not
 * include any 3rd party symbols. This header is included automatically in stdinc.h.
 *
 * If the API in this header is unsatisfactory: get dirty with the real jsapi by
 * manually including <ircd/js/js.h>
 */

#pragma once
#define HAVE_IRCD_JS_H

namespace ircd {
namespace js   {

// Root exception for this subsystem. The exception hierarchy integrating
// with JS itself inherits from this and is defined in TODO: _________
IRCD_EXCEPTION(ircd::error, error)

// Specific logging facility for this subsystem with snomask
extern struct log::log log;

// Fetch version information
enum class ver
{
	IMPLEMENTATION,
};
const char *version(const ver &ver);

// Initialize the subsystem (singleton held by IRCd main context only)
struct init
{
	init();
	~init() noexcept;
};

} // namespace js
} // namespace ircd

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
