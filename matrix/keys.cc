// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

std::ostream &
ircd::m::pretty_oneline(std::ostream &s,
                        const m::keys &keys)
{
	s << json::get<"server_name"_>(keys)
	  << ' ';

	char smbuf[32];
	s << smalldate(smbuf, json::get<"valid_until_ts"_>(keys) / 1000L)
	  << " (" << json::get<"valid_until_ts"_>(keys) << ")"
	  << ' ';

	for(const auto &[domain, verify_key_] : json::get<"verify_keys"_>(keys))
	{
		s << "key[ " << domain << ' ';
		for(const auto &[key_id, verify_key] : json::object(verify_key_))
			s << key_id << ' ';
		s << "] ";
	}

	for(const auto &[domain, signature_] : json::get<"signatures"_>(keys))
	{
		s << "sig[ " << domain << ' ';
		for(const auto &[key_id, signature] : json::object(signature_))
			s << key_id << ' ';
		s << "] ";
	}

	return s;
}

std::ostream &
ircd::m::pretty(std::ostream &s,
                const m::keys &keys)
{
	s << std::setw(16) << std::right << "server name  "
	  << json::get<"server_name"_>(keys)
	  << '\n';

	char tmbuf[64];
	s << std::setw(16) << std::right << "valid until  "
	  << timef(tmbuf, json::get<"valid_until_ts"_>(keys) / 1000, ircd::localtime)
	  << " (" << json::get<"valid_until_ts"_>(keys) << ")"
	  << '\n';

	for(const auto &[domain, verify_key_] : json::get<"verify_keys"_>(keys))
		for(const auto &[key_id, verify_key] : json::object(verify_key_))
			s << std::setw(16) << std::right << "[verify_key]  "
			  << domain << ' '
			  << key_id << ' '
			  << unquote(verify_key) << '\n';

	for(const auto &[domain, signature_] : json::get<"signatures"_>(keys))
		for(const auto &[key_id, signature] : json::object(signature_))
			s << std::setw(16) << std::right << "[signature]  "
			  << domain << ' '
			  << key_id << ' '
			  << unquote(signature) << '\n';

	for(const auto &[domain, old_verify_key_] : json::get<"old_verify_keys"_>(keys))
		for(const auto &[key_id, old_verify_key] : json::object(old_verify_key_))
			s << std::setw(16) << std::right << "[old_verify_key]  "
			  << domain << ' '
			  << key_id << ' '
			  << unquote(old_verify_key) << '\n';

	return s;
}

bool
ircd::m::verify(const m::keys &keys,
                std::nothrow_t)
noexcept try
{
	verify(keys);
	return true;
}
catch(const std::exception &e)
{
	log::error
	{
		m::log, "key verification for '%s' failed :%s",
		json::get<"server_name"_>(keys, "<no server name>"_sv),
		e.what()
	};

	return false;
}

void
ircd::m::verify(const m::keys &keys)
{
	const json::object &verify_keys
	{
		at<"verify_keys"_>(keys)
	};

	const string_view &key_id
	{
		!empty(verify_keys)?
			begin(verify_keys)->first:
			string_view{}
	};

	const json::object &key
	{
		!empty(verify_keys)?
			begin(verify_keys)->second:
			string_view{}
	};

	const ed25519::pk pk
	{
		[&key](auto&& pk)
		{
			b64::decode(pk, unquote(key.at("key")));
		}
	};

	const json::object &signatures
	{
		at<"signatures"_>(keys)
	};

	const json::string &server_name
	{
		at<"server_name"_>(keys)
	};

	const json::object &server_signatures
	{
		signatures.at(server_name)
	};

	const ed25519::sig sig
	{
		[&server_signatures, &key_id](auto&& sig)
		{
			b64::decode(sig, unquote(server_signatures.at(key_id)));
		}
	};

	m::keys copy{keys};
	at<"signatures"_>(copy) = string_view{};

	thread_local char buf[16_KiB];
	const const_buffer preimage
	{
		json::stringify(mutable_buffer{buf}, copy)
	};

	if(!pk.verify(preimage, sig))
		throw m::error
		{
			http::UNAUTHORIZED, "M_INVALID_SIGNATURE",
			"Failed to verify signature for public key of '%s'",
			server_name
		};

	if(expired(keys))
		log::warning
		{
			m::log, "key '%s' for '%s' expired on %s.",
			key_id,
			json::get<"server_name"_>(keys, "<no server name>"_sv),
			timestr(at<"valid_until_ts"_>(keys) / 1000L),
		};
}

bool
ircd::m::expired(const m::keys &keys)
{
	const auto &valid_until_ts
	{
		at<"valid_until_ts"_>(keys)
	};

	return valid_until_ts < ircd::time<milliseconds>();
}

//
// query
//

namespace ircd::m
{
	extern conf::item<milliseconds> keys_query_timeout;
}

decltype(ircd::m::keys_query_timeout)
ircd::m::keys_query_timeout
{
	{ "name",     "ircd.keys.query.timeout" },
	{ "default",  20000L                    }
};

bool
ircd::m::keys::query(const string_view &query_server,
                     const queries &queries,
                     const closure_bool &closure)
try
{
	assert(!query_server.empty());

	m::fed::key::opts opts;
	opts.remote = query_server;
	opts.dynamic = false;
	const unique_buffer<mutable_buffer> buf
	{
		32_KiB
	};

	m::fed::key::query request
	{
		queries, buf, std::move(opts)
	};

	const auto &code
	{
		request.get(milliseconds(keys_query_timeout))
	};

	const json::array response
	{
		request
	};

	bool ret(false);
	for(const json::object key : response) try
	{
		verify(m::keys{key});
		if(!closure(key))
			continue;

		cache::set(key);
		ret |= true;
	}
	catch(const std::exception &e)
	{
		log::derror
		{
			"Failed to verify keys for '%s' from '%s' :%s",
			key.get("server_name"),
			query_server,
			e.what()
		};
	}

	return ret;
}
catch(const ctx::timeout &e)
{
	throw m::error
	{
		http::REQUEST_TIMEOUT, "M_TIMEOUT",
		"Failed to query keys from '%s' in time",
		query_server
	};
}

//
// get
//

namespace ircd::m
{
	extern conf::item<milliseconds> keys_get_timeout;
}

decltype(ircd::m::keys_get_timeout)
ircd::m::keys_get_timeout
{
	{ "name",     "ircd.keys.get.timeout" },
	{ "default",  20000L                  }
};

size_t
ircd::m::keys::fetch(const queries &queries)
{
	size_t ret(0);
	get(queries, [&ret](const auto &)
	{
		++ret;
		return true;
	});

	return ret;
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
try
{
	assert(!server_name.empty());
	if(cache::get(server_name, key_id, closure))
		return true;

	if(server_name == my_host())
		throw m::NOT_FOUND
		{
			"key %s for '%s' (that's myself) not found",
			key_id?: "<all>"_sv,
			server_name
		};

	log::debug
	{
		log, "Keys for %s not cached; querying network...",
		server_name
	};

	const unique_buffer<mutable_buffer> buf
	{
		32_KiB
	};

	m::fed::key::opts opts;
	opts.dynamic = false;
	opts.remote = server_name;
	const m::fed::key::server_key query
	{
		server_name, key_id
	};

	m::fed::key::query request
	{
		{query}, buf, std::move(opts)
	};

	const auto status
	{
		request.get(milliseconds(keys_get_timeout))
	};

	// note fed::key::query gives us "server_keys" array via cast operator.
	const json::array response
	{
		request
	};

	for(const json::object keys : response)
	{
		if(unquote(keys["server_name"]) != server_name)
			continue;

		verify(m::keys(keys));
		log::debug
		{
			log, "Verified keys for '%s' from '%s'",
			server_name,
			server_name,
		};

		cache::set(keys);
		closure(keys);
		return true;
	}

	return false;
}
catch(const ctx::timeout &e)
{
	throw m::error
	{
		http::REQUEST_TIMEOUT, "M_TIMEOUT",
		"Failed to fetch keys for '%s' in time",
		server_name
	};
}

bool
ircd::m::keys::get(const queries &queries,
                   const closure_bool &closure)
{
	bool ret{true};
	std::vector<m::feds::opts> opts;
	opts.reserve(queries.size());
	for(const auto &[server_name, key_id] : queries) try
	{
		assert(key_id);
		assert(server_name);
		const bool cached
		{
			cache::get(server_name, key_id, [&ret, &closure]
			(const auto &object)
			{
				ret = closure(object);
			})
		};

		if(!ret)
			return ret;

		if(cached)
			continue;

		if(my_host(server_name))
		{
			log::derror
			{
				m::log, "key '%s' for '%s' (that's myself) not found.",
				key_id,
				server_name,
			};

			continue;
		}

		log::debug
		{
			m::log, "Key '%s' for %s not cached; querying network...",
			key_id,
			server_name,
		};

		opts.emplace_back();
		opts.back().op = feds::op::keys;
		opts.back().exclude_myself = true;
		opts.back().closure_errors = false;
		opts.back().nothrow_closure = true;
		opts.back().arg[0] = server_name;
		opts.back().arg[1] = key_id;
	}
	catch(const ctx::interrupted &)
	{
		throw;
	}
	catch(const std::exception &e)
	{
		log::error
		{
			log, "Failed to start request for key '%s' of '%s' :%s",
			key_id,
			server_name,
			e.what(),
		};
	}

	assert(opts.size() <= queries.size());
	m::feds::execute(opts, [&ret, &closure]
	(const auto &result)
	{
		const json::array &server_keys
		{
			result.object["server_keys"]
		};

		for(const json::object keys : server_keys)
		{
			const json::string &server_name
			{
				keys["server_name"]
			};

			if(server_name != result.request->arg[0] || server_name != result.origin)
			{
				log::derror
				{
					m::log, "Origin mismatch for '%s' got '%s' from '%s'",
					result.request->arg[0],
					server_name,
					result.origin,
				};

				continue;
			}

			if(!verify(m::keys(keys), std::nothrow))
			{
				log::derror
				{
					m::log, "Failed to verify key '%s' for '%s' from '%s'",
					result.request->arg[0],
					result.request->arg[1],
					result.origin,
				};

				continue;
			}

			cache::set(keys);
			if(!(ret = closure(keys)))
				return ret;
		}

		return ret;
	});

	return ret;
}

//
// m::keys::cache
//

size_t
ircd::m::keys::cache::set(const json::object &keys)
{
	const json::string &server_name
	{
		keys.at("server_name")
	};

	const m::node::room node_room
	{
		server_name
	};

	if(!exists(node_room.room_id))
		create(node_room, me());

	const auto send_to_cache{[&node_room, &keys]
	(const json::object::member &member)
	{
		const json::string &key_id{member.first};
		send(node_room, me(), "ircd.key", key_id, keys);
	}};

	size_t ret{0};
	static const size_t max{32};
	const json::object &old_vks{keys["old_verify_keys"]};
	for(auto it(begin(old_vks)); it != end(old_vks) && ret < max; ++it, ++ret)
		send_to_cache(*it);

	const json::object &vks{keys["verify_keys"]};
	for(auto it(begin(vks)); it != end(vks) && ret < max; ++it, ++ret)
		send_to_cache(*it);

	return ret;
}

bool
ircd::m::keys::cache::get(const string_view &server_name,
                          const string_view &key_id,
                          const keys::closure &closure)
{
	const m::node::room node_room
	{
		server_name
	};

	// Without a key_id we search for the most recent key; note this is not
	// the same as making a state_key="" query, as that would be an actual
	// ircd.key entry without an id (which shouldn't exist).
	const event::idx event_idx
	{
		key_id?
			node_room.get(std::nothrow, "ircd.key", key_id):
			node_room.get(std::nothrow, "ircd.key")
	};

	return event_idx?
		m::get(std::nothrow, event_idx, "content", closure):
		false;
}

bool
ircd::m::keys::cache::has(const string_view &server_name,
                          const string_view &key_id)
{
	const m::node::room node_room
	{
		server_name
	};

	// Without a key_id we search for the most recent key; note this is not
	// the same as making a state_key="" query, as that would be an actual
	// ircd.key entry without an id (which shouldn't exist).
	return
		key_id?
			node_room.has("ircd.key", key_id):
			node_room.has("ircd.key");
}

bool
ircd::m::keys::cache::for_each(const string_view &server_name,
                               const keys::closure_bool &closure)
{
	const m::node::room node_room
	{
		server_name
	};

	const m::room::state state
	{
		node_room
	};

	return state.for_each("ircd.key", [&closure]
	(const auto &type, const auto &key_id, const auto event_idx)
	{
		bool ret{true};
		m::get(std::nothrow, event_idx, "content", [&closure, &ret]
		(const json::object &content)
		{
			ret = closure(content);
		});

		return ret;
	});
}

///////////////////////////////////////////////////////////////////////////////
//
// (internal) ed25519 support sanity test
//

#ifdef RB_DEBUG
	#define TEST_ED25519 __attribute__((constructor))
#else
	#define TEST_ED25519
#endif

namespace ircd
{
	static void _test_ed25519() noexcept TEST_ED25519;
}

void
ircd::_test_ed25519()
noexcept try
{
	char seed_buf[ed25519::SEED_SZ + 10];
	const auto seed
	{
		b64::decode(seed_buf, "YJDBA9Xnr2sVqXD9Vj7XVUnmFZcZrlw8Md7kMW+3XA1")
	};

	ed25519::pk pk;
	ed25519::sk sk{&pk, seed};

	const auto SERVER_NAME {"domain"};
	const auto KEY_ID {"ed25519:1"};

	const auto test{[&]
	(const std::string &object) -> bool
	{
		const auto sig
		{
			sk.sign(const_buffer{object})
		};

		char sigb64_buf[128];
		const auto sigb64
		{
			b64::encode_unpadded(sigb64_buf, sig)
		};

		ed25519::sig unsig; const auto unsigb64
		{
			b64::decode(unsig, sigb64)
		};

		return pk.verify(const_buffer{object}, unsig);
	}};

	const bool tests[]
	{
		test(std::string{json::object
		{
			"{}"
		}}),

		test(json::strung(json::members
		{
			{ "one", 1L },
			{ "two", "Two" }
		})),
	};

	if(!std::all_of(begin(tests), end(tests), [](const bool &b) { return b; }))
		throw ircd::panic
		{
			"Seeded ed25519 test failed"
		};
}
catch(...)
{
	ircd::terminate
	{
		std::current_exception()
	};
}
