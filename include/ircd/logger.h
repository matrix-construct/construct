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

#ifdef __cplusplus
namespace ircd {

const char *smalldate(const time_t &);

}       // namespace ircd
#endif  // __cplusplus

// windows.h #define conflicts with our facility
#ifdef HAVE_WINDOWS_H
#undef ERROR
#endif

#ifdef __cplusplus
namespace ircd  {
namespace log   {

enum facility
{
	CRITICAL = 0,
	ERROR    = 1,
	WARNING  = 2,
	NOTICE   = 3,
	INFO     = 4,
	DEBUG    = 5,

	_NUM_
};

const char *reflect(const facility &);
void slog(const facility &, const std::function<void (std::ostream &)> &);
void vlog(const facility &, const std::string &name, const sno::mask &, const char *const &fmt, va_list ap);
void vlog(const facility &, const std::string &name, const char *const &fmt, va_list ap);
void vlog(const facility &, const char *const &fmt, va_list ap);
void logf(const facility &, const char *fmt, ...) AFP(2, 3);
void mark(const facility &, const char *const &msg = nullptr);
void mark(const char *const &msg = nullptr);

class log
{
	using lease_ptr = std::shared_ptr<mode_lease<sno::mask, sno::table>>;

	std::string name;
	lease_ptr snote;

  public:
	void operator()(const facility &, const char *fmt, ...) AFP(3, 4);
	void critical(const char *fmt, ...) AFP(2, 3);
	void error(const char *fmt, ...) AFP(2, 3);
	void warning(const char *fmt, ...) AFP(2, 3);
	void notice(const char *fmt, ...) AFP(2, 3);
	void info(const char *fmt, ...) AFP(2, 3);
	void debug(const char *fmt, ...) AFP(2, 3);

	log(const std::string &name, const lease_ptr &lease);
	log(const std::string &name, const char &snote);
	log(const std::string &name);
};

void critical(const char *fmt, ...) AFP(1, 2);
void error(const char *fmt, ...) AFP(1, 2);
void warning(const char *fmt, ...) AFP(1, 2);
void notice(const char *fmt, ...) AFP(1, 2);
void info(const char *fmt, ...) AFP(1, 2);
void debug(const char *fmt, ...) AFP(1, 2);

struct console_quiet
{
	console_quiet(const bool &showmsg = true);
	~console_quiet();
};

void flush();
void close();
void open();

void init();
void fini();

}      // namespace log


enum ilogfile
{
       L_MAIN,
       L_USER,
       L_FUSER,
       L_OPERED,
       L_FOPER,
       L_SERVER,
       L_KILL,
       L_KLINE,
       L_OPERSPY,
       L_IOERROR,
       LAST_LOGFILE
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wformat-security"
template<class... A> void ilog(ilogfile dest, A&&... a)
{
	log::logf(log::INFO, std::forward<A>(a)...);
}
template<class... A> void idebug(A&&... a)
{
	log::debug(std::forward<A>(a)...);
}
template<class... A> void inotice(A&&... a)
{
	log::notice(std::forward<A>(a)...);
}
template<class... A> void iwarn(A&&... a)
{
	log::warning(std::forward<A>(a)...);
}
template<class... A> void ierror(A&&... a)
{
	log::error(std::forward<A>(a)...);
}
template<class... A> void ilog_error(A&&... a)
{
	log::error(std::forward<A>(a)...);
}
template<class... A> void slog(const ilogfile, const uint sno, const char *fmt, A&&... a)
{
	log::info(fmt, std::forward<A>(a)...);
}
#pragma GCC diagnostic pop

}      // namespace ircd
#endif // __cplusplus
