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

// ircd::json::builder is forward list to compose JSON dynamically and
// efficiently on the stack. The product of the builder is an iteration of the
// added members for use by stringifying and iovectoring. This gathers the
// members on a trip up the stack without rewriting a JSON string at each frame.
//
namespace ircd::json
{
	struct builder;

	using member_closure = std::function<void (const member &)>;
	using member_closure_bool = std::function<bool (const member &)>;
	using builder_closure_bool = std::function<bool (const builder &)>;

	builder *head(builder *const &);
	builder *next(builder *const &);
	builder *prev(builder *const &);
	builder *tail(builder *);
	builder *find(builder *, const builder_closure_bool &);

	const builder *head(const builder *const &);
	const builder *next(const builder *const &);
	const builder *prev(const builder *const &);
	const builder *tail(const builder *);
	const builder *find(const builder *, const builder_closure_bool &);

	void for_each(const builder &, const member_closure &);
	bool until(const builder &, const member_closure_bool &);
	size_t count(const builder &, const member_closure_bool &);
	size_t count(const builder &);
}

struct ircd::json::builder
{
	struct add;
	struct set;
	struct push;

	IRCD_EXCEPTION(json::error, error);
	IRCD_EXCEPTION(error, exists);

	member m;
	const members *ms;
	builder *head;
	builder *child;

	bool test(const member_closure_bool &) const;

	// recursive!
	const member *find(const string_view &key) const;
	const json::value &at(const string_view &key) const;
	bool has(const string_view &key) const;

	builder(member m = {},
	        const members *const &ms = nullptr,
	        builder *const &head = nullptr,
	        builder *const &child = nullptr)
	:m{std::move(m)}
	,ms{ms}
	,head{head}
	,child{child}
	{}

	builder(const members &ms)
	:builder{{}, &ms}
	{}

	builder(member m)
	:builder{std::move(m)}
	{}

	friend string_view stringify(mutable_buffer &, const builder &);
};

struct ircd::json::builder::push
:private ircd::json::builder
{
	push(builder &head, const members &ms)
	:builder{{}, &ms, &head}
	{
		tail(&head)->child = this;
	}

	push(builder &head, member m)
	:builder{std::move(m), nullptr, &head}
	{
		tail(&head)->child = this;
	}
};

struct ircd::json::builder::add
:private ircd::json::builder
{
	add(builder &head, const members &ms);
	add(builder &head, member member);
};

struct ircd::json::builder::set
:private ircd::json::builder
{
	set(builder &head, const members &ms);
	set(builder &head, member member);
};

inline ircd::json::builder *
ircd::json::find(builder *builder,
                 const builder_closure_bool &test)
{
	for(; builder; builder = next(builder))
		if(test(*builder))
			return builder;

	return nullptr;
}

inline const ircd::json::builder *
ircd::json::find(const builder *builder,
                 const builder_closure_bool &test)
{
	for(; builder; builder = next(builder))
		if(test(*builder))
			return builder;

	return nullptr;
}

inline ircd::json::builder *
ircd::json::tail(builder *ret)
{
	while(ret && next(ret))
		ret = next(ret);

	return ret;
}

inline const ircd::json::builder *
ircd::json::tail(const builder *ret)
{
	while(ret && next(ret))
		ret = next(ret);

	return ret;
}

inline ircd::json::builder *
ircd::json::prev(builder *const &builder)
{
	assert(builder);
	auto *ret(builder->head);
	for(; ret; ret = next(ret))
		if(next(ret) == builder)
			return ret;

	return nullptr;
}

inline const ircd::json::builder *
ircd::json::prev(const builder *const &builder)
{
	assert(builder);
	const auto *ret(builder->head);
	for(; ret; ret = next(ret))
		if(next(ret) == builder)
			return ret;

	return nullptr;
}

inline ircd::json::builder *
ircd::json::next(builder *const &builder)
{
	assert(builder);
	return builder->child;
}

inline const ircd::json::builder *
ircd::json::next(const builder *const &builder)
{
	assert(builder);
	return builder->child;
}

inline ircd::json::builder *
ircd::json::head(builder *const &builder)
{
	assert(builder);
	return builder->head;
}

inline const ircd::json::builder *
ircd::json::head(const builder *const &builder)
{
	assert(builder);
	return builder->head;
}
