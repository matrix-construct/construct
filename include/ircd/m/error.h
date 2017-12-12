/*
 * charybdis: 21st Century IRC++d
 *
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
 *
 */

#pragma once
#define HAVE_IRCD_M_ERROR_H

namespace ircd::m
{
	struct error;
}

/// This hierarchy is designed to allow developers to throw an exception with
/// matrix protocol specific information which has the potential to become a
/// proper matrix JSON error object sent over HTTP to clients. Many exceptions
/// aren't intended to reach clients, but just in case, those can utilize an
/// http::INTERNAL_SERVER_ERROR. A regular IRCD_EXCEPTION won't be rooted in
/// m::error; if thrown during a client's request and not first caught it
/// may result in an internal server error sent to the client and may be
/// further rethrown.
///
/// The IRCD_M_EXCEPTION macro is provided and should be used to declare the
/// error type first rather than throwing an m::error directly if possible.
///
struct ircd::m::error
:http::error
{
	template<class... args> error(const http::code &, const string_view &errcode, const char *const &fmt, args&&...);
	template<class... args> error(const string_view &errcode, const char *const &fmt, args&&...);
	error(const http::code &, const json::object &object = {});
	error(const http::code &, const json::members &);
	error(const http::code &, const json::iov &);
	error(const http::code &);
	error(std::string = {});

	IRCD_OVERLOAD(child)
	template<class... args> error(child_t, args&&... a)
	:error{std::forward<args>(a)...}
	{}
};

/// Macro for all matrix exceptions; all errors rooted from m::error
///
/// Unfortunately we use another macro here instead of IRCD_EXCEPTION but what
/// we gain is a more suitable exception hierarchy for the matrix protocol
/// for fecundity of m::error, which is still the progeny of ircd::error. This
/// macro takes 3 arguments:
///
/// - parent: A parent exception class type similar to the rest of ircd's
/// system. For this macro, the parent can never be above m::error.
///
/// - name: The name of the exception is also what will be seen in the matrix
/// protocol JSON's errcode. The matrix protocol error codes are UPPER_CASE and
/// will appear as defined. This is also the name of this class itself too.
///
/// - httpcode: An HTTP code which will be used if this exception ever makes
/// it out to a client.
///
#define IRCD_M_EXCEPTION(_parent_, _name_, _httpcode_)              \
struct _name_                                                       \
: _parent_                                                          \
{                                                                   \
    template<class... args> _name_(args&&... a)                     \
    : _parent_                                                      \
    {                                                               \
        child, _httpcode_, "M_"#_name_, std::forward<args>(a)...    \
    }{}                                                             \
                                                                    \
    template<class... args> _name_(child_t, args&&... a)            \
    : _parent_                                                      \
    {                                                               \
        child, std::forward<args>(a)...                             \
    }{}                                                             \
};

// These are some common m::error, but not all of them; other declarations
// may be dispersed throughout ircd::m.
namespace ircd::m
{
	IRCD_M_EXCEPTION(error, UNKNOWN, http::INTERNAL_SERVER_ERROR);
	IRCD_M_EXCEPTION(error, NOT_FOUND, http::NOT_FOUND);
	IRCD_M_EXCEPTION(error, BAD_REQUEST, http::BAD_REQUEST);
	IRCD_M_EXCEPTION(BAD_REQUEST, BAD_JSON, http::BAD_REQUEST);
	IRCD_M_EXCEPTION(error, BAD_SIGNATURE, http::UNAUTHORIZED);
	IRCD_M_EXCEPTION(error, ACCESS_DENIED, http::UNAUTHORIZED);
}

template<class... args>
ircd::m::error::error(const http::code &status,
                      const string_view &errcode,
                      const char *const &fmt,
                      args&&... a)
:http::error
{
	status, [&]() -> json::strung
	{
		char estr[512]; const auto len
		{
			fmt::sprintf{estr, fmt, std::forward<args>(a)...}
		};

		return json::members
		{
			{ "errcode",  errcode                        },
			{ "error",    string_view{estr, size_t(len)} }
		};
	}()
}{}

template<class... args>
ircd::m::error::error(const string_view &errcode,
                      const char *const &fmt,
                      args&&... a)
:error
{
	http::BAD_REQUEST, errcode, fmt, std::forward<args>(a)...
}{}

inline
ircd::m::error::error(std::string c)
:http::error{http::INTERNAL_SERVER_ERROR, std::move(c)}
{}

inline
ircd::m::error::error(const http::code &c)
:http::error{c}
{}

inline
ircd::m::error::error(const http::code &c,
                      const json::members &members)
:http::error{c, json::strung(members)}
{}

inline
ircd::m::error::error(const http::code &c,
                      const json::iov &iov)
:http::error{c, json::strung(iov)}
{}

inline
ircd::m::error::error(const http::code &c,
                      const json::object &object)
:http::error{c, std::string{object}}
{}
