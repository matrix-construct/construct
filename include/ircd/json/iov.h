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

namespace ircd::json
{
	struct iov;
}

/// A forward list to compose JSON efficiently on the stack.
///
/// The IOV gathers members for a JSON object being assembled from various
/// sources and presents an iteration to a generator. This prevents the need
/// for multiple generations and copying to occur before the final JSON is
/// realized, if ever.
///
/// Add and remove items on the IOV by construction and destruction one of
/// the node objects. The IOV has a standard forward list interface, only use
/// that to observe and sort/rearrange the IOV. Do not add or remove things
/// that way.
///
/// Nodes support a single member each. To support initializer_list syntax
/// the iov allocates and internally manages the iov node that should have
/// been on your stack.
///
struct ircd::json::iov
:ircd::iov<ircd::json::member>
{
	IRCD_EXCEPTION(json::error, error);
	IRCD_EXCEPTION(error, exists);

	struct push;
	struct add;
	struct add_if;
	struct set;
	struct set_if;

  public:
	bool has(const string_view &key) const;
	const value &at(const string_view &key) const;

	iov() = default;

	friend string_view stringify(mutable_buffer &, const iov &);
	friend std::ostream &operator<<(std::ostream &, const iov &);
	friend size_t serialized(const iov &);
};

struct ircd::json::iov::push
:protected ircd::json::iov::node
{
	push(iov &iov, json::member m)
	:node{iov, std::move(m)}
	{}

	push() = default;
};

struct ircd::json::iov::add
:protected ircd::json::iov::node
{
	add(iov &, member);
	add() = default;
};

struct ircd::json::iov::add_if
:protected ircd::json::iov::node
{
	add_if(iov &, const bool &, member);
	add_if() = default;
};

struct ircd::json::iov::set
:protected ircd::json::iov::node
{
	set(iov &, member);
	set() = default;
};

struct ircd::json::iov::set_if
:protected ircd::json::iov::node
{
	set_if(iov &, const bool &, member);
	set_if() = default;
};
