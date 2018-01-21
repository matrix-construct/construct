/*
 * charybdis: an advanced Internet Relay Chat Daemon(ircd).
 *
 * Copyright (C) 2003 Lee H <lee@leeh.co.uk>
 * Copyright (C) 2003-2005 ircd-ratbox development team
 * Copyright (C) 2008 William Pitcock <nenolod@sacredspiral.co.uk>
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1.Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * 2.Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * 3.The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
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
	static void vlog_threadsafe(const facility &fac, const std::string &name, const char *const &fmt, const va_rtti &ap);
}

void
ircd::log::init()
{
	//TODO: XXX: config + cmd control + other fancy stuff

	//add_top_conf("log", NULL, NULL, conf_log_table);

	console_out[CRITICAL]    = true;
	console_out[ERROR]       = true;
	console_out[WARNING]     = true;
	console_out[NOTICE]      = true;
	console_out[INFO]        = true;
	console_out[DEBUG]       = ircd::debugmode;

	//console_err[CRITICAL]    = true;
	//console_err[ERROR]       = true;
	//console_err[WARNING]     = true;

	file_out[CRITICAL]       = true;
	file_out[ERROR]          = true;
	file_out[WARNING]        = true;
	file_out[NOTICE]         = true;
	file_out[INFO]           = true;
	file_out[DEBUG]          = ircd::debugmode;

	file_flush[CRITICAL]     = true;
	file_flush[ERROR]        = true;
	file_flush[WARNING]      = true;
	file_flush[NOTICE]       = false;
	file_flush[INFO]         = false;
	file_flush[DEBUG]        = false;

	console_flush[CRITICAL]  = true;
	console_flush[ERROR]     = true;
	console_flush[WARNING]   = true;
	console_flush[NOTICE]    = false;
	console_flush[INFO]      = false;
	console_flush[DEBUG]     = true;

	console_ansi[CRITICAL]  = "\033[1;5;37;45m";
	console_ansi[ERROR]     = "\033[1;37;41m";
	console_ansi[WARNING]   = "\033[0;30;43m";
	console_ansi[NOTICE]    = "\033[1;37;46m";
	console_ansi[INFO]      = "\033[1;37;42m";
	console_ansi[DEBUG]     = "\033[1;30;47m";
}

void
ircd::log::fini()
{
	//remove_top_conf("log");
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

ircd::log::log::log(const std::string &name)
:name{name}
{
}

ircd::log::log::log(const std::string &name,
              const char &snote)
:log{name}
{
}

void
ircd::log::mark(const char *const &msg)
{
	for_each<facility>([&msg]
	(const auto &fac)
	{
		mark(fac, msg);
	});
}

void
ircd::log::mark(const facility &fac,
                const char *const &msg)
{
	static const auto name{"*"s};
	vlog(fac, name, "%s", msg);
}

namespace ircd::log
{
	static void check(std::ostream &) noexcept;
	static void slog(const facility &fac, const string_view &name, const stream_buffer::closure &) noexcept;
}

/// ircd::log is not thread-safe. This internal function is called when the
/// normal vlog() detects it's not on the main IRCd thread. It then generates
/// the formatted log message on this thread, and posts the message to the
/// main IRCd event loop which is running on the main thread.
void
ircd::log::vlog_threadsafe(const facility &fac,
                           const std::string &name,
                           const char *const &fmt,
                           const va_rtti &ap)
{
	// Generate the formatted message on this thread first
	std::string str
	{
		fmt::vsnstringf(1024, fmt, ap)
	};

	// The reference to name has to be safely maintained
	ircd::post([fac, str(std::move(str)), name(std::move(name))]
	{
		slog(fac, name, [&str](const mutable_buffer &out) -> size_t
		{
			return copy(out, string_view{str});
		});
	});
}

void
ircd::log::vlog(const facility &fac,
                const char *const &fmt,
                const va_rtti &ap)
{
	static const auto name{"ircd"s};
	vlog(fac, name, fmt, ap);
}

void
ircd::log::vlog(const facility &fac,
                const std::string &name,
                const char *const &fmt,
                const va_rtti &ap)
{
	if(!is_main_thread())
	{
		vlog_threadsafe(fac, name, fmt, ap);
		return;
	}

	slog(fac, name, [&fmt, &ap](const mutable_buffer &out) -> size_t
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
ircd::log::slog(const facility &fac,
                const string_view &name,
                const stream_buffer::closure &closure)
noexcept
{
	if(!file[fac].is_open() && !console_out[fac] && !console_err[fac])
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
	s.rdbuf()->pubsetbuf(buf, max);
	s << microtime(date)
	  << ' '
	  << (console_ansi[fac]? console_ansi[fac] : "")
	  << std::setw(8)
	  << std::right
	  << reflect(fac)
	  << (console_ansi[fac]? "\033[0m " : " ")
	  << std::setw(9)
	  << std::right
	  << name
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
	stream_buffer sb{userspace};
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
	if(console_err[fac])
	{
		err_console.clear();
		write(err_console);
	}

	// copy to std::cout
	if(console_out[fac])
	{
		out_console.clear();
		write(out_console);
		if(console_flush[fac])
			std::flush(out_console);
	}

	// copy to file
	if(file[fac].is_open())
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
		case facility::DEBUG:      return "DEBUG";
		case facility::INFO:       return "INFO";
		case facility::NOTICE:     return "NOTICE";
		case facility::WARNING:    return "WARNING";
		case facility::ERROR:      return "ERROR";
		case facility::CRITICAL:   return "CRITICAL";
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
