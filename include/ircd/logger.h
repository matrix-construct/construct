/*
 * ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *
 * Copyright (C) 2003 Lee Hardy <lee@leeh.co.uk>
 * Copyright (C) 2003-2004 ircd-ratbox development team
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1.Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 2.Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
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

#pragma once
#define HAVE_IRCD_LOGGER_H

// windows.h #define conflicts with our facility
#ifdef HAVE_WINDOWS_H
#undef ERROR
#endif

namespace ircd
{
	const char *smalldate(const time_t &);
}

namespace ircd::log
{
	enum facility :int;
	const char *reflect(const facility &);

	struct log;
	struct console_quiet;

	void slog(const facility &, const std::function<void (std::ostream &)> &);
	void vlog(const facility &, const std::string &name, const char *const &fmt, const va_rtti &ap);
	void vlog(const facility &, const char *const &fmt, const va_rtti &ap);
	void mark(const facility &, const char *const &msg = nullptr);
	void mark(const char *const &msg = nullptr);

	template<class... args> void critical(const char *const &fmt, args&&...);
	template<class... args> void error(const char *const &fmt, args&&...);
	template<class... args> void warning(const char *const &fmt, args&&...);
	template<class... args> void notice(const char *const &fmt, args&&...);
	template<class... args> void info(const char *const &fmt, args&&...);
	template<class... args> void debug(const char *const &fmt, args&&...);

	void flush();
	void close();
	void open();

	void init();
	void fini();
}

enum ircd::log::facility
:int
{
	CRITICAL = 0,
	ERROR    = 1,
	WARNING  = 2,
	NOTICE   = 3,
	INFO     = 4,
	DEBUG    = 5,
	_NUM_
};

class ircd::log::log
{
	std::string name;

  public:
	template<class... args> void operator()(const facility &, const char *const &fmt, args&&...);

	template<class... args> void critical(const char *const &fmt, args&&...);
	template<class... args> void error(const char *const &fmt, args&&...);
	template<class... args> void warning(const char *const &fmt, args&&...);
	template<class... args> void notice(const char *const &fmt, args&&...);
	template<class... args> void info(const char *const &fmt, args&&...);
	template<class... args> void debug(const char *const &fmt, args&&...);

	log(const std::string &name, const char &snote);
	log(const std::string &name);
};

struct ircd::log::console_quiet
{
	console_quiet(const bool &showmsg = true);
	~console_quiet();
};

template<class... args>
void
ircd::log::debug(const char *const &fmt,
                 args&&... a)
{
	vlog(facility::DEBUG, fmt, va_rtti{std::forward<args>(a)...});
}

template<class... args>
void
ircd::log::info(const char *const &fmt,
                args&&... a)
{
	vlog(facility::INFO, fmt, va_rtti{std::forward<args>(a)...});
}

template<class... args>
void
ircd::log::notice(const char *const &fmt,
                  args&&... a)
{
	vlog(facility::NOTICE, fmt, va_rtti{std::forward<args>(a)...});
}

template<class... args>
void
ircd::log::warning(const char *const &fmt,
                   args&&... a)
{
	vlog(facility::WARNING, fmt, va_rtti{std::forward<args>(a)...});
}

template<class... args>
void
ircd::log::error(const char *const &fmt,
                 args&&... a)
{
	vlog(facility::ERROR, fmt, va_rtti{std::forward<args>(a)...});
}

template<class... args>
void
ircd::log::critical(const char *const &fmt,
                    args&&... a)
{
	vlog(facility::CRITICAL, fmt, va_rtti{std::forward<args>(a)...});
}

template<class... args>
void
ircd::log::log::debug(const char *const &fmt,
                      args&&... a)
{
	operator()(facility::DEBUG, fmt, va_rtti{std::forward<args>(a)...});
}

template<class... args>
void
ircd::log::log::info(const char *const &fmt,
                     args&&... a)
{
	operator()(facility::INFO, fmt, va_rtti{std::forward<args>(a)...});
}

template<class... args>
void
ircd::log::log::notice(const char *const &fmt,
                       args&&... a)
{
	operator()(facility::NOTICE, fmt, va_rtti{std::forward<args>(a)...});
}

template<class... args>
void
ircd::log::log::warning(const char *const &fmt,
                        args&&... a)
{
	operator()(facility::WARNING, fmt, va_rtti{std::forward<args>(a)...});
}

template<class... args>
void
ircd::log::log::error(const char *const &fmt,
                      args&&... a)
{
	operator()(facility::ERROR, fmt, va_rtti{std::forward<args>(a)...});
}

template<class... args>
void
ircd::log::log::critical(const char *const &fmt,
                         args&&... a)
{
	operator()(facility::CRITICAL, fmt, va_rtti{std::forward<args>(a)...});
}

template<class... args>
void
ircd::log::log::operator()(const facility &f,
                           const char *const &fmt,
                           args&&... a)
{
	vlog(f, name, fmt, va_rtti{std::forward<args>(a)...});
}
