// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

std::string
ircd::m::self::origin
{
	ircd::string_view{ircd::network_name}
};

std::string
ircd::m::self::servername
{
	ircd::string_view{ircd::server_name}
};

ircd::ed25519::sk
ircd::m::self::secret_key
{};

ircd::ed25519::pk
ircd::m::self::public_key
{};

std::string
ircd::m::self::public_key_b64
{};

std::string
ircd::m::self::public_key_id
{};

std::string
ircd::m::self::tls_cert_der
{};

std::string
ircd::m::self::tls_cert_der_sha256_b64
{};

//
// my user
//

ircd::m::user::id::buf
ircd_user_id
{
	"ircd", ircd::my_host()
};

ircd::m::user
ircd::m::me
{
	ircd_user_id
};

//
// my room
//

ircd::m::room::id::buf
ircd_room_id
{
	"ircd", ircd::my_host()
};

ircd::m::room
ircd::m::my_room
{
	ircd_room_id
};

//
// my node
//

ircd::m::node
ircd::m::my_node
{
	ircd::my_host()
};

bool
ircd::m::self::host(const string_view &other)
{
	// port() is 0 when the origin has no port (and implies 8448)
	const auto port
	{
		net::port(hostport(origin))
	};

	// If my_host has a port number, then the argument must also have the
	// same port number.
	if(port)
		return host() == other;

	/// If my_host has no port number, then the argument can have port
	/// 8448 or no port number, which will initialize net::hostport.port to
	/// the "canon_port" of 8448.
	assert(net::canon_port == 8448);
	const net::hostport _other{other};
	if(net::port(_other) != net::canon_port)
		return false;

	if(host() != host(_other))
		return false;

	return true;
}

ircd::string_view
ircd::m::self::host()
{
	return m::self::origin;
}

//
// init
//

/// ID of the room which stores ephemeral tokens (an instance of the room is
/// provided below).
ircd::m::room::id::buf
tokens_room_id
{
	"tokens", ircd::my_host()
};

/// The tokens room serves as a key-value lookup for various tokens to
/// users, etc. It primarily serves to store access tokens for users. This
/// is a separate room from the users room because in the future it may
/// have an optimized configuration as well as being more easily cleared.
///
ircd::m::room
ircd::m::user::tokens
{
	tokens_room_id
};

ircd::m::self::init::init()
try
{
	// Sanity check that these are valid hostname strings. This was likely
	// already checked, so these validators will simply throw without very
	// useful error messages if invalid strings ever make it this far.
	rfc3986::valid_host(origin);
	rfc3986::valid_host(servername);

	ircd_user_id = {"ircd", origin};
	m::me = {ircd_user_id};

	ircd_room_id = {"ircd", origin};
	m::my_room = {ircd_room_id};

	if(origin == "localhost")
		log::warning
		{
			"The origin is configured or has defaulted to 'localhost'"
		};
}
catch(const std::exception &e)
{
	log::critical
	{
		m::log, "Failed to init self origin[%s] servername[%s]",
		origin,
		servername,
	};

	throw;
}

static const auto self_init{[]
{
	ircd::m::self::init();
	return true;
}()};
