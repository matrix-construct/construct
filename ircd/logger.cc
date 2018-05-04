// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::log
{
	// Option toggles
	std::array<bool, num_of<facility>()> file_flush;
	std::array<bool, num_of<facility>()> console_flush;
	std::array<const char *, num_of<facility>()> console_ansi;

	// Runtime master switches
	std::array<bool, num_of<facility>()> file_out;
	std::array<bool, num_of<facility>()> console_out;
	std::array<bool, num_of<facility>()> console_err;

	// Suppression state (for struct console_quiet)
	std::array<bool, num_of<facility>()> quieted_out;
	std::array<bool, num_of<facility>()> quieted_err;

	// Logfile name and device
	std::array<const char *, num_of<facility>()> fname;
	std::array<std::ofstream, num_of<facility>()> file;

	std::ostream &out_console
	{
		std::cout
	};

	std::ostream &err_console
	{
		std::cerr
	};

	static void open(const facility &fac);
}

void
ircd::log::init()
{
	//TODO: XXX: config + cmd control + other fancy stuff

	//add_top_conf("log", NULL, NULL, conf_log_table);

	console_out[CRITICAL]    = true;
	console_out[ERROR]       = true;
	console_out[DERROR]      = true;
	console_out[WARNING]     = true;
	console_out[DWARNING]    = true;
	console_out[NOTICE]      = true;
	console_out[INFO]        = true;
	console_out[DEBUG]       = ircd::debugmode;

	//console_err[CRITICAL]    = true;
	//console_err[ERROR]       = true;
	//console_err[WARNING]     = true;

	file_out[CRITICAL]       = true;
	file_out[ERROR]          = true;
	file_out[DERROR]         = true;
	file_out[WARNING]        = true;
	file_out[DWARNING]       = true;
	file_out[NOTICE]         = true;
	file_out[INFO]           = true;
	file_out[DEBUG]          = ircd::debugmode;

	file_flush[CRITICAL]     = true;
	file_flush[ERROR]        = true;
	file_flush[DERROR]       = true;
	file_flush[WARNING]      = true;
	file_flush[DWARNING]     = true;
	file_flush[NOTICE]       = false;
	file_flush[INFO]         = false;
	file_flush[DEBUG]        = false;

	console_flush[CRITICAL]  = true;
	console_flush[ERROR]     = true;
	console_flush[DERROR]    = true;
	console_flush[WARNING]   = true;
	console_flush[DWARNING]  = true;
	console_flush[NOTICE]    = true;
	console_flush[INFO]      = true;
	console_flush[DEBUG]     = true;

	console_ansi[CRITICAL]  = "\033[1;5;37;45m";
	console_ansi[ERROR]     = "\033[1;37;41m";
	console_ansi[DERROR]    = "\033[0;31;47m";
	console_ansi[WARNING]   = "\033[0;30;43m";
	console_ansi[DWARNING]  = "\033[0;30;47m";
	console_ansi[NOTICE]    = "\033[1;37;46m";
	console_ansi[INFO]      = "\033[1;37;42m";
	console_ansi[DEBUG]     = "\033[1;30;47m";
}

void
ircd::log::fini()
{
	//remove_top_conf("log");
	flush();
	close();
}

void
ircd::log::open()
{
	for_each<facility>([](const facility &fac)
	{
		if(!fname[fac])
			return;

		if(!file_out[fac])
			return;

		if(file[fac].is_open())
			file[fac].close();

		file[fac].clear();
		file[fac].exceptions(std::ios::badbit | std::ios::failbit);
		open(fac);
	});
}

void
ircd::log::close()
{
	for_each<facility>([](const facility &fac)
	{
		if(file[fac].is_open())
			file[fac].close();
	});
}

void
ircd::log::flush()
{
	for_each<facility>([](const facility &fac)
	{
		file[fac].flush();
	});

	std::flush(out_console);
	std::flush(err_console);
}

void
ircd::log::open(const facility &fac)
try
{
	const auto &mode(std::ios::app);
	file[fac].open(fname[fac], mode);
}
catch(const std::exception &e)
{
	char buf[BUFSIZE];
	snprintf(buf, sizeof(buf), "!!! Opening log file [%s] failed: %s",
	         fname[fac],
	         e.what());

	std::cerr << buf << std::endl;
	throw;
}

void
ircd::log::console_enable(const facility &fac)
{
	console_out[fac] = true;
}

void
ircd::log::console_disable(const facility &fac)
{
	console_out[fac] = false;
}

bool
ircd::log::console_enabled(const facility &fac)
{
	return console_out[fac];
}

void
ircd::log::console_mask(const vector_view<string_view> &list)
{
	for(auto *const &log : log::list)
		log->cmasked = std::find(begin(list), end(list), log->name) != end(list);
}

void
ircd::log::console_unmask(const vector_view<string_view> &list)
{
	for(auto *const &log : log::list)
		log->cmasked = std::find(begin(list), end(list), log->name) == end(list);
}

//
// console quiet
//

ircd::log::console_quiet::console_quiet(const bool &showmsg)
{
	if(showmsg)
		notice("Log messages are now quieted at the console");

	std::copy(begin(console_out), end(console_out), begin(quieted_out));
	std::copy(begin(console_err), end(console_err), begin(quieted_err));
	std::fill(begin(console_out), end(console_out), false);
	std::fill(begin(console_err), end(console_err), false);

	// Make a special amend to never suppress CRITICAL messages because
	// these are usually for a crash or major b0rk where the console
	// user probably won't be continuing normally anyway...
	if(quieted_out[CRITICAL]) console_out[CRITICAL] = true;
	if(quieted_err[CRITICAL]) console_err[CRITICAL] = true;
}

ircd::log::console_quiet::~console_quiet()
{
	std::copy(begin(quieted_out), end(quieted_out), begin(console_out));
	std::copy(begin(quieted_err), end(quieted_err), begin(console_err));
}

//
// mark
//

ircd::log::mark::mark(const string_view &msg)
{
	for_each<facility>([&msg]
	(const auto &fac)
	{
		mark(fac, msg);
	});
}

ircd::log::mark::mark(const facility &fac,
                      const string_view &msg)
{
	vlog(star, fac, "%s", msg);
}

//
// log
//

/// Linkage for list of named loggers.
template<>
decltype(ircd::instance_list<ircd::log::log>::list)
ircd::instance_list<ircd::log::log>::list
{};

ircd::log::log *
ircd::log::log::find(const string_view &name)
{
	const auto it
	{
		std::find_if(begin(list), end(list), [&name]
		(const auto *const &log)
		{
			return log->name == name;
		})
	};

	return *it;
}

ircd::log::log *
ircd::log::log::find(const char &snote)
{
	const auto it
	{
		std::find_if(begin(list), end(list), [&snote]
		(const auto *const &log)
		{
			return log->snote == snote;
		})
	};

	return *it;
}

/// This function is related to cross-thread logging; its purpose
/// is to check if a given named logger still exists after the pointer
/// is conveyed to the main thread from some other thread.
bool
ircd::log::log::exists(const log *const &ptr)
{
	const auto it
	{
		std::find_if(begin(list), end(list), [&ptr]
		(const auto *const &log)
		{
			return log == ptr;
		})
	};

	return it != end(list);
}

//
// log::log
//

ircd::log::log::log(const string_view &name,
                    const char &snote)
:name{name}
,snote{snote}
{
	for(const auto *const &other : list)
	{
		if(other == this)
			continue;

		if(other->name == name)
			throw ircd::error
			{
				"Logger with name '%s' already exists at %p",
				name,
				other
			};

		if(snote != '\0' && other->snote == snote)
			throw ircd::error
			{
				"Logger with snote '%c' is '%s' and already exists at %p",
				snote,
				other->name,
				other
			};
	}
}

//
// vlog
//

namespace ircd::log
{
	static void check(std::ostream &) noexcept;
	static void slog(const log &, const facility &, const window_buffer::closure &) noexcept;
	static void vlog_threadsafe(const log &, const facility &, const char *const &fmt, const va_rtti &ap);
}

decltype(ircd::log::star)
ircd::log::star
{
	"*", '*'
};

decltype(ircd::log::general)
ircd::log::general
{
	"ircd", 'G'
};

/// ircd::log is not thread-safe. This internal function is called when the
/// normal vlog() detects it's not on the main IRCd thread. It then generates
/// the formatted log message on this thread, and posts the message to the
/// main IRCd event loop which is running on the main thread.
void
ircd::log::vlog_threadsafe(const log &log,
                           const facility &fac,
                           const char *const &fmt,
                           const va_rtti &ap)
{
	// Generate the formatted message on this thread first
	std::string str
	{
		fmt::vsnstringf(1024, fmt, ap)
	};

	// The pointer to the logger is copied to the main thread.
	auto *const logp{&log};
	ircd::post([fac, str(std::move(str)), logp]
	{
		// If that named logger was destroyed while this closure was
		// travelling to the main thread then we just discard this message.
		if(!log::exists(logp))
			return;

		slog(*logp, fac, [&str](const mutable_buffer &out) -> size_t
		{
			return copy(out, string_view{str});
		});
	});
}

ircd::log::vlog::vlog(const log &log,
                      const facility &fac,
                      const char *const &fmt,
                      const va_rtti &ap)
{
	if(!is_main_thread())
	{
		vlog_threadsafe(log, fac, fmt, ap);
		return;
	}

	slog(log, fac, [&fmt, &ap](const mutable_buffer &out) -> size_t
	{
		return fmt::vsprintf(out, fmt, ap);
	});
}

namespace ircd::log
{
	// linkage for slog() reentrance assertion
	bool entered;
}

void
ircd::log::slog(const log &log,
                const facility &fac,
                const window_buffer::closure &closure)
noexcept
{
	// When all of these conditions are true there is no possible log output
	// so we can bail real quick.
	if(!file[fac].is_open() && !console_out[fac] && !console_err[fac])
		return;

	// Same for this set of conditions...
	if((!file[fac].is_open() || !log.fmasked) && !log.cmasked)
		return;

	// Have to be on the main thread to call slog().
	assert_main_thread();

	// During the composition of this log message, if another log message
	// is created from calls for normal reasons or from errors, it's a
	// problem too. We can only have one log slog() at a time for now...
	const reentrance_assertion<entered> ra;

	// If slog() yields for some reason that's not good either...
	const ctx::critical_assertion ca;

	// The principal buffer doesn't have to be static but courtesy of all the
	// above effort we might as well take advantage...
	static char buf[1024], date[64];

	// Maximum size of log line leaving 2 characters for \r\n
	const size_t max(sizeof(buf) - 2);

	// Compose the prefix sequence into the buffer through stringstream
	std::stringstream s;
	pubsetbuf(s, buf);
	s << microtime(date)
	  << ' '
	  << (console_ansi[fac]? console_ansi[fac] : "")
	  << std::setw(8)
	  << std::right
	  << reflect(fac)
	  << (console_ansi[fac]? "\033[0m " : " ")
//	  << (log.snote? log.snote : '-')
	  << std::setw(9)
	  << std::right
	  << log.name
	  << ' '
	  << std::setw(8)
	  << trunc(ctx::name(), 8)
	  << ' '
	  << std::setw(6)
	  << std::right
	  << ctx::id()
	  << " :";

	// Compose the user message after prefix
	const size_t pos(s.tellp());
	const mutable_buffer userspace{buf + pos, max - pos};
	window_buffer sb{userspace};
	sb(closure);

	// Compose the newline after user message.
	size_t len{pos + sb.consumed()};
	assert(len + 2 <= sizeof(buf));
	buf[len++] = '\r';
	buf[len++] = '\n';

	// Closure to copy the message to various places
	assert(len <= sizeof(buf));
	const string_view msg{buf, len};
	const auto write{[&msg](std::ostream &s)
	{
		check(s);
		s.write(data(msg), size(msg));
	}};

	// copy to std::cerr
	if(log.cmasked && console_err[fac])
	{
		err_console.clear();
		write(err_console);
	}

	// copy to std::cout
	if(log.cmasked && console_out[fac])
	{
		out_console.clear();
		write(out_console);
		if(console_flush[fac])
			std::flush(out_console);
	}

	// copy to file
	if(log.fmasked && file[fac].is_open())
	{
		file[fac].clear();
		write(file[fac]);
		if(file_flush[fac])
			std::flush(file[fac]);
	}
}

void
ircd::log::check(std::ostream &s)
noexcept try
{
	if(likely(s.good()))
		return;

	char buf[128];
	snprintf(buf, sizeof(buf), "fatal: log stream good[%d] bad[%d] fail[%d] eof[%d]",
	         s.good(),
	         s.bad(),
	         s.fail(),
	         s.eof());

	fprintf(stderr, "log stream(%p) fatal: %s\n", (const void *)&s, buf);
	fprintf(stdout, "log stream(%p) fatal: %s\n", (const void *)&s, buf);
	fflush(stderr);
	fflush(stdout);
	s.exceptions(s.eofbit | s.failbit | s.badbit);
	throw std::runtime_error(buf);
}
catch(const std::exception &e)
{
	fprintf(stderr, "%s\n", e.what());
	fprintf(stdout, "%s\n", e.what());
	fflush(stderr);
	fflush(stdout);
	ircd::terminate();
}

const char *
ircd::log::reflect(const facility &f)
{
	switch(f)
	{
		case facility::CRITICAL:   return "CRITICAL";
		case facility::ERROR:      return "ERROR";
		case facility::DERROR:     return "ERROR";
		case facility::WARNING:    return "WARNING";
		case facility::DWARNING:   return "WARNING";
		case facility::INFO:       return "INFO";
		case facility::NOTICE:     return "NOTICE";
		case facility::DEBUG:      return "DEBUG";
		case facility::_NUM_:      break; // Allows -Wswitch to remind developer to add reflection here
	};

	return "??????";
}

const char *
ircd::smalldate(const time_t &ltime)
{
	static const size_t MAX_DATE_STRING
	{
		32 // maximum string length for a date string (ircd_defs.h)
	};

	struct tm lt;
	localtime_r(&ltime, &lt);
	static char buf[MAX_DATE_STRING];
	snprintf(buf, sizeof(buf), "%d/%d/%d %02d.%02d",
	         lt.tm_year + 1900,
	         lt.tm_mon + 1,
	         lt.tm_mday,
	         lt.tm_hour,
	         lt.tm_min);

	return buf;
}
