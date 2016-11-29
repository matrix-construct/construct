/*
 * charybdis5
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
 */

#pragma once
#define HAVE_IRCD_BUFFER_H

namespace ircd   {
namespace buffer {

template<class it>
struct buffer
:std::tuple<it, it>
{
	operator string_view() const;
	operator std::string_view() const;
	explicit operator std::string() const;

	buffer(const it &start = nullptr, const it &stop = nullptr)
	:std::tuple<it, it>{start, stop}
	{}

	buffer(const it &start, const size_t &size)
	:buffer{start, start + size}
	{}
};

template<class it,
         size_t align = 16>
struct unique_buffer
:buffer<it>
{
	template<class T> unique_buffer(std::unique_ptr<T[]> &&, const size_t &size);
	unique_buffer(const size_t &size);
	unique_buffer() = default;
	~unique_buffer() noexcept;
};

struct const_buffer
:buffer<const char *>
{
	operator boost::asio::const_buffer() const;

	using buffer<const char *>::buffer;
};

struct mutable_buffer
:buffer<char *>
{
	operator boost::asio::mutable_buffer() const;

	using buffer<char *>::buffer;
};

template<class T> using buffers = std::initializer_list<T>;
using const_buffers = buffers<const_buffer>;
using mutable_buffers = buffers<mutable_buffer>;

const mutable_buffer null_buffer
{
    nullptr, nullptr
};

const mutable_buffers null_buffers
{{
	null_buffer
}};

template<class it> it rend(const buffer<it> &buffer);
template<class it> it end(const buffer<it> &buffer);
template<class it> it rbegin(const buffer<it> &buffer);
template<class it> it begin(const buffer<it> &buffer);

template<class it> size_t size(const buffer<it> &buffer);
template<class T> size_t size(const buffers<T> &buffers);
template<class it> it data(const buffer<it> &buffer);

} // namespace buffer

using buffer::const_buffer;
using buffer::mutable_buffer;
using buffer::unique_buffer;
using buffer::const_buffers;
using buffer::mutable_buffers;
using buffer::null_buffer;
using buffer::null_buffers;

} // namespace ircd


template<class it>
it
ircd::buffer::data(const buffer<it> &buffer)
{
	return get<0>(buffer);
}

template<class T>
size_t
ircd::buffer::size(const buffers<T> &buffers)
{
	return std::accumulate(std::begin(buffers), std::end(buffers), size_t(0), []
	(auto ret, const auto &buffer)
	{
		return ret += size(buffer);
	});
}

template<class it>
size_t
ircd::buffer::size(const buffer<it> &buffer)
{
	assert(get<0>(buffer) <= get<1>(buffer));
	return std::distance(get<0>(buffer), get<1>(buffer));
}

template<class it>
it
ircd::buffer::begin(const buffer<it> &buffer)
{
	return get<0>(buffer);
}

template<class it>
it
ircd::buffer::rbegin(const buffer<it> &buffer)
{
	return std::reverse_iterator<it>(get<0>(buffer) + size(buffer));
}

template<class it>
it
ircd::buffer::end(const buffer<it> &buffer)
{
	return get<1>(buffer);
}

template<class it>
it
ircd::buffer::rend(const buffer<it> &buffer)
{
	return std::reverse_iterator<it>(get<0>(buffer));
}

template<class it,
         size_t alignment>
template<class T>
ircd::buffer::unique_buffer<it, alignment>::unique_buffer(std::unique_ptr<T[]> &&b,
                                                          const size_t &size)
:buffer<it>{it(b.release()), size}
{
}

template<class it,
         size_t alignment>
ircd::buffer::unique_buffer<it, alignment>::unique_buffer(const size_t &size)
:unique_buffer
{
	std::unique_ptr<uint8_t[]>
	{
		new __attribute__((aligned(alignment))) uint8_t[size]
	},
	size
}
{
}

template<class it,
         size_t alignment>
ircd::buffer::unique_buffer<it, alignment>::~unique_buffer()
noexcept
{
	delete[] data(*this);
}

template<class it>
ircd::buffer::buffer<it>::operator std::string()
const
{
	return { get<0>(*this), size(*this) };
}

template<class it>
ircd::buffer::buffer<it>::operator std::string_view()
const
{
	return { get<0>(*this), size(*this) };
}

template<class it>
ircd::buffer::buffer<it>::operator string_view()
const
{
	return { get<0>(*this), get<1>(*this) };
}
