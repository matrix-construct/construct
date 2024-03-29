// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_RESOURCE_REQUEST_H

struct ircd::resource::request
:json::object
{
	template<class> struct object;

	http::request::head head;
	string_view content;
	http::query::string query;
	pair<string_view> agent;
	string_view params;
	vector_view<string_view> parv;
	string_view param[8];

  private:
	static pair<string_view> parse_agent(const http::request::head &) noexcept;

  public:
	request(const http::request::head &, const string_view &content) noexcept;
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
	const pair<string_view> &agent;
	const string_view &params;
	const vector_view<string_view> &parv;
	const json::object &body;

	object(resource::request &) noexcept;
};

template<class tuple>
inline
ircd::resource::request::object<tuple>::object(resource::request &r)
noexcept
:tuple{r}
,r{r}
,head{r.head}
,content{r.content}
,query{r.query}
,agent{r.agent}
,params{r.params}
,parv{r.parv}
,body{r}
{}
