// The Construct
//
// Copyright (C) Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#ifndef HAVE_IRCD_IRCD_H
#define HAVE_IRCD_IRCD_H

//////////////////////////////////////////////////////////////////////////////>
//
// This is an aggregate header which ties together the general public
// interfaces for IRCd. Include this header to operate and embed the library in
// your application; no other includes from the project should be required.
//
// This header only includes standard library headers; we may forward declare
// third-party symbols, but only if their headers are not required.
//
//////////////////////////////////////////////////////////////////////////////>

//
// Project configuration
//

#include "config.h"                    // Generated by ./configure; do not edit
#include "assert.h"                    // Custom assert (during debug builds)
#include "portable.h"                  // Additional developer config; editable

//
// Standard library includes
//

#include "stdinc.h"

//
// Project library interfaces
//

#include "simd/simd.h"
#include "string_view.h"
#include "vector_view.h"
#include "byte_view.h"
#include "buffer/buffer.h"
#include "leb128.h"
#include "allocator.h"
#include "util/util.h"
#include "exception.h"
#include "panic.h"
#include "run.h"
#include "demangle.h"
#include "backtrace.h"
#include "info.h"
#include "icu.h"
#include "utf.h"
#include "time.h"
#include "lex_cast.h"
#include "logger.h"
#include "sys.h"
#include "fpe.h"
#include "rand.h"
#include "crh.h"
#include "base.h"
#include "stringops.h"
#include "strl.h"
#include "strn.h"
#include "cmp.h"
#include "globular.h"
#include "tokens.h"
#include "iov.h"
#include "grammar.h"
#include "parse.h"
#include "color.h"
#include "rfc1459.h"
#include "json/json.h"
#include "cbor.h"
#include "nacl.h"
#include "ed25519.h"
#include "openssl.h"
#include "pbc.h"
#include "fmt.h"
#include "http.h"
#include "http2/http2.h"
#include "conf.h"
#include "magic.h"
#include "stats.h"
#include "prof/prof.h"
#include "fs/fs.h"
#include "ios.h"
#include "ctx/ctx.h"
#include "db/db.h"
#include "js.h"
#include "mods/mods.h"
#include "rfc1035.h"
#include "rfc3986.h"
#include "net/net.h"
#include "server/server.h"
#include "magick.h"
#include "resource/resource.h"
#include "client.h"

/// \brief Internet Relay Chat daemon. This is the principal namespace for IRCd.
///
namespace ircd
{
	// Library version information (also see info.h for more version related)
	extern const info::versions version_api;
	extern const info::versions version_abi;

	extern conf::item<bool> restart;
	extern conf::item<bool> debugmode;
	extern conf::item<bool> read_only;
	extern conf::item<bool> write_avoid;
	extern conf::item<bool> soft_assert;
	extern conf::item<bool> defaults;

	// Informational
	seconds uptime();

	// Control panel
	void cont() noexcept;
	bool quit() noexcept;
	void init(boost::asio::executor &&);
}

#endif // HAVE_IRCD_IRCD_H
