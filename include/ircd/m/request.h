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
#define HAVE_IRCD_M_REQUEST_H

namespace ircd {
namespace m    {

struct request
{
	string_view method;
	string_view path;
	string_view query;
	string_view access_token;
	std::string _content;
	json::object content;

	request(const string_view &method,
	        const string_view &path,
	        const string_view &query = {},
	        json::members body = {});

	request(const string_view &method,
	        const string_view &path,
	        const string_view &query,
	        const json::object &content);
};

} // namespace m
} // namespace ircd

inline
ircd::m::request::request(const string_view &method,
                          const string_view &path,
                          const string_view &query,
                          json::members body)
:method{method}
,path{path}
,query{query}
,_content{json::string(body)}
,content{_content}
{
}

inline
ircd::m::request::request(const string_view &method,
                          const string_view &path,
                          const string_view &query,
                          const json::object &content)
:method{method}
,path{path}
,query{query}
,content{content}
{
}
