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
#define HAVE_IRCD_HTTP_H

/// HyperText TransPort: formal grammars & tools
namespace ircd::http
{
	enum code :int;
	struct error;
	struct line;
	struct query;
	struct headers;
	struct content;
	struct request;
	struct response;

	string_view status(const enum code &);
	enum code status(const string_view &);

	string_view urlencode(const string_view &url, const mutable_buffer &);
	string_view urldecode(const string_view &url, const mutable_buffer &);
}

namespace ircd
{
	using http::urldecode;
	using http::urlencode;
}

//
// Add more as you go...
//
enum ircd::http::code
:int
{
	CONTINUE                                = 100,
	SWITCHING_PROTOCOLS                     = 101,

	OK                                      = 200,
	CREATED                                 = 201,
	ACCEPTED                                = 202,
	NON_AUTHORITATIVE_INFORMATION           = 203,
	NO_CONTENT                              = 204,
	PARTIAL_CONTENT                         = 206,

	MULTIPLE_CHOICES                        = 300,
	MOVED_PERMANENTLY                       = 301,
	FOUND                                   = 302,
	SEE_OTHER                               = 303,
	NOT_MODIFIED                            = 304,
	TEMPORARY_REDIRECT                      = 305,
	PERMANENT_REDIRECT                      = 306,

	BAD_REQUEST                             = 400,
	UNAUTHORIZED                            = 401,
	FORBIDDEN                               = 403,
	NOT_FOUND                               = 404,
	METHOD_NOT_ALLOWED                      = 405,
	REQUEST_TIMEOUT                         = 408,
	CONFLICT                                = 409,
	REQUEST_URI_TOO_LONG                    = 414,
	EXPECTATION_FAILED                      = 417,
	IM_A_TEAPOT                             = 418,
	UNPROCESSABLE_ENTITY                    = 422,
	TOO_MANY_REQUESTS                       = 429,
	REQUEST_HEADER_FIELDS_TOO_LARGE         = 431,

	INTERNAL_SERVER_ERROR                   = 500,
	NOT_IMPLEMENTED                         = 501,
	SERVICE_UNAVAILABLE                     = 503,
	HTTP_VERSION_NOT_SUPPORTED              = 505,
	INSUFFICIENT_STORAGE                    = 507,
};

/// Root exception for HTTP.
struct ircd::http::error
:ircd::error
{
	enum code code;
	std::string content;

	error(const enum code &, std::string content = {});
};

/// Represents a single \r\n delimited line used in HTTP.
///
/// This object is just a string_view of that line. The actual data backing
/// that view is the responsibility of the user. This object is constructed
/// with an ircd::parse::capstan argument which is used by the formal grammar
/// in the constructor.
///
struct ircd::http::line
:string_view
{
	struct request;
	struct response;
	struct header;

	using string_view::string_view;
	line(parse::capstan &);
};

namespace ircd::http
{
	using header = line::header;
}

/// Represents a 'request line' or the first line a client sends to a server.
///
/// This is a dual-use class. For HTTP clients, one may simply connect the
/// members to the proper strings and then pass this structure to a function
/// making a client request. For HTTP servers, pass an http::line to the ctor
/// and the formal grammar will set the members appropriately. The actual data
/// behind these members is the responsibility of the user.
///
struct ircd::http::line::request
{
	string_view method;
	string_view path;
	string_view query;
	string_view fragment;
	string_view version;

	request(const line &);
	request() = default;
};

/// Represents a 'response line' or the first line a server sends to a client.
///
/// This is a dual-use class and symmetric to the http::line::request class.
/// Servers may set the members and then use this object to respond to a client
/// while clients should provide an http::line to the constructor which will
/// fill in the members.
///
struct ircd::http::line::response
{
	string_view version;
	string_view status;
	string_view reason;

	response(const line &);
	response() = default;
};

/// Represents a single key/value pair in a query string.
///
/// This is used by the ircd::http::query::string object when parsing query
/// strings.
///
struct ircd::http::query
:std::pair<string_view, string_view>
{
	struct string;

	bool operator<(const string_view &s) const   { return iless(first, s);                         }
	bool operator==(const string_view &s) const  { return iequals(first, s);                       }

	using std::pair<string_view, string_view>::pair;
	query() = default;
};

/// Tool for parsing an HTTP query string.
///
/// Query string is read as a complete string off the tape (into request.query)
/// and not parsed further. To make queries into that string use this class to
/// view it. Once this object is constructed by viewing the whole query string,
/// the member functions invoke the formal grammar to get individual key/value
/// pairs.
///
struct ircd::http::query::string
:string_view
{
	void for_each(const std::function<void (const query &)> &) const;
	bool until(const std::function<bool (const query &)> &) const;

	string_view at(const string_view &key) const;
	string_view operator[](const string_view &key) const;

	using string_view::string_view;
};

/// Represents an HTTP header key/value pair.
///
/// This is a dual-use class. Those sending headers will simply fill in the
/// components of the std::pair. Those receiving headers can pass the ctor an
/// ircd::http::line which will construct the pair using the formal grammars.
///
struct ircd::http::line::header
:std::pair<string_view, string_view>
{
	bool operator<(const string_view &s) const   { return iless(first, s);                         }
	bool operator==(const string_view &s) const  { return iequals(first, s);                       }

	using std::pair<string_view, string_view>::pair;
	header(const line &);
	header() = default;
};

/// This device allows parsing HTTP headers directly off the wire without state
///
/// The constructor of this object contains the grammar to read HTTP headers
/// from the capstan and then proffer them one by one to the provided closure,
/// that's all it does.
///
struct ircd::http::headers
:string_view
{
	using header = line::header;
	using closure = std::function<void (const header &)>;

	headers(parse::capstan &, const closure & = {});
};

/// Represents the content of an HTTP request after the head.
///
/// Use the request::content / response::content wrappers. They ensure the
/// proper amount of content is read and the tape is in the right position
/// for the next request with exception safety. In other words, this object
/// ensures the capstan is in the proper place for the next request no matter
/// what happens; whether an exception happened, or whether the user simply
/// didn't care to read the content. The capstan MUST advance Content-Length
/// bytes in any case.
///
struct ircd::http::content
:string_view
{
	IRCD_OVERLOAD(discard)
	IRCD_OVERLOAD(chunked)

	content(parse::capstan &, const size_t &length, discard_t);
	content(parse::capstan &, const size_t &length);
	content(parse::capstan &, chunked_t);
	content() = default;
};

/// HTTP response suite. Functionality to send and receive responses.
///
struct ircd::http::response
{
	struct head;
	struct content;
	struct chunked;

	using write_closure = std::function<void (const ilist<const_buffer> &)>;
	using proffer = std::function<void (const head &)>;
	using header = line::header;

	response(const code &,
	         const string_view &content,
	         const write_closure &,
	         const vector_view<const header> & = {});

	response(parse::capstan &,
	         content *const &          = nullptr,
	         const proffer &           = nullptr,
	         const headers::closure &  = {});

	response() = default;
};

struct ircd::http::response::chunked
:response
{
	struct chunk;

	write_closure closure;

	chunked(const code &,
	        const write_closure &,
	        const vector_view<const header> &headers);

	chunked(const chunked &) = delete;
	~chunked() noexcept;
};

struct ircd::http::response::chunked::chunk
{
	chunk(chunked &, const const_buffer &);
};

/// Represents an HTTP response head. This is for receiving responses only.
///
struct ircd::http::response::head
:line::response
{
	size_t content_length {0};
	string_view transfer_encoding;

	string_view headers;

	head(parse::capstan &pc, const headers::closure &c = {});
};

/// Represents an HTTP response content. This is for receiving only.
///
struct ircd::http::response::content
:http::content
{
	content(parse::capstan &pc, const head &h, discard_t)
	:http::content{pc, h.content_length, discard}
	{}

	content(parse::capstan &pc, const head &h, chunked_t)
	:http::content{pc, chunked}
	{}

	content(parse::capstan &pc, const head &h)
	:http::content{pc, h.content_length}
	{}

	content() = default;
};

/// HTTP request suite. Functionality to send and receive requests.
///
struct ircd::http::request
{
	struct head;
	struct content;

	using write_closure = std::function<void (const ilist<const_buffer> &)>;
	using proffer = std::function<void (const head &)>;
	using header = line::header;

	request(const string_view &host            = {},
	        const string_view &method          = "GET",
	        const string_view &path            = "/",
	        const string_view &query           = {},
	        const string_view &content         = {},
	        const write_closure &              = nullptr,
	        const vector_view<const header> &  = {});

	request(parse::capstan &,
	        content *const &          = nullptr,
	        const write_closure &     = nullptr,
	        const proffer &           = nullptr,
	        const headers::closure &  = {});
};

/// Represents an HTTP request head. This is only for receiving requests.
///
struct ircd::http::request::head
:line::request
{
	string_view host;
	string_view expect;
	string_view te;
	string_view authorization;
	string_view connection;
	size_t content_length {0};

	string_view headers;

	head(parse::capstan &pc, const headers::closure &c = {});
};

/// Represents an HTTP request content. This is only for receiving content.
///
struct ircd::http::request::content
:http::content
{
	content(parse::capstan &pc, const head &h, discard_t)
	:http::content{pc, h.content_length, discard}
	{}

	content(parse::capstan &pc, const head &h)
	:http::content{pc, h.content_length}
	{}
};
