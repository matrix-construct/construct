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
	BAD_REQUEST             = 400,
	NOT_FOUND               = 404,
	METHOD_NOT_ALLOWED      = 405,

	INTERNAL_SERVER_ERROR   = 500,
};

extern std::map<code, string_view> reason;

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
	using closure = std::function<void (const std::pair<string_view, string_view> &)>;

	string_view host;
	string_view expect;
	string_view te;
	size_t content_length {0};

	headers(parse::context &, const closure & = {});
};

struct content
:std::string
{
	content(parse::context &, const headers &);
};

struct request
{
	struct head;
	struct body;
};

struct response
{
	struct head;
	struct body;
};

struct request::head
:line::request
,headers
{
	head(parse::context &pc, const headers::closure &c = {})
	:line::request{pc}
	,headers{pc, c}
	{}
};

struct request::body
:content
{
	body(parse::context &pc, const head &h)
	:content{pc, h}
	{}
};

struct response::head
:line::response
,headers
{
	head(parse::context &pc, const headers::closure &c = {})
	:line::response{pc}
	,headers{pc, c}
	{}
};

struct response::body
:content
{
	body(parse::context &pc, const head &h)
	:content{pc, h}
	{}
};

} // namespace http
} // namespace ircd
