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

IRCD_EXCEPTION_HIDENAME(ircd::error, bad_command)

static void init_cmds();

mapi::header
IRCD_MODULE
{
	"IRCd terminal console: runtime-reloadable self-reflecting command library.", []
	{
		init_cmds();
	}
};

conf::item<seconds>
default_synapse
{
	{ "name",     "ircd.console.timeout" },
	{ "default",  45L                    },
};

/// The first parameter for all commands. This aggregates general options
/// passed to commands as well as providing the output facility with an
/// ostream interface. Commands should only send output to this object. The
/// command's input line is not included here; it's the second param to a cmd.
struct opt
{
	std::ostream &out;
	bool html {false};
	seconds timeout {default_synapse};
	string_view special;

	operator std::ostream &()
	{
		return out;
	}

	template<class T> auto &operator<<(T&& t)
	{
		out << std::forward<T>(t);
		return out;
	}

	auto &operator<<(std::ostream &(*manip)(std::ostream &))
	{
		return manip(out);
	}
};

/// Instances of this object are generated when this module reads its
/// symbols to find commands. These instances are then stored in the
/// cmds set for lookup and iteration.
struct cmd
{
	using is_transparent = void;

	static constexpr const size_t &MAX_DEPTH
	{
		8
	};

	std::string name;
	std::string symbol;
	mods::sym_ptr ptr;

	bool operator()(const cmd &a, const cmd &b) const
	{
		return a.name < b.name;
	}

	bool operator()(const string_view &a, const cmd &b) const
	{
		return a < b.name;
	}

	bool operator()(const cmd &a, const string_view &b) const
	{
		return a.name < b;
	}

	cmd(std::string name, std::string symbol)
	:name{std::move(name)}
	,symbol{std::move(symbol)}
	,ptr{ircd_module, this->symbol}
	{}

	cmd() = default;
	cmd(cmd &&) = delete;
	cmd(const cmd &) = delete;
};

std::set<cmd, cmd>
cmds;

void
init_cmds()
{
	auto symbols
	{
		mods::symbols(mods::path(ircd_module))
	};

	for(std::string &symbol : symbols)
	{
		// elide lots of grief by informally finding this first
		if(!has(symbol, "console_cmd"))
			continue;

		thread_local char buf[1024];
		const string_view demangled
		{
			demangle(buf, symbol)
		};

		std::string command
		{
			replace(between(demangled, "__", "("), "__", " ")
		};

		const auto iit
		{
			cmds.emplace(std::move(command), std::move(symbol))
		};

		if(!iit.second)
			throw error
			{
				"Command '%s' already exists", command
			};
	}
}

const cmd *
find_cmd(const string_view &line)
{
	const size_t elems
	{
		std::min(token_count(line, ' '), cmd::MAX_DEPTH)
	};

	for(size_t e(elems+1); e > 0; --e)
	{
		const auto name
		{
			tokens_before(line, ' ', e)
		};

		const auto it{cmds.lower_bound(name)};
		if(it == end(cmds) || it->name != name)
			continue;

		return &(*it);
	}

	return nullptr;
}

//
// Main command dispatch
//

int console_command_derived(opt &, const string_view &line);

static int
_console_command(opt &out,
                 const string_view &line)
{
	const cmd *const cmd
	{
		find_cmd(line)
	};

	if(!cmd)
		return console_command_derived(out, line);

	const auto args
	{
		lstrip(split(line, cmd->name).second, ' ')
	};

	const auto &ptr{cmd->ptr};
	using prototype = bool (struct opt &, const string_view &);
	return ptr.operator()<prototype>(out, args);
}

#pragma GCC visibility push(default)

bool
console_cmd__help(opt &,
                  const string_view &line);

/// This function may be linked and called by those wishing to execute a
/// command. Output from the command will be appended to the provided ostream.
/// The input to the command is passed in `line`. Since `struct opt` is not
/// accessible outside of this module, all public options are passed via a
/// plaintext string which is parsed here.
extern "C" int
console_command(std::ostream &out,
                const string_view &line,
                const string_view &opts)
try
{
	opt opt
	{
		out,
		has(opts, "html")
	};

	int ret
	{
		_console_command(opt, line)
	};

	if(ret < 0)
		if(console_cmd__help(opt, line))
			return -2;

	return ret;
}
catch(const params::error &e)
{
	out << e.what() << std::endl;
	return true;
}
catch(const bad_command &e)
{
	return -2;
}

//
// Derived commands
//

int console_command_numeric(opt &, const string_view &line);
bool console_id__user(opt &, const m::user::id &id, const string_view &line);
bool console_id__room(opt &, const m::room::id &id, const string_view &line);
bool console_id__event(opt &, const m::event::id &id, const string_view &line);
bool console_id__device(opt &, const m::device::id &id, const string_view &line);
bool console_id__group(opt &, const m::id::group &, const string_view &line);
bool console_id__node(opt &, const string_view &id, const string_view &line);
bool console_json(const json::object &);

int
console_command_derived(opt &out, const string_view &line)
{
	const string_view id
	{
		token(line, ' ', 0)
	};

	// First check if the line starts with a number, this is a special case
	// sent to a custom dispatcher (which right now is specifically for the
	// event stager suite).
	if(lex_castable<int>(id))
		return console_command_numeric(out, line);

	// Branch if the line starts with just a sigil (but not an identifier).
	// In this case we'll expand the sigil to its name as a convenience for
	// the apropos command suite.
	if(m::has_sigil(id) && size(id) == 1)
	{
		char lower_buf[16];
		const fmt::snstringf expanded_line
		{
			size(line) + 16, "%s %s",
			tolower(lower_buf, reflect(m::sigil(id))),
			tokens_after(line, ' ', 0),
		};

		return _console_command(out, expanded_line);
	}

	// Branch if the line starts with an identifier; identifiers are
	// themselves convenience commands.
	if(m::has_sigil(id)) switch(m::sigil(id))
	{
		case m::id::EVENT:
			return console_id__event(out, id, line);

		case m::id::ROOM:
			return console_id__room(out, id, line);

		case m::id::USER:
			return console_id__user(out, id, line);

		case m::id::DEVICE:
			return console_id__device(out, id, line);

		case m::id::GROUP:
			return console_id__group(out, id, line);

		case m::id::NODE:
			return console_id__node(out, id, line);

		case m::id::ROOM_ALIAS:
		{
			const auto room_id{m::room_id(id)};
			return console_id__room(out, room_id, line);
		}

		default:
			return -2;
	}

	return -1;
}

//
// Command by JSON
//

bool
console_json(const json::object &object)
{
	if(!object.has("type"))
		return true;

	//return console_cmd__exec__event(object);
	return true;
}

///////////////////////////////////////////////////////////////////////////////
//
// Console commands
//
// Function names take the format of `console_cmd__%s` where the command
// starts at %s. The handler that matches the beginning of the command is
// called. To match spaces, a `__` double-underscore is used in the function
// name.
//
// The remainder of the command after the match becomes the `line` arg
// to the handler.
//
// The `opt &out` argument is an arbitrary state structure for general use and
// also serves as an output stream for the response of a command. Note that the
// output of a command may be a tty or the event content in a local matrix room.
//

// Time cmd prefix (like /usr/bin/time)

bool
console_cmd__time(opt &out, const string_view &line)
{
	ircd::timer timer;

	const auto ret
	{
		_console_command(out, line)
	};

	thread_local char buf[32];
	out << std::endl
	    << pretty(buf, timer.at<microseconds>())
	    << std::endl;

	return ret;
}

// Help

bool
console_cmd__help(opt &out, const string_view &line)
{
	if(empty(line))
		for(size_t i(0); !empty(info::credits[i]); ++i)
			out << info::credits[i] << std::endl;

	const auto cmd
	{
		find_cmd(line)
	};

	if(cmd)
	{
		out << "No help available for '" << cmd->name << "'."
		    << std::endl;

		//TODO: help string symbol map
	}

	out << "\nSubcommands available:\n"
	    << std::endl;

	const size_t elems
	{
		std::min(token_count(line, ' '), cmd::MAX_DEPTH)
	};

	size_t num(0);
	for(size_t e(elems+1); e > 0; --e)
	{
		const auto name
		{
			tokens_before(line, ' ', e)
		};

		string_view last;
		auto it{cmds.lower_bound(name)};
		if(it == end(cmds))
			continue;

		for(; it != end(cmds); ++it)
		{
			if(!startswith(it->name, name))
				break;

			const auto prefix
			{
				tokens_before(it->name, ' ', e)
			};

			if(last == prefix)
				continue;

			if(name && prefix != name && !startswith(lstrip(prefix, name), ' '))
				break;

			last = prefix;
			const auto suffix
			{
				e > 1? tokens_after(prefix, ' ', e - 2) : prefix
			};

			if(empty(suffix))
				continue;

			out << std::left << std::setw(20) << suffix;
			if(++num % 4 == 0)
				out << std::endl;
		}

		break;
	}

	return true;
}

//
// util 
//

bool
console_cmd__exit(opt &out, const string_view &line)
{
	return false;
}

//
// Test trigger stub
//
bool
console_cmd__test(opt &out, const string_view &line)
{
	const bool result
	{
		ircd_test(line)
	};

	return true;
}

bool
console_cmd__stringify(opt &out, const string_view &line)
{
	out << json::value{line} << std::endl;
	return true;
}

bool
console_cmd__credits(opt &out, const string_view &line)
{
	for(size_t i(0); !empty(info::credits[i]); ++i)
		out << info::credits[i] << std::endl;

	return true;
}

bool
console_cmd__debug(opt &out, const string_view &line)
{
	if(!RB_DEBUG_LEVEL)
		out << "Debugging is not compiled in. Some messages optimized out."
		    << std::endl
		    << std::endl;

	const params param{line, " ",
	{
		"onoff"
	}};

	if(param["onoff"] == "on")
	{
		out << "Turning on debuglog..." << std::endl;
		while(!log::console_enabled(log::DEBUG))
			log::console_enable(log::DEBUG);
	}
	else if(param["onoff"] == "off")
	{
		out << "Turning off debuglog..." << std::endl;
		log::console_disable(log::DEBUG);
	}
	else if(log::console_enabled(log::DEBUG))
	{
		out << "Turning off debuglog..." << std::endl;
		log::console_disable(log::DEBUG);
	} else {
		out << "Turning on debuglog..." << std::endl;
		while(!log::console_enabled(log::DEBUG))
			log::console_enable(log::DEBUG);
	}

	// When not compiled in debug-mode we attempt to set all DEBUG related
	// levels (DERROR / DWARNING) to always match DEBUG. In debug-mode
	// compilation they remain independent, but if we don't do this in release
	// mode it will leave the user with DERROR messages which weren't DCE'ed
	if(!RB_DEBUG_LEVEL)
	{
		if(log::console_enabled(log::DEBUG))
		{
			log::console_enable(log::DERROR);
			log::console_enable(log::DWARNING);
		} else {
			log::console_disable(log::DERROR);
			log::console_disable(log::DWARNING);
		}
	}

	return true;
}

bool
console_cmd__demangle(opt &out, const string_view &line)
{
	out << ircd::demangle(line) << std::endl;
	return true;
}

bool
console_cmd__bt(opt &out, const string_view &line)
{
	const ircd::backtrace bt;
	for(size_t i(0); i < bt.size(); ++i)
	{
		out
		<< std::dec << std::setw(3) << i << ':'
		<< ' ' << std::hex << '[' << uintptr_t(bt[i]) << ']'
		<< std::endl;
	}

	return true;
}

//
// main
//

bool
console_cmd__restart(opt &out, const string_view &line)
{
	std::string argv(line);

	size_t swargs(0), posargs(0);
	ircd::tokens(line, ' ', [&swargs, &posargs]
	(const auto &token)
	{
		swargs += startswith(token, '-');
		posargs += !startswith(token, '-');
	});

	if(!posargs)
	{
		argv += swargs? " "_sv: ""_sv;
		argv += m::origin(m::my());
		argv += ' ';
		argv += m::server_name(m::my());
	}

	ircd::restart.set(argv);
	ircd::quit();
	return false;
}

bool
console_cmd__die(opt &out, const string_view &line)
{
	ircd::quit();
	return false;
}

[[noreturn]] bool
console_cmd__die__hard(opt &out, const string_view &line)
{
	ircd::terminate();
	__builtin_unreachable();
}

bool
console_cmd__sync(opt &out, const string_view &line)
{
	for(const auto &db : db::database::list)
	{
		sync(*db);
		out << "synchronized " << name(*db) << '.' << std::endl;
	}

	return true;
}

//
// log
//

bool
console_cmd__log(opt &out, const string_view &line)
{
	for(const auto *const &log : log::log::list)
		out << (log->snote? log->snote : '-')
		    << " " << std::setw(24) << std::left << log->name
		    << " "
		    << (log->fmasked? " FILE" : "")
		    << (log->cmasked? " CONSOLE" : "")
		    << std::endl;

	return true;
}

bool
console_cmd__log__level(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"level",
	}};

	if(!param.count())
	{
		for(auto i(0U); i < num_of<log::level>(); ++i)
			if(i > RB_LOG_LEVEL)
				out << "[\033[1;40m-\033[0m] " << reflect(log::level(i)) << std::endl;
			else if(console_enabled(log::level(i)))
				out << "[\033[1;42m+\033[0m] " << reflect(log::level(i)) << std::endl;
			else
				out << "[\033[1;41m-\033[0m] " << reflect(log::level(i)) << std::endl;

		return true;
	}

	const auto level_string
	{
		param["level"]
	};

	uint level;
	switch(hash(level_string))
	{
		case "CRITICAL"_:  level = 0U;   break;
		case "ERROR"_:     level = 1U;   break;
		case "WARNING"_:   level = 2U;   break;
		case "NOTICE"_:    level = 3U;   break;
		case "INFO"_:      level = 4U;   break;
		case "DWARNING"_:  level = 5U;   break;
		case "DERROR"_:    level = 6U;   break;
		case "DEBUG"_:     level = 7U;   break;
		default:           level = -1U;  break;
	};

	for(auto i(0U); i < num_of<log::level>(); ++i)
		if(i > RB_LOG_LEVEL)
		{
			out << "[\033[1;40m-\033[0m] " << reflect(log::level(i)) << std::endl;
		}
		else if(i <= level)
		{
			console_enable(log::level(i));
			out << "[\033[1;42m+\033[0m] " << reflect(log::level(i)) << std::endl;
		} else {
			console_disable(log::level(i));
			out << "[\033[1;41m-\033[0m] " << reflect(log::level(i)) << std::endl;
		}

	return true;
}

bool
console_cmd__log__mask(opt &out, const string_view &line)
{
	log::console_mask(tokens<std::vector>(line, ' '));

	out << std::endl;
	console_cmd__log(out, {});
	out << std::endl;
	console_cmd__log__level(out, {});
	return true;
}

bool
console_cmd__log__unmask(opt &out, const string_view &line)
{
	log::console_unmask(tokens<std::vector>(line, ' '));

	out << std::endl;
	console_cmd__log(out, {});
	out << std::endl;
	console_cmd__log__level(out, {});
	return true;
}

bool
console_cmd__log__mark(opt &out, const string_view &line)
{
	const string_view &msg
	{
		empty(line)?
			"marked by console":
			line
	};

	log::mark
	{
		msg
	};

	out << "The log files were marked with '" << msg
	    << "'"
	    << std::endl;

	return true;
}

bool
console_cmd__mark(opt &out, const string_view &line)
{
	return console_cmd__log__mark(out, line);
}

bool
console_cmd__log__flush(opt &out, const string_view &line)
{
	log::flush();
	return true;
}

//
// info
//

bool
console_cmd__version(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"all"
	}};

	if(param["all"] != "-a")
	{
		out << ircd_version << std::endl;
		return true;
	}

	out << "ircd_name                  " << ircd_name << std::endl;
	out << "ircd_version               " << ircd_version << std::endl;
	out << std::endl;

	out << "info::name                 " << info::name << std::endl;
	out << "info::version              " << info::version << std::endl;
	out << "info::tag                  " << info::tag << std::endl;
	out << "info::branch               " << info::branch << std::endl;
	out << "info::commit               " << info::commit << std::endl;
	out << "info::user_agent           " << info::user_agent << std::endl;
	out << "info::server_agent         " << info::server_agent << std::endl;
	out << std::endl;

	out << "VERSION                    " << VERSION << std::endl;
	out << std::endl;

	out << "BRANDING_NAME              " << BRANDING_NAME << std::endl;
	out << "BRANDING_VERSION           " << BRANDING_VERSION << std::endl;
	out << std::endl;

	out << "PACKAGE                    " << PACKAGE_VERSION << std::endl;
	out << "PACKAGE_VERSION            " << PACKAGE_VERSION << std::endl;
	out << "PACKAGE_NAME               " << PACKAGE_NAME << std::endl;
	out << "PACKAGE_STRING             " << PACKAGE_STRING << std::endl;
	out << "PACKAGE_VERSION            " << PACKAGE_VERSION << std::endl;
	out << "PACKAGE_TARNAME            " << PACKAGE_TARNAME << std::endl;
	out << std::endl;

	out << "RB_VERSION                 " << RB_VERSION << std::endl;
	out << "RB_VERSION_BRANCH          " << RB_VERSION_BRANCH << std::endl;
	out << "RB_VERSION_COMMIT          " << RB_VERSION_COMMIT << std::endl;
	out << "RB_VERSION_TAG             " << RB_VERSION_TAG << std::endl;
	out << std::endl;

	out << "info::configured           " << info::configured << std::endl;
	out << "info::compiled             " << info::compiled << std::endl;
	out << "info::startup              " << info::startup << std::endl;
	out << std::endl;

	out << "RB_DATESTR                 " << RB_DATESTR << std::endl;
	out << "RB_TIME_CONFIGURED         " << RB_TIME_CONFIGURED << std::endl;
	out << "RB_DATE_CONFIGURED         " << RB_DATE_CONFIGURED << std::endl;
	out << std::endl;

	return true;
}

bool
console_cmd__versions(opt &out, const string_view &line)
{
	out
	<< std::left << std::setw(6)  << "TYPE"
	<< " "
	<< std::left << std::setw(16) << "NAME"
	<< " "
	<< std::left << std::setw(14) << "MONOTONIC"
	<< " "
	<< std::left << std::setw(14) << "SEMANTIC"
	<< " "
	<< std::left << std::setw(16) << ":STRING"
	<< " "
	<< std::endl;

	for(const auto &version : info::versions::list)
	{
		const auto &type
		{
			version->type == version->API? "API":
			version->type == version->ABI? "ABI":
			                               "???"
		};

		char buf[32];
		const string_view semantic{fmt::sprintf
		{
			buf, "%ld.%ld.%ld",
			version->semantic[0],
			version->semantic[1],
			version->semantic[2],
		}};

		out
		<< std::left << std::setw(6) << type
		<< " "
		<< std::left << std::setw(16) << version->name
		<< " "
		<< std::left << std::setw(14) << version->monotonic
		<< " "
		<< std::left << std::setw(14) << semantic
		<< " :"
		<< std::left << std::setw(16) << version->string
		<< " "
		<< std::endl;
	}

	return true;
}

bool
console_cmd__info(opt &out, const string_view &line)
{
	info::dump();

	out << "Library information was written to the INFO and DEBUG logs."
	    << std::endl;

	return true;
}

bool
console_cmd__uptime(opt &out, const string_view &line)
{
	const seconds uptime
	{
		ircd::uptime()
	};

	char tmp[48];
	out << pretty(tmp, uptime) << std::endl;
	return true;
}

bool
console_cmd__date(opt &out, const string_view &line)
{
	out << ircd::time() << " sec" << std::endl;
	out << ircd::time<milliseconds>() << " ms" << std::endl;
	out << ircd::time<microseconds>() << " us" << std::endl;

	thread_local char buf[128];
	const auto now{ircd::now<system_point>()};
	out << timef(buf, now, ircd::localtime) << std::endl;
	out << timef(buf, now) << " (UTC)" << std::endl;

	return true;
}

//
// filesystem
//

bool
console_cmd__fs__ls(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"path_or_option", "[path]"
	}};

	const string_view option
	{
		startswith(param["path_or_option"], '-')?
			param["path_or_option"]:
			string_view{}
	};

	string_view path
	{
		option?
			param["[path]"]:
			param["path_or_option"]
	};

	const std::string cwd
	{
		!path?
			fs::cwd():
			std::string{}
	};

	if(!path)
		path = cwd;

	const auto list
	{
		option == "-r" || option == "-R"?
			fs::ls_r(path):
			fs::ls(path)
	};

	for(const auto &file : list)
		out << file << std::endl;

	return true;
}

bool
console_cmd__fs__dev(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"type"
	}};

	const string_view type
	{
		param["type"]
	};

	out
	<< std::setw(3) << std::right << "maj" << ':'
	<< std::setw(3) << std::left << "min" << ' '
	<< std::setw(10) << std::right << "TYPE" << ' '
	<< std::setw(12) << std::left << " " << ' '
	<< std::setw(6) << std::right << "NR_REQ" << ' '
	<< std::setw(6) << std::right << "DEPTH" << ' '
	<< std::setw(5) << std::right << "MERGE" << ' '
	<< std::setw(5) << std::right << "OPTSZ" << ' '
	<< std::setw(5) << std::right << "MINSZ" << ' '
	<< std::setw(5) << std::right << "LOGSZ" << ' '
	<< std::setw(5) << std::right << "PHYSZ" << ' '
	<< std::setw(6) << std::right << "SECTSZ" << ' '
	<< std::setw(14) << std::right << "SECTORS" << ' '
	<< std::setw(26) << "SIZE" << ' '
	<< std::setw(10) << std::right << "REV" << ' '
	<< std::setw(20) << std::left << "MODEL" << ' '
	<< std::setw(16) << std::left << "VENDOR" << ' '
	<< std::setw(24) << std::left << "SCHED" << ' '
	<< std::endl;

	fs::dev::for_each(type, [&out]
	(const ulong &id, const fs::dev::blk &dev)
	{
		const auto mm(fs::dev::id(id));
		char pbuf[48];
		out
		<< std::setw(3) << std::right << std::get<0>(mm) << ':'
		<< std::setw(3) << std::left << std::get<1>(mm) << ' '
		<< std::setw(10) << std::right << dev.type << ' '
		<< std::setw(12) << std::left << (dev.rotational? "rotating"_sv : string_view{}) << ' '
		<< std::setw(6) << std::right << dev.nr_requests << ' '
		<< std::setw(6) << std::right << dev.queue_depth << ' '
		<< std::setw(5) << std::right << (dev.merges? 'Y' : 'N') << ' '
		<< std::setw(5) << std::right << dev.optimal_io << ' '
		<< std::setw(5) << std::right << dev.minimum_io << ' '
		<< std::setw(5) << std::right << dev.logical_block << ' '
		<< std::setw(5) << std::right << dev.physical_block << ' '
		<< std::setw(6) << std::right << dev.sector_size << ' '
		<< std::setw(14) << std::right << dev.sectors << ' '
		<< std::setw(26) << pretty(pbuf, iec(dev.sectors * dev.sector_size)) << ' '
		<< std::setw(10) << std::right << dev.rev << ' '
		<< std::setw(20) << std::left << dev.model << ' '
		<< std::setw(16) << std::left << dev.vendor << ' '
		<< std::setw(24) << std::left << dev.scheduler << ' '
		<< std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__ls(opt &out, const string_view &line)
{
	return console_cmd__fs__ls(out, line);
}

//
// proc
//

bool
console_cmd__proc(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"filename"
	}};

	const auto filename
	{
		param.at("filename", ""_sv)
	};

	static const auto prefix
	{
		"/proc/self/"_sv
	};

	char pathbuf[128];
	const string_view path{fmt::sprintf
	{
		pathbuf, "%s%s", prefix, filename
	}};

	if(fs::is_dir(path))
	{
		for(const auto &file : fs::ls(path))
			out << lstrip(file, prefix)
			    << (fs::is_dir(file)? "/" : "")
			    << std::endl;

		return true;
	}

	fs::fd fd
	{
		path, std::ios::in
	};

	fs::read_opts opts;
	opts.aio = false;
	opts.offset = 0;
	const unique_buffer<mutable_buffer> buf
	{
		info::page_size
	};

	string_view read; do
	{
		read = fs::read(fd, buf, opts);
		opts.offset += size(read);
		out << read;
	}
	while(!empty(read));

	out << std::endl;
	return true;
}

// Specialized command, strips lines which have 0 values for shorter output.
bool
console_cmd__proc__smaps(opt &out, const string_view &line)
{
	fs::fd fd
	{
		"/proc/self/smaps", std::ios::in //TODO: XXX windows
	};

	fs::read_opts opts;
	opts.aio = false;
	opts.offset = 0;
	const unique_buffer<mutable_buffer> buf
	{
		4_MiB
	};

	const string_view read
	{
		fs::read(fd, buf, opts)
	};

	ircd::tokens(read, '\n', [&out]
	(const string_view &line)
	{
		const auto &[key, val]
		{
			split(line, ':')
		};

		if(lstrip(val, ' ') == "0 kB")
			return;

		out << line << std::endl;
	});

	return true;
}

//
// mem
//

bool
console_cmd__mem(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"opts"
	}};

	// Optional options string passed to implementation; might not be available
	// or ignored. See jemalloc(3) etc.
	const string_view &opts
	{
		param["opts"]
	};

	auto &this_thread
	{
		ircd::allocator::profile::this_thread
	};

	char pbuf[2][48];
	if(this_thread.alloc_count)
		out << "IRCd thread allocations:" << std::endl
		    << "alloc count:  " << this_thread.alloc_count << std::endl
		    << "freed count:  " << this_thread.free_count << std::endl
		    << "alloc bytes:  " << pretty(pbuf[0], iec(this_thread.alloc_bytes)) << std::endl
		    << "freed bytes:  " << pretty(pbuf[1], iec(this_thread.free_bytes)) << std::endl
		    << std::endl;

	if(opts == "ircd")
		return true;

	thread_local char buf[48_KiB];
	out << "Allocator information:" << std::endl
	    << allocator::info(buf, opts) << std::endl
	    ;

	return true;
}

bool
console_cmd__mem__trim(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"pad"
	}};

	const size_t &pad
	{
		param.at<size_t>("pad", 0UL)
	};

	const auto ret
	{
		ircd::allocator::trim(pad)
	};

	out << "malloc trim "
	    << (ret? "was able to release some memory." : "did not release any memory.")
	    << std::endl;

	return true;
}

bool
console_cmd__mem__set(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"key", "type", "val"
	}};

	const string_view &key
	{
		param.at("key")
	};

	const string_view &type
	{
		param.at("type", "string"_sv)
	};

	string_view set;
	thread_local char buf[2][4_KiB];
	switch(hash(type))
	{
		case "void"_:
			set = {};
			break;

		case "bool"_:
			*reinterpret_cast<bool *>(buf[0]) = lex_cast<bool>(param.at("val"));
			set = { buf[0], sizeof(bool) };
			break;

		case "size_t"_:
			*reinterpret_cast<size_t *>(buf[0]) = lex_cast<size_t>(param.at("val"));
			set = { buf[0], sizeof(size_t) };
			break;

		case "ssize_t"_:
			*reinterpret_cast<ssize_t *>(buf[0]) = lex_cast<ssize_t>(param.at("val"));
			set = { buf[0], sizeof(ssize_t) };
			break;

		case "unsigned"_:
			*reinterpret_cast<unsigned *>(buf[0]) = lex_cast<unsigned>(param.at("val"));
			set = { buf[0], sizeof(unsigned) };
			break;

		case "uint64_t"_:
			*reinterpret_cast<uint64_t *>(buf[0]) = lex_cast<uint64_t>(param.at("val"));
			set = { buf[0], sizeof(uint64_t) };
			break;

		case "uint64_t*"_:
			*reinterpret_cast<uintptr_t *>(buf[0]) = lex_cast<uintptr_t>(param.at("val"));
			set = { buf[0], sizeof(uintptr_t) };
			break;

		default:
		case "string"_:
			set = param.at("val");
			break;
	}

	const string_view &get
	{
		allocator::set(key, set, buf[1])
	};

	return true;
}

bool
console_cmd__mem__get(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"key", "type"
	}};

	const string_view &key
	{
		param.at("key")
	};

	const string_view &type
	{
		param.at("type", "unsigned"_sv)
	};

	char buf[512];
	const string_view &val
	{
		allocator::get(key, buf)
	};

	switch(hash(type))
	{
		case "void"_:
			out << std::endl;
			break;

		case "bool"_:
			out << lex_cast(*reinterpret_cast<const bool *>(data(val))) << std::endl;
			break;

		case "size_t"_:
			out << lex_cast(*reinterpret_cast<const size_t *>(data(val))) << std::endl;
			break;

		case "ssize_t"_:
			out << lex_cast(*reinterpret_cast<const ssize_t *>(data(val))) << std::endl;
			break;

		default:
		case "unsigned"_:
			out << lex_cast(*reinterpret_cast<const unsigned *>(data(val))) << std::endl;
			break;

		case "uint64_t"_:
			out << lex_cast(*reinterpret_cast<const uint64_t *>(data(val))) << std::endl;
			break;

		case "uint64_t*"_:
			out << lex_cast(*reinterpret_cast<const uintptr_t *>(data(val))) << std::endl;
			break;

		case "string"_:
			out << *reinterpret_cast<const char *const *const>(data(val)) << std::endl;
			break;
	}

	return true;
}

//
// vg
//

bool
console_cmd__vg(opt &out, const string_view &line)
{
	if(vg::active)
		out << "running on valgrind" << std::endl;
	else
		out << "bare metal" << std::endl;

	return true;
}

//
// prof
//

bool
console_cmd__prof__psi(opt &out, const string_view &line)
{
	if(!prof::psi::supported)
		throw error
		{
			"Pressure Still Information is not supported."
		};

	const auto show_file{[&out]
	(const string_view &name, prof::psi::file &file)
	{
		if(!refresh(file))
			return;

		const auto show_metric{[&out, &name]
		(const auto &metric, const string_view &metric_name)
		{
			char pbuf[48];
			out
			<< std::left << std::setw(6) << name
			<< ' '
			<< metric_name << " stall window   "
			<< pretty(pbuf, metric.stall.window)
			<< " ("
			<< metric.stall.window.count()
			<< ')'
			<< std::endl;

			out
			<< std::left << std::setw(6) << name
			<< ' '
			<< metric_name << " stall last     "
			<< pretty(pbuf, metric.stall.relative)
			<< " ("
			<< metric.stall.relative.count()
			<< ") "
			<< metric.stall.pct << '%'
			<< std::endl;

			out
			<< std::left << std::setw(6) << name
			<< ' '
			<< metric_name << " stall total    "
			<< pretty(pbuf, metric.stall.total)
			<< " ("
			<< metric.stall.total.count()
			<< ')'
			<< std::endl;

			for(size_t i(0); i < metric.avg.size(); i++)
				out
				<< std::left << std::setw(6) << name
				<< ' '
				<< metric_name << " "
				<< std::right << std::setw(4) << metric.avg.at(i).window.count()
				<< "s          "
				<< std::right << metric.avg.at(i).pct << '%'
				<< std::endl;
		}};

		show_metric(file.some, "some");
		show_metric(file.full, "full");
	}};

	const params param{line, " ",
	{
		"file", "metric", "threshold", "window"
	}};

	string_view filename
	{
		param["file"]
	};

	const string_view &metric
	{
		param["metric"]
	};

	const string_view &threshold
	{
		param["threshold"]
	};

	const string_view &window
	{
		param["window"]
	};

	if(metric && threshold && window)
	{
		const fmt::bsprintf<64> trigger
		{
			"%s %s %s",
			metric,
			threshold,
			window,
		};

		auto *const trigfile
		{
			filename == "cpu"?     &prof::psi::cpu:
			filename == "memory"?  &prof::psi::mem:
			filename == "io"?      &prof::psi::io:
			                       nullptr
		};

		if(!trigfile)
			throw error
			{
				"Unknown file '%s'",
				filename
			};

		prof::psi::trigger trig[1]
		{
			{ *trigfile, trigger }
		};

		auto &file
		{
			prof::psi::wait(trig)
		};

		out
		<< "Got: " << file.name
		<< std::endl << std::endl;
		filename = file.name;
	}

	if(!filename || filename == "cpu")
		show_file("cpu", prof::psi::cpu);

	if(!filename || filename == "memory")
		show_file("memory", prof::psi::mem);

	if(!filename || filename == "io")
		show_file("io ", prof::psi::io);

	return true;
}

bool
console_cmd__prof__vg__start(opt &out, const string_view &line)
{
	prof::vg::start();
	return true;
}

bool
console_cmd__prof__vg__stop(opt &out, const string_view &line)
{
	prof::vg::stop();
	return true;
}

bool
console_cmd__prof__vg__reset(opt &out, const string_view &line)
{
	prof::vg::reset();
	return true;
}

bool
console_cmd__prof__vg__toggle(opt &out, const string_view &line)
{
	prof::vg::toggle();
	return true;
}

bool
console_cmd__prof__vg__dump(opt &out, const string_view &line)
{
	char reason[128];
	prof::vg::dump(data(ircd::strlcpy(reason, line)));
	return true;
}

//
// env
//

bool
console_cmd__env(opt &out, const string_view &line)
{
	if(!::environ)
		throw error
		{
			"Env variable list not available."
		};

	const params param{line, " ",
	{
		"key"
	}};

	if(param["key"] == "*")
	{
		for(const char *const *e(::environ); *e; ++e)
			out << *e << std::endl;

		return true;
	}

	if(param["key"])
	{
		out << util::getenv(param["key"]) << std::endl;
		return true;
	}

	for(const char *const *e(::environ); *e; ++e)
	{
		string_view kv[2];
		tokens(*e, '=', kv);
		if(!startswith(kv[0], "IRCD_") && !startswith(kv[0], "ircd_"))
			continue;

		out << std::setw(64) << std::left << kv[0]
		    << " :" << kv[1]
		    << std::endl;
	}

	return true;
}

//
// stats
//

bool
console_cmd__stats(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"prefix", "all"
	}};

	const bool all
	{
		param[0] == "-a" || param[1] == "-a"
	};

	const string_view prefix
	{
		param[0] == "-a"? param[1]: param[0]
	};

	for(const auto &item : stats::items)
	{
		if(prefix && !startswith(item->name, prefix))
			continue;

		assert(item);
		if(!all && !*item)
			continue;

		static constexpr size_t name_width
		{
			80
		};

		const fmt::bsprintf<128> name
		{
			"%s ", trunc(item->name, name_width)
		};

		out
		<< std::left << std::setfill('_') << std::setw(name_width) << name
		<< " " << (*item)
		<< std::endl;
	}

	return true;
}

//
// ios
//

bool
console_cmd__ios(opt &out, const string_view &line)
{
	out << std::left << std::setw(3) << "ID"
	    << " " << std::left << std::setw(48) << "NAME"
	    << " " << std::right << std::setw(6) << "QUEUED"
	    << " " << std::right << std::setw(13) << "LAST LATENCY"
	    << " " << std::right << std::setw(13) << "AVG LATENCY"
	    << " " << std::right << std::setw(13) << "AVG CYCLES"
	    << " " << std::right << std::setw(13) << "LAST CYCLES"
	    << " " << std::right << std::setw(10) << "CALLS"
	    << " " << std::right << std::setw(10) << "ALLOCS"
	    << " " << std::right << std::setw(10) << "FREES"
	    << " " << std::right << std::setw(26) << "ALLOCATED NOW"
	    << " " << std::right << std::setw(26) << "ALLOCATED TOTAL"
	    << " " << std::right << std::setw(8) << "FAULTS"
	    << std::endl
	    ;

	const auto stats_output{[&out]
	(const auto &s)
	{
		thread_local char pbuf[64];
		const auto latency_avg
		{
			s.calls?
				(long double)s.latency_total / (long double)s.calls:
				0.0L
		};

		const auto cycles_avg
		{
			s.calls?
				(long double)s.slice_total / (long double)s.calls:
				0.0L
		};

		out
		<< " " << std::right << std::setw(6) << s.queued
		<< " " << std::right << std::setw(13) << pretty(pbuf, si(ulong(s.latency_last)), 2)
		<< " " << std::right << std::setw(13) << pretty(pbuf, si(ulong(latency_avg)), 2)
		<< " " << std::right << std::setw(13) << pretty(pbuf, si(ulong(cycles_avg)), 2)
		<< " " << std::right << std::setw(13) << pretty(pbuf, si(s.slice_last), 2)
		<< " " << std::right << std::setw(10) << s.calls
		<< " " << std::right << std::setw(10) << s.allocs
		<< " " << std::right << std::setw(10) << s.frees
		<< " " << std::right << std::setw(26) << pretty(pbuf, iec(s.alloc_bytes - s.free_bytes))
		<< " " << std::right << std::setw(26) << pretty(pbuf, iec(s.alloc_bytes))
		<< " " << std::right << std::setw(8) << s.faults
		;
	}};

	for(const auto *const &descriptor : ios::descriptor::list)
	{
		assert(descriptor);
		const auto &d(*descriptor);

		assert(d.stats);
		const auto &s(*d.stats);

		out
		<< std::left << std::setw(3) << d.id
		<< " " << std::left << std::setw(48) << d.name;

		stats_output(s);
		out << std::endl;
	}

	return true;
}

bool
console_cmd__ios__record(opt &out, const string_view &line)
{
	std::map<uint64_t, std::tuple<uint64_t, const ios::descriptor *>> map;
	for(const auto *const &descriptor : ios::descriptor::list)
	{
		assert(descriptor);
		const auto &d(*descriptor);

		const auto &history(d.history);
		const auto &pos(d.history_pos);

		for(size_t i(pos); i < 256; ++i)
		{
			if(!history[i][0])
				continue;

			map.emplace(history[i][0], std::make_tuple(history[i][1], descriptor));
		}

		for(size_t i(0); i < pos; ++i)
		{
			if(!history[i][0])
				continue;

			map.emplace(history[i][0], std::make_tuple(history[i][1], descriptor));
		}
	}

	uint64_t last(0);
	for(const auto &[epoch, tuple] : map)
	{
		const auto &[cyc, desc]
		{
			tuple
		};

		const char ch
		{
			epoch == last + 1? '|': 'T'
		};

		out
		<< " " << ch
		<< std::right
		<< " " << std::setw(12) << epoch
		<< " " << std::setw(12) << cyc
		<< " " << std::left << std::setw(36) << desc->name;

		out
		<< std::endl;

		last = epoch;
	}

	return true;
}

bool
console_cmd__ios__history(opt &out, const string_view &line)
{
	for(const auto *const &descriptor : ios::descriptor::list)
	{
		assert(descriptor);
		const auto &d(*descriptor);

		const auto &history(d.history);
		const auto &pos(d.history_pos);

		out
		<< std::left << std::setw(3) << d.id
		<< " " << std::left << std::setw(48) << d.name
		<< std::endl;

		size_t k(0);
		for(size_t i(pos); i < 256; ++i)
		{
			if(!history[i][0])
				continue;

			out
			<< "[" << std::right << std::setw(9) << history[i][0]
			<< " |" << std::right << std::setw(9) << history[i][1]
			<< "] ";

			if(++k % 12 == 0)
				out << std::endl;
		}

		for(size_t i(0); i < pos; ++i)
		{
			if(!history[i][0])
				continue;

			out
			<< "[" << std::right << std::setw(9) << history[i][0]
			<< " |" << std::right << std::setw(9) << history[i][1]
			<< "] ";

			if(++k % 12 == 0)
				out << std::endl;
		}

		out << std::endl;
	}

	return true;
}

bool
console_cmd__ios__depth(opt &out, const string_view &line)
{
	static ios::descriptor dispatch_desc
	{
		"ircd.console.depth.dispatch",
	};

	static ios::descriptor post_desc
	{
		"ircd.console.depth.post",
	};

	static ios::descriptor defer_desc
	{
		"ircd.console.latency.defer",
		nullptr,
		nullptr,
		true,
	};

	uint64_t returned, executed, started;

	// ios::dispatch
	{
		started = ios::epoch();
		ios::dispatch(dispatch_desc, ios::yield, [&executed]
		{
			executed = ios::epoch();
		});

		returned = ios::epoch();
	}

	out
	<< "disp send:    "  << (executed - started)    << std::endl
	<< "disp recv:    "  << (returned - executed)   << std::endl
	<< "disp rtt:     "  << (returned - started)    << std::endl
	<< std::endl;

	// ios::post
	{
		started = ios::epoch();
		ios::dispatch(post_desc, ios::defer, ios::yield, [&executed]
		{
			executed = ios::epoch();
		});

		returned = ios::epoch();
	}

	out
	<< "post send:    "  << (executed - started)    << std::endl
	<< "post recv:    "  << (returned - executed)   << std::endl
	<< "post rtt:     "  << (returned - started)    << std::endl
	<< std::endl;

	// ios::defer
	{
		started = ios::epoch();
		ios::dispatch(defer_desc, ios::defer, ios::yield, [&executed]
		{
			executed = ios::epoch();
		});

		returned = ios::epoch();
	}

	out
	<< "defer send:   "  << (executed - started)    << std::endl
	<< "defer recv:   "  << (returned - executed)   << std::endl
	<< "defer rtt:    "  << (returned - started)    << std::endl
	<< std::endl;

	return true;
}

#ifdef __x86_64__
bool
console_cmd__ios__latency(opt &out, const string_view &line)
{
	static ios::descriptor dispatch_desc
	{
		"ircd.console.latency.dispatch"
	};

	static ios::descriptor post_desc
	{
		"ircd.console.latency.post"
	};

	static ios::descriptor defer_desc
	{
		"ircd.console.latency.defer",
		nullptr,
		nullptr,
		true,
	};

	long long returned, executed, started;

	// control
	{
		__sync_synchronize();
		asm volatile ("lfence");
		started = prof::cycles();
		asm volatile ("lfence");

		__sync_synchronize();
		asm volatile ("lfence");
		executed = prof::cycles();
		asm volatile ("lfence");

		__sync_synchronize();
		asm volatile ("lfence");
		returned = prof::cycles();
		asm volatile ("lfence");
	}

	out
	<< "tsc send:     "  << (executed - started)    << std::endl
	<< "tsc recv:     "  << (returned - executed)   << std::endl
	<< "tsc rtt:      "  << (returned - started)    << std::endl
	<< std::endl;

	//
	// ios::dispatch
	//

	{
		__sync_synchronize();
		asm volatile ("lfence");
		started = prof::cycles();
		asm volatile ("lfence");

		ios::dispatch(dispatch_desc, ios::yield, [&executed]
		{
			__sync_synchronize();
			asm volatile ("lfence");
			executed = prof::cycles();
			asm volatile ("lfence");
		});

		__sync_synchronize();
		asm volatile ("lfence");
		returned = prof::cycles();
		asm volatile ("lfence");
	}

	out
	<< "disp send:    "  << (executed - started)    << std::endl
	<< "disp recv:    "  << (returned - executed)   << std::endl
	<< "disp rtt:     "  << (returned - started)    << std::endl
	<< std::endl;

	//
	// ios::post
	//

	{
		__sync_synchronize();
		asm volatile ("lfence");
		started = prof::cycles();
		asm volatile ("lfence");

		ios::dispatch(post_desc, ios::defer, ios::yield, [&executed]
		{
			__sync_synchronize();
			asm volatile ("lfence");
			executed = prof::cycles();
			asm volatile ("lfence");
		});

		__sync_synchronize();
		asm volatile ("lfence");
		returned = prof::cycles();
		asm volatile ("lfence");
	}

	out
	<< "post send:    "  << (executed - started)    << std::endl
	<< "post recv:    "  << (returned - executed)   << std::endl
	<< "post rtt:     "  << (returned - started)    << std::endl
	<< std::endl;

	//
	// ios::defer
	//

	{
		__sync_synchronize();
		asm volatile ("lfence");
		started = prof::cycles();
		asm volatile ("lfence");

		ios::dispatch(defer_desc, ios::defer, ios::yield, [&executed]
		{
			__sync_synchronize();
			asm volatile ("lfence");
			executed = prof::cycles();
			asm volatile ("lfence");
		});

		__sync_synchronize();
		asm volatile ("lfence");
		returned = prof::cycles();
		asm volatile ("lfence");
	}

	out
	<< "defer send:   "  << (executed - started)    << std::endl
	<< "defer recv:   "  << (returned - executed)   << std::endl
	<< "defer rtt:    "  << (returned - started)    << std::endl
	<< std::endl;

	return true;
}
#endif

//
// aio
//

bool
console_cmd__aio(opt &out, const string_view &line)
{
	if(!fs::aio::system)
		throw error
		{
			"AIO is not available."
		};

	const auto &s
	{
		fs::aio::stats
	};

	out << std::setw(18) << std::left << "requests"
	    << std::setw(9) << std::right << s.requests
	    << "   " << pretty(iec(s.bytes_requests))
	    << std::endl;

	out << std::setw(18) << std::left << "requests cur"
	    << std::setw(9) << std::right << (s.requests - s.complete)
	    << "   " << pretty(iec(s.bytes_requests - s.bytes_complete))
	    << std::endl;

	out << std::setw(18) << std::left << "requests que"
	    << std::setw(9) << std::right << s.cur_queued
	    << std::endl;

	out << std::setw(18) << std::left << "requests out"
	    << std::setw(9) << std::right << s.cur_submits
	    << std::endl;

	out << std::setw(18) << std::left << "requests out max"
	    << std::setw(9) << std::right << s.max_submits
	    << std::endl;

	out << std::setw(18) << std::left << "requests avg"
	    << std::setw(9) << std::right << "-"
	    << "   " << pretty(iec(s.bytes_requests / s.requests))
	    << std::endl;

	out << std::setw(18) << std::left << "requests max"
	    << std::setw(9) << std::right << s.max_requests
	    << std::endl;

	out << std::setw(18) << std::left << "reads"
	    << std::setw(9) << std::right << s.reads
	    << "   " << pretty(iec(s.bytes_read))
	    << std::endl;

	out << std::setw(18) << std::left << "reads cur"
	    << std::setw(9) << std::right << s.cur_reads
	    << std::endl;

	out << std::setw(18) << std::left << "reads avg"
	    << std::setw(9) << std::right << "-"
	    << "   " << pretty(iec(s.bytes_read / s.reads))
	    << std::endl;

	out << std::setw(18) << std::left << "reads max"
	    << std::setw(9) << std::right << s.max_reads
	    << std::endl;

	out << std::setw(18) << std::left << "writes"
	    << std::setw(9) << std::right << s.writes
	    << "   " << pretty(iec(s.bytes_write))
	    << std::endl;

	out << std::setw(18) << std::left << "writes cur"
	    << std::setw(9) << std::right << s.cur_writes
	    << "   " << pretty(iec(s.cur_bytes_write))
	    << std::endl;

	out << std::setw(18) << std::left << "writes avg"
	    << std::setw(9) << std::right << "-"
	    << "   " << pretty(iec(s.bytes_write / s.writes))
	    << std::endl;

	out << std::setw(18) << std::left << "writes max"
	    << std::setw(9) << std::right << s.max_writes
	    << std::endl;

	out << std::setw(18) << std::left << "submits"
	    << std::setw(9) << std::right << s.submits
	    << std::endl;

	out << std::setw(18) << std::left << "handles"
	    << std::setw(9) << std::right << s.handles
	    << std::endl;

	out << std::setw(18) << std::left << "events"
	    << std::setw(9) << std::right << s.events
	    << std::endl;

	out << std::setw(18) << std::left << "stalls"
	    << std::setw(9) << std::right << s.stalls
	    << std::endl;

	out << std::setw(18) << std::left << "errors"
	    << std::setw(9) << std::right << s.errors
	    << "   " << pretty(iec(s.bytes_errors))
	    << std::endl;

	out << std::setw(18) << std::left << "cancel"
	    << std::setw(9) << std::right << s.cancel
	    << "   " << pretty(iec(s.bytes_cancel))
	    << std::endl;

	return true;
}

//
// conf
//

bool
console_cmd__conf__list(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"prefix"
	}};

	const auto prefix
	{
		param.at("prefix", string_view{})
	};

	const unique_mutable_buffer val
	{
		4_KiB
	};

	for(const auto &[key, item_p] : conf::items)
	{
		if(prefix && !startswith(key, prefix))
			continue;

		const string_view _key
		{
			fmt::sprintf{val, "%s ", key}
		};

		out
		<< std::setw(64) << std::left << std::setfill('_') << _key
		<< " " << item_p->get(val + size(_key))
		<< std::endl;
	}

	return true;
}

bool
console_cmd__conf(opt &out, const string_view &line)
{
	return console_cmd__conf__list(out, line);
}

bool
console_cmd__conf__set(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"key", "value"
	}};

	const auto &key
	{
		param.at(0)
	};

	const auto &val
	{
		tokens_after(line, ' ', 0)
	};

	const auto event_id
	{
		m::my().conf->set(key, val)
	};

	out << event_id << " <- " << key << " = " << val << std::endl;
	return true;
}

bool
console_cmd__conf__get(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"key"
	}};

	const auto &key
	{
		param.at("key")
	};

	const unique_mutable_buffer val
	{
		4_KiB
	};

	for(const auto &[_key, item_p] : conf::items)
	{
		if(_key != key)
			continue;

		assert(item_p);
		out << item_p->get(val) << std::endl;
		return true;
	}

	throw m::NOT_FOUND
	{
		"Conf item '%s' not found", key
	};
}

bool
console_cmd__conf__rehash(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"prefix"
	}};

	string_view prefix
	{
		param.at("prefix", "*"_sv)
	};

	if(prefix == "*")
		prefix = string_view{};

	const auto stored
	{
		m::my().conf->store(prefix)
	};

	out
	<< "Saved runtime conf items"
	<< (prefix? " with the prefix "_sv : string_view{})
	<< (prefix? prefix : string_view{})
	<< " from the current state into "
	<< m::my().conf->room_id
	<< std::endl;

	return true;
}

bool
console_cmd__conf__default(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"prefix"
	}};

	const auto &prefix
	{
		param["prefix"]
	};

	const auto defaulted
	{
		m::my().conf->defaults(prefix)
	};

	out
	<< "Set "
	<< defaulted
	<< " runtime conf items"
	<< (prefix? " with the prefix "_sv : string_view{})
	<< (prefix? prefix : string_view{})
	<< " to their default value."
	<< std::endl
	<< "Note: These values must be saved with the rehash command"
	<< " to survive a restart/reload."
	<< std::endl;

	return true;
}

bool
console_cmd__conf__load(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "prefix"
	}};

	const auto &room_id
	{
		startswith(param["room_id"], '!')?
			param["room_id"]:
			m::my().conf->room_id
	};

	const auto &prefix
	{
		startswith(param["room_id"], '!')?
			param["prefix"]:
			param["room_id"]
	};

	// TODO: interface for room_id

	const auto loaded
	{
		m::my().conf->load(prefix)
	};

	out
	<< "Updated "
	<< loaded
	<< " runtime conf items from the current state in "
	<< room_id
	<< std::endl;

	return true;
}

bool
console_cmd__conf__reset(opt &out, const string_view &line)
{
	ircd::conf::reset();
	out << "All conf items which execute code upon a change"
	    << " have done so with the latest runtime value."
	    << std::endl;

	return true;
}

bool
console_cmd__conf__diff(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"key"
	}};

	const auto &key
	{
		param[0]
	};

	const unique_buffer<mutable_buffer> buf
	{
		4_KiB
	};

	out << std::setw(48) << std::left << "NAME"
	    << " | " << std::setw(36) << "CURRENT"
	    << " | " << std::setw(36) << "DEFAULT"
	    << std::endl;

	for(const auto &item : conf::items)
	{
		if(!startswith(item.first, key))
			continue;

		const json::string &default_
		{
			item.second->feature.get("default")
		};

		const auto &val
		{
			item.second->get(buf)
		};

		if(val == default_)
			continue;

		out << std::setw(48) << std::left << item.first
		    << " | " << std::setw(36) << val
		    << " | " << std::setw(36) << default_
		    << std::endl;
	}

	return true;
}

//
// hook
//

bool
console_cmd__hook__list(opt &out, const string_view &line)
{
	for(const auto &site : m::hook::base::site::list)
	{
		out
		<< std::left << site->name() << ':'
		<< std::endl
		<< string_view{site->feature}
		<< std::endl
		<< "matchers:    " << site->matchers
		<< std::endl
		<< "count:       " << site->count
		<< std::endl
		<< "calls:       " << site->calls
		<< std::endl
		<< "calling:     " << site->calling
		<< std::endl
		<< std::endl
		;

		for(const auto &hookp : site->hooks)
			out
			<< (hookp->registered? '+' : '-')
			<< " " << std::setw(4) << std::left << hookp->id()
			<< " " << std::setw(8) << std::right << hookp->calls
			<< " " << std::setw(3) << std::right << hookp->calling
			<< " " << string_view{hookp->feature}
			<< std::endl
			;

		out
		<< std::endl
		;
	}

	return true;
}

bool
console_cmd__hook(opt &out, const string_view &line)
{
	return console_cmd__hook__list(out, line);
}

//
// mod
//

bool
console_cmd__mod(opt &out, const string_view &line)
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
			mods::loaded(mod)? "\033[1;32;42m+\033[0m" : " "
		};

		out << "[" << loadstr << "] " << mod << std::endl;
	}

	return true;
}

bool
console_cmd__mod__path(opt &out, const string_view &line)
{
	for(const auto &path : ircd::mods::paths)
		out << path << std::endl;

	return true;
}

bool
console_cmd__mod__sections(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"path"
	}};

	const string_view path
	{
		param.at("path")
	};

	auto sections(mods::sections(path));
	std::sort(begin(sections), end(sections));
	for(const auto &name : sections)
	{
		out << name;

		const auto &symbols(mods::symbols(path, name));
		if(!symbols.empty())
			out << " (" << symbols.size() << ")";

		out << std::endl;
	}

	out << std::endl;
	return true;
}

bool
console_cmd__mod__symbols(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"path", "section"
	}};

	const string_view path
	{
		param.at("path")
	};

	const string_view section
	{
		param.at("section", string_view{})
	};

	const std::vector<std::string> symbols
	{
		mods::symbols(path, section)
	};

	for(const auto &sym : symbols)
		out << sym << std::endl;

	out << " -- " << symbols.size() << " symbols in " << path;

	if(section)
		out << " in " << section;

	out << std::endl;
	return true;
}

bool
console_cmd__mod__mangles(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"path", "section"
	}};

	const string_view path
	{
		param.at("path")
	};

	const string_view section
	{
		param.at("section", string_view{})
	};

	const auto mangles
	{
		mods::mangles(path, section)
	};

	for(const auto &p : mangles)
		out << p.first << "  " << p.second << std::endl;

	out << std::endl;
	return true;
}

bool
console_cmd__mod__exports(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"name"
	}};

	const string_view name
	{
		param.at("name")
	};

	if(!mods::loaded(name))
		throw error
		{
			"Module '%s' is not loaded", name
		};

	const module module
	{
		name
	};

	auto &exports
	{
		mods::exports(module)
	};

	for(const auto &p : exports)
		out << p.first << "  " << p.second << std::endl;

	out << std::endl;
	return true;
}

bool
console_cmd__mod__reload(opt &out, const string_view &line)
{
	const auto names
	{
		tokens<std::vector>(line, ' ')
	};

	for(auto it(names.begin()); it != names.end(); ++it)
	{
		const auto &name{*it};
		if(mods::imports.erase(std::string{name}))
		{
			out << name << " unloaded." << std::endl;
			break;
		}
	}

	for(auto it(names.rbegin()); it != names.rend(); ++it)
	{
		const auto &name{*it};
		if(mods::imports.emplace(std::string{name}, name).second)
			out << name << " loaded." << std::endl;
		else
			out << name << " is already loaded." << std::endl;
	}

	return true;
}

bool
console_cmd__mod__load(opt &out, const string_view &line)
{
	tokens(line, ' ', [&out]
	(const string_view &name)
	{
		if(mods::imports.find(name) != end(mods::imports))
		{
			out << name << " is already loaded." << std::endl;
			return;
		}

		mods::imports.emplace(std::string{name}, name);
		out << name << " loaded." << std::endl;
	});

	return true;
}

bool
console_cmd__mod__unload(opt &out, const string_view &line)
{
	tokens(line, ' ', [&out]
	(const string_view &name)
	{
		if(!mods::imports.erase(std::string{name}))
		{
			out << name << " is not loaded." << std::endl;
			return;
		}

		out << "unloaded " << name << std::endl;
	});

	return true;
}

bool
console_cmd__mod__links(opt &out, const string_view &line)
{
	size_t i(0);
	mods::ldso::for_each([&out, &i]
	(const auto &link)
	{
		out << std::setw(2) << (i++)
		    << " " << mods::ldso::fullname(link)
		    << std::endl;

		return true;
	});

	return true;
}

bool
console_cmd__mod__needed(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"name"
	}};

	const string_view name
	{
		param.at("name")
	};

	size_t i(0);
	mods::ldso::for_each_needed(mods::ldso::get(name), [&out, &i]
	(const string_view &name)
	{
		out << std::setw(2) << (i++)
		    << " " << name
		    << std::endl;

		return true;
	});

	return true;
}

//
// ctx
//

bool
console_cmd__ctx__interrupt(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"id", "[id]..."
	}};

	size_t count(0);
	for(size_t i(0); i < param.count() && cont; ++i)
		count += !ctx::for_each([&](auto &ctx)
		{
			if(id(ctx) == param.at<uint64_t>(i))
			{
				interrupt(ctx);
				return false;
			}
			else return true;
		});

	return true;
}

bool
console_cmd__ctx__prof(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"id",
	}};

	const auto display{[&out]
	(const ctx::prof::ticker &t)
	{
		for_each<ctx::prof::event>([&out, &t]
		(const auto &event)
		{
			out << std::left << std::setw(15) << std::setfill('_') << reflect(event)
			    << " " << t.event.at(uint8_t(event))
			    << std::endl;
		});
	}};

	if(!param["id"])
	{
		out << "Profile totals for all contexts:\n"
		    << std::endl;

		display(ctx::prof::get());
		return true;
	}

	bool cont{true};
	for(size_t i(0); i < param.count() && cont; ++i)
		cont = ctx::for_each([&](auto &ctx)
		{
			if(id(ctx) == param.at<uint64_t>(i))
			{
				out << "Profile for ctx:" << id(ctx) << " '" << name(ctx) << "':\n"
				    << std::endl;

				display(ctx::prof::get(ctx));
				return false;
			}
			else return true;
		});

	return true;
}

bool
console_cmd__ctx__term(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"id", "[id]..."
	}};

	size_t count(0);
	for(size_t i(0); i < param.count() && cont; ++i)
		count += !ctx::for_each([&](auto &ctx)
		{
			if(id(ctx) == param.at<uint64_t>(i))
			{
				ctx::terminate(ctx);
				return false;
			}
			else return true;
		});

	return true;
}

bool
console_cmd__ctx__list(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"name",
	}};

	const auto name_filter
	{
		param["name"]
	};

	out << " "
	    << std::setw(5)
	    << "ID"
	    << " "
	    << std::setw(7)
	    << "STATE"
	    << " "
	    << std::setw(8)
	    << "YIELDS"
	    << " "
	    << std::setw(5)
	    << "NOTES"
	    << " "
	    << std::setw(15)
	    << "CYCLE COUNT"
	    << " "
	    << std::setw(6)
	    << "PCT"
	    << " "
	    << std::setw(25)
	    << "STACK"
	    << " "
	    << std::setw(25)
	    << "PEAK OBSERVED"
	    << " "
	    << std::setw(25)
	    << "IN CORE"
	    << " "
	    << std::setw(25)
	    << "LIMIT"
	    << " "
	    << std::setw(6)
	    << "PCT"
	    << " "
	    << ":NAME"
	    << std::endl;

	ctx::for_each([&out, &name_filter]
	(auto &ctx)
	{
		if(name_filter)
			if(name(ctx) != name_filter)
				return true;

		out << std::setw(5) << std::right << id(ctx);
		out << " "
		    << (started(ctx)? 'A' : '-')
		    << (finished(ctx)? 'F' : '-')
		    << (termination(ctx)? 'T' : '-')
		    << (interruptible(ctx)? '-' : 'N')
		    << (waiting(ctx)? 'S' : '-')
		    << (queued(ctx)? 'Q' : '-')
		    << (interruption(ctx)? 'I' : '-')
		    << (running(ctx)? 'R' : '-')
		    ;

		out << " "
		    << std::setw(8) << std::right << epoch(ctx);

		out << " "
		    << std::setw(5) << std::right << notes(ctx);

		out << " "
		    << std::setw(15) << std::right << cycles(ctx);

		const long double total_cyc(ctx::prof::get(ctx::prof::event::CYCLES));
		const auto tsc_pct
		{
			total_cyc > 0.0? (cycles(ctx) / total_cyc) : 0.0L
		};

		out << " "
		    << std::setw(5) << std::right << std::fixed << std::setprecision(2) << (tsc_pct * 100)
		    << "%";

		thread_local char pbuf[32];
		out << " "
		    << std::setw(25) << std::right << pretty(pbuf, iec(ctx::stack::get(ctx).at));

		out << " "
		    << std::setw(25) << std::right << pretty(pbuf, iec(ctx::stack::get(ctx).peak));

		out << " "
		    << std::setw(25) << std::right << pretty(pbuf, iec(allocator::incore(ctx::stack::get(ctx).buf)));

		out << " "
		    << std::setw(25) << std::right << pretty(pbuf, iec(ctx::stack::get(ctx).max));

		const auto stack_pct
		{
			ctx::stack::get(ctx).at / (long double)(ctx::stack::get(ctx).max)
		};

		out << " "
		    << std::setw(5) << std::right << std::fixed << std::setprecision(2) << (stack_pct * 100)
		    << "%";

		out << " :"
		    << name(ctx);

		out << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__ctx(opt &out, const string_view &line)
{
	if(empty(line))
		return console_cmd__ctx__list(out, line);

	return true;
}

//
// db
//

bool
console_cmd__db__compressions(opt &out, const string_view &line)
{
	out << "Available compressions:"
	    << std::endl
	    << std::endl;

	for(const auto &[name, type] : db::compressions)
		if(!name.empty())
			out << name << std::endl;

	return true;
}

bool
console_cmd__db__pause(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname",
	}};

	const auto dbname
	{
		param.at(0)
	};

	auto &database
	{
		db::database::get(dbname)
	};

	bgpause(database);
	out << "Paused background jobs for '" << dbname << "'" << std::endl;
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__continue(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname",
	}};

	const auto dbname
	{
		param.at(0)
	};

	auto &database
	{
		db::database::get(dbname)
	};

	bgcontinue(database);
	out << "Resumed background jobs for '" << dbname << "'" << std::endl;
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__cancel(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname",
	}};

	const auto dbname
	{
		param.at(0)
	};

	auto &database
	{
		db::database::get(dbname)
	};

	bgcancel(database);
	out << "canceld background jobs for '" << dbname << "'" << std::endl;
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__sync(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname",
	}};

	const auto dbname
	{
		param.at(0)
	};

	auto &database
	{
		db::database::get(dbname)
	};

	sync(database);
	out << "done" << std::endl;
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__refresh(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname",
	}};

	const auto dbname
	{
		param.at(0)
	};

	auto &database
	{
		db::database::get(dbname)
	};

	if(!database.slave)
	{
		out << dbname << " is the master. Can only refresh slaves." << std::endl;
		return true;
	}

	const auto before_dbseq{sequence(database)};
	const auto before_retired{m::vm::sequence::retired};

	refresh(database);

	m::event::id::buf event_id;
	if(dbname == "events")
		m::vm::sequence::retired = m::vm::sequence::get(event_id);

	out
	<< dbname << " refreshed from "
	<< before_dbseq
	<< " to "
	<< sequence(database)
	<< std::endl;

	if(dbname == "events")
	{
		out
		<< "latest event from "
		<< before_retired
		<< " to "
		<< m::vm::sequence::retired
		<< " ["
		<< event_id
		<< "]"
		<< std::endl;
	}

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__loglevel(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "level"
	}};

	const auto dbname
	{
		param.at("dbname")
	};

	auto &database
	{
		db::database::get(dbname)
	};

	if(param.count() == 1)
	{
		out << reflect(loglevel(database)) << std::endl;
		return true;
	}

	const log::level &lev
	{
		log::reflect(param.at("level"))
	};

	loglevel(database, lev);

	out << "set logging level of '" << name(database) << "'"
	    << " database to '" << reflect(lev) << "'"
	    << std::endl;

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__flush(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "[sync]"
	}};

	const auto dbname
	{
		param.at(0)
	};

	const auto sync
	{
		param.at(1, false)
	};

	auto &database
	{
		db::database::get(dbname)
	};

	flush(database, sync);
	out << "done" << std::endl;
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__sort(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column", "[blocking]", "[now]"
	}};

	const auto dbname
	{
		param.at(0)
	};

	const auto colname
	{
		param.at("column", "*"_sv)
	};

	const auto blocking
	{
		param.at("[blocking]", true)
	};

	const auto now
	{
		param.at("[now]", true)
	};

	auto &database
	{
		db::database::get(dbname)
	};

	if(colname == "*")
	{
		db::sort(database, blocking, now);
		out << "done" << std::endl;
		return true;
	}

	db::column column
	{
		database, colname
	};

	db::sort(column, blocking, now);
	out << "done" << std::endl;
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__compact(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "[colname]", "[begin]", "[end]", "[level]"
	}};

	const auto dbname
	{
		param.at(0)
	};

	const auto colname
	{
		param[1]
	};

	const auto begin
	{
		param[2]
	};

	const auto end
	{
		param[3]
	};

	const auto level
	{
		param.at(4, -1)
	};

	auto &database
	{
		db::database::get(dbname)
	};

	if(!colname)
	{
		compact(database);
		out << "done" << std::endl;
		return true;
	}

	const bool integer
	{
		begin? lex_castable<uint64_t>(begin) : false
	};

	const uint64_t integers[2]
	{
		integer? lex_cast<uint64_t>(begin) : 0,
		integer && end? lex_cast<uint64_t>(end) : 0
	};

	const std::pair<string_view, string_view> range
	{
		integer?
			byte_view<string_view>(integers[0]):
		begin == "*"?
			string_view{}:
			begin,

		integer && end?
			byte_view<string_view>(integers[1]):
		end == "*"?
			string_view{}:
			end,
	};

	const auto compact_column{[&out, &database, &level, &range]
	(const string_view &colname)
	{
		db::column column
		{
			database, colname
		};

		compact(column, range, level);
	}};

	if(colname != "*")
		compact_column(colname);
	else
		for(const auto &column : database.columns)
			compact_column(name(*column));

	out << "done" << std::endl;
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__compact__files(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "[colname]", "[srclevel]", "[dstlevel]"
	}};

	const auto dbname
	{
		param.at(0)
	};

	const auto colname
	{
		param[1]
	};

	const std::pair<int, int> level
	{
		param.at(2, -1),
		param.at(3, -1)
	};

	auto &database
	{
		db::database::get(dbname)
	};

	if(!colname)
	{
		compact(database, level);
		out << "done" << std::endl;
		return true;
	}

	const auto compact_column{[&out, &database, &level]
	(const string_view &colname)
	{
		db::column column
		{
			database, colname
		};

		compact(column, level);
	}};

	if(colname != "*")
		compact_column(colname);
	else
		for(const auto &column : database.columns)
			compact_column(name(*column));

	out << "done" << std::endl;
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__resume(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname",
	}};

	const auto dbname
	{
		param.at("dbname")
	};

	auto &database
	{
		db::database::get(dbname)
	};

	resume(database);
	out << "resumed database " << dbname << std::endl;
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__errors(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname",
	}};

	const auto dbname
	{
		param.at("dbname")
	};

	auto &database
	{
		db::database::get(dbname)
	};

	const auto &errors
	{
		db::errors(database)
	};

	size_t i(0);
	for(const auto &error : errors)
		out << std::setw(2) << std::left << (i++) << ':' << error << std::endl;

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__ticker(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "[ticker]"
	}};

	const auto dbname
	{
		param.at(0)
	};

	const auto ticker
	{
		param[1]
	};

	auto &database
	{
		db::database::get(dbname)
	};

	// Special branch for integer properties that RocksDB aggregates.
	if(ticker && ticker != "-a")
	{
		out << ticker << ": " << db::ticker(database, ticker) << std::endl;
		return true;
	}

	for(uint32_t i(0); i < db::ticker_max; ++i)
	{
		const string_view &name
		{
			db::ticker_id(i)
		};

		if(!name)
			continue;

		const auto &val
		{
			db::ticker(database, i)
		};

		if(val == 0 && ticker != "-a")
			continue;

		char buf[48];
		out << std::left << std::setw(48) << std::setfill('_') << name << " ";
		if(has(name, ".bytes"))
			out << pretty(buf, iec(val));
		else
			out << val;

		out << std::endl;
	}

	for(uint32_t i(0); i < db::histogram_max; ++i)
	{
		const string_view &name
		{
			db::histogram_id(i)
		};

		if(!name)
			continue;

		const auto &val
		{
			db::histogram(database, i)
		};

		if(!(val.max > 0.0) && ticker != "-a")
			continue;

		out << std::left << std::setw(48) << std::setfill('_') << name
		    << std::setfill(' ') << std::right
		    << " " << std::setw(10) << val.hits << " hit "
		    << " " << std::setw(13) << val.time << " tot "
		    << " " << std::setw(12) << uint64_t(val.max) << " max "
		    << " " << std::setw(10) << uint64_t(val.median) << " med "
		    << " " << std::setw(9) << uint64_t(val.avg) << " avg "
		    << " " << std::setw(10) << val.stddev << " dev "
		    << " " << std::setw(10) << val.pct95 << " p95 "
		    << " " << std::setw(10) << val.pct99 << " p99 "
		    << std::endl;
	}

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__io(opt &out, const string_view &line)
{
	const auto &ic
	{
		db::iostats_current()
	};

	out << db::string(ic) << std::endl;
	return true;
}

bool
console_cmd__db__perf(opt &out, const string_view &line)
{
	const auto &pc
	{
		db::perf_current()
	};

	out << db::string(pc) << std::endl;
	return true;
}

bool
console_cmd__db__perf__level(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"[level]"
	}};

	if(!param.count())
	{
		const auto &level
		{
			db::perf_level()
		};

		out << "Current level is: " << level << std::endl;
		return true;
	}

	const auto &level
	{
		param.at<uint>(0)
	};

	db::perf_level(level);
	out << "Set level to: " << level << std::endl;
	return true;
}

bool
console_cmd__db__prop(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column", "property"
	}};

	const auto dbname
	{
		param.at(0)
	};

	const auto colname
	{
		param.at(1, "*"_sv)
	};

	const auto property
	{
		param.at(2)
	};

	auto &database
	{
		db::database::get(dbname)
	};

	// Special branch for integer properties that RocksDB aggregates.
	if(colname == "*")
	{
		const uint64_t value
		{
			db::property(database, property)
		};

		out << value << std::endl;
		return true;
	}

	const auto query{[&out, &database, &property]
	(const string_view &colname)
	{
		const db::column column
		{
			database, colname
		};

		const auto value
		{
			db::property<db::prop_map>(column, property)
		};

		for(const auto &p : value)
			out << p.first << " : " << p.second << std::endl;
	}};

	// Branch for querying the property for a single column
	if(colname != "**")
	{
		query(colname);
		return true;
	}

	// Querying the property for all columns in a loop
	for(const auto &column : database.columns)
	{
		out << std::setw(16) << std::right << name(*column) << " : ";
		query(name(*column));
	}

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__cache(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column",
	}};

	const auto dbname
	{
		param.at(0)
	};

	auto colname
	{
		param[1]
	};

	auto &database
	{
		db::database::get(dbname)
	};

	struct stats
	{
		size_t count;
		size_t usage;
		size_t pinned;
		size_t capacity;
		size_t hits;
		size_t misses;
		size_t inserts;
		size_t inserts_bytes;

		stats &operator+=(const stats &b)
		{
			count += b.count;
			usage += b.usage;
			pinned += b.pinned;
			capacity += b.capacity;
			hits += b.hits;
			misses += b.misses;
			inserts += b.inserts;
			inserts_bytes += b.inserts_bytes;
			return *this;
		}
	};

	if(!colname)
	{
		const auto count(db::count(cache(database)));
		const auto usage(db::usage(cache(database)));
		const auto pinned(db::pinned(cache(database)));
		const auto capacity(db::capacity(cache(database)));
		const auto util_pct
		{
			capacity > 0.0? (double(usage) / double(capacity)) : 0.0L
		};

		const auto hits(db::ticker(cache(database), db::ticker_id("rocksdb.block.cache.hit")));
		const auto misses(db::ticker(cache(database), db::ticker_id("rocksdb.block.cache.miss")));
		const auto hit_pct
		{
			(misses + hits) > 0? (double(hits) / double(hits + misses)) : 0.0L
		};

		const auto inserts(db::ticker(cache(database), db::ticker_id("rocksdb.block.cache.add")));
		const auto inserts_bytes(db::ticker(cache(database), db::ticker_id("rocksdb.block.cache.data.bytes.insert")));
		const auto ins_miss_pct
		{
			misses > 0.0? (double(inserts) / double(misses)) : 0.0L
		};

		const auto ins_hit_rat
		{
			inserts > 0.0? (double(hits) / double(inserts)) : 0.0L
		};

		const auto ins_cnt_rat
		{
			count > 0.0? (double(inserts) / double(count)) : 0.0L
		};

		out << std::left
		    << std::setw(24) << "ROW"
		    << std::right
		    << " "
		    << std::setw(26) << "CACHED"
		    << " "
		    << std::setw(26) << "CAPACITY"
		    << " "
		    << std::setw(9) << "UTIL%"
		    << "  "
		    << std::setw(11) << "HITS"
		    << " "
		    << std::setw(10) << "MISSES"
		    << " "
		    << std::setw(9) << "HIT%"
		    << "  "
		    << std::setw(26) << "INSERT TOTAL"
		    << " "
		    << std::setw(10) << "INSERT"
		    << " "
		    << std::setw(10) << "HIT:INS"
		    << "  "
		    << std::setw(8) << "COUNT"
		    << " "
		    << std::setw(10) << "INS:CNT"
		    << "  "
		    << std::setw(20) << "LOCKED"
		    << " "
		    << std::endl;

		out << std::left
		    << std::setw(24) << "*"
		    << std::right
		    << " "
		    << std::setw(26) << std::right << pretty(iec(usage))
		    << " "
		    << std::setw(26) << std::right << pretty(iec(capacity))
		    << " "
		    << std::setw(8) << std::right << std::fixed << std::setprecision(2) << (util_pct * 100)
		    << "%"
		    << "  "
		    << std::setw(11) << hits
		    << " "
		    << std::setw(10) << misses
		    << " "
		    << std::setw(8) << std::right << std::fixed << std::setprecision(2) << (hit_pct * 100)
		    << "%"
		    << "  "
		    << std::setw(26) << std::right << pretty(iec(inserts_bytes))
		    << " "
		    << std::setw(10) << inserts
		    << " "
		    << std::setw(8) << std::right << std::fixed << std::setprecision(0) << ins_hit_rat
		    << ":1"
		    << "  "
		    << std::setw(8) << std::right << count
		    << " "
		    << std::setw(8) << std::right << std::fixed << std::setprecision(0) << ins_cnt_rat
		    << ":1"
		    << "  "
		    << std::setw(20) << std::right << pretty(iec(pinned))
		    << " "
		    << std::endl
		    << std::endl;

		// Now set the colname to * so the column total branch is taken
		// below and we output that line too.
		colname = "*";
	}

	out << std::left
	    << std::setw(24) << "COLUMN"
	    << std::right
	    << " "
	    << std::setw(26) << "CACHED"
	    << " "
	    << std::setw(26) << "CAPACITY"
	    << " "
	    << std::setw(9) << "UTIL%"
	    << "  "
	    << std::setw(11) << "HITS"
	    << " "
	    << std::setw(10) << "MISSES"
	    << " "
	    << std::setw(9) << "HIT%"
	    << "  "
	    << std::setw(26) << "INSERT TOTAL"
		<< " "
	    << std::setw(10) << "INSERT"
	    << " "
	    << std::setw(10) << "HIT:INS"
	    << "  "
	    << std::setw(8) << "COUNT"
	    << " "
	    << std::setw(10) << "INS:CNT"
	    << "  "
	    << std::setw(20) << "LOCKED"
	    << " "
	    << std::endl;

	const auto output{[&out]
	(const string_view &column_name, const stats &s)
	{
		const auto util_pct
		{
			s.capacity > 0.0? (double(s.usage) / double(s.capacity)) : 0.0L
		};

		const auto hit_pct
		{
			(s.misses + s.hits) > 0.0? (double(s.hits) / double(s.hits + s.misses)) : 0.0L
		};

		const auto ins_miss_pct
		{
			s.misses > 0.0? (double(s.inserts) / double(s.misses)) : 0.0L
		};

		const auto ins_hit_rat
		{
			s.inserts > 0.0? (double(s.hits) / double(s.inserts)) : 0.0L
		};

		const auto ins_cnt_rat
		{
			s.count > 0.0? (double(s.inserts) / double(s.count)) : 0.0L
		};

		out << std::setw(24) << std::left << column_name
		    << std::right
		    << " "
		    << std::setw(26) << std::right << pretty(iec(s.usage))
		    << " "
		    << std::setw(26) << std::right << pretty(iec(s.capacity))
		    << " "
		    << std::setw(8) << std::right << std::fixed << std::setprecision(2) << (util_pct * 100)
		    << '%'
		    << "  "
		    << std::setw(11) << s.hits
		    << " "
		    << std::setw(10) << s.misses
		    << " "
		    << std::setw(8) << std::right << std::fixed << std::setprecision(2) << (hit_pct * 100)
		    << '%'
		    << "  "
		    << std::setw(26) << pretty(iec(s.inserts_bytes))
		    << " "
		    << std::setw(10) << s.inserts
		    << " "
		    << std::setw(8) << std::right << std::fixed << std::setprecision(0) << ins_hit_rat
		    << ":1"
		    << "  "
		    << std::setw(8) << std::right << s.count
		    << " "
		    << std::setw(8) << std::right << std::fixed << std::setprecision(0) << ins_cnt_rat
		    << ":1"
		    << "  "
		    << std::setw(20) << std::right << pretty(iec(s.pinned))
		    << " "
		    << std::endl;
	}};

	const auto totals{[&output]
	(const string_view &colname, const stats &uncompressed, const stats &compressed)
	{
		if(uncompressed.capacity)
			output(colname, uncompressed);

		if(compressed.capacity)
		{
			thread_local char buf[64];
			const fmt::sprintf rename
			{
				buf, "%s (compressed)", colname
			};

			output(rename, compressed);
		}
	}};

	const auto query{[&database, &totals]
	(const string_view &colname, const auto &output)
	{
		const db::column column
		{
			database, colname
		};

		const stats uncompressed
		{
			db::count(cache(column)),
			db::usage(cache(column)),
			db::pinned(cache(column)),
			db::capacity(cache(column)),
			db::ticker(cache(column), db::ticker_id("rocksdb.block.cache.hit")),
			db::ticker(cache(column), db::ticker_id("rocksdb.block.cache.miss")),
			db::ticker(cache(column), db::ticker_id("rocksdb.block.cache.add")),
			db::ticker(cache(column), db::ticker_id("rocksdb.block.cache.data.bytes.insert")),
		};

		const stats compressed
		{
			db::count(cache_compressed(column)),
			db::usage(cache_compressed(column)),
			0,
			db::capacity(cache_compressed(column)),
			db::ticker(cache_compressed(column), db::ticker_id("rocksdb.block.cache.hit")),
			0,
			db::ticker(cache_compressed(column), db::ticker_id("rocksdb.block.cache.add")),
			0
		};

		output(colname, uncompressed, compressed);
	}};

	// Querying the totals for all caches for all columns in a loop
	if(colname == "*")
	{
		stats s_total{0}, comp_total{0};
		for(const auto &column : database.columns)
			query(name(*column), [&](const auto &column, const auto &s, const auto &comp)
			{
				s_total += s;
				comp_total += comp;
			});

		totals("*", s_total, comp_total);
		return true;
	}

	// Query the cache for a single column
	if(colname != "**")
	{
		query(colname, totals);
		return true;
	}

	// Querying the cache for all columns in a loop
	for(const auto &column : database.columns)
		query(name(*column), totals);

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__cache__clear(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column", "[key]"
	}};

	const auto &dbname
	{
		param.at(0)
	};

	const auto &colname
	{
		param[1]
	};

	const auto &key
	{
		param[2]
	};

	auto &database
	{
		db::database::get(dbname)
	};

	const auto clear{[&out, &database]
	(const string_view &colname)
	{
		db::column column
		{
			database, colname
		};

		db::clear(cache(column));
		db::clear(cache_compressed(column));
		out << "Cleared caches for '" << name(database) << "' '" << colname << "'"
		    << std::endl;
	}};

	const auto remove{[&out, &database]
	(const string_view &colname, const string_view &key)
	{
		db::column column
		{
			database, colname
		};

		const bool removed[]
		{
			db::remove(cache(column), key),
			db::remove(cache_compressed(column), key)
		};

		out << "Removed key from";
		if(removed[0])
			out << " [uncompressed cache]";

		if(removed[1])
			out << " [compressed cache]";

		out << std::endl;
	}};

	if(!colname || colname == "**")
	{
		for(const auto &column : database.columns)
			clear(name(*column));

		return true;
	}

	if(!key)
	{
		clear(colname);
		return true;
	}

	remove(colname, key);
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__cache__fetch(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column", "key"
	}};

	const auto dbname
	{
		param.at(0)
	};

	const auto colname
	{
		param[1]
	};

	const auto key
	{
		param[2]
	};

	auto &database
	{
		db::database::get(dbname)
	};

	db::column column
	{
		database, colname
	};

	db::has(column, key);
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__cache__each(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column", "limit"
	}};

	const auto dbname
	{
		param.at(0)
	};

	auto colname
	{
		param[1]
	};

	const auto limit
	{
		param.at("limit", 32UL)
	};

	auto &database
	{
		db::database::get(dbname)
	};

	if(!colname)
	{
		out << "No column specified."
		    << std::endl;

		return true;
	}

	const db::column column
	{
		database, colname
	};

	size_t i(0);
	db::for_each(db::cache(column), [&]
	(const const_buffer &value)
	{
		out
		<< std::right << std::setw(4) << i
		<< ' ' << std::right << std::setw(8) << size(value)
		<< ' ' << std::left << std::setw(15)
		<< std::endl;

		return i++ < limit;
	});

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__stats(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"dbname", "column"
	}};

	const fmt::snstringf new_line
	{
		1024, "%s %s rocksdb.stats",
		param.at(0),
		param.at(1)
	};

	return console_cmd__db__prop(out, new_line);
}

bool
console_cmd__db__set(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column", "option", "value"
	}};

	const auto dbname
	{
		param.at(0)
	};

	const auto colname
	{
		param.at(1, "*"_sv)
	};

	const auto option
	{
		param.at(2)
	};

	const auto value
	{
		param.at(3)
	};

	auto &database
	{
		db::database::get(dbname)
	};

	// Special branch for DBOptions
	if(colname == "*")
	{
		db::setopt(database, option, value);
		out << "done" << std::endl;
		return true;
	}

	const auto setopt{[&out, &database, &option, &value]
	(const string_view &colname)
	{
		db::column column
		{
			database, colname
		};

		db::setopt(column, option, value);
		out << colname << " :done" << std::endl;
	}};

	// Branch for querying the property for a single column
	if(colname != "**")
	{
		setopt(colname);
		return true;
	}

	// Querying the property for all columns in a loop
	for(const auto &column : database.columns)
		setopt(name(*column));

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__ingest(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column", "path"
	}};

	const auto dbname
	{
		param.at("dbname")
	};

	const auto colname
	{
		param.at("column")
	};

	const auto path
	{
		param.at("path")
	};

	auto &database
	{
		db::database::get(dbname)
	};

	db::column column
	{
		database, colname
	};

	db::ingest(column, path);
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

static void
_print_sst_info_header(opt &out)
{
	out << std::left << std::setfill(' ')
	    << std::setw(12) << "name"
	    << "  " << std::setw(32) << "creation"
	    << "  " << std::setw(3) << "flt"
	    << std::right
	    << "  " << std::setw(7) << "pressed"
	    << std::left
	    << "  " << std::setw(5) << "press"
	    << "  " << std::setw(24) << "file size"
	    << "  " << std::setw(23) << "sequence number"
	    << "  " << std::setw(23) << "key range"
	    << std::right
	    << "  " << std::setw(10) << "reads"
	    << "  " << std::setw(10) << "entries"
	    << "  " << std::setw(10) << "blocks"
	    << "  " << std::setw(7) << "idxs"
	    << "  " << std::setw(3) << "lev"
	    << std::left
	    << "  " << std::setw(20) << "column"
	    << std::endl;
}

static void
_print_sst_info(opt &out,
                const db::database::sst::info &f)
{
	const uint64_t &min_key
	{
		f.min_key.size() == 8?
			uint64_t(byte_view<uint64_t>(f.min_key)):
			0UL
	};

	const uint64_t &max_key
	{
		f.max_key.size() == 8?
			uint64_t(byte_view<uint64_t>(f.max_key)):
			0UL
	};

	char tmbuf[64], pbuf[48];
	out << std::left << std::setfill(' ')
	    << std::setw(12) << f.name
	    << "  " << std::setw(32) << std::left << (f.created? timef(tmbuf, f.created, ircd::localtime) : string_view{})
	    << "  " << std::setw(1) << std::left << (!f.filter.empty()? 'F' : '-')
	    <<         std::setw(1) << std::left << (f.delta_encoding? 'D' : '-')
	    <<         std::setw(1) << std::left << (true? '-' : '-')
	    << "  " << std::setw(6) << std::right << std::fixed << std::setprecision(2) << f.compression_pct << '%'
	    << "  " << std::setw(5) << std::left << trunc(f.compression, 5)
	    << "  " << std::setw(24) << std::left << pretty(pbuf, iec(f.size))
	    ;

	if(f.min_seq)
		out << "  " << std::setw(10) << std::right << f.min_seq << " : " << std::setw(10) << std::left << f.max_seq;
	else
		out << "  " << std::setw(10) << std::right << ' ' << "   " << std::setw(10) << std::left << "<sorted>";

	if(min_key)
		out << "  " << std::setw(10) << std::right << min_key << " : " << std::setw(10) << std::left << max_key;
	else
		out << "  " << std::setw(10) << std::right << ' ' << "   " << std::setw(10) << std::left << "<string>";

	out << "  " << std::setw(10) << std::right << f.num_reads
	    << "  " << std::setw(10) << std::right << f.entries
	    << "  " << std::setw(10) << std::right << f.data_blocks
	    << "  " << std::setw(7) << std::right << f.index_parts
	    << "  " << std::setw(3) << std::right << f.level
	    << "  " << std::setw(20) << std::left << f.column
	    << std::endl;
}

static void
_print_sst_info_full(opt &out,
                     const db::database::sst::info &f)
{
	const uint64_t &min_key
	{
		f.min_key.size() == 8?
			uint64_t(byte_view<uint64_t>(f.min_key)):
			0UL
	};

	const uint64_t &max_key
	{
		f.max_key.size() == 8?
			uint64_t(byte_view<uint64_t>(f.max_key)):
			0UL
	};

	const auto closeout{[&out]
	(const string_view &name, const auto &closure)
	{
		out << std::left << std::setw(40) << std::setfill('_') << name << " ";
		closure(out);
		out << std::endl;
	}};

	const auto close_auto{[&closeout]
	(const string_view &name, const auto &value)
	{
		closeout(name, [&value](opt &out)
		{
			out << value;
		});
	}};

	const auto close_size{[&closeout]
	(const string_view &name, const size_t &value)
	{
		closeout(name, [&value](opt &out)
		{
			out << pretty(iec(value));
		});
	}};

	close_auto("name", f.name);
	close_auto("directory", f.path);
	close_auto("format", f.format);
	close_auto("version", f.version);
	close_auto("creation", timestr(f.created, ircd::localtime));
	close_auto("checksum function", f.checksum_func);
	close_auto("checksum value", f.checksum);
	close_auto("column ID", f.cfid);
	close_auto("column", f.column);
	close_auto("column comparator", f.comparator);
	close_auto("column merge operator", f.merge_operator);
	close_auto("column prefix extractor", f.prefix_extractor);
	close_auto("level", f.level);
	close_auto("lowest sequence", f.min_seq);
	close_auto("highest sequence", f.max_seq);
	close_auto("lowest key", min_key);
	close_auto("highest key", max_key);
	close_auto("fixed key length", f.fixed_key_len);
	close_auto("delta encode", f.delta_encoding? "yes"_sv : "no"_sv);
	close_auto("compression", f.compression);
	close_auto("compacting", f.compacting? "yes"_sv : "no"_sv);
	close_auto("range deletes", f.range_deletes);
	close_auto("", "");

	close_size("file phys size", f.size);
	close_size("file virt size", f.file_size);
	close_auto("file compress percent", f.compression_pct);
	close_auto("", "");

	close_size("file head phys size", f.meta_size);
	close_size("file head virt size", f.head_size);
	close_auto("file head compress percent", 100 - 100.0L * (f.meta_size / (long double)f.head_size));
	close_auto("", "");

	close_size("index size", f.index_size);
	close_size("index head size", f.index_root_size);
	close_size("index data size", f.index_data_size);
	close_auto("index data blocks", f.index_parts);
	close_size("index data block average size", f.index_data_size / double(f.index_parts));
	close_size("index data average per-key", f.index_data_size / double(f.entries));
	close_size("index data average per-block", f.index_data_size / double(f.data_blocks));
	close_auto("index head percent of index", 100.0 * (f.index_root_size / double(f.index_data_size)));
	close_auto("index data percent of keys", 100.0 * (f.index_data_size / double(f.keys_size)));
	close_auto("index data percent of values", 100.0 * (f.index_data_size / double(f.values_size)));
	close_auto("index data percent of data", 100.0 * (f.index_data_size / double(f.data_size)));
	close_auto("index data compress percent", f.index_compression_pct);
	close_auto("", "");

	if(!f.filter.empty())
	{
		close_auto("filter", f.filter);
		close_size("filter size", f.filter_size);
		close_auto("filter average per-key", f.filter_size / double(f.entries));
		close_auto("filter average per-block", f.filter_size / double(f.data_blocks));
		close_auto("", "");
	}

	close_auto("blocks", f.data_blocks);
	close_size("blocks phys size", f.data_size);
	close_size("blocks phys average size", f.data_size / double(f.data_blocks));
	close_size("blocks virt size", f.blocks_size);
	close_size("blocks virt average size", f.blocks_size / double(f.data_blocks));
	close_auto("blocks compress percent", f.blocks_compression_pct);
	close_auto("", "");

	close_auto("keys", f.entries);
	close_size("keys virt size", f.keys_size);
	close_size("keys virt average size", f.keys_size / double(f.entries));
	close_auto("keys virt percent of blocks", 100.0 * (f.keys_size / double(f.blocks_size)));
	close_auto("", "");

	close_auto("values", f.entries);
	close_size("values virt size", f.values_size);
	close_size("values virt average size", f.values_size / double(f.entries));
	close_auto("values virt average per-index", f.entries / double(f.index_parts));
	close_auto("values virt average per-block", f.entries / double(f.data_blocks));
	close_auto("", "");
}

bool
console_cmd__db__sst(opt &out, const string_view &line)
{
	string_view buf[16];
	const vector_view<const string_view> args
	{
		buf, tokens(line, " ", buf)
	};

	db::database::sst::tool(args);
	return true;
}

bool
console_cmd__db__sst__dump(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"dbname", "column", "begin", "end", "path"
	}};

	const auto dbname
	{
		param.at("dbname")
	};

	const auto colname
	{
		param.at("column", "*"_sv)
	};

	const auto begin
	{
		param["begin"]
	};

	const auto end
	{
		param["end"]
	};

	const auto path
	{
		param["path"]
	};

	auto &database
	{
		db::database::get(dbname)
	};

	_print_sst_info_header(out);

	const auto do_dump{[&](const string_view &colname)
	{
		db::column column
		{
			database, colname
		};

		const db::database::sst::dump dump
		{
			column, {begin, end}, path
		};

		_print_sst_info(out, dump.info);
	}};

	if(colname != "*")
	{
		do_dump(colname);
		return true;
	}

	for(const auto &column : database.columns)
		do_dump(name(*column));

	return true;
}

bool
console_cmd__db__wal(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname",
	}};

	const auto dbname
	{
		param.at("dbname")
	};

	auto &database
	{
		db::database::get(dbname)
	};

	const db::database::wal::info::vector vec
	{
		database
	};

	out
	<< std::setw(12) << std::left << "PATH" << "  "
	<< std::setw(8) << std::left << "ID" << "  "
	<< std::setw(12) << std::right << "START SEQ" << "  "
	<< std::setw(20) << std::left << "SIZE" << "  "
	<< std::setw(8) << std::left << "STATUS" << "  "
	<< std::endl;

	for(const auto &info : vec)
		out
		<< std::setw(12) << std::left << info.name << "  "
		<< std::setw(8) << std::left << info.number << "  "
		<< std::setw(12) << std::right << info.seq << "  "
		<< std::setw(20) << std::left << pretty(iec(info.size)) << "  "
		<< std::setw(8) << std::left << (info.alive? "LIVE"_sv : "ARCHIVE"_sv) << "  "
		<< std::endl;

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__files(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column"
	}};

	const auto dbname
	{
		param.at("dbname")
	};

	const auto colname
	{
		param.at("column", "*"_sv)
	};

	auto &database
	{
		db::database::get(dbname)
	};

	const auto _print_totals{[&out]
	(const auto &vector)
	{
		db::database::sst::info total;
		total.name = "total"s;
		for(const auto &info : vector)
		{
			total.size += info.size;
			total.data_size += info.data_size;
			total.index_data_size += info.index_data_size;
			total.index_root_size += info.index_root_size;
			total.filter_size += info.filter_size;
			total.keys_size += info.keys_size;
			total.values_size += info.values_size;
			total.index_parts += info.index_parts;
			total.data_blocks += info.data_blocks;
			total.entries += info.entries;
			total.range_deletes += info.range_deletes;
			total.num_reads += info.num_reads;
		}

		_print_sst_info_header(out);
		_print_sst_info(out, total);
		out << "--- " << vector.size() << " files." << std::endl;
	}};

	if(colname == "*")
	{
		db::database::sst::info::vector vector
		{
			database
		};

		std::sort(begin(vector), end(vector), []
		(const auto &a, const auto &b)
		{
			return a.created < b.created;
		});

		_print_sst_info_header(out);
		for(const auto &fileinfo : vector)
			_print_sst_info(out, fileinfo);

		out << std::endl;
		_print_totals(vector);
		return true;
	}

	if(startswith(colname, "/"))
	{
		const db::database::sst::info info{database, colname};
		_print_sst_info_full(out, info);
		return true;
	}

	const db::column column
	{
		database, colname
	};

	db::database::sst::info::vector vector
	{
		column
	};

	std::sort(begin(vector), end(vector), []
	(const auto &a, const auto &b)
	{
		return a.created < b.created;
	});

	_print_sst_info_header(out);
	for(const auto &info : vector)
		_print_sst_info(out, info);

	out << std::endl;
	_print_totals(vector);
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__bytes(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column", "key"
	}};

	auto &database
	{
		db::database::get(param.at(0))
	};

	if(!param["column"] || param["column"] == "*")
	{
		const auto bytes
		{
			db::bytes(database)
		};

		out << bytes << std::endl;
		return true;
	}

	if(param["key"])
	{
		db::column column
		{
			database, param["column"]
		};

		const bool is_integer_key
		{
			lex_castable<ulong>(param["key"])
		};

		const uint64_t integer_key[2]
		{
			is_integer_key?
				lex_cast<ulong>(param["key"]):
				0UL,

			integer_key[0] + 1
		};

		const string_view key[2]
		{
			is_integer_key?
				byte_view<string_view>{integer_key[0]}:
				param["key"],

			is_integer_key?
				byte_view<string_view>{integer_key[1]}:
				param["key"]
		};

		const auto value
		{
			db::bytes_value(column, key[0])
		};

		const auto value_compressed
		{
			db::bytes(column, {key[0], key[1]})
		};

		out << param["column"]
		    << (is_integer_key? "[(binary)" : "[") << param["key"] << "] "
		    << ": " << value << " (uncompressed value)"
		    << std::endl;

		out << param["column"]
		    << (is_integer_key? "[(binary)" : "[") << param["key"] << "] "
		    << ": " << value_compressed
		    << std::endl;

		return true;
	}

	const auto query{[&out, &database]
	(const string_view &colname)
	{
		const db::column column
		{
			database, colname
		};

		const auto value
		{
			db::bytes(column)
		};

		out << std::setw(16) << std::right << colname
		    << " : " << value
		    << std::endl;
	}};

	if(param["column"] == "**")
	{
		for(const auto &column : database.columns)
			query(name(*column));

		return true;
	}

	query(param["column"]);
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__txns(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "seqnum", "limit"
	}};

	const auto dbname
	{
		param.at("dbname")
	};

	if(dbname != "events")
		throw error
		{
			"Sorry, this command is specific to the events db for now."
		};

	auto &database
	{
		db::database::get(dbname)
	};

	const auto cur_seq
	{
		db::sequence(database)
	};

	const auto seqnum
	{
		param.at<int64_t>("seqnum", cur_seq)
	};

	const auto limit
	{
		param.at<int64_t>("limit", 32L)
	};

	// note that we decrement the sequence number here optimistically
	// based on the number of possible entries in a txn. There are likely
	// fewer entries in a txn thus we will be missing the latest txns or
	// outputting more txns than the limit. We choose the latter here.
	const auto start
	{
		std::max(seqnum - limit * ssize_t(database.columns.size()), 0L)
	};

	out << std::setw(12) << std::left << "SEQUENCE"
	    << "  "
	    << std::setw(6) << std::left << "DELTAS"
	    << "  "
	    << std::setw(18) << std::left << "SIZE"
	    << " : "
	    << std::endl;

	for_each(database, start, db::seq_closure_bool{[&out, &seqnum]
	(db::txn &txn, const int64_t &_seqnum) -> bool
	{
		m::event::id::buf event_id;
		txn.get(db::op::SET, "event_id", [&event_id]
		(const db::delta &delta)
		{
			event_id = m::event::id
			{
				std::get<db::delta::VAL>(delta)
			};
		});

		if(!event_id)
			return true;

		thread_local char iecbuf[48];
		out << std::setw(12) << std::left << _seqnum
		    << "  "
		    << std::setw(6) << std::left << txn.size()
		    << "  "
		    << std::setw(18) << std::left << pretty(iecbuf, iec(txn.bytes()))
		    << " : "
		    << string_view{event_id}
		    << std::endl;

		return _seqnum <= seqnum;
	}});

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__txn(opt &out, const string_view &line)
try
{
	const auto dbname
	{
		token(line, ' ', 0)
	};

	if(dbname != "events")
		throw error
		{
			"Sorry, this command is specific to the events db for now."
		};

	const auto seqnum
	{
		lex_cast<uint64_t>(token(line, ' ', 1, "0"))
	};

	auto &database
	{
		db::database::get(dbname)
	};

	get(database, seqnum, db::seq_closure{[&out]
	(db::txn &txn, const uint64_t &seqnum)
	{
		for_each(txn, [&out, &seqnum]
		(const db::delta &delta)
		{
			const string_view &dkey
			{
				std::get<db::delta::KEY>(delta)
			};

			// !!! Assumption based on the events database schema. If the
			// key is 8 bytes we assume it's an event::idx in binary. No
			// other columns have 8 byte keys; instead they have plaintext
			// event_id amalgams with some binary characters which are simply
			// not displayed by the ostream. We could have a switch here to
			// use m::dbs's key parsers based on the column name but that is
			// not done here yet.
			const string_view &key
			{
				dkey.size() == 8?
					lex_cast(uint64_t(byte_view<uint64_t>(dkey))):
					dkey
			};

			out << std::setw(12)  << std::right  << seqnum << " : "
			    << std::setw(8)   << std::left   << reflect(std::get<db::delta::OP>(delta)) << " "
			    << std::setw(18)  << std::right  << std::get<db::delta::COL>(delta) << " "
			    << key
			    << std::endl;
		});
	}});

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__checkpoint(opt &out, const string_view &line)
try
{
	const auto dbname
	{
		token(line, ' ', 0)
	};

	auto &database
	{
		db::database::get(dbname)
	};

	const auto seqnum
	{
		checkpoint(database)
	};

	out << "Checkpoint " << name(database)
	    << " at sequence " << seqnum << " complete."
	    << std::endl;

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__check(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column"
	}};

	const auto &dbname
	{
		param.at("dbname")
	};

	const auto &colname
	{
		param["column"]
	};

	auto &database
	{
		db::database::get(dbname)
	};

	if(colname)
	{
		db::column column
		{
			database[colname]
		};

		check(column);
		out << "Check of " << colname << " in " << dbname << " completed without error."
		    << std::endl;

		return true;
	}

	check(database);
	out << "Check of " << dbname << " completed without error."
	    << std::endl;

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__DROP__DROP__DROP(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column"
	}};

	const auto dbname
	{
		param.at("dbname")
	};

	const auto colname
	{
		param.at("column")
	};

	auto &database
	{
		db::database::get(dbname)
	};

	db::column column
	{
		database, colname
	};

	db::drop(column);

	out << "DROPPED COLUMN " << colname
	    << " FROM DATABASE " << dbname
	    << std::endl;

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__list(opt &out, const string_view &line)
{
	const auto available
	{
		db::available()
	};

	for(const auto &path : available)
	{
		const auto name
		{
			replace(lstrip(lstrip(path, fs::base::db), '/'), "/", ":")
		};

		const auto &d
		{
			db::database::get(std::nothrow, name)
		};

		const auto &light
		{
			d? "\033[1;42m \033[0m" : " "
		};

		out << "[" << light << "]"
		    << " " << name
		    << " `" << path << "'"
		    << std::endl;
	}

	return true;
}

bool
console_cmd__db__opts(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "[column]"
	}};

	auto &d
	{
		db::database::get(param.at("dbname"))
	};

	const db::column c
	{
		param.at("[column]", string_view{})?
			db::column{d, param.at("[column]")}:
			db::column{}
	};

	const db::options::map opts_
	{
		c?
			db::getopt(c):
			db::getopt(d)
	};

	// Remap from the std::unordered_map to a sorted map for better UX.
	const std::map<std::string, std::string> opts
	{
		begin(opts_), end(opts_)
	};

	for(const auto &p : opts)
		out << std::left
		    << std::setw(45) << std::setfill('_') << p.first
		    << " " << p.second
		    << std::endl;

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__columns(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname"
	}};

	auto &d
	{
		db::database::get(param.at("dbname"))
	};

	for(const auto &c : d.columns)
	{
		const db::column &column(*c);
		out << '[' << std::setw(3) << std::right << db::id(column) << ']'
		    << " " << std::setw(18) << std::left << db::name(column)
		    << " " << std::setw(25) << std::right << pretty(iec(bytes(column)))
		    << std::endl;
	}

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__info(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "[column]"
	}};

	auto &d
	{
		db::database::get(param.at("dbname"))
	};

	const db::column c
	{
		param.at("[column]", string_view{})?
			db::column{d, param.at("[column]")}:
			db::column{}
	};

	const auto closeout{[&out]
	(const string_view &name, const auto &closure)
	{
		out << std::left << std::setw(40) << std::setfill('_') << name << " ";
		closure();
		out << std::endl;
	}};

	const auto property{[&out, &d, &c, &closeout]
	(const string_view &prop)
	{
		const auto name(lstrip(prop, "rocksdb."));
		size_t val(0); try
		{
			val = c?
				db::property<db::prop_int>(c, prop):
				db::property<db::prop_int>(d, prop);
		}
		catch(const std::exception &e)
		{
			log::derror{"%s", e.what()};
		}

		if(!!val) closeout(name, [&out, &val]
		{
			out << val;
		});
	}};

	const auto sizeprop{[&out, &d, &c, &closeout]
	(const string_view &prop)
	{
		const auto name(lstrip(prop, "rocksdb."));
		size_t val(0); try
		{
			val = c?
				db::property<db::prop_int>(c, prop):
				db::property<db::prop_int>(d, prop);
		}
		catch(const std::exception &e)
		{
			log::derror{"%s", e.what()};
		}

		if(!!val) closeout(name, [&out, &val]
		{
			out << pretty(iec(val));
		});
	}};

	if(c)
	{
		out << db::describe(c).explain
		    << std::endl;

		closeout("size", [&] { out << pretty(iec(bytes(c))); });
		closeout("files", [&] { out << file_count(c); });
	} else {
		closeout("uuid", [&] { out << uuid(d); });
		closeout("size", [&] { out << pretty(iec(bytes(d))); });
		closeout("columns", [&] { out << d.columns.size(); });
		closeout("files", [&] { out << file_count(d); });
		closeout("sequence", [&] { out << sequence(d); });
	}

	property("rocksdb.estimate-num-keys");
	property("rocksdb.background-errors");
	property("rocksdb.base-level");
	property("rocksdb.num-live-versions");
	property("rocksdb.current-super-version-number");
	property("rocksdb.min-log-number-to-keep");
	property("rocksdb.is-file-deletions-enabled");
	property("rocksdb.is-write-stopped");
	property("rocksdb.actual-delayed-write-rate");
	property("rocksdb.num-entries-active-mem-table");
	property("rocksdb.num-deletes-active-mem-table");
	property("rocksdb.mem-table-flush-pending");
	property("rocksdb.num-running-flushes");
	property("rocksdb.compaction-pending");
	property("rocksdb.num-running-compactions");
	sizeprop("rocksdb.estimate-pending-compaction-bytes");
	property("rocksdb.num-snapshots");
	property("rocksdb.oldest-snapshot-time");
	sizeprop("rocksdb.size-all-mem-tables");
	sizeprop("rocksdb.cur-size-all-mem-tables");
	sizeprop("rocksdb.cur-size-active-mem-table");
	sizeprop("rocksdb.estimate-table-readers-mem");
	sizeprop("rocksdb.block-cache-capacity");
	sizeprop("rocksdb.block-cache-usage");
	sizeprop("rocksdb.block-cache-pinned-usage");
	if(!c)
		closeout("row cache size", [&] { out << pretty(iec(db::usage(cache(d)))); });

	sizeprop("rocksdb.estimate-live-data-size");
	sizeprop("rocksdb.live-sst-files-size");
	sizeprop("rocksdb.total-sst-files-size");

	if(c)
	{
		out << "\n--- files:" << std::endl;
		_print_sst_info_header(out);
		db::database::sst::info::vector vector{c};
		std::sort(begin(vector), end(vector), []
		(const auto &a, const auto &b)
		{
			return a.created < b.created;
		});

		for(const auto &info : vector)
			_print_sst_info(out, info);

		out << std::setfill(' ');
		out << "\n--- caches:" << std::endl;
		console_cmd__db__cache(out, line);
	}
	else
	{
		out
		<< std::endl
		<< std::left << std::setfill (' ') << std::setw(3) << "ID"
		<< " " << std::setw(20) << "NAME"
		<< " " << std::right << std::setw(12) << "KEYS"
		<< "   " << std::left << std::setw(24) << "SIZE (COMPRESSED)"
		<< " :" << "DESCRIPTION" << std::endl;
		for(const auto &column : d.columns)
		{
			const auto explain
			{
				split(db::describe(*column).explain, '\n').first
			};

			const auto &num_keys
			{
				db::property<db::prop_int>(*column, "rocksdb.estimate-num-keys")
			};

			out << std::left << std::setfill (' ') << std::setw(3) << db::id(*column)
			    << " " << std::setw(20) << db::name(*column)
			    << " " << std::right << std::setw(12) << num_keys
			    << "   " << std::left << std::setw(24) << pretty(iec(db::bytes(*column)))
			    << " :" << explain << std::endl;
		}
	}

	if(!c && !errors(d).empty())
	{
		size_t i(0);
		out << std::endl;
		out << "ERRORS (" << errors(d).size() << "): " << std::endl;
		for(const auto &error : errors(d))
			out << std::setw(2) << (i++) << ':' << error << std::endl;
	}

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db(opt &out, const string_view &line)
{
	if(empty(line))
		return console_cmd__db__list(out, line);

	return console_cmd__db__info(out, line);
}

//
// peer
//

static bool
html__peer(opt &out, const string_view &line)
{
	out << "<table>";

	out << "<tr>";
	out << "<td> HOST </td>";
	out << "<td> ADDR </td>";
	out << "<td> LINKS </td>";
	out << "<td> REQS </td>";
	out << "<td> ▲ BYTES Q</td>";
	out << "<td> ▼ BYTES Q</td>";
	out << "<td> ▲ BYTES</td>";
	out << "<td> ▼ BYTES</td>";
	out << "<td> ERROR </td>";
	out << "</tr>";

	for(const auto &p : server::peers)
	{
		using std::setw;
		using std::left;
		using std::right;

		const auto &host{p.first};
		const auto &peer{*p.second};
		const net::ipport &ipp{peer.remote};

		out << "<tr>";

		out << "<td>" << host << "</td>";
		out << "<td>" << ipp << "</td>";

		out << "<td>" << peer.link_count() << "</td>";
		out << "<td>" << peer.tag_count() << "</td>";
		out << "<td>" << peer.write_size() << "</td>";
		out << "<td>" << peer.read_size() << "</td>";
		out << "<td>" << peer.write_total() << "</td>";
		out << "<td>" << peer.read_total() << "</td>";

		out << "<td>";
		if(peer.err_has() && peer.err_msg())
			out << peer.err_msg();
		else if(peer.err_has())
			out << "<unknown error>"_sv;
		out << "</td>";

		out << "</tr>";
	}

	out << "</table>";
	return true;
}

bool
console_cmd__peer(opt &out, const string_view &line)
try
{
	if(out.html)
		return html__peer(out, line);

	const params param{line, " ",
	{
		"[hostport]", "[all]"
	}};

	const auto &hostport
	{
		param[0]
	};

	const auto print_head{[&out]
	{
		out
		<< std::setw(4) << std::left << "ID" << ' '
		<< std::setw(40) << std::right << "ADDRESS" << ' '
		<< std::setw(7) << std::right << "TTL" << ' '
		<< std::setw(50) << std::left << "NAME" << ' '
		<< std::setw(23) << std::left << "READ-TOTAL" << ' '
		<< std::setw(23) << std::left << "WRITE-TOTAL" << ' '
		<< std::setw(8) << std::right << "TOTAL" << ' '
		<< std::setw(5) << std::right << "DONE" << ' '
		<< std::setw(4) << std::right << "TAGS" << ' '
		<< std::setw(4) << std::right << "PIPE" << ' '
		<< std::setw(4) << std::right << "LNKS" << ' '
		<< std::setw(15) << std::left << "FLAGS" << ' '
		<< std::setw(32) << std::left << "ERROR" << ' '
		<< std::endl;
	}};

	const auto print{[&out]
	(const auto &host, const auto &peer)
	{
		const string_view &error
		{
			peer.err_has() && peer.err_msg()?
				peer.err_msg():
			peer.err_has()?
				"<unknown error>"_sv:
				string_view{}
		};

		char flags[16] {0};
		if(peer.op_resolve)  ircd::strlcat(flags, "RESOLVING ");
		if(peer.op_fini)     ircd::strlcat(flags, "FINISHED ");

		char pbuf[32];
		out
		<< std::setw(4) << std::left << peer.id << ' '
		<< std::setw(40) << std::right << net::ipport{peer.remote} << ' '
		<< std::setw(7) << std::right << duration_cast<seconds>(peer.remote_expires - now<system_point>()).count() << ' '
		<< std::setw(50) << std::left << trunc(host, 50) << ' '
		<< std::setw(23) << std::left << pretty(pbuf, iec(peer.read_total())) << ' '
		<< std::setw(23) << std::left << pretty(pbuf, iec(peer.write_total())) << ' '
		<< std::setw(8) << std::right << peer.tag_done << ' '
		<< std::setw(5) << std::right << peer.link_tag_done() << ' '
		<< std::setw(4) << std::right << peer.tag_count() << ' '
		<< std::setw(4) << std::right << peer.tag_committed() << ' '
		<< std::setw(4) << std::right << peer.link_count() << ' '
		<< std::setw(15) << std::left << flags << ' '
		<< std::setw(32) << std::left << error << ' '
		<< std::endl;
	}};

	const bool all(has(line, "all"));
	const bool active(has(line, "active"));
	const bool conn(has(line, "conn"));
	if(hostport && !all && !active && !conn)
	{
		char buf[rfc3986::DOMAIN_BUFSIZE];
		const auto remote
		{
			net::service(net::hostport(hostport)) == "matrix"?
				m::fed::server(buf, hostport):
				m::fed::matrix_service(hostport)
		};

		auto &peer
		{
			server::find(remote)
		};

		print_head();
		print(peer.hostcanon, peer);
		return true;
	}

	print_head();
	for(const auto &p : server::peers)
	{
		const auto &host{p.first};
		const auto &peer{*p.second};
		if(!all && peer.err_has())
			continue;

		if(conn && !peer.link_count())
			continue;

		if(active && !peer.tag_count())
			continue;

		print(host, peer);
	}

	return true;
}
catch(const std::out_of_range &)
{
	throw error
	{
		"Peer not found"
	};
}

bool
console_cmd__peer__count(opt &out, const string_view &line)
{
	size_t i{0};
	for(const auto &pair : ircd::server::peers)
	{
		assert(bool(pair.second));
		const auto &peer{*pair.second};
		if(!peer.err_has())
			++i;
	}

	out << i << std::endl;
	return true;
}

bool
console_cmd__peer__error(opt &out, const string_view &line)
{
	for(const auto &pair : ircd::server::peers)
	{
		using std::setw;
		using std::left;
		using std::right;

		const auto &host{pair.first};
		assert(bool(pair.second));
		const auto &peer{*pair.second};
		if(!peer.err_has())
			continue;

		const net::ipport &ipp{peer.remote};
		out << setw(40) << right << host;

		if(ipp)
		    out << ' ' << setw(40) << left << ipp;
		else
		    out << ' ' << setw(40) << left << " ";

		out << peer.e->etime;

		if(peer.err_msg())
			out << "  :" << peer.err_msg();
		else
			out << "  <unknown error>"_sv;

		out << std::endl;
	}

	return true;
}

bool
console_cmd__peer__error__count(opt &out, const string_view &line)
{
	size_t i{0};
	for(const auto &pair : ircd::server::peers)
	{
		assert(bool(pair.second));
		const auto &peer{*pair.second};
		if(peer.err_has())
			++i;
	}

	out << i << std::endl;
	return true;
}

bool
console_cmd__peer__error__clear__all(opt &out, const string_view &line)
{
	size_t cleared(0);
	for(auto &pair : ircd::server::peers)
	{
		const auto &name{pair.first};
		assert(bool(pair.second));
		auto &peer{*pair.second};
		cleared += peer.err_clear();
	}

	out << "cleared " << cleared
	    << " of " << ircd::server::peers.size()
	    << std::endl;

	return true;
}

bool
console_cmd__peer__error__clear(opt &out, const string_view &line)
{
	if(empty(line))
		return console_cmd__peer__error__clear__all(out, line);

	const string_view &input
	{
		token(line, ' ', 0)
	};

	char buf[rfc3986::DOMAIN_BUFSIZE];
	const net::hostport remote
	{
		service(net::hostport(input)) == "matrix"?
			m::fed::server(buf, input):
			m::fed::matrix_service(input)
	};

	const auto cleared
	{
		server::errclear(remote)
	};

	out << std::boolalpha << cleared << std::endl;
	return true;
}

bool
console_cmd__peer__version(opt &out, const string_view &line)
{
	for(const auto &p : server::peers)
	{
		using std::setw;
		using std::left;
		using std::right;

		const auto &host{p.first};
		const auto &peer{*p.second};
		const net::ipport &ipp{peer.remote};

		out << setw(40) << right << host;

		if(ipp)
		    out << ' ' << setw(40) << left << ipp;
		else
		    out << ' ' << setw(40) << left << " ";

		if(!empty(peer.server_version))
			out << " :" << peer.server_version;

		out << std::endl;
	}

	return true;
}

bool
console_cmd__peer__find(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"ip:port"
	}};

	const auto &ip{rsplit(param.at(0), ':').first};
	const auto &port{rsplit(param.at(0), ':').second};
	const net::ipport ipp{ip, port? port : "0"};

	for(const auto &p : server::peers)
	{
		const auto &hostname{p.first};
		const auto &peer{*p.second};
		const net::ipport &ipp_
		{
			peer.remote
		};

		if(is_v6(ipp) && (!is_v6(ipp_) || host6(ipp) != host6(ipp_)))
			continue;

		if(is_v4(ipp) && (!is_v4(ipp_) || host4(ipp) != host4(ipp_)))
			continue;

		if(net::port(ipp) && net::port(ipp) != net::port(ipp_))
			continue;

		out << hostname << std::endl;
		break;
	}

	return true;
}

bool
console_cmd__peer__cancel(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"hostport"
	}};

	const auto &hp
	{
		param["hostport"]
	};

	char buf[rfc3986::DOMAIN_BUFSIZE];
	const net::hostport remote
	{
		service(net::hostport(hp)) == "matrix"?
			m::fed::server(buf, hp):
			m::fed::matrix_service(hp)
	};

	auto &peer
	{
		server::find(remote)
	};

	peer.cancel();
	return true;
}
catch(const std::out_of_range &e)
{
	throw error
	{
		"Peer not found"
	};
}

bool
console_cmd__peer__close(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"hostport", "[dc]"
	}};

	const auto &hostport
	{
		param.at(0)
	};

	const auto &dc
	{
		param.at(1, "SSL_NOTIFY"_sv)
	};

	auto &peer
	{
		server::find(hostport)
	};

	const net::close_opts opts
	{
		dc == "RST"?
			net::dc::RST:
		dc == "SSL_NOTIFY"?
			net::dc::SSL_NOTIFY:
			net::dc::SSL_NOTIFY
	};

	peer.close(opts);
	peer.err_clear();
	return true;
}
catch(const std::out_of_range &e)
{
	throw error
	{
		"Peer not found"
	};
}

bool
console_cmd__peer__request(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"servername", "linkid"
	}};

	const auto &servername
	{
		param["servername"]
	};

	const auto &linkid
	{
		param["linkid"]
	};

	out
	<< std::right  << std::setw(32) << "PEER NAME" << "  "
	<< std::left   << std::setw(32) << "REMOTE ADDRESS" << "  "
	<< std::right   << std::setw(8)  << "PEER" << "  "
	<< std::right   << std::setw(8)  << "LINK" << "  "
	<< std::right   << std::setw(8)  << "TAG" << "  "
	<< std::right   << std::setw(4)  << "POS" << "  "
	<< std::right  << std::setw(8)  << "WROTE" << "  "
	<< std::right  << std::setw(5)  << "RHEAD" << "  "
	<< std::right  << std::setw(9)  << "RCONT" << "  "
	<< std::right  << std::setw(9)  << "CONTLEN" << "  "
	<< std::right  << std::setw(4)  << "CODE" << "  "
	<< std::right  << std::setw(4)  << "FLAG" << "  "
	<< std::right  << std::setw(4)  << "FLAG" << "  "
	<< std::right  << std::setw(4)  << "FLAG" << "  "
	<< std::right  << std::setw(7)  << "METHOD" << "  "
	<< std::left   << std::setw(72) << "PATH" << "  "
	<< std::endl
	;

	const auto each{[&out]
	(const server::peer &peer, const server::link &link, const server::request &request)
	{
		const auto out_head
		{
			request.out.gethead(request)
		};

		thread_local char rembuf[128];
		const string_view &remote
		{
			link.socket?
				string(rembuf, remote_ipport(*link.socket)):
				"<no socket>"_sv
		};

		size_t pos(0);
		if(request.tag)
			for(auto it(begin(link.queue)); it != end(link.queue); ++it, ++pos)
				if(std::addressof(*it) == request.tag)
					break;

		out
		<< std::right  << std::setw(32) << trunc(peer.hostcanon, 32) << "  "
		<< std::left   << std::setw(32) << trunc(remote, 32) << "  "
		<< std::right   << std::setw(8)  << peer.id << "  "
		<< std::right   << std::setw(8)  << link.id << "  "
		<< std::right   << std::setw(8)  << id(request) << "  "
		<< std::right   << std::setw(4)  << pos << "  "
		;

		if(request.tag)
		{
			out
			<< std::right << std::setw(8) << request.tag->state.written << "  "
			<< std::right << std::setw(5) << request.tag->state.head_read << "  "
			<< std::right << std::setw(9) << request.tag->state.content_read << "  "
			<< std::right << std::setw(9) << request.tag->state.content_length << "  "
			;
		}

		if(request.tag)
			out << std::setw(4) << uint(request.tag->state.status) << "  ";
		else if(!request.tag)
			out << std::setw(4) << "----" << "  ";
		else
			out << std::setw(4) << "CNCL" << "  ";

		if(request.tag && request.tag->committed() && bool(request.tag->state.status))
			out << std::setw(4) << "DONE" << "  ";
		else if(request.tag && request.tag->committed())
			out << std::setw(4) << "PIPE" << "  ";
		else if(!request.tag)
			out << std::setw(4) << "----" << "  ";
		else
			out << std::setw(4) << "    " << "  ";

		if(request.tag && request.tag->abandoned())
			out << std::setw(4) << "GONE" << "  ";
		else if(!request.tag)
			out << std::setw(4) << "----" << "  ";
		else
			out << std::setw(4) << "    " << "  ";

		if(request.tag && request.tag->canceled())
			out << std::setw(4) << "CNCL" << "  ";
		else if(!request.tag)
			out << std::setw(4) << "----" << "  ";
		else
			out << std::setw(4) << "    " << "  ";

		out
		<< std::right  << std::setw(7)  << out_head.method << "  "
		<< std::left   << std::setw(72) << trunc(out_head.path, 72) << "  "
		;

		out << std::endl;
		return true;
	}};

	if(servername && linkid)
	{
		auto &peer
		{
			server::find(servername)
		};

		throw m::UNSUPPORTED
		{
			"Link identifiers are not yet implemented;"
			" cannot iterate request for one link."
		};

		return true;
	}

	if(servername)
	{
		auto &peer
		{
			server::find(servername)
		};

		server::for_each(peer, each);
		return true;
	}

	server::for_each(each);
	return true;
}
catch(const std::out_of_range &e)
{
	throw error
	{
		"Peer not found"
	};
}

//
// net
//

bool
console_cmd__net__addrs(opt &out, const string_view &line)
{
	net::addrs::for_each([&out]
	(const net::addrs::addr &addr)
	{
		out << std::left << std::setw(16) << addr.name << " "
		    << std::setw(32) << addr.address << " "
		    << "family[" << std::setw(2) << addr.family << "] "
		    << "scope[" << addr.scope_id << "] "
		    << "flowinfo[" << addr.flowinfo << "] "
		    << "flags[0x" << std::hex << addr.flags << "]" << std::dec
		    << std::endl;

		return true;
	});

	return true;
}

bool
console_cmd__net__service(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"service", "proto"
	}};

	const string_view &service
	{
		param.at("service")
	};

	const string_view &proto
	{
		param.at("proto", "tcp"_sv)
	};

	if(lex_castable<uint16_t>(service))
	{
		char buf[64];
		const auto name
		{
			net::dns::service_name(buf, lex_cast<uint16_t>(service), proto)
		};

		out << name << std::endl;
		return true;
	}

	const auto port
	{
		net::dns::service_port(service, proto)
	};

	out << port << std::endl;
	return true;
}

bool
console_cmd__net__host(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"hostport", "qtype"
	}};

	const net::hostport hostport
	{
		param.at("hostport")
	};

	const string_view &qtype
	{
		param["qtype"]
	};

	ctx::dock dock;
	bool done{false};
	std::string res[2];
	std::exception_ptr eptr;
	net::dns::opts opts;
	opts.qtype = qtype? rfc1035::qtype.at(qtype) : 0;

	const net::dns::callback_ipport cbipp{[&done, &dock, &eptr, &res]
	(std::exception_ptr eptr_, const net::hostport &hp, const net::ipport &ipport)
	{
		eptr = std::move(eptr_);
		res[0] = string(hp);
		res[1] = string(ipport);
		done = true;
		dock.notify_one();
	}};

	const net::dns::callback cbarr{[&done, &dock, &eptr, &res]
	(const net::hostport &hp, const json::array &rrs)
	{
		res[0] = string(hp);
		res[1] = rrs;
		done = true;
		dock.notify_one();
	}};

	if(!opts.qtype)
		net::dns::resolve(hostport, opts, cbipp);
	else
		net::dns::resolve(hostport, opts, cbarr);

	const ctx::uninterruptible ui;
	dock.wait([&done]
	{
		return done;
	});

	if(eptr)
		std::rethrow_exception(eptr);
	else
		out << res[0] << " : " << res[1] << std::endl;

	return true;
}

bool
console_cmd__host(opt &out, const string_view &line)
{
	return console_cmd__net__host(out, line);
}

bool
console_cmd__net__host__cache(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"qtype", "hostport"
	}};

	const string_view &qtype
	{
		param["qtype"]
	};

	if(!param["hostport"])
	{
		net::dns::cache::for_each(qtype, [&]
		(const string_view &host, const auto &r)
		{
			out << std::left << std::setw(48) << host
			    << r
			    << std::endl;

			return true;
		});

		return true;
	}

	const net::hostport hostport
	{
		param["hostport"]
	};

	net::dns::opts opts;
	opts.qtype = rfc1035::qtype.at(qtype);
	net::dns::cache::for_each(hostport, opts, [&]
	(const auto &host, const auto &r)
	{
		out << std::left << std::setw(48) << host
		    << r
		    << std::endl;

		return true;
	});

	return true;
}

bool
console_cmd__net__host__cache__count(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"qtype"
	}};

	const string_view &qtype
	{
		param["qtype"]
	};

	size_t count[2] {0};
	net::dns::cache::for_each(qtype, [&]
	(const auto &host, const auto &r)
	{
		++count[bool(r.size() > 1)];
		return true;
	});

	out << "resolved:  " << count[1] << std::endl;
	out << "error:     " << count[0] << std::endl;
	return true;
}

bool
console_cmd__net__host__cache__clear(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"hostport", "[service]"
	}};

	out << "NOT IMPLEMENTED" << std::endl;
	return true;
}

bool
console_cmd__net__listen__list(opt &out, const string_view &line)
{
	using list = std::list<net::listener>;

	static mods::import<list> listeners
	{
		"m_listen", "listeners"
	};

	const list &l(listeners);
	for(const auto &listener : l)
	{
		out << "name       : " << net::name(listener) << std::endl;
		out << "binder     : " << net::binder(listener) << std::endl;
		out << "bound      : " << net::local(listener) << std::endl;
		out << "config     : " << net::config(listener) << std::endl;
		out << std::endl;
	}

	return true;
}

bool
console_cmd__net__listen__ciphers(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"name",
	}};

	const auto &name
	{
		param["name"]
	};

	using list = std::list<net::listener>;

	static mods::import<list> listeners
	{
		"m_listen", "listeners"
	};

	const list &l(listeners);
	for(const auto &listener : l)
	{
		if(name && listener.name() != name)
			continue;

		out << listener.name() << ": " << std::endl
		    << cipher_list(listener)
		    << std::endl
		    << std::endl;
	}

	return true;
}

bool
console_cmd__net__listen(opt &out, const string_view &line)
{
	if(empty(line))
		return console_cmd__net__listen__list(out, line);

	const params token{line, " ",
	{
		"name",
		"host",
		"port",
		"private_key_pem_path",
		"certificate_pem_path",
		"certificate_chain_path",
	}};

	const json::members _opts
	{
		{ "host",                        token.at("host", "0.0.0.0"_sv)            },
		{ "port",                        token.at("port", 8448L)                   },
		{ "private_key_pem_path",        token.at("private_key_pem_path")          },
		{ "certificate_pem_path",        token.at("certificate_pem_path")          },
		{ "certificate_chain_path",      token.at("certificate_chain_path", ""_sv) },
	};

	const json::object &addl
	{
		tokens_after(line, ' ', token.names.size())
	};

	json::strung opts{_opts};
	for(const auto &[name, prop] : addl)
		opts = insert(opts, json::member(name, prop));

	const m::room::id::buf my_room
	{
		"ircd", origin(m::my())
	};

	const auto eid
	{
		m::send(my_room, m::me(), "ircd.listen", token.at("name"), opts)
	};

	out << eid << std::endl;
	return true;
}

bool
console_cmd__net__listen__del(opt &out, const string_view &line)
{
	const params token{line, " ",
	{
		"name"
	}};

	const m::room::id::buf my_room_id
	{
		"ircd", origin(m::my())
	};

	const m::room my_room
	{
		my_room_id
	};

	const auto event_idx
	{
		my_room.get("ircd.listen", token.at("name"))
	};

	const auto event_id
	{
		m::event_id(event_idx)
	};

	const auto redact_id
	{
		m::redact(my_room, m::me(), event_id, "deleted")
	};

	out << "Removed listener '" << token.at("name") << "' configuration. " << std::endl
	    << "The configuration is still saved in the content of " << event_id << std::endl
	    << "You may still need to unload this listener from service." << std::endl
	    ;

	return true;
}

bool
console_cmd__net__listen__load(opt &out, const string_view &line)
{
	using prototype = bool (const string_view &);

	static mods::import<prototype> load_listener
	{
		"m_listen", "load_listener"
	};

	const params params{line, " ",
	{
		"name",
	}};

	if(load_listener(params.at("name")))
		out << "loaded listener '" << params.at("name") << "'" << std::endl;
	else
		out << "failed to load listener '" << params.at("name") << "'" << std::endl;

	return true;
}

bool
console_cmd__net__listen__unload(opt &out, const string_view &line)
{
	using prototype = bool (const string_view &);

	static mods::import<prototype> unload_listener
	{
		"m_listen", "unload_listener"
	};

	const params params{line, " ",
	{
		"name",
	}};

	if(unload_listener(params.at("name")))
		out << "unloaded listener '" << params.at("name") << "'" << std::endl;
	else
		out << "failed to unload listener '" << params.at("name") << "'" << std::endl;

	return true;
}

bool
console_cmd__net__listen__crt(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"listener|path"
	}};

	string_view filename;
	const string_view &targ
	{
		param.at("listener|path")
	};

	static mods::import<std::list<net::listener>> listeners
	{
		"m_listen", "listeners"
	};

	const auto &list{*listeners};
	for(const auto &listener : list)
	{
		if(listener.name() != targ)
			continue;

		const json::object config
		{
			listener
		};

		filename = unquote(config.get("certificate_pem_path"));
	}

	if(!filename)
	{
		filename = targ;
		return true;
	}

	const unique_buffer<mutable_buffer> buf
	{
		32_KiB
	};

	const std::string certfile
	{
		fs::read(filename)
	};

	out << openssl::printX509(buf, certfile, 0) << std::endl;
	return true;
}

//
// client
//

bool
console_cmd__client(opt &out, const string_view &line)
{
	using std::setw;
	using std::left;
	using std::right;

	const params param{line, " ",
	{
		"[request|id]",
	}};

	const bool &reqs
	{
		param[0] == "request"
	};

	const auto &idnum
	{
		!reqs?
			param.at<ulong>(0, 0):
			0
	};

	std::vector<client *> clients(client::map.size());
	static const values<decltype(client::map)> values;
	std::transform(begin(client::map), end(client::map), begin(clients), values);
	std::sort(begin(clients), end(clients), []
	(const auto &a, const auto &b)
	{
		return a->id < b->id;
	});

	out << right
	    << setw(8) << "ID"
	    << " "
	    << setw(8) << "SOCKID"
	    << " "
	    << setw(6) << "RDY"
	    << " "
	    << setw(6) << "REQ"
	    << " "
	    << right
	    << setw(4) << "CTX"
	    << " "
	    << left
	    << setw(11) << "TIME"
	    << " "
	    << right
	    << setw(25) << "BYTES FROM"
	    << " "
	    << setw(25) << "BYTES TO"
	    << " "
	    << setw(50) << "LOCAL"
	    << " "
	    << left
	    << setw(50) << "REMOTE"
	    << " "
	    << std::endl;

	for(const auto &client : clients)
	{
		thread_local char pbuf[2][64];

		if(idnum && client->id < idnum)
			continue;
		else if(idnum && client->id > idnum)
			break;
		else if(reqs && !client->reqctx)
			continue;

		out << right << setw(8) << client->id;

		out << " "
		    << right << setw(8) << (client->sock? net::id(*client->sock) : 0UL)
		    ;

		const std::pair<size_t, size_t> stat
		{
			bool(client->sock)?
				net::bytes(*client->sock):
				std::pair<size_t, size_t>{0, 0}
		};

		out << " "
		    << right << setw(6) << client->ready_count
		    << " "
		    << right << setw(6) << client->request_count
		    ;

		out << " " << right << setw(4)
		    << (client->reqctx? id(*client->reqctx) : 0UL)
		    ;

		out << " "
		    << left << setw(11) << pretty(pbuf[0], client->timer.at<nanoseconds>(), true)
		    ;

		out << " "
		    << right << setw(25) << pretty(pbuf[0], iec(stat.first))
		    << " "
		    << right << setw(25) << pretty(pbuf[1], iec(stat.second))
		    ;

		out << " "
		    << right << setw(50) << local(*client)
		    << " "
		    << left << setw(50) << remote(*client)
		    ;
/*
		if(client->request.user_id)
			out << " " << client->request.user_id;
		else if(client->request.origin)
			out << " " << client->request.origin;
*/
		if(client->request.head.method)
			out << " " << client->request.head.method;

		if(client->request.head.path)
			out << " " << client->request.head.path;

		out << std::endl;
	}

	return true;
}

bool
console_cmd__client__clear(opt &out, const string_view &line)
{
	client::terminate_all();
	client::close_all();
	client::wait_all();
	return true;
}

bool
console_cmd__client__spawn(opt &out, const string_view &line)
{
	client::spawn();
	return true;
}

//
// resource
//

bool
console_cmd__resource(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"path", "method"
	}};

	const auto path
	{
		param["path"]
	};

	const auto method
	{
		param["method"]
	};

	if(path && method && path != "-a")
	{
		const auto &r
		{
			resource::find(path)
		};

		const auto &m
		{
			r[method]
		};

		out << method << " "
		    << path
		    << std::endl;

		out << (m.opts->flags & resource::method::REQUIRES_AUTH? " REQUIRES_AUTH" : "")
		    << (m.opts->flags & resource::method::RATE_LIMITED? " RATE_LIMITED" : "")
		    << (m.opts->flags & resource::method::VERIFY_ORIGIN? " VERIFY_ORIGIN" : "")
		    << (m.opts->flags & resource::method::CONTENT_DISCRETION? " CONTENT_DISCRETION" : "")
		    << std::endl;

		return true;
	}

	for(const auto &p : resource::resources)
	{
		const auto &r(*p.second);
		for(const auto &mp : p.second->methods)
		{
			assert(mp.second);
			const auto &m{*mp.second};
			if(path != "-a" && !m.stats->requests)
				continue;

			out << std::setw(56) << std::left << p.first
			    << " "
			    << std::setw(7) << mp.first
			    << std::right
			    << " | CUR " << std::setw(8) << m.stats->pending
			    << " | REQ " << std::setw(8) << m.stats->requests
			    << " | RET " << std::setw(8) << m.stats->completions
			    << " | TIM " << std::setw(8) << m.stats->timeouts
			    << " | ERR " << std::setw(8) << m.stats->internal_errors
			    << std::endl;
		}
	}

	return true;
}

//
// me
//

bool
console_cmd__me(opt &out, const string_view &line)
{
	out << m::me() << std::endl;
	out << m::public_key_id(m::my()) << std::endl;
	return true;
}

//
// key
//

bool
console_cmd__key(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"server_name"
	}};

	const auto &server_name
	{
		param.at("server_name")
	};

	// keys cached for server by param.
	m::keys::cache::for_each(server_name, [&out]
	(const m::keys &keys)
	{
		pretty_oneline(out, keys) << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__key__get(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"server_name", "[query_server]"
	}};

	const auto server_name
	{
		param.at(0)
	};

	const auto query_server
	{
		param[1]
	};

	if(!query_server)
	{
		m::keys::get(server_name, [&out]
		(const m::keys &keys)
		{
			pretty(out, keys) << std::endl;
		});
	}
	else
	{
		const std::pair<string_view, string_view> queries[1]
		{
			{ server_name, {} }
		};

		m::keys::query(query_server, queries, [&out]
		(const m::keys &keys)
		{
			pretty_oneline(out, keys) << std::endl;
			return true;
		});
	}

	return true;
}

//
// stage
//

std::vector<std::string> stage;

bool
console_cmd__stage__list(opt &out, const string_view &line)
{
	for(const json::object &object : stage)
	{
		const m::event event{object};
		out << pretty_oneline(event) << std::endl;
	}

	return true;
}

bool
console_cmd__stage(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"id", "[json]"
	}};

	if(!param.count())
		return console_cmd__stage__list(out, line);

	const auto &id
	{
		param.at<uint>(0)
	};

	if(stage.size() < id)
		throw error
		{
			"Cannot stage position %d without composing %d first", id, stage.size()
		};

	const auto &key
	{
		param[1]
	};

	const auto &val
	{
		key? tokens_after(line, ' ', 1) : string_view{}
	};

	const m::room::id::buf my_room
	{
		"ircd", origin(m::my())
	};

	if(stage.size() == id)
	{
		m::event base_event{json::members
		{
			{ "depth",             json::undefined_number  },
			{ "origin",            my_host()               },
			{ "origin_server_ts",  time<milliseconds>()    },
			{ "sender",            m::me()                 },
			{ "room_id",           my_room                 },
			{ "type",              "m.room.message"        },
			{ "prev_state",        "[]"                    },
		}};

		const json::strung content{json::members
		{
			{ "body",     "test"    },
			{ "msgtype",  "m.text"  }
		}};

		json::get<"content"_>(base_event) = content;
		stage.emplace_back(json::strung(base_event));
	}

	if(key && val)
	{
		m::event event{stage.at(id)};
		set(event, key, val);
		stage.at(id) = json::strung{event};
	}
	else if(key)
	{
		stage.at(id) = std::string{key};
	}

	const m::event event
	{
		stage.at(id)
	};

	out << pretty(event) << std::endl;
	out << stage.at(id) << std::endl;

	try
	{
		if(!verify(event))
			out << "- SIGNATURE FAILED" << std::endl;
	}
	catch(const std::exception &e)
	{
		out << "- UNABLE TO VERIFY SIGNATURES" << std::endl;
	}

	try
	{
		char buf[512];
		if(!verify_hash(event))
			out << "- HASH MISMATCH: " << b64::encode_unpadded(buf, hash(event)) << std::endl;
	}
	catch(const std::exception &e)
	{
		out << "- UNABLE TO VERIFY HASHES" << std::endl;
	}

	const m::event::conforms conforms
	{
		event
	};

	if(!conforms.clean())
		out << "- " << conforms << std::endl;

	return true;
}

bool
console_cmd__stage__make_prev(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"[id]", "[limit]"
	}};

	const int &id
	{
		param.at<int>(0, -1)
	};

	const auto &limit
	{
		param.at<size_t>(1, 16)
	};

	m::event event
	{
		stage.at(id)
	};

	const m::room room
	{
		at<"room_id"_>(event)
	};

	const m::room::head head
	{
		room
	};

	const unique_mutable_buffer buf
	{
		8_KiB
	};

	const m::room::head::generate prev
	{
		buf, head,
		{
			16,     // .limit           = 20,
			false,  // .need_top_head   = true,
			false,  // .need_my_head    = false,
		}
	};

	json::get<"prev_events"_>(event) = prev.array;
	json::get<"depth"_>(event) = prev.depth[1];
	stage.at(id) = json::strung
	{
		event
	};

	event = m::event(stage.at(id));
	out << pretty(event) << std::endl;
	out << stage.at(id) << std::endl;
	return true;
}

bool
console_cmd__stage__make_auth(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"[id]"
	}};

	const int &id
	{
		param.at<int>(0, -1)
	};

	m::event event
	{
		stage.at(id)
	};

	const m::room room
	{
		at<"room_id"_>(event)
	};

	const unique_mutable_buffer buf
	{
		1_KiB
	};

	json::get<"auth_events"_>(event) =
	{
		m::room::auth::generate(buf, room, event)
	};

	stage.at(id) = json::strung
	{
		event
	};

	event = m::event(stage.at(id));
	out << pretty(event) << std::endl;
	out << stage.at(id) << std::endl;
	return true;
}

bool
console_cmd__stage__final(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"[id]", "[options]"
	}};

	const int &id
	{
		param.at<int>(0, -1)
	};

	const auto opts
	{
		param[1]
	};

	m::event event
	{
		stage.at(id)
	};

	m::event::id::buf event_id_buf;
	if(!has(opts, "no_event_id"))
		json::get<"event_id"_>(event) = make_id(event, "1", event_id_buf);

	thread_local char hashes_buf[512];
	if(!has(opts, "no_hashes"))
		json::get<"hashes"_>(event) = m::hashes(hashes_buf, event);

	thread_local char sigs_buf[512];
	if(!has(opts, "no_signatures"))
		event = signatures(sigs_buf, event);

	stage.at(id) = json::strung
	{
		event
	};

	event = m::event(stage.at(id));
	out << pretty(event) << std::endl;
	out << stage.at(id) << std::endl;
	return true;
}

bool
console_cmd__stage__make_vector(opt &out, const string_view &line)
{
	m::event::id::buf prev_;
	for(size_t i(1); i < stage.size(); ++i)
	{
		const auto prev(unquote(json::object{stage.at(i-1)}.get("event_id")));
		const int64_t depth(json::object{stage.at(i-1)}.get<int64_t>("depth"));
		thread_local char buf[1024], hb[512], sb[512];
		m::event event{stage.at(i)};
		json::stack st{buf};
		{
			json::stack::array top{st};
			{
				json::stack::array a{top};
				a.append(prev);
				{
					json::stack::object hash{a};
					json::stack::member{hash, "w", "nil"};
				}
			}
		}
		json::get<"depth"_>(event) = depth + 1;
		json::get<"prev_events"_>(event) = json::array{st.completed()};
		json::get<"event_id"_>(event) = make_id(event, "1", prev_);
		json::get<"hashes"_>(event) = m::hashes(hb, event);
		event = signatures(sb, event);
		stage.at(i) = json::strung{event};
		out << unquote(json::object{stage.at(i)}.at("event_id")) << std::endl;
	}

	return true;
}

bool
console_cmd__stage__copy(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"srcid", "[dstid]"
	}};

	const auto &srcid
	{
		param.at<uint>(0)
	};

	const auto &dstid
	{
		param.at<uint>(1, stage.size())
	};

	const auto &src
	{
		stage.at(srcid)
	};

	if(stage.size() < dstid)
		throw error
		{
			"Cannot stage position %d without composing %d first", dstid, stage.size()
		};

	if(stage.size() == dstid)
	{
		stage.emplace_back(src);
		return true;
	}

	stage.at(dstid) = src;
	return true;
}

bool
console_cmd__stage__clear(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"[id]",
	}};

	const int &id
	{
		param.at<int>(0, -1)
	};

	if(id == -1)
	{
		stage.clear();
		return true;
	}

	stage.at(id).clear();
	return true;
}

bool
console_cmd__stage__eval(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"[id]",
	}};

	const int &id
	{
		param.at<int>(0, -1)
	};

	m::vm::opts opts;
	m::vm::eval eval
	{
		opts
	};

	if(id >= 0)
	{
		const std::vector<m::event> events
		{
			json::object(stage.at(id))
		};

		execute(eval, events);
		return true;
	}

	const std::vector<m::event> events
	{
		std::begin(stage), std::end(stage)
	};

	execute(eval, events);
	return true;
}

bool
console_cmd__stage__send(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"remote", "[id]"
	}};

	const string_view remote
	{
		param.at(0)
	};

	const int &id
	{
		param.at<int>(1, -1)
	};

	std::vector<json::value> pduv;
	if(id > -1)
		pduv.emplace_back(stage.at(id));
	else
		for(size_t i(0); i < stage.size(); ++i)
			pduv.emplace_back(stage.at(i));

	const auto txn
	{
		m::txn::create(pduv)
	};

	thread_local char idbuf[128];
	const auto txnid
	{
		m::txn::create_id(idbuf, txn)
	};

	const unique_buffer<mutable_buffer> bufs
	{
		16_KiB
	};

	m::fed::send::opts opts;
	opts.remote = remote;
	m::fed::send request
	{
		txnid, const_buffer{txn}, bufs, std::move(opts)
	};

	request.wait(out.timeout);
	const auto code
	{
		request.get()
	};

	const json::object response{request};
	const m::fed::send::response resp
	{
		response
	};

	resp.for_each_pdu([&]
	(const m::event::id &event_id, const json::object &error)
	{
		out << remote << " ->" << txnid << " " << event_id << " ";
		if(empty(error))
			out << http::status(code) << std::endl;
		else
			out << string_view{error} << std::endl;
	});

	return true;
}

bool
console_cmd__stage__broadcast(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"[id]"
	}};

	const int &id
	{
		param.at<int>(0, -1)
	};

	const auto start
	{
		id > -1? id : 0
	};

	const auto stop
	{
		id > -1? id + 1 : stage.size()
	};

	for(size_t i(start); i < stop; ++i)
	{
		const m::vm::opts opts;
		const m::event event{stage.at(i)};
		//m::vm::accepted a{event, &opts, nullptr, &opts.report};
		//m::vm::accept(a);
	}

	return true;
}

int
console_command_numeric(opt &out, const string_view &line)
{
	return console_cmd__stage(out, line);
}

//
// events
//

bool
console_cmd__events(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"start", "stop"
	}};

	const int64_t start
	{
		param.at<int64_t>("start", -1)
	};

	const int64_t stop
	{
		param.at<int64_t>("stop", start == -1? 0 : -1)
	};

	size_t limit
	{
		stop == 0 || stop == -1?
			32:
			std::numeric_limits<size_t>::max()
	};

	const auto closure{[&out, &limit]
	(const m::event::idx &seq, const m::event &event)
	{
		out << seq << " " << pretty_oneline(event) << std::endl;
		return --limit > 0;
	}};

	const m::events::range range
	{
		uint64_t(start), uint64_t(stop)
	};

	m::events::for_each(range, closure);
	return true;
}

bool
console_cmd__events__filter(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"start", "event_filter_json"
	}};

	const uint64_t start
	{
		param.at<uint64_t>(0, uint64_t(-1))
	};

	const m::event_filter filter
	{
		param.at(1)
	};

	m::events::for_each({start, 0}, filter, [&out]
	(const m::event::idx &seq, const m::event &event)
	{
		out << seq << " " << pretty_oneline(event) << std::endl;;
		return true;
	});

	return true;
}

bool
console_cmd__events__in__sender(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id"
	}};

	const m::user::id &user_id
	{
		param.at("user_id")
	};

	size_t i(0);
	m::events::sender::for_each_in(user_id, [&out, &i]
	(const m::user::id &user_id, const m::event::idx &event_idx)
	{
		const m::event::fetch event
		{
			std::nothrow, event_idx
		};

		if(!event.valid)
		{
			out << event_idx << " " << "NOT FOUND" << std::endl;
			return true;
		}

		if(json::get<"room_id"_>(event) == "!2Ae7qzmYoskWNSUuTMRTdze6DQo5:zemos.net")
			return true;

		if(json::get<"room_id"_>(event) == "!AAAANTUiY1fBZ230:zemos.net")
			return true;

		out << event_idx << " " << pretty_oneline(event) << std::endl;;
		return i++ < 2048;
	});

	return true;
}

bool
console_cmd__events__in__origin(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"origin"
	}};

	const string_view &origin
	{
		lstrip(param.at("origin"), ':')
	};

	m::events::origin::for_each_in(origin, [&out]
	(const m::user::id &user_id, const m::event::idx &event_idx)
	{
		const m::event::fetch event
		{
			std::nothrow, event_idx
		};

		if(!event.valid)
		{
			out << event_idx << " " << "NOT FOUND" << std::endl;
			return true;
		}

		out << event_idx << " " << pretty_oneline(event) << std::endl;;
		return true;
	});

	return true;
}

bool
console_cmd__events__in__type(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"type"
	}};

	const string_view &type
	{
		param.at("type")
	};

	m::events::type::for_each_in(type, [&out]
	(const string_view &type, const m::event::idx &event_idx)
	{
		const m::event::fetch event
		{
			std::nothrow, event_idx
		};

		if(!event.valid)
		{
			out << event_idx << " " << "NOT FOUND" << std::endl;
			return true;
		}

		out << event_idx << " " << pretty_oneline(event) << std::endl;;
		return true;
	});

	return true;
}

bool
console_cmd__events__in(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"what"
	}};

	const string_view &what
	{
		param.at("what")
	};

	if(valid(m::id::USER, what))
		return console_cmd__events__in__sender(out, line);

	if(startswith(what, ':') && rfc3986::valid_host(std::nothrow, lstrip(what, ':')))
		return console_cmd__events__in__origin(out, line);

	return console_cmd__events__in__type(out, line);
}

bool
console_cmd__events__type(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"prefix"
	}};

	const string_view &prefix
	{
		param["prefix"]
	};

	m::events::type::for_each(prefix, [&out]
	(const string_view &type)
	{
		out << type << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__events__type__counts(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"prefix"
	}};

	const string_view &prefix
	{
		param["prefix"]
	};

	m::events::type::for_each(prefix, [&out]
	(const string_view &type)
	{
		size_t i(0);
		m::events::type::for_each_in(type, [&i]
		(const string_view &type, const m::event::idx &event_idx)
		{
			++i;
			return true;
		});

		out
		<< std::setw(8) << std::right << i
		<< " " << type
		<< std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__events__sender(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"prefix"
	}};

	const string_view &prefix
	{
		param["prefix"]
	};

	m::events::sender::for_each(prefix, [&out]
	(const m::user::id &user_id)
	{
		out << user_id << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__events__origin(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"prefix"
	}};

	const string_view &prefix
	{
		param["prefix"]
	};

	m::events::origin::for_each(prefix, [&out]
	(const string_view &origin)
	{
		out << origin << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__events__state(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"state_key", "type", "room_id", "depth", "idx"
	}};

	const m::events::state::tuple key
	{
		param["state_key"],
		param["type"],
		param.at("room_id", m::room::id{}),
		param.at("depth", -1L),
		param.at("idx", 0UL),
	};

	size_t i(0);
	m::events::state::for_each(key, [&out, &i]
	(const auto &tuple)
	{
		const auto &[state_key, type, room_id, depth, event_idx]
		{
			tuple
		};

		out
		<< std::right << std::setw(6) << (i++) << "  "
		<< std::left << std::setw(48) << room_id << " "
		<< std::right << std::setw(8) << depth << " [ "
		<< std::right << std::setw(48) << type << " | "
		<< std::left << std::setw(48) << state_key << " ] "
		<< std::left << std::setw(10) << event_idx << " "
		<< std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__events__refs(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"start", "stop", "type", "limit"
	}};

	const m::event::idx start
	{
		param.at("start", m::vm::sequence::retired - 128)
	};

	const m::event::idx stop
	{
		param.at("stop", m::vm::sequence::retired + 1)
	};

	const string_view &typestr
	{
		param.at("type", "*"_sv)
	};

	size_t limit
	{
		param.at("limit", 2048UL)
	};

	m::dbs::ref type
	{
		typestr == "*"?
			m::dbs::ref(-1):
			m::dbs::ref(0)
	};

	if(type != m::dbs::ref(-1))
		for(; uint8_t(type) < sizeof(m::dbs::ref) * 256; type = m::dbs::ref(uint8_t(type) + 1))
			if(reflect(type) == typestr)
				break;

	m::events::refs::for_each({start, stop}, [&out, &limit]
	(const auto &src, const auto &type, const auto &tgt)
	-> bool
	{
		const auto src_id
		{
			m::event_id(std::nothrow, src)
		};

		const auto tgt_id
		{
			m::event_id(std::nothrow, tgt)
		};

		out
		<< " " << std::right << std::setw(10) << src
		<< " " << std::left << std::setw(45) << trunc(src_id? string_view{src_id}: "<index error>"_sv, 45)
		<< " " << std::right << std::setw(12) << trunc(reflect(type), 12)
		<< " -> " << std::right << std::setw(10) << tgt
		<< " " << std::left << std::setw(45) << trunc(tgt_id? string_view{tgt_id}: "<index error>"_sv, 45)
		<< std::endl
		;

		return --limit;
	});

	return true;
}

bool
console_cmd__events__dump(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"filename"
	}};

	const auto filename
	{
		param.at(0)
	};

	m::events::dump__file(filename);
	return true;
}

bool
console_cmd__events__rebuild(opt &out, const string_view &line)
{
	m::events::rebuild();
	return true;
}

//
// event
//

bool
console_cmd__event(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id"
	}};

	m::event::id::buf event_id_buf;
	if(lex_castable<ulong>(param.at("event_id")))
		event_id_buf = m::event_id(param.at<ulong>("event_id"));

	const m::event::id event_id
	{
		event_id_buf?: param.at("event_id")
	};

	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	const auto event_idx
	{
		index(event_id)
	};

	const m::event::fetch event
	{
		event_id
	};

	if(!empty(args)) switch(hash(token(args, ' ', 0)))
	{
		case hash("raw"):
		{
			if(event.source)
				out << json::strung{event.source} << std::endl;
			else
				out << event << std::endl;

			return true;
		}

		case hash("source"):
		{
			if(event.source)
				out << string_view{event.source} << std::endl;

			return true;
		}

		case hash("idx"):
			out << event_idx << std::endl;
			return true;

		case hash("content"):
		{
			for(const auto &m : json::get<"content"_>(event))
				out << m.first << ": " << m.second << std::endl;

			return true;
		}
	}

	pretty_detailed(out, event, event_idx);
	out << std::endl;
	return true;
}

bool
console_id__event(opt &out,
                  const m::event::id &id,
                  const string_view &line)
{
	return console_cmd__event(out, line);
}

bool
console_cmd__event__sign(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id", "[host]", "[accept|eval]"
	}};

	const m::event::id event_id
	{
		param.at(0)
	};

	const auto &host
	{
		param.at(1, event_id.host())
	};

	const auto &op
	{
		param[2]
	};

	m::fed::event::opts opts;
	opts.remote = host;
	opts.dynamic = false;
	const unique_buffer<mutable_buffer> buf
	{
		128_KiB
	};

	m::fed::event request
	{
		event_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	const auto code
	{
		request.get()
	};

	const m::event orig_event
	{
		request
	};

	const unique_mutable_buffer sigbuf
	{
		64_KiB
	};

	const auto event
	{
		m::signatures(mutable_buffer{sigbuf}, orig_event)
	};

	out << pretty(event)
	    << std::endl;

	if(op == "accept")
	{
		const m::vm::opts opts;
/*
		m::vm::accepted a
		{
			event, &opts, nullptr, &opts.report
		};

		m::vm::accept(a);
*/
	}
	else if(op == "eval")
	{
		m::vm::opts opts;
		m::vm::eval
		{
			event, opts
		};
	}

	return true;
}

bool
console_cmd__event__bad(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id",
	}};

	const m::event::id event_id
	{
		param.at(0)
	};

	const bool b
	{
		bad(event_id)
	};

	out << event_id << "is"
	    << (b? " " : " NOT ")
	    << "BAD"
	    << std::endl;

	return true;
}

bool
console_cmd__event__horizon(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id",
	}};

	const string_view &event_id
	{
		param["event_id"]
	};

	if(!event_id)
	{
		const auto &num_keys
		{
			db::property<db::prop_int>(m::dbs::event_horizon, "rocksdb.estimate-num-keys")
		};

		out << "Estimated event_id's unresolved: " << num_keys << '.' << std::endl;
		return true;
	}

	const m::event::horizon horizon
	{
		event_id
	};

	horizon.for_each([&out, &event_id]
	(const auto &, const auto &event_idx)
	{
		const m::event::fetch event
		{
			std::nothrow, event_idx
		};

		out
		<< event.event_id
		<< " -> "
		<< event_idx
		<< " ";

		if(event.valid)
			out << pretty_oneline(event);
		else
			out << "Not Found.";

		out << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__event__horizon__list(opt &out, const string_view &line)
{
	const m::event::horizon horizon;
	horizon.for_each([&out]
	(const auto &event_id, const auto &event_idx)
	{
		const auto _event_id
		{
			m::event_id(std::nothrow, event_idx)
		};

		out << event_id
		    << " -> "
		    << event_idx
		    << " "
		    << _event_id
		    << std::endl;

		return true;
	});

	return true;
}

bool
console_cmd__event__horizon__rebuild(opt &out, const string_view &line)
{
	const auto count
	{
		m::event::horizon::rebuild()
	};

	out << "done " << count << std::endl;
	return true;
}

bool
console_cmd__event__horizon__flush(opt &out, const string_view &line)
{
	size_t count(0);
	const m::event::horizon horizon;
	horizon.for_each([&out, &count]
	(const auto &event_id, const auto &event_idx)
	{
		m::room::id::buf room_id_buf;
		const string_view &room_id
		{
			m::get(std::nothrow, event_idx, "room_id", room_id_buf)
		};

		if(!room_id)
			return true;

		//m::fetch::start(room_id, event_id);
		++count;

		//TODO: XXX
		while(m::fetch::count() > 64)
			ctx::sleep(seconds(1));

		return true;
	});

	return true;
}

bool
console_cmd__event__cached(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id|event_idx",
	}};

	const string_view id
	{
		param.at(0)
	};

	static const m::event::fetch::opts opts
	{
		m::event::keys::exclude {}
	};

	if(valid(m::id::EVENT, id))
	{
		const m::event::id event_id
		{
			id
		};

		const bool cached
		{
			m::cached(event_id, opts)
		};

		out << event_id << " is"
		    << (cached? " " : " not ")
		    << "cached"
		    << std::endl;

		return true;
	}
	else if(lex_castable<ulong>(id))
	{
		const m::event::idx event_idx
		{
			lex_cast<ulong>(id)
		};

		const bool cached
		{
			m::cached(event_idx, opts)
		};

		out << "idx[" << event_idx << "] is"
		    << (cached? " " : " not ")
		    << "cached"
		    << std::endl;

		return true;
	}
	else throw m::BAD_REQUEST
	{
		"Not a valid event_id or `event_idx"
	};

	return true;
}

bool
console_cmd__event__erase(opt &out, const string_view &line)
{
	const m::event::id event_id
	{
		token(line, ' ', 0)
	};

	m::event::fetch event
	{
		event_id
	};

	db::txn txn
	{
		*m::dbs::events
	};

	m::dbs::write_opts opts;
	opts.op = db::op::DELETE;
	opts.event_idx = index(event);
	m::dbs::write(txn, event, opts);
	txn();

	out << "erased " << txn.size() << " cells"
	    << " for " << event_id << std::endl;

	return true;
}

bool
console_cmd__event__rewrite(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id"
	}};

	const m::event::id &event_id
	{
		param.at("event_id")
	};

	const m::event::fetch event
	{
		event_id
	};

	m::dbs::write_opts opts;
	opts.op = db::op::SET;
	opts.event_idx = event.event_idx;

	db::txn txn{*m::dbs::events};
	m::dbs::write(txn, event, opts);

	out << "executing cells:" << txn.size()
	    << " "
	    << "size: " << pretty(iec(txn.bytes()))
	    << " for " << event_id << std::endl;

	txn();
	return true;
}

bool
console_cmd__event__visible(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id", "user_id|node_id"
	}};

	const m::event::id &event_id
	{
		param.at(0)
	};

	const string_view &mxid
	{
		param[1]
	};

	const m::event::fetch event
	{
		event_id
	};

	const bool visible
	{
		m::visible(event, mxid)
	};

	out << event.event_id << " is "
	    << (visible? "VISIBLE" : "NOT VISIBLE")
	    << (mxid? " to " : "")
	    << mxid
	    << std::endl;

	return true;
}

bool
console_cmd__event__auth(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id"
	}};

	const m::event::id &event_id
	{
		param.at("event_id")
	};

	const m::event::fetch event
	{
		event_id
	};

	m::room::auth::check(event);
	out << "pass" << std::endl;
	return true;
}

bool
console_cmd__event__refs__rebuild(opt &out, const string_view &line)
{
	m::event::refs::rebuild();
	out << "done" << std::endl;
	return true;
}

bool
console_cmd__event__refs(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id", "type"
	}};

	const m::event::id &event_id
	{
		param.at("event_id")
	};

	const m::event::refs refs
	{
		index(event_id)
	};

	const string_view &typestr
	{
		param["type"]
	};

	m::dbs::ref type
	{
		empty(typestr)?
			m::dbs::ref(-1):
			m::dbs::ref(0)
	};

	if(!empty(typestr))
		for(; uint8_t(type) < sizeof(m::dbs::ref) * 256; type = m::dbs::ref(uint8_t(type) + 1))
			if(reflect(type) == typestr)
				break;

	refs.for_each(type, [&out, &event_id, &refs]
	(const auto &tgt, const auto &type)
	{
		const auto tgt_id
		{
			m::event_id(std::nothrow, tgt)
		};

		out
		<< " " << std::right << std::setw(10) << refs.idx
		<< " " << std::left << std::setw(45) << trunc(event_id, 45)
		<< " " << std::right << std::setw(12) << trunc(reflect(type), 12)
		<< " -> " << std::right << std::setw(10) << tgt
		<< " " << std::left << std::setw(60) << trunc(tgt_id? string_view{tgt_id}: "<index error>"_sv, 60)
		<< std::endl
		;

		return true;
	});

	return true;
}

bool
console_cmd__event__refs__count(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id", "type"
	}};

	const m::event::id &event_id
	{
		param.at("event_id")
	};

	const m::event::refs refs
	{
		index(event_id)
	};

	const string_view &typestr
	{
		param["type"]
	};

	m::dbs::ref type
	{
		empty(typestr)?
			m::dbs::ref(-1):
			m::dbs::ref(0)
	};

	if(!empty(typestr))
		for(; uint8_t(type) < sizeof(m::dbs::ref) * 256; type = m::dbs::ref(uint8_t(type) + 1))
			if(reflect(type) == typestr)
				break;

	const auto count
	{
		refs.count(type)
	};

	out << count << std::endl;
	return true;
}

bool
console_cmd__event__refs__next(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id"
	}};

	const m::event::id &event_id
	{
		param.at("event_id")
	};

	const m::event::refs refs
	{
		index(event_id)
	};

	refs.for_each(m::dbs::ref::NEXT, [&out]
	(const auto &idx, const auto &type)
	{
		const m::event::fetch event
		{
			std::nothrow, idx
		};

		if(!event.valid)
			return true;

		out << idx
		    << " " << m::event_id(idx)
		    << " " << reflect(type)
		    << std::endl;

		return true;
	});

	return true;
}

bool
console_cmd__event__refs__auth(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id", "type"
	}};

	const m::event::id &event_id
	{
		param.at("event_id")
	};

	const string_view type
	{
		param.at("type", ""_sv)
	};

	const m::room::auth::refs auth
	{
		index(event_id)
	};

	auth.for_each(type, [&out]
	(const m::event::idx &idx)
	{
		const m::event::fetch event
		{
			std::nothrow, idx
		};

		if(!event.valid)
			return true;

		out << idx
		    << " " << pretty_oneline(event)
		    << std::endl;

		return true;
	});

	return true;
}

//
// commit
//

//
// eval
//

bool
console_cmd__eval__file(opt &out, const string_view &line);

bool
console_cmd__eval(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id", "opts",
	}};

	if(!valid(m::id::EVENT, param.at(0)))
		return console_cmd__eval__file(out, line);

	const m::event::id &event_id
	{
		param.at(0)
	};

	const auto &args
	{
		tokens_after(line, ' ', 1)
	};

	const m::event::fetch event
	{
		event_id
	};

	m::vm::opts opts;
	opts.nothrows = 0;

	tokens(args, ' ', [&opts](const auto &arg)
	{
		switch(hash(arg))
		{
			case "replay"_:
				opts.replays = true;
				break;

			case "nowrite"_:
				opts.phase.reset(m::vm::phase::WRITE);
				break;

			case "noverify"_:
				opts.phase.reset(m::vm::phase::VERIFY);
				break;
		}
	});

	out
	<< pretty(event)
	<< std::endl;

	m::vm::eval
	{
		event, opts
	};

	out
	<< "done"
	<< std::endl;
	return true;
}

bool
console_cmd__eval__file(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"path", "limit"
	}};

	const auto path
	{
		param.at("path")
	};

	const auto limit
	{
		param.at<size_t>("limit", -1UL)
	};

	fs::fd::opts file_opts(std::ios::in);
	const fs::fd file
	{
		path, file_opts
	};

	fs::map::opts map_opts(file_opts);
	const fs::map map
	{
		file, map_opts
	};

	// This array is backed by the mmap
	const json::array events
	{
		const_buffer{map}
	};

	m::vm::opts vm_opts;
	vm_opts.infolog_accept = true;
	vm_opts.limit = limit;
	m::vm::eval
	{
		events, vm_opts
	};

	return true;
}

//
// rooms
//

bool
console_cmd__rooms(opt &out, const string_view &line)
{
	const string_view &search_term
	{
		line
	};

	const m::rooms::opts opts
	{
		search_term
	};

	auto limit
	{
		64
	};

	m::rooms::for_each(opts, [&limit, &out]
	(const m::room::id &room_id) -> bool
	{
		out << room_id << std::endl;
		return --limit > 0;
	});

	return true;
}

bool
console_cmd__rooms__dump(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"filename"
	}};

	const auto filename
	{
		param.at(0)
	};

	static conf::item<size_t>
	rooms_dump_prefetch
	{
		{ "name",     "ircd.console.rooms.dump.prefetch" },
		{ "default",  16L                                },
	};

	m::rooms::opts opts;
	opts.remote_only = true;
	opts.prefetch = rooms_dump_prefetch;
	m::rooms::dump__file(opts, filename);
	return true;
}

bool
console_cmd__rooms__public(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"server", "search_term", "limit"
	}};

	const string_view &server
	{
		param.at("server", string_view{})
	};

	const string_view &search_term
	{
		param["server"] && startswith(param["server"], ':') && param["search_term"] != "*"?
			param["search_term"]:

		param["server"] && startswith(param["server"], ':')?
			string_view{}:

		param["server"] != "*"?
			param["server"]:

		string_view{}
	};

	auto limit
	{
		param.at("limit", 32L)
	};

	m::rooms::opts opts;
	opts.server = server;
	opts.search_term = search_term;
	opts.summary = true;
	opts.join_rule = "public";
	m::rooms::for_each(opts, [&limit, &out]
	(const m::room::id &room_id) -> bool
	{
		out << room_id << std::endl;
		return --limit > 0;
	});

	return true;
}

bool
console_cmd__rooms__fetch(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"server", "since"
	}};

	const string_view &server
	{
		param.at("server")
	};

	const string_view &since
	{
		param.at("since", string_view{})
	};

	const m::rooms::summary::fetch fetch
	{
		server, since
	};

	out << "done" << std::endl
	    << "total room count estimate: " << fetch.total_room_count_estimate << std::endl
	    << "next batch: " << fetch.next_batch << std::endl
	    ;

	return true;
}

bool
console_cmd__rooms__head__reset(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"server"
	}};

	const string_view &server
	{
		param["server"] != "*" &&
		param["server"] != "remote_joined_only" &&
		param["server"] != "local_only"?
			param["server"]:
			string_view{}
	};

	m::rooms::opts opts;
	opts.server = server;

	if(param["server"] == "remote_joined_only")
		opts.remote_joined_only = true;

	if(param["server"] == "local_only")
		opts.local_only = true;

	m::rooms::for_each(opts, [&out]
	(const m::room::id &room_id) -> bool
	{
		const m::room::head head
		{
			room_id
		};

		m::room::head::reset(head);
		return true;
	});

	return true;
}

//
// room
//

bool console_cmd__room__events(opt &out, const string_view &line);

bool
console_cmd__room(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id",
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const auto top
	{
		m::top(std::nothrow, room_id)
	};

	const m::room room
	{
		room_id, std::get<m::event::id::buf>(top)
	};

	const m::room::state state
	{
		room
	};

	const m::room::auth::chain auth
	{
		std::get<m::event::idx>(top)
	};

	char version_buf[32], display_buf[256];

	out << "display name:      " << m::display_name(display_buf, room_id) << std::endl;
	out << "creator:           " << m::creator(room_id) << std::endl;
	out << "version:           " << m::version(version_buf, room_id) << std::endl;
	out << "internal:          " << m::internal(room_id) << std::endl;
	out << "local only:        " << std::boolalpha << m::local_only(room_id) << std::endl;
	out << "local joined:      " << std::boolalpha << m::local_joined(room_id) << std::endl;
	out << "remote joined:     " << std::boolalpha << m::remote_joined(room_id) << std::endl;
	out << std::endl;

	const m::room::members members{room_id};
	out
	<< "invite local:      " << members.count("invite", my_host()) << std::endl
	<< "invite:            " << members.count("invite") << std::endl
	<< "join local:        " << members.count("join", my_host()) << std::endl
	<< "join:              " << members.count("join") << std::endl
	<< "leave local:       " << members.count("leave", my_host()) << std::endl
	<< "leave:             " << members.count("leave") << std::endl
	<< "ban local:         " << members.count("ban", my_host()) << std::endl
	<< "ban:               " << members.count("ban") << std::endl
	<< std::endl;

	out << "servers:           " << m::room::origins{room_id}.count() << std::endl;
	out << "servers up:        " << m::room::origins{room_id}.count_online() << std::endl;
	out << "servers err:       " << m::room::origins{room_id}.count_error() << std::endl;

	out << std::endl;
	out << "heads:             " << m::room::head{room_id}.count() << std::endl;
	out << "auth depth:        " << auth.depth() << std::endl;
	out << "state:             " << m::room::state{room_id}.count() << std::endl;
	out << "states:            " << m::room::state::space{room_id}.count() << std::endl;
	out << "events:            " << m::room{room_id}.count() << std::endl;
	out << "index:             " << m::room::index(room_id) << std::endl;
	out << std::endl;

	out << "top depth:         " << std::get<int64_t>(top) << std::endl;
	out << "top event:         " << std::get<m::event::id::buf>(top) << std::endl;
	out << "top index:         " << std::get<m::event::idx>(top) << std::endl;
	out << std::endl;

	out << "m.room state: " << std::endl;

	state.for_each(m::room::state::type_prefix{"m.room."}, [&out, &state]
	(const string_view &type, const string_view &state_key, const m::event::idx &event_idx)
	{
		assert(startswith(type, "m.room."));
		if(type == "m.room.member")
			return true;

		if(state_key != ""_sv && type != "m.room.aliases")
			return true;

		const m::event::fetch event
		{
			std::nothrow, event_idx
		};

		if(!event.valid)
			return true;

		const size_t &evw
		{
			event.event_id.version() == "1"? 64UL : 40UL
		};

		for(const auto &[prop, val] : json::get<"content"_>(event))
			out
			<< std::left << std::setw(evw) << event.event_id
			<< " " << std::right << std::setw(30) << json::get<"type"_>(event)
			<< " | " << std::left << std::setw(24) << prop
			<< " " << val
			<< std::endl;

		return true;
	});

	out << std::endl;
	out << "recent auth:" << std::endl;

	ssize_t adi(auth.depth());
	auth.for_each([&out, &adi]
	(const m::event::idx &event_idx)
	{
		if(adi-- > 5)
			return true;

		const m::event::fetch event
		{
			std::nothrow, event_idx
		};

		if(event.valid)
			m::pretty_stateline(out, event, event_idx);

		return true;
	});

	out << std::endl;
	out << "recent events: " << std::endl;

	char linebuf[256];
	static const size_t last_count(5);
	console_cmd__room__events(out, fmt::sprintf
	{
		linebuf, "%s -%ld",
		string_view{room_id},
		last_count,
	});

	out << std::endl;
	out << "recent missing: " << std::endl;

	const m::room::events::missing missing
	{
		room
	};

	ssize_t missing_count(3);
	missing.rfor_each({0, 0}, [&out, &missing_count, &top]
	(const auto &event_id, const auto &ref_depth, const auto &ref_idx)
	{
		out
		<< std::right << std::setw(8) << (int64_t(ref_depth) - std::get<int64_t>(top))
		<< " "
		<< std::right << std::setw(8) << ref_depth
		<< " "
		<< std::right << std::setw(10) << ref_idx
		<< " "
		<< std::left << std::setw(64) << m::event_id(ref_idx)
		<< " missing: "
		<< std::left << event_id
		<< std::endl;
		return missing_count--;
	});

	out << std::endl;
	out << "oldest missing: " << std::endl;

	missing_count = 3;
	missing.for_each({0, 0}, [&out, &missing_count, &top]
	(const auto &event_id, const auto &ref_depth, const auto &ref_idx)
	{
		out
		<< std::right << std::setw(8) << (int64_t(ref_depth) - std::get<int64_t>(top))
		<< " "
		<< std::right << std::setw(8) << ref_depth
		<< " "
		<< std::right << std::setw(10) << ref_idx
		<< " "
		<< std::left << std::setw(64) << m::event_id(ref_idx)
		<< " missing: "
		<< std::left << event_id
		<< std::endl;
		return missing_count--;
	});

	out << std::endl;
	out << "recent gaps: " << std::endl;

	size_t gap_count(4);
	const m::room::events::sounding gaps
	{
		room
	};

	gaps.rfor_each([&out, &gap_count, &top]
	(const auto &range, const auto &event_idx)
	{
		out
		<< std::right << std::setw(8) << (int64_t(range.first) - std::get<int64_t>(top))
		<< " "
		<< std::right << std::setw(8) << range.first
		<< " -> "
		<< std::left << std::setw(8) << range.second
		<< " "
		<< (m::room::state::is(std::nothrow, event_idx)? "S" : " ")
		<< " "
		<< m::event_id(event_idx)
		<< std::endl;

		return gap_count--;
	});

	return true;
}

bool
console_cmd__room__version(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id",
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	char buf[32];
	out << m::version(buf, room_id) << std::endl;
	return true;
}

bool
console_cmd__room__head(opt &out, const string_view &line)
{
	const auto &room_id
	{
		m::room_id(token(line, ' ', 0))
	};

	const m::room room
	{
		room_id
	};

	const m::room::head head
	{
		room
	};

	head.for_each([&out]
	(const m::event::idx &event_idx, const m::event::id &event_id)
	{
		const m::event::fetch event
		{
			std::nothrow, event_idx
		};

		out << pretty_oneline(event) << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__room__head__count(opt &out, const string_view &line)
{
	const auto &room_id
	{
		m::room_id(token(line, ' ', 0))
	};

	const m::room room
	{
		room_id
	};

	const m::room::head head
	{
		room
	};

	out << head.count() << std::endl;
	return true;
}

bool
console_cmd__room__head__rebuild(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id",
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::room room
	{
		room_id
	};

	const m::room::head head
	{
		room
	};

	const size_t count
	{
		head.rebuild(head)
	};

	out << "done " << count << std::endl;
	return true;
}

bool
console_cmd__room__head__add(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id"
	}};

	const m::event::id &event_id
	{
		param.at(0)
	};

	m::room::head::modify(event_id, db::op::SET, true);
	out << "Added " << event_id << " to head " << std::endl;
	return true;
}

bool
console_cmd__room__head__del(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id"
	}};

	const m::event::id &event_id
	{
		param.at(0)
	};

	m::room::head::modify(event_id, db::op::DELETE, true);
	out << "Deleted " << event_id << " from head (if existed)" << std::endl;
	return true;
}

bool
console_cmd__room__head__reset(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id",
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::room room
	{
		room_id
	};

	const m::room::head head
	{
		room
	};

	const size_t count
	{
		head.reset(head)
	};

	out << "done " << count << std::endl;
	return true;
}

bool
console_cmd__room__head__fetch(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id"
	}};

	const auto room_id
	{
		m::room_id(param.at("room_id"))
	};

	const m::room room
	{
		room_id
	};

	m::room::head::fetch::opts opts;
	opts.room_id = room_id;
	m::room::head::fetch fetch
	{
		opts, [&out](const m::event &result)
		{
			out
			<< pretty_oneline(result)
			<< std::endl;
			return true;
		}
	};

	out
	<< std::endl
	<< "results:        " << fetch.heads << std::endl
	<< "exists:         " << fetch.exists << std::endl
	<< "concur:         " << fetch.concur << std::endl
	<< "unique:         " << fetch.head.size() << std::endl
	<< "servers:        " << fetch.respond << std::endl
	<< "depth ahead:    " << fetch.depth[2] << std::endl
	<< "depth equal:    " << fetch.depth[1] << std::endl
	<< "depth behind:   " << fetch.depth[0] << std::endl
	<< "ots ahead:      " << fetch.ots[2] << std::endl
	<< "ots equal:      " << fetch.ots[1] << std::endl
	<< "ots behind:     " << fetch.ots[0] << std::endl
	;

	return true;
}

bool
console_cmd__room__sounding(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "event_id"
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const m::event::id event_id
	{
		param["event_id"]?
			m::event::id{param["event_id"]}:
			m::event::id{}
	};

	const m::room room
	{
		room_id, event_id
	};

	const auto hazard
	{
		m::hazard(room)
	};

	const auto twain
	{
		m::twain(room)
	};

	const auto head
	{
		m::head_idx(room)
	};

	const auto create
	{
		m::room::index(room)
	};

	const auto sounding
	{
		m::sounding(room)
	};

	out << "head:      " << std::setw(8) << m::depth(room)
	    << "   " << m::event_id(head) << " (" << head << ")"
	    << std::endl;

	out << "hazard:    " << std::setw(8) << hazard.first
	    << std::endl;

	out << "sounding:  " << std::setw(8) << sounding.first
	    << "   " << m::event_id(sounding.second) << " (" << sounding.second << ")"
	    << std::endl;

	out << "twain:     " << std::setw(8) << twain.first
	    << std::endl;

	out << "create:    " << std::setw(8) << m::get<uint64_t>(create, "depth")
	    << "   " << m::event_id(create) << " (" << create << ")"
	    << std::endl;

	return true;
}

bool
console_cmd__room__depth(opt &out, const string_view &line)
{
	const auto &room_id
	{
		m::room_id(token(line, ' ', 0))
	};

	const m::room room
	{
		room_id
	};

	out << depth(room_id) << std::endl;
	return true;
}

bool
console_cmd__room__depth__gaps(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "reverse"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::room room
	{
		room_id
	};

	const auto closure{[&out]
	(const auto &range, const auto &event_idx)
	{
		out << std::right << std::setw(8) << range.first
		    << " -> "
		    << std::left << std::setw(8) << range.second
		    << " "
		    << (m::room::state::is(std::nothrow, event_idx)? "S" : " ")
		    << " "
		    << m::event_id(event_idx)
		    << std::endl;

		return true;
	}};

	const m::room::events::sounding gaps
	{
		room
	};

	if(param["reverse"] == "reverse")
		gaps.rfor_each(closure);
	else
		gaps.for_each(closure);

	return true;
}

bool
console_cmd__room__visible(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "user_id|node_id", "event_id"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const string_view &mxid
	{
		param[1] && param[1] != "*"?
			param[1]:
		param[1] == "*"?
			string_view{}:
			param[1]
	};

	const auto &event_id
	{
		param[2]
	};

	const m::room room
	{
		room_id, event_id
	};

	const bool visible
	{
		m::visible(room, mxid)
	};

	out << room_id << " is "
	    << (visible? "VISIBLE" : "NOT VISIBLE")
	    << (mxid? " to " : "")
	    << mxid
	    << (event_id? " at " : "")
	    << event_id
	    << std::endl;

	return true;
}

//
// room alias
//

bool
console_cmd__room__alias(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "server"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const string_view &server
	{
		param["server"]
	};

	const m::room::aliases aliases
	{
		room_id
	};

	aliases.for_each(server, [&out]
	(const m::room::alias &alias)
	{
		out << alias << std::endl;
		return true;
	});

	return true;
}

//
// room cache
//

bool
console_cmd__room__alias__cache(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"server"
	}};

	const string_view &server
	{
		param["server"]
	};

	out
	<< std::left << std::setw(40) << "EXPIRES"
	<< " " << std::left << std::setw(48) << "ROOM ALIAS"
	<< " " << std::left << std::setw(48) << "ROOM ID"
	<< std::endl;

	m::room::aliases::cache::for_each(server, [&out]
	(const m::room::alias &alias, const m::room::id &room_id)
	{
		const auto expire_point
		{
			m::room::aliases::cache::expires(alias)
		};

		char buf[48];
		const auto expires
		{
			timef(buf, expire_point, ircd::localtime)
		};

		out
		<< std::left << std::setw(40) << expires
		<< " " << std::left << std::setw(48) << alias
		<< " " << std::left << std::setw(48) << room_id
		<< std::endl;

		return true;
	});

	return true;
}

bool
console_cmd__room__alias__cache__has(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"alias"
	}};

	const m::room::alias &alias
	{
		param["alias"]
	};

	const bool has
	{
		m::room::aliases::cache::has(alias)
	};

	out << std::boolalpha << has << std::endl;
	return true;
}

bool
console_cmd__room__alias__cache__set(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"alias", "room_id"
	}};

	const m::room::alias &alias
	{
		param["alias"]
	};

	const m::room::id &room_id
	{
		param["room_id"]
	};

	const bool set
	{
		m::room::aliases::cache::set(alias, room_id)
	};

	out << std::boolalpha << set << std::endl;
	return true;
}

bool
console_cmd__room__alias__cache__fetch(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"alias", "remote"
	}};

	const m::room::alias &alias
	{
		param["alias"]
	};

	const auto &remote
	{
		param["remote"]?
			param["remote"]:
			alias.host()
	};

	m::room::aliases::cache::fetch(alias, remote);

	out << "done" << std::endl;
	return true;
}

bool
console_cmd__room__alias__cache__get(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"alias"
	}};

	const m::room::alias &alias
	{
		param["alias"]
	};

	const m::room::id::buf room_id
	{
		m::room::aliases::cache::get(alias)
	};

	out << room_id << std::endl;
	return true;
}

bool
console_cmd__room__alias__cache__del(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"alias"
	}};

	const m::room::alias &alias
	{
		param["alias"]
	};

	const bool del
	{
		m::room::aliases::cache::del(alias)
	};

	out << std::boolalpha << del << std::endl;
	return true;
}

bool
console_cmd__room__server_acl(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "server"
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const m::room::server_acl server_acl
	{
		room_id
	};

	if(param["server"])
	{
		const bool allowed
		{
			server_acl(param.at("server"))
		};

		out << (allowed? "allow"_sv : "deny"_sv)
		    << std::endl;

		return true;
	}

	server_acl.for_each("allow", [&out]
	(const string_view &expression)
	{
		out << "allow         " << expression << std::endl;
		return true;
	});

	out << std::endl;
	server_acl.for_each("deny", [&out]
	(const string_view &expression)
	{
		out << "deny          " << expression << std::endl;
		return true;
	});

	out << std::endl;
	out << "IP literals   ";
	if(server_acl.getbool("allow_ip_literals") != false)
		out << "allow." << std::endl;
	else
		out << "deny." << std::endl;

	return true;
}

bool
console_cmd__room__members(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "[membership]" "[host]"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const string_view &membership
	{
		param[1] != "\"\""?
			param[1]:
			string_view{}
	};

	const string_view &host
	{
		param[2]
	};

	const m::room room
	{
		room_id
	};

	const m::room::members members
	{
		room
	};

	if(membership)
	{
		members.for_each(membership, host, [&out, &membership]
		(const m::user::id &user_id)
		{
			out << std::setw(8) << std::left << membership
			    << " " << user_id << std::endl;

			return true;
		});

		return true;
	}

	members.for_each(membership, host, [&out]
	(const m::user::id &user_id, const m::event::idx &event_idx)
	{
		char buf[32];
		out << std::setw(8) << std::left << m::membership(buf, event_idx)
		    << " " << user_id << std::endl;

		return true;
	});

	return true;
}

bool
console_cmd__room__members__events(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "[membership]"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const string_view membership
	{
		param[1]
	};

	const m::room room
	{
		room_id
	};

	const m::room::members members
	{
		room
	};

	const auto closure{[&out](const auto &user_id, const auto &event_idx)
	{
		const m::event::fetch event
		{
			std::nothrow, event_idx
		};

		if(!event.valid)
			return true;

		out << pretty_oneline(event) << std::endl;
		return true;
	}};

	members.for_each(membership, closure);
	return true;
}

bool
console_cmd__room__members__count(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "[membership]"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const string_view membership
	{
		param[1]
	};

	const m::room room
	{
		room_id
	};

	const m::room::members members
	{
		room
	};

	out << members.count(membership) << std::endl;
	return true;
}

bool
console_cmd__room__members__origin(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "origin", "[membership]"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const auto &origin
	{
		param.at(1)
	};

	const string_view membership
	{
		param[2]
	};

	const m::room room
	{
		room_id
	};

	const m::room::members members
	{
		room
	};

	members.for_each(membership, [&out, &origin]
	(const auto &user_id, const auto &event_idx)
	{
		const bool same_origin
		{
			m::query(std::nothrow, event_idx, "origin", [&origin]
			(const auto &_origin)
			{
				return _origin == origin;
			})
		};

		if(!same_origin)
			return true;

		const m::event::fetch event
		{
			std::nothrow, event_idx
		};

		if(!event.valid)
			return true;

		out << pretty_oneline(event) << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__room__members__read(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id", "[membership]"
	}};

	const m::event::id &event_id
	{
		param.at(0)
	};

	const string_view membership
	{
		param.at(1, "join"_sv)
	};

	m::room::id::buf room_id
	{
		m::get(event_id, "room_id", room_id)
	};

	const m::room room
	{
		room_id
	};

	const m::room::members members
	{
		room
	};

	const m::event::closure event_closure{[&out, &event_id]
	(const m::event &event)
	{
		if(event_id)
			if(unquote(at<"content"_>(event).get("event_id")) != event_id)
				return;

		out << timestr(at<"origin_server_ts"_>(event) / 1000)
		    << " " << at<"sender"_>(event)
		    << " " << at<"content"_>(event)
		    << " " << event.event_id
		    << std::endl;
	}};

	members.for_each(membership, [&room_id, &event_closure]
	(const m::user::id &user_id, const m::event::idx &event_idx)
	{
		const m::user user
		{
			user_id
		};

		static const m::event::fetch::opts fopts
		{
			m::event::keys::include
			{
				"event_id", "content", "origin_server_ts", "sender"
			},
			{
				db::get::NO_CACHE
			},
		};

		const m::user::room user_room
		{
			user, nullptr, &fopts
		};

		user_room.get(std::nothrow, "ircd.read", room_id, event_closure);
		return true;
	});

	return true;
}

bool
console_cmd__room__origins(opt &out, const string_view &line)
{
	const auto &room_id
	{
		m::room_id(token(line, ' ', 0))
	};

	const m::room room
	{
		room_id
	};

	const m::room::origins origins
	{
		room
	};

	origins.for_each([&out](const string_view &origin)
	{
		out << origin << std::endl;
	});

	return true;
}

bool
console_cmd__room__origins__random(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "[noerror]"
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const bool noerror
	{
		param.at<bool>("[noerror]", false)
	};

	const m::room room
	{
		room_id
	};

	const m::room::origins origins
	{
		room
	};

	const auto ok{[&noerror]
	(const string_view &origin)
	{
		if(noerror && m::fed::errant(origin))
			return false;

		return true;
	}};

	char buf[256];
	const string_view origin
	{
		origins.random(buf, ok)
	};

	if(!origin)
		throw m::NOT_FOUND
		{
			"No origins for this room."
		};

	out << origin << std::endl;
	return true;
}

bool
console_cmd__room__state(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "event_id_or_type"
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const auto &event_id_or_type
	{
		param.at("event_id_or_type", string_view{})
	};

	const auto is_event_id
	{
		m::has_sigil(event_id_or_type) && valid(m::id::EVENT, event_id_or_type)
	};

	const m::room room
	{
		room_id, is_event_id? event_id_or_type : string_view{}
	};

	const m::room::state state
	{
		room
	};

	const string_view &type
	{
		!is_event_id?
			event_id_or_type:
			string_view{}
	};

	state.for_each(type, [&out]
	(const string_view &type, const string_view &state_key, const m::event::idx &event_idx)
	{
		const m::event::fetch event
		{
			std::nothrow, event_idx
		};

		if(!event.valid)
			return true;

		m::pretty_stateline(out, event, event_idx);

		/*
		size_t i(0);
		auto prev_idx(event_idx);
		for(; i < 4 && prev_idx; ++i, prev_idx = state.prev(prev_idx))
			out << (i? " <-- " : "") << prev_idx;
		out << std::endl;
		*/

		return true;
	});

	return true;
}

bool
console_cmd__room__state__events(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "event_id_or_type"
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const auto &event_id_or_type
	{
		param.at("event_id_or_type", string_view{})
	};

	const auto is_event_id
	{
		m::has_sigil(event_id_or_type) && valid(m::id::EVENT, event_id_or_type)
	};

	const m::room room
	{
		room_id, is_event_id? event_id_or_type : string_view{}
	};

	const m::room::state state
	{
		room
	};

	const string_view &type
	{
		!is_event_id?
			event_id_or_type:
			string_view{}
	};

	state.for_each(type, [&out](const m::event &event)
	{
		out << pretty_oneline(event) << std::endl;
	});

	return true;
}

bool
console_cmd__room__state__count(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "event_id_or_type"
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const auto &event_id_or_type
	{
		param.at("event_id_or_type", string_view{})
	};

	const auto is_event_id
	{
		m::has_sigil(event_id_or_type) && valid(m::id::EVENT, event_id_or_type)
	};

	const m::room room
	{
		room_id, is_event_id? event_id_or_type : string_view{}
	};

	const m::room::state state
	{
		room
	};

	const string_view &type
	{
		!is_event_id?
			event_id_or_type:
			string_view{}
	};

	out << state.count(type) << std::endl;
	return true;
}

bool
console_cmd__room__state__types(opt &out, const string_view &line)
{
	const auto &room_id
	{
		m::room_id(token(line, ' ', 0))
	};

	const auto &event_id
	{
		token(line, ' ', 1, {})
	};

	const m::room room
	{
		room_id, event_id
	};

	const m::room::state state
	{
		room
	};

	state.for_each([&out]
	(const string_view &type, const string_view &state_key, const m::event::idx &)
	{
		out << type << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__room__state__keys(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "type", "event_id", "prefix"
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const auto &type
	{
		param.at("type")
	};

	const auto &event_id
	{
		param.at("event_id", string_view{})
	};

	const string_view &prefix
	{
		param.at("prefix", string_view{})
	};

	const m::room room
	{
		room_id, event_id
	};

	const m::room::state state
	{
		room
	};

	state.for_each(type, prefix, [&out, &prefix]
	(const string_view &, const string_view &state_key, const m::event::idx &)
	{
		if(prefix && !startswith(state_key, prefix))
			return false;

		out << state_key << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__room__state__history(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "event_id|depth", "type", "state_key"
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const auto point
	{
		param.at("event_id|depth")
	};

	const string_view &type
	{
		param["type"]
	};

	const string_view &state_key
	{
		param["state_key"]
	};

	const m::event::id &event_id
	{
		!lex_castable<int64_t>(point)?
			m::event::id{point}:
			m::event::id{}
	};

	const int64_t bound
	{
		lex_castable<int64_t>(point)?
			lex_cast<int64_t>(point):
			-1L
	};

	const m::room room
	{
		room_id, event_id
	};

	const m::room::state state
	{
		room
	};

	const m::room::state::history history
	{
		room, bound
	};

	history.for_each(type, state_key, [&out]
	(const auto &type, const auto &state_key, const auto &depth, const auto &event_idx)
	{
		const m::event::fetch event
		{
			std::nothrow, event_idx
		};

		if(!event.valid)
			return true;

		m::pretty_stateline(out, event, event_idx);
		return true;
	});

	return true;
}

bool
console_cmd__room__state__space(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "type", "state_key", "depth"
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const string_view &type
	{
		param["type"] != "*"?
			param["type"] : string_view{},
	};

	const string_view &state_key
	{
		param["state_key"] != "\"\""?
			param["state_key"] : string_view{},
	};

	const int64_t depth
	{
		param.at("depth", -1L)
	};

	const m::room::state state
	{
		room_id
	};

	const m::room::state::space space
	{
		room_id
	};

	space.for_each(type, state_key, depth, [&out]
	(const auto &type, const auto &state_key, const auto &depth, const auto &event_idx)
	{
		const m::event::fetch event
		{
			std::nothrow, event_idx
		};

		if(!event.valid)
			return true;

		m::pretty_stateline(out, event, event_idx);
		return true;
	});

	return true;
}

bool
console_cmd__room__state__space__rebuild(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id",
	}};

	const string_view &room_id
	{
		param["room_id"]
	};

	if(room_id == "*" || room_id == "remote_joined_only")
	{
		m::rooms::opts opts;
		opts.remote_joined_only = room_id == "remote_joined_only";
		m::rooms::for_each(opts, []
		(const m::room::id &room_id)
		{
			m::room::state::space::rebuild
			{
				room_id
			};

			return true;
		});

		return true;
	}

	const auto _room_id
	{
		room_id?
			m::room_id(room_id):
			m::room::id::buf{}
	};

	m::room::state::space::rebuild
	{
		_room_id
	};

	return true;
}

bool
console_cmd__room__state__purge__replaced(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id",
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const size_t ret
	{
		m::room::state::purge_replaced(room_id)
	};

	out << "erased " << ret << std::endl;
	return true;
}

bool
console_cmd__room__state__rebuild(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id"
	}};

	const m::room::id::buf room_id
	{
		param.at("room_id") != "*" && param.at("room_id") != "remote_joined_only"?
			m::room_id(param.at(0)):
			param["room_id"]
	};

	if(room_id == "*" || room_id == "remote_joined_only")
	{
		m::rooms::opts opts;
		opts.remote_joined_only = room_id == "remote_joined_only";
		m::rooms::for_each(opts, []
		(const m::room::id &room_id)
		{
			m::room::state::rebuild
			{
				room_id
			};

			return true;
		});

		return true;
	}

	m::room::state::rebuild
	{
		room_id
	};

	out << "done" << std::endl;
	return true;
}

bool
console_cmd__room__state__prefetch(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "[event_id_or_type]", "[type]"
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const auto &event_id_or_type
	{
		param.at("[event_id_or_type]", string_view{})
	};

	const auto is_event_id
	{
		m::has_sigil(event_id_or_type) && valid(m::id::EVENT, event_id_or_type)
	};

	const string_view &event_id
	{
		is_event_id?
			event_id_or_type:
			string_view{}
	};

	const auto &type
	{
		is_event_id?
			param.at("[type]", string_view{}):
			event_id_or_type
	};

	const m::room room
	{
		room_id, event_id
	};

	const m::room::state state
	{
		room
	};

	const size_t prefetched
	{
		state.prefetch(type)
	};

	out << "prefetched " << prefetched << std::endl;
	return true;
}

bool
console_cmd__room__state__cache(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "[event_id_or_type]", "[type]"
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const auto &event_id_or_type
	{
		param.at("[event_id_or_type]", string_view{})
	};

	const auto is_event_id
	{
		m::has_sigil(event_id_or_type) && valid(m::id::EVENT, event_id_or_type)
	};

	const string_view &event_id
	{
		is_event_id?
			event_id_or_type:
			string_view{}
	};

	const auto &type
	{
		is_event_id?
			param.at("[type]", string_view{}):
			event_id_or_type
	};

	const m::room room
	{
		room_id, event_id
	};

	const m::room::state state
	{
		room
	};

	size_t total(0);
	std::array<size_t, m::dbs::event_columns> res {{0}};
	state.for_each(type, m::event::closure_idx{[&total, &res]
	(const m::event::idx &event_idx)
	{
		const byte_view<string_view> &key(event_idx);
		for(size_t i(0); i < m::dbs::event_column.size(); ++i)
		{
			auto &column(m::dbs::event_column[i]);
			res[i] += db::cached(column, key);
		}

		++total;
	}});

	std::array<string_view, m::event::size()> keys;
	_key_transform(m::event{}, begin(keys), end(keys));
	assert(res.size() == keys.size());

	for(size_t i(0); i < keys.size(); ++i)
		out << std::left << std::setw(16) << keys[i]
		    << " " << std::right << std::setw(6) << res[i]
		    << " of " << std::left << std::setw(6) << total
		    << std::endl;

	return true;
}

bool
console_cmd__room__state__fetch(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "event_id", "opt"
	}};

	const auto room_id
	{
		m::room_id(param.at("room_id"))
	};

	const auto event_id
	{
		param["event_id"]?
			m::event::id::buf{param["event_id"]}:
			m::head(room_id)
	};

	m::room::state::fetch::opts opts;
	opts.room.room_id = room_id;
	opts.room.event_id = event_id;
	opts.existing = has(param["opt"], "existing");
	opts.unique = true;

	size_t i(0);
	m::room::state::fetch fetch
	{
		opts, [&out, &i](const m::event::id &event_id, const string_view &remote)
		{
			out
			<< std::setw(4) << std::left << i++
			<< " "
			<< std::setw(60) << std::left << event_id
			<< " "
			<< remote
			<< std::endl;
			return true;
		}
	};

	out
	<< std::endl
	<< "servers:    " << fetch.respond << std::endl
	<< "unique:     " << fetch.result.size() << std::endl
	<< "concur:     " << fetch.concur << std::endl
	<< "exists:     " << fetch.exists << std::endl
	<< "results:    " << fetch.responses << std::endl
	;

	return true;
}

bool
console_cmd__room__count(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "{event_filter_json}"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::event_filter filter
	{
		param[1]
	};

	const m::room room
	{
		room_id
	};

	auto limit
	{
		json::get<"limit"_>(filter)?: -1
	};

	if(param[1])
	{
		size_t count{0};
		m::room::events it{room};
		for(; it && limit; --it, --limit)
		{
			const m::event &event{*it};
			count += match(filter, event);
		}

		out << count << std::endl;
		return true;
	}

	const size_t count
	{
		room.count()
	};

	out << count << std::endl;
	return true;
}

bool
console_cmd__room__events(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "depth|-limit", "order", "limit"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const int64_t depth
	{
		param.at<int64_t>(1, std::numeric_limits<int64_t>::max())
	};

	const char order
	{
		param.at(2, "b"_sv).at(0)
	};

	ssize_t limit
	{
		depth < 0?
			std::abs(depth):
			param.at(3, ssize_t(32))
	};

	const m::room room
	{
		room_id
	};

	m::room::events it
	{
		room, uint64_t(depth >= 0? depth : -1)
	};

	for(; it && limit > 0; order == 'b'? --it : ++it, --limit)
		out << std::left << std::setw(10) << it.event_idx() << " "
		    << pretty_oneline(*it)
		    << std::endl;

	return true;
}

bool
console_cmd__room__events__missing(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "limit", "min_depth", "max_depth", "event_id"
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	auto limit
	{
		param.at("limit", 16L)
	};

	const auto &min_depth
	{
		param.at("min_depth", 0L)
	};

	const auto &max_depth
	{
		param.at("max_depth", 0L)
	};

	const auto &event_id
	{
		param["event_id"]
	};

	m::room room
	{
		room_id
	};

	if(m::valid(m::id::EVENT, event_id))
		room.event_id = event_id;

	const auto top
	{
		m::top(room)
	};

	const m::room::events::missing missing
	{
		room
	};

	out
	<< std::right << std::setw(10) << "DIFF"
	<< " "
	<< std::left << std::setw(10) << "SEQUENCE"
	<< " "
	<< std::right << std::setw(10) << "DIFF"
	<< " "
	<< std::left << std::setw(10) << "DEPTH"
	<< " "
	<< std::right << std::setw(6) << "HORIZO"
	<< "  "
	<< std::left << std::setw(52) << "EVENT ID"
	<< std::endl;

	missing.for_each({min_depth, max_depth}, [&out, &limit, &top]
	(const auto &event_id, const auto &ref_depth, const auto &ref_idx)
	{
		const auto &[top_event_id, top_depth, top_idx] {top};

		out
		<< std::right << std::setw(10) << int64_t(ref_idx - top_idx)
		<< " "
		<< std::left << std::setw(10) << ref_idx
		<< " "
		<< std::right << std::setw(10) << int64_t(ref_depth - top_depth)
		<< " "
		<< std::left << std::setw(10) << ref_depth
		<< " "
		<< std::right << std::setw(6) << m::event::horizon(event_id).count()
		<< " "
		<< std::left << std::setw(52) << event_id
		<< std::endl;
		return --limit;
	});

	return true;
}

bool
console_cmd__room__events__missing__count(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "limit", "min_depth", "event_id"
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	auto limit
	{
		param.at("limit", 16L)
	};

	const auto &min_depth
	{
		param.at("min_depth", 0L)
	};

	const auto &event_id
	{
		param["event_id"]
	};

	const m::room room
	{
		room_id, event_id
	};

	const m::room::events::missing missing
	{
		room
	};

	out << missing.count() << std::endl;
	return true;
}

bool
console_cmd__room__events__horizon(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "limit"
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	auto limit
	{
		param.at("limit", 32L)
	};

	const m::room room
	{
		room_id
	};

	const m::room::events::horizon horizon
	{
		room
	};

	horizon.for_each([&out, &limit]
	(const auto &event_id, const auto &ref_depth, const auto &ref_idx)
	{
		out
		<< std::right << std::setw(10) << ref_idx
		<< " "
		<< std::right << std::setw(8) << ref_depth
		<< " "
		<< std::left << std::setw(52) << event_id
		<< std::endl;
		return --limit;
	});

	return true;
}

bool
console_cmd__room__events__horizon__count(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "event_id"
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const auto &event_id
	{
		param.at("event_id", "*"_sv)
	};

	const m::room room
	{
		room_id, event_id
	};

	const m::room::events::horizon horizon
	{
		room
	};

	out << horizon.count() << std::endl;
	return true;
}

bool
console_cmd__room__events__horizon__rebuild(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id"
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const m::room room
	{
		room_id
	};

	m::room::events::horizon horizon
	{
		room
	};

	out << "done " << horizon.rebuild() << std::endl;
	return true;
}

bool
console_cmd__room__acquire__list(opt &out, const string_view &line)
{
	size_t i(0);
	for(const auto *const &a : m::acquire::list)
	{
		size_t j(0);
		for(const auto &result : a->fetching)
			out
			<< std::left << std::setw(4) << i
			<< " "
			<< std::left << std::setw(4) << j++
			<< " "
			<< std::left << std::setw(50) << trunc(a->opts.room.room_id, 40)
			<< " "
			<< std::right << std::setw(4) << a->opts.viewport_size
			<< " ["
			<< std::right << std::setw(7) << a->opts.depth.first
			<< " "
			<< std::right << std::setw(7) << a->opts.depth.second
			<< " | "
			<< std::right << std::setw(8) << a->opts.ref.first
			<< " "
			<< std::right << std::setw(8) << long(a->opts.ref.second)
			<< "] "
			<< std::left << std::setw(50) << trunc(result.event_id, 60)
			<< " "
			<< std::endl;

		i++;
	}

	return true;
}

bool
console_cmd__room__acquire(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "depth_start", "depth_stop", "viewport_size", "gap_min", "rounds"
	}};

	if(!param["room_id"])
		return console_cmd__room__acquire__list(out, line);

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const auto depth_start
	{
		param.at("depth_start", 0L)
	};

	const auto depth_stop
	{
		param.at("depth_stop", 0L)
	};

	const auto viewport_size
	{
		param.at("viewport_size", 0L)
	};

	const auto gap_min
	{
		param.at("gap_min", 0UL)
	};

	const auto rounds
	{
		param.at("rounds", -1UL)
	};

	const m::room room
	{
		room_id
	};

	struct m::acquire::opts opts;
	opts.vmopts.infolog_accept = true;
	opts.room = room_id;
	opts.depth.first = depth_start;
	opts.depth.second = depth_stop;
	opts.viewport_size = viewport_size;
	opts.rounds = rounds;
	opts.head = depth_stop == 0;
	opts.gap.first = gap_min;
	m::acquire
	{
		opts
	};

	return true;
}

bool
console_cmd__room__gossip__list(opt &out, const string_view &line)
{
	size_t i(0);
	for(const auto *const &a : m::gossip::list)
	{
		size_t j(0);
		for(const auto &result : a->requests)
			out
			<< std::left << std::setw(4) << i
			<< " "
			<< std::left << std::setw(4) << j++
			<< " "
			<< std::left << std::setw(50) << trunc(a->opts.room.room_id, 40)
			<< " ["
			<< std::right << std::setw(7) << a->opts.depth.first
			<< " "
			<< std::right << std::setw(7) << a->opts.depth.second
			<< " | "
			<< std::right << std::setw(8) << a->opts.ref.first
			<< " "
			<< std::right << std::setw(8) << long(a->opts.ref.second)
			<< "] "
			<< std::endl;

		i++;
	}

	return true;
}

bool
console_cmd__room__gossip(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "remote", "rounds"
	}};

	if(!param["room_id"])
		return console_cmd__room__gossip__list(out, line);

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const auto remote
	{
		param["remote"]
	};

	const auto rounds
	{
		param.at("rounds", -1UL)
	};

	const m::room room
	{
		room_id
	};

	struct m::gossip::opts opts;
	opts.room = room;
	opts.hint = remote != "*"? remote: string_view{};
	opts.hint_only = !opts.hint.empty();
	opts.rounds = rounds;
	m::gossip gossip
	{
		opts
	};

	return true;
}

bool
console_cmd__room__messages(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "depth|-limit", "order", "limit"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const int64_t depth
	{
		param.at<int64_t>(1, std::numeric_limits<int64_t>::max())
	};

	const char order_
	{
		param.at(2, "B"_sv).at(0)
	};

	const char order
	(
		std::tolower(order_)
	);

	const bool text_only
	{
		false
		|| order_ == 'B'
		|| order_ == 'F'
	};

	ssize_t limit
	{
		depth < 0?
			std::abs(depth):
			param.at(3, ssize_t(32))
	};

	const m::room room
	{
		room_id
	};

	m::room::events it{room};
	if(depth >= 0 && depth < std::numeric_limits<int64_t>::max())
		it.seek(depth);

	for(; it && limit >= 0; order == 'b'? --it : ++it, --limit)
		out
		<< pretty_msgline(*it, text_only? 1: 0)
		<< std::endl;

	return true;
}

bool
console_cmd__room__type(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "type", "start_depth", "end_depth"
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const auto &type
	{
		param["type"]
	};

	const uint64_t start_depth
	{
		param.at<uint64_t>(2, -1UL)
	};

	const int64_t end_depth
	{
		param.at<int64_t>(3, -1L)
	};

	const bool prefix_match
	{
		endswith(type, "...")
	};

	const m::room::type events
	{
		room_id,
		rstrip(type, "..."),
		{ start_depth, end_depth },
		prefix_match
	};

	m::event::fetch event;
	events.for_each([&out, &event]
	(const string_view &type, const uint64_t &depth, const m::event::idx &event_idx)
	{
		if(!seek(std::nothrow, event, event_idx))
			return true;

		out
		<< std::left << std::setw(10) << event_idx
		<< " "
		<< pretty_oneline(event)
		<< std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__room__type__count(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "type", "start_depth", "end_depth"
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const auto &type
	{
		param["type"]
	};

	const uint64_t start_depth
	{
		param.at<uint64_t>(2, -1UL)
	};

	const int64_t end_depth
	{
		param.at<int64_t>(3, -1L)
	};

	const bool prefix_match
	{
		endswith(type, "...")
	};

	const m::room::type events
	{
		room_id,
		rstrip(type, "..."),
		{ start_depth, end_depth },
		prefix_match
	};

	size_t ret(0);
	events.for_each([&ret]
	(const string_view &type, const uint64_t &depth, const m::event::idx &event_idx)
	{
		++ret;
		return true;
	});

	out
	<< ret
	<< std::endl;
	return true;
}

bool
console_cmd__room__get(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "type", "state_key", "args"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const string_view type
	{
		param.at(1)
	};

	const string_view state_key
	{
		param.at(2, ""_sv)
	};

	const string_view arg
	{
		param[3]
	};

	const m::room::state room
	{
		room_id
	};

	room.get(type, state_key, [&out, &arg]
	(const m::event &event)
	{
		if(has(arg, "raw"))
			out << event << std::endl;
		else if(has(arg, "content"))
			out << json::get<"content"_>(event) << std::endl;
		else
			out << pretty(event) << std::endl;
	});

	return true;
}

bool
console_cmd__get(opt &out, const string_view &line)
{
	return console_cmd__room__get(out, line);
}

bool
console_cmd__room__set(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "sender", "type", "state_key", "content", "[prev_event_id]"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::user::id &sender
	{
		param.at(1)
	};

	const string_view type
	{
		param.at(2)
	};

	const string_view state_key
	{
		param.at(3)
	};

	const json::object &content
	{
		param.at(4, json::object{})
	};

	const string_view prev_event_id
	{
		param[5]
	};

	const m::room room
	{
		room_id, prev_event_id
	};

	const auto event_id
	{
		send(room, sender, type, state_key, content)
	};

	out << event_id << std::endl;
	return true;
}

bool
console_cmd__set(opt &out, const string_view &line)
{
	return console_cmd__room__set(out, line);
}

bool
console_cmd__room__send(opt &out, const string_view &line)
{
	const params param
	{
		line, " ",
		{
			"room_id", "sender", "type", "content", "[prev_event_id]"
		}
	};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::user::id &sender
	{
		param.at(1)
	};

	const string_view type
	{
		param.at(2)
	};

	const json::object &content
	{
		param.at(3, json::object{})
	};

	const string_view prev_event_id
	{
		param[4]
	};

	const m::room room
	{
		room_id, prev_event_id
	};

	const auto event_id
	{
		send(room, sender, type, content)
	};

	out << event_id << std::endl;
	return true;
}

bool
console_cmd__room__message(opt &out, const string_view &line)
{
	const auto &room_id
	{
		m::room_id(token(line, ' ', 0))
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
console_cmd__room__join(opt &out, const string_view &line)
{
	const string_view room_id_or_alias
	{
		token(line, ' ', 0)
	};

	const m::user::id &user_id
	{
		token(line, ' ', 1)
	};

	const string_view &event_id
	{
		token(line, ' ', 2, {})
	};

	switch(m::sigil(room_id_or_alias))
	{
		case m::id::ROOM:
		{
			const m::room room
			{
				room_id_or_alias, event_id
			};

			const auto join_event
			{
				m::join(room, user_id)
			};

			out << join_event << std::endl;
			return true;
		}

		case m::id::ROOM_ALIAS:
		{
			const m::room::alias alias
			{
				room_id_or_alias
			};

			const auto join_event
			{
				m::join(alias, user_id)
			};

			out << join_event << std::endl;
			return true;
		}

		default: throw error
		{
			"Don't know how to join '%s'", room_id_or_alias
		};
	}

	return true;
}

bool
console_cmd__room__leave(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id_or_alias", "user_id"
	}};

	const m::room::id::buf room_id
	{
		m::room_id(param.at("room_id_or_alias"))
	};

	const m::user::id::buf user_id
	{
		param.at("user_id")
	};

	const m::room room
	{
		room_id
	};

	const auto leave_event_id
	{
		m::leave(room, user_id)
	};

	out << leave_event_id << std::endl;
	return true;
}

bool
console_cmd__room__create(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "[creator]", "[type]"
	}};

	const m::room::id room_id
	{
		param.at(0)
	};

	const m::user::id creator
	{
		param.at(1, m::me())
	};

	const string_view type
	{
		param[2]
	};

	const m::room room
	{
		m::create(room_id, creator, type)
	};

	out << room.room_id << std::endl;
	return true;
}

bool
console_cmd__room__id(opt &out, const string_view &id)
{
	out << m::room_id(id)
	    << std::endl;

	return true;
}

bool
console_cmd__room__purge(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id",
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::room room
	{
		room_id
	};

	const size_t ret
	{
		m::room::purge(room)
	};

	out << "erased " << ret << std::endl;
	return true;
}

bool
console_cmd__room__auth(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id|room_id", "event_id"
	}};

	const string_view &p0
	{
		param.at("event_id|room_id")
	};

	const m::room::id::buf room_id{[&p0]
	() -> m::room::id::buf
	{
		switch(m::sigil(p0))
		{
			case m::id::ROOM:
				return p0;

			case m::id::ROOM_ALIAS:
				return m::room_id(p0);

			case m::id::EVENT:
				return m::get(m::event::id(p0), "room_id");

			default: throw params::invalid
			{
				"%s is the wrong kind of MXID for this argument",
				reflect(m::sigil(p0))
			};
		}
	}()};

	const m::event::id &event_id
	{
		m::sigil(p0) != m::id::EVENT?
			param.at("event_id"):
			p0
	};

	const m::room::auth::chain ac
	{
		m::index(event_id)
	};

	ac.for_each([&out](const auto &idx)
	{
		const m::event::fetch event
		{
			std::nothrow, idx
		};

		out << idx;
		if(event.valid)
			out << " " << pretty_oneline(event);

		out << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__room__stats(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id",
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const size_t bytes_json
	{
		m::room::stats::bytes_json(room_id)
	};

	out << "JSON bytes:    "
	    << pretty(iec(bytes_json))
	    << std::endl;

	return true;
}

bool
console_cmd__room__restrap(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "host"
	}};

	const auto room_id
	{
		m::room_id(param.at("room_id"))
	};

	const m::user::id::buf user_id
	{
		valid(m::id::EVENT, param.at("room_id"))?
			m::user::id::buf{}:
			any_user(room_id, my_host(), "join")
	};

	const m::event::id::buf &event_id
	{
		valid(m::id::EVENT, param.at("room_id"))?
			param.at("room_id"):
			m::event_id(m::room(room_id).get("m.room.member", user_id))
	};

	const net::hostport &host_
	{
		param.at("host")
	};

	const string_view &host
	{
		param.at("host")
	};

	m::room::bootstrap
	{
		event_id, host
	};

	return true;
}

bool
console_cmd__room__power(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id",
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const m::room::power power
	{
		room_id
	};

	power.for_each([&out]
	(const auto &key, const auto &level)
	{
		out
		<< std::left << std::setw(16) << ' '
		<< " "
		<< std::right << std::setw(8) << level
		<< "  : "
		<< std::left << key
		<< std::endl;
		return true;
	});

	out << std::endl;
	power.for_each_collection([&out, &power]
	(const auto &collection, const auto &level)
	{
		power.for_each(collection, [&out, &collection]
		(const auto &key, const auto &level)
		{
			out
			<< std::left << std::setw(16) << collection
			<< " "
			<< std::right << std::setw(8) << level
			<< "  : "
			<< std::left << key
			<< std::endl;
			return true;
		});

		return true;
	});

	return true;
}

bool
console_cmd__room__power__grant(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "sender", "collection", "key", "level"
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const m::user::id &sender
	{
		param.at("sender")
	};

	string_view collection
	{
		param.at("collection")
	};

	string_view key
	{
		param.at("key")
	};

	const int64_t level
	{
		lex_castable<int64_t>(key)?
			lex_cast<int64_t>(key):
			param.at<int64_t>("level")

	};

	if(lex_castable<int64_t>(key))
	{
		key = collection;
		if(valid(m::id::USER, collection))
			collection = "users";
		else
			collection = {};
	}

	const m::room::power power
	{
		room_id
	};

	const unique_mutable_buffer buf
	{
		48_KiB
	};

	json::stack stack{buf};
	{
		json::stack::object content
		{
			stack
		};

		m::room::power::grant
		{
			content,
			power,
			pair<string_view>
			{
				collection, key
			},
			level
		};
	}

	const auto event_id
	{
		m::send(room_id, "m.room.power_levels", "", json::object
		{
			stack.completed()
		})
	};

	out
	<< sender << ' '
	<< "granted level " << level << ' '
	<< "to " << key << ' '
	<< "in " << collection << ' '
	<< "with " << event_id << ' '
	<< std::endl;
	return true;
}

bool
console_cmd__room__power__revoke(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "sender", "collection", "key"
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const m::user::id &sender
	{
		param.at("sender")
	};

	string_view collection
	{
		param.at("collection")
	};

	string_view key
	{
		param["key"]
	};

	if(!key)
	{
		key = collection;
		if(valid(m::id::USER, collection))
			collection = "users";
		else
			collection = {};
	}

	const m::room::power power
	{
		room_id
	};

	const unique_mutable_buffer buf
	{
		48_KiB
	};

	json::stack stack{buf};
	{
		json::stack::object content
		{
			stack
		};

		m::room::power::revoke
		{
			content,
			power,
			pair<string_view>
			{
				collection, key
			},
		};
	}

	const auto event_id
	{
		m::send(room_id, sender, "m.room.power_levels", "", json::object
		{
			stack.completed()
		})
	};

	out
	<< sender << ' '
	<< "revoked power "
	<< "from " << key << ' '
	<< "in " << collection << ' '
	<< "by " << event_id << ' '
	<< std::endl;
	return true;
}

bool
console_cmd__room__redactfill(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "count", "sender", "reason"
	}};

	const auto room_id
	{
		m::room_id(param.at("room_id"))
	};

	size_t count
	{
		param.at("count", 0UL)
	};

	const auto sender
	{
		param["sender"]?
			m::user::id{param["sender"]}:
			m::me()
	};

	const auto reason
	{
		param.at("reason", "redactfill"_sv)
	};

	m::room::events it
	{
		room_id, -1UL
	};

	while(it && count > 0)
	{
		const auto event_id
		{
			m::event_id(it.event_idx())
		};

		const auto redact_id
		{
			m::redact(room_id, sender, event_id, reason)
		};

		--count;
		--it;
	}

	return true;
}

bool
console_id__room(opt &out,
                 const m::room::id &id,
                 const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "type", "state_key"
	}};

	// Delegate to allow direct command "#foo:bar.com ircd.test fookey"
	if(param["type"] && param["state_key"])
		return console_cmd__room__get(out, line);

	//TODO: XXX more detailed summary
	return console_cmd__room(out, line);
}

//
// user
//

//TODO: XXX
bool
console_id__user(opt &out,
                 const m::user::id &id,
                 const string_view &args)
{
	const bool exists
	{
		m::exists(id)
	};

	if(!exists)
		throw m::NOT_FOUND
		{
			"User %s is not known to this server.",
			string_view{id},
		};

	return true;
}

bool
console_cmd__user__register(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"username", "password"
	}};

	const string_view &username
	{
		param.at("username")
	};

	const string_view &password
	{
		param.at("password")
	};

	const m::user::registar request
	{
		{ "username",       username  },
		{ "password",       password  },
		{ "bind_email",     false     },
		{ "inhibit_login",  true      },
	};

	const unique_buffer<mutable_buffer> buf
	{
		4_KiB
	};

	const auto ret
	{
		request(buf)
	};

	out << ret << std::endl;
	return true;
}

bool
console_cmd__user__password(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "password"
	}};

	m::user user
	{
		param.at("user_id")
	};

	const string_view &password
	{
		param.at("password")
	};

	const auto eid
	{
		user.password(password)
	};

	out << eid << std::endl;
	return true;
}

bool
console_cmd__user__active(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id"
	}};

	const m::user user
	{
		param.at("user_id")
	};

	out << user.user_id << " is "
	    << (active(user)? "active" : "inactive")
	    << std::endl;

	return true;
}

bool
console_cmd__user__activate(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id"
	}};

	m::user user
	{
		param.at("user_id")
	};

	if(active(user))
	{
		out << user.user_id << " is already active" << std::endl;
		return true;
	}

	const auto eid
	{
		user.activate()
	};

	out << eid << std::endl;
	return true;
}

bool
console_cmd__user__deactivate(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id"
	}};

	m::user user
	{
		param.at("user_id")
	};

	if(!active(user))
	{
		out << user.user_id << " is already inactive" << std::endl;
		return true;
	}

	const auto eid
	{
		user.deactivate()
	};

	out << eid << std::endl;
	return true;
}

bool
console_cmd__user__presence(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "limit"
	}};

	const m::user user
	{
		param.at("user_id")
	};

	auto limit
	{
		param.at("limit", size_t(16))
	};

	const m::user::room user_room{user};
	user_room.for_each("ircd.presence", m::event::closure_bool{[&out, &limit]
	(const m::event &event)
	{
		out << timestr(at<"origin_server_ts"_>(event) / 1000)
		    << " " << at<"content"_>(event)
		    << " " << event.event_id
		    << std::endl;

		return --limit > 0;
	}});

	return true;
}

bool
console_cmd__user__presence__set(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "state", "status"
	}};

	const m::user user
	{
		param.at("user_id")
	};

	const string_view &state
	{
		param.at("state")
	};

	const string_view &status
	{
		param["status"]
	};

	const auto eid
	{
		m::presence::set(user, state, status)
	};

	out << eid << std::endl;
	return true;
}

bool
console_cmd__user__rooms(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "[membership]"
	}};

	const m::user &user
	{
		param.at(0)
	};

	const string_view &membership
	{
		param[1]
	};

	const m::user::rooms rooms
	{
		user
	};

	rooms.for_each(membership, m::user::rooms::closure{[&out]
	(const m::room &room, const string_view &membership)
	{
		out << room.room_id
		    << " " << membership
		    << std::endl;
	}});

	return true;
}

bool
console_cmd__user__rooms__count(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "[membership]"
	}};

	const m::user &user
	{
		param.at(0)
	};

	const string_view &membership
	{
		param[1]
	};

	const m::user::rooms rooms
	{
		user
	};

	out << rooms.count(membership) << std::endl;
	return true;
}

bool
console_cmd__user__rooms__origins(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "[membership]"
	}};

	const m::user &user
	{
		param.at(0)
	};

	const string_view &membership
	{
		param[1]
	};

	const m::user::servers origins
	{
		user
	};

	origins.for_each(membership, [&out]
	(const string_view &origin)
	{
		out << origin << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__user__read(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "room_id", "limit"
	}};

	const m::user::id user_id
	{
		param.at("user_id")
	};

	const auto room_id
	{
		param["room_id"] && !startswith(param["room_id"], '*')?
			m::room_id(param["room_id"]):
			m::room::id::buf{}
	};

	const bool all_rooms
	{
		param["room_id"] == "*"
	};

	const bool eye_track
	{
		param["room_id"] == "**"
		|| !param["room_id"]
	};

	const bool fully_read
	{
		param["room_id"] == "***"
	};

	size_t limit
	{
		param.at("limit", 32UL)
	};

	const m::user::room user_room
	{
		user_id
	};

	const m::event::closure each_event{[&out]
	(const m::event &event)
	{
		const json::string event_id
		{
			json::get<"content"_>(event).get("event_id")
		};

		const milliseconds receipt_ts
		{
			json::get<"content"_>(event).get("ts", 0ms)
		};

		const milliseconds origin_server_ts
		{
			json::get<"origin_server_ts"_>(event)
		};

		const bool hidden
		{
			json::get<"content"_>(event).get("m.hidden", false)
		};

		char tsbuf[2][2][64];
		const string_view timef[2]
		{
			ircd::timef(tsbuf[0][0], origin_server_ts.count() / 1000L),
			ircd::timef(tsbuf[1][0], receipt_ts.count() / 1000L, ircd::localtime),
		};

		const string_view ago[2]
		{
			ircd::ago(tsbuf[0][1], system_point(origin_server_ts)),
			ircd::ago(tsbuf[1][1], system_point(receipt_ts), 1),
		};

		const m::event::fetch target
		{
			std::nothrow, event_id
		};

		out
		<< (!hidden? "PUBLIC"_sv: "      "_sv)
		<< ' ' << std::right << std::setw(12) << ago[bool(receipt_ts.count())]
		<< ' ' << std::left << timef[bool(receipt_ts.count())]
		<< ' '
		;

		if(!target.valid)
		{
			out
			<< json::get<"state_key"_>(event)
			<< ' ' << std::left << std::setw(60) << event_id
			<< std::endl;
			return;
		}

		m::pretty_oneline(out, target);
		out << std::endl;
	}};

	if(all_rooms)
	{
		const m::room::state state
		{
			user_room
		};

		state.for_each("ircd.read", each_event);
		return true;
	}

	if(eye_track)
	{
		const m::room::type type
		{
			user_room, "ircd.read"
		};

		type.for_each([&each_event, &limit]
		(const auto &, const auto &, const auto &event_idx) -> bool
		{
			const m::event::fetch event
			{
				std::nothrow, event_idx
			};

			if(likely(event.valid))
				each_event(event);

			return --limit;
		});

		return true;
	}

	if(fully_read)
	{
		const m::room::type type
		{
			user_room, "ircd.account_data!", { -1UL, -1UL }, true
		};

		type.for_each([&each_event, &limit]
		(const auto &, const auto &, const auto &event_idx) -> bool
		{
			const m::event::fetch event
			{
				std::nothrow, event_idx
			};

			if(!event.valid)
				return true;

			if(json::get<"state_key"_>(event) != "m.fully_read")
				return true;

			each_event(event);
			return --limit;
		});

		return true;
	}

	const m::room::state::space space
	{
		user_room
	};

	space.for_each("ircd.read", room_id, [&each_event, &limit]
	(const auto &type, const auto &state_key, const auto &depth, const auto &event_idx) -> bool
	{
		const m::event::fetch event
		{
			std::nothrow, event_idx
		};

		if(likely(event.valid))
			each_event(event);

		return --limit;
	});

	return true;
}

bool
console_cmd__user__read__count(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "room_id"
	}};

	const m::user::id user_id
	{
		param.at("user_id")
	};

	const auto room_id
	{
		param["room_id"]?
			m::room_id(param["room_id"]):
			m::room::id::buf{}
	};

	const m::user::room user_room
	{
		user_id
	};

	if(!room_id)
	{
		const m::room::state state
		{
			user_room
		};

		const size_t count
		{
			state.count("ircd.read")
		};

		out << count << std::endl;
		return true;
	}

	const m::room::state::space space
	{
		user_room
	};

	size_t count {0};
	space.for_each("ircd.read", room_id, [&out, &count]
	(const auto &type, const auto &state_key, const auto &depth, const auto &event_idx) -> bool
	{
		++count;
		return true;
	});

	out << count << std::endl;
	return true;
}

bool
console_cmd__user__read__receipt(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "event_id", "[room_id]|[time]"
	}};

	const m::user::id user_id
	{
		param.at(0)
	};

	const m::event::id event_id
	{
		param.at(1)
	};

	m::room::id::buf room_id
	{
		param[2]?
			param[2]:
			string_view{m::get(event_id, "room_id", room_id)}
	};

	const time_t &ms
	{
		param.at(3, ircd::time<milliseconds>())
	};

	const json::strung content{json::members
	{
		{ "ts", ms },
	}};

	const auto eid
	{
		m::receipt::read(room_id, user_id, event_id, json::object(content))
	};

	out << eid << std::endl;
	return true;
}

bool
console_cmd__user__read__ignore(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"my_user_id", "target_user|room_id"
	}};

	const m::user my_user
	{
		param.at(0)
	};

	string_view target
	{
		param[1]
	};

	const m::user::room user_room
	{
		my_user
	};

	if(!target)
	{
		m::room::state{user_room}.for_each("ircd.read.ignore", [&out]
		(const m::event &event)
		{
			out << at<"state_key"_>(event)
			    << std::endl;
		});

		return true;
	}

	char buf[m::id::MAX_SIZE];
	switch(m::sigil(target))
	{
		case m::id::USER:
		case m::id::ROOM:
			break;

		case m::id::ROOM_ALIAS:
			target = m::room_id(buf, target);
			break;

		default: throw error
		{
			"Unsupported target MXID type for receipt ignores."
		};
	}

	if(user_room.has("ircd.read.ignore", target))
	{
		out << "User " << my_user.user_id << " is already not sending"
		    << " receipts for messages from " << target
		    << std::endl;

		return true;
	}

	const auto eid
	{
		send(user_room, m::me(), "ircd.read.ignore", target, json::object{})
	};

	out << "User " << my_user.user_id << " will not send receipts for"
	    << " messages from " << target
	    << " (" << eid << ")"
	    << std::endl;

	return true;
}

bool
console_cmd__user__filter(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "[filter_id]"
	}};

	const m::user user
	{
		param.at(0)
	};

	const auto &filter_id
	{
		param[1]
	};

	const m::user::filter filter
	{
		user
	};

	if(filter_id)
	{
		out << filter.get(filter_id) << std::endl;
		return true;
	}

	filter.for_each([&out]
	(const string_view &id, const json::object &filter)
	{
		out << id << std::endl;
		out << filter << std::endl;
		out << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__user__events(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "limit"
	}};

	const m::user::events user
	{
		m::user(param.at("user_id"))
	};

	size_t limit
	{
		param.at<size_t>("limit", 32)
	};

	user.for_each([&out, &limit]
	(const m::event &event)
	{
		out << pretty_oneline(event) << std::endl;;
		return bool(--limit);
	});

	return true;
}

bool
console_cmd__user__events__count(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id"
	}};

	const m::user::events user
	{
		m::user(param.at("user_id"))
	};

	out << user.count() << std::endl;
	return true;
}

bool
console_cmd__user__sees(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id_a", "user_id_b", "membership"
	}};

	const m::user user_a
	{
		m::user(param.at("user_id_a"))
	};

	const m::user user_b
	{
		m::user(param.at("user_id_b"))
	};

	const string_view membership
	{
		param.at("membership", "join"_sv)
	};

	const m::user::mitsein mitsein
	{
		user_a
	};

	const bool result
	{
		mitsein.has(user_b, membership)
	};

	out << std::boolalpha << result
	    << std::endl;

	return true;
}

bool
console_cmd__user__mitsein(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id_a", "user_id_b", "membership"
	}};

	const m::user user_a
	{
		m::user(param.at("user_id_a"))
	};

	const string_view user_b
	{
		param.at("user_id_b", "*"_sv)
	};

	const string_view membership
	{
		param["membership"]
	};

	const m::user::mitsein mitsein
	{
		user_a
	};

	if(user_b != "*")
		mitsein.for_each(m::user(user_b), membership, [&out]
		(const m::room &room, const string_view &membership)
		{
			out
			<< room.room_id
			<< std::endl;
			return true;
		});
	else
		mitsein.for_each(membership, [&out]
		(const m::user &other)
		{
			out
			<< other.user_id
			<< std::endl;
			return true;
		});

	return true;
}

bool
console_cmd__user__mitsein__count(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id_a", "user_id_b", "membership"
	}};

	const m::user user_a
	{
		m::user(param.at("user_id_a"))
	};

	const string_view user_b
	{
		param.at("user_id_b", "*"_sv)
	};

	const string_view membership
	{
		param["membership"]
	};

	const m::user::mitsein mitsein
	{
		user_a
	};

	const auto result
	{
		user_b != "*"?
			mitsein.count(m::user(user_b), membership):
			mitsein.count(membership)
	};

	out
	<< result
	<< std::endl;
	return true;
}

bool
console_cmd__user__tokens(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "clear"
	}};

	const m::user user
	{
		param.at("user_id")
	};

	const bool clear
	{
		param["clear"] == "clear"
	};

	const m::user::tokens tokens
	{
		user
	};

	if(clear)
	{
		const size_t count
		{
			tokens.del("Invalidated by administrator console.")
		};

		out << "Invalidated " << count << std::endl;
		return true;
	}

	tokens.for_each([&out]
	(const m::event::idx &event_idx, const string_view &token)
	{
		const milliseconds ost
		{
			m::get<long>(event_idx, "origin_server_ts")
		};

		const milliseconds now
		{
			time<milliseconds>()
		};

		const auto event_id
		{
			m::event_id(std::nothrow, event_idx)
		};

		const auto device_id
		{
			m::user::tokens::device(token)
		};

		out
		<< token
		<< " "
		<< device_id
		<< " "
		<< ost
		<< " "
		<< string_view{event_id}
		<< " "
		<< pretty(now - ost) << " ago"
		<< std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__user__profile(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "key"
	}};

	const m::user::id &user_id
	{
		param.at("user_id")
	};

	const string_view &key
	{
		param["key"]
	};

	const m::user::profile profile
	{
		user_id
	};

	if(key)
	{
		profile.get(key, [&out]
		(const string_view &key, const string_view &val)
		{
			out << val << std::endl;
		});

		return true;
	}

	profile.for_each([&out]
	(const string_view &key, const string_view &val)
	{
		out << key << ": " << val << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__user__profile__fetch(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "key", "remote"
	}};

	const m::user::id &user_id
	{
		param.at("user_id")
	};

	const string_view &key
	{
		param["key"]
	};

	const auto &remote
	{
		param["remote"]?
			param["remote"]:
			user_id.host()
	};

	m::user::profile::fetch(user_id, remote, key);
	out << "done" << std::endl;
	return true;
}

bool
console_cmd__user__account_data(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "key", "[val]"
	}};

	const m::user::id &user_id
	{
		param.at("user_id")
	};

	const string_view &key
	{
		param["key"]
	};

	const json::object &val
	{
		param["[val]"]
	};

	const m::user::account_data account_data
	{
		user_id
	};

	if(!empty(val))
	{
		account_data.set(key, val);
		return true;
	}

	if(key)
	{
		account_data.get(key, [&out]
		(const string_view &key, const json::object &val)
		{
			out << val << std::endl;
		});

		return true;
	}

	account_data.for_each([&out]
	(const string_view &key, const json::object &val)
	{
		out << key << ": " << val << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__user__room_account_data(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "room_id", "key", "[val]"
	}};

	const m::user::id &user_id
	{
		param.at("user_id")
	};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const string_view &key
	{
		param["key"]
	};

	const json::object &val
	{
		param["[val]"]
	};

	const m::user::room_account_data room_account_data
	{
		user_id, room_id
	};

	if(!empty(val))
	{
		room_account_data.set(key, val);
		return true;
	}

	if(key)
	{
		room_account_data.get(key, [&out]
		(const string_view &key, const json::object &val)
		{
			out << val << std::endl;
		});

		return true;
	}

	room_account_data.for_each([&out]
	(const string_view &key, const json::object &val)
	{
		out << key << ": " << val << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__user__room_tags(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "room_id", "tag"
	}};

	const m::user::id &user_id
	{
		param.at("user_id")
	};

	const auto &room_id
	{
		param["room_id"]?
			m::room_id(param.at("room_id")):
			m::room::id::buf{}
	};

	const string_view &tag
	{
		param["tag"]
	};

	if(room_id)
	{
		const auto output
		{
			[&out, &room_id](const string_view &key, const json::object &val)
			{
				out << room_id << " " << key << ": " << val << std::endl;
				return true;
			}
		};

		const m::user::room_tags room_tags
		{
			user_id, room_id
		};

		if(tag)
			room_tags.get(tag, output);
		else
			room_tags.for_each(output);

		return true;
	}

	const m::user::rooms rooms
	{
		user_id
	};

	rooms.for_each(m::user::rooms::closure{[&user_id, &tag, &out]
	(const m::room &room, const string_view &membership)
	{
		const auto output
		{
			[&out, &room](const string_view &key, const json::object &val)
			{
				out << room.room_id << " " << key << ": " << val << std::endl;
				return true;
			}
		};

		const m::user::room_tags room_tags
		{
			user_id, room
		};

		if(tag)
			room_tags.get(tag, output);
		else
			room_tags.for_each(output);
	}});

	return true;
}

bool
console_cmd__user__room_tags__set(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "room_id", "tag", "content"
	}};

	const m::user::id &user_id
	{
		param.at("user_id")
	};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const string_view &tag
	{
		param.at("tag")
	};

	const json::object &content
	{
		param.at("content")
	};

	const m::user::room_tags room_tags
	{
		user_id, room_id
	};

	out << room_tags.set(tag, content)
	    << std::endl;

	return true;
}

bool
console_cmd__user__devices(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "device_id"
	}};

	const m::user::id &user_id
	{
		param.at("user_id")
	};

	const string_view &device_id
	{
		param.at("device_id", string_view{})
	};

	const m::user::devices devices
	{
		user_id
	};

	if(!device_id)
	{
		devices.for_each([&out]
		(const auto &event_idx, const string_view &device_id)
		{
			out << device_id << std::endl;
			return true;
		});

		return true;
	}

	devices.for_each(device_id, [&out, &devices, &device_id]
	(const auto &event_idx, const string_view &prop)
	{
		devices.get(std::nothrow, device_id, prop, [&out, &prop]
		(const auto &event_idx, const string_view &value)
		{
			out << prop << ": "
			    << value
			    << std::endl;
		});

		return true;
	});

	return true;
}

bool
console_cmd__user__devices__update(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "device_id", "deleted"
	}};

	const m::user::id &user_id
	{
		param.at("user_id")
	};

	const string_view &device_id
	{
		param.at("device_id")
	};

	const bool deleted
	{
		param["deleted"] == "deleted"
	};

	const m::user::devices devices
	{
		user_id
	};

	json::iov content;
	const json::iov::push push[]
	{
		{ content,  { "user_id",    user_id       } },
		{ content,  { "device_id",  device_id     } },
		{ content,  { "deleted",    deleted       } },
	};

	const bool broadcasted
	{
		m::user::devices::send(content)
	};

	out << "done" << std::endl;
	return true;
}

bool
console_id__device(opt &out,
                   const m::device::id &id,
                   const string_view &line)
{
	return true;
}

bool
console_cmd__user__ignores(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "other_id"
	}};

	const m::user::id &user_id
	{
		param.at("user_id")
	};

	const string_view &other_id
	{
		param["other_id"]
	};

	const m::user::ignores ignores
	{
		user_id
	};

	if(other_id)
	{
		const bool ignored(ignores.has(other_id));
		out << user_id << " is "
		    << (ignored? "" : "not ")
		    << "ignoring " << other_id
		    << std::endl;

		return true;
	}

	ignores.for_each([&out]
	(const m::user::id &user_id, const json::object &object)
	{
		out << user_id;
		if(!empty(object))
			out << " " << object;

		out << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__user__breadcrumbs(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id"
	}};

	const m::user::id &user_id
	{
		param.at("user_id")
	};

	const m::breadcrumbs breadcrumbs
	{
		user_id
	};

	breadcrumbs.for_each([&out]
	(const string_view &room_id)
	{
		out << room_id << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__user__viewing(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "idx"
	}};

	const m::user::id user_id
	{
		param.at("user_id")
	};

	const size_t idx
	{
		param.at("idx", 0UL)
	};

	const m::user user
	{
		user_id
	};

	const auto room_id
	{
		m::viewing(user, idx)
	};

	out << room_id << std::endl;
	return true;
}

bool
console_cmd__user__reading(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id"
	}};

	const m::user::id user_id
	{
		param.at("user_id")
	};

	const m::user::reading r
	{
		user_id
	};

	out
	<< r.room_id
	<< " "
	<< r.last
	<< " "
	<< r.last_ts
	<< " "
	<< r.full
	<< " "
	<< r.full_ots
	<< " "
	<< (r.currently_active? "active"_sv: "inactive"_sv)
	<< std::endl;
	return true;
}

bool
console_cmd__user__pushrules(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "scope", "kind", "ruleid"
	}};

	const m::user::id &user_id
	{
		param.at("user_id")
	};

	const auto &scope
	{
		param["scope"]
	};

	const auto &kind
	{
		param["kind"]
	};

	const auto &ruleid
	{
		param["ruleid"]
	};

	const m::user::pushrules pushrules
	{
		user_id
	};

	pushrules.for_each({scope, kind, ruleid}, [&out]
	(const auto &event_idx, const auto &path, const json::object &rule)
	{
		const auto &[scope, kind, ruleid]
		{
			path
		};

		out
		<< std::right << std::setw(10) << event_idx << " | "
		<< std::left << std::setw(10) << scope << " | "
		<< std::left << std::setw(10) << kind << " | "
		<< std::left << std::setw(36) << ruleid << "  "
		<< rule
		<< std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__user__pushers(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "pushkey",
	}};

	const m::user::id &user_id
	{
		param.at("user_id")
	};

	const auto &pushkey
	{
		param["pushkey"]
	};

	const m::user::pushers pushers
	{
		user_id
	};

	if(pushkey)
	{
		pushers.get(pushkey, [&out]
		(const auto &event_idx, const auto &key, const json::object &pusher)
		{
			out
			<< pusher
			<< std::endl;
		});

		return true;
	}

	pushers.for_each([&out]
	(const auto &event_idx, const auto &pushkey, const json::object &pusher)
	{
		out
		<< std::left << std::setw(40) << pushkey << " | "
		<< pusher
		<< std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__user__notifications(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "only", "room_id", "from", "to"
	}};

	const m::user::id &user_id
	{
		param.at("user_id")
	};

	const string_view &only
	{
		param["only"] == "*"?
			string_view{}:
			param["only"]
	};

	const m::room::id &room_id
	{
		!param["room_id"] || param["room_id"] == "*"?
			m::room::id{}:
			m::room::id{param["room_id"]}
	};

	const m::user::notifications notifications
	{
		user_id
	};

	m::user::notifications::opts opts;
	opts.only = only;
	opts.room_id = room_id;
	opts.from = param.at<m::event::idx>("from", 0UL);
	opts.to = param.at<m::event::idx>("to", 0UL);
	notifications.for_each(opts, [&out]
	(const auto &idx, const json::object &notification)
	{
		out
		<< std::right << std::setw(10) << idx << " | "
		<< notification
		<< std::endl;
		return true;
	});

	return true;
}

//
// users
//

bool
console_cmd__users(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"query"
	}};

	const auto query
	{
		param.at("query", string_view{})
	};

	m::users::opts opts
	{
		query
	};

	if(!query || query != "*")
		opts.hostpart = my_host();

	m::users::for_each(opts, [&out]
	(const m::user &user)
	{
		out << user.user_id << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__user__typing(opt &out, const string_view &line)
{
	m::typing::for_each([&out]
	(const m::typing::edu &event)
	{
		out << event << std::endl;
		return true;
	});

	return true;
}

//
// node
//

bool
console_cmd__node(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"node_id"
	}};

	return true;
}

bool
console_id__node(opt &out,
                 const string_view &id,
                 const string_view &line)
{
	return console_cmd__node(out, line);
}

bool
console_cmd__node__keys(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"node_id", "[limit]"
	}};

	const m::node &node
	{
		param.at("node_id")
	};

	auto limit
	{
		param.at(1, size_t(1))
	};

	const m::node::room node_room{node};
	const m::room::state state{node_room};
	state.for_each("ircd.key", m::event::closure_bool{[&out, &limit]
	(const m::event &event)
	{
		const m::keys keys
		{
			json::get<"content"_>(event)
		};

		pretty_oneline(out, keys);
		out << std::endl;
		return --limit;
	}});

	return true;
}

bool
console_cmd__node__key(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"node_id", "key_id"
	}};

	const m::node &node
	{
		param.at("node_id")
	};

	const auto &key_id
	{
		param[1]
	};

	const m::node::room node_room{node};
	node_room.get("ircd.key", [&out]
	(const m::event &event)
	{
		const m::keys key
		{
			json::get<"content"_>(event)
		};

		pretty(out, key);
		out << std::endl;
	});

	return true;
}

//
// feds
//

bool
console_cmd__feds__version(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id",
	}};

	const auto room_id
	{
		m::room_id(param.at(0))
	};

	m::feds::opts opts;
	opts.op = m::feds::op::version;
	opts.room_id = room_id;
	m::feds::execute(opts, [&out]
	(const auto &result)
	{
		out << (result.eptr? '-' : '+')
		    << " "
		    << std::setw(40) << std::left << result.origin
		    << " ";

		if(result.eptr)
			out << what(result.eptr);
		else
			out << string_view{result.object};

		out << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__feds__state(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id",
	}};

	const auto room_id
	{
		m::room_id(param.at(0))
	};

	const m::event::id::buf &event_id
	{
		param.count() > 1? param.at(1) : m::head(room_id)
	};

	std::forward_list<std::string> origins;
	std::map<std::string, std::forward_list<string_view>, std::less<>> grid;
	const auto closure{[&out, &grid, &origins](const auto &result)
	{
		if(result.eptr)
			return true;

		const json::array &auth_chain
		{
			result.object["auth_chain_ids"]
		};

		const json::array &pdus
		{
			result.object["pdu_ids"]
		};

		for(const auto &pdu_id : pdus)
		{
			const auto &event_id{unquote(pdu_id)};
			auto it
			{
				grid.lower_bound(event_id)
			};

			if(it == end(grid) || it->first != event_id)
				it = grid.emplace_hint(it, event_id, std::forward_list<string_view>{});

			origins.emplace_front(result.origin);
			it->second.emplace_front(origins.front());
		}

		return true;
	}};

	m::feds::opts opts;
	opts.op = m::feds::op::state;
	opts.timeout = out.timeout;
	opts.event_id = event_id;
	opts.room_id = room_id;
	opts.arg[0] = "ids";

	m::feds::execute(opts, closure);

	for(auto &p : grid)
	{
		p.second.sort();

		out << std::setw(64) << std::left << p.first << " : ";
		for(const auto &origin : p.second)
			out << " " << origin;

		out << std::endl;
	}

	return true;
}

bool
console_cmd__feds__event(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id", "room_id"
	}};

	const m::event::id event_id
	{
		param.at(0)
	};

	m::room::id::buf room_id;
	if(param["room_id"])
		room_id = m::room_id(param["room_id"]);

	if(!room_id)
		room_id = m::room_id(event_id);

	if(!room_id)
	{
		out << "Cannot find the room_id for this event; supply it as a paramter."
		    << std::endl;

		return true;
	}

	m::feds::opts opts;
	opts.op = m::feds::op::event;
	opts.room_id = room_id;
	opts.event_id = event_id;
	m::feds::execute(opts, [&out](const auto &result)
	{
		out << (result.eptr? '-': empty(result.object)? '?': '+')
		    << " "
		    << std::setw(40) << std::left << result.origin
		    << " "
		    ;

		if(result.eptr)
			out << " :" << what(result.eptr);

		out << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__feds__head(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "[user_id]"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::user::id &user_id
	{
		param["[user_id]"]?
			m::user::id{param["[user_id]"]}:
			m::user::id{}
	};

	m::feds::opts opts;
	opts.op = m::feds::op::head;
	opts.room_id = room_id;
	opts.user_id = user_id;
	opts.timeout = out.timeout;
	m::feds::execute(opts, [&out](const auto &result)
	{
		if(result.eptr)
		{
			out << std::setw(8) << std::right << 0 << " ";
			out << std::setw(3) << std::right << 0 << " ";
			out << std::setw(40) << std::left << result.origin << " ";
			out << what(result.eptr);
			out << std::endl;
			return true;
		}

		const json::object &event
		{
			result.object["event"]
		};

		const m::event::prev prev
		{
			event
		};

		for(ssize_t i(prev.prev_events_count() - 1); i >= 0; --i)
		{
			const auto &prev_event_id
			{
				prev.prev_event(i)
			};

			const m::event::fetch prev_event
			{
				std::nothrow, prev_event_id
			};

			out << std::setw(8) << std::right << event["depth"] << " ";
			out << std::setw(3) << std::right << i << " ";
			out << std::setw(40) << std::left << result.origin;
			if(prev_event.valid)
				out << pretty_oneline(prev_event);
			else
				out << string_view{prev_event_id};

			out << std::endl;
		}

		return true;
	});

	return true;
}

bool
console_cmd__feds__auth(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "event_id"
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const m::event::id &event_id
	{
		param.at("event_id")
	};

	m::feds::opts opts;
	opts.op = m::feds::op::auth;
	opts.room_id = room_id;
	opts.event_id = event_id;
	m::feds::execute(opts, [&out](const auto &result)
	{
		if(result.eptr)
			return true;

		const json::array auth_chain
		{
			result.object.at("auth_chain")
		};

		out << "+ " << std::setw(40) << std::left << result.origin;
		for(const json::object &auth_event : auth_chain)
		{
			out << " " << unquote(auth_event.at("event_id"));
		};

		out << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__feds__heads(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "[user_id]"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::user::id &user_id
	{
		param.at(1, m::me())
	};

	using closure_prototype = bool (const string_view &,
	                                std::exception_ptr,
	                                const json::object &);

	using prototype = void (const m::room::id &,
	                        const m::user::id &,
	                        const milliseconds &,
	                        const std::function<closure_prototype> &);

	static mods::import<prototype> feds__head
	{
		"federation_federation", "feds__head"
	};

	std::forward_list<std::string> origins;
	std::map<std::string, std::forward_list<string_view>, std::less<>> grid;

	feds__head(room_id, user_id, out.timeout, [&origins, &grid]
	(const string_view &origin, std::exception_ptr eptr, const json::object &event)
	{
		if(eptr)
			return true;

		const json::array &prev_events
		{
			event.at("prev_events")
		};

		const m::event::prev prev(prev_events);
		for(size_t i(0); i < prev.prev_events_count(); ++i)
		{
			const auto &event_id
			{
				prev.prev_event(i)
			};

			auto it
			{
				grid.lower_bound(event_id)
			};

			if(it == end(grid) || it->first != event_id)
				it = grid.emplace_hint(it, event_id, std::forward_list<string_view>{});

			origins.emplace_front(origin);
			it->second.emplace_front(origins.front());
		}

		return true;
	});

	for(auto &p : grid)
	{
		p.second.sort();

		out << std::setw(64) << std::left << p.first << " : ";
		for(const auto &origin : p.second)
			out << " " << origin;

		out << std::endl;
	}

	return true;
}

bool
console_cmd__feds__perspective(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "server_name", "key_id",
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const string_view &server_name
	{
		param.at(1)
	};

	const string_view &key_id
	{
		param.at(2)
	};

	const m::fed::key::server_key server_key
	{
		server_name, key_id
	};

	m::feds::opts opts;
	opts.op = m::feds::op::keys;
	opts.timeout = out.timeout;
	opts.room_id = room_id;
	opts.arg[0] = server_key.first;
	opts.arg[1] = server_key.second;
	m::feds::execute(opts, [&out](const auto &result)
	{
		out << std::setw(32) << trunc(result.origin, 32) << " :";

		if(result.eptr)
		{
			out << what(result.eptr)
			    << std::endl;

			return true;
		}

		const json::array &server_keys
		{
			result.object["server_keys"]
		};

		for(const json::object &server_key : server_keys)
		{
			const m::keys &key{server_key};
			out << key << std::endl;
		}

		return true;
	});

	return true;
}

bool
console_cmd__feds__backfill(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "[event_id]", "[limit]"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::event::id::buf &event_id
	{
		param.count() > 1? param.at(1) : head(room_id)
	};

	const size_t limit
	{
		param.at(2, size_t(4))
	};

	std::map<std::string, std::set<std::string>, std::less<>> grid;
	std::set<string_view> origins;

	m::feds::opts opts;
	opts.op = m::feds::op::backfill;
	opts.room_id = room_id;
	opts.event_id = event_id;
	opts.argi[0] = limit;
	m::feds::execute(opts, [&grid, &origins]
	(const auto &result)
	{
		if(result.eptr)
			return true;

		const json::array &pdus
		{
			result.object["pdus"]
		};

		for(const json::object &pdu : pdus)
		{
			const auto &event_id
			{
				unquote(pdu.at("event_id"))
			};

			auto it(grid.lower_bound(event_id));
			if(it == end(grid) || it->first != event_id)
				it = grid.emplace_hint(it, event_id, std::set<std::string>{});

			auto &set(it->second);
			const auto iit(set.emplace(result.origin));
			origins.emplace(*iit.first);
		}

		return true;
	});

	size_t i(0);
	for(const auto &p : grid)
		out << i++ << " " << p.first << std::endl;

	for(size_t j(0); j < i; ++j)
		out << "| " << std::left << std::setw(2) << j;
	out << "|" << std::endl;

	for(const auto &origin : origins)
	{
		for(const auto &p : grid)
			out << "| " << (p.second.count(origin)? '+' : ' ') << " ";
		out << "| " << origin << std::endl;
	}

	return true;
}

bool
console_cmd__feds__send(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id",
	}};

	const m::event::id event_id
	{
		param.at(0)
	};

	const m::event::fetch event
	{
		event_id
	};

	const json::value event_json
	{
		event.source
	};

	const m::txn::array pduv
	{
		&event_json, 1
	};

	const std::string content
	{
		m::txn::create(pduv)
	};

	char txnidbuf[64];
	const auto txnid
	{
		m::txn::create_id(txnidbuf, content)
	};

	m::feds::opts opts;
	opts.op = m::feds::op::send;
	opts.room_id = at<"room_id"_>(event);
	opts.arg[0] = txnid;
	opts.arg[1] = content;
	m::feds::execute(opts, [&out]
	(const auto &result)
	{
		out << (result.eptr? '-' : '+')
		    << " "
		    << std::setw(40) << std::left << result.origin
		    << " ";

		if(result.eptr)
			out << what(result.eptr);
		else
			out << string_view{result.object};

		out << std::endl;
		return true;
	});

	return true;
}

//
// fed
//

bool
console_cmd__fed__groups(opt &out, const string_view &line)
{
	const string_view node
	{
		token(line, ' ', 0)
	};

	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	m::user::id ids[8];
	string_view tok[8];
	const auto count{std::min(tokens(args, ' ', tok), size_t(8))};
	std::copy(begin(tok), begin(tok) + count, begin(ids));

	const unique_buffer<mutable_buffer> buf
	{
		32_KiB
	};

	m::fed::groups::publicised::opts opts;
	m::fed::groups::publicised request
	{
		node, vector_view<const m::user::id>(ids, count), buf, std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	const json::object response
	{
		request.in.content
	};

	out << string_view{response} << std::endl;
	return true;
}

bool
console_cmd__fed__rooms__complexity(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "remote"
	}};

	const auto room_id
	{
		m::room_id(param.at("room_id"))
	};

	const auto remote
	{
		param["remote"]
	};

	const unique_buffer<mutable_buffer> buf
	{
		16_KiB
	};

	m::fed::rooms::complexity::opts opts;
	opts.remote = remote;
	opts.dynamic = false;
	m::fed::rooms::complexity request
	{
		room_id, buf, std::move(opts)
	};

	const auto code
	{
		request.get(out.timeout)
	};

	const json::object response
	{
		request
	};

	out << string_view{response} << std::endl;
	return true;
}

bool
console_cmd__fed__head(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "remote", "user_id", "op"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const string_view remote
	{
		param.at(1, room_id.host())
	};

	/// Select a better user_id if possible
	const m::room room{room_id};
	m::user::id::buf user_id;
	if(param["user_id"])
		user_id = param["user_id"];

	if(!user_id)
		user_id = any_user(room, my_host(), "join");

	// Make another attempt to find an invited user because that carries some
	// value (this query is not as fast as querying join memberships).
	if(!user_id)
		user_id = any_user(room, my_host(), "invite");

	const unique_mutable_buffer buf
	{
		16_KiB
	};

	m::fed::make_join::opts opts;
	opts.remote = remote;
	m::fed::make_join request
	{
		room_id, user_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	const json::object &proto
	{
		request.in.content
	};

	if(param["op"] == "raw")
	{
		out << string_view{proto} << std::endl;
		return true;
	}

	const json::object event
	{
		proto["event"]
	};

	out << "VERSION "
	    << proto["room_version"]
	    << std::endl;

	out << "DEPTH   "
	    << event["depth"]
	    << std::endl;

	const m::event::auth auth{event};
	for(size_t i(0); i < auth.auth_events_count(); ++i)
	{
		const m::event::id &id
		{
			auth.auth_event(i)
		};

		out << "AUTH    " << id << " " << std::endl;
	}

	const m::event::prev prev{event};
	for(size_t i(0); i < prev.prev_events_count(); ++i)
	{
		const m::event::id &id
		{
			prev.prev_event(i)
		};

		out << "PREV    " << id << " " << std::endl;
	}

	return true;
}

bool
console_cmd__fed__send(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"remote", "event_id",
	}};

	const string_view remote
	{
		param.at(0)
	};

	const m::event::id &event_id
	{
		param.at(1)
	};

	const m::event::fetch event
	{
		event_id
	};

	const unique_mutable_buffer pdubuf
	{
		m::event::MAX_SIZE
	};

	const json::value pdu
	{
		json::stringify(mutable_buffer{pdubuf}, event)
	};

	const vector_view<const json::value> pdus
	{
		&pdu, &pdu + 1
	};

	const auto txn
	{
		m::txn::create(pdus)
	};

	thread_local char idbuf[128];
	const auto txnid
	{
		m::txn::create_id(idbuf, txn)
	};

	const unique_buffer<mutable_buffer> bufs
	{
		16_KiB
	};

	m::fed::send::opts opts;
	opts.remote = remote;
	m::fed::send request
	{
		txnid, const_buffer{txn}, bufs, std::move(opts)
	};

	request.wait(out.timeout);

	const auto code
	{
		request.get()
	};

	const json::object response
	{
		request
	};

	const m::fed::send::response resp
	{
		response
	};

	resp.for_each_pdu([&]
	(const m::event::id &event_id, const json::object &error)
	{
		out << remote << " ->" << txnid << " " << event_id << " ";
		if(empty(error))
			out << http::status(code) << std::endl;
		else
			out << string_view{error} << std::endl;
	});

	return true;
}

bool
console_cmd__fed__state(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "remote", "event_id|op", "op"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const string_view remote
	{
		param.at(1, room_id.host())
	};

	string_view event_id
	{
		param[2]
	};

	string_view op
	{
		param[3]
	};

	if(!op && event_id == "eval")
		std::swap(op, event_id);

	const m::event::id::buf head_buf
	{
		event_id?
			m::event::id::buf{}:
			m::head(std::nothrow, room_id)
	};

	if(!event_id)
		event_id = head_buf;

	// Used for out.head, out.content, in.head, but in.content is dynamic
	const unique_mutable_buffer buf
	{
		16_KiB
	};

	m::fed::state::opts opts;
	opts.remote = remote;
	opts.event_id = event_id;
	m::fed::state request
	{
		room_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	const json::object response
	{
		request
	};

	if(op == "raw")
	{
		out << string_view{response} << std::endl;
		return true;
	}

	const json::array &auth_chain
	{
		response["auth_chain"]
	};

	const json::array &pdus
	{
		response["pdus"]
	};

	if(op != "eval")
	{
		if(op != "auth")
		{
			out << "state at " << event_id << ":" << std::endl;
			for(const json::object &event : pdus)
				out << pretty_oneline(m::event{event}) << std::endl;
		}

		out << std::endl;
		if(op != "state")
		{
			out << "auth chain at " << event_id << ":" << std::endl;
			for(const json::object &event : auth_chain)
				out << pretty_oneline(m::event{event}) << std::endl;
		}

		return true;
	}

	m::vm::opts vmopts;
	vmopts.nothrows = -1;
	vmopts.phase.set(m::vm::phase::FETCH_PREV, false);
	vmopts.phase.set(m::vm::phase::FETCH_STATE, false);
	vmopts.notify_servers = false;
	vmopts.node_id = remote;

	m::vm::eval
	{
		auth_chain, vmopts
	};

	m::vm::eval
	{
		pdus, vmopts
	};

	return true;
}

bool
console_cmd__fed__state_ids(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "remote", "event_id"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const string_view remote
	{
		param.at(1, room_id.host())
	};

	string_view event_id
	{
		param[2]
	};

	// Used for out.head, out.content, in.head, but in.content is dynamic
	const unique_mutable_buffer buf
	{
		16_KiB
	};

	m::fed::state::opts opts;
	opts.remote = remote;
	opts.event_id = event_id;
	opts.ids_only = true;
	m::fed::state request
	{
		room_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	const json::object response
	{
		request
	};

	const json::array &auth_chain
	{
		response["auth_chain_ids"]
	};

	const json::array &pdus
	{
		response["pdu_ids"]
	};

	out << "AUTH:" << std::endl;
	for(const string_view &event_id : auth_chain)
		out << unquote(event_id) << std::endl;

	out << std::endl;

	out << "STATE:" << std::endl;
	for(const string_view &event_id : pdus)
		out << unquote(event_id) << std::endl;

	return true;
}

bool
console_cmd__fed__backfill(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "remote", "count", "event_id", "op"
	}};

	const auto &room_param
	{
		param.at("room_id")
	};

	const auto &room_id
	{
		m::room_id(room_param)
	};

	const string_view remote
	{
		param["remote"] && !lex_castable<uint>(param["remote"])?
			param["remote"]:
		valid(m::id::ROOM_ALIAS, room_param)?
			m::room::alias{room_param}.host():
			room_id.host()
	};

	const string_view &count
	{
		!lex_castable<uint>(param["remote"])?
			param.at("count", "32"_sv):
			param.at("remote")
	};

	string_view event_id
	{
		param["event_id"]
	};

	string_view op
	{
		param["op"]
	};

	if(!op && event_id == "eval")
		std::swap(op, event_id);

	else if(!event_id && !lex_castable<uint>(param["count"]))
		op = param["count"];

	// Used for out.head, out.content, in.head, but in.content is dynamic
	const unique_mutable_buffer buf
	{
		16_KiB
	};

	m::fed::backfill::opts opts;
	opts.remote = remote;
	opts.limit = lex_cast<size_t>(count);
	opts.event_id = event_id;

	m::fed::backfill request
	{
		room_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	const json::object response
	{
		request
	};

	if(op == "raw")
	{
		out << string_view{response} << std::endl;
		return true;
	}

	const json::array &pdus
	{
		response["pdus"]
	};

	if(op != "eval")
	{
		for(const json::object &event : pdus)
			out << pretty_oneline(m::event{event}) << std::endl;

		return true;
	}

	m::vm::opts vmopts;
	vmopts.nothrows = -1;
	vmopts.wopts.appendix[m::dbs::appendix::ROOM_HEAD_RESOLVE] = false;
	vmopts.wopts.appendix[m::dbs::appendix::ROOM_HEAD] = false;
	vmopts.phase.set(m::vm::phase::FETCH_PREV, false);
	vmopts.phase.set(m::vm::phase::FETCH_STATE, false);
	vmopts.node_id = remote;
	vmopts.notify_servers = false;
	m::vm::eval eval
	{
		pdus, vmopts
	};

	return true;
}

bool
console_cmd__fed__frontfill(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "remote", "earliest", "latest", "[limit]", "[min_depth]"
	}};

	const auto room_id
	{
		m::room_id(param.at(0))
	};

	const string_view remote
	{
		param.at(1, room_id.host())
	};

	const m::event::id &earliest
	{
		param["earliest"] == "*"?
			m::event::id{}:
			param.at(2, m::event::id{})
	};

	const m::event::id &latest
	{
		param["latest"] == "*"?
			m::event::id{}:
			param.at(3, m::event::id{})
	};

	const auto &limit
	{
		param.at(4, 32UL)
	};

	const auto &min_depth
	{
		param.at(5, 0UL)
	};

	m::fed::frontfill::opts opts;
	opts.remote = remote;
	opts.limit = limit;
	opts.min_depth = min_depth;
	const unique_buffer<mutable_buffer> buf
	{
		16_KiB
	};

	const m::fed::frontfill::span span
	{
		earliest, latest
	};

	m::fed::frontfill request
	{
		room_id, span, buf, std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	const json::array response
	{
		request
	};

	for(const json::object &event : response)
		out << pretty_oneline(m::event{event}) << std::endl;

	return true;
}

bool
console_cmd__fed__event(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id", "remote", "[op]", "[oparg]"
	}};

	const m::event::id &event_id
	{
		param.at("event_id")
	};

	const string_view &remote
	{
		param["remote"]?
			param["remote"]:
		event_id.host()?
			event_id.host():
			param.at("remote")
	};

	const string_view op
	{
		param[2]
	};

	const string_view oparg
	{
		param[3]
	};

	m::fed::event::opts opts;
	opts.remote = remote;
	opts.dynamic = false;
	const unique_buffer<mutable_buffer> buf
	{
		128_KiB
	};

	m::fed::event request
	{
		event_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	// Use this option to debug from the actual http response
	// content sent from the remote without any further action.
	if(has(op, "noparse"))
	{
		out << string_view{request.in.content} << std::endl;
		return true;
	}

	if(has(op, "parse"))
	{
		out << json::object{request.in.content} << std::endl;
		return true;
	}

	const json::object response
	{
		request
	};

	if(has(op, "raw"))
	{
		out
		<< string_view{response}
		<< std::endl;
		return true;
	}

	if(has(op, "essential"))
	{
		const unique_mutable_buffer buf
		{
			m::event::MAX_SIZE
		};

		out << m::essential(response, buf) << std::endl;
		return true;
	}

	if(has(op, "preimage"))
	{
		const unique_mutable_buffer buf
		{
			m::event::MAX_SIZE
		};

		const string_view preimage(m::event::preimage(buf, response));
		out << preimage << std::endl;
		return true;
	}

	m::event::id::buf _event_id;
	const m::event event
	{
		_event_id, response
	};

	if(has(op, "eval"))
	{
		m::vm::opts vmopts;
		vmopts.phase.set(m::vm::phase::FETCH_PREV, has(oparg, "prev"));
		vmopts.phase.set(m::vm::phase::FETCH_STATE, false);
		vmopts.phase.set(m::vm::phase::ACCESS, !has(oparg, "noacl"));
		vmopts.phase.set(m::vm::phase::CONFORM, !has(oparg, "noconform"));
		vmopts.phase.set(m::vm::phase::VERIFY, !has(oparg, "noverify"));
		vmopts.phase.set(m::vm::phase::AUTH_STATIC, !has(oparg, "noauth"));
		vmopts.phase.set(m::vm::phase::AUTH_RELA, !has(oparg, "noauth"));
		vmopts.phase.set(m::vm::phase::AUTH_PRES, !has(oparg, "noauth"));
		vmopts.phase.set(m::vm::phase::WRITE, !has(oparg, "nowrite"));
		vmopts.replays = has(oparg, "replay");
		vmopts.notify_servers = false;
		vmopts.node_id = remote;
		m::vm::eval eval
		{
			event, vmopts
		};

		return true;
	}

	m::pretty_detailed(out, event, 0UL);
	out << std::endl;
	return true;
}

bool
console_cmd__fed__public_rooms(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"remote", "limit", "search_term", "all_networks", "tpid"
	}};

	const string_view remote
	{
		param.at("remote")
	};

	const auto limit
	{
		param.at("limit", 32)
	};

	const auto search_term
	{
		param["search_term"]
	};

	const auto all_nets
	{
		param.at("all_networks", false)
	};

	const auto tpid
	{
		param["tpid"]
	};

	m::fed::public_rooms::opts opts;
	opts.limit = limit;
	opts.third_party_instance_id = tpid;
	opts.include_all_networks = all_nets;
	opts.search_term = search_term;
	const unique_buffer<mutable_buffer> buf
	{
		16_KiB
	};

	m::fed::public_rooms request
	{
		remote, buf, std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	const json::object response
	{
		request
	};

	const auto total_estimate
	{
		response.get<size_t>("total_room_count_estimate")
	};

	const json::string next_batch
	{
		response["next_batch"]
	};

	const json::array &rooms
	{
		response["chunk"]
	};

	for(const json::object &summary : rooms)
	{
		for(const auto &member : summary)
			out << std::setw(24) << member.first << " => " << member.second << std::endl;

		out << std::endl;
	}

	out << "total: " << total_estimate << std::endl;
	out << "next: " << next_batch << std::endl;
	return true;
}

bool
console_cmd__fed__auth(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "event_id", "remote", "op", "oparg"
	}};

	const auto room_id
	{
		m::room_id(param.at(0))
	};

	const m::event::id &event_id
	{
		param.at(1)
	};

	const string_view remote
	{
		param.at(2, event_id.host())
	};

	const string_view &op
	{
		param["op"]
	};

	m::fed::event_auth::opts opts;
	opts.remote = remote;
	opts.ids = op == "ids";
	opts.ids_only = op == "ids_only";
	const unique_buffer<mutable_buffer> buf
	{
		16_KiB
	};

	m::fed::event_auth request
	{
		room_id, event_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	const json::array &auth_chain
	{
		opts.ids_only?
			json::object(request.in.content).at("auth_chain_ids"):
			json::array(request)
	};

	if(opts.ids_only)
	{
		for(const json::string &event_id : auth_chain)
			out << event_id << std::endl;

		return true;
	}

	if(param["op"] == "raw")
	{
		for(const string_view &event : auth_chain)
			out << event << std::endl;

		return true;
	}

	if(param["op"] == "eval")
	{
		m::vm::opts vmopts;
		vmopts.node_id = opts.remote;
		vmopts.nothrows = -1;
		vmopts.wopts.appendix[m::dbs::appendix::ROOM_HEAD_RESOLVE] = false;
		vmopts.wopts.appendix[m::dbs::appendix::ROOM_HEAD] = false;
		vmopts.phase.set(m::vm::phase::FETCH_PREV, false);
		vmopts.phase.set(m::vm::phase::FETCH_STATE, false);
		vmopts.phase.set(m::vm::phase::FETCH_AUTH, false);
		vmopts.notify_servers = false;
		vmopts.auth = !has(param["oparg"], "noauth");
		vmopts.replays = has(param["oparg"], "replay");
		m::vm::eval eval
		{
			auth_chain, vmopts
		};

		return true;
	}

	std::vector<m::event> events(size(auth_chain));
	std::transform(begin(auth_chain), end(auth_chain), begin(events), []
	(const json::object &event) -> m::event
	{
		return event;
	});

	std::sort(begin(events), end(events));
	for(const auto &event : events)
		out << pretty_oneline(event) << std::endl;

	return true;
}

bool
console_cmd__fed__query_auth(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "event_id", "remote"
	}};

	const auto room_id
	{
		m::room_id(param.at(0))
	};

	const m::event::id &event_id
	{
		param.at(1)
	};

	const string_view remote
	{
		param.at(2, event_id.host())
	};

	m::fed::query_auth::opts opts;
	opts.remote = remote;
	const unique_buffer<mutable_buffer> buf
	{
		128_KiB
	};

	json::stack ost{buf};
	{
	    json::stack::object top{ost};
	    json::stack::array auth_chain
		{
			top, "auth_chain"
		};

		const m::room::auth::chain chain
		{
			m::index(event_id)
		};

		m::event::fetch event;
		chain.for_each([&auth_chain, &event]
		(const m::event::idx &event_idx)
		{
			if(seek(std::nothrow, event, event_idx))
				auth_chain.append(event);

			return true;
		});
	}

	const json::object content
	{
		ost.completed()
	};

	m::fed::query_auth request
	{
		room_id, event_id, content, buf + size(ost.completed()), std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	const json::object response{request};
	const json::array &auth_chain
	{
		response["auth_chain"]
	};

	const json::array &missing
	{
		response["missing"]
	};

	const json::object &rejects
	{
		response["rejects"]
	};

	out << "auth_chain: " << std::endl;
	for(const json::object &event : auth_chain)
		out << pretty_oneline(m::event{event}) << std::endl;

	out << std::endl;
	out << "missing: " << std::endl;
	for(const string_view &event_id : missing)
		out << event_id << std::endl;

	out << std::endl;
	out << "rejects: " << std::endl;
	for(const auto &member : rejects)
		out << member.first << ": " << member.second << std::endl;

	return true;
}

bool
console_cmd__fed__query__profile(opt &out, const string_view &line)
{
	const m::user::id &user_id
	{
		token(line, ' ', 0)
	};

	const string_view remote
	{
		token_count(line, ' ') > 1? token(line, ' ', 1) : user_id.host()
	};

	m::fed::query::opts opts;
	opts.remote = remote;

	const unique_mutable_buffer buf
	{
		8_KiB
	};

	m::fed::query::profile request
	{
		user_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	const auto code
	{
		request.get()
	};

	const json::object response
	{
		request
	};

	out << string_view{response} << std::endl;
	return true;
}

bool
console_cmd__fed__query__directory(opt &out, const string_view &line)
{
	const m::id::room_alias &room_alias
	{
		token(line, ' ', 0)
	};

	const string_view remote
	{
		token_count(line, ' ') > 1? token(line, ' ', 1) : room_alias.host()
	};

	m::fed::query::opts opts;
	opts.remote = remote;

	const unique_mutable_buffer buf
	{
		8_KiB
	};

	m::fed::query::directory request
	{
		room_alias, buf, std::move(opts)
	};

	request.wait(out.timeout);
	const auto code
	{
		request.get()
	};

	const json::object response
	{
		request
	};

	out << string_view{response} << std::endl;
	return true;
}

bool
console_cmd__fed__user__devices(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "remote"
	}};

	const m::user::id &user_id
	{
		param.at("user_id")
	};

	const string_view remote
	{
		param.at("remote", user_id.host())
	};

	m::fed::user::devices::opts opts;
	opts.remote = remote;

	const unique_buffer<mutable_buffer> buf
	{
		8_KiB
	};

	m::fed::user::devices request
	{
		user_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	const auto code
	{
		request.get()
	};

	const json::object response
	{
		request
	};

	const string_view stream_id
	{
		unquote(response["stream_id"])
	};

	const json::array &devices
	{
		response["devices"]
	};

	for(const json::object &device : devices)
		out << string_view{device} << std::endl;

	out << "-- " << size(devices) << " devices." << std::endl;
	return true;
}

bool
console_cmd__fed__user__keys__query(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "device_id", "remote"
	}};

	const m::user::id &user_id
	{
		param.at("user_id")
	};

	const string_view &device_id
	{
		param.at("device_id", string_view{})
	};

	const string_view remote
	{
		param.at("remote", user_id.host())
	};

	m::fed::user::opts opts;
	opts.remote = remote;

	const unique_buffer<mutable_buffer> buf
	{
		8_KiB
	};

	m::fed::user::keys::query request
	{
		user_id, device_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	const auto code
	{
		request.get()
	};

	const json::object response
	{
		request
	};

	const json::object &device_keys
	{
		response["device_keys"]
	};

	for(const auto &p : device_keys)
	{
		const m::user::id &user_id{p.first};
		out << user_id << ": " << std::endl;

		const json::object &devices{p.second};
		for(const auto &p : devices)
		{
			const auto &device_id{p.first};
			out << " " << device_id << ": " << std::endl;

			const m::device_keys &device{p.second};
			for_each(device, [&out]
			(const auto &key, const auto &val)
			{
				out << "  " << key
				    << ": " << val
				    << std::endl;
			});

			out << std::endl;
		}

		out << std::endl;
	}

	return true;
}

bool
console_cmd__fed__user__keys__claim(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "device_id", "algorithm", "remote"
	}};

	const m::user::id &user_id
	{
		param.at("user_id")
	};

	const string_view &device_id
	{
		param.at("device_id")
	};

	const string_view &algorithm
	{
		param.at("algorithm")
	};

	const string_view remote
	{
		param.at("remote", user_id.host())
	};

	m::fed::user::opts opts;
	opts.remote = remote;

	const unique_buffer<mutable_buffer> buf
	{
		8_KiB
	};

	m::fed::user::keys::claim request
	{
		user_id, device_id, algorithm, buf, std::move(opts)
	};

	request.wait(out.timeout);
	const auto code
	{
		request.get()
	};

	const json::object response
	{
		request
	};

	const json::object &one_time_keys
	{
		response["one_time_keys"]
	};

	out << one_time_keys << std::endl;
	return true;
}

bool
console_cmd__fed__key(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"remote",
	}};

	const auto &server_name
	{
		param.at(0)
	};

	const auto &key_id
	{
		param[1]
	};

	const unique_buffer<mutable_buffer> buf{16_KiB};
	m::fed::key::opts opts;
	m::fed::key::keys request
	{
		{server_name, key_id}, buf, std::move(opts)
	};

	request.wait(out.timeout);
	const auto code
	{
		request.get()
	};

	const json::object response
	{
		request
	};

	const m::keys key
	{
		response
	};

	pretty(out, key);
	out << std::endl;
	out << string_view{response};
	out << std::endl;

	if(!m::verify(key, std::nothrow))
		out << "SIGNATURE FAILIED" << std::endl;

	if(expired(key))
		out << "EXPIRED" << std::endl;

	return true;
}

bool
console_cmd__fed__key__query(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"remote", "[server_name,key_id]...",
	}};

	const auto requests
	{
		tokens_after(line, ' ', 0)
	};

	std::vector<std::pair<string_view, string_view>> r;
	tokens(requests, ' ', [&r]
	(const string_view &req)
	{
		r.emplace_back(split(req, ','));
	});

	m::fed::key::opts opts;
	opts.remote = param.at("remote");

	const unique_buffer<mutable_buffer> buf{24_KiB};
	m::fed::key::query request
	{
		r, buf, std::move(opts)
	};

	request.wait(out.timeout);
	const auto code
	{
		request.get()
	};

	const json::array keys
	{
		request
	};

	for(const json::object &key : keys)
	{
		const m::keys &k{key};
		out << k << std::endl;
	}

	return true;
}

bool
console_cmd__fed__version(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"remote"
	}};

	const auto remote
	{
		param.at("remote")
	};

	if(valid(m::id::ROOM, remote) || valid(m::id::ROOM_ALIAS, remote))
		return console_cmd__feds__version(out, line);

	m::fed::version::opts opts;
	opts.remote = remote;
	opts.dynamic = false;
	const unique_mutable_buffer buf
	{
		16_KiB
	};

	m::fed::version request
	{
		buf, std::move(opts)
	};

	request.wait(out.timeout);
	const auto code
	{
		request.get()
	};

	const json::object response
	{
		request
	};

	out << string_view{response} << std::endl;
	return true;
}

//
// file
//

bool
console_cmd__file__room(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"server|amalgam", "file"
	}};

	auto server
	{
		param.at(0)
	};

	auto file
	{
		param[1]
	};

	const m::media::mxc mxc
	{
		server, file
	};

	out << m::media::file::room_id(mxc) << std::endl;
	return true;
}

bool
console_cmd__file__download(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"server|file", "[remote]"
	}};

	const string_view &path
	{
		param.at("server|file")
	};

	const auto &server
	{
		split(path, '/').first
	};

	const auto &file
	{
		split(path, '/').second
	};

	const string_view &remote
	{
		param.at("[remote]", server)
	};

	const m::media::mxc mxc
	{
		server, file
	};

	const auto room_id
	{
		m::media::file::download(mxc, m::me(), remote)
	};

	out << room_id << std::endl;
	return true;
}

//
// vm
//

bool
console_cmd__vm(opt &out, const string_view &line)
{
	out
	<< std::right << std::setw(8) << "ID" << " "
	<< std::right << std::setw(4) << "CTX" << " "
	<< std::left << std::setw(8) << " " << " "
	<< std::left << std::setw(24) << "USER" << " "
	<< std::right << std::setw(4) << "PDUS" << " "
	<< std::right << std::setw(4) << "EVAL" << " "
	<< std::right << std::setw(4) << "EXEC" << " "
	<< std::right << std::setw(4) << "ERRS" << " "
	<< std::right << std::setw(8) << "PARENT" << " "
	<< std::right << std::setw(9) << "SEQUENCE" << " "
	<< std::left << std::setw(4) << "HOOK" << " "
	<< std::left << std::setw(10) << "PHASE" << " "
	<< std::right << std::setw(6) << "SIZE" << "  "
	<< std::right << std::setw(5) << "CELLS" << " "
	<< std::right << std::setw(8) << "DEPTH" << " "
	<< std::right << std::setw(5) << "VER" << " "
	<< std::left << std::setw(40) << "ROOM ID" << " "
	<< std::left << std::setw(60) << "EVENT ID" << " "
	<< std::left << std::setw(20) << "SENDER" << " "
	<< std::left << std::setw(20) << "TYPE" << " "
	<< std::left << std::setw(20) << "STATE_KEY" << " "
	<< std::endl;

	for(const auto *const &eval : m::vm::eval::list)
	{
		assert(eval);
		assert(eval->ctx);

		out
		<< std::right << std::setw(8) << eval->id << " "
		<< std::right << std::setw(4) << (eval->ctx? ctx::id(*eval->ctx) : 0UL) << " "
		<< std::left << std::setw(8) << (eval->ctx? trunc(ctx::name(*eval->ctx), 8) : string_view{}) << " "
		<< std::left << std::setw(24) << trunc(eval->opts->node_id?: eval->opts->user_id, 24) << " "
		<< std::right << std::setw(4) << eval->pdus.size() << " "
		<< std::right << std::setw(4) << eval->evaluated << " "
		<< std::right << std::setw(4) << eval->accepted << " "
		<< std::right << std::setw(4) << eval->faulted << " "
		<< std::right << std::setw(8) << (eval->parent? eval->parent->id : 0UL) << " "
		<< std::right << std::setw(9) << eval->sequence << " "
		<< std::right << std::setw(4) << (eval->hook? eval->hook->id(): 0U)  << " "
		<< std::left << std::setw(10) << trunc(reflect(eval->phase), 10) << " "
		<< std::right << std::setw(6) << (eval->txn? eval->txn->bytes() : 0UL) << "  "
		<< std::right << std::setw(5) << (eval->txn? eval->txn->size() : 0UL) << " "
		<< std::right << std::setw(8) << (eval->event_ && eval->event_id? long(json::get<"depth"_>(*eval->event_)) : -1L) << " "
		<< std::right << std::setw(5) << eval->room_version << " "
		<< std::left << std::setw(40) << trunc(eval->room_id, 40) << " "
		<< std::left << std::setw(60) << trunc(eval->event_id, 60) << " "
		<< std::left << std::setw(20) << trunc(eval->event_? json::get<"sender"_>(*eval->event_) : json::string{}, 20) << " "
		<< std::left << std::setw(20) << trunc(eval->event_? json::get<"type"_>(*eval->event_) : json::string{}, 20) << " "
		<< std::left << std::setw(20) << trunc(eval->event_? json::get<"state_key"_>(*eval->event_) : json::string{}, 20) << " "
		<< std::endl
		;
	}

	out << std::endl;

	out << "    retired " << std::left << std::setw(10) << m::vm::sequence::retired;
	out << "  committed " << std::left << std::setw(10) << m::vm::sequence::committed;
	out << "   uncommit " << std::left << std::setw(10) << m::vm::sequence::uncommitted;
	out << std::endl;

	out << "    pending " << std::left << std::setw(10) << m::vm::sequence::pending;
	out << "      evals " << std::left << std::setw(10) << m::vm::eval::id_ctr;
	out << "     spread " << std::left << std::setw(10) << m::vm::sequence::min()
	               << ' ' << std::left << std::setw(10) << m::vm::sequence::max();
	out << std::endl;

	out << "       inst " << std::left << std::setw(10) << size(m::vm::eval::list);
	out << "       exec " << std::left << std::setw(10) << m::vm::eval::executing;
	out << "     inject " << std::left << std::setw(10) << m::vm::eval::injecting;
	out << std::endl;

	return true;
}

//
// mc
//

bool
console_cmd__mc__versions(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"remote"
	}};

	const net::hostport remote
	{
		param.at("remote")
	};

	const unique_mutable_buffer buf
	{
		16_KiB
	};

	window_buffer wb{buf};
	http::request
	{
		wb, host(remote), "GET", "/_matrix/client/versions"
	};

	server::request request
	{
		remote,
		server::out  { wb.completed()     },
		server::in   { mutable_buffer{wb} },
	};

	const auto code
	{
		request.get(out.timeout)
	};

	const json::object response
	{
		request.in.content
	};

	out << string_view{response} << std::endl;
	return true;
}

bool
console_cmd__mc__register(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "password", "[remote]"
	}};

	const m::user::id &user_id
	{
		param.at("user_id")
	};

	const string_view &password
	{
		param.at("password")
	};

	const net::hostport remote
	{
		param.at("[remote]", user_id.host())
	};

	static const string_view uri
	{
		"/_matrix/client/r0/register?kind=user"
	};

	server::request::opts sopts;
	const unique_buffer<mutable_buffer> buf
	{
		16_KiB
	};

	window_buffer wb{buf};
	wb([&](mutable_buffer buf)
	{
		return json::stringify(buf, json::members
		{
			{ "username", user_id.localname() },
			{ "password", password            },
			{ "auth", json::members
			{
				{ "type", "m.login.dummy" }
			}}
		});
	});

	const string_view &content
	{
		wb.completed()
	};

	wb = mutable_buffer{wb};
	http::request
	{
		wb, host(remote), "POST", uri, size(content), "application/json"
	};

	server::out sout
	{
		wb.completed(), content
	};

	server::in sin
	{
		mutable_buffer{wb}
	};

	server::request request
	{
		remote, std::move(sout), std::move(sin)
	};

	request.wait(out.timeout);
	const auto code
	{
		request.get()
	};

	const json::object response
	{
		request.in.content
	};

	out << uint(code) << ": " << std::endl;
	out << string_view{response} << std::endl;
	return true;
}

bool
console_cmd__mc__register__available(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id", "[remote]"
	}};

	const m::user::id &user_id
	{
		param.at("user_id")
	};

	const net::hostport remote
	{
		param.at("[remote]", user_id.host())
	};

	server::request::opts sopts;
	const unique_buffer<mutable_buffer> buf
	{
		16_KiB
	};

	window_buffer wb{buf};
	wb([&user_id](const mutable_buffer &buf)
	{
		char urlencbuf[256];
		return fmt::sprintf
		{
			buf, "/_matrix/client/r0/register/available?username=%s",
			url::encode(urlencbuf, user_id.localname())
		};
	});

	const string_view uri{wb.completed()};
	wb = mutable_buffer{wb};
	http::request
	{
		wb, host(remote), "GET", uri
	};

	server::out sout{wb.completed()};
	server::in sin{mutable_buffer{wb}};
	server::request request
	{
		remote, std::move(sout), std::move(sin)
	};

	request.wait(out.timeout);
	const auto code
	{
		request.get()
	};

	const json::object response
	{
		request.in.content
	};

	out << uint(code) << ": " << string_view{response} << std::endl;
	return true;
}

//
// fetch
//

bool
console_cmd__fetch(opt &out, const string_view &line)
{
	m::fetch::for_each([&out]
	(const m::fetch::request &request)
	{
		out
		<< std::right << std::setw(10) << reflect(request.opts.op) << " "
		<< std::left << std::setw(64) << trunc(request.event_id, 64) << " "
		<< std::left << std::setw(40) << trunc(request.room_id, 40) << " "
		<< std::left << std::setw(32) << trunc(request.origin, 32) << " "
		<< std::left << "S:" << request.started << " "
		<< std::left << "A:" << request.attempted.size() << " "
		<< std::left << "E:" << bool(request.eptr) << " "
		<< std::left << "F:" << request.finished << " "
		<< std::endl
		;

		return true;
	});

	return true;
}

bool
console_cmd__fetch__event(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "event_id", "hint", "limit"
	}};

	const auto room_id
	{
		m::room_id(param.at("room_id"))
	};

	const m::event::id &event_id
	{
		m::event::id{param.at("event_id")}
	};

	const string_view hint
	{
		param["hint"]
	};

	const size_t limit
	{
		param.at("limit", 0UL)
	};

	m::fetch::opts opts;
	opts.op = m::fetch::op::event;
	opts.room_id = room_id;
	opts.event_id = event_id;
	opts.hint = hint;
	opts.attempt_limit = limit;
	auto future
	{
		m::fetch::start(opts)
	};

	const auto result
	{
		future.get()
	};

	out << "Received "
	    << event_id << " in "
	    << room_id
	    << std::endl
	    << std::endl
	    ;

	out << json::object{result}
	    << std::endl
	    ;

	return true;
}

bool
console_cmd__fetch__event__auth(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "event_id"
	}};

	const auto room_id
	{
		m::room_id(param.at("room_id"))
	};

	const m::event::id &event_id
	{
		m::event::id{param.at("event_id")}
	};

	m::fetch::opts opts;
	opts.op = m::fetch::op::auth;
	opts.room_id = room_id;
	opts.event_id = event_id;
	auto future
	{
		m::fetch::start(opts)
	};

	const auto result
	{
		future.get()
	};

	const json::object response
	{
		result
	};

	const json::array &auth_chain
	{
		response["auth_chain"]
	};

	out << "Received "
	    << auth_chain.size()
	    << " auth events for "
	    << event_id
	    << " in "
	    << room_id
	    << std::endl
	    << std::endl
	    ;

	for(const json::object &event : auth_chain)
		out << string_view{event} << std::endl;

	return true;
}

//
// synchron
//

bool
console_cmd__synchron(opt &out, const string_view &line)
{
	for(auto *const &data_p : m::sync::data::list)
	{
		const auto *const &client(data_p->client);
		if(client)
			out << client->loghead() << " | ";

		out << m::sync::loghead(*data_p) << " | ";
		out << std::endl;
	}

	return true;
}

bool
console_cmd__synchron__item(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"prefix"
	}};

	const auto prefix
	{
		param.at("prefix", ""_sv)
	};

	ircd::m::sync::for_each(prefix, [&out]
	(const auto &item)
	{
		out << item.name() << std::endl;
		return true;
	});

	return true;
}

//
// redact
//

bool
console_cmd__redact(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id", "sender", "reason"
	}};

	const params param_alt{line, " ",
	{
		"room_id", "type", "state_key", "sender", "reason"
	}};

	const m::room::id::buf room_id
	{
		m::room_id(param.at(0))
	};

	const m::room room
	{
		room_id
	};

	const auto state_idx
	{
		!valid(m::id::EVENT, param["event_id"])?
			room.get(param_alt["type"], param_alt["state_key"]):
			0UL
	};

	const m::event::id::buf redacts
	{
		valid(m::id::EVENT, param["event_id"])?
			param["event_id"]:
			m::event_id(state_idx)
	};

	const m::user::id &sender
	{
		state_idx && param_alt["sender"]?
			param_alt["sender"]:
		!state_idx && param["sender"]?
			param["sender"]:
			m::me()
	};

	const string_view reason
	{
		state_idx && param_alt["reason"]?
			param_alt["reason"]:
		!state_idx && param["reason"]?
			param["reason"]:
			string_view{}
	};

	const auto event_id
	{
		redact(room, sender, redacts, reason)
	};

	out
	<< redacts
	<< " redacted by " << sender
	<< " with " << event_id
	<< std::endl;
	return true;
}

//
// well-known
//

bool
console_cmd__well_known(opt &out, const string_view &line)
{
	const ctx::critical_assertion ca;

	out
	<< std::left << std::setw(8) << "ID"
	<< " "
	<< std::right << std::setw(8) << "REDIRS"
	<< " "
	<< std::right << std::setw(6) << "CODE"
	<< " "
	<< std::left << std::setw(40) << "TARGET"
	<< " "
	<< std::left << std::setw(40) << "CACHED"
	<< std::endl;

	for(const auto *const request : m::fed::well_known::request::list)
	{
		out
		<< std::left << std::setw(8) << request->id
		<< " "
		<< std::right << std::setw(8) << request->redirects
		<< " "
		<< std::right << std::setw(6) << uint(request->code)
		<< " "
		<< std::left << std::setw(40) << trunc(request->target, 40)
		<< " "
		<< std::left << std::setw(40) << trunc(request->m_server, 40)
		<< std::endl;
	}

	return true;
}

bool
console_cmd__well_known__matrix__server(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"remote" // ...
	}};

	std::forward_list
	<
		std::tuple
		<
			unique_mutable_buffer,
			string_view,
			ctx::future<string_view>
		>
	>
	reqs;
	tokens(line, ' ', [&reqs]
	(const string_view &remote)
	{
		m::fed::well_known::opts opts;
		opts.cache_check = false;
		opts.cache_result = false;
		unique_mutable_buffer buf{1_KiB};
		const mutable_buffer _buf{buf};
		reqs.emplace_front(std::make_tuple
		(
			std::move(buf),
			remote,
			m::fed::well_known::get(_buf, remote, opts)
		));
	});

	for(auto &[buf, tgt, req] : reqs)
		out
		<< std::right << std::setw(40) << trunc(tgt, 40)
		<< " => "
		<< std::left << string_view{req}
		<< std::endl;

	return true;
}

//
// bridge
//

bool
console_cmd__bridge(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"id"
	}};

	const string_view &id
	{
		param["id"]
	};

	if(!id)
	{
		m::bridge::config::for_each([&out]
		(const auto &event_idx, const auto &event, const m::bridge::config &config)
		{
			out
			<< json::get<"id"_>(config)
			<< std::endl;
			return true;
		});

		return true;
	}

	m::bridge::config::get(id, [&out]
	(const auto &event_idx, const auto &event, const m::bridge::config &config)
	{
		for(const auto &[key, val] : config.source)
			out
			<< std::right << std::setw(24) << key
			<< " : "
			<< std::left << val
			<< std::endl;
	});

	return true;
}

bool
console_cmd__bridge__exists(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"bridge_id", "mxid"
	}};

	const string_view &bridge_id
	{
		param.at("bridge_id")
	};

	const string_view &mxid
	{
		param.at("mxid")
	};

	std::string config;
	m::bridge::config::get(bridge_id, [&config]
	(const auto &, const auto &, const auto &object)
	{
		config = object.source;
	});

	bool exists {false};
	switch(m::sigil(mxid))
	{
		case m::id::USER:
		{
			exists = m::bridge::exists(m::bridge::config(config), m::user::id(mxid));
			break;
		}

		case m::id::ROOM_ALIAS:
		{
			exists = m::bridge::exists(m::bridge::config(config), m::room::alias(mxid));
			break;
		}

		default:
			throw error
			{
				"Invalid MXID argument"
			};
	}

	out
	<< mxid
	<< " "
	<< (exists? "exists"_sv: "does not exist")
	<< " on the "
	<< bridge_id
	<< " bridge."
	<< std::endl;
	return true;
}

bool
console_cmd__bridge__protocol(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"id", "protocol"
	}};

	const string_view &id
	{
		param["id"]
	};

	const string_view &protocol
	{
		param["protocol"]
	};

	m::bridge::config::get(id, [&out, &protocol]
	(const auto &event_idx, const auto &event, const m::bridge::config &config)
	{
		const unique_mutable_buffer buf
		{
			8_KiB
		};

		const json::object &info
		{
			m::bridge::protocol(buf, config, protocol)
		};

		out
		<< string_view{info}
		<< std::endl;
	});

	return true;
}

//
// icu
//

bool
console_cmd__icu(opt &out, const string_view &line)
{
	const unique_mutable_buffer buf
	{
		size(line) * 4
	};

	char32_t *const ch
	{
		reinterpret_cast<char32_t *>(data(buf))
	};

	const size_t count
	{
		icu::utf8::decode(ch, size(line), line)
	};

	char namebuf[64]; size_t li(0);
	for(size_t i(0); i < count; ++i, li += icu::utf8::length(ch[i]))
		out
		<< ' ' << std::dec << std::right << std::setw(6) << int(icu::block(ch[i]))
		<< ' ' << std::dec << std::right << std::setw(4) << int(icu::category(ch[i]))
		<< ' ' << std::dec << std::right << std::setw(2) << int(icu::utf8::length(ch[i]))
		<< ' ' << "U+" << std::hex << std::right << std::setw(6) << std::setfill('0') << uint32_t(ch[i]) << std::setfill(' ')
		<< ' ' << ' ' << icu::name(namebuf, ch[i])
		<< std::endl;

	return true;
}

//
// group
//

bool
console_cmd__group(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"group_id"
	}};

	return true;
}

bool
console_id__group(opt &out,
                  const m::id::group &id,
                  const string_view &line)
{
	return console_cmd__group(out, line);
}

//
// exec
//

bool
console_cmd__exec__list(opt &out, const string_view &line)
{
	for(const auto *const &exec : ircd::exec::list)
		out
		<< " " << exec->id
		<< " " << exec->pid
		<< " " << (!exec->pid? lex_cast(exec->code): "-"_sv)
		<< " " << exec->path
		<< std::endl;

	return true;
}

bool
console_cmd__exec(opt &out, const string_view &line)
{
	if(empty(line))
		return console_cmd__exec__list(out, line);

	if(ctx::name(ctx::cur()) != "console")
		throw ircd::error
		{
			"Command access denied to non-terminal administrators."
		};

	string_view argv[16];
	const auto argc
	{
		tokens(line, ' ', argv)
	};

	exec p
	{
		{ argv, argc }
	};

	const auto pid
	{
		p.run()
	};

	unique_mutable_buffer buf
	{
		4_KiB, 4_KiB
	};

	const string_view in
	{
		read(p, buf)
	};

	out << in << std::endl;
	return true;
}

//
// app
//

bool
console_cmd__app(opt &out, const string_view &line)
{
	out
	<< " " << std::right << std::setw(5) << "ID"
	<< " " << std::right << std::setw(10) << "EVENTID"
	<< " " << std::right << std::setw(8) << "PID"
	<< " " << std::right << std::setw(6) << "EXIT"
	<< " " << std::left << std::setw(40) << "ROOM"
	<< " " << "PATH"
	<< std::endl;

	for(const auto *const &app : ircd::m::app::list)
	{
		const auto room_id
		{
			m::room_id(app->event_idx)
		};

		out
		<< " " << std::right << std::setw(5) << app->child.id
		<< " " << std::right << std::setw(10) << app->event_idx
		<< " " << std::right << std::setw(8) << app->child.pid
		<< " " << std::right << std::setw(6) << (!app->child.pid? lex_cast(app->child.code): "---"_sv)
		<< " " << std::left << std::setw(40) << room_id
		<< " `" << app->argv.at(0) << "'"
		;

		if(app->child.eptr)
			out << " :" << what(app->child.eptr);

		out << std::endl;
	}

	return true;
}

bool
console_cmd__app__load(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "name"
	}};

	const auto room_id
	{
		m::room_id(param.at("room_id"))
	};

	const auto name
	{
		param.at("name")
	};

	const auto event_idx
	{
		m::room(room_id).get("ircd.app", name)
	};

	auto *const app
	{
		std::make_unique<m::app>(event_idx).release()
	};

	const auto pid
	{
		app->child.run()
	};

	out << "Started PID " << pid << "..." << std::endl;
	return true;
}

bool
console_cmd__app__unload(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "name"
	}};

	const auto room_id
	{
		m::room_id(param.at("room_id"))
	};

	const auto name
	{
		param["name"]
	};

	const auto event_idx
	{
		m::valid(m::id::EVENT, param.at("room_id"))?
			m::index(param.at("room_id")):
			m::room(room_id).get("ircd.app", name)
	};

	for(auto *const &app : m::app::list)
		if(app->event_idx == event_idx)
		{
			out << "Stopped PID " << app->child.pid << "..." << std::endl;
			app->child.join(15);
			delete app;
			return true;
		}

	out << "not found." << std::endl;
	return true;
}

bool
console_cmd__app__signal(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"signum", "event_id"
	}};

	const auto signum
	{
		param.at<uint>("signum")
	};

	const auto event_idx
	{
		lex_castable<m::event::idx>(param.at("event_id"))?
			lex_cast<m::event::idx>(param.at("event_id")):
			m::index(param.at("event_id"))
	};

	for(auto *const &app : m::app::list)
		if(app->event_idx == event_idx)
		{
			out << "Signal " << signum;
			if(!app->child.signal(signum))
				out << " failed";

			out << " to PID " << app->child.pid << std::endl;
			return true;
		}

	out << "not found." << std::endl;
	return true;
}
