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

namespace ircd {
namespace m    {

struct error
:http::error
{
	template<class... args> error(const http::code &, const string_view &errcode, const char *const &fmt, args&&...);
	template<class... args> error(const string_view &errcode, const char *const &fmt, args&&...);
	error(const http::code &, const json::doc &doc = {});
	error(const http::code &, const json::obj &obj);
};

} // namespace m
} // namespace ircd

inline
ircd::m::error::error(const http::code &c,
                      const json::obj &obj)
:http::error{c, std::string{obj}}
{}

inline
ircd::m::error::error(const http::code &c,
                      const json::doc &doc)
:http::error{c, std::string{doc}}
{}

template<class... args>
ircd::m::error::error(const string_view &errcode,
                      const char *const &fmt,
                      args&&... a)
:error
{
	http::BAD_REQUEST, errcode, fmt, std::forward<args>(a)...
}{}

template<class... args>
ircd::m::error::error(const http::code &status,
                      const string_view &errcode,
                      const char *const &fmt,
                      args&&... a)
:http::error
{
	status, [&]() -> std::string
	{
		char estr[256]; const auto estr_len
		{
			fmt::snprintf{estr, sizeof(estr), fmt, std::forward<args>(a)...}
		};

		return json::obj
		{
			{ "errcode",  errcode                      },
			{ "error",    string_view(estr, estr_len)  }
		};
	}()
}{}
