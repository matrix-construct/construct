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

#include <ircd/m/m.h>

///////////////////////////////////////////////////////////////////////////////
//
// m.h
//

namespace ircd::m
{
	struct log::log log
	{
		"matrix", 'm'
	};

	std::map<std::string, ircd::module> modules;
	std::list<ircd::net::listener> listeners;

	static void leave_ircd_room();
	static void join_ircd_room();
	static void init_bootstrap(const json::object &conf);
	static void init_listeners(const json::object &conf);
	static void init_modules(const json::object &conf);
	static void init_cert(const json::object &conf);
	static void init_keys(const json::object &conf);
}

const ircd::m::room::id::buf
init_room_id
{
	"init", ircd::my_host()
};

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

ircd::m::init::init()
try
{
	const json::object conf
	{
		ircd::conf
	};

	init_modules(conf);
	init_keys(conf);
	init_cert(conf);
	init_bootstrap(conf);
	init_listeners(conf);
	join_ircd_room();
}
catch(const m::error &e)
{
	log.error("%s %s", e.what(), e.content);
	throw;
}
catch(const std::exception &e)
{
	log.error("%s", e.what());
	throw;
}

ircd::m::init::~init()
noexcept try
{
	leave_ircd_room();
	listeners.clear();
	vm::fronts.map.clear();
	modules.clear();
}
catch(const m::error &e)
{
	log.critical("%s %s", e.what(), e.content);
	ircd::terminate();
}

namespace ircd::m
{
	static void init_listener(const json::object &conf, const json::object &opts, const string_view &bindaddr);
	static void init_listener(const json::object &conf, const json::object &opts);
}

static void
ircd::m::init_listeners(const json::object &conf)
{
	const json::array listeners
	{
		conf["listeners"]
	};

	if(listeners.empty())
		init_listener(conf, {});
	else
		for(const json::object opts : listeners)
			init_listener(conf, opts);
}

static void
ircd::m::init_listener(const json::object &conf,
                       const json::object &opts)
{
	const json::array binds
	{
		opts["bind_addresses"]
	};

	if(binds.empty())
		init_listener(conf, opts, "0.0.0.0");
	else
		for(const auto &bindaddr : binds)
			init_listener(conf, opts, unquote(bindaddr));
}

static void
ircd::m::init_listener(const json::object &conf,
                       const json::object &opts,
                       const string_view &host)
{
	const json::array resources
	{
		opts["resources"]
	};

	// resources has multiple names with different configs which are being
	// ignored :-/
	const std::string name{"Matrix"s};

	// Translate synapse options to our options (which reflect asio::ssl)
	const json::strung options{json::members
	{
		{ "name",                      name                          },
		{ "host",                      host                          },
		{ "port",                      opts.get("port", 8448)        },
		{ "ssl_certificate_file_pem",  conf["tls_certificate_path"]  },
		{ "ssl_private_key_file_pem",  conf["tls_private_key_path"]  },
		{ "ssl_tmp_dh_file",           conf["tls_dh_params_path"]    },
	}};

	listeners.emplace_back(options);
}

static void
ircd::m::init_modules(const json::object &conf)
{
	const string_view prefixes[]
	{
		"m_", "client_", "key_", "federation_", "media_"
	};

	for(const auto &name : mods::available())
		if(startswith_any(name, std::begin(prefixes), std::end(prefixes)))
			modules.emplace(name, name);

	modules.emplace("root.so"s, "root.so"s);
}

void
ircd::m::join_ircd_room()
try
{
	join(my_room, me.user_id);
}
catch(const m::ALREADY_MEMBER &e)
{
	log.warning("IRCd did not shut down correctly...");
}

void
ircd::m::leave_ircd_room()
{
	leave(my_room, me.user_id);
}

namespace ircd::m
{
	static void bootstrap_keys();
	static void bootstrap();
}

void
ircd::m::init_bootstrap(const json::object &conf)
{
	if(db::sequence(*event::events) == 0)
		bootstrap();
}

void
ircd::m::bootstrap()
{
	assert(event::events);
	assert(db::sequence(*event::events) == 0);

	ircd::log::notice
	(
		"This appears to be your first time running IRCd because the events "
		"database is empty. I will be bootstrapping it with initial events now..."
	);

	create(my_room, me.user_id);
	send(my_room, me.user_id, "m.room.name", "",
	{
		{ "name", "IRCd's Room" }
	});

	create(control, me.user_id);
	send(control, me.user_id, "m.room.name", "",
	{
		{ "name", "Control Room" }
	});

	create(user::accounts, me.user_id);
	join(user::accounts, me.user_id);
	send(user::accounts, me.user_id, "m.room.name", "",
	{
		{ "name", "User Accounts" }
	});

	create(user::sessions, me.user_id);
	send(user::sessions, me.user_id, "m.room.name", "",
	{
		{ "name", "User Sessions" }
	});

	create(filter::filters, me.user_id);
	send(filter::filters, me.user_id, "m.room.name", "",
	{
		{ "name", "User Filters Database" }
	});

	bootstrap_keys();

	message(control, me.user_id, "Welcome to the control room.");
	message(control, me.user_id, "I am the daemon. You can talk to me in this room by highlighting me.");
}

//
// dbs
//

namespace ircd::m::dbs
{
	std::map<std::string, ircd::module> modules;
	std::map<std::string, import_shared<database>> databases;

	void init_modules();
	void init_databases();
}

ircd::m::dbs::init::init()
{
	init_modules();
	init_databases();

	ircd::m::event::events = databases.at("events").get();
}

ircd::m::dbs::init::~init()
noexcept
{
	ircd::m::event::events = nullptr;

	databases.clear();
	modules.clear();
}

void
ircd::m::dbs::init_databases()
{
	for(const auto &pair : modules)
	{
		const auto &name(pair.first);
		const auto dbname(mods::unpostfixed(name));
		const std::string shortname(lstrip(dbname, "db_"));
		const std::string symname(shortname + "_database"s);
		databases.emplace(shortname, import_shared<database>
		{
			dbname, symname
		});
	}
}

void
ircd::m::dbs::init_modules()
{
	for(const auto &name : mods::available())
		if(startswith(name, "db_"))
			modules.emplace(name, name);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/keys.h
//

const ircd::m::room::id::buf
keys_room_id
{
	"keys", ircd::my_host()
};

/// The keys room is where the public key data for each server is stored as
/// state indexed by the server name.
ircd::m::room
ircd::m::keys::room
{
	keys_room_id
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

static void
ircd::m::init_cert(const json::object &options)
{
	const std::string cert_file
	{
		unquote(options.at("tls_certificate_path"))
	};

	if(!fs::exists(cert_file))
		throw fs::error("Failed to find SSL certificate @ `%s'", cert_file);

	const auto cert_pem
	{
		fs::read(cert_file)
	};

	const unique_buffer<mutable_raw_buffer> der_buf
	{
		8_KiB
	};

	const auto cert_der
	{
		openssl::cert2d(der_buf, cert_pem)
	};

	const fixed_buffer<const_raw_buffer, crh::sha256::digest_size> hash
	{
		sha256{cert_der}
	};

	self::tls_cert_der_sha256_b64 =
	{
		b64encode_unpadded(hash)
	};

	log.info("Certificate `%s' (PEM: %zu bytes; DER: %zu bytes) sha256b64: %s",
	         cert_file,
	         cert_pem.size(),
	         size(cert_der),
	         self::tls_cert_der_sha256_b64);
}

static void
ircd::m::init_keys(const json::object &options)
{
	const std::string sk_file
	{
		unquote(options.get("signing_key_path", "construct.sk"))
	};

	if(fs::exists(sk_file))
		log.info("Using ed25519 secret key @ `%s'", sk_file);
	else
		log.notice("Creating new ed25519 secret key @ `%s'", sk_file);

	self::secret_key = ed25519::sk
	{
		sk_file, &self::public_key
	};

	self::public_key_b64 = b64encode_unpadded(self::public_key);
	const fixed_buffer<const_raw_buffer, sha256::digest_size> hash
	{
		sha256{const_raw_buffer{self::public_key}}
	};

	const auto public_key_hash_b64
	{
		b64encode_unpadded(hash)
	};

	self::public_key_id = fmt::snstringf
	{
		BUFSIZE, "ed25519:%s", public_key_hash_b64
	};

	log.info("Current key is '%s' and the public key is: %s",
	         self::public_key_id,
	         self::public_key_b64);
}

static void
ircd::m::bootstrap_keys()
{
	create(keys::room, me.user_id);
	send(keys::room, me.user_id, "m.room.name", "",
	{
		{ "name", "Key Room" }
	});

	const json::strung verify_keys
	{
		json::members
		{{
			string_view{self::public_key_id},
			{
				{ "key", self::public_key_b64 }
			}
		}}
	};

	keys my_key;
	json::get<"verify_keys"_>(my_key) = verify_keys;
	json::get<"server_name"_>(my_key) = my_host();
	json::get<"old_verify_keys"_>(my_key) = "{}";
	json::get<"valid_until_ts"_>(my_key) = ircd::time<milliseconds>() + duration_cast<milliseconds>(hours(2160)).count();

	const json::members tlsfps
	{
		{ "sha256", self::tls_cert_der_sha256_b64 }
	};

	const json::value tlsfp[1]
	{
		{ tlsfps }
	};

	const json::strung tls_fingerprints{json::value
	{
		tlsfp, 1
	}};

	json::get<"tls_fingerprints"_>(my_key) = tls_fingerprints;

	const auto presig
	{
		json::strung(my_key)
	};

	const ed25519::sig sig
	{
		self::secret_key.sign(const_raw_buffer{presig})
	};

	static char signature[256];
	const json::strung signatures{json::members
	{
		{ my_host(), json::members
		{
			{ string_view{self::public_key_id}, b64encode_unpadded(signature, sig) }
		}}
	}};

	json::get<"signatures"_>(my_key) = signatures;
	keys::set(my_key);
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

		// The key is not unquote() because some types of keys may be
		// more complex than just a string one day; think: RLWE.
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
                   const keys_closure &closure)
{
	assert(!server_name.empty());

	if(get_local(server_name, closure))
		return;

	if(server_name == my_host())
		throw m::NOT_FOUND
		{
			"keys for '%s' (that's myself) not found", server_name
		};

	log.debug("Keys for %s not cached; querying network...",
	          server_name);

	char url[1024]; const auto url_len
	{
		fmt::snprintf(url, sizeof(url), "_matrix/key/v2/server/")
	};

	//TODO: XXX
	const unique_buffer<mutable_buffer> buffer
	{
		8192
	};

	ircd::parse::buffer pb{mutable_buffer{buffer}};
	m::request request{"GET", url, {}, {}};
	m::session session{server_name};
	const json::object response
	{
		session(pb, request)
	};

	const m::keys &keys
	{
		response
	};

	if(!verify(keys))
		throw m::error
		{
			http::UNAUTHORIZED, "M_INVALID_SIGNATURE",
			"Failed to verify keys for '%s'", server_name
		};

	log.debug("Verified keys from '%s'",
	          server_name);

	set(keys);
	closure(keys);
}

void
ircd::m::keys::get(const string_view &server_name,
                   const string_view &key_id,
                   const string_view &query_server,
                   const keys_closure &closure)
try
{
	assert(!server_name.empty());
	assert(!query_server.empty());

	char key_id_buf[1024];
	char server_name_buf[1024];
	char url[1024]; const auto url_len
	{
		fmt::snprintf(url, sizeof(url), "_matrix/key/v2/query/%s/%s/",
		              urlencode(server_name, server_name_buf),
		              urlencode(key_id, key_id_buf))
	};

	//TODO: XXX
	const unique_buffer<mutable_buffer> buffer
	{
		8192
	};

	// Make request and receive response synchronously.
	// This ircd::ctx will block here fetching.
	ircd::parse::buffer pb{mutable_buffer{buffer}};
	m::request request{"GET", url, {}, {}};
	m::session session{server_name};
	const json::object response
	{
		session(pb, request)
	};

	const json::array &keys
	{
		response.at("server_keys")
	};

	log::debug("Fetched %zu candidate keys seeking '%s' for '%s' from '%s' (%s)",
	           keys.count(),
	           empty(key_id)? "*" : key_id,
	           server_name,
	           query_server,
	           string(net::remote(session.server)));

	bool ret{false};
	for(auto it(begin(keys)); it != end(keys); ++it)
	{
		const m::keys &keys{*it};
		const auto &_server_name
		{
			at<"server_name"_>(keys)
		};

		if(!verify(keys))
			throw m::error
			{
				http::UNAUTHORIZED, "M_INVALID_SIGNATURE",
				"Failed to verify keys for '%s' from '%s'",
				_server_name,
				query_server
			};

		log.debug("Verified keys for '%s' from '%s'",
		          _server_name,
		          query_server);

		set(keys);
		const json::object vks{json::get<"verify_keys"_>(keys)};
		if(_server_name == server_name)
		{
			closure(keys);
			ret = true;
		}
	}

	if(!ret)
		throw m::NOT_FOUND
		{
			"Failed to get any keys for '%s' from '%s' (got %zu total keys otherwise)",
			server_name,
			query_server,
			keys.count()
		};
}
catch(const json::not_found &e)
{
	throw m::NOT_FOUND
	{
		"Failed to find key '%s' for '%s' when querying '%s': %s",
		key_id,
		server_name,
		query_server,
		e.what()
	};
}

bool
ircd::m::keys::get_local(const string_view &server_name,
                         const keys_closure &closure)
{
	const m::vm::query<m::vm::where::equal> query
	{
		{ "room_id",      keys::room.room_id   },
		{ "type",        "ircd.key"            },
		{ "state_key",    server_name          },
	};

	const auto have
	{
		[&closure](const auto &event)
		{
			closure(json::get<"content"_>(event));
			return true;
		}
	};

	return m::vm::test(query, have);
}

void
ircd::m::keys::set(const keys &keys)
{
	const auto &state_key
	{
		unquote(at<"server_name"_>(keys))
	};

	const m::user::id::buf sender
	{
		"ircd", unquote(at<"server_name"_>(keys))
	};

	const json::strung content
	{
		keys
	};

	json::iov event;
	json::iov::push members[]
	{
		{ event, { "type",       "ircd.key"  }},
		{ event, { "state_key",  state_key   }},
		{ event, { "sender",     sender      }},
		{ event, { "content",    content     }}
	};

	keys::room.send(event);
}

/// Verify this key data (with itself).
bool
ircd::m::keys::verify(const keys &keys)
noexcept try
{
	const auto &valid_until_ts
	{
		at<"valid_until_ts"_>(keys)
	};

	if(valid_until_ts < ircd::time<milliseconds>())
		throw ircd::error("Key was valid until %s", timestr(valid_until_ts));

	const json::object &verify_keys
	{
		at<"verify_keys"_>(keys)
	};

	const string_view &key_id
	{
		begin(verify_keys)->first
	};

	const json::object &key
	{
		begin(verify_keys)->second
	};

	const ed25519::pk pk
	{
		[&key](auto &pk)
		{
			b64decode(pk, unquote(key.at("key")));
		}
	};

	const json::object &signatures
	{
		at<"signatures"_>(keys)
	};

	const string_view &server_name
	{
		unquote(at<"server_name"_>(keys))
	};

	const json::object &server_signatures
	{
		signatures.at(server_name)
	};

	const ed25519::sig sig{[&server_signatures, &key_id](auto &sig)
	{
		b64decode(sig, unquote(server_signatures.at(key_id)));
	}};

	///TODO: XXX
	m::keys copy{keys};
	at<"signatures"_>(copy) = string_view{};
	const json::strung preimage{copy};
	return pk.verify(const_raw_buffer{preimage}, sig);
}
catch(const std::exception &e)
{
	log.error("key verification for '%s' failed: %s",
	          json::get<"server_name"_>(keys, "<no server name>"_sv),
	          e.what());

	return false;
}

///////////////////////////////////////////////////////////////////////////////
//
// m/filter.h
//

const ircd::m::room::id::buf
filters_room_id
{
	"filters", ircd::my_host()
};

ircd::m::room
ircd::m::filter::filters
{
	filters_room_id
};

ircd::m::filter::filter(const string_view &filter_id,
                        const mutable_buffer &buf)
{
	const m::vm::query<m::vm::where::equal> query
	{
		{ "room_id",      filters.room_id   },
		{ "type",        "ircd.filter"      },
		{ "state_key",    filter_id         },
	};

	size_t len{0};
	m::vm::test(query, [&buf, &len]
	(const auto &event)
	{
		len = copy(buf, json::get<"content"_>(event));
		return true;
	});

	new (this) filter{json::object{buf}};
}

size_t
ircd::m::filter::size(const string_view &filter_id)
{
	const m::vm::query<m::vm::where::equal> query
	{
		{ "room_id",      filters.room_id   },
		{ "type",        "ircd.filter"      },
		{ "state_key",    filter_id         },
	};

	size_t ret{0};
	m::vm::test(query, [&ret]
	(const auto &event)
	{
		const string_view content
		{
			json::get<"content"_>(event)
		};

		ret = content.size();
		return true;
	});

	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//
// m/room.h
//

//
// room::state
//

ircd::m::room::state::state(const room::id &room_id,
                            const event::id &event_id,
                            const mutable_buffer &buf)
{
	fetch tab
	{
		event_id, room_id, buf
	};

	new (this) state{tab};
}

ircd::m::room::state::state(fetch &tab)
{
	io::acquire(tab);

	if(bool(tab.error))
		std::rethrow_exception(tab.error);

	new (this) state{tab.pdus};
}

ircd::m::room::state::state(const json::array &pdus)
{
	for(const json::object &pdu : pdus)
	{
		const m::event &event{pdu};
		json::set(*this, at<"type"_>(event), event);
	}
}

//
// room
//

ircd::m::room::room(const id::alias &alias)
:room_id{}
{}

ircd::m::room
ircd::m::create(const id::room &room_id,
                const id::user &creator,
                const string_view &type)
{
	return create(room_id, creator, init_room_id, type);
}

ircd::m::room
ircd::m::create(const id::room &room_id,
                const id::user &creator,
                const id::room &parent,
                const string_view &type)
{
	json::iov event;
	json::iov content;
	const json::iov::push push[]
	{
		{ event,     { "sender",   creator  }},
		{ content,   { "creator",  creator  }},
	};

	const json::iov::add_if _parent
	{
		content, !parent.empty() && parent.local() != "init",
		{
			"parent", parent
		}
	};

	const json::iov::add_if _type
	{
		content, !type.empty() && type != "room",
		{
			"type", type
		}
	};

	room room
	{
		room_id
	};

	room.create(event, content);
	return room;
}

ircd::m::event::id::buf
ircd::m::join(const m::room::id &room_id,
              const m::id::user &user_id)
{
	return membership(room_id, user_id, "join");
}

ircd::m::event::id::buf
ircd::m::leave(const m::room::id &room_id,
               const m::id::user &user_id)
{
	return membership(room_id, user_id, "leave");
}

ircd::m::event::id::buf
ircd::m::membership(const m::id::room &room_id,
                    const m::id::user &user_id,
                    const string_view &membership)
{
	json::iov event;
	json::iov content;
	json::iov::push push[]
	{
		{ event,    { "sender",      user_id     }},
		{ content,  { "membership",  membership  }},
	};

	room room
	{
		room_id
	};

	return room.membership(event, content);
}

ircd::m::event::id::buf
ircd::m::message(const m::id::room &room_id,
                 const m::id::user &sender,
                 const string_view &body,
                 const string_view &msgtype)
{
	return message(room_id, sender,
	{
		{ "body",      body     },
		{ "msgtype",   msgtype  }
	});
}

ircd::m::event::id::buf
ircd::m::message(const m::id::room &room_id,
                 const m::id::user &sender,
                 const json::members &contents)
{
	return send(room_id, sender, "m.room.message", contents);
}

ircd::m::event::id::buf
ircd::m::send(const m::id::room &room_id,
              const m::id::user &sender,
              const string_view &type,
              const string_view &state_key,
              const json::members &contents)
{
	size_t i(0);
	json::iov content;
	json::iov::push _content[contents.size()];
	for(const auto &member : contents)
		new (_content + i++) json::iov::push(content, member);

	return send(room_id, sender, type, state_key, content);
}

ircd::m::event::id::buf
ircd::m::send(const m::id::room &room_id,
              const m::id::user &sender,
              const string_view &type,
              const string_view &state_key,
              const json::iov &content)
{
	json::iov event;
	const json::iov::push push[]
	{
		{ event,    { "sender",     sender     }},
		{ event,    { "type",       type       }},
		{ event,    { "state_key",  state_key  }},
	};

	room room
	{
		room_id
	};

	return room.send(event, content);
}

ircd::m::event::id::buf
ircd::m::send(const m::id::room &room_id,
              const m::id::user &sender,
              const string_view &type,
              const json::members &contents)
{
	size_t i(0);
	json::iov content;
	json::iov::push _content[contents.size()];
	for(const auto &member : contents)
		new (_content + i++) json::iov::push(content, member);

	room room
	{
		room_id
	};

	return send(room_id, sender, type, content);
}

ircd::m::event::id::buf
ircd::m::send(const m::id::room &room_id,
              const m::id::user &sender,
              const string_view &type,
              const json::iov &content)
{
	json::iov event;
	const json::iov::push push[]
	{
		{ event,    { "sender",  sender  }},
		{ event,    { "type",    type    }},
	};

	room room
	{
		room_id
	};

	return room.send(event, content);
}

bool
ircd::m::exists(const id::room &room_id)
{
	const vm::query<vm::where::equal> query
	{
		{ "room_id", room_id },
	};

	return m::vm::test(query);
}

ircd::m::event::id::buf
ircd::m::room::create(json::iov &event,
                      json::iov &content)
{
	const json::iov::defaults defaults[]
	{
		{ event,    { "sender",   me.user_id  }},
		{ content,  { "creator",  me.user_id  }},
	};

	json::iov::set _set[]
	{
		{ event,  { "depth",       1                }},
		{ event,  { "type",        "m.room.create"  }},
		{ event,  { "state_key",   ""               }},
	};

	return send(event, content);
}

ircd::m::event::id::buf
ircd::m::room::membership(json::iov &event,
                          const json::iov &content)
{
	const user::id &user_id
	{
		event.at("sender")
	};

	const string_view membership
	{
		content.at("membership")
	};

	if(this->membership(user_id, membership))
		throw m::ALREADY_MEMBER
		{
			"Member '%s' is already '%s'.", string_view{user_id}, membership
		};

	const json::iov::set _event[]
	{
		{ event,  { "type",       "m.room.member"  }},
		{ event,  { "state_key",   user_id         }},
		{ event,  { "membership",  membership      }},
	};

	return send(event, content);
}

ircd::m::event::id::buf
ircd::m::room::message(json::iov &event,
                       const json::iov &content)
{
	const json::iov::set _type[]
	{
		{ event,  { "type", "m.room.message" }},
	};

	const json::strung c //TODO: child iov
	{
		content
	};

	const json::iov::set_if _content[]
	{
		{ event, !content.empty(), { "content", string_view{c} }},
	};

	return send(event);
}

ircd::m::event::id::buf
ircd::m::room::send(json::iov &event,
                    const json::iov &content)
{
	const json::strung c //TODO: child iov
	{
		content
	};

	const json::iov::set_if _content[]
	{
		{ event, !content.empty(), { "content", string_view{c} }},
	};

	return send(event);
}

ircd::m::event::id::buf
ircd::m::room::send(json::iov &event)
{
	const json::iov::set room_id
	{
		event, { "room_id", this->room_id }
	};

	//std::cout << this->room_id << " at " << this->maxdepth() << std::endl;

	// TODO: XXX
	// commitment to room here @ exclusive acquisition of depth

	const json::iov::defaults depth
	{
		event, { "depth", int64_t(this->maxdepth()) + 1 }
	};

	return m::vm::commit(event);
}

bool
ircd::m::room::membership(const m::id::user &user_id,
                          const string_view &membership)
const
{
	const vm::query<vm::where::equal> member_event
	{
		{ "room_id",      room_id           },
		{ "type",        "m.room.member"    },
		{ "state_key",    user_id           },
	};

	if(!membership)
		return m::vm::test(member_event);

	const vm::query<vm::where::test> membership_test{[&membership]
	(const auto &event)
	{
		const json::object &content
		{
			json::at<"content"_>(event)
		};

		const auto &existing_membership
		{
			unquote(content.at("membership"))
		};

		return membership == existing_membership;
	}};

	return m::vm::test(member_event && membership_test);
}

bool
ircd::m::room::get(const string_view &type,
                   const event::closure &closure)
const
{
	return get(type, "", closure);
}

bool
ircd::m::room::get(const string_view &type,
                   const string_view &state_key,
                   const event::closure &closure)
const
{
	const vm::query<vm::where::equal> query
	{
		{ "room_id",      room_id           },
		{ "type",         type              },
		{ "state_key",    state_key         },
	};

	return m::vm::test(query, [&closure]
	(const auto &event)
	{
		closure(event);
		return true;
	});
}

bool
ircd::m::room::has(const string_view &type)
const
{
	return test(type, [](const auto &event)
	{
		return true;
	});
}

bool
ircd::m::room::has(const string_view &type,
                   const string_view &state_key)
const
{
	const vm::query<vm::where::equal> query
	{
		{ "room_id",      room_id           },
		{ "type",         type              },
		{ "state_key",    state_key         },
	};

	return m::vm::test(query);
}

void
ircd::m::room::for_each(const string_view &type,
                        const event::closure &closure)
const
{
	const vm::query<vm::where::equal> query
	{
		{ "room_id",      room_id           },
		{ "type",         type              },
	};

	return m::vm::for_each(query, closure);
}

bool
ircd::m::room::test(const string_view &type,
                    const event::closure_bool &closure)
const
{
	const vm::query<vm::where::equal> query
	{
		{ "room_id",      room_id           },
		{ "type",         type              },
	};

	return m::vm::test(query, closure);
}

/// academic search
uint64_t
ircd::m::room::maxdepth()
const
{
	event::id::buf buf;
	return maxdepth(buf);
}

/// academic search
uint64_t
ircd::m::room::maxdepth(event::id::buf &buf)
const
{
	const vm::query<vm::where::equal> query
	{
		{ "room_id", room_id },
	};

	int64_t depth{0};
	vm::for_each(query, [&buf, &depth]
	(const auto &event)
	{
		if(json::get<"depth"_>(event) > depth)
		{
			depth = json::get<"depth"_>(event);
			buf = json::get<"event_id"_>(event);
		}
	});

	return depth;
}

std::string
ircd::m::pretty(const room::state &state)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 2048);

	const auto out{[&s]
	(const string_view &key, const auto &event)
	{
		if(!json::get<"event_id"_>(event))
			return;

		s << std::setw(28) << std::right << key
		  << " : " << at<"event_id"_>(event)
		  << " " << json::get<"sender"_>(event)
		  << " " << json::get<"depth"_>(event)
		  << " " << pretty_oneline(event::prev{event})
		  << std::endl;
	}};

	json::for_each(state, out);
	resizebuf(s, ret);
	return ret;
}

std::string
ircd::m::pretty_oneline(const room::state &state)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 1024);

	const auto out{[&s]
	(const string_view &key, const auto &event)
	{
		if(!json::get<"event_id"_>(event))
			return;

		s << key << " ";
	}};

	json::for_each(state, out);
	resizebuf(s, ret);
	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//
// m/user.h
//

const ircd::m::room::id::buf
accounts_room_id
{
	"accounts", ircd::my_host()
};

ircd::m::room
ircd::m::user::accounts
{
	accounts_room_id
};

const ircd::m::room::id::buf
sessions_room_id
{
	"sessions", ircd::my_host()
};

ircd::m::room
ircd::m::user::sessions
{
	sessions_room_id
};

/// Register the user by joining them to the accounts room.
///
/// The content of the join event may store keys including the registration
/// options. Once this call completes the join was successful and the user is
/// registered, otherwise throws.
void
ircd::m::user::activate(const json::members &contents)
try
{
	json::iov content;
	json::iov::push push[]
	{
		{ content, { "membership", "join" }},
	};

	size_t i(0);
	json::iov::push _content[contents.size()];
	for(const auto &member : contents)
		new (_content + i++) json::iov::push(content, member);

	send(accounts, me.user_id, "ircd.user", user_id, content);
}
catch(const m::ALREADY_MEMBER &e)
{
	throw m::error
	{
		http::CONFLICT, "M_USER_IN_USE", "The desired user ID is already in use."
	};
}

void
ircd::m::user::deactivate(const json::members &contents)
{
	json::iov content;
	json::iov::push push[]
	{
		{ content, { "membership", "leave" }},
	};

	size_t i(0);
	json::iov::push _content[contents.size()];
	for(const auto &member : contents)
		new (_content + i++) json::iov::push(content, member);

	send(accounts, me.user_id, "ircd.user", user_id, content);
}

void
ircd::m::user::password(const string_view &password)
try
{
	//TODO: ADD SALT
	char b64[64];
	uint8_t hash[32];
	sha256{hash, const_buffer{password}};
	const auto digest{b64encode_unpadded(b64, hash)};
	send(accounts, me.user_id, "ircd.password", user_id,
	{
		{ "sha256", digest }
	});
}
catch(const m::ALREADY_MEMBER &e)
{
	throw m::error
	{
		http::CONFLICT, "M_USER_IN_USE", "The desired user ID is already in use."
	};
}

bool
ircd::m::user::is_password(const string_view &supplied_password)
const
{
	const vm::query<vm::where::equal> member_event
	{
		{ "room_id",      accounts.room_id  },
		{ "type",        "ircd.password"    },
		{ "state_key",    user_id           },
	};

	//TODO: ADD SALT
	char b64[64];
	uint8_t hash[32];
	sha256{hash, const_buffer{supplied_password}};
	const auto supplied_hash{b64encode_unpadded(b64, hash)};
	const vm::query<vm::where::test> correct_password{[&supplied_hash]
	(const auto &event)
	{
		const json::object &content
		{
			json::at<"content"_>(event)
		};

		const auto &correct_hash
		{
			unquote(content.at("sha256"))
		};

		return supplied_hash == correct_hash;
	}};

	const auto query
	{
		member_event && correct_password
	};

	return m::vm::test(member_event && correct_password);
}

bool
ircd::m::user::is_active()
const
{
	bool ret{false};
	accounts.get("ircd.user", user_id, [&ret]
	(const auto &event)
	{
		const json::object &content
		{
			at<"content"_>(event)
		};

		ret = unquote(content.at("membership")) == "join";
	});

	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//
// m/event.h
//

ircd::database *
ircd::m::event::events
{};

ircd::m::event::event(const id &id,
                      const mutable_buffer &buf)
{
	fetch tab
	{
		id, buf
	};

	new (this) event{tab};
}

ircd::m::event::event(fetch &tab)
{
	io::acquire(tab);

	if(bool(tab.error))
		std::rethrow_exception(tab.error);

	new (this) super_type{tab.pdu};
}

ircd::m::event::temporality
ircd::m::temporality(const event &event,
                     const int64_t &rel)
{
	const auto &depth
	{
		json::get<"depth"_>(event)
	};

	return depth > rel?   event::temporality::FUTURE:
	       depth == rel?  event::temporality::PRESENT:
	                      event::temporality::PAST;
}

ircd::m::event::lineage
ircd::m::lineage(const event &event)
{
	const json::array prev[]
	{
		json::get<"prev_events"_>(event),
		json::get<"auth_events"_>(event),
		json::get<"prev_state"_>(event),
	};

	const auto count{std::accumulate(begin(prev), end(prev), size_t(0), []
	(auto ret, const auto &array)
	{
		return ret += array.count();
	})};

	return count > 1?   event::lineage::MERGE:
	       count == 1?  event::lineage::FORWARD:
	                    event::lineage::ROOT;
}

ircd::string_view
ircd::m::reflect(const event::lineage &lineage)
{
	switch(lineage)
	{
		case event::lineage::MERGE:    return "MERGE";
		case event::lineage::FORWARD:  return "FORWARD";
		case event::lineage::ROOT:     return "ROOT";
	}

	return "?????";
}

ircd::string_view
ircd::m::reflect(const event::temporality &temporality)
{
	switch(temporality)
	{
		case event::temporality::FUTURE:   return "FUTURE";
		case event::temporality::PRESENT:  return "PRESENT";
		case event::temporality::PAST:     return "PAST";
	}

	return "?????";
}

size_t
ircd::m::degree(const event &event)
{
	return degree(event::prev{event});
}

size_t
ircd::m::degree(const event::prev &prev)
{
	size_t ret{0};
	json::for_each(prev, [&ret]
	(const auto &, const json::array &prevs)
	{
		ret += prevs.count();
	});

	return ret;
}

size_t
ircd::m::count(const event::prev &prev)
{
	size_t ret{0};
	m::for_each(prev, [&ret](const event::id &event_id)
	{
		++ret;
	});

	return ret;
}

void
ircd::m::for_each(const event::prev &prev,
                  const std::function<void (const event::id &)> &closure)
{
	json::for_each(prev, [&closure]
	(const auto &key, const json::array &prevs)
	{
		for(const json::array &prev : prevs)
		{
			const event::id &id{unquote(prev[0])};
			closure(id);
		}
	});
}

std::string
ircd::m::pretty(const event::prev &prev)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 2048);

	const auto out{[&s]
	(const string_view &key, auto&& val)
	{
		if(json::defined(val))
			s << key << ": " << val << std::endl;
	}};

	const auto &auth_events{json::get<"auth_events"_>(prev)};
	for(const json::array auth_event : auth_events)
		out("auth_event", unquote(auth_event[0]));

	const auto &prev_states{json::get<"prev_state"_>(prev)};
	for(const json::array prev_state : prev_states)
		out("prev_state", unquote(prev_state[0]));

	const auto &prev_events{json::get<"prev_events"_>(prev)};
	for(const json::array prev_event : prev_events)
		out("prev_event", unquote(prev_event[0]));

	resizebuf(s, ret);
	return ret;
}

std::string
ircd::m::pretty_oneline(const event::prev &prev)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 1024);

	const auto &auth_events{json::get<"auth_events"_>(prev)};
	s << "A[ ";
	for(const json::array auth_event : auth_events)
		s << unquote(auth_event[0]) << " ";
	s << "] ";

	const auto &prev_states{json::get<"prev_state"_>(prev)};
	s << "S[ ";
	for(const json::array prev_state : prev_states)
		s << unquote(prev_state[0]) << " ";
	s << "] ";

	const auto &prev_events{json::get<"prev_events"_>(prev)};
	s << "E[ ";
	for(const json::array prev_event : prev_events)
		s << unquote(prev_event[0]) << " ";
	s << "] ";

	resizebuf(s, ret);
	return ret;
}

std::string
ircd::m::pretty(const event &event)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 2048);

	const auto out{[&s]
	(const string_view &key, auto&& val)
	{
		if(json::defined(val))
			s << std::setw(16) << std::right << key << ": " << val << std::endl;
	}};

	const string_view top_keys[]
	{
		"origin",
		"event_id",
		"room_id",
		"sender",
		"type",
		"depth",
		"state_key",
		"membership",
	};

	json::for_each(event, top_keys, out);

	const auto &hashes{json::get<"hashes"_>(event)};
	for(const auto &hash : hashes)
	{
		s << std::setw(16) << std::right << "[hash]" << ": "
		  << hash.first
		  //<< " "
		  //<< hash.second
		  << std::endl;
	}

	const auto &signatures{json::get<"signatures"_>(event)};
	for(const auto &signature : signatures)
	{
		s << std::setw(16) << std::right << "[signature]" << ": "
		  << signature.first << " ";

		for(const auto &key : json::object{signature.second})
			s << key.first << " ";

		s << std::endl;
	}

	const json::object &contents{json::get<"content"_>(event)};
	if(!contents.empty())
	{
		s << std::setw(16) << std::right << "[content]" << ": ";
		for(const auto &content : contents)
			s << content.first << ", ";
		s << std::endl;
	}

	const auto &auth_events{json::get<"auth_events"_>(event)};
	for(const json::array auth_event : auth_events)
		out("[auth_event]", unquote(auth_event[0]));

	const auto &prev_states{json::get<"prev_state"_>(event)};
	for(const json::array prev_state : prev_states)
		out("[prev_state]", unquote(prev_state[0]));

	const auto &prev_events{json::get<"prev_events"_>(event)};
	for(const json::array prev_event : prev_events)
		out("[prev_event]", unquote(prev_event[0]));

	resizebuf(s, ret);
	return ret;
}

std::string
ircd::m::pretty_oneline(const event &event)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 1024);

	const auto out{[&s]
	(const string_view &key, auto&& val)
	{
		if(defined(val))
			s << val << " ";
		else
			s << "* ";
	}};

	const string_view top_keys[]
	{
		"origin",
		"event_id",
		"room_id",
		"sender",
		"depth",
	};

	s << ':';
	json::for_each(event, top_keys, out);

	const auto &auth_events{json::get<"auth_events"_>(event)};
	s << "pa:" << auth_events.count() << " ";

	const auto &prev_states{json::get<"prev_state"_>(event)};
	s << "ps:" << prev_states.count() << " ";

	const auto &prev_events{json::get<"prev_events"_>(event)};
	s << "pe:" << prev_events.count() << " ";

	const auto &hashes{json::get<"hashes"_>(event)};
	s << "[ ";
	for(const auto &hash : hashes)
		s << hash.first << " ";
	s << "] ";

	const auto &signatures{json::get<"signatures"_>(event)};
	s << "[ ";
	for(const auto &signature : signatures)
	{
		s << signature.first << "[ ";
		for(const auto &key : json::object{signature.second})
			s << key.first << " ";

		s << "] ";
	}
	s << "] ";

	out("type", json::get<"type"_>(event));

	const auto &state_key
	{
		json::get<"state_key"_>(event)
	};

	if(defined(state_key) && empty(state_key))
		s << "\"\"" << " ";
	else if(defined(state_key))
		s << state_key << " ";
	else
		s << "*" << " ";

	const json::object &contents{json::get<"content"_>(event)};
	if(!contents.empty())
	{
		s << "+" << string_view{contents}.size() << " bytes :";
		for(const auto &content : contents)
			s << content.first << " ";
	}

	resizebuf(s, ret);
	return ret;
}
