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
#define HAVE_IRCD_M_ID_H

namespace ircd {
namespace m    {

const size_t USER_ID_BUFSIZE = 256;
const size_t ACCESS_TOKEN_BUFSIZE = 256;

inline bool
username_valid(const string_view &username)
{
	return true;
}

inline bool
mxid_valid(const string_view &mxid)
{
	const auto userhost(split(mxid, ':'));
	const auto &user(userhost.first);
	const auto &host(userhost.second);
	if(user.empty() || host.empty())
		return false;

	if(!startswith(user, '@') || user.size() == 1)
		return false;

	return true;
}

inline string_view
username_generate(char *const &buf,
                  const size_t &max)
{
	const uint32_t num(rand() % 100000U);
	const size_t len(snprintf(buf, max, "@guest%u", num));
	return { buf, len };
}

inline string_view
access_token_generate(char *const &buf,
                      const size_t &max)
{
	const int num[] { rand(), rand(), rand() };
	const size_t len(snprintf(buf, max, "charybdis%d%d%d", num[0], num[1], num[2]));
	return { buf, len };
}

struct id
{
	string_view user;
	string_view host;

	template<size_t size>
	id(string_view user, string_view host)
	:user{std::move(user)}
	,host{std::move(host)}
	{}

	id(const string_view &user, const string_view &host, char *const &buf, const size_t &max)
	:user{user}
	,host{host}
	{
		char gen_buf[USER_ID_BUFSIZE];
		fmt::snprintf(buf, max, "%s%s:%s",
		              user.empty() || startswith(user, '@')? "" : "@",
		              !user.empty()? user : username_generate(gen_buf, sizeof(gen_buf)),
		              host);
	}

	template<size_t size>
	id(const string_view &user, const string_view &host, char (&buf)[size])
	:id{user, host, buf, size}
	{}
};

} // namespace m
} // namespace ircd
