// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#pragma once
#define HAVE_IRCD_UTIL_VA_RTTI_H

namespace ircd::util
{
	struct va_rtti;

	const size_t VA_RTTI_MAX_SIZE
	{
		12
	};
}

//
// Similar to a va_list, but conveying std-c++ type data acquired from a variadic template
// parameter pack acting as the ...) elipsis. This is used to implement fmt::snprintf(),
// exceptions and logger et al in their respective translation units rather than the header
// files.
//
// Limitations: The choice of a fixed array of N is because std::initializer_list doesn't
// work here and other containers may be heavy in this context. Ideas to improve this are
// welcome.
//
struct ircd::util::va_rtti
:std::array<std::pair<const void *, const std::type_info *>, ircd::util::VA_RTTI_MAX_SIZE>
{
	using base_type = std::array<value_type, ircd::util::VA_RTTI_MAX_SIZE>;

	static constexpr size_t max_size()
	{
		return std::tuple_size<base_type>();
	}

	size_t argc;
	const size_t &size() const
	{
		return argc;
	}

	template<class... Args>
	va_rtti(Args&&... args)
	:base_type
	{{
		std::make_pair(std::addressof(args), std::addressof(typeid(Args)))...
	}}
	,argc
	{
		sizeof...(args)
	}
	{
		assert(argc <= max_size());
	}
};

static_assert
(
	sizeof(ircd::util::va_rtti) == (ircd::util::va_rtti::max_size() * 16) + 8,
	"va_rtti should be (8 + 8) * N + 8;"
	" where 8 + 8 are the two pointers carrying the argument and its type data;"
	" where N is the max arguments;"
	" where the final + 8 bytes holds the actual number of arguments passed;"
);
