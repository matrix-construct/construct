// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

mapi::header IRCD_MODULE
{
	"IRCd terminal console: runtime-reloadable library supporting the console."
};

IRCD_EXCEPTION_HIDENAME(ircd::error, bad_command)

// Buffer all output into this rather than writing to std::cout. This allows
// the console to be reused easily inside the application (like a matrix room).
std::stringstream out;

static bool console_cmd__state(const string_view &line);
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

		case hash("state"):
			return console_cmd__state(args);
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
	for(const auto &mod : mods::available())
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

//
// state
//

static bool console_cmd__state_head(const string_view &line);
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
		case hash("head"):
			return console_cmd__state_head(args);

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
console_cmd__state_head(const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	char buf[m::state::ID_MAX_SZ];
	out << m::state::get_head(buf, room_id) << std::endl;
	return true;
}

bool
console_cmd__state_count(const string_view &line)
{
	const string_view arg
	{
		token(line, ' ', 0)
	};

	char buf[m::state::ID_MAX_SZ];
	const string_view head
	{
		startswith(arg, '!')? m::state::get_head(buf, arg) : arg
	};

	out << m::state::count(head) << std::endl;
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

	char buf[m::state::ID_MAX_SZ];
	const string_view head
	{
		startswith(arg, '!')? m::state::get_head(buf, arg) : arg
	};

	m::state::each(head, type, []
	(const string_view &key, const string_view &val)
	{
		out << key << " => " << val << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__state_get(const string_view &line)
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

	m::state::get__room(room_id, type, state_key, []
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

	char buf[m::state::ID_MAX_SZ];
	const string_view head
	{
		startswith(arg, '!')? m::state::get_head(buf, arg) : arg
	};

	m::state::dfs(head, []
	(const auto &key, const string_view &val, const uint &depth, const uint &pos)
	{
		out << std::setw(2) << depth << " + " << pos
		    << " : " << key << " => " << val
		    << std::endl;

		return true;
	});

	return true;
}
