// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/m/m.h>

namespace ircd::m
{
	static void leave_ircd_room();
	static void join_ircd_room();
}

decltype(ircd::m::log)
ircd::m::log
{
	"matrix", 'm'
};

decltype(ircd::m::listeners)
ircd::m::listeners
{};

//
// my user
//

const ircd::m::user::id::buf
ircd_user_id
{
	"ircd", ircd::my_host()  //TODO: hostname
};

ircd::m::user
ircd::m::me
{
	ircd_user_id
};

//
// my room
//

const ircd::m::room::id::buf
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

const ircd::m::node::id::buf
ircd_node_id
{
	"", ircd::my_host()  //TODO: hostname
};

ircd::m::node
ircd::m::my_node
{
	ircd_node_id
};

//
// misc
//

const ircd::m::room::id::buf
control_room_id
{
	"control", ircd::my_host()
};

ircd::m::room
ircd::m::control
{
	control_room_id
};

//
// init
//

struct ircd::m::init::modules
{
	modules(const json::object &);
	~modules() noexcept;
};

struct ircd::m::init::listeners
{
	listeners(const json::object &);
	~listeners() noexcept;
};

//
// init::init
//

ircd::m::init::init()
try
:config
{
	ircd::conf::config
}
,_keys
{
	this->config
}
,modules{[this]
{
	auto ret{std::make_unique<struct modules>(config)};
	if(db::sequence(*dbs::events) == 0)
		bootstrap();

	return ret;
}()}
,listeners
{
	std::make_unique<struct listeners>(config)
}
{
	join_ircd_room();
}
catch(const m::error &e)
{
	log.error("%s %s", e.what(), e.content);
}
catch(const std::exception &e)
{
	log.error("%s", e.what());
}

ircd::m::init::~init()
noexcept try
{
	leave_ircd_room();
}
catch(const m::error &e)
{
	log.critical("%s %s", e.what(), e.content);
	ircd::terminate();
}

ircd::m::init::modules::modules(const json::object &config)
try
{
	if(ircd::noautomod)
	{
		log::warning
		{
			"Not loading modules because noautomod flag is set. "
			"You may still load modules manually."
		};

		return;
	}

	const string_view prefixes[]
	{
		"s_", "m_", "client_", "key_", "federation_", "media_"
	};

	for(const auto &name : mods::available())
		if(startswith_any(name, std::begin(prefixes), std::end(prefixes)))
			m::modules.emplace(name, name);

	m::modules.emplace("root"s, "root"s);
}
catch(...)
{
	this->~modules();
}

ircd::m::init::modules::~modules()
noexcept
{
	m::modules.clear();
}

namespace ircd::m
{
	static void init_listener(const json::object &config, const string_view &name, const json::object &conf);
}

ircd::m::init::listeners::listeners(const json::object &config)
try
{
	if(ircd::nolisten)
	{
		log::warning
		{
			"Not listening on any addresses because nolisten flag is set."
		};

		return;
	}

	const json::array listeners
	{
		config[{"ircd", "listen"}]
	};

	for(const auto &name : listeners) try
	{
		const json::object &opts
		{
			config.at({"listen", unquote(name)})
		};

		if(!opts.has("tmp_dh_path"))
			throw user_error
			{
				"Listener %s requires a 'tmp_dh_path' in the config. We do not"
				" create this yet. Try `openssl dhparam -outform PEM -out dh512.pem 512`",
				name
			};

		init_listener(config, unquote(name), opts);
	}
	catch(const json::not_found &e)
	{
		throw ircd::user_error
		{
			"Failed to find configuration block for listener %s", name
		};
	}
}
catch(...)
{
	this->~listeners();
}

ircd::m::init::listeners::~listeners()
noexcept
{
	m::listeners.clear();
}

static void
ircd::m::init_listener(const json::object &config,
                       const string_view &name,
                       const json::object &opts)
{
	m::listeners.emplace_back(name, opts);
}

void
ircd::m::init::bootstrap()
{
	assert(dbs::events);
	assert(db::sequence(*dbs::events) == 0);

	ircd::log::notice
	(
		"This appears to be your first time running IRCd because the events "
		"database is empty. I will be bootstrapping it with initial events now..."
	);

	create(user::users, me.user_id);
	create(me.user_id);
	me.activate();

	create(my_room, me.user_id);
	send(my_room, me.user_id, "m.room.name", "",
	{
		{ "name", "IRCd's Room" }
	});

	send(my_room, me.user_id, "m.room.topic", "",
	{
		{ "topic", "The daemon's den." }
	});

	send(user::users, me.user_id, "m.room.name", "",
	{
		{ "name", "Users" }
	});

	create(user::tokens, me.user_id);
	send(user::tokens, me.user_id, "m.room.name", "",
	{
		{ "name", "User Tokens" }
	});
}

bool
ircd::m::self::host(const string_view &s)
{
	return s == host();
}

ircd::string_view
ircd::m::self::host()
{
	return "zemos.net"; //me.user_id.host();
}

ircd::conf::item<std::string>
me_online_status_msg
{
	{ "name",     "ircd.me.online.status_msg"          },
	{ "default",  "Wanna chat? IRCd at your service!"  }
};

void
ircd::m::join_ircd_room()
try
{
	presence::set(me, "online", me_online_status_msg);
	join(my_room, me.user_id);
}
catch(const m::ALREADY_MEMBER &e)
{
	log.warning("IRCd did not shut down correctly...");
}

ircd::conf::item<std::string>
me_offline_status_msg
{
	{ "name",     "ircd.me.offline.status_msg"     },
	{ "default",  "Catch ya on the flip side..."   }
};

void
ircd::m::leave_ircd_room()
{
	leave(my_room, me.user_id);
	presence::set(me, "offline", me_offline_status_msg);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/keys.h
//

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

void
ircd::m::keys::get(const string_view &server_name,
                   const string_view &key_id,
                   const ed25519_closure &closure)
{
	get(server_name, key_id, key_closure{[&closure]
	(const string_view &keyb64)
	{
		const ed25519::pk pk
		{
			[&keyb64](auto &buf)
			{
				b64decode(buf, unquote(keyb64));
			}
		};

		closure(pk);
	}});
}

void
ircd::m::keys::get(const string_view &server_name,
                   const string_view &key_id,
                   const key_closure &closure)
try
{
	get(server_name, [&key_id, &closure](const keys &keys)
	{
		const json::object vks
		{
			at<"verify_keys"_>(keys)
		};

		const json::object vkk
		{
			vks.at(key_id)
		};

		const string_view &key
		{
			vkk.at("key")
		};

		closure(key);
	});
}
catch(const json::not_found &e)
{
	throw m::NOT_FOUND
	{
		"Failed to find key '%s' for '%s': %s",
		key_id,
		server_name,
		e.what()
	};
}

void
ircd::m::keys::get(const string_view &server_name,
                   const string_view &key_id,
                   const closure &closure)
{
	get(server_name, [&key_id, &closure](const keys &keys)
	{
		closure(keys);
	});
}

void
ircd::m::keys::get(const string_view &server_name,
                   const closure &closure_)
{
	using prototype = void (const string_view &, const closure &);

	static import<prototype> function
	{
		"key_keys", "get__keys"
	};

	return function(server_name, closure_);
}

void
ircd::m::keys::get(const string_view &server_name,
                   const string_view &key_id,
                   const string_view &query_server,
                   const closure &closure_)
{
	using prototype = void (const string_view &, const string_view &, const string_view &, const closure &);

	static import<prototype> function
	{
		"key_keys", "query__keys"
	};

	return function(server_name, key_id, query_server, closure_);
}

//
// init
//

ircd::m::keys::init::init(const json::object &config)
:config{config}
{
	certificate();
	signing();
}

ircd::m::keys::init::~init()
noexcept
{
}

void
ircd::m::keys::init::certificate()
{
	const string_view origin
	{
		unquote(this->config.at({"ircd", "origin"}))
	};

	const json::object config
	{
		this->config.at({"origin", origin})
	};

	const std::string private_key_file
	{
		unquote(config.get("ssl_private_key_pem_path"))
	};

	const std::string public_key_file
	{
		unquote(config.get("ssl_public_key_pem_path", private_key_file + ".pub"))
	};

	if(!private_key_file)
		throw user_error
		{
			"You must specify an SSL private key file at"
			" origin.[%s].ssl_private_pem_path even if you do not have one;"
			" it will be created there.",
			origin
		};

	if(!fs::exists(private_key_file))
	{
		log::warning
		{
			"Failed to find certificate private key @ `%s'; creating...",
			private_key_file
		};

		openssl::genrsa(private_key_file, public_key_file);
	}

	const std::string cert_file
	{
		unquote(config.get("ssl_certificate_pem_path"))
	};

	if(!cert_file)
		throw user_error
		{
			"You must specify an SSL certificate file path in the config at"
			" origin.[%s].ssl_certificate_pem_path even if you do not have one;"
			" it will be created there.",
			origin
		};

	if(!fs::exists(cert_file))
	{
		if(!this->config.has({"certificate", origin, "subject"}))
			throw user_error
			{
				"Failed to find SSL certificate @ `%s'. Additionally, no"
				" certificate.[%s].subject was found in the conf to generate one.",
				cert_file,
				origin
			};

		log::warning
		{
			"Failed to find SSL certificate @ `%s'; creating for '%s'...",
			cert_file,
			origin
		};

		const unique_buffer<mutable_buffer> buf
		{
			1_MiB
		};

		const json::strung opts{json::members
		{
			{ "private_key_pem_path",  private_key_file  },
			{ "public_key_pem_path",   public_key_file   },
			{ "subject", this->config.get({"certificate", origin, "subject"}) }
		}};

		const auto cert
		{
			openssl::genX509_rsa(buf, opts)
		};

		fs::overwrite(cert_file, cert);
	}

	const auto cert_pem
	{
		fs::read(cert_file)
	};

	const unique_buffer<mutable_buffer> der_buf
	{
		8_KiB
	};

	const auto cert_der
	{
		openssl::cert2d(der_buf, cert_pem)
	};

	const fixed_buffer<const_buffer, crh::sha256::digest_size> hash
	{
		sha256{cert_der}
	};

	self::tls_cert_der_sha256_b64 =
	{
		b64encode_unpadded(hash)
	};

	log.info("Certificate `%s' :PEM %zu bytes; DER %zu bytes; sha256b64 %s",
	         cert_file,
	         cert_pem.size(),
	         ircd::size(cert_der),
	         self::tls_cert_der_sha256_b64);

	thread_local char print_buf[8_KiB];
	log.info("Certificate `%s' :%s",
	         cert_file,
	         openssl::print_subject(print_buf, cert_pem));
}

void
ircd::m::keys::init::signing()
{
	const string_view origin
	{
		unquote(this->config.at({"ircd", "origin"}))
	};

	const json::object config
	{
		this->config.at({"origin", unquote(origin)})
	};

	const std::string sk_file
	{
		unquote(config.get("ed25519_private_key_path"))
	};

	if(!sk_file)
		throw user_error
		{
			"Failed to find ed25519 secret key path at"
			" origin.[%s].ed25519_private_key_path in config. If you do not"
			" have a private key, specify a path for one to be created.",
			origin
		};

	if(fs::exists(sk_file))
		log.info("Using ed25519 secret key @ `%s'", sk_file);
	else
		log.notice("Creating ed25519 secret key @ `%s'", sk_file);

	self::secret_key = ed25519::sk
	{
		sk_file, &self::public_key
	};

	self::public_key_b64 = b64encode_unpadded(self::public_key);
	const fixed_buffer<const_buffer, sha256::digest_size> hash
	{
		sha256{self::public_key}
	};

	const auto public_key_hash_b58
	{
		b58encode(hash)
	};

	static const auto trunc_size{8};
	self::public_key_id = fmt::snstringf
	{
		BUFSIZE, "ed25519:%s", trunc(public_key_hash_b58, trunc_size)
	};

	log.info("Current key is '%s' and the public key is: %s",
	         self::public_key_id,
	         self::public_key_b64);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/receipt.h
//

ircd::m::event::id::buf
ircd::m::receipt::read(const id::room &room_id,
                       const id::user &user_id,
                       const id::event &event_id)
{
	return read(room_id, user_id, event_id, ircd::time<milliseconds>());
}

ircd::m::event::id::buf
ircd::m::receipt::read(const id::room &room_id,
                       const id::user &user_id,
                       const id::event &event_id,
                       const time_t &ms)
{
	using prototype = event::id::buf (const id::room &, const id::user &, const id::event &, const time_t &);

	static import<prototype> function
	{
		"client_rooms", "commit__m_receipt_m_read"
	};

	return function(room_id, user_id, event_id, ms);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/typing.h
//

ircd::m::event::id::buf
ircd::m::typing::set(const m::typing &object)
{
	using prototype = event::id::buf (const m::typing &);

	static import<prototype> function
	{
		"client_rooms", "commit__m_typing"
	};

	return function(object);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/presence.h
//

ircd::m::presence::presence(const user &user,
                            const mutable_buffer &buf)
:presence
{
	get(user, buf)
}
{
}

ircd::m::event::id::buf
ircd::m::presence::set(const user &user,
                       const string_view &presence,
                       const string_view &status_msg)
{
	return set(m::presence
	{
		{ "user_id",     user.user_id  },
		{ "presence",    presence      },
		{ "status_msg",  status_msg    },
	});
}

ircd::m::event::id::buf
ircd::m::presence::set(const presence &object)
{
	using prototype = event::id::buf (const presence &);

	static import<prototype> function
	{
		"client_presence", "commit__m_presence"
	};

	return function(object);
}

ircd::json::object
ircd::m::presence::get(const user &user,
                       const mutable_buffer &buffer)
{
	using prototype = json::object (const m::user &, const mutable_buffer &);

	static import<prototype> function
	{
		"client_presence", "m_presence_get"
	};

	return function(user, buffer);
}

bool
ircd::m::presence::valid_state(const string_view &state)
{
	using prototype = bool (const string_view &);

	static import<prototype> function
	{
		"m_presence", "presence_valid_state"
	};

	return function(state);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/node.h
//

/// ID of the room which indexes all nodes (an instance of the room is
/// provided below).
const ircd::m::room::id::buf
nodes_room_id
{
	"nodes", ircd::my_host()
};

/// The nodes room is the database of all nodes. It primarily serves as an
/// indexing mechanism and for top-level node related keys.
///
const ircd::m::room
ircd::m::nodes
{
	nodes_room_id
};

ircd::m::node
ircd::m::create(const id::node &node_id,
                const json::members &args)
{
	using prototype = node (const id::node &, const json::members &);

	static import<prototype> function
	{
		"s_node", "create_node"
	};

	assert(node_id);
	return function(node_id, args);
}

bool
ircd::m::exists(const node::id &node_id)
{
	using prototype = bool (const node::id &);

	static import<prototype> function
	{
		"s_node", "exists__nodeid"
	};

	return function(node_id);
}

bool
ircd::m::my(const node &node)
{
	return my(node.node_id);
}

//
// node
//

/// Generates a node-room ID into buffer; see room_id() overload.
ircd::m::id::room::buf
ircd::m::node::room_id()
const
{
	ircd::m::id::room::buf buf;
	return buf.assigned(room_id(buf));
}

/// This generates a room mxid for the "node's room" essentially serving as
/// a database mechanism for this specific node. This room_id is a hash of
/// the node's full mxid.
///
ircd::m::id::room
ircd::m::node::room_id(const mutable_buffer &buf)
const
{
	assert(!empty(node_id));
	const sha256::buf hash
	{
		sha256{node_id}
	};

	char b58[size(hash) * 2];
	return
	{
		buf, b58encode(b58, hash), my_host()
	};
}

//
// node::room
//

ircd::m::node::room::room(const m::node::id &node_id)
:room{m::node{node_id}}
{}

ircd::m::node::room::room(const m::node &node)
:node{node}
,room_id{node.room_id()}
{
	static_cast<m::room &>(*this) = room_id;
}

///////////////////////////////////////////////////////////////////////////////
//
// m/rooms.h
//

void
ircd::m::rooms::for_each(const user &user,
                         const user::rooms::closure &closure)
{
	const m::user::rooms rooms{user};
	rooms.for_each(closure);
}

void
ircd::m::rooms::for_each(const user &user,
                         const user::rooms::closure_bool &closure)
{
	const m::user::rooms rooms{user};
	rooms.for_each(closure);
}

void
ircd::m::rooms::for_each(const user &user,
                         const string_view &membership,
                         const user::rooms::closure &closure)
{
	const m::user::rooms rooms{user};
	rooms.for_each(membership, closure);
}

void
ircd::m::rooms::for_each(const user &user,
                         const string_view &membership,
                         const user::rooms::closure_bool &closure)
{
	const m::user::rooms rooms{user};
	rooms.for_each(membership, closure);
}

void
ircd::m::rooms::for_each(const room::closure &closure)
{
	for_each(room::closure_bool{[&closure]
	(const room &room)
	{
		closure(room);
		return true;
	}});
}

void
ircd::m::rooms::for_each(const room::closure_bool &closure)
{
	for_each(room::id::closure_bool{[&closure]
	(const room::id &room_id)
	{
		return closure(room_id);
	}});
}

void
ircd::m::rooms::for_each(const room::id::closure &closure)
{
	for_each(room::id::closure_bool{[&closure]
	(const room::id &room_id)
	{
		closure(room_id);
		return true;
	}});
}

void
ircd::m::rooms::for_each(const room::id::closure_bool &closure)
{
	const room::state state
	{
		my_room
	};

	state.test("ircd.room", room::state::keys_bool{[&closure]
	(const string_view &key)
	{
		return !closure(key);
	}});
}

///////////////////////////////////////////////////////////////////////////////
//
// m/user.h
//

/// ID of the room which indexes all users (an instance of the room is
/// provided below).
const ircd::m::room::id::buf
users_room_id
{
	"users", ircd::my_host()
};

/// The users room is the database of all users. It primarily serves as an
/// indexing mechanism and for top-level user related keys. Accounts
/// registered on this server will be among state events in this room.
/// Users do not have access to this room, it is used internally.
///
ircd::m::room
ircd::m::user::users
{
	users_room_id
};

/// ID of the room which stores ephemeral tokens (an instance of the room is
/// provided below).
const ircd::m::room::id::buf
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

ircd::m::user
ircd::m::create(const id::user &user_id,
                const json::members &contents)
{
	using prototype = user (const id::user &, const json::members &);

	static import<prototype> function
	{
		"client_user", "user_create"
	};

	return function(user_id, contents);
}

bool
ircd::m::exists(const user::id &user_id)
{
	return user::users.has("ircd.user", user_id);
}

bool
ircd::m::exists(const user &user)
{
	return exists(user.user_id);
}

bool
ircd::m::my(const user &user)
{
	return my(user.user_id);
}

//
// user
//

/// Generates a user-room ID into buffer; see room_id() overload.
ircd::m::id::room::buf
ircd::m::user::room_id()
const
{
	ircd::m::id::room::buf buf;
	return buf.assigned(room_id(buf));
}

/// This generates a room mxid for the "user's room" essentially serving as
/// a database mechanism for this specific user. This room_id is a hash of
/// the user's full mxid.
///
ircd::m::id::room
ircd::m::user::room_id(const mutable_buffer &buf)
const
{
	assert(!empty(user_id));
	const ripemd160::buf hash
	{
		ripemd160{user_id}
	};

	char b58[size(hash) * 2];
	return
	{
		buf, b58encode(b58, hash), my_host()
	};
}

ircd::string_view
ircd::m::user::gen_access_token(const mutable_buffer &buf)
{
	static const size_t token_max{32};
	static const auto &token_dict{rand::dict::alpha};

	const mutable_buffer out
	{
		data(buf), std::min(token_max, size(buf))
	};

	return rand::string(token_dict, out);
}

ircd::m::event::id::buf
ircd::m::user::activate()
{
	using prototype = event::id::buf (const m::user &);

	static import<prototype> function
	{
		"client_account", "activate__user"
	};

	return function(*this);
}

ircd::m::event::id::buf
ircd::m::user::deactivate()
{
	using prototype = event::id::buf (const m::user &);

	static import<prototype> function
	{
		"client_account", "deactivate__user"
	};

	return function(*this);
}

bool
ircd::m::user::is_active()
const
{
	using prototype = bool (const m::user &);

	static import<prototype> function
	{
		"client_account", "is_active__user"
	};

	return function(*this);
}

ircd::m::event::id::buf
ircd::m::user::account_data(const m::user &sender,
                            const string_view &type,
                            const json::object &val)
{
	using prototype = event::id::buf (const m::user &, const m::user &, const string_view &, const json::object &);

	static import<prototype> function
	{
		"client_user", "account_data_set"
	};

	return function(*this, sender, type, val);
}

ircd::json::object
ircd::m::user::account_data(const mutable_buffer &out,
                            const string_view &type)
const
{
	json::object ret;
	account_data(std::nothrow, type, [&out, &ret]
	(const json::object &val)
	{
		ret = string_view { data(out), copy(out, val) };
	});

	return ret;
}

bool
ircd::m::user::account_data(std::nothrow_t,
                            const string_view &type,
                            const account_data_closure &closure)
const try
{
	account_data(type, closure);
	return true;
}
catch(const std::exception &e)
{
	return false;
}

void
ircd::m::user::account_data(const string_view &type,
                            const account_data_closure &closure)
const
{
	using prototype = void (const m::user &, const string_view &, const account_data_closure &);

	static import<prototype> function
	{
		"client_user", "account_data_get"
	};

	return function(*this, type, closure);
}

ircd::m::event::id::buf
ircd::m::user::profile(const m::user &sender,
                       const string_view &key,
                       const string_view &val)
{
	using prototype = event::id::buf (const m::user &, const m::user &, const string_view &, const string_view &);

	static import<prototype> function
	{
		"client_profile", "profile_set"
	};

	return function(*this, sender, key, val);
}

ircd::string_view
ircd::m::user::profile(const mutable_buffer &out,
                       const string_view &key)
const
{
	string_view ret;
	profile(std::nothrow, key, [&out, &ret]
	(const string_view &val)
	{
		ret = { data(out), copy(out, val) };
	});

	return ret;
}

bool
ircd::m::user::profile(std::nothrow_t,
                       const string_view &key,
                       const profile_closure &closure)
const try
{
	profile(key, closure);
	return true;
}
catch(const std::exception &e)
{
	return false;
}

void
ircd::m::user::profile(const string_view &key,
                       const profile_closure &closure)
const
{
	using prototype = void (const m::user &, const string_view &, const profile_closure &);

	static import<prototype> function
	{
		"client_profile", "profile_get"
	};

	return function(*this, key, closure);
}

ircd::m::event::id::buf
ircd::m::user::password(const string_view &password)
{
	using prototype = event::id::buf (const m::user::id &, const string_view &) noexcept;

	static import<prototype> function
	{
		"client_account", "set_password"
	};

	return function(user_id, password);
}

bool
ircd::m::user::is_password(const string_view &password)
const noexcept try
{
	using prototype = bool (const m::user::id &, const string_view &) noexcept;

	static import<prototype> function
	{
		"client_account", "is_password"
	};

	return function(user_id, password);
}
catch(const std::exception &e)
{
	log::critical
	{
		"user::is_password(): %s %s",
		string_view{user_id},
		e.what()
	};

	return false;
}

//
// user::room
//

ircd::m::user::room::room(const m::user::id &user_id)
:room{m::user{user_id}}
{}

ircd::m::user::room::room(const m::user &user)
:user{user}
,room_id{user.room_id()}
{
	static_cast<m::room &>(*this) = room_id;
}

//
// user::rooms
//

ircd::m::user::rooms::rooms(const m::user &user)
:user_room{user}
{
}

size_t
ircd::m::user::rooms::count()
const
{
	size_t ret{0};
	for_each([&ret]
	(const m::room &, const string_view &membership)
	{
		++ret;
	});

	return ret;
}

size_t
ircd::m::user::rooms::count(const string_view &membership)
const
{
	size_t ret{0};
	for_each(membership, [&ret]
	(const m::room &, const string_view &membership)
	{
		++ret;
	});

	return ret;
}

void
ircd::m::user::rooms::for_each(const closure &closure)
const
{
	for_each(closure_bool{[&closure]
	(const m::room &room, const string_view &membership)
	{
		closure(room, membership);
		return true;
	}});
}

void
ircd::m::user::rooms::for_each(const closure_bool &closure)
const
{
	const m::room::state state{user_room};
	state.test("ircd.member", [&closure]
	(const m::event &event)
	{
		const m::room::id &room_id
		{
			at<"state_key"_>(event)
		};

		const string_view &membership
		{
			unquote(at<"content"_>(event).at("membership"))
		};

		return !closure(room_id, membership);
	});
}

void
ircd::m::user::rooms::for_each(const string_view &membership,
                               const closure &closure)
const
{
	for_each(membership, closure_bool{[&closure]
	(const m::room &room, const string_view &membership)
	{
		closure(room, membership);
		return true;
	}});
}

void
ircd::m::user::rooms::for_each(const string_view &membership,
                               const closure_bool &closure)
const
{
	const m::room::state state{user_room};
	state.test("ircd.member", [&membership, &closure]
	(const m::event &event)
	{
		const string_view &membership_
		{
			unquote(at<"content"_>(event).at("membership"))
		};

		if(membership_ != membership)
			return false;

		const m::room::id &room_id
		{
			at<"state_key"_>(event)
		};

		return !closure(room_id, membership);
	});
}

///////////////////////////////////////////////////////////////////////////////
//
// m/room.h
//

ircd::m::room
ircd::m::create(const id::room &room_id,
                const id::user &creator,
                const string_view &type)
{
	using prototype = room (const id::room &, const id::user &, const string_view &);

	static import<prototype> function
	{
		"client_createroom", "createroom__type"
	};

	return function(room_id, creator, type);
}

ircd::m::room
ircd::m::create(const id::room &room_id,
                const id::user &creator,
                const id::room &parent,
                const string_view &type)
{
	using prototype = room (const id::room &, const id::user &, const id::room &, const string_view &);

	static import<prototype> function
	{
		"client_createroom", "createroom__parent_type"
	};

	return function(room_id, creator, parent, type);
}

ircd::m::event::id::buf
ircd::m::join(const id::room_alias &room_alias,
              const id::user &user_id)
{
	using prototype = event::id::buf (const id::room_alias &, const id::user &);

	static import<prototype> function
	{
		"client_rooms", "join__alias_user"
	};

	return function(room_alias, user_id);
}

ircd::m::event::id::buf
ircd::m::join(const room &room,
              const id::user &user_id)
{
	using prototype = event::id::buf (const m::room &, const id::user &);

	static import<prototype> function
	{
		"client_rooms", "join__room_user"
	};

	return function(room, user_id);
}

ircd::m::event::id::buf
ircd::m::leave(const room &room,
               const id::user &user_id)
{
	using prototype = event::id::buf (const m::room &, const id::user &);

	static import<prototype> function
	{
		"client_rooms", "leave__room_user"
	};

	return function(room, user_id);
}

ircd::m::event::id::buf
ircd::m::invite(const room &room,
                const id::user &target,
                const id::user &sender)
{
	using prototype = event::id::buf (const m::room &, const id::user &, const id::user &);

	static import<prototype> function
	{
		"client_rooms", "invite__room_user"
	};

	return function(room, target, sender);
}

ircd::m::event::id::buf
ircd::m::redact(const room &room,
                const id::user &sender,
                const id::event &event_id,
                const string_view &reason)
{
	using prototype = event::id::buf (const m::room &, const id::user &, const id::event &, const string_view &);

	static import<prototype> function
	{
		"client_rooms", "redact__"
	};

	return function(room, sender, event_id, reason);
}

ircd::m::event::id::buf
ircd::m::notice(const room &room,
                const string_view &body)
{
	return message(room, me.user_id, body, "m.notice");
}

ircd::m::event::id::buf
ircd::m::notice(const room &room,
                const m::id::user &sender,
                const string_view &body)
{
	return message(room, sender, body, "m.notice");
}

ircd::m::event::id::buf
ircd::m::msghtml(const room &room,
                 const m::id::user &sender,
                 const string_view &html,
                 const string_view &alt,
                 const string_view &msgtype)
{
	return message(room, sender,
	{
		{ "msgtype",         msgtype                       },
		{ "format",          "org.matrix.custom.html"      },
		{ "body",            { alt?: html, json::STRING }  },
		{ "formatted_body",  { html, json::STRING }        },
	});
}

ircd::m::event::id::buf
ircd::m::message(const room &room,
                 const m::id::user &sender,
                 const string_view &body,
                 const string_view &msgtype)
{
	return message(room, sender,
	{
		{ "body",     { body,    json::STRING } },
		{ "msgtype",  { msgtype, json::STRING } },
	});
}

ircd::m::event::id::buf
ircd::m::message(const room &room,
                 const m::id::user &sender,
                 const json::members &contents)
{
	return send(room, sender, "m.room.message", contents);
}

ircd::m::event::id::buf
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const string_view &state_key,
              const json::members &contents)
{
	json::iov _content;
	json::iov::push content[contents.size()];
	return send(room, sender, type, state_key, make_iov(_content, content, contents.size(), contents));
}

ircd::m::event::id::buf
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const string_view &state_key,
              const json::object &contents)
{
	json::iov _content;
	json::iov::push content[contents.size()];
	return send(room, sender, type, state_key, make_iov(_content, content, contents.size(), contents));
}

ircd::m::event::id::buf
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const string_view &state_key,
              const json::iov &content)
{
	using prototype = event::id::buf (const m::room &, const id::user &, const string_view &, const string_view &, const json::iov &);

	static import<prototype> function
	{
		"client_rooms", "state__iov"
	};

	return function(room, sender, type, state_key, content);
}

ircd::m::event::id::buf
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const json::members &contents)
{
	json::iov _content;
	json::iov::push content[contents.size()];
	return send(room, sender, type, make_iov(_content, content, contents.size(), contents));
}

ircd::m::event::id::buf
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const json::object &contents)
{
	json::iov _content;
	json::iov::push content[contents.count()];
	return send(room, sender, type, make_iov(_content, content, contents.count(), contents));
}

ircd::m::event::id::buf
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const json::iov &content)
{
	using prototype = event::id::buf (const m::room &, const id::user &, const string_view &, const json::iov &);

	static import<prototype> function
	{
		"client_rooms", "send__iov"
	};

	return function(room, sender, type, content);
}

ircd::m::event::id::buf
ircd::m::commit(const room &room,
                json::iov &event,
                const json::iov &contents)
{
	using prototype = event::id::buf (const m::room &, json::iov &, const json::iov &);

	static import<prototype> function
	{
		"client_rooms", "commit__iov_iov"
	};

	return function(room, event, contents);
}

ircd::m::id::room::buf
ircd::m::room_id(const id::room_alias &room_alias)
{
	char buf[256];
	return room_id(buf, room_alias);
}

ircd::m::id::room::buf
ircd::m::room_id(const string_view &room_id_or_alias)
{
	char buf[256];
	return room_id(buf, room_id_or_alias);
}

ircd::m::id::room
ircd::m::room_id(const mutable_buffer &out,
                 const string_view &room_id_or_alias)
{
	switch(m::sigil(room_id_or_alias))
	{
		case id::ROOM:
			return id::room{out, room_id_or_alias};

		default:
			return room_id(out, id::room_alias{room_id_or_alias});
	}
}

ircd::m::id::room
ircd::m::room_id(const mutable_buffer &out,
                 const id::room_alias &room_alias)
{
	using prototype = id::room (const mutable_buffer &, const id::room_alias &);

	static import<prototype> function
	{
		"client_directory_room", "room_id__room_alias"
	};

	return function(out, room_alias);
}

bool
ircd::m::exists(const id::room_alias &room_alias,
                const bool &remote_query)
{
	using prototype = bool (const id::room_alias, const bool &);

	static import<prototype> function
	{
		"client_directory_room", "room_alias_exists"
	};

	return function(room_alias, remote_query);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/txn.h
//

/// Returns the serial size of the JSON this txn would consume. Note: this
/// creates a json::iov involving a timestamp to figure out the total size
/// of the txn. When the user creates the actual txn a different timestamp
/// is created which may be a different size. Consider using the lower-level
/// create(closure) or add some pad to be sure.
///
size_t
ircd::m::txn::serialized(const array &pdu,
                         const array &edu,
                         const array &pdu_failure)
{
	size_t ret;
	const auto closure{[&ret]
	(const json::iov &iov)
	{
		ret = json::serialized(iov);
	}};

	create(closure, pdu, edu, pdu_failure);
	return ret;
}

/// Stringifies a txn from the inputs into the returned std::string
///
std::string
ircd::m::txn::create(const array &pdu,
                     const array &edu,
                     const array &pdu_failure)
{
	std::string ret;
	const auto closure{[&ret]
	(const json::iov &iov)
	{
		ret = json::strung(iov);
	}};

	create(closure, pdu, edu, pdu_failure);
	return ret;
}

/// Stringifies a txn from the inputs into the buffer
///
ircd::string_view
ircd::m::txn::create(const mutable_buffer &buf,
                     const array &pdu,
                     const array &edu,
                     const array &pdu_failure)
{
	string_view ret;
	const auto closure{[&buf, &ret]
	(const json::iov &iov)
	{
		ret = json::stringify(mutable_buffer{buf}, iov);
	}};

	create(closure, pdu, edu, pdu_failure);
	return ret;
}

/// Forms a txn from the inputs into a json::iov and presents that iov
/// to the user's closure.
///
void
ircd::m::txn::create(const closure &closure,
                     const array &pdu,
                     const array &edu,
                     const array &pdu_failure)
{
	using ircd::size;

	json::iov iov;
	const json::iov::push push[]
	{
		{ iov, { "origin",            my_host()                   }},
		{ iov, { "origin_server_ts",  ircd::time<milliseconds>()  }},
	};

	const json::iov::add_if _pdus
	{
		iov, !empty(pdu),
		{
			"pdus", { data(pdu), size(pdu) }
		}
	};

	const json::iov::add_if _edus
	{
		iov, !empty(edu),
		{
			"edus", { data(edu), size(edu) }
		}
	};

	const json::iov::add_if _pdu_failures
	{
		iov, !empty(pdu_failure),
		{
			"pdu_failures", { data(pdu_failure), size(pdu_failure) }
		}
	};

	closure(iov);
}

ircd::string_view
ircd::m::txn::create_id(const mutable_buffer &out,
                        const string_view &txn)
{
	const sha256::buf hash
	{
		sha256{txn}
	};

	const string_view txnid
	{
		b58encode(out, hash)
	};

	return txnid;
}

///////////////////////////////////////////////////////////////////////////////
//
// m/hook.h
//

/// Alternative hook ctor simply allowing the the function argument
/// first and description after.
ircd::m::hook::hook(decltype(function) function,
                    const json::members &members)
:hook
{
	members, std::move(function)
}
{
}

/// Primary hook ctor
ircd::m::hook::hook(const json::members &members,
                    decltype(function) function)
try
:_feature{[&members]() -> json::strung
{
	const ctx::critical_assertion ca;
	std::vector<json::member> copy
	{
		begin(members), end(members)
	};

	for(auto &member : copy) switch(hash(member.first))
	{
		case hash("room_id"):
		{
			// Rewrite the room_id if the supplied input has no hostname
			if(valid_local_only(id::ROOM, member.second))
			{
				assert(my_host());
				thread_local char buf[256];
				member.second = id::room { buf, member.second, my_host() };
			}

			validate(id::ROOM, member.second);
			continue;
		}

		case hash("sender"):
		{
			// Rewrite the sender if the supplied input has no hostname
			if(valid_local_only(id::USER, member.second))
			{
				assert(my_host());
				thread_local char buf[256];
				member.second = id::user { buf, member.second, my_host() };
			}

			validate(id::USER, member.second);
			continue;
		}

		case hash("state_key"):
		{
			const bool is_member_event
			{
				end(members) != std::find_if(begin(members), end(members), [](const auto &member)
				{
					return member.first == "type" && member.second == "m.room.member";
				})
			};

			// Rewrite the sender if the supplied input has no hostname
			if(valid_local_only(id::USER, member.second))
			{
				assert(my_host());
				thread_local char buf[256];
				member.second = id::user { buf, member.second, my_host() };
			}

			validate(id::USER, member.second);
			continue;
		}
	}

	return { copy.data(), copy.data() + copy.size() };
}()}
,feature
{
	_feature
}
,matching
{
	feature
}
,function
{
	std::move(function)
}
,registered
{
	list.add(*this)
}
{
}
catch(...)
{
	if(registered)
		list.del(*this);
}

ircd::m::hook::~hook()
noexcept
{
	if(registered)
		list.del(*this);
}

ircd::string_view
ircd::m::hook::site_name()
const try
{
	return unquote(feature.at("_site"));
}
catch(const std::out_of_range &e)
{
	throw assertive
	{
		"Hook %p must name a '_site' to register with.", this
	};
}

bool
ircd::m::hook::match(const m::event &event)
const
{
	if(json::get<"origin"_>(matching))
		if(at<"origin"_>(matching) != json::get<"origin"_>(event))
			return false;

	if(json::get<"room_id"_>(matching))
		if(at<"room_id"_>(matching) != json::get<"room_id"_>(event))
			return false;

	if(json::get<"sender"_>(matching))
		if(at<"sender"_>(matching) != json::get<"sender"_>(event))
			return false;

	if(json::get<"type"_>(matching))
		if(at<"type"_>(matching) != json::get<"type"_>(event))
			return false;

	if(json::get<"state_key"_>(matching))
		if(at<"state_key"_>(matching) != json::get<"state_key"_>(event))
			return false;

	if(json::get<"membership"_>(matching))
		if(at<"membership"_>(matching) != json::get<"membership"_>(event))
			return false;

	if(json::get<"content"_>(matching))
		if(json::get<"type"_>(event) == "m.room.message")
			if(at<"content"_>(matching).has("msgtype"))
				if(at<"content"_>(matching).get("msgtype") != json::get<"content"_>(event).get("msgtype"))
					return false;

	return true;
}

//
// hook::site
//

ircd::m::hook::site::site(const json::members &members)
try
:_feature
{
	members
}
,feature
{
	_feature
}
,registered
{
	list.add(*this)
}
{
}
catch(...)
{
	if(registered)
		list.del(*this);
}

ircd::m::hook::site::~site()
noexcept
{
	if(registered)
		list.del(*this);
}

void
ircd::m::hook::site::operator()(const event &event)
{
	std::set<hook *> matching;     //TODO: allocator
	const auto site_match{[&matching]
	(auto &map, const string_view &key)
	{
		auto pit{map.equal_range(key)};
		for(; pit.first != pit.second; ++pit.first)
			matching.emplace(pit.first->second);
	}};

	if(json::get<"origin"_>(event))
		site_match(origin, at<"origin"_>(event));

	if(json::get<"room_id"_>(event))
		site_match(room_id, at<"room_id"_>(event));

	if(json::get<"sender"_>(event))
		site_match(sender, at<"sender"_>(event));

	if(json::get<"type"_>(event))
		site_match(type, at<"type"_>(event));

	if(json::get<"state_key"_>(event))
		site_match(state_key, at<"state_key"_>(event));

	auto it(begin(matching));
	while(it != end(matching))
	{
		const hook &hook(**it);
		if(!hook.match(event))
			it = matching.erase(it);
		else
			++it;
	}

	for(const auto &hook : matching)
		call(*hook, event);
}

void
ircd::m::hook::site::call(hook &hook,
                          const event &event)
try
{
	hook.function(event);
}
catch(const ctx::interrupted &e)
{
	throw;
}
catch(const std::exception &e)
{
	log::critical
	{
		"Unhandled hookfn(%p) %s error :%s",
		&hook,
		string_view{hook.feature},
		e.what()
	};
}

bool
ircd::m::hook::site::add(hook &hook)
{
	if(json::get<"origin"_>(hook.matching))
		origin.emplace(at<"origin"_>(hook.matching), &hook);

	if(json::get<"room_id"_>(hook.matching))
		room_id.emplace(at<"room_id"_>(hook.matching), &hook);

	if(json::get<"sender"_>(hook.matching))
		sender.emplace(at<"sender"_>(hook.matching), &hook);

	if(json::get<"state_key"_>(hook.matching))
		state_key.emplace(at<"state_key"_>(hook.matching), &hook);

	if(json::get<"type"_>(hook.matching))
		type.emplace(at<"type"_>(hook.matching), &hook);

	++count;
	return true;
}

bool
ircd::m::hook::site::del(hook &hook)
{
	const auto unmap{[&hook]
	(auto &map, const string_view &key)
	{
		auto pit{map.equal_range(key)};
		for(; pit.first != pit.second; ++pit.first)
			if(pit.first->second == &hook)
				return map.erase(pit.first);

		assert(0);
		return end(map);
	}};

	if(json::get<"origin"_>(hook.matching))
		unmap(origin, at<"origin"_>(hook.matching));

	if(json::get<"room_id"_>(hook.matching))
		unmap(room_id, at<"room_id"_>(hook.matching));

	if(json::get<"sender"_>(hook.matching))
		unmap(sender, at<"sender"_>(hook.matching));

	if(json::get<"state_key"_>(hook.matching))
		unmap(state_key, at<"state_key"_>(hook.matching));

	if(json::get<"type"_>(hook.matching))
		unmap(type, at<"type"_>(hook.matching));

	--count;
	return true;
}

ircd::string_view
ircd::m::hook::site::name()
const try
{
	return unquote(feature.at("name"));
}
catch(const std::out_of_range &e)
{
	throw assertive
	{
		"Hook site %p requires a name", this
	};
}

//
// hook::list
//

decltype(ircd::m::hook::list)
ircd::m::hook::list
{};

bool
ircd::m::hook::list::del(hook &hook)
try
{
	const auto site(at(hook.site_name()));
	assert(site != nullptr);
	return site->del(hook);
}
catch(const std::out_of_range &e)
{
	log::critical
	{
		"Tried to unregister hook(%p) from missing hook::site '%s'",
		&hook,
		hook.site_name()
	};

	assert(0);
	return false;
}

bool
ircd::m::hook::list::add(hook &hook)
try
{
	const auto site(at(hook.site_name()));
	assert(site != nullptr);
	return site->add(hook);
}
catch(const std::out_of_range &e)
{
	throw error
	{
		"No hook::site named '%s' is registered...", hook.site_name()
	};
}

bool
ircd::m::hook::list::del(site &site)
{
	return erase(site.name());
}

bool
ircd::m::hook::list::add(site &site)
{
	const auto iit
	{
		emplace(site.name(), &site)
	};

	if(unlikely(!iit.second))
		throw error
		{
			"Hook site name '%s' already in use", site.name()
		};

	return true;
}

///////////////////////////////////////////////////////////////////////////////
//
// m/import.h
//

decltype(ircd::m::modules)
ircd::m::modules
{};

///////////////////////////////////////////////////////////////////////////////
//
// m/error.h
//

thread_local char
ircd::m::error::fmtbuf[768]
{};

ircd::m::error::error()
:http::error{http::INTERNAL_SERVER_ERROR}
{}

ircd::m::error::error(std::string c)
:http::error{http::INTERNAL_SERVER_ERROR, std::move(c)}
{}

ircd::m::error::error(const http::code &c)
:http::error{c, std::string{}}
{}

ircd::m::error::error(const http::code &c,
                      const json::members &members)
:http::error{c, json::strung{members}}
{}

ircd::m::error::error(const http::code &c,
                      const json::iov &iov)
:http::error{c, json::strung{iov}}
{}

ircd::m::error::error(const http::code &c,
                      const json::object &object)
:http::error{c, std::string{object}}
{}
