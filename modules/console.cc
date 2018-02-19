// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/util/params.h>

using namespace ircd;

mapi::header IRCD_MODULE
{
	"IRCd terminal console: runtime-reloadable library supporting the console."
};

IRCD_EXCEPTION_HIDENAME(ircd::error, bad_command)

// Buffer all output into this rather than writing to std::cout. This allows
// the console to be reused easily inside the application (like a matrix room).
std::stringstream out;

static bool console_cmd__fed(const string_view &line);
static bool console_cmd__room(const string_view &line);
static bool console_cmd__state(const string_view &line);
static bool console_cmd__event(const string_view &line);
static bool console_cmd__exec(const string_view &line);
static bool console_cmd__key(const string_view &line);
static bool console_cmd__db(const string_view &line);
static bool console_cmd__net(const string_view &line);
static bool console_cmd__mod(const string_view &line);

extern "C" int
console_command(const string_view &line, std::string &output)
try
{
	const unwind writeback{[&output]
	{
		output = out.str();
		out.str(std::string{});
	}};

	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	switch(hash(token(line, " ", 0)))
	{
		case hash("mod"):
			return console_cmd__mod(args);

		case hash("net"):
			return console_cmd__net(args);

		case hash("db"):
			return console_cmd__db(args);

		case hash("key"):
			return console_cmd__key(args);

		case hash("exec"):
			return console_cmd__exec(args);

		case hash("event"):
			return console_cmd__event(args);

		case hash("state"):
			return console_cmd__state(args);

		case hash("room"):
			return console_cmd__room(args);

		case hash("fed"):
			return console_cmd__fed(args);
	}

	return -1;
}
catch(const bad_command &e)
{
	return -2;
}

//
// mod
//

static bool console_cmd__mod_path(const string_view &line);
static bool console_cmd__mod_list(const string_view &line);
static bool console_cmd__mod_syms(const string_view &line);
static bool console_cmd__mod_reload(const string_view &line);

bool
console_cmd__mod(const string_view &line)
{
	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	switch(hash(token(line, " ", 0)))
	{
		case hash("path"):
			return console_cmd__mod_path(args);

		case hash("list"):
			return console_cmd__mod_list(args);

		case hash("syms"):
			return console_cmd__mod_syms(args);

		case hash("reload"):
			return console_cmd__mod_reload(args);

		default:
			throw bad_command{};
	}
}

bool
console_cmd__mod_path(const string_view &line)
{
	for(const auto &path : ircd::mods::paths)
		out << path << std::endl;

	return true;
}

bool
console_cmd__mod_list(const string_view &line)
{
	auto avflist(mods::available());
	const auto b(std::make_move_iterator(begin(avflist)));
	const auto e(std::make_move_iterator(end(avflist)));
	std::vector<std::string> available(b, e);
	std::sort(begin(available), end(available));

	for(const auto &mod : available)
	{
		const auto loadstr
		{
			mods::loaded(mod)? "\033[1;42m \033[0m" : " "
		};

		out << "[" << loadstr << "] " << mod << std::endl;
	}

	return true;
}

bool
console_cmd__mod_syms(const string_view &line)
{
	const std::string path
	{
		token(line, ' ', 0)
	};

	const std::vector<std::string> symbols
	{
		mods::symbols(path)
	};

	for(const auto &sym : symbols)
		out << sym << std::endl;

	out << " -- " << symbols.size() << " symbols in " << path << std::endl;
	return true;
}

bool
console_cmd__mod_reload(const string_view &line)
{
	const std::string name
	{
		token(line, ' ', 0)
	};

	if(!m::modules.erase(name))
	{
		out << name << " is not loaded." << std::endl;
		return true;
	}

	m::modules.emplace(name, name);
	return true;
}

//
// db
//

static bool console_cmd__db_list(const string_view &line);

bool
console_cmd__db(const string_view &line)
{
	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	switch(hash(token(line, " ", 0)))
	{
		default:
		case hash("list"):
			return console_cmd__db_list(args);
	}
}

bool
console_cmd__db_list(const string_view &line)
{
	const auto available
	{
		db::available()
	};

	for(const auto &path : available)
	{
		const auto name
		{
			lstrip(path, db::path("/"))
		};

		const auto it
		{
			db::database::dbs.find(name)
		};

		const auto &light
		{
			it != end(db::database::dbs)? "\033[1;42m \033[0m" : " "
		};

		out << "[" << light << "]"
		    << " " << name
		    << " `" << path << "'"
		    << std::endl;
	}

	return true;
}

//
// net
//

static bool console_cmd__net_host(const string_view &line);

bool
console_cmd__net(const string_view &line)
{
	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	switch(hash(token(line, " ", 0)))
	{
		case hash("host"):
			return console_cmd__net_host(args);

		default:
			throw bad_command{};
	}
}

static bool console_cmd__net_host__default(const string_view &line);

bool
console_cmd__net_host(const string_view &line)
{
	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	switch(hash(token(line, " ", 0)))
	{
		default:
			return console_cmd__net_host__default(line);
	}

	return true;
}

bool
console_cmd__net_host__default(const string_view &line)
{
	const params token
	{
		line, " ", {"host", "service"}
	};

	const auto &host{token.at(0)};
	const auto &service
	{
		token.count() > 1? token.at(1) : string_view{}
	};

	const net::hostport hostport
	{
		host, service
	};

	ctx::dock dock;
	bool done{false};
	net::ipport ipport;
	std::exception_ptr eptr;
	net::dns(hostport, [&done, &dock, &eptr, &ipport]
	(std::exception_ptr eptr_, const net::ipport &ipport_)
	{
		eptr = std::move(eptr_);
		ipport = ipport_;
		done = true;
		dock.notify_one();
	});

	while(!done)
		dock.wait();

	if(eptr)
		std::rethrow_exception(eptr);
	else
		std::cout << ipport << std::endl;

	return true;
}

//
// key
//

static bool console_cmd__key__default(const string_view &line);
static bool console_cmd__key__fetch(const string_view &line);
static bool console_cmd__key__get(const string_view &line);

bool
console_cmd__key(const string_view &line)
{
	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	if(!empty(args)) switch(hash(token(line, " ", 0)))
	{
		case hash("get"):
			return console_cmd__key__get(args);

		case hash("fetch"):
			return console_cmd__key__fetch(args);
	}

	return console_cmd__key__default(args);
}

bool
console_cmd__key__get(const string_view &line)
{
	const auto server_name
	{
		token(line, ' ', 0)
	};

	m::keys::get(server_name, [](const auto &keys)
	{
		out << keys << std::endl;
	});

	return true;
}

bool
console_cmd__key__fetch(const string_view &line)
{

	return true;
}

bool
console_cmd__key__default(const string_view &line)
{
	out << "origin:                  " << m::my_host() << std::endl;
	out << "public key ID:           " << m::self::public_key_id << std::endl;
	out << "public key base64:       " << m::self::public_key_b64 << std::endl;
	out << "TLS cert sha256 base64:  " << m::self::tls_cert_der_sha256_b64 << std::endl;

	return true;
}

//
// event
//

static bool console_cmd__event__default(const string_view &line);
static bool console_cmd__event__dump(const string_view &line);
static bool console_cmd__event__fetch(const string_view &line);

bool
console_cmd__event(const string_view &line)
{
	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	switch(hash(token(line, " ", 0)))
	{
		case hash("fetch"):
			return console_cmd__event__fetch(args);

		case hash("dump"):
			return console_cmd__event__dump(args);

		default:
			return console_cmd__event__default(line);
	}
}

bool
console_cmd__event__dump(const string_view &line)
{
	const auto filename
	{
		token(line, ' ', 0)
	};

	db::column column
	{
		*m::dbs::events, "event_id"
	};

	static char buf[512_KiB];
	char *pos{buf};
	size_t foff{0}, ecount{0}, acount{0}, errcount{0};
	m::event::fetch event;
	for(auto it(begin(column)); it != end(column); ++it, ++ecount)
	{
		const auto remain
		{
			size_t(buf + sizeof(buf) - pos)
		};

		assert(remain >= 64_KiB && remain <= sizeof(buf));
		const mutable_buffer mb{pos, remain};
		const string_view event_id{it->second};
		seek(event, event_id, std::nothrow);
		if(unlikely(!event.valid(event_id)))
		{
			++errcount;
			continue;
		}

		pos += json::print(mb, event);
		if(pos + 64_KiB > buf + sizeof(buf))
		{
			const const_buffer cb{buf, pos};
			foff += size(fs::append(filename, cb));
			pos = buf;
			++acount;
		}
	}

	if(pos > buf)
	{
		const const_buffer cb{buf, pos};
		foff += size(fs::append(filename, cb));
		++acount;
	}

	out << "Dumped " << ecount << " events"
	    << " using " << foff << " bytes"
	    << " in " << acount << " writes"
	    << " to " << filename
	    << " with " << errcount << " errors"
	    << std::endl;

	return true;
}

bool
console_cmd__event__fetch(const string_view &line)
{
	const m::event::id event_id
	{
		token(line, ' ', 0)
	};

	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	const auto host
	{
		!empty(args)? token(args, ' ', 0) : ""_sv
	};

	m::v1::event::opts opts;
	if(host)
		opts.remote = host;

	static char buf[96_KiB];
	m::v1::event request
	{
		event_id, buf, opts
	};

	//TODO: TO

	const auto code
	{
		request.get()
	};

	const m::event event
	{
		request
	};

	out << json::object{request} << std::endl;
	out << std::endl;
	out << pretty(event) << std::endl;
	return true;
}

bool
console_cmd__event__default(const string_view &line)
{
	const m::event::id event_id
	{
		token(line, ' ', 0)
	};

	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	static char buf[64_KiB];
	const m::event event
	{
		event_id, buf
	};

	if(!empty(args)) switch(hash(token(args, ' ', 0)))
	{
		case hash("raw"):
			out << json::object{buf} << std::endl;
			return true;
	}

	out << pretty(event) << std::endl;
	return true;
}

//
// state
//

static bool console_cmd__state_root(const string_view &line);
static bool console_cmd__state_count(const string_view &line);
static bool console_cmd__state_get(const string_view &line);
static bool console_cmd__state_each(const string_view &line);
static bool console_cmd__state_dfs(const string_view &line);

bool
console_cmd__state(const string_view &line)
{
	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	switch(hash(token(line, " ", 0)))
	{
		case hash("root"):
			return console_cmd__state_root(args);

		case hash("get"):
			return console_cmd__state_get(args);

		case hash("count"):
			return console_cmd__state_count(args);

		case hash("each"):
			return console_cmd__state_each(args);

		case hash("dfs"):
			return console_cmd__state_dfs(args);

		default:
			throw bad_command{};
	}
}

bool
console_cmd__state_count(const string_view &line)
{
	const string_view arg
	{
		token(line, ' ', 0)
	};

	const string_view root
	{
		arg
	};

	out << m::state::count(root) << std::endl;
	return true;
}

bool
console_cmd__state_each(const string_view &line)
{
	const string_view arg
	{
		token(line, ' ', 0)
	};

	const string_view type
	{
		token(line, ' ', 1)
	};

	const string_view root
	{
		arg
	};

	m::state::for_each(root, type, []
	(const string_view &key, const string_view &val)
	{
		out << key << " => " << val << std::endl;
	});

	return true;
}

bool
console_cmd__state_get(const string_view &line)
{
	const string_view root
	{
		token(line, ' ', 0)
	};

	const string_view type
	{
		token(line, ' ', 1)
	};

	const string_view state_key
	{
		token(line, ' ', 2)
	};

	m::state::get(root, type, state_key, []
	(const auto &value)
	{
		out << "got: " << value << std::endl;
	});

	return true;
}

bool
console_cmd__state_dfs(const string_view &line)
{
	const string_view arg
	{
		token(line, ' ', 0)
	};

	const string_view root
	{
		arg
	};

	m::state::dfs(root, []
	(const auto &key, const string_view &val, const uint &depth, const uint &pos)
	{
		out << std::setw(2) << depth << " + " << pos
		    << " : " << key << " => " << val
		    << std::endl;

		return true;
	});

	return true;
}

bool
console_cmd__state_root(const string_view &line)
{
	const m::event::id event_id
	{
		token(line, ' ', 0)
	};

	char buf[m::state::ID_MAX_SZ];
	out << m::dbs::state_root(buf, event_id) << std::endl;
	return true;
}

//
// exec
//

static bool console_cmd__exec_file(const string_view &line);

bool
console_cmd__exec(const string_view &line)
{
	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	switch(hash(token(line, " ", 0)))
	{
		default:
			return console_cmd__exec_file(line);
	}
}

bool
console_cmd__exec_file(const string_view &line)
{
	const params token{line, " ",
	{
		"file path", "limit", "start", "room_id"
	}};

	const auto path
	{
		token.at(0)
	};

	const auto limit
	{
		token.at<size_t>(1)
	};

	const auto start
	{
		token[2]? lex_cast<size_t>(token[2]) : 0
	};

	const string_view room_id
	{
		token[3]
	};

	struct m::vm::eval::opts opts;
	m::vm::eval eval
	{
		opts
	};

	size_t foff{0};
	size_t i(0), j(0), r(0);
	for(; !limit || i < limit; ++r)
	{
		static char buf[512_KiB];
		const string_view read
		{
			ircd::fs::read(path, buf, foff)
		};

		size_t boff(0);
		json::vector vector{read};
		for(; boff < size(read) && i < limit; ) try
		{
			const json::object object{*begin(vector)};
			boff += size(object);
			vector = { data(read) + boff, size(read) - boff };

			const m::event event{object};
			if(room_id && json::get<"room_id"_>(event) != room_id)
				continue;

			if(j++ < start)
				continue;

			eval(event);
			++i;
		}
		catch(const json::parse_error &e)
		{
			break;
		}

		foff += boff;
	}

	out << "Executed " << i
	    << " of " << j << " events"
	    << " in " << foff << " bytes"
	    << " using " << r << " reads"
	    << std::endl;

	return true;
}

//
// room
//

static bool console_cmd__room__redact(const string_view &line);
static bool console_cmd__room__message(const string_view &line);
static bool console_cmd__room__set(const string_view &line);
static bool console_cmd__room__get(const string_view &line);
static bool console_cmd__room__messages(const string_view &line);
static bool console_cmd__room__members(const string_view &line);
static bool console_cmd__room__state(const string_view &line);
static bool console_cmd__room__depth(const string_view &line);
static bool console_cmd__room__head(const string_view &line);

bool
console_cmd__room(const string_view &line)
{
	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	switch(hash(token(line, " ", 0)))
	{
		case hash("depth"):
			return console_cmd__room__depth(args);

		case hash("head"):
			return console_cmd__room__head(args);

		case hash("state"):
			return console_cmd__room__state(args);

		case hash("members"):
			return console_cmd__room__members(args);

		case hash("messages"):
			return console_cmd__room__messages(args);

		case hash("get"):
			return console_cmd__room__get(args);

		case hash("set"):
			return console_cmd__room__set(args);

		case hash("message"):
			return console_cmd__room__message(args);

		case hash("redact"):
			return console_cmd__room__redact(args);
	}

	return true;
}

bool
console_cmd__room__head(const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const m::room room
	{
		room_id
	};

	out << head(room_id) << std::endl;
	return true;
}

bool
console_cmd__room__depth(const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const m::room room
	{
		room_id
	};

	out << depth(room_id) << std::endl;
	return true;
}

bool
console_cmd__room__members(const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const m::room room
	{
		room_id
	};

	const m::room::members members
	{
		room
	};

	members.for_each([](const m::event &event)
	{
		out << pretty_oneline(event) << std::endl;
	});

	return true;
}

bool
console_cmd__room__state(const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const m::room room
	{
		room_id
	};

	const m::room::state state
	{
		room
	};

	state.for_each([](const m::event &event)
	{
		out << pretty_oneline(event) << std::endl;
	});

	return true;
}

bool
console_cmd__room__messages(const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const int64_t depth
	{
		token_count(line, ' ') > 1? lex_cast<int64_t>(token(line, ' ', 1)) : -1
	};

	const char order
	{
		token_count(line, ' ') > 2? token(line, ' ', 2).at(0) : 'b'
	};

	const m::room room
	{
		room_id
	};

	m::room::messages it{room};
	if(depth >= 0)
		it.seek(depth);

	for(; it; order == 'b'? --it : ++it)
		out << pretty_oneline(*it) << std::endl;

	return true;
}

bool
console_cmd__room__get(const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const string_view type
	{
		token(line, ' ', 1)
	};

	const string_view state_key
	{
		token(line, ' ', 2)
	};

	const m::room room
	{
		room_id
	};

	room.get(type, state_key, [](const m::event &event)
	{
		out << pretty(event) << std::endl;
	});

	return true;
}

bool
console_cmd__room__set(const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const m::user::id &sender
	{
		token(line, ' ', 1)
	};

	const string_view type
	{
		token(line, ' ', 2)
	};

	const string_view state_key
	{
		token(line, ' ', 3)
	};

	const json::object &content
	{
		token(line, ' ', 4)
	};

	const m::room room
	{
		room_id
	};

	const auto event_id
	{
		send(room, sender, type, state_key, content)
	};

	out << event_id << std::endl;
	return true;
}

bool
console_cmd__room__message(const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const m::user::id &sender
	{
		token(line, ' ', 1)
	};

	const string_view body
	{
		tokens_after(line, ' ', 1)
	};

	const m::room room
	{
		room_id
	};

	const auto event_id
	{
		message(room, sender, body)
	};

	out << event_id << std::endl;
	return true;
}

bool
console_cmd__room__redact(const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const m::event::id &redacts
	{
		token(line, ' ', 1)
	};

	const m::user::id &sender
	{
		token(line, ' ', 2)
	};

	const string_view reason
	{
		tokens_after(line, ' ', 2)
	};

	const m::room room
	{
		room_id
	};

	const auto event_id
	{
		redact(room, sender, redacts, reason)
	};

	out << event_id << std::endl;
	return true;
}

//
// fed
//

static bool console_cmd__fed__version(const string_view &line);

bool
console_cmd__fed(const string_view &line)
{
	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	switch(hash(token(line, " ", 0)))
	{
		case hash("version"):
			return console_cmd__fed__version(args);

		default:
			throw bad_command{};
	}

	return true;
}

bool
console_cmd__fed__version(const string_view &line)
{
	const net::hostport remote
	{
		token(line, ' ', 0)
	};

	m::v1::version::opts opts;
	opts.remote = remote;

	thread_local char buf[8_KiB];
	m::v1::version request
	{
		buf, opts
	};

	//TODO: TO
	const auto code
	{
		request.get()
	};

	const json::object &response
	{
		request
	};

	out << response << std::endl;
	return true;
}
