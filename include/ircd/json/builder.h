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
#define HAVE_IRCD_JSON_BUILDER_H

// ircd::json::builder is an interface to compose JSON dynamically and
// efficiently. The product of the builder is an iteration of the added members
// for use by stringifying and iovectoring. This gathers the members on a trip
// up the stack without rewriting a JSON string at each frame.
//
struct ircd::json::builder
{
	using member = index::member;
	using members = std::initializer_list<member>;
	using member_closure = std::function<void (const index::member &)>;
	using member_closure_bool = std::function<bool (const index::member &)>;

	const builder *parent;
	member m;
	const members *ms;

	void for_each(const member_closure &) const;
	bool until(const member_closure_bool &) const;
	size_t count(const member_closure_bool &) const;
	size_t count() const;

	const index::member *find(const string_view &key) const;
	const json::value &at(const string_view &key) const;

	builder(const builder *const &parent, const members *const &);
	builder(const builder *const &parent, member);

	friend string_view stringify(char *const &buf, const size_t &max, const builder &);
};

inline ircd::string_view
ircd::json::stringify(char *const &buf,
                      const size_t &max,
                      const builder &builder)
{
	size_t i(0);
	const auto num(builder.count());
	const builder::member *m[num];
	builder.for_each([&i, &m]
	(const auto &member)
	{
		m[i++] = &member;
	});

	char *p(buf);
	char *const e(buf + max);
	return serialize(m, m + num, p, e);
}

inline
ircd::json::builder::builder(const builder *const &parent,
                             member m)
:parent{parent}
,m{std::move(m)}
,ms{nullptr}
{
}

inline
ircd::json::builder::builder(const builder *const &parent,
                             const members *const &ms)
:parent{parent}
,m{}
,ms{ms}
{
}

inline const ircd::json::value &
ircd::json::builder::at(const string_view &key)
const
{
	const auto *const member(find(key));
	if(!member)
		throw not_found("'%s'", key);

	return member->second;
}

inline size_t
ircd::json::builder::count()
const
{
	return count([]
	(const auto &)
	{
		return true;
	});
}

inline size_t
ircd::json::builder::count(const member_closure_bool &closure)
const
{
	size_t ret(0);
	for_each([&closure, &ret]
	(const auto &member)
	{
		ret += closure(member);
	});

	return ret;
}

inline const ircd::json::index::member *
ircd::json::builder::find(const string_view &key)
const
{
	const member *ret;
	const auto test
	{
		[&key, &ret](const auto &member)
		{
			if(key == string_view{member.first})
			{
				ret = &member;
				return false;
			}
			else return true;
		}
	};

	return !until(test)? ret : nullptr;
}

inline void
ircd::json::builder::for_each(const member_closure &closure)
const
{
	if(ms)
		std::for_each(begin(*ms), end(*ms), closure);
	else
		closure(m);

	if(parent)
		parent->for_each(closure);
}

inline bool
ircd::json::builder::until(const member_closure_bool &closure)
const
{
	if(ms)
	{
		if(!ircd::until(begin(*ms), end(*ms), closure))
			return false;
	}
	else if(!closure(m))
		return false;

	if(parent)
		return parent->until(closure);

	return true;
}
