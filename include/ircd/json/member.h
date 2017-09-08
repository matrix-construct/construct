/*
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
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
 */

#pragma once
#define HAVE_IRCD_JSON_MEMBER_H

namespace ircd::json
{
	struct member;

	size_t size(const member *const &begin, const member *const &end);
	object serialize(const member *const *const &begin, const member *const *const &end, char *&start, char *const &stop);
	object serialize(const member *const &begin, const member *const &end, char *&start, char *const &stop);
	size_t print(char *const &buf, const size_t &max, const member *const &begin, const member *const &end);
	std::string string(const member *const &begin, const member *const &end);

	using members = std::initializer_list<member>;
	object serialize(const members &, char *&start, char *const &stop);
	size_t print(char *const &buf, const size_t &max, const members &);
	std::string string(const members &);

	array serialize(const std::vector<json::object> &, char *&start, char *const &stop);
	string_view stringify(char *const &buf, const size_t &max, const members &);
}

struct ircd::json::member
:std::pair<value, value>
{
	template<class K> member(const K &k, std::initializer_list<member> v);
	template<class K, class V> member(const K &k, V&& v);
	explicit member(const string_view &k);
	explicit member(const object::member &m);
	member() = default;

	friend bool operator==(const member &a, const member &b);
	friend bool operator!=(const member &a, const member &b);
	friend bool operator<(const member &a, const member &b);

	friend bool operator==(const member &a, const string_view &b);
	friend bool operator!=(const member &a, const string_view &b);
	friend bool operator<(const member &a, const string_view &b);

	friend std::ostream &operator<<(std::ostream &, const member &);
};

template<class K,
         class V>
ircd::json::member::member(const K &k,
                           V&& v)
:std::pair<value, value>
{
	value { k }, value { std::forward<V>(v) }
}
{}

template<class K>
ircd::json::member::member(const K &k,
                           std::initializer_list<member> v)
:std::pair<value, value>
{
	value { k }, value { std::make_unique<index>(std::move(v)) }
}
{}

inline
ircd::json::member::member(const object::member &m)
:std::pair<value, value>
{
	m.first, value { m.second, type(m.second) }
}
{}

inline
ircd::json::member::member(const string_view &k)
:std::pair<value, value>
{
	k, string_view{}
}
{}

inline bool
ircd::json::operator<(const member &a, const member &b)
{
	return a.first < b.first;
}

inline bool
ircd::json::operator!=(const member &a, const member &b)
{
	return a.first != b.first;
}

inline bool
ircd::json::operator==(const member &a, const member &b)
{
	return a.first == b.first;
}

inline bool
ircd::json::operator<(const member &a, const string_view &b)
{
	return string_view(a.first.string, a.first.len) < b;
}

inline bool
ircd::json::operator!=(const member &a, const string_view &b)
{
	return string_view(a.first.string, a.first.len) != b;
}

inline bool
ircd::json::operator==(const member &a, const string_view &b)
{
	return string_view(a.first.string, a.first.len) == b;
}
