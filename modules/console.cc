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

	return _console_command(opt, line);
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
// Help
//

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

	out << "Commands available: \n"
	    << std::endl;

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

			out << suffix << std::endl;
		}

		break;
	}

	return true;
}

//
// Test trigger stub
//

bool
console_cmd__test(opt &out, const string_view &line)
{
	ircd::test();
	return true;
}

bool
console_cmd__stringify(opt &out, const string_view &line)
{
	out << json::value{line} << std::endl;
	return true;
}

//
// Time cmd prefix (like /usr/bin/time)
//

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

//
// Derived commands
//

int console_command_numeric(opt &, const string_view &line);
bool console_id__user(opt &, const m::user::id &id, const string_view &line);
bool console_id__node(opt &, const m::node::id &id, const string_view &line);
bool console_id__room(opt &, const m::room::id &id, const string_view &line);
bool console_id__event(opt &, const m::event::id &id, const string_view &line);
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
	if(try_lex_cast<int>(id))
		return console_command_numeric(out, line);

	if(m::has_sigil(id)) switch(m::sigil(id))
	{
		case m::id::EVENT:
			return console_id__event(out, id, line);

		case m::id::ROOM:
			return console_id__room(out, id, line);

		case m::id::NODE:
			return console_id__node(out, id, line);

		case m::id::USER:
			return console_id__user(out, id, line);

		case m::id::ROOM_ALIAS:
		{
			const auto room_id{m::room_id(id)};
			return console_id__room(out, room_id, line);
		}

		default:
			break;
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

//
// Command by ID
//

//
// misc
//

bool
console_cmd__exit(opt &out, const string_view &line)
{
	return false;
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
	{
		out << "Debugging is not compiled in." << std::endl;
		return true;
	}

	const params param{line, " ",
	{
		"onoff"
	}};

	if(param["onoff"] == "on")
	{
		out << "Turning on debuglog..." << std::endl;
		while(!log::console_enabled(log::DEBUG))
			log::console_enable(log::DEBUG);

		return true;
	}
	else if(param["onoff"] == "off")
	{
		out << "Turning off debuglog..." << std::endl;
		log::console_disable(log::DEBUG);
		return true;
	}
	else if(log::console_enabled(log::DEBUG))
	{
		out << "Turning off debuglog..." << std::endl;
		log::console_disable(log::DEBUG);
		return true;
	} else {
		out << "Turning on debuglog..." << std::endl;
		while(!log::console_enabled(log::DEBUG))
			log::console_enable(log::DEBUG);

		return true;
	}
}

bool
console_cmd__demangle(opt &out, const string_view &line)
{
	out << ircd::demangle(line) << std::endl;
	return true;
}

//
// main
//

bool
console_cmd__restart(opt &out, const string_view &line)
{
	ircd::restart.set("true");
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

//
// log
//

bool
console_cmd__log(opt &out, const string_view &line)
{
	for(const auto *const &log : log::log::list)
		out << (log->snote? log->snote : '-')
		    << " " << std::setw(16) << std::left << log->name
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
		for(int i(0); i < num_of<log::level>(); ++i)
			if(i > RB_LOG_LEVEL)
				out << "[\033[1;40m-\033[0m] " << reflect(log::level(i)) << std::endl;
			else if(console_enabled(log::level(i)))
				out << "[\033[1;42m+\033[0m] " << reflect(log::level(i)) << std::endl;
			else
				out << "[\033[1;41m-\033[0m] " << reflect(log::level(i)) << std::endl;

		return true;
	}

	const int level
	{
		param.at<int>(0)
	};

	for(int i(0); i < num_of<log::level>(); ++i)
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
	return true;
}

bool
console_cmd__log__unmask(opt &out, const string_view &line)
{
	log::console_unmask(tokens<std::vector>(line, ' '));
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

	out << "info::kname                " << info::kname << std::endl;
	out << "info::kversion_str         " << info::kversion_str << std::endl;
	out << "info::glibc_version_str    " << info::glibc_version_str << std::endl;
	out << "ircd::boost_version_str    " << ircd::boost_version_str << std::endl;
	out << "db::version_str            " << db::version_str << std::endl;
	out << "nacl::version              " << nacl::version() << std::endl;
	out << "openssl::version           " << openssl::version().second << std::endl;
	out << "magic::version             " << magic::version() << std::endl;
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

	out << "RB_DATECODE                " << RB_DATECODE << std::endl;
	out << "RB_DATESTR                 " << RB_DATESTR << std::endl;
	out << "RB_DATE_CONFIGURED         " << RB_DATE_CONFIGURED << std::endl;
	out << std::endl;

	return true;
}

bool
console_cmd__info(opt &out, const string_view &line)
{
	info::dump();

	out << "Daemon information was written to the log."
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

	const hours uptime_h
	{
		uptime.count() / (60L * 60L)
	};

	const minutes uptime_m
	{
		(uptime.count() / 60L) % 60L
	};

	const minutes uptime_s
	{
		uptime.count() % 60L
	};

	out << "Running for ";

	if(uptime_h.count())
		out << uptime_h.count() << " hours ";

	if(uptime_m.count())
		out << uptime_m.count() << " minutes ";

	out << uptime_s.count() << " seconds." << std::endl;
	return true;
}

bool
console_cmd__date(opt &out, const string_view &line)
{
	out << ircd::time() << std::endl;
	out << ircd::time<milliseconds>() << std::endl;

	thread_local char buf[128];
	const auto now{ircd::now<system_point>()};
	out << timef(buf, now, ircd::localtime) << std::endl;
	out << timef(buf, now) << " (UTC)" << std::endl;

	return true;
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

//
// mem
//

bool
console_cmd__mem(opt &out, const string_view &line)
{
	auto &this_thread
	{
		ircd::allocator::profile::this_thread
	};

	out << "IRCd thread allocations:" << std::endl
	    << "alloc count:  " << this_thread.alloc_count << std::endl
	    << "freed count:  " << this_thread.free_count << std::endl
	    << "alloc bytes:  " << pretty(iec(this_thread.alloc_bytes)) << std::endl
	    << "freed bytes:  " << pretty(iec(this_thread.free_bytes)) << std::endl
	    << std::endl;

	thread_local char buf[1024];
	out << "malloc() information:" << std::endl
	    << allocator::info(buf) << std::endl
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

//
// prof
//

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
	prof::vg::dump(data(strlcpy(reason, line)));
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
	for(const auto &[name, item] : stats::items)
	{
		static constexpr size_t name_width {60};

		assert(item);
		const auto trunc_name(trunc(name, name_width));
		out << std::left << std::setw(name_width) << trunc_name;

		if(size(trunc_name) == name_width)
			out << "...";
		else
			out << "   ";

		out << "  "
		    << std::left << (*item)
		    << std::endl
		    ;
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
	    << " " << std::right << std::setw(10) << "CALLS"
	    << " " << std::right << std::setw(15) << "TOTAL CYCLES"
	    << " " << std::right << std::setw(15) << "LAST CYCLES"
	    << " " << std::right << std::setw(10) << "ALLOCS"
	    << " " << std::right << std::setw(10) << "FREES"
	    << " " << std::right << std::setw(26) << "ALLOCATED"
	    << " " << std::right << std::setw(26) << "FREED"
	    << " " << std::right << std::setw(8) << "QUEUED"
	    << " " << std::right << std::setw(8) << "FAULTS"
	    << std::endl
	    ;

	const auto stats_output{[&out]
	(const auto &s)
	{
		thread_local char pbuf[64];
		out << " " << std::right << std::setw(10) << s.calls
		    << " " << std::right << std::setw(15) << s.slice_total
		    << " " << std::right << std::setw(15) << s.slice_last
		    << " " << std::right << std::setw(10) << s.allocs
		    << " " << std::right << std::setw(10) << s.frees
		    << " " << std::right << std::setw(26) << pretty(iec(s.alloc_bytes))
		    << " " << std::right << std::setw(26) << pretty(iec(s.free_bytes))
		    << " " << std::right << std::setw(8) << s.queued
		    << " " << std::right << std::setw(8) << s.faults
		    ;
	}};

	struct ios::descriptor::stats total;
	for(const auto *const &descriptor : ios::descriptor::list)
	{
		assert(descriptor);
		const auto &d(*descriptor);

		assert(d.stats);
		const auto &s(*d.stats);
		total += s;

		out << std::left << std::setw(3) << d.id
		    << " " << std::left << std::setw(48) << d.name;
		stats_output(s);
		out << std::endl;
	}

	out << std::endl;
	out << std::left << std::setw(3) << '-'
	    << " " << std::left << std::setw(48) << "TOTAL";
	stats_output(total);
	out << std::endl;
	return true;
}

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

	thread_local char val[4_KiB];
	for(const auto &item : conf::items)
	{
		if(prefix && !startswith(item.first, prefix))
			continue;

		out
		<< std::setw(64) << std::left << std::setfill('_') << item.first
		<< " " << item.second->get(val) << "\033[0m"
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

	using prototype = m::event::id::buf (const m::user::id &,
	                                     const string_view &key,
	                                     const string_view &val);

	static mods::import<prototype> set_conf_item
	{
		"s_conf", "set_conf_item"
	};

	const auto event_id
	{
		set_conf_item(m::me, key, val)
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
		param.at(0)
	};

	thread_local char val[4_KiB];
	for(const auto &item : conf::items)
	{
		if(item.first != key)
			continue;

		out << std::setw(48) << std::right << item.first
		    << " = " << item.second->get(val)
		    << std::endl;

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
		"prefix", "force"
	}};

	using prototype = void (const string_view &, const bool &);
	static mods::import<prototype> rehash_conf
	{
		"s_conf", "rehash_conf"
	};

	string_view prefix
	{
		param.at("prefix", "*"_sv)
	};

	if(prefix == "*")
		prefix = string_view{};

	const bool force
	{
		param["force"] == "force"
	};

	rehash_conf(prefix, force);

	out << "Saved runtime conf items"
	    << (prefix? " with the prefix "_sv : string_view{})
	    << (prefix? prefix : string_view{})
	    << " from the current state into !conf:" << my_host()
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

	using prototype = void (const string_view &);
	static mods::import<prototype> default_conf
	{
		"s_conf", "default_conf"
	};

	string_view prefix
	{
		param.at("prefix", "*"_sv)
	};

	if(prefix == "*")
		prefix = string_view{};

	default_conf(prefix);

	out << "Set runtime conf items"
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
console_cmd__conf__reload(opt &out, const string_view &line)
{
	using prototype = void ();
	static mods::import<prototype> reload_conf
	{
		"s_conf", "reload_conf"
	};

	reload_conf();
	out << "Updated any runtime conf items"
	    << " from the current state in !conf:" << my_host()
	    << std::endl;

	return true;
}

bool
console_cmd__conf__reset(opt &out, const string_view &line)
{
	using prototype = void ();
	static mods::import<prototype> refresh_conf
	{
		"s_conf", "refresh_conf"
	};

	refresh_conf();
	out << "All conf items which execute code upon a change"
	    << " have done so with the latest runtime value."
	    << std::endl;

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
		out << std::setw(24) << std::left << site->name()
		    << std::endl;

		out << string_view{site->feature}
		    << std::endl;

		for(const auto &hookp : site->hooks)
			out << (hookp->registered? '+' : '-')
			    << " " << string_view{hookp->feature}
			    << std::endl;

		out << std::endl;
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
		param.at("path", string_view{})
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
			out << name << " unloaded." << std::endl;
		else
			out << name << " is not loaded." << std::endl;
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

	bool cont{true};
	for(size_t i(0); i < param.count() && cont; ++i)
		cont = ctx::for_each([&](auto &ctx)
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

	bool cont {true};
	for(size_t i(0); i < param.count() && cont; ++i)
		cont = ctx::for_each([&](auto &ctx)
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
	    << "LIMIT"
	    << " "
	    << std::setw(6)
	    << "PCT"
	    << " "
	    << ":NAME"
	    << std::endl;

	ctx::for_each([&out](auto &ctx)
	{
		out << std::setw(5) << std::right << id(ctx);
		out << " "
		    << (started(ctx)? 'A' : '-')
		    << (finished(ctx)? 'F' : '-')
		    << (termination(ctx)? 'T' : '-')
		    << (interruptible(ctx)? '-' : 'N')
		    << (waiting(ctx)? 'S' : '-')
		    << (!running(ctx) && notes(ctx)? 'Q' : '-')
		    << (interruption(ctx)? 'I' : '-')
		    << (running(ctx)? 'R' : '-')
		    ;

		out << " "
		    << std::setw(8) << std::right << epoch(ctx);

		out << " "
		    << std::setw(5) << std::right << notes(ctx);

		out << " "
		    << std::setw(15) << std::right << cycles(ctx);

		const long double total_cyc(ctx::prof::get(ctx, ctx::prof::event::CYCLES));
		const auto tsc_pct
		{
			total_cyc > 0.0? (cycles(ctx) / total_cyc) : 0.0L
		};

		out << " "
		    << std::setw(5) << std::right << std::fixed << std::setprecision(2) << (tsc_pct * 100)
		    << "%";

		thread_local char pbuf[32];
		out << " "
		    << std::setw(25) << std::right << pretty(pbuf, iec(stack_at(ctx)));

		out << " "
		    << std::setw(25) << std::right << pretty(pbuf, iec(stack_max(ctx)));

		const auto stack_pct
		{
			stack_at(ctx) / (long double)stack_max(ctx)
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

	for(const auto &name : db::compressions)
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

	const auto level
	{
		param.at(2, -1)
	};

	const auto begin
	{
		param[3]
	};

	const auto end
	{
		param[4]
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
		begin? try_lex_cast<uint64_t>(begin) : false
	};

	const uint64_t integers[2]
	{
		integer? lex_cast<uint64_t>(begin) : 0,
		integer && end? lex_cast<uint64_t>(end) : 0
	};

	const std::pair<string_view, string_view> range
	{
		integer? byte_view<string_view>(integers[0]) : begin,
		integer && end? byte_view<string_view>(integers[1]) : end,
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

		out << std::left << std::setw(48) << std::setfill('_') << name
		    << " " << val
		    << std::endl;
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
		    << " " << std::setw(9) << val.hits << " hit "
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
		size_t usage;
		size_t pinned;
		size_t capacity;
		size_t hits;
		size_t misses;
		size_t inserts;
		size_t inserts_bytes;

		stats &operator+=(const stats &b)
		{
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
		const auto usage(db::usage(cache(database)));
		const auto pinned(db::pinned(cache(database)));
		const auto capacity(db::capacity(cache(database)));
		const auto usage_pct
		{
			capacity > 0.0? (double(usage) / double(capacity)) : 0.0L
		};

		const auto hits(db::ticker(cache(database), db::ticker_id("rocksdb.block.cache.hit")));
		const auto misses(db::ticker(cache(database), db::ticker_id("rocksdb.block.cache.miss")));
		const auto inserts(db::ticker(cache(database), db::ticker_id("rocksdb.block.cache.add")));
		const auto inserts_bytes(db::ticker(cache(database), db::ticker_id("rocksdb.block.cache.data.bytes.insert")));

		out << std::left
		    << std::setw(32) << "ROW"
		    << std::right
		    << " "
		    << std::setw(7) << "PCT"
		    << " "
		    << std::setw(9) << "HITS"
		    << " "
		    << std::setw(9) << "MISSES"
		    << " "
		    << std::setw(9) << "INSERT"
		    << " "
		    << std::setw(26) << "CACHED"
		    << " "
		    << std::setw(26) << "CAPACITY"
		    << " "
		    << std::setw(26) << "INSERT TOTAL"
		    << " "
		    << std::setw(20) << "LOCKED"
		    << " "
		    << std::endl;

		out << std::left
		    << std::setw(32) << "*"
		    << std::right
		    << " "
		    << std::setw(6) << std::right << std::fixed << std::setprecision(2) << (usage_pct * 100)
		    << "%"
		    << " "
		    << std::setw(9) << hits
		    << " "
		    << std::setw(9) << misses
		    << " "
		    << std::setw(9) << inserts
		    << " "
		    << std::setw(26) << std::right << pretty(iec(usage))
		    << " "
		    << std::setw(26) << std::right << pretty(iec(capacity))
		    << " "
		    << std::setw(26) << std::right << pretty(iec(inserts_bytes))
		    << " "
		    << std::setw(20) << std::right << pretty(iec(pinned))
		    << " "
		    << std::endl
		    << std::endl;

		// Now set the colname to * so the column total branch is taken
		// below and we output that line too.
		colname = "*";
	}

	out << std::left
	    << std::setw(32) << "COLUMN"
	    << std::right
	    << " "
	    << std::setw(7) << "PCT"
	    << " "
	    << std::setw(9) << "HITS"
	    << " "
	    << std::setw(9) << "MISSES"
	    << " "
	    << std::setw(9) << "INSERT"
	    << " "
	    << std::setw(26) << "CACHED"
	    << " "
	    << std::setw(26) << "CAPACITY"
	    << " "
	    << std::setw(26) << "INSERT TOTAL"
	    << " "
	    << std::setw(20) << "LOCKED"
	    << " "
	    << std::endl;

	const auto output{[&out]
	(const string_view &column_name, const stats &s)
	{
		const auto pct
		{
			s.capacity > 0.0? (double(s.usage) / double(s.capacity)) : 0.0L
		};

		out << std::setw(32) << std::left << column_name
		    << std::right
		    << " "
		    << std::setw(6) << std::right << std::fixed << std::setprecision(2) << (pct * 100)
		    << '%'
		    << " "
		    << std::setw(9) << s.hits
		    << " "
		    << std::setw(9) << s.misses
		    << " "
		    << std::setw(9) << s.inserts
		    << " "
		    << std::setw(26) << std::right << pretty(iec(s.usage))
		    << " "
		    << std::setw(26) << std::right << pretty(iec(s.capacity))
		    << " "
		    << std::setw(26) << pretty(iec(s.inserts_bytes))
		    << " "
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
console_cmd__db__stats(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"dbname", "column"
	}};

	return console_cmd__db__prop(out, fmt::snstringf
	{
		1024, "%s %s rocksdb.stats",
		param.at(0),
		param.at(1)
	});
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
	    << std::right
	    << "  " << std::setw(23) << "key range"
	    << "  " << std::setw(23) << "sequence number"
	    << "  " << std::setw(3) << "lev"
	    << std::left
	    << "  " << std::setw(24) << "file size"
	    << std::right
	    << "  " << std::setw(6) << "idxs"
	    << "  " << std::setw(9) << "blocks"
	    << "  " << std::setw(9) << "entries"
	    << "  " << std::setw(4) << "cfid"
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

	out << std::left << std::setfill(' ')
	    << std::setw(12) << f.name
	    << "  " << std::setw(32) << std::left << timestr(f.created, ircd::localtime)
	    << "  " << std::setw(10) << std::right << min_key << " : " << std::setw(10) << std::left << max_key
	    << "  " << std::setw(10) << std::right << f.min_seq << " : " << std::setw(10) << std::left << f.max_seq
	    << "  " << std::setw(3) << std::right << f.level
	    << "  " << std::setw(24) << std::left << pretty(iec(f.size))
	    << "  " << std::setw(6) << std::right << f.index_parts
	    << "  " << std::setw(9) << std::right << f.data_blocks
	    << "  " << std::setw(9) << std::right << f.entries
	    << "  " << std::setw(4) << std::right << f.cfid
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
	close_auto("column ID", f.cfid);
	close_auto("column", f.column);
	close_auto("format", f.format);
	close_auto("version", f.version);
	close_auto("comparator", f.comparator);
	close_auto("merge operator", f.merge_operator);
	close_auto("prefix extractor", f.prefix_extractor);
	close_size("file size", f.size);
	close_auto("creation", timestr(f.created, ircd::localtime));
	close_auto("level", f.level);
	close_auto("lowest sequence", f.min_seq);
	close_auto("highest sequence", f.max_seq);
	close_auto("lowest key", min_key);
	close_auto("highest key", max_key);
	close_auto("fixed key length", f.fixed_key_len);
	close_auto("range deletes", f.range_deletes);
	close_auto("compacting", f.compacting? "yes"_sv : "no"_sv);
	close_auto("compression", f.compression);
	close_auto("", "");

	const auto blocks_size{f.keys_size + f.values_size};
	const auto index_size{f.index_size + f.top_index_size};
	const auto overhead_size{index_size + f.filter_size};
	const auto file_size{overhead_size + f.data_size};

	close_size("size", file_size);
	close_size("head size", overhead_size);
	close_size("data size", f.data_size);
	close_size("data blocks average size", f.data_size / double(f.data_blocks));
	close_auto("data compression percent", 100 - 100.0L * (f.data_size / double(blocks_size)));
	close_auto("", "");

	close_size("index size", index_size);
	close_size("index root size", f.top_index_size);
	close_auto("index data blocks", f.index_parts);
	close_size("index data size", f.index_size);
	close_size("index data block average size", f.index_size / double(f.index_parts));
	close_size("index data average per-key", f.index_size / double(f.entries));
	close_size("index data average per-block", f.index_size / double(f.data_blocks));
	close_auto("index root percent of index", 100.0 * (f.top_index_size / double(f.index_size)));
	close_auto("index data percent of keys", 100.0 * (f.index_size / double(f.keys_size)));
	close_auto("index data percent of values", 100.0 * (f.index_size / double(f.values_size)));
	close_auto("index data percent of data", 100.0 * (f.index_size / double(f.data_size)));
	close_auto("", "");

	close_auto("filter", f.filter);
	close_size("filter size", f.filter_size);
	close_auto("filter average per-key", f.filter_size / double(f.entries));
	close_auto("filter average per-block", f.filter_size / double(f.data_blocks));
	close_auto("filter percent of index", 100.0 * (f.filter_size / double(f.index_size)));
	close_auto("filter percent of data", 100.0 * (f.filter_size / double(f.data_size)));
	close_auto("filter percent of keys", 100.0 * (f.filter_size / double(f.keys_size)));
	close_auto("filter percent of values", 100.0 * (f.filter_size / double(f.values_size)));
	close_auto("", "");

	close_auto("blocks", f.data_blocks);
	close_size("blocks size", blocks_size);
	close_size("blocks average size", blocks_size / double(f.data_blocks));
	close_auto("blocks percent of keys", 100.0 * (f.data_blocks / double(f.entries)));
	close_auto("", "");

	close_auto("keys", f.entries);
	close_size("keys size", f.keys_size);
	close_size("keys average size", f.keys_size / double(f.entries));
	close_auto("keys percent of values", 100.0 * (f.keys_size / double(f.values_size)));
	close_auto("keys average per-block", f.entries / double(f.data_blocks));
	close_auto("keys average per-index", f.entries / double(f.index_parts));
	close_auto("", "");

	close_auto("values", f.entries);
	close_size("values size", f.values_size);
	close_size("values average size", f.values_size / double(f.entries));
	close_size("values average size per-block", f.values_size / double(f.data_blocks));
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

	if(colname == "*")
	{
		const auto fileinfos
		{
			db::database::sst::info::vector(database)
		};

		_print_sst_info_header(out);
		for(const auto &fileinfo : fileinfos)
			_print_sst_info(out, fileinfo);

		out << "-- " << fileinfos.size() << " files"
		    << std::endl;

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

	const db::database::sst::info::vector vector{column};
	_print_sst_info_header(out);
	for(const auto &info : vector)
		_print_sst_info(out, info);

	const size_t total_bytes
	{
		std::accumulate(begin(vector), end(vector), size_t(0), []
		(auto ret, const auto &info)
		{
			return ret += info.size;
		})
	};

	out << "-- " << pretty(iec(total_bytes))
	    << " in " << vector.size() << " files"
	    << std::endl;

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
			try_lex_cast<ulong>(param["key"])
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
				std::get<delta.VAL>(delta)
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
				std::get<delta.KEY>(delta)
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
			    << std::setw(8)   << std::left   << reflect(std::get<delta.OP>(delta)) << " "
			    << std::setw(18)  << std::right  << std::get<delta.COL>(delta) << " "
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
	const auto dbname
	{
		token(line, ' ', 0)
	};

	auto &database
	{
		db::database::get(dbname)
	};

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
			replace(lstrip(lstrip(path, fs::path(fs::DB)), '/'), "/", ":")
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
		out << std::endl;
		_print_sst_info_header(out);
		const db::database::sst::info::vector v{c};
		for(const auto &info : v)
			_print_sst_info(out, info);
	}
	else
	{
		out << std::endl;
		for(const auto &column : d.columns)
		{
			const auto explain
			{
				split(db::describe(*column).explain, '\n').first
			};

			out << std::left << std::setfill (' ') << std::setw(3) << db::id(*column)
			    << " " << std::setw(20) << db::name(*column)
			    << " " << std::setw(24) << pretty(iec(db::bytes(*column)))
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
{
	if(out.html)
		return html__peer(out, line);

	const auto print{[&out]
	(const auto &host, const auto &peer)
	{
		using std::setw;
		using std::left;
		using std::right;

		const net::ipport &ipp{peer.remote};
		out << setw(40) << left << host;

		if(ipp)
		    out << ' ' << setw(40) << left << ipp;
		else
		    out << ' ' << setw(40) << left << " ";

		out << " " << setw(2) << right << peer.link_count()     << " L"
		    << " " << setw(2) << right << peer.tag_count()      << " T"
		    << " " << setw(2) << right << peer.tag_committed()  << " TC"
		    << " " << setw(9) << right << peer.write_size()     << " UP Q"
		    << " " << setw(9) << right << peer.read_size()      << " DN Q"
		    << " " << setw(9) << right << peer.write_total()    << " UP"
		    << " " << setw(9) << right << peer.read_total()     << " DN"
		    ;

		if(peer.err_has() && peer.err_msg())
			out << "  :" << peer.err_msg();
		else if(peer.err_has())
			out << "  <unknown error>"_sv;

		out << std::endl;
	}};

	const params param{line, " ",
	{
		"[hostport]", "[all]"
	}};

	const auto &hostport
	{
		param[0]
	};

	const bool all
	{
		has(line, "all")
	};

	if(hostport && hostport != "all")
	{
		auto &peer
		{
			server::find(hostport)
		};

		print(peer.hostcanon, peer);
		return true;
	}

	for(const auto &p : server::peers)
	{
		const auto &host{p.first};
		const auto &peer{*p.second};
		if(peer.err_has() && !all)
			continue;

		print(host, peer);
	}

	return true;
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

	const net::hostport hp
	{
		token(line, ' ', 0)
	};

	const auto cleared
	{
		server::errclear(hp)
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

	const auto &hostport
	{
		param.at(0)
	};

	auto &peer
	{
		server::find(hostport)
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
	<< std::left   << std::setw(8)  << "PEER" << "  "
	<< std::left   << std::setw(8)  << "LINK" << "  "
	<< std::left   << std::setw(8)  << "TAG" << "  "
	<< std::right  << std::setw(32) << "PEER NAME" << "  "
	<< std::left   << std::setw(32) << "REMOTE ADDRESS" << "  "
	<< std::right  << std::setw(7)  << "METHOD" << "  "
	<< std::left   << std::setw(64) << "PATH" << "  "
	<< std::left   << std::setw(4)  << "CODE" << "  "
	<< std::endl
	;

	const auto each{[&out]
	(const server::peer &peer, const server::link &link, const server::request &request)
	{
		const auto out_head(request.out.gethead(request));
		if(!out_head.method || !out_head.path)
			return true;

		thread_local char rembuf[128];
		const string_view &remote
		{
			link.socket?
				string(rembuf, remote_ipport(*link.socket)):
				"<no socket>"_sv
		};

		out
		<< std::left   << std::setw(8)  << peer.id << "  "
		<< std::left   << std::setw(8)  << link.id << "  "
		<< std::left   << std::setw(8)  << id(request) << "  "
		<< std::right  << std::setw(32) << trunc(peer.hostcanon, 32) << "  "
		<< std::left   << std::setw(32) << trunc(remote, 32) << "  "
		<< std::right  << std::setw(7)  << out_head.method << "  "
		<< std::left   << std::setw(64) << trunc(out_head.path, 64) << "  "
		;

		if(request.tag)
			out << std::setw(4) << uint(request.tag->state.status) << " ";
		else
			out << std::setw(4) << "CNCL" << " ";

		if(request.tag && request.tag->state.status)
			out << http::status(request.tag->state.status) << " ";

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
console_cmd__net__host(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"hostport", "qtype"
	}};

	const net::hostport hostport
	{
		param["hostport"]
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
	while(!done)
		dock.wait();

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
		"s_listen", "listeners"
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
		"s_listen", "listeners"
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

	const auto eid
	{
		m::send(m::my_room, m::me, "ircd.listen", token.at("name"), opts)
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

	const auto event_idx
	{
		m::my_room.get("ircd.listen", token.at("name"))
	};

	const auto event_id
	{
		m::event_id(event_idx)
	};

	const auto redact_id
	{
		m::redact(m::my_room, m::me, event_id, "deleted")
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
		"s_listen", "load_listener"
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
		"s_listen", "unload_listener"
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
		"[reqs|id]",
	}};

	const bool &reqs
	{
		param[0] == "reqs"
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

	out << left
	    << setw(8) << "ID"
	    << " "
	    << setw(8) << "SOCKID"
	    << " "
	    << left
	    << setw(40) << "LOCAL"
	    << " "
	    << setw(40) << "REMOTE"
	    << " "
	    << right
	    << setw(25) << "BYTES FROM"
	    << " "
	    << setw(25) << "BYTES TO"
	    << " "
	    << setw(4) << "RDY"
	    << " "
	    << setw(4) << "REQ"
	    << " "
	    << setw(6) << "MODE"
	    << " "
	    << setw(4) << "CTX"
	    << " "
	    << setw(11) << "TIME"
	    << " "
	    << left
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

		out << left << setw(8) << client->id;

		out << " "
		    << left << setw(8) << (client->sock? net::id(*client->sock) : 0UL)
		    ;

		out << " "
		    << left << setw(40) << local(*client)
		    << " "
		    << left << setw(40) << remote(*client)
		    ;

		const std::pair<size_t, size_t> stat
		{
			bool(client->sock)?
				net::bytes(*client->sock):
				std::pair<size_t, size_t>{0, 0}
		};

		out << " "
		    << right << setw(25) << pretty(pbuf[0], iec(stat.first))
		    << " "
		    << right << setw(25) << pretty(pbuf[1], iec(stat.second))
		    ;

		out << " "
		    << right << setw(4) << client->ready_count
		    << " "
		    << right << setw(4) << client->request_count
		    ;

		out << " " << right << setw(6)
		    << (client->reqctx? "CTX"_sv : "ASYNC"_sv)
		    ;

		out << " " << right << setw(4)
		    << (client->reqctx? id(*client->reqctx) : 0UL)
		    ;

		out << " "
		    << right << setw(11) << pretty(pbuf[0], client->timer.at<nanoseconds>(), true)
		    ;

		out << " "
		    << left;

		if(client->request.user_id)
			out << " USER " << client->request.user_id;
		else if(client->request.origin)
			out << " PEER " << client->request.origin;

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
//
//

bool
console_cmd__crt(opt &out, const string_view &line)
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
		"s_listen", "listeners"
	};

	const auto &list{*listeners};
	for(const auto &listener : list)
	{
		if(listener.name() != targ)
			continue;

		const json::object &config
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
// key
//

bool
console_cmd__key(opt &out, const string_view &line)
{
	out << "origin:                  " << m::my_host() << std::endl;
	out << "public key ID:           " << m::self::public_key_id << std::endl;
	out << "public key base64:       " << m::self::public_key_b64 << std::endl;
	out << "TLS cert sha256 base64:  " << m::self::tls_cert_der_sha256_b64 << std::endl;

	return true;
}

bool
console_cmd__key__create(opt &out, const string_view &line)
{
	using prototype = void ();
	static mods::import<prototype> create_my_key
	{
		"s_keys", "ircd::m::create_my_key"
	};

	create_my_key();
	out << "done" << std::endl;
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
		(const auto &keys)
		{
			out << keys << std::endl;
		});
	}
	else
	{
		const std::pair<string_view, string_view> queries[1]
		{
			{ server_name, {} }
		};

		m::keys::query(query_server, queries, [&out]
		(const auto &keys)
		{
			out << keys << std::endl;
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

	if(stage.size() == id)
	{
		m::event base_event
		{
			{ "depth",             json::undefined_number  },
			{ "origin",            my_host()               },
			{ "origin_server_ts",  time<milliseconds>()    },
			{ "sender",            m::me.user_id           },
			{ "room_id",           m::my_room.room_id      },
			{ "type",              "m.room.message"        },
			{ "prev_state",        "[]"                    },
		};

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
		if(!verify_hash(event))
			out << "- HASH MISMATCH: " << b64encode_unpadded(hash(event)) << std::endl;
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

	thread_local char buf[8192];
	const auto prev
	{
		head.make_refs(buf, limit, true)
	};

	json::get<"prev_events"_>(event) = prev.first;
	json::get<"depth"_>(event) = prev.second;

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

	static const string_view types_general[]
	{
		"m.room.create",
		"m.room.power_levels",
	};

	static const string_view types_membership[]
	{
		"m.room.create",
		"m.room.join_rules",
		"m.room.power_levels",
	};

	const auto is_membership
	{
		at<"type"_>(event) == "m.room.member"
	};

	const auto &types
	{
		is_membership?
			vector_view<const string_view>(types_membership):
			vector_view<const string_view>(types_general)
	};

	const auto member
	{
		!is_membership?
			at<"sender"_>(event):
			string_view{}
	};

	const m::room room
	{
		at<"room_id"_>(event)
	};

	const m::room::auth auth
	{
		room
	};

	thread_local char buf[1024];
	json::get<"auth_events"_>(event) = auth.make_refs(buf, types, member);

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
		json::get<"event_id"_>(event) = make_id(event, event_id_buf);

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
		json::get<"event_id"_>(event) = make_id(event, prev_);
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

	if(id == -1)
		for(size_t i(0); i < stage.size(); ++i)
			eval(m::event{stage.at(i)});
	else
		eval(m::event{stage.at(id)});

	return true;
}

bool
console_cmd__stage__commit(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"[id]",
	}};

	const int &id
	{
		param.at<int>(0, -1)
	};

	m::vm::copts copts;
	m::vm::eval eval
	{
		copts
	};

	if(id == -1)
		for(size_t i(0); i < stage.size(); ++i)
			eval(m::event{stage.at(i)});
	else
		eval(m::event{stage.at(id)});

	return true;
}

bool
console_cmd__stage__send(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"remote", "[id]"
	}};

	const net::hostport remote
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

	m::v1::send::opts opts;
	opts.remote = remote;
	m::v1::send request
	{
		txnid, const_buffer{txn}, bufs, std::move(opts)
	};

	request.wait(out.timeout);
	const auto code
	{
		request.get()
	};

	const json::object response{request};
	const m::v1::send::response resp
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

	m::events::for_each_in_sender(user_id, [&out]
	(const m::user::id &user_id, const m::event::idx &event_idx)
	{
		const m::event::fetch event
		{
			event_idx, std::nothrow
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

	m::events::for_each_in_origin(origin, [&out]
	(const m::user::id &user_id, const m::event::idx &event_idx)
	{
		const m::event::fetch event
		{
			event_idx, std::nothrow
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

	m::events::for_each_in_type(type, [&out]
	(const string_view &type, const m::event::idx &event_idx)
	{
		const m::event::fetch event
		{
			event_idx, std::nothrow
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

conf::item<size_t>
events_dump_buffer_size
{
	{ "name",     "ircd.console.events.dump.buffer_size" },
	{ "default",  int64_t(4_MiB)                         },
};

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

	const fs::fd file
	{
		filename, std::ios::out | std::ios::app
	};

	const unique_buffer<mutable_buffer> buf
	{
		size_t(events_dump_buffer_size)
	};

	char *pos{data(buf)};
	size_t foff{0}, ecount{0}, acount{0}, errcount{0};
	m::events::for_each(m::event::idx{0}, [&]
	(const m::event::idx &seq, const m::event &event)
	{
		const auto remain
		{
			size_t(data(buf) + size(buf) - pos)
		};

		assert(remain >= m::event::MAX_SIZE && remain <= size(buf));
		const mutable_buffer mb{pos, remain};
		pos += json::print(mb, event);
		++ecount;

		if(pos + m::event::MAX_SIZE > data(buf) + size(buf))
		{
			const const_buffer cb{data(buf), pos};
			foff += size(fs::append(file, cb));
			pos = data(buf);
			++acount;

			const double pct
			{
				(seq / double(m::vm::sequence::retired)) * 100.0
			};

			log::info
			{
				"dump[%s] %lf$%c @ seq %zu of %zu; %zu events; %zu bytes; %zu writes; %zu errors",
				filename,
				pct,
				'%', //TODO: fix gram
				seq,
				m::vm::sequence::retired,
				ecount,
				foff,
				acount,
				errcount
			};
		}

		return true;
	});

	if(pos > data(buf))
	{
		const const_buffer cb{data(buf), pos};
		foff += size(fs::append(file, cb));
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
	if(try_lex_cast<ulong>(param.at("event_id")))
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

	const bool cached
	{
		m::cached(event_idx)
	};

	const auto cached_keys
	{
		m::cached_keys(event_idx, m::event::keys::selection{})
	};

	const bool full_json
	{
		has(m::dbs::event_json, byte_view<string_view>(event_idx))
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
				out << event.source << std::endl;
			else
				out << event << std::endl;

			return true;
		}

		case hash("source"):
		{
			if(event.source)
				out << event.source << std::endl;

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

	out << event_idx
	    << std::endl;

	out << pretty(event)
	    << std::endl;

	if(cached || cached_keys.count())
	{
		out << "+ CACHED";

		if(cached)
			out << " _json";

		for(const auto &key : m::event::keys{cached_keys})
			out << " " << key;

		out << std::endl;
	}

	if(!full_json)
		out << "- JSON NOT FOUND" << std::endl;

	if(event.source)
		out << "+ JSON SOURCE " << size(string_view{event.source}) << " bytes."
		    << std::endl;

	if(event.source && !json::valid(event.source, std::nothrow))
		out << "- JSON SOURCE INVALID" << std::endl;

	const m::event::conforms conforms
	{
		event
	};

	if(!conforms.clean())
		out << "- " << conforms << std::endl;

	try
	{
		if(!verify(event))
			out << "- SIGNATURE FAILED" << std::endl;
	}
	catch(const std::exception &e)
	{
		out << "- SIGNATURE FAILED: " << e.what() << std::endl;
	}

	if(!verify_hash(event))
		out << "- HASH MISMATCH: " << b64encode_unpadded(hash(event)) << std::endl;

	const auto failmsg(m::event::auth::failed(event));
	if(failmsg)
		out << "- UNAUTHORIZED: " << failmsg << std::endl;

	if(m::event::auth::is_power_event(event))
		out << "+ POWER EVENT" << std::endl;

	const m::event::prev &prev{event};
	if(prev.auth_events_count() || prev.prev_events_count())
		out << std::endl;

	for(size_t i(0); i < prev.auth_events_count(); ++i)
	{
		const m::event::id &id{prev.auth_event(i)};
		const m::event::fetch event{id, std::nothrow};
		if(!event.valid)
		{
			out << "- AUTH " << id << std::endl;
			continue;
		}

		out << "+ AUTH"
		    << " " << std::setw(9) << event.event_idx
		    << " " << pretty_oneline(event, false) << std::endl;
	}

	for(size_t i(0); i < prev.prev_events_count(); ++i)
	{
		const m::event::id &id{prev.prev_event(i)};
		const m::event::fetch event{id, std::nothrow};
		if(!event.valid)
		{
			out << "- PREV " << id << std::endl;
			continue;
		}

		out << "+ PREV"
		    << " " << std::setw(9) << event.event_idx
		    << " " << pretty_oneline(event, false) << std::endl;
	}

	const m::event::refs &refs{event_idx};
	const auto refcnt(refs.count());
	if(refcnt)
	{
		out << std::endl;
		out << "+ REFERENCED BY " << refs.count() << std::endl;
		refs.for_each([&out]
		(const m::event::idx &idx, const auto &type)
		{
			const m::event::fetch event{idx};
			out << "  " << reflect(type)
			    << " " << std::setw(9) << idx
			    << " " << pretty_oneline(event, false) << std::endl;

			return true;
		});
	}

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

	m::v1::event::opts opts;
	opts.remote = host;
	opts.dynamic = false;
	const unique_buffer<mutable_buffer> buf
	{
		96_KiB
	};

	m::v1::event request
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

	thread_local char sigbuf[64_KiB];
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
		const auto _event_id
		{
			m::event_id(event_idx, std::nothrow)
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
		{
			out << "- failed to find room_id for " << event_id
			    << " from " << event_idx
			    << std::endl;

			return true;
		}

		m::fetch::start(room_id, event_id);
		out << room_id << " : "
		    << event_id
		    << " via " << event_idx
		    << std::endl;

		++count;
		return true;
	});

	out << "started " << count << std::endl;
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
	else if(try_lex_cast<ulong>(id))
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
console_cmd__event__fetch(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "event_id"
	}};

	const m::event::id &event_id
	{
		param.at("event_id")
	};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const net::hostport hostport
	{
		room_id.host()
	};

	using prototype = json::object (const m::room::id &,
	                                const m::event::id &,
	                                const net::hostport &);

	static mods::import<prototype> acquire
	{
		"vm_fetch", "state_fetch"
	};

	acquire(room_id, event_id, hostport);
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

	const bool visible
	{
		m::visible(event_id, mxid)
	};

	out << event_id << " is "
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

	const m::event::auth::chain ac
	{
		m::index(event_id)
	};

	ac.for_each([&out](const auto &idx)
	{
		const m::event::fetch event
		{
			idx, std::nothrow
		};

		out << idx;
		if(event.valid)
			out << " " << pretty_oneline(event);

		out << std::endl;
	});

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
		param.at("type")
	};

	m::dbs::ref type
	{
		empty(typestr)?
			m::dbs::ref(-1):
			m::dbs::ref(0)

	};

	if(!empty(typestr))
		for(; uint8_t(type) < sizeof(m::dbs::ref) * 8; type = m::dbs::ref(uint8_t(type) + 1))
			if(reflect(type) == typestr)
				break;

	refs.for_each(type, [&out]
	(const auto &idx, const auto &type)
	{
		const m::event::fetch event
		{
			idx, std::nothrow
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
			idx, std::nothrow
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

	const m::event::auth::refs auth
	{
		index(event_id)
	};

	auth.for_each(type, [&out]
	(const m::event::idx &idx)
	{
		const m::event::fetch event
		{
			idx, std::nothrow
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
// state
//

bool
console_cmd__state__count(opt &out, const string_view &line)
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
console_cmd__state__each(opt &out, const string_view &line)
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

	m::state::for_each(root, type, [&out]
	(const string_view &key, const string_view &val)
	{
		out << key << " => " << val << std::endl;
	});

	return true;
}

bool
console_cmd__state__get(opt &out, const string_view &line)
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

	m::state::get(root, type, state_key, [&out]
	(const auto &value)
	{
		out << "got: " << value << std::endl;
	});

	return true;
}

bool
console_cmd__state__find(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"root", "[type]" "[state_key]"
	}};

	const string_view &root
	{
		param.at(0)
	};

	const string_view &type
	{
		param[1]
	};

	const string_view &state_key
	{
		param[2]
	};

	const auto closure{[&out]
	(const auto &key, const string_view &val)
	{
		out << key << " => " << val << std::endl;
		return true;
	}};

	m::state::for_each(root, type, state_key, closure);
	return true;
}

bool
console_cmd__state__root(opt &out, const string_view &line)
{
	const m::event::id event_id
	{
		token(line, ' ', 0)
	};

	char buf[m::state::ID_MAX_SZ];
	out << m::dbs::state_root(buf, event_id) << std::endl;
	return true;
}

bool
console_cmd__state__gc(opt &out, const string_view &line)
{
	using prototype = size_t ();
	static mods::import<prototype> gc
	{
		"m_state", "ircd__m__state__gc"
	};

	const size_t count
	{
		gc()
	};

	out << "done: " << count << std::endl;
	return true;
}

bool
console_cmd__state__CLEAR__CLEAR__CLEAR(opt &out, const string_view &line)
{
	using prototype = void ();
	static mods::import<prototype> clear
	{
		"m_state", "ircd__m__state__clear"
	};

	clear();

	out << "done" << std::endl;
	return true;
}

//
// commit
//

bool
console_cmd__commit(opt &out, const string_view &line)
{
	m::event event
	{
		json::object{line}
	};

	return true;
}

//
// eval
//

bool
console_cmd__eval(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id", "opts",
	}};

	const m::event::id &event_id
	{
		param.at(0)
	};

	const auto &args
	{
		param[1]
	};

	const m::event::fetch event
	{
		event_id
	};

	m::vm::opts opts;
	opts.errorlog = 0;
	opts.warnlog = 0;
	opts.nothrows = 0;
	opts.non_conform |= m::event::conforms::MISSING_PREV_STATE;

	tokens(args, ' ', [&opts](const auto &arg)
	{
		switch(hash(arg))
		{
			case "replay"_:
				opts.replays = true;
				break;

			case "noverify"_:
				opts.verify = false;
				break;
		}
	});

	m::vm::eval eval{opts};
	out << pretty(event) << std::endl;
	eval(event);
	out << "done" << std::endl;
	return true;
}

bool
console_cmd__eval__file(opt &out, const string_view &line)
{
	const params token{line, " ",
	{
		"file path", "limit", "start", "room_id/event_id/sender"
	}};

	const auto path
	{
		token.at(0)
	};

	const fs::fd file
	{
		path
	};

	const auto limit
	{
		token.at<size_t>(1)
	};

	const auto start
	{
		token[2]? lex_cast<size_t>(token[2]) : 0
	};

	const string_view id{token[3]};
	const string_view room_id
	{
		id && m::sigil(id) == m::id::ROOM? id : string_view{}
	};

	const string_view event_id
	{
		id && m::sigil(id) == m::id::EVENT? id : string_view{}
	};

	const string_view sender
	{
		id && m::sigil(id) == m::id::USER? id : string_view{}
	};

	m::vm::opts opts;
	opts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	opts.notify = false;
	opts.verify = false;
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
			fs::read(file, buf, foff)
		};

		size_t boff(0);
		json::object object;
		json::vector vector{read};
		for(; boff < size(read) && i < limit; ) try
		{
			object = *begin(vector);
			boff += size(string_view{object});
			vector = { data(read) + boff, size(read) - boff };
			const m::event event
			{
				object
			};

			if(room_id && json::get<"room_id"_>(event) != room_id)
				continue;

			if(event_id && json::get<"event_id"_>(event) != event_id)
				continue;

			if(sender && json::get<"sender"_>(event) != sender)
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
		catch(const std::exception &e)
		{
			out << fmt::snstringf
			{
				128, "Error at i=%zu j=%zu r=%zu foff=%zu boff=%zu\n",
				i, j, r, foff, boff
			};

			out << string_view{object} << std::endl;
			out << e.what() << std::endl;
			return true;
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
// rooms
//

bool
console_cmd__rooms(opt &out, const string_view &line)
{
	m::rooms::for_each([&out]
	(const m::room::id &room_id)
	{
		out << room_id << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__rooms__public(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"server|room_id_lb", "limit"
	}};

	const string_view &key
	{
		param.at("server|room_id_lb", string_view{})
	};

	auto limit
	{
		param.at("limit", 32L)
	};

	m::rooms::each_opts opts;
	opts.public_rooms = true;
	opts.key = key;
	opts.closure = [&limit, &out]
	(const m::room::id &room_id) -> bool
	{
		out << room_id << std::endl;
		return --limit > 0;
	};

	m::rooms::for_each(opts);
	return true;
}

bool
console_cmd__rooms__fetch(opt &out, const string_view &line)
{
	using prototype = std::pair<size_t, std::string> (const net::hostport &, const string_view &);

	static mods::import<prototype> fetch_update
	{
		"m_rooms", "ircd::m::rooms::fetch_update"
	};

	const params param{line, " ",
	{
		"server", "since"
	}};

	const net::hostport server
	{
		param.at("server")
	};

	const string_view &since
	{
		param.at("since", string_view{})
	};

	const auto pair
	{
		fetch_update(server, since)
	};

	out << "done" << std::endl
	    << "total room count estimate: " << pair.first << std::endl
	    << "next batch: " << pair.second << std::endl
	    ;

	return true;
}

//
// room
//

bool
console_cmd__room__top(opt &out, const string_view &line)
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

	const m::room::state state
	{
		room_id
	};

	char version_buf[32];
	out << "index:         " << m::room::index(room_id) << std::endl;
	out << "version:       " << m::version(version_buf, room_id) << std::endl;
	out << "federate:      " << std::boolalpha << m::federate(room_id) << std::endl;
	out << "top index:     " << std::get<m::event::idx>(top) << std::endl;
	out << "top depth:     " << std::get<int64_t>(top) << std::endl;
	out << "top event:     " << std::get<m::event::id::buf>(top) << std::endl;
	out << std::endl;

	state.for_each(m::room::state::types{[&out, &state]
	(const string_view &type)
	{
		if(!startswith(type, "m."))
			return true;

		state.for_each(type, m::event::closure{[&out]
		(const m::event &event)
		{
			if(json::get<"state_key"_>(event) != "")
				return;

			out << json::get<"type"_>(event) << ':' << std::endl;
			for(const auto &member : json::get<"content"_>(event))
				out << '\t' << member.first << ": " << member.second << std::endl;
			out << std::endl;
		}});

		return true;
	}});

	return true;
}

bool
console_id__room(opt &out,
                 const m::room::id &id,
                 const string_view &line)
{
	//TODO: XXX more detailed summary
	return console_cmd__room__top(out, line);
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
			event_idx, std::nothrow
		};

		out << pretty_oneline(event) << std::endl;
	});

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
console_cmd__room__herd(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "user_id"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::user::id &user_id
	{
		param.at(1)
	};

	using prototype = void (const m::room &, const m::user &, const milliseconds &);
	static mods::import<prototype> room_herd
	{
		"m_room", "room_herd"
	};

	room_herd(room_id, user_id, out.timeout);
	out << "done" << std::endl;
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
console_cmd__room__sounding(opt &out, const string_view &line)
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

	out << "head:      " << std::setw(8) << m::depth(room)
	    << "   " << m::event_id(head) << " (" << head << ")"
	    << std::endl;

	m::sounding(room, [&out]
	(const auto &range, const auto &event_idx)
	{
		out << "sounding:  " << std::setw(8) << range.second
		    << "   " << m::event_id(event_idx) << " (" << event_idx << ")"
		    << std::endl;

		return true;
	});

	out << "twain:     " << std::setw(8) << twain.first
	    << std::endl;

	out << "hazard:    " << std::setw(8) << hazard.first
	    << "   " << m::event_id(hazard.second) << " (" << hazard.second << ")"
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
		    << m::event_id(event_idx)
		    << std::endl;

		return true;
	}};

	if(param["reverse"] == "reverse")
		m::rfor_each_depth_gap(room, closure);
	else
		m::for_each_depth_gap(room, closure);

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
		room.visible(mxid)
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

	m::room::aliases::cache::for_each(server, [&out]
	(const m::room::alias &alias, const m::room::id &room_id)
	{
		out << std::left << std::setw(40) << alias << " " << room_id << std::endl;
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

	const net::hostport &remote
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

	const m::room::members::closure closure{[&out]
	(const m::user::id &user_id)
	{
		out << user_id << std::endl;
	}};

	members.for_each(membership, closure);
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

	const auto closure{[&out](const m::event &event)
	{
		out << pretty_oneline(event) << std::endl;
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
	(const m::event &event)
	{
		if(json::get<"origin"_>(event) != origin)
			return;

		out << pretty_oneline(event) << std::endl;
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
		    << " " << at<"event_id"_>(event)
		    << std::endl;
	}};

	const auto member_closure{[&room_id, event_closure]
	(const m::event &event)
	{
		const m::user user
		{
			at<"state_key"_>(event)
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
	}};

	members.for_each(membership, member_closure);
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
		if(noerror && ircd::server::errmsg(origin))
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

	state.for_each(type, [&out, &state]
	(const string_view &type, const string_view &state_key, const m::event::idx &event_idx)
	{
		const m::event::fetch event
		{
			event_idx, std::nothrow
		};

		if(!event.valid)
			return true;

		thread_local char smbuf[48];
		out
		    << smalldate(smbuf, json::get<"origin_server_ts"_>(event) / 1000L)
		    << std::right << " "
		    << std::setw(9) << json::get<"depth"_>(event)
		    << std::right << " [ "
		    << std::setw(30) << type
		    << std::left << " ] [ "
		    << std::setw(50) << state_key
		    << std::left << " ] "
		    << std::setw(72) << json::get<"event_id"_>(event)
		    << std::left << " "
		    ;

		size_t i(0);
		auto prev_idx(event_idx);
		for(; i < 4 && prev_idx; ++i, prev_idx = state.prev(prev_idx))
			out << (i? " <-- " : "") << prev_idx;

		out << std::endl;
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

	state.for_each(m::room::state::types{[&out]
	(const string_view &type)
	{
		out << type << std::endl;
		return true;
	}});

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

	state.for_each(type, prefix, m::room::state::keys_bool{[&out, &prefix]
	(const string_view &state_key)
	{
		if(prefix && !startswith(state_key, prefix))
			return false;

		out << state_key << std::endl;
		return true;
	}});

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
		param["type"]
	};

	const string_view &state_key
	{
		param["state_key"]
	};

	const auto depth
	{
		param.at("depth", -1L)
	};

	const m::room::state::space space
	{
		room_id
	};

	space.for_each(type, state_key, depth, [&out]
	(const auto &type, const auto &state_key, const auto &depth, const auto &event_idx)
	{
		out << std::setw(11) << std::left << event_idx;
		out << " " << std::setw(9) << std::left << depth;
		out << " " << std::setw(32) << std::left << type;
		out << " " << std::setw(64) << std::left << state_key;
		out << std::endl;
		return true;
	});

	return true;
}

bool
console_cmd__room__state__space__rebuild(opt &out, const string_view &line)
{
	m::room::state::space::rebuild{};
	return true;
}

bool
console_cmd__room__state__force(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"event_id"
	}};

	const m::event::id &event_id
	{
		param.at(0)
	};

	const m::event::fetch event
	{
		event_id
	};

	const auto res
	{
		m::room::state::force_present(event)
	};

	out << "forced " << event_id << " into present state" << std::endl;
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

	const m::room::state state
	{
		room_id
	};

	const size_t ret
	{
		m::room::state::purge_replaced(state)
	};

	out << "erased " << ret << std::endl;
	return true;
}

bool
console_cmd__room__state__rebuild__present(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id"
	}};

	const auto &room_id
	{
		param.at("room_id") != "*"?
			m::room_id(param.at(0)):
			"*"_sv
	};

	if(room_id == "*")
	{
		m::rooms::for_each([&out]
		(const m::room::id &room_id)
		{
			const m::room room{room_id};
			const m::room::state state{room};
			const size_t count
			{
				m::room::state::rebuild_present(state)
			};

			out << "done " << room_id << " " << count << std::endl;
			return true;
		});

		return true;
	}

	const m::room room{room_id};
	const m::room::state state{room};
	const size_t count
	{
		state.rebuild_present(state)
	};

	out << "done " << room_id << " " << count << std::endl;
	return true;
}

bool
console_cmd__room__state__rebuild__history(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::room::state state
	{
		room_id
	};

	const size_t count
	{
		state.rebuild_history(state)
	};

	out << "done " << count << std::endl;
	return true;
}

bool
console_cmd__room__state__history__clear(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const m::room::state state
	{
		room_id
	};

	using prototype = size_t (const m::room &);
	static mods::import<prototype> state__clear_history
	{
		"m_room", "state__clear_history"
	};

	const size_t count
	{
		state.clear_history(state)
	};

	out << "done " << count << std::endl;
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

	size_t count{0};
	m::room::messages it{room};
	for(; it && limit; --it, --limit)
	{
		const m::event &event{*it};
		count += match(filter, event);
	}

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

	const bool roots
	{
		has(out.special, "roots")
	};

	const m::room room
	{
		room_id
	};

	m::room::messages it
	{
		room, uint64_t(depth >= 0? depth : -1)
	};

	for(; it && limit >= 0; order == 'b'? --it : ++it, --limit)
		if(roots)
			out << std::setw(48) << std::left << it.state_root()
			    << " " << std::setw(8) << std::right << it.event_idx()
			    << " " << std::setw(8) << std::right << it.depth()
			    << " " << it.event_id()
			    << std::endl;
		else
			out << std::left << std::setw(10) << it.event_idx() << " "
			    << pretty_oneline(*it)
			    << std::endl;

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

	m::room::messages it{room};
	if(depth >= 0 && depth < std::numeric_limits<int64_t>::max())
		it.seek(depth);

	for(; it && limit >= 0; order == 'b'? --it : ++it, --limit)
		out << pretty_msgline(*it)
		    << std::endl;

	return true;
}

bool
console_cmd__room__timeline(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "x", "y"
	}};

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const m::room room
	{
		room_id
	};

	const m::room::timeline timeline
	{
		room
	};

	int64_t last_depth(0);
	const auto closure{[&out, &last_depth]
	(const auto &coord, const auto &event_idx)
	{
		const m::event::fetch event
		{
			event_idx, std::nothrow
		};

		if(json::get<"depth"_>(event) > last_depth + 1)
			out << std::endl;

		last_depth = json::get<"depth"_>(event);

		out << "("
		    << std::left
		    << std::setw(2) << coord.x
		    << " "
		    << std::right
		    << std::setw(3) << coord.y
		    << ") "
		    << std::left
		    << std::setw(11) << event_idx
		    << " "
		    << pretty_oneline(event, false)
		    << std::endl;

		return true;
	}};

	timeline.for_each(closure,
	{
		param.at<int64_t>("x", 0),
		param.at<int64_t>("y", 0)
	});
	return true;
}

bool
console_cmd__room__timeline__rebuild(opt &out, const string_view &line)
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

	m::room::timeline::rebuild(room);
	out << "done" << std::endl;
	return true;
}

bool
console_cmd__room__roots(opt &out, const string_view &line)
{
	assert(!out.special);
	out.special = "roots";
	return console_cmd__room__events(out, line);
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
		param.at(2)
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
console_cmd__room__redact(opt &out, const string_view &line)
{
	const auto &room_id
	{
		m::room_id(token(line, ' ', 0))
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
		param.at(1, m::me.user_id)
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
	if(m::has_sigil(id)) switch(m::sigil(id))
	{
		case m::id::USER:
			out << m::user{id}.room_id() << std::endl;
			break;

		case m::id::NODE:
			out << m::node{id}.room_id() << std::endl;
			break;

		case m::id::ROOM_ALIAS:
			out << m::room_id(m::room::alias(id)) << std::endl;
			break;

		default:
			break;
	}

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
console_cmd__room__dagree(opt &out, const string_view &line)
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

	using prototype = size_t (const m::room &, std::vector<size_t> &);
	static mods::import<prototype> dagree_histogram
	{
		"m_room", "dagree_histogram"
	};

	std::vector<size_t> v(32, 0);
	const size_t count
	{
		dagree_histogram(room, v)
	};

	for(size_t i(0); i < v.size(); ++i)
		out << std::setw(2) << std::right << i << ": " << v.at(i) << std::endl;

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

	const m::room room
	{
		room_id, event_id
	};

	const m::room::auth auth
	{
		room
	};

	auth.for_each([&out]
	(const m::event::idx &idx)
	{
		const m::event::fetch event{idx};
		out << idx
		    << " " << pretty_oneline(event)
		    << std::endl;
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

//
// user
//

//TODO: XXX
bool
console_id__user(opt &out,
                 const m::user::id &id,
                 const string_view &args)
{
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
		param.at(0)
	};

	const string_view &password
	{
		param.at(1)
	};

	const m::registar request
	{
		{ "username",    username },
		{ "password",    password },
		{ "bind_email",  false    },
	};

	using prototype = std::string
	                  (const m::registar &,
	                   const client *const &,
	                   const bool &);

	static mods::import<prototype> register_user
	{
		"client_register", "register_user"
	};

	const auto ret
	{
		register_user(request, nullptr, false)
	};

	out << ret << std::endl;
	return true;
}

bool
console_cmd__user__password(opt &out, const string_view &line)
{
	const params param
	{
		line, " ",
		{
			"user_id", "password"
		}
	};

	m::user user
	{
		param.at(0)
	};

	const string_view &password
	{
		param.at(1)
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
	const params param
	{
		line, " ",
		{
			"user_id"
		}
	};

	const m::user user
	{
		param.at(0)
	};

	out << user.user_id << " is "
	    << (user.is_active()? "active" : "inactive")
	    << std::endl;

	return true;
}

bool
console_cmd__user__activate(opt &out, const string_view &line)
{
	const params param
	{
		line, " ",
		{
			"user_id"
		}
	};

	m::user user
	{
		param.at(0)
	};

	if(user.is_active())
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
	const params param
	{
		line, " ",
		{
			"user_id"
		}
	};

	m::user user
	{
		param.at(0)
	};

	if(!user.is_active())
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
		param.at(0)
	};

	auto limit
	{
		param.at(1, size_t(16))
	};

	const m::user::room user_room{user};
	user_room.for_each("ircd.presence", m::event::closure_bool{[&out, &limit]
	(const m::event &event)
	{
		out << timestr(at<"origin_server_ts"_>(event) / 1000)
		    << " " << at<"content"_>(event)
		    << " " << at<"event_id"_>(event)
		    << std::endl;

		return --limit > 0;
	}});

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

	const m::user::rooms::origins origins
	{
		user
	};

	origins.for_each(membership, m::user::rooms::origins::closure{[&out]
	(const string_view &origin)
	{
		out << origin << std::endl;
	}});

	return true;
}

bool
console_cmd__user__read(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"user_id",
	}};

	const m::user user
	{
		param.at(0)
	};

	const m::user::room user_room{user};
	const m::room::state state{user_room};
	state.for_each("ircd.read", m::event::closure{[&out]
	(const m::event &event)
	{
		out << timestr(at<"origin_server_ts"_>(event) / 1000)
		    << " " << at<"state_key"_>(event)
		    << " " << at<"content"_>(event)
		    << " " << at<"event_id"_>(event)
		    << std::endl;
	}});

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

	const auto eid
	{
		m::receipt::read(room_id, user_id, event_id, ms)
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
		send(user_room, m::me.user_id, "ircd.read.ignore", target, json::object{})
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

	const m::room::state &tokens
	{
		m::user::tokens
	};

	tokens.for_each("ircd.access_token", m::event::closure_idx{[&out, &user, &clear]
	(const m::event::idx &event_idx)
	{
		bool match(false);
		m::get(std::nothrow, event_idx, "sender", [&match, &user]
		(const string_view &sender)
		{
			match = sender == user.user_id;
		});

		if(!match)
			return;

		const m::event::fetch event
		{
			event_idx
		};

		const string_view &token
		{
			at<"state_key"_>(event)
		};

		const milliseconds &ost
		{
			at<"origin_server_ts"_>(event)
		};

		const milliseconds now
		{
			time<milliseconds>()
		};

		out << token
		    << " "
		    << ost
		    << " "
		    << pretty(now - ost) << " ago"
		    << " "
		    << json::get<"event_id"_>(event);


		if(clear)
		{
			const auto eid
			{
				m::redact(m::user::tokens, user.user_id, at<"event_id"_>(event), "cleared")
			};

			out << " - cleared by " << eid;
		}

		out << std::endl;
	}});

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

	const net::hostport &remote
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
		m::room_id(param.at("room_id"))
	};

	const string_view &tag
	{
		param["tag"]
	};

	const m::user::room_tags room_tags
	{
		user_id, room_id
	};

	if(tag)
	{
		room_tags.get(tag, [&out]
		(const string_view &key, const json::object &val)
		{
			out << val << std::endl;
		});

		return true;
	}

	room_tags.for_each([&out]
	(const string_view &key, const json::object &val)
	{
		out << key << ": " << val << std::endl;
		return true;
	});

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

	if(!device_id)
	{
		m::device::for_each(user_id, [&out]
		(const string_view &device_id)
		{
			out << device_id << std::endl;
			return true;
		});

		return true;
	}

	m::device::for_each(user_id, device_id, [&out, &user_id, &device_id]
	(const string_view &prop)
	{
		m::device::get(std::nothrow, user_id, device_id, prop, [&out, &prop]
		(const string_view &value)
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

//
// users
//

bool
console_cmd__users(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"prefix"
	}};

	const auto prefix
	{
		param.at("prefix", string_view{})
	};

	m::users::for_each(prefix, m::user::closure_bool{[&out, &prefix]
	(const m::user &user)
	{
		if(prefix && !startswith(user.user_id, prefix))
			return false;

		out << user.user_id << std::endl;
		return true;
	}});

	return true;
}

//
// typing
//

bool
console_cmd__typing(opt &out, const string_view &line)
{
	m::typing::for_each([&out]
	(const m::typing &event)
	{
		out << event << std::endl;
	});

	return true;
}

//
// node
//

//TODO: XXX
bool
console_id__node(opt &out,
                 const m::node::id &id,
                 const string_view &args)
{
	return true;
}

bool
console_cmd__node__keys(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"node_id", "[limit]"
	}};

	const m::node::id::buf node_id
	{
		m::node::id::origin, param.at(0)
	};

	const m::node &node
	{
		node_id
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

		out << keys << std::endl;
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

	const m::node::id::buf node_id
	{
		m::node::id::origin, param.at(0)
	};

	const m::node &node
	{
		node_id
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

		out << key << std::endl;
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
	m::feds::acquire(opts, [&out]
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

	m::feds::acquire(opts, closure);

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
		room_id = m::get(std::nothrow, event_id, "room_id", room_id);

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
	m::feds::acquire(opts, [&out](const auto &result)
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
	m::feds::acquire(opts, [&out](const auto &result)
	{
		if(result.eptr)
		{
			out << "- " << std::setw(40) << std::left << result.origin
			    << " " << what(result.eptr)
			    << std::endl;

			return true;
		}

		const json::object &event
		{
			result.object.at("event")
		};

		const json::array prev_events
		{
			event["prev_events"]
		};

		out << "+ " << std::setw(40) << std::left << result.origin;
		out << " " << event["depth"];
		for(const json::array prev_event : prev_events)
		{
			const auto &prev_event_id
			{
				unquote(prev_event.at(0))
			};

			out << " " << string_view{prev_event_id};
		};

		out << std::endl;
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
	m::feds::acquire(opts, [&out](const auto &result)
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
		param.at(1, m::me.user_id)
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

		for(const json::array &prev_event : prev_events)
		{
			const auto &event_id
			{
				unquote(prev_event.at(0))
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

	const m::v1::key::server_key server_key
	{
		server_name, key_id
	};

	m::feds::opts opts;
	opts.op = m::feds::op::keys;
	opts.timeout = out.timeout;
	opts.room_id = room_id;
	opts.arg[0] = server_key.first;
	opts.arg[1] = server_key.second;
	m::feds::acquire(opts, [&out](const auto &result)
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
	m::feds::acquire(opts, [&grid, &origins]
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
console_cmd__feds__resend(opt &out, const string_view &line)
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

	m::vm::opts opts;
	opts.replays = true;
	opts.conforming = false;
	opts.fetch = false;
	opts.eval = false;
	opts.write = false;
	opts.effects = false;
	opts.notify = true;
	m::vm::eval
	{
		m::event{event}, opts
	};

	return true;
}

//
// fed
//

bool
console_cmd__fed__groups(opt &out, const string_view &line)
{
	const m::id::node &node
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

	m::v1::groups::publicised::opts opts;
	m::v1::groups::publicised request
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

	const net::hostport remote
	{
		param.at(1, room_id.host())
	};

	/// Select a better user_id if possible
	const m::room room{room_id};
	m::user::id::buf user_id;
	if(param["user_id"])
		user_id = param["user_id"];

	if(!user_id)
		user_id = room.any_user(my_host(), "join");

	// Make another attempt to find an invited user because that carries some
	// value (this query is not as fast as querying join memberships).
	if(!user_id)
		user_id = room.any_user(my_host(), "invite");

	thread_local char buf[16_KiB];
	m::v1::make_join::opts opts;
	opts.remote = remote;
	m::v1::make_join request
	{
		room_id, user_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	const json::object proto
	{
		request.in.content
	};

	if(param["op"] == "raw")
	{
		out << proto << std::endl;
		return true;
	}

	const json::object event
	{
		proto["event"]
	};

	out << "DEPTH "
	    << event["depth"]
	    << std::endl;

	const json::array &prev_events
	{
		event["prev_events"]
	};

	for(const json::array &prev_event : prev_events)
	{
		const m::event::id &id
		{
			unquote(prev_event.at(0))
		};

		out << "PREV "
		    << id << " "
		    << string_view{prev_event.at(1)}
		    << std::endl;
	}

	const json::array &auth_events
	{
		event["auth_events"]
	};

	for(const json::array &auth_event : auth_events)
	{
		const m::event::id &id
		{
			unquote(auth_event.at(0))
		};

		out << "AUTH "
		    << id << " "
		    << string_view{auth_event.at(1)}
		    << std::endl;
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

	const net::hostport remote
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

	thread_local char pdubuf[m::event::MAX_SIZE];
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

	m::v1::send::opts opts;
	opts.remote = remote;
	m::v1::send request
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

	const m::v1::send::response resp
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
console_cmd__fed__sync(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "remote", "limit", "event_id", "timeout"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const net::hostport remote
	{
		param.at(1, room_id.host())
	};

	const auto &limit
	{
		param.at(2, size_t(128))
	};

	const string_view &event_id
	{
		param[3]
	};

	const auto timeout
	{
		param.at(4, out.timeout)
	};

	// Used for out.head, out.content, in.head, but in.content is dynamic
	const unique_buffer<mutable_buffer> buf
	{
		32_KiB
	};

	m::v1::state::opts stopts;
	stopts.remote = remote;
	stopts.event_id = event_id;
	const mutable_buffer stbuf
	{
		data(buf), size(buf) / 2
	};

	m::v1::state strequest
	{
		room_id, stbuf, std::move(stopts)
	};

	m::v1::backfill::opts bfopts;
	bfopts.remote = remote;
	bfopts.event_id = event_id;
	bfopts.limit = limit;
	const mutable_buffer bfbuf
	{
		buf + size(stbuf)
	};

	m::v1::backfill bfrequest
	{
		room_id, bfbuf, std::move(bfopts)
	};

	const auto when
	{
		now<steady_point>() + timeout
	};

	bfrequest.wait_until(when);
	strequest.wait_until(when);

	bfrequest.get();
	strequest.get();

	const json::array &auth_chain
	{
		json::object{strequest}.get("auth_chain")
	};

	const json::array &pdus
	{
		json::object{strequest}.get("pdus")
	};

	const json::array &messages
	{
		json::object{bfrequest}.get("pdus")
	};

	std::vector<m::event> events;
	events.reserve(auth_chain.size() + pdus.size() + messages.size());

	for(const json::object &event : auth_chain)
		events.emplace_back(event);

	for(const json::object &event : pdus)
		events.emplace_back(event);

	for(const json::object &event : messages)
		events.emplace_back(event);

	std::sort(begin(events), end(events));
	events.erase(std::unique(begin(events), end(events)), end(events));

	m::vm::opts vmopts;
	vmopts.nothrows = -1;
	vmopts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	vmopts.debuglog_accept = true;
	vmopts.fetch_prev_check = false;
	vmopts.fetch_state_check = false;
	char rembuf[256];
	vmopts.node_id = string(rembuf, remote);
	m::vm::eval eval
	{
		vmopts
	};

	for(const auto &event : events)
		eval(event);

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

	const net::hostport remote
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
	thread_local char buf[8_KiB];
	m::v1::state::opts opts;
	opts.remote = remote;
	opts.event_id = event_id;
	m::v1::state request
	{
		room_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	const json::object &response
	{
		request
	};

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
	vmopts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	m::vm::eval eval
	{
		vmopts
	};

	std::vector<m::event> events;
	events.reserve(size(pdus) + size(auth_chain));

	for(const json::object &event : auth_chain)
		events.emplace_back(event);

	for(const json::object &event : pdus)
		events.emplace_back(event);

	std::sort(begin(events), end(events));
	events.erase(std::unique(begin(events), end(events)), end(events));
	for(const auto &event : events)
		eval(event);

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

	const net::hostport remote
	{
		param.at(1, room_id.host())
	};

	string_view event_id
	{
		param[2]
	};

	// Used for out.head, out.content, in.head, but in.content is dynamic
	thread_local char buf[8_KiB];
	m::v1::state::opts opts;
	opts.remote = remote;
	opts.event_id = event_id;
	opts.ids_only = true;
	m::v1::state request
	{
		room_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	const json::object &response
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

	const auto &room_id
	{
		m::room_id(param.at("room_id"))
	};

	const net::hostport remote
	{
		param["remote"]?
			param["remote"]:
			room_id.host()
	};

	const string_view &count
	{
		param.at("count", "32"_sv)
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

	// Used for out.head, out.content, in.head, but in.content is dynamic
	thread_local char buf[16_KiB];
	m::v1::backfill::opts opts;
	opts.remote = remote;
	opts.limit = lex_cast<size_t>(count);
	opts.event_id = event_id;

	m::v1::backfill request
	{
		room_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	const json::object &response
	{
		request
	};

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
	vmopts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	vmopts.nothrows = -1;
	vmopts.room_head = false;
	vmopts.room_head_resolve = true;
	vmopts.fetch_prev_check = false;
	vmopts.fetch_state_check = false;
	char rembuf[256];
	vmopts.node_id = string(rembuf, remote);
	m::vm::eval eval
	{
		vmopts
	};

	std::vector<m::event> events;
	events.reserve(lex_cast<size_t>(count));
	for(const json::object &event : pdus)
		events.emplace_back(event);

	std::sort(begin(events), end(events));
	events.erase(std::unique(begin(events), end(events)), end(events));
	for(const auto &event : events)
		eval(event);

	return true;
}

bool
console_cmd__fed__frontfill(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "remote", "earliest", "latest", "[limit]", "[min_depth]"
	}};

	const auto &room_id
	{
		m::room_id(param.at(0))
	};

	const net::hostport remote
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

	m::v1::frontfill::opts opts;
	opts.remote = remote;
	opts.limit = limit;
	opts.min_depth = min_depth;
	const unique_buffer<mutable_buffer> buf
	{
		16_KiB
	};

	m::v1::frontfill request
	{
		room_id, {earliest, latest}, buf, std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	const json::array &response
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
		"event_id", "remote", "[op]"
	}};

	const m::event::id &event_id
	{
		param.at(0)
	};

	const net::hostport &remote
	{
		param.at(1, event_id.host())
	};

	const string_view op
	{
		param[2]
	};

	m::v1::event::opts opts;
	opts.remote = remote;
	opts.dynamic = false;
	const unique_buffer<mutable_buffer> buf
	{
		96_KiB
	};

	m::v1::event request
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

	const json::object &response
	{
		request
	};

	const m::event event
	{
		response
	};

	out << pretty(event) << std::endl;

	try
	{
		if(!verify(event))
			out << "- SIGNATURE FAILED" << std::endl;
	}
	catch(const std::exception &e)
	{
		out << "- SIGNATURE FAILED: " << e.what() << std::endl;
	}

	if(!verify_hash(event))
		out << "- HASH MISMATCH: " << b64encode_unpadded(hash(event)) << std::endl;

	const m::event::conforms conforms
	{
		event
	};

	if(!conforms.clean())
		out << "- " << conforms << std::endl;

	if(has(op, "raw"))
		out << string_view{response} << std::endl;

	if(!has(op, "eval"))
		return true;

	m::vm::opts vmopts;
	vmopts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	vmopts.fetch_prev = false;
	m::vm::eval eval
	{
		event, vmopts
	};

	return true;
}

bool
console_cmd__fed__public_rooms(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"remote", "limit", "all_networks", "3pid"
	}};

	const net::hostport remote
	{
		param.at(0)
	};

	const auto limit
	{
		param.at(1, 32)
	};

	const auto all_nets
	{
		param.at(2, false)
	};

	const auto tpid
	{
		param[3]
	};

	m::v1::public_rooms::opts opts;
	opts.limit = limit;
	opts.third_party_instance_id = tpid;
	opts.include_all_networks = all_nets;
	const unique_buffer<mutable_buffer> buf
	{
		16_KiB
	};

	m::v1::public_rooms request
	{
		remote, buf, std::move(opts)
	};

	request.wait(out.timeout);
	request.get();

	const json::object &response
	{
		request
	};

	const auto total_estimate
	{
		response.get<size_t>("total_room_count_estimate")
	};

	const auto next_batch
	{
		unquote(response.get("next_batch"))
	};

	const json::array &rooms
	{
		response.get("chunk")
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
		"room_id", "event_id", "remote", "ids_only"
	}};

	const auto room_id
	{
		m::room_id(param.at(0))
	};

	const m::event::id &event_id
	{
		param.at(1)
	};

	const net::hostport remote
	{
		param.at(2, event_id.host())
	};

	const string_view &ids_only
	{
		param["ids_only"]
	};

	m::v1::event_auth::opts opts;
	opts.remote = remote;
	opts.ids_only = ids_only == "ids";
	const unique_buffer<mutable_buffer> buf
	{
		16_KiB
	};

	m::v1::event_auth request
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

	const net::hostport remote
	{
		param.at(2, event_id.host())
	};

	m::v1::query_auth::opts opts;
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

		const m::event::auth::chain chain
		{
			m::index(event_id)
		};

		m::event::fetch event;
		chain.for_each([&auth_chain, &event]
		(const m::event::idx &event_idx)
		{
			if(seek(event, event_idx, std::nothrow))
				auth_chain.append(event);
		});
	}

	const json::object content
	{
		ost.completed()
	};

	m::v1::query_auth request
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

	const net::hostport remote
	{
		token_count(line, ' ') > 1? token(line, ' ', 1) : user_id.host()
	};

	m::v1::query::opts opts;
	opts.remote = remote;

	thread_local char buf[8_KiB];
	m::v1::query::profile request
	{
		user_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	const auto code
	{
		request.get()
	};

	const json::object &response
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

	const net::hostport remote
	{
		token_count(line, ' ') > 1? token(line, ' ', 1) : room_alias.host()
	};

	m::v1::query::opts opts;
	opts.remote = remote;

	thread_local char buf[8_KiB];
	m::v1::query::directory request
	{
		room_alias, buf, std::move(opts)
	};

	request.wait(out.timeout);
	const auto code
	{
		request.get()
	};

	const json::object &response
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

	const net::hostport remote
	{
		param.at("remote", user_id.host())
	};

	m::v1::user::devices::opts opts;
	opts.remote = remote;

	const unique_buffer<mutable_buffer> buf
	{
		8_KiB
	};

	m::v1::user::devices request
	{
		user_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	const auto code
	{
		request.get()
	};

	const json::object &response
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

	const net::hostport remote
	{
		param.at("remote", user_id.host())
	};

	m::v1::user::opts opts;
	opts.remote = remote;

	const unique_buffer<mutable_buffer> buf
	{
		8_KiB
	};

	m::v1::user::keys::query request
	{
		user_id, device_id, buf, std::move(opts)
	};

	request.wait(out.timeout);
	const auto code
	{
		request.get()
	};

	const json::object &response
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

	const net::hostport remote
	{
		param.at("remote", user_id.host())
	};

	m::v1::user::opts opts;
	opts.remote = remote;

	const unique_buffer<mutable_buffer> buf
	{
		8_KiB
	};

	m::v1::user::keys::claim request
	{
		user_id, device_id, algorithm, buf, std::move(opts)
	};

	request.wait(out.timeout);
	const auto code
	{
		request.get()
	};

	const json::object &response
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
	m::v1::key::opts opts;
	m::v1::key::keys request
	{
		{server_name, key_id}, buf, std::move(opts)
	};

	request.wait(out.timeout);
	const auto code
	{
		request.get()
	};

	const json::object &response
	{
		request
	};

	const m::keys &key{response};
	out << key << std::endl;
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

	m::v1::key::opts opts;
	opts.dynamic = true;
	opts.remote = net::hostport
	{
		param.at(0)
	};

	const unique_buffer<mutable_buffer> buf{24_KiB};
	m::v1::key::query request
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
	const net::hostport remote
	{
		token(line, ' ', 0)
	};

	m::v1::version::opts opts;
	opts.remote = remote;

	thread_local char buf[8_KiB];
	m::v1::version request
	{
		buf, std::move(opts)
	};

	request.wait(out.timeout);
	const auto code
	{
		request.get()
	};

	const json::object &response
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

	if(empty(file))
	{
		const auto s(split(server, '/'));
		server = s.first;
		file = s.second;
	}

	using prototype = m::room::id (m::room::id::buf &,
	                               const string_view &server,
	                               const string_view &file);

	static mods::import<prototype> file_room_id
	{
		"media_media", "file_room_id"
	};

	m::room::id::buf buf;
	out << file_room_id(buf, server, file) << std::endl;
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

	using prototype = m::room::id::buf (const string_view &server,
	                                    const string_view &file,
	                                    const m::user::id &,
	                                    const net::hostport &remote);

	static mods::import<prototype> download
	{
		"media_media", "download"
	};

	const m::room::id::buf room_id
	{
		download(server, file, m::me.user_id, remote)
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
	out << "sequence:       "
	    << std::right << std::setw(10) << m::vm::sequence::retired
	    << std::endl;

	out << "eval total:     "
	    << std::right << std::setw(10) << m::vm::eval::id_ctr
	    << std::endl;

	out << "eval current:   "
	    << std::right << std::setw(10) << size(m::vm::eval::list)
	    << std::endl;

	return true;
}

bool
console_cmd__vm__eval(opt &out, const string_view &line)
{
	for(const auto *const &eval : m::vm::eval::list)
	{
		assert(eval);
		assert(eval->ctx);

		out << std::setw(9) << std::right << eval->id
		    << " | " << std::setw(4) << std::right << id(*eval->ctx)
		    << " | " << std::setw(4) << std::right << eval->sequence
		    << " | " << std::setw(64) << std::left << eval->event_id
		    ;

		out << std::endl;
	}

	return true;
}

//
// mc
//

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
console_cmd__fetch__list(opt &out, const string_view &line)
{
	m::fetch::for_each([&out]
	(const m::fetch::request &request)
	{
		out
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
console_cmd__fetch(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "event_id"
	}};

	if(!param.count())
		return console_cmd__fetch__list(out, line);

	const auto room_id
	{
		m::room_id(param.at("room_id"))
	};

	const m::event::id event_id
	{
		param.at("event_id")
	};

	if(!m::fetch::start(room_id, event_id))
	{
		out << "failed to start." << std::endl;
		return true;
	}

	out << "starting..." << std::endl;
	return true;
}

bool
console_cmd__fetch__sync(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"room_id", "event_id"
	}};

	const auto room_id
	{
		m::room_id(param.at("room_id"))
	};

	const string_view &event_id
	{
		param["event_id"]
	};

	const m::room room
	{
		room_id, event_id
	};

	if(!m::fetch::synchronize(room))
	{
		out << "failed to start." << std::endl;
		return true;
	}

	out << "done." << std::endl;
	return true;
}
