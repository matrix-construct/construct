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

namespace ircd {
namespace http {

enum code
{
	CONTINUE                                = 100,
	SWITCHING_PROTOCOLS                     = 101,

	OK                                      = 200,
	CREATED                                 = 201,
	ACCEPTED                                = 202,
	NON_AUTHORITATIVE_INFORMATION           = 203,
	NO_CONTENT                              = 204,

	BAD_REQUEST                             = 400,
	UNAUTHORIZED                            = 401,
	FORBIDDEN                               = 403,
	NOT_FOUND                               = 404,
	METHOD_NOT_ALLOWED                      = 405,
	REQUEST_TIMEOUT                         = 408,
	EXPECTATION_FAILED                      = 417,
	TOO_MANY_REQUESTS                       = 429,
	REQUEST_HEADER_FIELDS_TOO_LARGE         = 431,

	INTERNAL_SERVER_ERROR                   = 500,
	NOT_IMPLEMENTED                         = 501,
	SERVICE_UNAVAILABLE                     = 503,
	HTTP_VERSION_NOT_SUPPORTED              = 505,
};

extern std::map<code, std::string> reason;

struct error
:ircd::error
{
	enum code code;
	std::string content;

	error(const enum code &, const string_view &reason = {});
};

struct line
:string_view
{
	struct request;
	struct response;
	struct header;

	using string_view::string_view;
	line(parse::context &);
};

struct line::request
{
	string_view method;
	string_view resource;
	string_view version;

	request(const line &);
	request() = default;
};

struct line::response
{
	string_view version;
	string_view status;
	string_view reason;

	response(const line &);
	response() = default;
};

struct line::header
:std::pair<string_view, string_view>
{
	bool operator<(const string_view &s) const   { return iless(first, s);                         }
	bool operator==(const string_view &s) const  { return iequals(first, s);                       }

	using std::pair<string_view, string_view>::pair;
	header(const line &);
	header() = default;
};

struct headers
{
	using closure = std::function<void (const line::header &)>;

	headers(parse::context &, const closure & = {});
};

struct content
:string_view
{
	IRCD_OVERLOAD(discard)

	content(parse::context &, const size_t &length, discard_t);
	content(parse::context &, const size_t &length);
	content() = default;
};

struct response
{
	struct head;
	struct content;

	using write_closure = std::function<void (const_buffers &)>;
	using proffer = std::function<void (const head &)>;

	response(const code &,
	         const string_view &content,
	         const write_closure &,
	         const std::initializer_list<line::header> & = {});

	response(parse::context &,
	         content *const & = nullptr,
	         const proffer & = nullptr,
	         const headers::closure & = {});
};

struct response::head
:line::response
{
	size_t content_length {0};

	head(parse::context &pc, const headers::closure &c = {});
};

struct response::content
:http::content
{
	content(parse::context &pc, const head &h, discard_t)
	:http::content{pc, h.content_length, discard}
	{}

	content(parse::context &pc, const head &h)
	:http::content{pc, h.content_length}
	{}

	content() = default;
};

struct request
{
	struct head;
	struct content;

	using write_closure = std::function<void (const_buffers &)>;
	using proffer = std::function<void (const head &)>;

	request(const string_view &host       = {},
	        const string_view &method     = "GET",
	        const string_view &resource   = "/",
	        const string_view &content    = {},
	        const write_closure &         = nullptr,
	        const std::initializer_list<line::header> & = {});

	request(parse::context &,
	        content *const & = nullptr,
	        const write_closure & = nullptr,
	        const proffer & = nullptr,
	        const headers::closure & = {});
};

struct request::head
:line::request
{
	string_view host;
	string_view expect;
	string_view te;
	size_t content_length {0};

	head(parse::context &pc, const headers::closure &c = {});
};

struct request::content
:http::content
{
	content(parse::context &pc, const head &h, discard_t)
	:http::content{pc, h.content_length, discard}
	{}

	content(parse::context &pc, const head &h)
	:http::content{pc, h.content_length}
	{}
};

} // namespace http
} // namespace ircd
