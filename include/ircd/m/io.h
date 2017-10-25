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
#define HAVE_IRCD_M_IO_H

namespace ircd::m::io
{
	struct session;
	struct request;
	struct response;

	bool verify_x_matrix_authorization(const string_view &authorization, const string_view &method, const string_view &uri, const string_view &content);

	// Synchronous acquire
	size_t acquire(vector_view<event::fetch>);
	json::object acquire(event::fetch &);

	size_t acquire(vector_view<room::fetch>);
	json::array acquire(room::fetch &);

	size_t acquire(vector_view<room::state::fetch>);
	json::array acquire(room::state::fetch &);

	// Synchronous acquire with user buffer
	json::object get(const id::event &, const mutable_buffer &);
}

namespace ircd::m
{
	using io::get;
	using io::session;
	using io::request;
	using io::response;
}

struct ircd::m::event::fetch
{
	// out
	event::id event_id;
	mutable_buffer buf;
	net::remote hint;

	// in
	json::object pdu;
	std::exception_ptr error;
};

struct ircd::m::room::fetch
{
	// out
	event::id event_id;
	room::id room_id;
	mutable_buffer buf;
	net::remote hint;
	uint64_t limit {128};

	// in
	json::array pdus;
	json::array auth_chain;
	std::exception_ptr error;
};

struct ircd::m::room::state::fetch
{
	// out
	event::id event_id;
	room::id room_id;
	mutable_buffer buf;
	net::remote hint;
	uint64_t limit {128};

	// in
	json::array pdus;
	json::array auth_chain;
	std::exception_ptr error;
};

struct ircd::m::io::request
{
	struct authorization;
	struct keys;

	string_view origin;
	string_view destination;
	string_view method;
	string_view path;
	string_view query;
	string_view fragment;
	std::string _content;
	json::object content;

	string_view generate_authorization(const mutable_buffer &out) const;
	void operator()(server &, const vector_view<const http::header> & = {}) const;
	void operator()(const vector_view<const http::header> & = {}) const;

	request(const string_view &method,
	        const string_view &path,
	        const string_view &query = {},
	        json::members body = {})
	:method{method}
	,path{path}
	,query{query}
	,_content{json::strung(body)}
	,content{_content}
	{}

	request(const string_view &method,
	        const string_view &path,
	        const string_view &query,
	        const json::object &content)
	:method{method}
	,path{path}
	,query{query}
	,content{content}
	{}

	request() = default;
};

struct ircd::m::io::response
:json::object
{
	response(server &, parse::buffer &);
};

struct ircd::m::io::session
{
	ircd::server server;
	std::string destination;
	std::string access_token;

	json::object operator()(parse::buffer &pb, request &);

	session(const net::remote &);
	session() = default;
};
