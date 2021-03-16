// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_VECTOR_VIEW_H

namespace ircd
{
	template<class T> struct vector_view;

	template<class T> bool empty(const vector_view<T> &) noexcept;
	template<class T> size_t size(const vector_view<T> &) noexcept;
	template<class T> T *data(const vector_view<T> &) noexcept;
}

/// Template to represent a contiguous vector or array in a generic way.
template<class T>
struct ircd::vector_view
{
	using value_type = T;
	using pointer = value_type *;
	using reference = value_type &;
	using difference_type = size_t;
	using iterator = value_type *;
	using const_iterator = const value_type *;

	pointer _data                                { nullptr                                         };
	pointer _stop                                { nullptr                                         };

  public:
	pointer data() const noexcept                { return _data;                                   }
	size_t size() const noexcept                 { return std::distance(_data, _stop);             }
	bool empty() const noexcept                  { return !size();                                 }

	const_iterator begin() const noexcept        { return data();                                  }
	const_iterator end() const noexcept          { return _stop;                                   }
	const_iterator cbegin() noexcept             { return data();                                  }
	const_iterator cend() noexcept               { return _stop;                                   }
	iterator begin() noexcept                    { return data();                                  }
	iterator end() noexcept                      { return _stop;                                   }

	// Bounds check in debug only.
	reference operator[](const size_t &pos) const noexcept
	{
		assert(pos < size());
		return *(data() + pos);
	}

	// Bounds check at runtime.
	reference at(const size_t &pos) const
	{
		if(unlikely(pos >= size()))
			throw std::out_of_range
			{
				"vector_view::range_check"
			};

		return operator[](pos);
	}

	reference back() const
	{
		return at(size() - 1);
	}

	reference front() const
	{
		return at(0);
	}

	vector_view(const pointer __restrict__ start, const pointer __restrict__ stop)
	noexcept
	:_data{start}
	,_stop{stop}
	{}

	vector_view(const pointer __restrict__ start, const size_t size)
	noexcept
	:vector_view(start, start + size)
	{}

	vector_view(const vector_view<value_type> &start, const size_t &size)
	noexcept
	:vector_view(start.data(), std::min(start.size(), size))
	{}

	vector_view(const std::initializer_list<value_type> &list)
	noexcept
	:vector_view(std::begin(list), std::end(list))
	{}

	template<class U,
	         class A>
	vector_view(const std::vector<U, A> &v)
	noexcept
	:vector_view(v.data(), v.size())
	{}

	template<class U,
	         class A>
	vector_view(std::vector<U, A> &v)
	noexcept
	:vector_view(v.data(), v.size())
	{}

	template<size_t SIZE>
	vector_view(value_type (&__restrict__ buffer)[SIZE])
	noexcept
	:vector_view(buffer, SIZE)
	{}

	template<class U,
	         size_t SIZE>
	vector_view(const std::array<U, SIZE> &array)
	noexcept
	:vector_view(const_cast<pointer>(array.data()), array.size())
	{}

	template<size_t SIZE>
	vector_view(std::array<value_type, SIZE> &array)
	noexcept
	:vector_view(array.data(), array.size())
	{}

	// Required for reasonable implicit const conversion of value_type.
	vector_view(const vector_view<typename std::remove_const<value_type>::type> &v)
	noexcept
	:vector_view(v.data(), v.size())
	{}

	vector_view() noexcept = default;
	vector_view &operator=(const vector_view &) noexcept = default;
};

template<class T>
inline T *
ircd::data(const vector_view<T> &v)
noexcept
{
	return v.data();
}

template<class T>
inline size_t
ircd::size(const vector_view<T> &v)
noexcept
{
	return v.size();
}

template<class T>
inline bool
ircd::empty(const vector_view<T> &v)
noexcept
{
	return v.empty();
}
