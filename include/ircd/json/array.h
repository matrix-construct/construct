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
#define HAVE_IRCD_JSON_ARRAY_H

namespace ircd {
namespace json {

struct array
{
	struct const_iterator;

    using value_type = const string_view;
    using pointer = value_type *;
    using reference = value_type &;
    using iterator = const_iterator;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;
    using key_compare = std::less<string_view>;

	string_view state;

	operator string_view() const                 { return state;                                   }

	const_iterator end() const;
	const_iterator begin() const;

	bool empty() const;
	size_t size() const;

	array(const string_view &state = {})
	:state{state}
	{}
};

struct array::const_iterator
{
	using value_type = const string_view;
	using pointer = value_type *;
	using reference = value_type &;
	using difference_type = size_t;
	using iterator_category = std::forward_iterator_tag;

  protected:
	friend class array;

	const char *start;
	const char *stop;
	string_view state;

	const_iterator(const char *const &start, const char *const &stop)
	:start{start}
	,stop{stop}
	{}

  public:
	auto operator==(const const_iterator &o) const     { return start == o.start && stop == o.stop;      }
	auto operator!=(const const_iterator &o) const     { return !(*this == o);                           }
	value_type *operator->() const         { return &state;                                  }
	value_type &operator*() const          { return *operator->();                           }

	const_iterator &operator++();
};

} // namespace json
} // namespace ircd

inline size_t
ircd::json::array::size()
const
{
	return std::distance(begin(), end());
}

inline bool
ircd::json::array::empty()
const
{
	return state.empty();
}
