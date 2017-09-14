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
#define HAVE_IRCD_JSON_H

/// JavaScript Object Notation: formal grammars & tools
///
/// The IRCd JSON subsystem is meant to be a fast, safe, and extremely
/// lightweight interface. We have taken a somewhat non-traditional approach
/// and it's important for the developer to understand a few things.
///
/// Most JSON interfaces are functions to convert some JSON input to and from
/// text into native-machine state like JSON.parse() for JS, boost::ptree, etc.
/// For a parsing operation, they make a pass recursing over the entire text,
/// allocating native structures, copying data into them, indexing their keys,
/// and perhaps performing native-type conversions and checks to present the
/// user with a final tree of machine-state usable in their language. The
/// original input is then discarded.
///
/// Instead, we are interested in having the ability to *compute directly over
/// JSON text* itself, and perform the allocating, indexing, copying and
/// converting entirely at the time and place of our discretion -- if ever.
///
/// The core of this system is a robust and efficient abstract formal grammar
/// built with boost::spirit. The formal grammar provides a *proof of robust-
/// ness*: security vulnerabilities are more easily spotted by vetting this
/// grammar rather than laboriously tracing the program flow of an informal
/// handwritten parser.
///
/// Next we have taught boost::spirit how to parse into std::string_view rather
/// than std::string. Parsing is now a composition of pointers into the original
/// string of JSON. No dynamic allocation ever takes place. No copying of data
/// ever takes place. IRCd can service an entire request from the original
/// network input with absolutely minimal requisite cost.
///
/// The output side is also ambitious but probably a little more friendly to
/// the developer. We leverage boost::spirit here also providing *formally
/// proven* output safety. In other words, the grammar prevents exploits like
/// injecting and terminating JSON as it composes the output.
///
namespace ircd::json
{
	IRCD_EXCEPTION(ircd::error, error);
	IRCD_EXCEPTION(error, parse_error);
	IRCD_EXCEPTION(error, print_error);
	IRCD_EXCEPTION(error, type_error);
	IRCD_EXCEPTION(error, not_found);

	struct array;
	struct object;
	struct value;
	struct member;
	struct iov;

	enum type
	{
		STRING  = 0,
		OBJECT  = 1,
		ARRAY   = 2,
		NUMBER  = 3,
		LITERAL = 4,
	};
	enum type type(const string_view &);
	enum type type(const string_view &, std::nothrow_t);
	string_view reflect(const enum type &);

	/// Higher order type beyond a string to cleanly delimit multiple keys.
	using path = std::initializer_list<string_view>;
	std::ostream &operator<<(std::ostream &, const path &);

	/// These templates are generic frontends for building a JSON string. They
	/// eventually all lead to the stringify() friend function of the argument
	/// you pass to the template.
	template<class... T> string_view stringify(const mutable_buffer &&mb, T&&... t);
	template<class... T> size_t print(char *const &buf, const size_t &max, T&&... t);
	template<class... T> std::string string(T&&... t);

	size_t serialized(const string_view &);

	using members = std::initializer_list<member>;
}

#include "json/array.h"
#include "json/object.h"
#include "json/value.h"
#include "json/member.h"
#include "json/property.h"
#include "json/iov.h"
#include "json/tuple.h"

namespace ircd
{
	using json::operator<<;
}

///
/// Convenience template for const rvalue mutable_buffers or basically
/// allowing a bracket initialization of a mutable_buffer in the argument
/// to stringify()
///
template<class... T>
ircd::string_view
ircd::json::stringify(const mutable_buffer &&mb,
                      T&&... t)
{
	mutable_buffer mbc{mb};
	return stringify(mbc, std::forward<T>(t)...);
}

///
/// Convenience template using the syntax print(buf, sizeof(buf), ...)
/// which stringifies with null termination into buffer.
///
template<class... T>
size_t
ircd::json::print(char *const &buf,
                  const size_t &max,
                  T&&... t)
{
	if(!max)
		return 0;

	mutable_buffer mb
	{
		buf, max - 1
	};

	const auto sv
	{
		stringify(mb, std::forward<T>(t)...)
	};

	assert(sv.size() < max);
	buf[sv.size()] = '\0';
	return sv.size();
}

///
/// Convenience template using the syntax string(...) which returns
/// an std::string of the printed JSON
///
template<class... T>
std::string
ircd::json::string(T&&... t)
{
	const auto size
	{
		serialized(std::forward<T>(t)...)
	};

	std::string ret(size, char{});
	const auto buf{const_cast<char *>(ret.data())};
	const auto max{ret.size() + 1};

	const auto printed
	{
		print(buf, max, std::forward<T>(t)...)
	};

	assert(printed == ret.size());
	return ret;
}

inline std::ostream &
ircd::json::operator<<(std::ostream &s, const path &p)
{
	auto it(std::begin(p));
	if(it != std::end(p))
	{
		s << *it;
		++it;
	}

	for(; it != std::end(p); ++it)
		s << '.' << *it;

	return s;
}
