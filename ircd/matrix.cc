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
	static void init_keys(const std::string &secret_key_file);
	static void bootstrap();
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

bool
ircd::m::my_host(const string_view &s)
{
	return s == my_host();
}

ircd::string_view
ircd::m::my_host()
{
	return "cdc.z:8447"; //me.user_id.host();
}

ircd::m::init::init()
try
{
	init_keys("charybdis.sk");

	const string_view prefixes[]
	{
		"client_", "key_", "federation_", "media_"
	};

	for(const auto &name : mods::available())
		if(startswith_any(name, std::begin(prefixes), std::end(prefixes)))
			modules.emplace(name, name);

	if(db::sequence(*event::events) == 0)
		bootstrap();

	modules.emplace("root.so"s, "root.so"s);

	const auto options{json::strung(json::members
	{
		{ "name", "Chat Matrix" },
		{ "host", "0.0.0.0" },
		{ "port", 8447 },
		{ "ssl_certificate_file", "/home/jason/zemos.net.tls2.crt" },
		{ "ssl_certificate_chain_file", "/home/jason/zemos.net.tls2.crt" },
		{ "ssl_tmp_dh_file", "/home/jason/zemos.net.tls.dh" },
		{ "ssl_private_key_file_pem", "/home/jason/zemos.net.tls2.key" },
	})};

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
	create(user::accounts, me.user_id);
	create(user::sessions, me.user_id);
	create(filter::filters, me.user_id);
	join(user::accounts, me.user_id);
	bootstrap_keys();
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
		my::secret_key.sign(request_object)
	};

	static fixed_buffer<mutable_buffer, 128> signature_buf;
	const auto x_matrix_len
	{
		fmt::sprintf(out, "X-Matrix origin=%s,key=\"%s\",sig=\"%s\"",
		             unquote(string_view{at<"origin"_>(*this)}),
		             my::public_key_id,
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

	request::authorization authorization{json::members
	{
		{ "destination", my_host() },
		{ "method", method },
		{ "origin", origin },
		{ "uri", uri }
	}};

	if(content.size() > 2)
		json::get<"content"_>(authorization) = content;

	//TODO: XXX
	const auto request_object
	{
		json::strung(authorization)
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
			m::keys::get(origin, [&key, &buf](const auto &keys)
			{
				const json::object vks{at<"verify_keys"_>(keys)};
				const json::object vkk{vks.at(key)};
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
ircd::my::secret_key
{};

ircd::ed25519::pk
ircd::my::public_key
{};

std::string
ircd::my::public_key_b64
{};

std::string
ircd::my::public_key_id
{};

static void
ircd::m::init_keys(const std::string &sk_file)
{
	my::secret_key = ed25519::sk
	{
		sk_file, &my::public_key
	};

	my::public_key_b64 = b64encode_unpadded(my::public_key);
	const fixed_buffer<const_raw_buffer, sha256::digest_size> hash
	{
		sha256{const_raw_buffer{my::public_key}}
	};

	const auto public_key_hash_b64
	{
		b64encode_unpadded(hash)
	};

	my::public_key_id = fmt::snstringf(BUFSIZE, "ed25519:%s", public_key_hash_b64);

	log.info("Current key is '%s' and the public key is: %s",
	         my::public_key_id,
	         my::public_key_b64);
}

static void
ircd::m::bootstrap_keys()
{
	create(key::keys, me.user_id);

	const auto verify_keys{json::strung(json::members
	{{
		string_view{my::public_key_id},
		{
			{ "key", my::public_key_b64 }
		}
	}})};

	key my_key;
	json::get<"verify_keys"_>(my_key) = verify_keys;
	json::get<"server_name"_>(my_key) = my_host();
	json::get<"old_verify_keys"_>(my_key) = "{}";
	json::get<"valid_until_ts"_>(my_key) = ircd::time<milliseconds>() + duration_cast<milliseconds>(hours(2160)).count();

	const fixed_buffer<mutable_raw_buffer, 32> tls_hash
	{
		[](const auto &buffer)
		{
			a2u(buffer, "C259B83ABED34D81B31F773737574FBD966CE33BDED708BF502CA1D4CEC3D318");
		}
	};

	fixed_buffer<mutable_buffer, 256> tls_b64;
	const json::members tlsfps
	{
		{ "sha256", b64encode_unpadded(tls_b64, tls_hash) }
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
		my::secret_key.sign(const_raw_buffer{presig})
	};

	static char signature[256];
	const auto signatures{json::strung(json::members
	{
		{ my_host(),
		{
			{ string_view{my::public_key_id}, b64encode_unpadded(signature, sig) }
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
	const m::event::query<m::event::where::equal> query
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
			"key '%s' for '%s' not found", key_id, server_name
		};

	string_view server_name_encoded;
	const fixed_buffer<const_buffer, 128> server_name_buf{[&server_name, &server_name_encoded]
	(const auto &buf)
	{
		server_name_encoded = http::urlencode(server_name, buf);
	}};

	string_view key_id_encoded;
	const fixed_buffer<const_buffer, 128> key_id_buf{[&key_id, &key_id_encoded]
	(const auto &buf)
	{
		key_id_encoded = http::urlencode(key_id, buf);
	}};

	m::session session
	{
		server_name
	};

	char url[128]; const auto url_len
	{
/*
		fmt::snprintf(url, sizeof(url), "_matrix/key/v2/query/%s/%s",
		              server_name,
		              key_id):
*/
		fmt::snprintf(url, sizeof(url), "_matrix/key/v2/server/%s",
		              key_id)
	};

	char buf[4096];
	ircd::parse::buffer pb{buf};
	m::request request
	{
		"GET", url, {}, {}
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
	const m::event::query<m::event::where::equal> query
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
	const m::event::query<m::event::where::equal> query
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

bool
ircd::m::room::membership(const m::id::user &user_id,
                          const string_view &membership)
const
{
	const event::query<event::where::equal> member_event
	{
		{ "room_id",      room_id           },
		{ "type",        "m.room.member"    },
		{ "state_key",    user_id           },
	};

	if(!membership)
		return m::vm::test(member_event);

	const event::query<event::where::test> membership_test{[&membership]
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
	const event::query<event::where::equal> query
	{
		{ "room_id", room_id },
	};

	std::set<std::string, std::less<string_view>> ret;
	vm::for_each(query, [&ret, &min_depth](const auto &event)
	{
		if(json::get<"depth"_>(event) < min_depth)
			return;

		const json::array prev_events
		{
			json::get<"prev_events"_>(event)
		};

		for(const json::array &prev_event : prev_events)
		{
			const event::id &event_id
			{
				unquote(prev_event[0])
			};

			ret.erase(std::string{event_id});
		}

		ret.emplace(at<"event_id"_>(event));
	});

	return { std::begin(ret), std::end(ret) };
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
	const event::query<event::where::equal> query
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
		event, { "depth", int64_t(this->maxdepth()) }
	};

	return m::vm::commit(event);
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
	const event::query<event::where::equal> member_event
	{
		{ "room_id",      accounts.room_id  },
		{ "type",        "ircd.password"    },
		{ "state_key",    user_id           },
	};

	const event::query<event::where::test> correct_password{[&supplied_password]
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
// m/vm.h
//

bool
ircd::m::vm::test(const event::query<> &where)
{
	return test(where, [](const auto &event)
	{
		return true;
	});
}

bool
ircd::m::vm::test(const event::query<> &where,
                      const closure_bool &closure)
{
	bool ret{false};
	query(where, [&ret, &closure]
	(const auto &event)
	{
		ret = closure(event);
		return true;
	});

	return ret;
}

size_t
ircd::m::vm::count(const event::query<> &where)
{
	return count(where, [](const auto &event)
	{
		return true;
	});
}

size_t
ircd::m::vm::count(const event::query<> &where,
                       const closure_bool &closure)
{
	size_t i(0);
	for_each(where, [&closure, &i](const auto &event)
	{
		i += closure(event);
	});

	return i;
}

/*
void
ircd::m::vm::rfor_each(const closure &closure)
{
	const event::query<event::where::noop> where{};
	rfor_each(where, closure);
}

void
ircd::m::vm::rfor_each(const event::query<> &where,
                           const closure &closure)
{
	rquery(where, [&closure](const auto &event)
	{
		closure(event);
		return false;
	});
}

bool
ircd::m::vm::rquery(const closure_bool &closure)
{
	const event::query<event::where::noop> where{};
	return rquery(where, closure);
}

bool
ircd::m::vm::rquery(const event::query<> &where,
                        const closure_bool &closure)
{
	//return _rquery_(where, closure);
	return true;
}
*/

void
ircd::m::vm::for_each(const event::query<> &where,
                          const closure &closure)
{
	query(where, [&closure](const auto &event)
	{
		closure(event);
		return false;
	});
}

void
ircd::m::vm::for_each(const closure &closure)
{
	const event::query<event::where::noop> where{};
	for_each(where, closure);
}

bool
ircd::m::vm::query(const closure_bool &closure)
{
	const event::query<event::where::noop> where{};
	return query(where, closure);
}

namespace ircd::m::vm
{
	bool _query_event_id(const event::query<> &, const closure_bool &);
	bool _query_in_room_id(const event::query<> &, const closure_bool &, const room::id &);
	bool _query_for_type_state_key_in_room_id(const event::query<> &, const closure_bool &, const room::id &, const string_view &type = {}, const string_view &state_key = {});
}

bool
ircd::m::vm::query(const event::query<> &where,
                       const closure_bool &closure)
{
	switch(where.type)
	{
		case event::where::equal:
		{
			auto &clause
			{
				dynamic_cast<const event::query<event::where::equal> &>(where)
			};

			const auto &value{clause.value};
			const auto &room_id{json::get<"room_id"_>(value)};
			const auto &type{json::get<"type"_>(value)};
			const auto &state_key{json::get<"state_key"_>(value)};
			const bool is_state{json::get<"is_state"_>(value) == true};
			if(room_id && type && is_state)
				return _query_for_type_state_key_in_room_id(where, closure, room_id, type, state_key);

			if(room_id && is_state)
				return _query_for_type_state_key_in_room_id(where, closure, room_id, type, state_key);

			if(room_id)
				return _query_in_room_id(where, closure, room_id);

			break;
		}

		case event::where::logical_and:
		{
			auto &clause
			{
				dynamic_cast<const event::query<event::where::logical_and> &>(where)
			};

			const auto &lhs{*clause.a}, &rhs{*clause.b};
			const auto reclosure{[&lhs, &rhs, &closure]
			(const auto &event)
			{
				if(!rhs(event))
					return false;

				return closure(event);
			}};

			return vm::query(lhs, reclosure);
		}

		default:
			break;
	}

	return _query_event_id(where, closure);
}

bool
ircd::m::vm::_query_event_id(const event::query<> &where,
                                 const closure_bool &closure)
{
	event::cursor cursor
	{
		"event_id",
		&where
	};

	for(auto it(cursor.begin()); bool(it); ++it)
		if(closure(*it))
			return true;

	return false;
}

bool
ircd::m::vm::_query_in_room_id(const event::query<> &query,
                                   const closure_bool &closure,
                                   const room::id &room_id)
{
	event::cursor cursor
	{
		"event_id in room_id",
		&query
	};

	for(auto it(cursor.begin(room_id)); bool(it); ++it)
		if(closure(*it))
			return true;

	return false;
}

bool
ircd::m::vm::_query_for_type_state_key_in_room_id(const event::query<> &query,
                                                      const closure_bool &closure,
                                                      const room::id &room_id,
                                                      const string_view &type,
                                                      const string_view &state_key)
{
	event::cursor cursor
	{
		"event_id for type,state_key in room_id",
		&query
	};

	static const size_t max_type_size
	{
		255
	};

	static const size_t max_state_key_size
	{
		255
	};

	const auto key_max
	{
		room::id::buf::SIZE + max_type_size + max_state_key_size + 2
	};

	size_t key_len;
	char key[key_max]; key[0] = '\0';
	key_len = strlcat(key, room_id, sizeof(key));
	key_len = strlcat(key, "..", sizeof(key)); //TODO: prefix protocol
	key_len = strlcat(key, type, sizeof(key)); //TODO: prefix protocol
	key_len = strlcat(key, state_key, sizeof(key)); //TODO: prefix protocol
	for(auto it(cursor.begin(key)); bool(it); ++it)
		if(closure(*it))
			return true;

	return false;
}

namespace ircd::m
{
	void append_indexes(const event &, db::iov &);
}

namespace ircd::m::vm
{
	struct head;
}

struct ircd::m::vm::head
{
	struct leaf;

	int64_t depth {-1};
	std::set<leaf, std::less<std::pair<std::string_view, int64_t>>> heads;
};

struct ircd::m::vm::head::leaf
:std::pair<std::string, int64_t>
{
	using std::pair<std::string, int64_t>::pair;
};

namespace ircd::m::vm
{
	std::map<std::string, head, std::less<std::string_view>> heads;
}

namespace ircd::m::vm
{
	void eval_future(head &, const event &);
	void eval_present(head &, const event &);
	void eval_past(head &, const event &);
}

ircd::ctx::view<const ircd::m::event>
ircd::m::vm::inserted
{};

uint64_t
ircd::m::vm::current_sequence
{};

ircd::m::event::id::buf
ircd::m::vm::join(const room::id &room_id,
                  json::iov &iov)
{
	const user::id user_id
	{
		iov.at("sender")
	};

	const auto &hostname{room_id.hostname()};
	const auto &hostport{room_id.hostport()};
	m::log.debug("%s make_join %s to %s from %s:%u",
	             my_host(),
	             string_view{user_id},
	             string_view{room_id},
	             hostname,
	             hostport);

	char room_id_urle_buf[768];
	const auto room_id_urle
	{
		urlencode(room_id, room_id_urle_buf),
	};

	char user_id_urle_buf[768];
	const auto user_id_urle
	{
		urlencode(user_id, user_id_urle_buf)
	};

	const fmt::snstringf url
	{
		1024, "_matrix/federation/v1/make_join/%s/%s", room_id_urle, user_id_urle
	};

	m::request request
	{
		"GET", url, {}, {}
	};

	struct session session
	{
		{ std::string(hostname), hostport }
	};

	unique_buffer<mutable_buffer> buf
	{
		64_KiB
	};

	ircd::parse::buffer pb{buf};
	const json::object response
	{
		session(pb, request)
	};

	const m::event proto
	{
		response.at("event")
	};

	//TODO: hash prototype
	//at<"hashes"_>(proto)

	//TODO: verify prototype
	//at<"signatures"_>(proto)

	m::log.debug("%s make_join %s to %s responded. depth: %ld prev: %s auth: %s",
	             room_id.host(),
	             string_view{user_id},
	             string_view{room_id},
	             json::get<"depth"_>(proto),
	             json::get<"prev_events"_>(proto),
	             json::get<"auth_events"_>(proto));

	const json::strung content
	{
		json::members {{ "membership", "join" }}
	};

	json::iov event;
	const json::iov::push push[]
	{
		{ event, { "type",              "m.room.member"              }},
		{ event, { "membership",        "join"                       }},
		{ event, { "room_id",           room_id                      }},
		{ event, { "origin",            my_host()                    }},
		{ event, { "sender",            user_id                      }},
		{ event, { "state_key",         user_id                      }},
		{ event, { "is_state",          true                         }},
		{ event, { "origin_server_ts",  ircd::time<milliseconds>()   }},
		{ event, { "depth",             at<"depth"_>(proto)          }},
		{ event, { "content",           content                      }},
	//	{ event, { "auth_events",       at<"auth_events"_>(proto)    }},
	//	{ event, { "prev_events",       at<"prev_events"_>(proto)    }},
	//	{ event, { "prev_state",        at<"prev_state"_>(proto)     }},
	};

	//TODO: XXX
	auto replaced_auth_events{replace(at<"auth_events"_>(proto), '\\', "")};
	auto replaced_prev_events{replace(at<"prev_events"_>(proto), '\\', "")};
	auto replaced_prev_state{replace(at<"prev_state"_>(proto), '\\', "")};
	const json::iov::push replacements[]
	{
		{ event, { "auth_events", replaced_auth_events }},
		{ event, { "prev_events", replaced_prev_events }},
		{ event, { "prev_state",  replaced_prev_state  }},
	};

	const json::strung hash_preimage
	{
		event
	};

	const fixed_buffer<const_raw_buffer, sha256::digest_size> hash
	{
		sha256{const_buffer{hash_preimage}}
	};

	char hashb64[uint(hash.size() * 1.34) + 2];
	const json::strung hashes
	{
		json::members
		{
			{ "sha256", b64encode_unpadded(hashb64, hash) }
		}
	};

	const json::strung event_id_preimage
	{
		event
	};

	const fixed_buffer<const_raw_buffer, sha256::digest_size> event_id_hash
	{
		sha256{const_buffer{event_id_preimage}}
	};

	char event_id_hash_b64[uint(event_id_hash.size() * 1.34) + 2];
	const event::id::buf event_id_buf
	{
		b64encode_unpadded(event_id_hash_b64, event_id_hash), my_host()
	};

	const json::iov::push _event_id
	{
		event, { "event_id", event_id_buf }
	};

	const json::strung signature_preimage
	{
		event
	};

	const ed25519::sig sig
	{
		my::secret_key.sign(const_buffer{signature_preimage})
	};

	char signature_buffer[128];
	const json::strung signatures{json::members
	{{
		my_host(), json::members
		{
			json::member { my::public_key_id, b64encode_unpadded(signature_buffer, sig) }
		}
	}}};

	const json::iov::push _signatures
	{
		event, { "signatures", signatures }
	};

	char event_id_urle_buf[768];
	const auto event_id_urle
	{
		urlencode(event.at("event_id"), event_id_urle_buf)
	};

	const fmt::bsprintf<1024> send_join_url
	{
		"_matrix/federation/v1/send_join/%s/%s", room_id_urle, event_id_urle
	};

	const auto join_event
	{
		json::strung(event)
	};

	m::log.debug("%s send_join %s to %s sending: %s membership: %s %s",
	             my_host(),
	             string_view{user_id},
	             string_view{room_id},
	             string_view{event.at("type")},
	             string_view{event.at("membership")},
	             string_view{event.at("event_id")});

	m::request send_join_request
	{
		"PUT", send_join_url, {}, join_event
	};

	unique_buffer<mutable_buffer> send_join_buf
	{
		64_KiB
	};

	ircd::parse::buffer sjpb{send_join_buf};
	const json::array send_join_response
	{
		session(sjpb, send_join_request)
	};

	const auto status
	{
		send_join_response.at<uint>(0)
	};

	const json::object data
	{
		send_join_response.at(1)
	};

	const json::array state
	{
		data.at("state")
	};

	const json::array auth_chain
	{
		data.at("auth_chain")
	};

	m::log.debug("%s %u send_join %s to %s responded with %zu state and %zu auth_chain events",
	             room_id.host(),
	             status,
	             string_view{user_id},
	             string_view{room_id},
	             state.count(),
	             auth_chain.count());

	vm::eval(state);
	vm::eval(event);
	return event_id_buf;
}

namespace ircd::m::vm
{
	//void mtrace(const id::event &event_id, const tracer &closure);
}

void
ircd::m::vm::trace(const id::event &event_id,
                   const tracer &closure)
{
	id::event::buf id{event_id};
	unique_buffer<mutable_buffer> buf[8];

	while(1)
	{
		buf[0] = { 64_KiB };
		const event event
		{
			get(id, buf[0])
		};

		const json::array &prev_events
		{
			json::get<"prev_events"_>(event)
		};

		const auto count
		{
			prev_events.count()
		};

		if(!count)
			break;

		event::fetch tab[count];
		for(size_t i(0); i < count; ++i)
		{
			const json::array &prev_event
			{
				prev_events[i]
			};

			tab[i].event_id = unquote(prev_event[0]);
			buf[i + 1] = { 64_KiB };
			tab[i].buf = buf[i + 1];
		}

		size_t j(0);
		m::io::acquire({tab, count});
		for(size_t i(0); i < count; ++i)
		{
			if(tab[i].error)
				continue;
			else
				++j;

			const auto &event
			{
				tab[i].pdu
			};

			if(!closure(event, id))
				return;
		}

		if(!j)
			break;
	}
}

void
ircd::m::vm::state(const room::id &room_id,
                   const event::id &event_id)
try
{
	const unique_buffer<mutable_buffer> buf
	{
		16_MiB //TODO: XXX
	};

	room::state::fetch tab
	{
		event_id, room_id, buf
	};

	io::acquire(tab);

	const json::array &auth_chain
	{
		tab.auth_chain
	};

	const json::array &pdus
	{
		tab.pdus
	};

	std::vector<m::event> events(pdus.count());
	std::transform(begin(pdus), end(pdus), begin(events), []
	(const json::object &event) -> m::event
	{
		return event;
	});

	vm::eval(events);
}
catch(const std::exception &e)
{
	log.error("Acquiring state for %s at %s: %s",
	          string_view{room_id},
	          string_view{event_id},
	          e.what());
	throw;
}

void
ircd::m::vm::backfill(const room::id &room_id,
                      const event::id &event_id,
                      const size_t &limit)
try
{
	const unique_buffer<mutable_buffer> buf
	{
		16_MiB //TODO: XXX
	};

	room::fetch tab
	{
		event_id, room_id, buf
	};

	io::acquire(tab);

	const json::array &auth_chain
	{
		tab.auth_chain
	};

	const json::array &pdus
	{
		tab.pdus
	};

	std::vector<m::event> events(pdus.count());
	std::transform(begin(pdus), end(pdus), begin(events), []
	(const json::object &event) -> m::event
	{
		return event;
	});

	vm::eval(events);
}
catch(const std::exception &e)
{
	log.error("Acquiring backfill for %s at %s: %s",
	          string_view{room_id},
	          string_view{event_id},
	          e.what());
	throw;
}

ircd::json::object
ircd::m::vm::acquire(const id::event &event_id,
                     const mutable_buffer &buf)
{
	const auto event
	{
		get(event_id, buf)
	};

	vm::eval(m::event{event});
	return event;
}

size_t
ircd::m::vm::acquire(const vector_view<id::event> &event_id,
                     const vector_view<mutable_buffer> &buf)
{
	std::vector<event::fetch> tabs(event_id.size());
	for(size_t i(0); i < event_id.size(); ++i)
		tabs[i] = event::fetch { event_id[i], buf[i] };

	size_t i(0);
	io::acquire(vector_view<event::fetch>(tabs));
	for(const auto &fetch : tabs)
		if(fetch.pdu)
		{
			i++;
			vm::eval(m::event{fetch.pdu});
		}

	return i;
}

/// Insert a new event originating from this server.
///
/// Figure 1:
///          in    .
///    ___:::::::__V  <-- this function
///    |  ||||||| //
///    |   \\|// //|
///    |    ||| // |
///    |    ||//   |
///    |    !!!    |
///    |     *     |   <----- IRCd core
///    | |//|||\\| |
///    |/|/|/|\|\|\|    <---- release commitment propagation cone
///         out
///
/// This function takes an event object vector and adds our origin and event_id
/// and hashes and signature and attempts to inject the event into the core.
/// The caller is expected to have done their best to check if this event will
/// succeed once it hits the core because failures blow all this effort. The
/// caller's ircd::ctx will obviously yield for evaluation, which may involve
/// requests over the internet in the worst case. Nevertheless, the evaluation,
/// write and release sequence of the core commitment is designed to allow the
/// caller to service a usual HTTP request conveying success or error without
/// hanging their client too much.
///
ircd::m::event::id::buf
ircd::m::vm::commit(json::iov &iov)
{
	const json::iov::set set[]
	{
		{ iov, { "origin_server_ts",  ircd::time<milliseconds>() }},
		{ iov, { "origin",            my_host()                  }},
		{ iov, { "is_state",          iov.has("state_key")       }},
	};

	if(iov.has("room_id"))
	{
		const room::id &room_id
		{
			iov.at("room_id")
		};

		if(room_id.host() != my_host())
			return join(room_id, iov);
	}

	// Need this for now
	const unique_buffer<mutable_buffer> scratch
	{
		64_KiB
	};

	auto preimage
	{
		json::stringify(mutable_buffer{scratch}, iov)
	};

	const fixed_buffer<const_raw_buffer, sha256::digest_size> event_id_hash
	{
		sha256{const_buffer{preimage}}
	};

	char event_id_hash_b64[uint(event_id_hash.size() * 1.34) + 2];
	const event::id::buf event_id_buf
	{
		b64encode_unpadded(event_id_hash_b64, event_id_hash), my_host()
	};

	const json::iov::set event_id
	{
		iov, { "event_id", event_id_buf }
	};

	preimage =
	{
		json::stringify(mutable_buffer{scratch}, iov)
	};

	const fixed_buffer<const_raw_buffer, sha256::digest_size> hash
	{
		sha256{const_buffer{preimage}}
	};

	fixed_buffer<mutable_buffer, uint(hash.size() * 1.34) + 1> hashb64;
	const json::iov::set hashes
	{
		iov, json::member
		{
			"hashes", json::members
			{
				{ "sha256", b64encode_unpadded(hashb64, hash) }
			}
		}
	};

	preimage =
	{
		json::stringify(mutable_buffer{scratch}, iov)
	};

	const ed25519::sig sig
	{
		my::secret_key.sign(const_buffer{preimage})
	};

	char signature_buffer[128];
	const json::iov::set signatures
	{
		iov, json::member
		{
			"signatures", json::members
			{{
				my_host(), json::members
				{
					json::member { my::public_key_id, b64encode_unpadded(signature_buffer, sig) }
				}
			}}
		}
	};

	const m::event event
	{
		iov
	};

	if(!json::get<"type"_>(event))
		throw BAD_JSON("Required event field: type");

	if(!json::get<"sender"_>(event))
		throw BAD_JSON("Required event field: sender");

	log.debug("injecting event %s '%s' from %s :%s %s (mark: %ld)",
	          at<"event_id"_>(event),
	          at<"type"_>(event),
	          at<"sender"_>(event),
	          at<"origin"_>(event),
	          json::get<"is_state"_>(event)? "state" : "",
	          vm::current_sequence);

	ircd::timer timer;

	vm::eval(event);

	log.debug("committed event %s '%s' from %s :%s %s (mark: %ld time: %ld$ms)",
	          at<"event_id"_>(event),
	          at<"type"_>(event),
	          at<"sender"_>(event),
	          at<"origin"_>(event),
	          json::get<"is_state"_>(event)? "state" : "",
	          vm::current_sequence,
	          timer.at<milliseconds>().count());

	return event_id_buf;
}

namespace ircd::m::vm
{
	void fetch_head(const room::id &, head &, const event &);
	head &dome(const room::id &room_id, const event &);
	void eval_future(head &, const event &);
	void write(const event &, db::iov &txn);
}

/// Processes a bulk set of events similar to eval(event) except with
/// some transactionality and optimization over the set (i.e if two events
/// in the set cancel each other out their effects might be ignored rather
/// than invoking those operations with IRCd etc).
///
/// TEMP: Only calls single eval() in a loop for now.
void
ircd::m::vm::eval(const vector_view<event> &events)
{
	for(const auto &event : events)
		eval(m::event{event});
}

void
ircd::m::vm::eval(const json::array &events)
{
	for(const auto &event : events)
		eval(m::event{event});
}

/// Processes any event from any place from any time and does whatever is
/// necessary to validate, reject, learn from new information, ignore old
/// information and advance the state of IRCd as best as possible.
///
void
ircd::m::vm::eval(const event &event)
{
	std::cout << event << std::endl;

	const auto &event_id
	{
		at<"event_id"_>(event)
	};

	const std::string &room_id
	{
		at<"room_id"_>(event)
	};

	// Head fetch phase. If the head is not loaded into RAM then it
	// may have to be requested with IO and recursive evaluation which
	// could be a massive suspension for this stack. This usually only
	// occurs on the first eval for a room after IRCd startup or after
	// some other cache eviction.
	auto &head
	{
		vm::dome(room_id, event)
	};

	log.debug("evaluating event %s '%s' from %s :%s %s %s to %ld",
	          at<"event_id"_>(event),
	          at<"type"_>(event),
	          at<"sender"_>(event),
	          at<"origin"_>(event),
	          json::get<"is_state"_>(event)? "state" : "",
	          reflect(temporality(event, head.depth)),
	          head.depth);

	switch(temporality(event, head.depth))
	{
		case event.temporality::FUTURE:
			eval_future(head, event);
			break;

		case event.temporality::PRESENT:
			eval_present(head, event);
			break;

		case event.temporality::PAST:
			eval_past(head, event);
			break;
	}

	log.debug("committing event %s '%s' from %s :%s %s",
	          at<"event_id"_>(event),
	          at<"type"_>(event),
	          at<"sender"_>(event),
	          at<"origin"_>(event),
	          json::get<"is_state"_>(event)? "state" : "");

	db::iov txn
	{
		*event::events
	};

	write(event, txn);
	vm::current_sequence++;

	log.debug("release sequence %s '%s' from %s :%s %s",
	          at<"event_id"_>(event),
	          at<"type"_>(event),
	          at<"sender"_>(event),
	          at<"origin"_>(event),
	          json::get<"is_state"_>(event)? "state" : "");

	vm::inserted.notify(event);
}

void
ircd::m::vm::write(const event &event,
                   db::iov &txn)
{
	db::iov::append
	{
		txn, at<"event_id"_>(event), event
	};

	if(!txn.has(db::op::SET, "is_state"))
		db::iov::append
		{
			txn, db::delta
			{
				"is_state", at<"event_id"_>(event), byte_view<string_view>
				{
					defined(json::get<"state_key"_>(event))
				}
			}
		};

	append_indexes(event, txn);
	txn();
}

void
ircd::m::vm::eval_past(head &head,
                       const event &event)
{
}

void
ircd::m::vm::eval_present(head &head,
                          const event &event)
{
	const auto &depth
	{
		json::get<"depth"_>(event)
	};

	switch(depth)
	{
		case 0:
			//TODO: XXX
			//std::cout << "NO DEPTH?" << std::endl;
			break;

		case 1:
			std::cout << "CREATE?" << std::endl;
			return;

		default:
			break;
	}

	const json::array &prev_events
	{
		json::get<"prev_events"_>(event)
	};

	for(const json::array &prev_event : prev_events)
	{
		std::string prev_event_id
		{
			replace(unquote(prev_event[0]), '\\', "")
		};

		const unique_buffer<mutable_buffer> buf
		{
			64_KiB
		};

		try
		{
			acquire(prev_event_id, buf);
		}
		catch(const std::exception &e)
		{
			log.error("Failed to acquire %s: %s",
			          string_view{prev_event_id},
			          e.what());
		}
	}
}

void
ircd::m::vm::eval_future(head &head,
                         const event &event)
{
	const auto push_prev{[&head](const json::array &prev_events)
	{
		for(const json::array &prev_event : prev_events)
		{
			const std::string &prev_event_id
			{
				unquote(prev_event[0])
			};

			auto it
			{
				head.heads.lower_bound({prev_event_id, 0})
			};

			if(it == head.heads.end()) try
			{
				const unique_buffer<mutable_buffer> buf
				{
					64_KiB
				};

				acquire(prev_event_id, buf);
			}
			catch(const std::exception &e)
			{
				log.error("Failed to acquire %s: %s",
				          string_view{prev_event_id},
				          e.what());
			}

			it = head.heads.lower_bound({prev_event_id, 0});
			if(it != head.heads.end() && it->first == prev_event_id)
				head.heads.erase(it);
		}
	}};

	push_prev(json::get<"prev_events"_>(event));
	push_prev(json::get<"prev_state"_>(event));
	push_prev(json::get<"auth_events"_>(event));
	head.depth = json::get<"depth"_>(event);
	head.heads.emplace(head::leaf
	{
		at<"event_id"_>(event), json::get<"depth"_>(event)
	});
}

ircd::m::vm::head &
ircd::m::vm::dome(const room::id &room_id,
                  const event &event)
{
	head &ret
	{
		heads[std::string(room_id)]
	};

	if(ret.depth < 0)
		fetch_head(room_id, ret, event);

	return ret;
}

void
ircd::m::vm::fetch_head(const room::id &room_id,
                        head &head,
                        const event &event)
{
	const room room
	{
		room_id
	};

	const auto event_ids
	{
		room.barren()
	};

	if(event_ids.empty())
	{
		const event::id &event_id
		{
			at<"event_id"_>(event)
		};

		log.debug("No heads available for %s using %s",
		          string_view{room_id},
		          string_view{event_id});

		if(!my_host(room_id.host()))
			state(room_id, event_id);

		head.depth = json::get<"depth"_>(event);
		head.heads.emplace(head::leaf
		{
			at<"event_id"_>(event), json::get<"depth"_>(event),
		});

		if(!my_host(room_id.host()))
			backfill(room_id, event_id, 64);
		return;
	}

	std::vector<unique_buffer<mutable_buffer>> buf(event_ids.size());
	std::vector<event::fetch> tab(event_ids.size());
	for(size_t i(0); i < event_ids.size(); ++i)
	{
		buf[i] = unique_buffer<mutable_buffer>
		{
			64_KiB
		};

		tab[i] = event::fetch
		{
			event_ids[i], buf[i]
		};
	}

	io::acquire(vector_view<event::fetch>(tab));

	for(const auto &t : tab)
	{
		const m::event &event
		{
			t.pdu
		};

		const auto &event_id
		{
			at<"event_id"_>(event)
		};

		const auto &depth
		{
			at<"depth"_>(event)
		};

		if(head.depth < depth)
			head.depth = depth;

		head.heads.emplace(head::leaf{event_id, depth});
		log.debug("Found %s head for %s depth: %ld",
		          string_view{event_id},
		          string_view{room.room_id},
		          depth);
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// m/io.h
//

ircd::json::object
ircd::m::io::get(const id::event &event_id,
                 const mutable_buffer &buf)
{
	const m::event::query<m::event::where::equal> query
	{
		{ "event_id", event_id },
	};

	json::object ret;
	const auto test{[&buf, &ret](const auto &event)
	{
		ret = stringify(mutable_buffer{buf}, event);
		return true;
	}};

	// Attempt to find the event in the local database and return it
	if(m::vm::test(query, test))
		return ret;

	// Prevents any queries to the network going to own host.
	if(my_host(event_id.host()))
		throw m::NOT_FOUND
		{
			"Event '%s' not found", string_view{event_id}
		};

	event::fetch tab
	{
		event_id, buf
	};

	ret = io::acquire(tab);
	return ret;
}

ircd::json::array
ircd::m::io::acquire(room::state::fetch &tab)
{
	acquire({&tab, 1});
	if(unlikely(bool(tab.error)))
		std::rethrow_exception(tab.error);

	return tab.pdus;
}

size_t
ircd::m::io::acquire(vector_view<room::state::fetch> tab)
{
	const auto count
	{
		tab.size()
	};

	std::string url[count];
	std::string query[count];
	m::io::request request[count];
	struct session session[count];
	for(size_t i(0); i < count; ++i) try
	{
		static char tmp[768];
		url[i] = fmt::snstringf
		{
			1024, "_matrix/federation/v1/state/%s/", urlencode(tab[i].room_id, tmp)
		};

		query[i] = fmt::snstringf
		{
			1024, "event_id=%s", urlencode(tab[i].event_id, tmp)
		};

		request[i] =
		{
			"GET", url[i], query[i], {}
		};

		session[i] =
		{
			tab[i].hint? tab[i].hint : tab[i].event_id.hostname()
		};

		request[i].destination = session[i].destination;
		request[i](session[i].server);
	}
	catch(const std::exception &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.warning("request for event_id %s in room_id %s will not succeed: %s",
		            string_view{tab[i].event_id},
		            string_view{tab[i].room_id},
		            e.what());
	}

	size_t ret(0);
	json::object response[count];
	for(size_t i(0); i < count; ++i) try
	{
		if(bool(tab[i].error))
			continue;

		ircd::parse::buffer pb{tab[i].buf};
		response[i] = m::io::response(session[i].server, pb);
		tab[i].auth_chain = response[i]["auth_chain"];
		tab[i].pdus = response[i]["pdus"];
		//TODO: check event id
		//TODO: check hashes
		//TODO: check signatures
		ret += !tab[i].error;

		log.debug("%s sent us event %s in room %s pdus: %zu (size: %zu) error: %d",
		          string(net::remote{session[i].server}),
		          string_view{tab[i].event_id},
		          string_view{tab[i].room_id},
		          json::array{response[i]["pdus"]}.count(),
		          string_view{response[i]}.size(),
		          bool(tab[i].error));
	}
	catch(const std::exception &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.warning("request for event_id %s failed: %s",
		            string_view{tab[i].event_id},
		            e.what());
	}

	return ret;
}

ircd::json::array
ircd::m::io::acquire(room::fetch &tab)
{
	acquire({&tab, 1});
	if(unlikely(bool(tab.error)))
		std::rethrow_exception(tab.error);

	return tab.pdus;
}

size_t
ircd::m::io::acquire(vector_view<room::fetch> tab)
{
	const auto count
	{
		tab.size()
	};

	std::string url[count];
	std::string query[count];
	m::io::request request[count];
	struct session session[count];
	for(size_t i(0); i < count; ++i) try
	{
		static char tmp[768];
		url[i] = fmt::snstringf
		{
			1024, "_matrix/federation/v1/backfill/%s/", urlencode(tab[i].room_id, tmp)
		};

		query[i] = fmt::snstringf
		{
			1024, "limit=%zu&v=%s", tab[i].limit, urlencode(tab[i].event_id, tmp)
		};

		session[i] =
		{
			tab[i].hint? tab[i].hint : tab[i].event_id.hostname()
		};

		request[i] =
		{
			"GET", url[i], query[i], {}
		};

		request[i].destination = session[i].destination;
		request[i](session[i].server);
	}
	catch(const std::exception &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.warning("request for event_id %s in room_id %s will not succeed: %s",
		            string_view{tab[i].event_id},
		            string_view{tab[i].room_id},
		            e.what());
	}

	size_t ret(0);
	json::object response[count];
	for(size_t i(0); i < count; ++i) try
	{
		if(bool(tab[i].error))
			continue;

		ircd::parse::buffer pb{tab[i].buf};
		response[i] = m::io::response(session[i].server, pb);
		tab[i].auth_chain = response[i]["auth_chain"];
		tab[i].pdus = response[i]["pdus"];
		//TODO: check event id
		//TODO: check hashes
		//TODO: check signatures
		ret += !tab[i].error;

		log.debug("%s sent us event %s in room %s pdus: %zu (size: %zu) error: %d",
		          string(net::remote{session[i].server}),
		          string_view{tab[i].event_id},
		          string_view{tab[i].room_id},
		          json::array{response[i]["pdus"]}.count(),
		          string_view{response[i]}.size(),
		          bool(tab[i].error));
	}
	catch(const std::exception &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.warning("request for event_id %s failed: %s",
		            string_view{tab[i].event_id},
		            e.what());
	}

	return ret;
}

ircd::json::object
ircd::m::io::acquire(event::fetch &tab)
{
	acquire({&tab, 1});
	if(unlikely(bool(tab.error)))
		std::rethrow_exception(tab.error);

	return tab.pdu;
}

size_t
ircd::m::io::acquire(vector_view<event::fetch> tab)
{
	const auto count
	{
		tab.size()
	};

	std::string url[count];
	m::io::request request[count];
	struct session session[count];
	for(size_t i(0); i < count; ++i) try
	{
		static char tmp[768];
		url[i] = fmt::snstringf
		{
			1024, "_matrix/federation/v1/event/%s/", urlencode(tab[i].event_id, tmp)
		};

		session[i] =
		{
			tab[i].hint? tab[i].hint : tab[i].event_id.hostname()
		};

		request[i] =
		{
			"GET", url[i], {}, {}
		};

		request[i].destination = session[i].destination;
		request[i](session[i].server);
	}
	catch(const std::exception &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.warning("request for event_id %s will not succeed: %s",
		            string_view{tab[i].event_id},
		            e.what());
	}

	size_t ret(0);
	json::object response[count];
	for(size_t i(0); i < count; ++i) try
	{
		if(bool(tab[i].error))
			continue;

		ircd::parse::buffer pb{tab[i].buf};
		response[i] = m::io::response{session[i].server, pb};
		tab[i].pdu = json::array{response[i]["pdus"]}[0];
		//TODO: check event id
		//TODO: check hashes
		//TODO: check signatures
		ret += !tab[i].error;

		log.debug("%s sent us event %s pdus: %zu (size: %zu) error: %d",
		          string(net::remote{session[i].server}),
		          string_view{tab[i].event_id},
		          json::array{response[i]["pdus"]}.count(),
		          string_view{response[i]}.size(),
		          bool(tab[i].error));
	}
	catch(const std::exception &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.warning("request for event_id %s failed: %s",
		            string_view{tab[i].event_id},
		            e.what());
	}

	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//
// m/event.h
//

// vtable for the db::query partially specialized to m::event as its tuple
template ircd::db::query<ircd::m::event, ircd::db::where::noop>::~query();

ircd::database *
ircd::m::event::events
{};

ircd::m::event::const_iterator
ircd::m::event::find(const event::id &id)
{
	cursor c{name::event_id};
	return c.begin(string_view{id});
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

namespace ircd::m
{
	struct indexer;

	extern std::set<std::shared_ptr<indexer>> indexers;
}

struct ircd::m::indexer
{
	struct concat;
	struct concat_v;
	struct concat_2v;

	std::string name;

	virtual void operator()(const event &, db::iov &iov) const = 0;

	indexer(std::string name)
	:name{std::move(name)}
	{}

	virtual ~indexer() noexcept;
};

ircd::m::indexer::~indexer()
noexcept
{
}

void
ircd::m::append_indexes(const event &event,
                        db::iov &iov)
{
	for(const auto &ptr : indexers)
	{
		const m::indexer &indexer{*ptr};
		indexer(event, iov);
	}
}


struct ircd::m::indexer::concat
:indexer
{
	std::string col_a;
	std::string col_b;

	void operator()(const event &, db::iov &) const override;

	concat(std::string col_a, std::string col_b)
	:indexer
	{
		fmt::snstringf(512, "%s in %s", col_a, col_b)
	}
	,col_a{col_a}
	,col_b{col_b}
	{}
};

void
ircd::m::indexer::concat::operator()(const event &event,
                                     db::iov &iov)
const
{
	if(!iov.has(db::op::SET, col_a) || !iov.has(db::op::SET, col_b))
		return;

	static const size_t buf_max
	{
		1024
	};

	char index[buf_max];
	index[0] = '\0';
	const auto function
	{
		[&index](auto &val)
		{
			strlcat(index, byte_view<string_view>{val}, buf_max);
		}
	};

	at(event, col_b, function);
	at(event, col_a, function);

	db::iov::append
	{
		iov, db::delta
		{
			name,        // col
			index,       // key
			{},          // val
		}
	};
}

struct ircd::m::indexer::concat_v
:indexer
{
	std::string col_a;
	std::string col_b;
	std::string col_c;

	void operator()(const event &, db::iov &) const override;

	concat_v(std::string col_a, std::string col_b, std::string col_c)
	:indexer
	{
		fmt::snstringf(512, "%s for %s in %s", col_a, col_b, col_c)
	}
	,col_a{col_a}
	,col_b{col_b}
	,col_c{col_c}
	{}
};

void
ircd::m::indexer::concat_v::operator()(const event &event,
                                       db::iov &iov)
const
{
	if(!iov.has(db::op::SET, col_c) || !iov.has(db::op::SET, col_b) || !iov.has(db::op::SET, col_a))
		return;

	static const size_t buf_max
	{
		1024
	};

	char index[buf_max];
	index[0] = '\0';
	const auto concat
	{
		[&index](auto &val)
		{
			strlcat(index, byte_view<string_view>{val}, buf_max);
		}
	};

	at(event, col_c, concat);
	at(event, col_b, concat);

	string_view val;
	at(event, col_a, [&val](auto &_val)
	{
		val = byte_view<string_view>{_val};
	});

	db::iov::append
	{
		iov, db::delta
		{
			name,        // col
			index,       // key
			val,         // val
		}
	};
}

struct ircd::m::indexer::concat_2v
:indexer
{
	std::string col_a;
	std::string col_b0;
	std::string col_b1;
	std::string col_c;

	void operator()(const event &, db::iov &) const override;

	concat_2v(std::string col_a, std::string col_b0, std::string col_b1, std::string col_c)
	:indexer
	{
		fmt::snstringf(512, "%s for %s,%s in %s", col_a, col_b0, col_b1, col_c)
	}
	,col_a{col_a}
	,col_b0{col_b0}
	,col_b1{col_b1}
	,col_c{col_c}
	{}
};

void
ircd::m::indexer::concat_2v::operator()(const event &event,
                                        db::iov &iov)
const
{
	if(!iov.has(db::op::SET, col_c) || !iov.has(db::op::SET, col_b0) || !iov.has(db::op::SET, col_b1))
		return;

	static const size_t buf_max
	{
		2048
	};

	char index[buf_max];
	index[0] = '\0';
	const auto concat
	{
		[&index](auto &val)
		{
			strlcat(index, byte_view<string_view>{val}, buf_max);
		}
	};

	at(event, col_c, concat);
	strlcat(index, "..", buf_max);  //TODO: special
	at(event, col_b0, concat);
	at(event, col_b1, concat);

	string_view val;
	at(event, col_a, [&val](auto &_val)
	{
		val = byte_view<string_view>{_val};
	});

	db::iov::append
	{
		iov, db::delta
		{
			name,        // col
			index,       // key
			val,         // val
		}
	};
}

std::set<std::shared_ptr<ircd::m::indexer>> ircd::m::indexers
{{
	std::make_shared<ircd::m::indexer::concat>("event_id", "sender"),
	std::make_shared<ircd::m::indexer::concat>("event_id", "room_id"),
	std::make_shared<ircd::m::indexer::concat_v>("event_id", "room_id", "type"),
	std::make_shared<ircd::m::indexer::concat_v>("event_id", "room_id", "sender"),
	std::make_shared<ircd::m::indexer::concat_2v>("event_id", "type", "state_key", "room_id"),
}};

///////////////////////////////////////////////////////////////////////////////
//
// m/id.h
//

struct ircd::m::id::generator
{
	static string_view random_timebased(const enum sigil &, const mutable_buffer &);
	static string_view random_prefixed(const enum sigil &, const string_view &prefix, const mutable_buffer &);
};

ircd::m::id::id(const string_view &id)
:string_view{id}
,sigil{m::sigil(id)}
{
}

ircd::m::id::id(const enum sigil &sigil)
:string_view{}
,sigil{sigil}
{
}

ircd::m::id::id(const enum sigil &sigil,
                const string_view &id)
:string_view{id}
,sigil{sigil}
{
	if(!valid())
		throw INVALID_MXID("Not a valid '%s' mxid", reflect(sigil));
}

ircd::m::id::id(const enum sigil &sigil,
                const mutable_buffer &buf,
                const string_view &id)
:string_view
{
	buffer::data(buf), strlcpy(buffer::data(buf), id, buffer::size(buf))
}
,sigil{sigil}
{
	if(!valid())
		throw INVALID_MXID("Not a valid '%s' mxid", reflect(sigil));
}

ircd::m::id::id(const enum sigil &sigil,
                const mutable_buffer &buf,
                const string_view &name,
                const string_view &host)
:string_view{[&]() -> string_view
{
	using buffer::data;
	using buffer::size;

	const size_t &max{size(buf)};
	if(!max)
		return {};

	size_t len(0);
	if(!startswith(name, sigil))
		buf[len++] = char(sigil);

	const auto has_sep
	{
		std::count(std::begin(name), std::end(name), ':')
	};

	if(!has_sep && host.empty())
	{
		len += strlcpy(data(buf) + len, name, max - len);
	}
	else if(!has_sep && !host.empty())
	{
		len += fmt::snprintf(data(buf) + len, max - len, "%s:%s",
		                     name,
		                     host);
	}
	else if(has_sep == 1 && !host.empty() && !split(name, ':').second.empty())
	{
		len += strlcpy(data(buf) + len, name, max - len);
	}
	else if(has_sep >= 1 && !host.empty())
	{
		if(split(name, ':').second != host)
			throw INVALID_MXID("MXID must be on host '%s'", host);

		len += strlcpy(data(buf) + len, name, max - len);
	}
	//else throw INVALID_MXID("Not a valid '%s' mxid", reflect(sigil));

	return { data(buf), len };
}()}
,sigil{sigil}
{
}

ircd::m::id::id(const enum sigil &sigil,
                const mutable_buffer &buf,
                const generate_t &,
                const string_view &host)
:string_view{[&]
{
	char name[64]; switch(sigil)
	{
		case sigil::USER:
			generator::random_prefixed(sigil::USER, "guest", name);
			break;

		case sigil::ALIAS:
			generator::random_prefixed(sigil::ALIAS, "", name);
			break;

		default:
			generator::random_timebased(sigil, name);
			break;
	};

	const auto len
	{
		fmt::sprintf(buf, "%s:%s", name, host)
	};

	return string_view { buffer::data(buf), size_t(len) };
}()}
,sigil{sigil}
{
}

void
ircd::m::id::validate()
const
{
	if(!valid())
		throw INVALID_MXID("Not a valid '%s' mxid", reflect(sigil));
}

bool
ircd::m::id::valid()
const
{
	const auto parts(split(*this, ':'));
	const auto &local(parts.first);
	const auto &host(parts.second);

	// this valid() requires a full canonical mxid with a host
	if(host.empty())
		return false;

	// local requires a sigil plus at least one character
	if(local.size() < 2)
		return false;

	// local requires the correct sigil type
	if(!startswith(local, sigil))
		return false;

	return true;
}

bool
ircd::m::id::valid_local()
const
{
	const auto parts(split(*this, ':'));
	const auto &local(parts.first);

	// local requires a sigil plus at least one character
	if(local.size() < 2)
		return false;

	// local requires the correct sigil type
	if(!startswith(local, sigil))
		return false;

	return true;
}

ircd::string_view
ircd::m::id::generator::random_prefixed(const enum sigil &sigil,
                                        const string_view &prefix,
                                        const mutable_buffer &buf)
{
	using buffer::data;

	const auto len
	{
		fmt::sprintf(buf, "%c%s%u", char(sigil), prefix, rand::integer())
	};

	return { data(buf), size_t(len) };
}

ircd::string_view
ircd::m::id::generator::random_timebased(const enum sigil &sigil,
                                         const mutable_buffer &buf)
{
	using buffer::data;
	using buffer::size;

	const auto utime(microtime());
	const auto len
	{
		snprintf(data(buf), size(buf), "%c%zd%06d", char(sigil), utime.first, utime.second)
	};

	return { data(buf), size_t(len) };
}

enum ircd::m::id::sigil
ircd::m::sigil(const string_view &s)
try
{
	return sigil(s.at(0));
}
catch(const std::out_of_range &e)
{
	throw BAD_SIGIL("sigil undefined");
}

enum ircd::m::id::sigil
ircd::m::sigil(const char &c)
{
	switch(c)
	{
		case '$':  return id::EVENT;
		case '@':  return id::USER;
		case '#':  return id::ALIAS;
		case '!':  return id::ROOM;
	}

	throw BAD_SIGIL("'%c' is not a valid sigil", c);
}

const char *
ircd::m::reflect(const enum id::sigil &c)
{
	switch(c)
	{
		case id::EVENT:   return "EVENT";
		case id::USER:    return "USER";
		case id::ALIAS:   return "ALIAS";
		case id::ROOM:    return "ROOM";
	}

	return "?????";
}

bool
ircd::m::valid_sigil(const string_view &s)
try
{
	return valid_sigil(s.at(0));
}
catch(const std::out_of_range &e)
{
	return false;
}

bool
ircd::m::valid_sigil(const char &c)
{
	switch(c)
	{
		case id::EVENT:
		case id::USER:
		case id::ALIAS:
		case id::ROOM:
			return true;
	}

	return false;
}
