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
#define HAVE_IRCD_RESOURCE_H

namespace ircd
{
	struct client;
	struct resource;
}

struct ircd::resource
{
	IRCD_EXCEPTION(ircd::error, error)

	enum flag :uint;
	struct opts;
	struct method;
	struct request;
	struct response;

	static std::map<string_view, resource *, iless> resources;

	string_view path;
	string_view description;
	enum flag flags;
	std::map<string_view, method *> methods;
	unique_const_iterator<decltype(resources)> resources_it;

	string_view allow_methods_list(const mutable_buffer &buf);

  private:
	virtual void handle_request(client &, method &, resource::request &);

  public:
	method &operator[](const string_view &path);

	void operator()(client &, const http::request::head &, const string_view &content_partial);

	resource(const string_view &path, const opts &);
	resource(const string_view &path);
	resource() = default;
	virtual ~resource() noexcept;

	static resource &find(const string_view &path);
};

enum ircd::resource::flag
:uint
{
	DIRECTORY  = 0x01,
};

struct ircd::resource::opts
{
	/// developer's literal description of the resource
	string_view description
	{
		"no description"
	};

	/// flags for the resource
	flag flags
	{
		flag(0)
	};

	/// parameter count limits (DIRECTORY only)
	std::pair<short, short> parc
	{
		0,   // minimum params
		15   // maximum params
	};
};

struct ircd::resource::request
:json::object
{
	template<class> struct object;

	http::request::head head;
	string_view content;
	http::query::string query;
	string_view origin;
	string_view access_token;
	vector_view<string_view> parv;
	string_view param[8];
	m::user::id::buf user_id;

	request(const http::request::head &head,
	        const string_view &content)
	:json::object{content}
	,head{head}
	,content{content}
	,query{this->head.query}
	{}

	request() = default;
};

template<class tuple>
struct ircd::resource::request::object
:tuple
{
	resource::request &r;
	const http::request::head &head;
	const string_view &content;
	const http::query::string &query;
	const decltype(r.origin) &origin;
	const decltype(r.user_id) &user_id;
	const decltype(r.access_token) &access_token;
	const vector_view<string_view> &parv;
	const json::object &body;

	object(resource::request &r)
	:tuple{r}
	,r{r}
	,head{r.head}
	,content{r.content}
	,query{r.query}
	,origin{r.origin}
	,user_id{r.user_id}
	,access_token{r.access_token}
	,parv{r.parv}
	,body{r}
	{}
};

struct ircd::resource::response
{
	struct chunked;

	response(client &, const http::code &, const string_view &content_type, const size_t &content_length, const string_view &headers = {});
	response(client &, const string_view &str, const string_view &content_type, const http::code &, const vector_view<const http::header> &);
	response(client &, const string_view &str, const string_view &content_type, const http::code & = http::OK, const string_view &headers = {});
	response(client &, const json::object &str, const http::code & = http::OK);
	response(client &, const json::array &str, const http::code & = http::OK);
	response(client &, const json::members & = {}, const http::code & = http::OK);
	response(client &, const json::value &, const http::code & = http::OK);
	response(client &, const json::iov &, const http::code & = http::OK);
	response(client &, const http::code &, const json::members &);
	response(client &, const http::code &, const json::value &);
	response(client &, const http::code &, const json::iov &);
	response(client &, const http::code &);
	response() = default;
};

struct ircd::resource::response::chunked
:resource::response
{
	client *c {nullptr};

	size_t write(const const_buffer &chunk);
	bool finish();

	chunked(client &, const http::code &, const string_view &content_type, const string_view &headers = {});
	chunked(client &, const http::code &, const string_view &content_type, const vector_view<const http::header> &);
	chunked(client &, const http::code &, const vector_view<const http::header> &);
	chunked(client &, const http::code &);
	chunked(const chunked &) = delete;
	chunked(chunked &&) noexcept;
	chunked() = default;
	~chunked() noexcept;
};

struct ircd::resource::method
{
	using handler = std::function<response (client &, request &)>;

	enum flag
	{
		REQUIRES_AUTH         = 0x01,
		RATE_LIMITED          = 0x02,
		VERIFY_ORIGIN         = 0x04,
		CONTENT_DISCRETION    = 0x08,
	};

	struct opts
	{
		flag flags {(flag)0};

		/// Timeout specific to this resource.
		seconds timeout {30s};

		/// The maximum size of the Content-Length for this method. Anything
		/// larger will be summarily rejected with a 413.
		size_t payload_max {128_KiB};

		/// MIME type; first part is the Registry (i.e application) and second
		/// part is the format (i.e json). Empty value means nothing rejected.
		std::pair<string_view, string_view> mime;
	};

	string_view name;
	struct resource *resource;
	handler function;
	struct opts opts;
	unique_const_iterator<decltype(resource::methods)> methods_it;

  public:
	virtual response operator()(client &, request &);

	method(struct resource &, const string_view &name, const handler &, const struct opts &);
	method(struct resource &, const string_view &name, const handler &);
	virtual ~method() noexcept;
};
