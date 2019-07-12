// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

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
class ircd::m::error
:public http::error
{
	static thread_local char fmtbuf[4_KiB];

	IRCD_OVERLOAD(internal)
	error(internal_t, const http::code &, std::string object);

  protected:
	IRCD_OVERLOAD(child)
	template<class... args> error(child_t, args&&... a)
	:error{std::forward<args>(a)...}
	{}

  public:
	string_view errcode() const noexcept;
	string_view errstr() const noexcept;

	template<class... args> error(const http::code &, const string_view &errcode, const string_view &fmt, args&&...);
	template<class... args> error(const string_view &errcode, const string_view &fmt, args&&...);
	error(const http::code &, const json::object &object);
	error(const http::code &, const json::members &);
	error(const http::code &, const json::iov &);
	error(const http::code &);
	error(std::string);
	error();
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
#define IRCD_M_EXCEPTION(_parent_, _name_, _httpcode_)                  \
struct _name_                                                           \
: _parent_                                                              \
{                                                                       \
    _name_()                                                            \
    : _parent_                                                          \
    {                                                                   \
        child, _httpcode_, "M_"#_name_, "%s", http::status(_httpcode_)  \
    }{}                                                                 \
                                                                        \
    template<class... args> _name_(const string_view &fmt, args&&... a) \
    : _parent_                                                          \
    {                                                                   \
        child, _httpcode_, "M_"#_name_, fmt, std::forward<args>(a)...   \
    }{}                                                                 \
                                                                        \
    template<class... args> _name_(child_t, args&&... a)                \
    : _parent_                                                          \
    {                                                                   \
        child, std::forward<args>(a)...                                 \
    }{}                                                                 \
};

// These are some common m::error, but not all of them; other declarations
// may be dispersed throughout ircd::m.
namespace ircd::m
{
	IRCD_M_EXCEPTION(error, UNKNOWN, http::INTERNAL_SERVER_ERROR);
	IRCD_M_EXCEPTION(error, BAD_REQUEST, http::BAD_REQUEST);
	IRCD_M_EXCEPTION(error, BAD_JSON, http::BAD_REQUEST);
	IRCD_M_EXCEPTION(error, NOT_JSON, http::BAD_REQUEST);
	IRCD_M_EXCEPTION(error, BAD_SIGNATURE, http::UNAUTHORIZED);
	IRCD_M_EXCEPTION(error, ACCESS_DENIED, http::UNAUTHORIZED);
	IRCD_M_EXCEPTION(error, FORBIDDEN, http::FORBIDDEN);
	IRCD_M_EXCEPTION(error, NOT_FOUND, http::NOT_FOUND);
	IRCD_M_EXCEPTION(error, UNSUPPORTED, http::NOT_IMPLEMENTED);
	IRCD_M_EXCEPTION(error, NEED_MORE_PARAMS, http::MULTIPLE_CHOICES);
	IRCD_M_EXCEPTION(error, UNAVAILABLE, http::SERVICE_UNAVAILABLE);
	IRCD_M_EXCEPTION(error, BAD_PAGINATION, http::BAD_REQUEST);
}

template<class... args>
ircd::m::error::error(const http::code &status,
                      const string_view &errcode,
                      const string_view &fmt,
                      args&&... a)
:error
{
	internal, status, [&errcode, &fmt, &a...]() -> json::strung
	{
		const string_view str
		{
			fmt::sprintf{fmtbuf, fmt, std::forward<args>(a)...}
		};

		return json::members
		{
			{ "errcode",  errcode  },
			{ "error",    str      },
		};
	}()
}{}

template<class... args>
ircd::m::error::error(const string_view &errcode,
                      const string_view &fmt,
                      args&&... a)
:error
{
	http::INTERNAL_SERVER_ERROR, errcode, fmt, std::forward<args>(a)...
}
{}
