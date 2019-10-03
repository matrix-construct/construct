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
#define HAVE_IRCD_M_HOMESERVER_H

namespace ircd::m
{
	struct homeserver;

	IRCD_M_EXCEPTION(error, NOT_MY_HOMESERVER, http::NOT_FOUND)
	IRCD_M_EXCEPTION(error, NOT_A_HOMESERVER, http::SERVICE_UNAVAILABLE)

	string_view origin(const homeserver &);
	string_view server_name(const homeserver &);

	bool origin(const homeserver &, const string_view &);
	bool server_name(const homeserver &, const string_view &);

	string_view public_key_id(const homeserver &);
	const ed25519::sk &secret_key(const homeserver &);

	bool for_each(const std::function<bool (homeserver &)> &);

	homeserver &my(const string_view &origin);
	homeserver &my(); // primary

	user::id me(const string_view &origin);
	user::id me(); // primary
}

///NOTE: instance_multimap is used because there is no instance_map yet.
struct ircd::m::homeserver
:instance_multimap<string_view, homeserver, std::less<>>
{
	struct key;
	struct cert;
	struct opts;

	/// Internal state; use m::my().
	static homeserver *primary;

	/// Options from the user.
	const struct opts *opts;

	/// Federation key related.
	std::unique_ptr<struct key> key;

	/// Database
	std::shared_ptr<dbs::init> database;

	/// List of special room specific to this homeserver. This includes the
	/// fundamental !ircd room, !conf room, etc. This list generally doesn't
	/// change after construction, but it's mutable to allow modules to add
	/// new rooms to the list.
	std::set<m::room::id::buf> rooms;

	/// An inscription of @ircd:network.name to root various references to
	/// a user representing the server.
	m::user::id::buf self;

	/// Loaded modules.
	std::list<ircd::module> modules;

	/// vm
	std::shared_ptr<vm::init> vm;

	homeserver(const struct opts *const &);
	~homeserver() noexcept;

	/// Factory to create homeserver with single procedure for shlib purposes.
	static homeserver *init(const struct opts *);
	static void fini(homeserver *) noexcept;
};

struct ircd::m::homeserver::key
{
	/// Current secret key path
	std::string secret_key_path;

	/// Current federation public key instance
	ed25519::pk public_key;

	/// Current federation secret key instance
	ed25519::sk secret_key;

	/// Current federation public key base64
	std::string public_key_b64;

	/// Current ed25519:ident string
	std::string public_key_id;

	key(const string_view &origin);
	key() = default;
};

struct ircd::m::homeserver::cert
{
	std::string tls_private_key_path;
	std::string tls_public_key_path;
	std::string tls_cert_path;
};

struct ircd::m::homeserver::opts
{
	/// Network name. This is the mxid hostpart (i.e @user:origin).
	string_view origin;

	/// This server's name. This is the unique domain-name of this server
	/// participating in the cluster to serve the origin. The servername may
	/// be the origin itself; otherwise, SRV/well-known indirection is required
	/// to reach the servername starting from the origin.
	string_view server_name;
};
