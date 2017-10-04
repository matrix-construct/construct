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
// m/session.h
//

ircd::m::session::session(const hostport &host_port)
:client{std::make_shared<ircd::client>(host_port)}
{
}

ircd::json::object
ircd::m::session::operator()(parse::buffer &pb,
                             request &r)
{
	const json::member origin
	{
		"origin", my_host()
	};

	const json::member destination
	{
		//host(remote_hostport(*client->sock))
		"destination", "zemos.net"
	};

	const json::member method
	{
		"method", r.method
	};

	const std::string uri
	{
		std::string {"/"} + std::string{r.path} +
		(r.query? (std::string{"?"} + std::string{r.query}) : std::string{})
	};

	json::iov iov;
	const json::iov::push pushed[]
	{
		{ iov,  json::member { "uri", uri }  },
		{ iov,  origin                       },
		{ iov,  method                       },
		{ iov,  destination                  },
	};

	const json::iov::add_if content
	{
		iov, r.content.size() > 2, json::member
		{
			"content", r.content
		}
	};

	size_t headers{2};
	http::line::header header[3]
	{
		{ "Content-Type",  "application/json"  },
		{ "User-Agent",    "IRCd"              }
	};

	char x_matrix_buf[2048];
	if(startswith(r.path, "_matrix/federation"))
	{
		// These buffers can be comfortably large if they're not on a stack and
		// nothing in this procedure has a yield; the assertion is tripped if so
		static char request_object_buffer[4096];
		static char signature_buffer[128];
		ctx::critical_assertion ca;

		const auto request_object
		{
			json::stringify(request_object_buffer, iov)
		};

		std::cout << request_object << std::endl;

		const ed25519::sig sig
		{
			my::secret_key.sign(request_object)
		};

		const auto signature
		{
			b64encode_unpadded(signature_buffer, sig)
		};

		const auto x_matrix_len
		{
			fmt::sprintf(x_matrix_buf, "X-Matrix origin=%s,key=\"ed25519:pk2\",sig=\"%s\"",
			             string_view{origin.second},
			             signature)
		};

		const string_view x_matrix
		{
			x_matrix_buf, size_t(x_matrix_len)
		};

		header[headers++] = { "Authorization",  x_matrix };
	}

	http::request
	{
		string_view{destination.second}, //host(remote(*client)),
		r.method,
		r.path,
		r.query,
		r.content,
		write_closure(*client),
		{ header, headers }
	};

	http::code status;
	json::object object;
	parse::capstan pc
	{
		pb, read_closure(*client)
	};

	http::response
	{
		pc,
		nullptr,
		[&pc, &status, &object](const http::response::head &head)
		{
			status = http::status(head.status);
			object = http::response::content{pc, head};
		}
	};

	if(status < 200 || status >= 300)
		throw m::error(status, object);

	return object;
}

///////////////////////////////////////////////////////////////////////////////
//
// m.h
//

namespace ircd::m
{
	std::map<std::string, ircd::module> modules;
	ircd::net::listener *listener;

	static void leave_ircd_room();
	static void join_ircd_room();
	static void init_keys(const std::string &secret_key_file);
	static void bootstrap();
}

const ircd::m::user::id::buf
ircd_user_id
{
	"ircd", "cdc.z"  //TODO: hostname
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
		"client_", "key_",
	};

	for(const auto &name : mods::available())
		if(startswith_any(name, std::begin(prefixes), std::end(prefixes)))
			modules.emplace(name, name);

	if(db::sequence(*event::events) == 0)
		bootstrap();

	modules.emplace("root.so"s, "root.so"s);

	const auto options{json::string(json::members
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
	log::critical("%s %s", e.what(), e.content);
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
	log::critical("%s %s", e.what(), e.content);
	std::terminate();
}

void
ircd::m::join_ircd_room()
try
{
	// ircd.start event
	json::iov content;
	my_room.join(me.user_id, content);
}
catch(const m::ALREADY_MEMBER &e)
{
	log::warning("IRCd did not shut down correctly...");
}

void
ircd::m::leave_ircd_room()
{
	// ircd.start event
	json::iov content;
	my_room.leave(me.user_id, content);
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

	json::iov content;
	my_room.create(me.user_id, me.user_id, content);
	user::accounts.create(me.user_id, me.user_id, content);
	user::accounts.join(me.user_id, content);
	user::sessions.create(me.user_id, me.user_id, content);
	filter::filters.create(me.user_id, me.user_id, content);
	bootstrap_keys();
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

static void
ircd::m::init_keys(const std::string &sk_file)
{
	my::secret_key = ed25519::sk
	{
		sk_file, &my::public_key
	};

	my::public_key_b64 = b64encode_unpadded(my::public_key);

	log::info("My ed25519 public key is: %s",
	          my::public_key_b64);
}

namespace ircd
{
	size_t certbytes(const mutable_raw_buffer &buf, const std::string &certfile);
}

static void
ircd::m::bootstrap_keys()
{
	json::iov content;
	key::keys.create(me.user_id, me.user_id, content);

	key my_key;
	json::val<name::server_name>(my_key) = my_host();
	json::val<name::old_verify_keys>(my_key) = "{}";

	const auto valid_until
	{
		ircd::time<milliseconds>() + duration_cast<milliseconds>(hours(2160)).count()
	};
	json::val<name::valid_until_ts>(my_key) = valid_until;

	static char verify_keys_buf[256];
	json::val<name::verify_keys>(my_key) = json::stringify(verify_keys_buf, json::members
	{
		{ "ed25519:pk2", json::members
		{
			{ "key", my::public_key_b64 }
		}}
	});

	//static unsigned char pembuf[4096];
	//const auto cbsz{ircd::certbytes(pembuf, "/home/jason/zemos.net.tls.crt")};

	static std::array<uint8_t, 32> tls_hash;
	a2u(tls_hash, "C259B83ABED34D81B31F773737574FBD966CE33BDED708BF502CA1D4CEC3D318");

	static char tls_b64_buf[256];
	const json::members tlsfps
	{
		{ "sha256", b64encode_unpadded(tls_b64_buf, tls_hash) }
	};

	const json::value tlsfp[1]
	{
		{ tlsfps }
	};

	static char tls_fingerprints_buf[256];
	json::get<"tls_fingerprints"_>(my_key) = json::stringify(tls_fingerprints_buf, json::value
	{
		tlsfp, 1
	});

	const std::string presig
	{
		json::string(my_key)
	};

	const ed25519::sig sig
	{
		my::secret_key.sign(const_raw_buffer{presig})
	};

	static char signature[128], signatures[256];
	json::get<"signatures"_>(my_key) = json::stringify(signatures, json::members
	{
		{ my_host(), json::members
		{
			{ "ed25519:pk2", b64encode_unpadded(signature, sig) }
		}}
	});

	keys::set(my_key);
}

void
ircd::m::keys::set(const key &key)
{
	const auto &state_key
	{
		at<"server_name"_>(key)
	};

	const m::user::id::buf sender
	{
		"ircd", at<"server_name"_>(key)
	};

	const auto content
	{
		json::string(key)
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

bool
ircd::m::keys::get(const string_view &server_name,
                   const closure &closure)
{
	const m::event::query<m::event::where::equal> query
	{
		{ "room_id",      key::keys.room_id   },
		{ "type",        "ircd.key"           },
		{ "state_key",    server_name         },
	};

	return m::events::test(query, [&closure]
	(const auto &event)
	{
		closure(json::val<name::content>(event));
		return true;
	});
}

///////////////////////////////////////////////////////////////////////////////
//
// m/db.h
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
	m::events::test(query, [&buf, &len]
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
	m::events::test(query, [&ret]
	(const auto &event)
	{
		ret = json::get<"content"_>(event).size();
		return true;
	});

	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//
// m/room.h
//

//
// m::room
//

void
ircd::m::room::create(const m::id::user &sender,
                      const m::id::user &creator,
                      json::iov &content)
{
	const json::iov::add _creator
	{
		content, { "creator", me.user_id }
	};

	const auto _content
	{
		json::string(content)
	};

	send(
	{
		{ "type",        "m.room.create" },
		{ "sender",      sender          },
		{ "state_key",   ""              },
		{ "content",     _content        }
	});
}

void
ircd::m::room::join(const m::id::user &user_id,
                    json::iov &content)
{
	const json::iov::set membership_join
	{
		content, { "membership", "join" }
	};

	membership(user_id, content);
}

void
ircd::m::room::leave(const m::id::user &user_id,
                     json::iov &content)
{
	const json::iov::set membership_leave
	{
		content, { "membership", "leave" }
	};

	membership(user_id, content);
}

void
ircd::m::room::membership(const m::id::user &user_id,
                          json::iov &content)
{
	const string_view &membership
	{
		content.at("membership")
	};

	if(this->membership(user_id, membership))
		throw m::ALREADY_MEMBER
		{
			"Member '%s' is already '%s'.", string_view{user_id}, membership
		};

	//TODO: child iov
	const std::string c
	{
		json::string(content)
	};

	json::iov event;
	json::iov::push members[]
	{
		{ event, json::member { "type",      "m.room.member"  }},
		{ event, json::member { "state_key",  user_id         }},
		{ event, json::member { "sender",     user_id         }},
		{ event, json::member { "content",    string_view{c}  }}
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
		return m::events::test(member_event);

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

	return m::events::test(member_event && membership_test);
}

ircd::m::event::id
ircd::m::room::head(event::id::buf &buf)
const
{
	const event::query<event::where::equal> query
	{
		{ "room_id", room_id },
	};

	events::test(query, [&buf]
	(const auto &event)
	{
		buf = json::get<"event_id"_>(event);
		return true;
	});

	return buf;
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
	const json::iov::add room_id
	{
		event, { "room_id", this->room_id }
	};

	const id::event::buf generated_event_id
	{
		id::generate, this->room_id.host()
	};

	const json::iov::add event_id
	{
		event, { "event_id", generated_event_id }
	};

	m::event::insert(event);
	return generated_event_id;
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
	json::iov::push members[contents.size()];

	size_t i(0);
	for(const auto &member : contents)
		new (members + i++) json::iov::push(content, member);

	accounts.join(user_id, content);
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
	json::iov::push members[contents.size()];

	size_t i(0);
	for(const auto &member : contents)
		new (members + i++) json::iov::push(content, member);

	accounts.leave(user_id, content);
}

void
ircd::m::user::password(const string_view &password)
try
{
	json::iov event;
	json::iov::push members[]
	{
		{ event, json::member { "type",      "ircd.password"  }},
		{ event, json::member { "state_key",  user_id         }},
		{ event, json::member { "sender",     user_id         }},
		{ event, json::member { "content",    json::members
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

	return m::events::test(member_event && correct_password);
}

bool
ircd::m::user::is_active()
const
{
	return accounts.membership(user_id);
}

///////////////////////////////////////////////////////////////////////////////
//
// query lib
//
/*
bool
ircd::m::room::members::_rquery_(const event::query<> &query,
                                 const event_closure_bool &closure)
const
{
	event::cursor cursor
	{
		"event_id for type,state_key in room_id",
		&query
	};

	//TODO: ???
	static const size_t max_type_size
	{
		256
	};

	const auto key_max
	{
		room::id::buf::SIZE + max_type_size
	};

	size_t key_len;
	char key[key_max]; key[0] = '\0';
	key_len = strlcat(key, room_id, sizeof(key));
	key_len = strlcat(key, "..m.room.member", sizeof(key)); //TODO: prefix protocol
	for(auto it(cursor.rbegin(key)); bool(it); ++it)
		if(closure(*it))
			return true;

	return false;
}

bool
ircd::m::room::members::_query_(const event::query<> &query,
                                const event_closure_bool &closure)
const
{
	event::cursor cursor
	{
		"event_id for type,state_key in room_id",
		&query
	};

	//TODO: ???
	static const size_t max_type_size
	{
		256
	};

	const auto key_max
	{
		room::id::buf::SIZE + max_type_size
	};

	size_t key_len;
	char key[key_max]; key[0] = '\0';
	key_len = strlcat(key, room_id, sizeof(key));
	key_len = strlcat(key, "..m.room.member", sizeof(key)); //TODO: prefix protocol
	for(auto it(cursor.begin(key)); bool(it); ++it)
		if(closure(*it))
			return true;

	return false;
}

bool
ircd::m::room::state::_rquery_(const event::query<> &query,
                               const event_closure_bool &closure)
const
{
	event::cursor cursor
	{
		"event_id for type,state_key in room_id",
		&query
	};

	for(auto it(cursor.rbegin(room_id)); bool(it); ++it)
		if(closure(*it))
			return true;

	return false;
}

bool
ircd::m::room::state::_query_(const event::query<> &query,
                              const event_closure_bool &closure)
const
{
	event::cursor cursor
	{
		"event_id for type,state_key in room_id",
		&query
	};

	for(auto it(cursor.begin(room_id)); bool(it); ++it)
		if(closure(*it))
			return true;

	return false;
}

bool
ircd::m::room::events::_rquery_(const event::query<> &query,
                                const event_closure_bool &closure)
const
{
	event::cursor cursor
	{
		"event_id in room_id",
		&query
	};

	for(auto it(cursor.rbegin(room_id)); bool(it); ++it)
		if(closure(*it))
			return true;

	return false;
}

bool
ircd::m::room::events::_query_(const event::query<> &query,
                               const event_closure_bool &closure)
const
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
ircd::m::user::rooms::_rquery_(const event::query<> &query,
                               const event_closure_bool &closure)
const
{
	event::cursor cursor
	{
		"event_id for room_id in sender",
		&query
	};

	for(auto it(cursor.rbegin(user_id)); bool(it); ++it)
		if(closure(*it))
			return true;

	return false;
}

bool
ircd::m::user::rooms::_query_(const event::query<> &query,
                              const event_closure_bool &closure)
const
{
	event::cursor cursor
	{
		"event_id for room_id in sender",
		&query
	};

	for(auto it(cursor.begin(user_id)); bool(it); ++it)
		if(closure(*it))
			return true;

	return false;
}

bool
ircd::m::events::_rquery_(const event::query<> &where,
                          const event_closure_bool &closure)
const
{
	event::cursor cursor
	{
		"event_id",
		&where
	};

	for(auto it(cursor.rbegin()); bool(it); ++it)
		if(closure(*it))
			return true;

	return false;
}

bool
ircd::m::events::_query_(const event::query<> &where,
                         const event_closure_bool &closure)
const
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
*/

///////////////////////////////////////////////////////////////////////////////
//
// m/events.h
//

bool
ircd::m::events::test(const event::query<> &where)
{
	return test(where, [](const auto &event)
	{
		return true;
	});
}

bool
ircd::m::events::test(const event::query<> &where,
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
ircd::m::events::count(const event::query<> &where)
{
	return count(where, [](const auto &event)
	{
		return true;
	});
}

size_t
ircd::m::events::count(const event::query<> &where,
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
ircd::m::events::rfor_each(const closure &closure)
{
	const event::query<event::where::noop> where{};
	rfor_each(where, closure);
}

void
ircd::m::events::rfor_each(const event::query<> &where,
                           const closure &closure)
{
	rquery(where, [&closure](const auto &event)
	{
		closure(event);
		return false;
	});
}

bool
ircd::m::events::rquery(const closure_bool &closure)
{
	const event::query<event::where::noop> where{};
	return rquery(where, closure);
}

bool
ircd::m::events::rquery(const event::query<> &where,
                        const closure_bool &closure)
{
	//return _rquery_(where, closure);
	return true;
}
*/

void
ircd::m::events::for_each(const event::query<> &where,
                          const closure &closure)
{
	query(where, [&closure](const auto &event)
	{
		closure(event);
		return false;
	});
}

void
ircd::m::events::for_each(const closure &closure)
{
	const event::query<event::where::noop> where{};
	for_each(where, closure);
}

bool
ircd::m::events::query(const closure_bool &closure)
{
	const event::query<event::where::noop> where{};
	return query(where, closure);
}

namespace ircd::m::events
{
	bool _query_event_id(const event::query<> &, const closure_bool &);
	bool _query_in_room_id(const event::query<> &, const closure_bool &, const room::id &);
	bool _query_for_type_state_key_in_room_id(const event::query<> &, const closure_bool &, const room::id &, const string_view &type = {}, const string_view &state_key = {});
}

bool
ircd::m::events::query(const event::query<> &where,
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
			if(room_id && type && state_key.defined())
				return _query_for_type_state_key_in_room_id(where, closure, room_id, type, state_key);

			if(room_id && state_key.defined())
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

			return events::query(lhs, reclosure);
		}

		default:
			break;
	}

	return _query_event_id(where, closure);
}

bool
ircd::m::events::_query_event_id(const event::query<> &where,
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
ircd::m::events::_query_in_room_id(const event::query<> &query,
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
ircd::m::events::_query_for_type_state_key_in_room_id(const event::query<> &query,
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

///////////////////////////////////////////////////////////////////////////////
//
// m/event.h
//

namespace ircd::m
{
	void append_indexes(const event &, db::iov &);
}

// vtable for the db::query partially specialized to m::event as its tuple
template ircd::db::query<ircd::m::event, ircd::db::where::noop>::~query();

ircd::database *
ircd::m::event::events
{};

ircd::ctx::view<const ircd::m::event>
ircd::m::event::inserted
{};

ircd::m::event::id::buf
ircd::m::event::head
{};

void
ircd::m::event::insert(json::iov &iov)
{
	const id::event::buf generated_event_id
	{
		iov.has("event_id")? id::event::buf{} : id::event::buf{id::generate, my_host()}
	};

	const json::iov::add_if event_id
	{
		iov, !iov.has("event_id"), { "event_id", generated_event_id }
	};

	const json::iov::set origin_server_ts
	{
		iov, { "origin_server_ts", ircd::time<milliseconds>() }
	};

	const m::event event
	{
		iov
	};

	if(!json::at<"type"_>(event))
		throw BAD_JSON("Required event field: '%s'", name::type);

	if(!json::at<"sender"_>(event))
		throw BAD_JSON("Required event field: '%s'", name::sender);

	db::iov txn
	{
		*event::events
	};

	db::iov::append
	{
		txn, json::at<"event_id"_>(event), iov
	};

	append_indexes(event, txn);
	txn(*event::events);
	event::head = json::at<"event_id"_>(event);
	event::inserted.notify(event);
}

ircd::m::event::const_iterator
ircd::m::event::find(const event::id &id)
{
	cursor c{name::event_id};
	return c.begin(string_view{id});
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
		[&index](const auto &val)
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
	if(!iov.has(db::op::SET, col_c) || !iov.has(db::op::SET, col_b))
		return;

	static const size_t buf_max
	{
		1024
	};

	char index[buf_max];
	index[0] = '\0';
	const auto concat
	{
		[&index](const auto &val)
		{
			strlcat(index, byte_view<string_view>{val}, buf_max);
		}
	};

	at(event, col_c, concat);
	at(event, col_b, concat);

	string_view val;
	at(event, col_a, [&val](const auto &_val)
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
		[&index](const auto &val)
		{
			strlcat(index, byte_view<string_view>{val}, buf_max);
		}
	};

	at(event, col_c, concat);
	strlcat(index, "..", buf_max);  //TODO: special
	at(event, col_b0, concat);
	at(event, col_b1, concat);

	string_view val;
	at(event, col_a, [&val](const auto &_val)
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
                char *const &buf,
                const size_t &max,
                const string_view &id)
:string_view
{
	buf, strlcpy(buf, id, max)
}
,sigil{sigil}
{
	if(!valid())
		throw INVALID_MXID("Not a valid '%s' mxid", reflect(sigil));
}

ircd::m::id::id(const enum sigil &sigil,
                char *const &buf,
                const size_t &max,
                const string_view &name,
                const string_view &host)
:string_view{[&]() -> string_view
{
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
		len += strlcpy(buf + len, name, max - len);
	}
	else if(!has_sep && !host.empty())
	{
		len += fmt::snprintf(buf + len, max - len, "%s:%s",
		                     name,
		                     host);
	}
	else if(has_sep == 1 && !host.empty() && !split(name, ':').second.empty())
	{
		len += strlcpy(buf + len, name, max - len);
	}
	else if(has_sep == 1 && !host.empty())
	{
		if(split(name, ':').second != host)
			throw INVALID_MXID("MXID must be on host '%s'", host);

		len += strlcpy(buf + len, name, max - len);
	}
	else if(has_sep && !host.empty())
	{
		throw INVALID_MXID("Not a valid '%s' mxid", reflect(sigil));
	}

	return { buf, len };
}()}
,sigil{sigil}
{
}

ircd::m::id::id(const enum sigil &sigil,
                char *const &buf,
                const size_t &max,
                const generate_t &,
                const string_view &host)
:string_view{[&]
{
	char name[64]; switch(sigil)
	{
		case sigil::USER:
			generate_random_prefixed(sigil::USER, "guest", name, sizeof(name));
			break;

		case sigil::ALIAS:
			generate_random_prefixed(sigil::ALIAS, "", name, sizeof(name));
			break;

		default:
			generate_random_timebased(sigil, name, sizeof(name));
			break;
	};

	const auto len
	{
		fmt::snprintf(buf, max, "%s:%s",
		              name,
		              host)
	};

	return string_view { buf, size_t(len) };
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
ircd::m::id::generate_random_prefixed(const enum sigil &sigil,
                                      const string_view &prefix,
                                      char *const &buf,
                                      const size_t &max)
{
	const uint32_t num(rand::integer());
	const size_t len(fmt::snprintf(buf, max, "%c%s%u", char(sigil), prefix, num));
	return { buf, len };
}

ircd::string_view
ircd::m::id::generate_random_timebased(const enum sigil &sigil,
                                       char *const &buf,
                                       const size_t &max)
{
	const auto utime(microtime());
	const size_t len(snprintf(buf, max, "%c%zd%06d", char(sigil), utime.first, utime.second));
	return { buf, len };
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
