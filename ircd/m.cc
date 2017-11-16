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
	ircd::net::listener *listener;

	static void leave_ircd_room();
	static void join_ircd_room();
	static void bootstrap();
	static void init_keys(const json::object &options);
	static void init_cert(const json::object &options);
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
	const json::strung options{json::members
	{
		{ "name", "Chat Matrix" },
		{ "host", "0.0.0.0" },
		{ "port", 8448 },
		{ "ssl_certificate_file", "/home/jason/.synapse/zemos.net.crt" },
		{ "ssl_certificate_chain_file", "/home/jason/.synapse/zemos.net.crt" },
		{ "ssl_tmp_dh_file", "/home/jason/.synapse/cdc.z.tls.dh" },
		{ "ssl_private_key_file_pem", "/home/jason/.synapse/cdc.z.tls.key" },
		{ "secret_key_file", "/home/jason/charybdis.sk" },
	}};

	init_keys(options);
	init_cert(options);

	const string_view prefixes[]
	{
		"m_", "client_", "key_", "federation_", "media_"
	};

	for(const auto &name : mods::available())
		if(startswith_any(name, std::begin(prefixes), std::end(prefixes)))
			modules.emplace(name, name);

	if(db::sequence(*event::events) == 0)
		bootstrap();

	modules.emplace("root.so"s, "root.so"s);

	//TODO: conf obviously
	listener = new ircd::net::listener{options};

	join_ircd_room();
}
catch(const m::error &e)
{
	log.critical("%s %s", e.what(), e.content);
}

ircd::m::init::~init()
noexcept try
{
	leave_ircd_room();

	delete listener;
	modules.clear();
}
catch(const m::error &e)
{
	log.critical("%s %s", e.what(), e.content);
	std::terminate();
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
	create(control, me.user_id);
	create(user::accounts, me.user_id);
	create(user::sessions, me.user_id);
	create(filter::filters, me.user_id);
	join(user::accounts, me.user_id);
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
// m/session.h
//

ircd::m::io::session::session(const net::remote &remote)
:server{remote}
,destination{remote.hostname}
{
}

ircd::json::object
ircd::m::io::session::operator()(parse::buffer &pb,
                                 request &request)
{
	request.destination = destination;
	request(server);
	return response
	{
		server, pb
	};
}

//
// response
//

ircd::m::io::response::response(server &server,
                                parse::buffer &pb)
{
	http::code status;
	json::object &object
	{
		static_cast<json::object &>(*this)
	};

	parse::capstan pc
	{
		pb, read_closure(server)
	};

	http::response
	{
		pc,
		nullptr,
		[&pc, &status, &object](const http::response::head &head)
		{
			status = http::status(head.status);
			object = http::response::content{pc, head};
		},
		[](const auto &header)
		{
			//std::cout << header.first << " :" << header.second << std::endl;
		}
	};

	if(status < 200 || status >= 300)
		throw m::error(status, object);
}

//
// request
//

namespace ircd::m::name
{
//	constexpr const char *const content {"content"};
	constexpr const char *const destination {"destination"};
	constexpr const char *const method {"method"};
//	constexpr const char *const origin {"origin"};
	constexpr const char *const uri {"uri"};
}

struct ircd::m::io::request::authorization
:json::tuple
<
	json::property<name::content, string_view>,
	json::property<name::destination, string_view>,
	json::property<name::method, string_view>,
	json::property<name::origin, string_view>,
	json::property<name::uri, string_view>
>
{
	string_view generate(const mutable_buffer &out);

	using super_type::tuple;
};

void
ircd::m::io::request::operator()(const vector_view<const http::header> &addl_headers)
const
{
}

void
ircd::m::io::request::operator()(server &server,
                                 const vector_view<const http::header> &addl_headers)
const
{
	const size_t addl_headers_size
	{
		std::min(addl_headers.size(), size_t(64UL))
	};

	size_t headers{2 + addl_headers_size};
	http::line::header header[headers + 1]
	{
		{ "User-Agent",    BRANDING_NAME " (IRCd " BRANDING_VERSION ")" },
		{ "Content-Type",  "application/json"                           },
	};

	for(size_t i(0); i < addl_headers_size; ++i)
		header[headers++] = addl_headers.at(i);

	char x_matrix[1024];
	if(startswith(path, "_matrix/federation"))
		header[headers++] =
		{
			"Authorization",  generate_authorization(x_matrix)
		};

	http::request
	{
		destination,
		method,
		path,
		query,
		content,
		write_closure(server),
		{ header, headers }
	};
}

ircd::string_view
ircd::m::io::request::generate_authorization(const mutable_buffer &out)
const
{
	const fmt::bsprintf<2048> uri
	{
		"/%s%s%s", lstrip(path, '/'), query? "?" : "", query
	};

	request::authorization authorization
	{
		json::members
		{
			{ "destination",  destination  },
			{ "method",       method       },
			{ "origin",       my_host()    },
			{ "uri",          uri          },
		}
	};

	if(string_view{content}.size() > 2)
		json::get<"content"_>(authorization) = content;

	return authorization.generate(out);
}

ircd::string_view
ircd::m::io::request::authorization::generate(const mutable_buffer &out)
{
	// Any buffers here can be comfortably large if they're not on a stack and
	// nothing in this procedure has a yield which risks decohering static
	// buffers; the assertion is tripped if so.
	ctx::critical_assertion ca;

	static fixed_buffer<mutable_buffer, 131072> request_object_buf;
	const auto request_object
	{
		json::stringify(request_object_buf, *this)
	};

	const ed25519::sig sig
	{
		self::secret_key.sign(request_object)
	};

	static fixed_buffer<mutable_buffer, 128> signature_buf;
	const auto x_matrix_len
	{
		fmt::sprintf(out, "X-Matrix origin=%s,key=\"%s\",sig=\"%s\"",
		             unquote(string_view{at<"origin"_>(*this)}),
		             self::public_key_id,
		             b64encode_unpadded(signature_buf, sig))
	};

	return
	{
		data(out), size_t(x_matrix_len)
	};
}

bool
ircd::m::io::verify_x_matrix_authorization(const string_view &x_matrix,
                                           const string_view &method,
                                           const string_view &uri,
                                           const string_view &content)
{
	string_view tokens[3], origin, key, sig;
	if(ircd::tokens(split(x_matrix, ' ').second, ',', tokens) != 3)
		return false;

	for(const auto &token : tokens)
	{
		const auto &key_value
		{
			split(token, '=')
		};

		switch(hash(key_value.first))
		{
			case hash("origin"):  origin = unquote(key_value.second);  break;
			case hash("key"):     key = unquote(key_value.second);     break;
			case hash("sig"):     sig = unquote(key_value.second);     break;
		}
	}

	request::authorization authorization
	{
		json::members
		{
			{ "destination",  my_host() },
			{ "method",       method    },
			{ "origin",       origin    },
			{ "uri",          uri       },
		}
	};

	if(content.size() > 2)
		json::get<"content"_>(authorization) = content;

	//TODO: XXX
	const json::strung request_object
	{
		authorization
	};

	const ed25519::sig _sig
	{
		[&sig](auto &buf)
		{
			b64decode(buf, sig);
		}
	};

	const ed25519::pk pk
	{
		[&origin, &key](auto &buf)
		{
			m::keys::get(origin, key, [&key, &buf](const auto &keys)
			{
				const json::object vks
				{
					at<"verify_keys"_>(keys)
				};

				const json::object vkk
				{
					vks.at(key)
				};

				b64decode(buf, unquote(vkk.at("key")));
			});
		}
	};

	return pk.verify(const_raw_buffer{request_object}, _sig);
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

ircd::m::room
ircd::m::key::keys
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
	const string_view &cert_file
	{
		unquote(options.at("ssl_certificate_file"))
	};

	const auto cert_pem
	{
		fs::read(std::string(cert_file))
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
	const auto &sk_file
	{
		unquote(options.at("secret_key_file"))
	};

	self::secret_key = ed25519::sk
	{
		std::string{sk_file}, &self::public_key
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

	self::public_key_id = fmt::snstringf(BUFSIZE, "ed25519:%s", public_key_hash_b64);

	log.info("Current key is '%s' and the public key is: %s",
	         self::public_key_id,
	         self::public_key_b64);
}

static void
ircd::m::bootstrap_keys()
{
	create(key::keys, me.user_id);

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

	key my_key;
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

	const auto tls_fingerprints{json::strung(json::value
	{
		tlsfp, 1
	})};

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
	const auto signatures{json::strung(json::members
	{
		{ my_host(),
		{
			{ string_view{self::public_key_id}, b64encode_unpadded(signature, sig) }
		}}
	})};

	json::get<"signatures"_>(my_key) = signatures;
	keys::set(my_key);
}

bool
ircd::m::keys::get(const string_view &server_name,
                   const closure &closure)
{
	return get(server_name, string_view{}, closure);
}

bool
ircd::m::keys::get(const string_view &server_name,
                   const string_view &key_id,
                   const closure &closure)
{
	assert(!server_name.empty());

	const m::vm::query<m::vm::where::equal> query
	{
		{ "room_id",      key::keys.room_id   },
		{ "type",        "ircd.key"           },
		{ "state_key",    server_name         },
	};

	const auto have
	{
		[&closure](const auto &event)
		{
			closure(json::get<"content"_>(event));
			return true;
		}
	};

	if(m::vm::test(query, have))
		return true;

	if(server_name == my_host())
		throw m::NOT_FOUND
		{
			"key '%s' for '%s' not found", key_id?: "<unspecified>", server_name
		};

	log.debug("Key %s for %s not cached; querying network...",
	          key_id?: "<unspecified>",
	          server_name);

	char key_id_buf[1024], server_name_buf[1024];
	char url[1024]; const auto url_len
	{
/*
		fmt::snprintf(url, sizeof(url), "_matrix/key/v2/query/%s/%s",
		              server_name,
		              key_id):
*/
		fmt::snprintf(url, sizeof(url), "_matrix/key/v2/server/%s",
		              urlencode(key_id, key_id_buf))
	};

	//TODO: XXX
	const unique_buffer<mutable_buffer> buffer
	{
		8192
	};

	ircd::parse::buffer pb{mutable_buffer{buffer}};
	m::request request
	{
		"GET", url, {}, {}
	};

	m::session session
	{
		server_name
	};

	const string_view response
	{
		session(pb, request)
	};

/*
	const json::array &keys
	{
		response.at("server_keys")
	};

	log::debug("Fetched %zu candidate keys from '%s' (%s)",
	           keys.size(),
	           server_name,
	           string(remote(*session.client)));

	if(keys.empty())
		throw m::NOT_FOUND
		{
			"Failed to get key '%s' for '%s'", key_id, server_name
		};

	const m::key &key
	{
		keys[0]
	};
*/
	const m::key &key
	{
		response
	};

	if(!key.verify())
		throw m::error
		{
			http::UNAUTHORIZED, "M_INVALID_SIGNATURE", "Failed to verify key from '%s'", server_name
		};

	log.debug("Verified key from '%s'",
	          server_name);

	m::keys::set(key);
	closure(key);
	return true;
}

void
ircd::m::keys::set(const key &key)
{
	const auto &state_key
	{
		unquote(at<"server_name"_>(key))
	};

	const m::user::id::buf sender
	{
		"ircd", unquote(at<"server_name"_>(key))
	};

	const json::strung content
	{
		key
	};

	json::iov event;
	json::iov::push members[]
	{
		{ event, json::member { "type",       "ircd.key"   }},
		{ event, json::member { "state_key",  state_key    }},
		{ event, json::member { "sender",     sender       }},
		{ event, json::member { "content",    content      }}
	};

	key::keys.send(event);
}

/// Verify this key data (with itself).
bool
ircd::m::key::verify()
const noexcept try
{
	const auto &valid_until_ts
	{
		at<"valid_until_ts"_>(*this)
	};

	if(valid_until_ts < ircd::time<milliseconds>())
		throw ircd::error("Key was valid until %s", timestr(valid_until_ts));

	const json::object &verify_keys
	{
		at<"verify_keys"_>(*this)
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
		at<"signatures"_>(*this)
	};

	const string_view &server_name
	{
		unquote(at<"server_name"_>(*this))
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
	m::key copy{*this};
	at<"signatures"_>(copy) = string_view{};
	const std::string preimage{json::strung(copy)};
	return pk.verify(const_raw_buffer{preimage}, sig);
}
catch(const std::exception &e)
{
	log.error("key verification for '%s' failed: %s",
	          json::get<"server_name"_>(*this, "<no server name>"_sv),
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

void
ircd::m::room::create(json::iov &event,
                      json::iov &content)
{
	const json::iov::defaults defaults[]
	{
		{ event,    { "sender",   me.user_id  }},
		{ content,  { "creator",  me.user_id  }},
	};

	const json::strung _content
	{
		content
	};

	json::iov::set set[]
	{
		{ event,  { "depth",       1                }},
		{ event,  { "type",        "m.room.create"  }},
		{ event,  { "state_key",   ""               }},
		{ event,  { "content",     _content         }}
	};

	send(event);
}

void
ircd::m::join(const m::room::id &room_id,
              const m::id::user &user_id)
{
	membership(room_id, user_id, "join");
}

void
ircd::m::leave(const m::room::id &room_id,
               const m::id::user &user_id)
{
	membership(room_id, user_id, "leave");
}

void
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

	room.membership(event, content);
}

void
ircd::m::message(const m::id::room &room_id,
                 const m::id::user &user_id,
                 const string_view &body,
                 const string_view &msgtype)
{
	json::iov event;
	json::iov content;
	json::iov::push push[]
	{
		{ event,    { "sender",      user_id     }},
		{ content,  { "body",        body        }},
		{ content,  { "msgtype",     msgtype     }},
	};

	room room
	{
		room_id
	};

	room.message(event, content);
}

void
ircd::m::room::membership(json::iov &event,
                          json::iov &content)
{
	const user::id &user_id
	{
		event.at("sender")
	};

	const string_view &membership
	{
		content.at("membership")
	};

	if(this->membership(user_id, membership))
		throw m::ALREADY_MEMBER
		{
			"Member '%s' is already '%s'.", string_view{user_id}, membership
		};

	const json::strung c //TODO: child iov
	{
		content
	};

	const json::iov::set _event[]
	{
		{ event,  { "type",       "m.room.member"  }},
		{ event,  { "state_key",   user_id         }},
		{ event,  { "membership",  membership      }},
		{ event,  { "content",     string_view{c}  }},
	};

	send(event);
}

void
ircd::m::room::message(json::iov &event,
                       json::iov &content)
{
	const json::strung c //TODO: child iov
	{
		content
	};

	const json::iov::set _event[]
	{
		{ event,  { "type",       "m.room.message"  }},
		{ event,  { "content",     string_view{c}   }},
	};

	send(event);
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

/// academic search
std::vector<std::string>
ircd::m::room::barren(const int64_t &min_depth)
const
{
	return {};
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

ircd::m::event::id::buf
ircd::m::room::send(const json::members &event)
{
	size_t i(0);
	json::iov iov;
	json::iov::push members[event.size()];
	for(const auto &member : event)
		new (members + i++) json::iov::push(iov, member);

	return send(iov);
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
	json::iov event;
	json::iov content;
	json::iov::push push[]
	{
		{ event,    { "sender",      user_id     }},
		{ content,  { "membership",  "join"      }},
	};

	accounts.membership(event, content);
	control.membership(event, content);
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
	json::iov event;
	json::iov content;
	json::iov::push push[]
	{
		{ event,    { "sender",      user_id     }},
		{ content,  { "membership",  "leave"     }},
	};

	accounts.membership(event, content);
}

void
ircd::m::user::password(const string_view &password)
try
{
	json::iov event;
	json::iov::push members[]
	{
		{ event,  { "type",      "ircd.password"  }},
		{ event,  { "state_key",  user_id         }},
		{ event,  { "sender",     user_id         }},
		{ event,  { "content",    json::members
		{
			{ "plaintext", password }
		}}},
	};

	accounts.send(event);
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

	const vm::query<vm::where::test> correct_password{[&supplied_password]
	(const auto &event)
	{
		const json::object &content
		{
			json::at<"content"_>(event)
		};

		const auto &correct_password
		{
			unquote(content.at("plaintext"))
		};

		return supplied_password == correct_password;
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
	return accounts.membership(user_id);
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
