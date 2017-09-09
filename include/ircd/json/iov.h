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
#define HAVE_IRCD_JSON_IOV_H

// ircd::json::iov is forward list to compose JSON dynamically and
// efficiently on the stack. The product of the iov is an iteration of the
// added members for use by stringifying and iovectoring. This gathers the
// members on a trip up the stack without rewriting a JSON string at each frame.
//
namespace ircd::json
{
	struct iov;

	using member_closure = std::function<void (const member &)>;
	using member_closure_bool = std::function<bool (const member &)>;
	using iov_closure_bool = std::function<bool (const iov &)>;

	iov *head(iov *const &);
	iov *next(iov *const &);
	iov *prev(iov *const &);
	iov *tail(iov *);
	iov *find(iov *, const iov_closure_bool &);

	const iov *head(const iov *const &);
	const iov *next(const iov *const &);
	const iov *prev(const iov *const &);
	const iov *tail(const iov *);
	const iov *find(const iov *, const iov_closure_bool &);

	void for_each(const iov &, const member_closure &);
	bool until(const iov &, const member_closure_bool &);
	size_t count(const iov &, const member_closure_bool &);
	size_t count(const iov &);
}

struct ircd::json::iov
{
	struct add;
	struct set;
	struct push;

	IRCD_EXCEPTION(json::error, error);
	IRCD_EXCEPTION(error, exists);

	iov *head {nullptr};
	iov *child {nullptr};
	const member *b {nullptr};
	const member *e {nullptr};
	member m;

	bool test(const member_closure_bool &) const;

	// recursive!
	const member *find(const string_view &key) const;
	const json::value &at(const string_view &key) const;
	bool has(const string_view &key) const;

	iov(iov *const &head,
	    iov *const &child,
	    member m)
	:head{head}
	,child{child}
	,m{std::move(m)}
	{}

	iov(iov *const &head,
	    iov *const &child,
	    const member *const &begin,
	    const member *const &end)
	:head{head}
	,child{child}
	,b{begin}
	,e{end}
	{}

	iov(iov *const &head,
	    iov *const &child,
	    const members &ms)
	:head{head}
	,child{child}
	,b{std::begin(ms)}
	,e{std::end(ms)}
	{}

	iov(const members &ms)
	:iov{nullptr, nullptr, std::begin(ms), std::end(ms)}
	{}

	iov(member m = {})
	:iov{nullptr, nullptr, std::move(m)}
	{}

	friend string_view stringify(mutable_buffer &, const iov &);
};

struct ircd::json::iov::push
:private ircd::json::iov
{
	template<class... args>
	push(iov &head, args&&... a)
	:iov{&head, nullptr, std::forward<args>(a)...}
	{
		tail(&head)->child = this;
	}
};

struct ircd::json::iov::add
:private ircd::json::iov
{
	add(iov &head, const members &ms);
	add(iov &head, member member);
};

struct ircd::json::iov::set
:private ircd::json::iov
{
	set(iov &head, const members &ms);
	set(iov &head, member member);
};

inline ircd::json::iov *
ircd::json::find(iov *iov,
                 const iov_closure_bool &test)
{
	for(; iov; iov = next(iov))
		if(test(*iov))
			return iov;

	return nullptr;
}

inline const ircd::json::iov *
ircd::json::find(const iov *iov,
                 const iov_closure_bool &test)
{
	for(; iov; iov = next(iov))
		if(test(*iov))
			return iov;

	return nullptr;
}

inline ircd::json::iov *
ircd::json::tail(iov *ret)
{
	while(ret && next(ret))
		ret = next(ret);

	return ret;
}

inline const ircd::json::iov *
ircd::json::tail(const iov *ret)
{
	while(ret && next(ret))
		ret = next(ret);

	return ret;
}

inline ircd::json::iov *
ircd::json::prev(iov *const &iov)
{
	assert(iov);
	auto *ret(iov->head);
	for(; ret; ret = next(ret))
		if(next(ret) == iov)
			return ret;

	return nullptr;
}

inline const ircd::json::iov *
ircd::json::prev(const iov *const &iov)
{
	assert(iov);
	const auto *ret(iov->head);
	for(; ret; ret = next(ret))
		if(next(ret) == iov)
			return ret;

	return nullptr;
}

inline ircd::json::iov *
ircd::json::next(iov *const &iov)
{
	assert(iov);
	return iov->child;
}

inline const ircd::json::iov *
ircd::json::next(const iov *const &iov)
{
	assert(iov);
	return iov->child;
}

inline ircd::json::iov *
ircd::json::head(iov *const &iov)
{
	assert(iov);
	return iov->head;
}

inline const ircd::json::iov *
ircd::json::head(const iov *const &iov)
{
	assert(iov);
	return iov->head;
}
