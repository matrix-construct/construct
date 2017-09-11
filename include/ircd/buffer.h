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

namespace ircd::buffer
{
	template<class it> struct buffer;
	struct const_buffer;
	struct mutable_buffer;
	template<class buffer, size_t align> struct unique_buffer;

	template<class T> using buffers = std::initializer_list<T>;
	using const_buffers = buffers<const_buffer>;
	using mutable_buffers = buffers<mutable_buffer>;

	extern const mutable_buffer null_buffer;
	extern const mutable_buffers null_buffers;

	template<class it> const it &begin(const buffer<it> &buffer);
	template<class it> const it &end(const buffer<it> &buffer);
	template<class it> it &begin(buffer<it> &buffer);
	template<class it> it &end(buffer<it> &buffer);

	template<class it> it rbegin(const buffer<it> &buffer);
	template<class it> it rend(const buffer<it> &buffer);

	template<class it> size_t size(const buffer<it> &buffer);
	template<class T> size_t size(const buffers<T> &buffers);
	template<class it> const it &data(const buffer<it> &buffer);

	template<class it> size_t consume(buffer<it> &buffer, const size_t &bytes);
	template<class T> size_t consume(buffers<T> &buffers, const size_t &bytes);

	template<class it> char *copy(char *&dest, char *const &stop, const buffer<it> &buffer);
	template<class it> size_t copy(char *const &dest, const size_t &max, const buffer<it> &buffer);

	template<class T> char *copy(char *&dest, char *const &stop, const buffers<T> &buffer);
	template<class T> size_t copy(char *const &dest, const size_t &max, const buffers<T> &buffer);

	template<class it> std::ostream &operator<<(std::ostream &s, const buffer<it> &buffer);
	template<class T> std::ostream &operator<<(std::ostream &s, const buffers<T> &buffers);
}

// Export these important aliases down to main ircd namespace
namespace ircd
{
	using buffer::const_buffer;
	using buffer::mutable_buffer;
	using buffer::unique_buffer;
	using buffer::const_buffers;
	using buffer::mutable_buffers;
	using buffer::null_buffer;
	using buffer::null_buffers;
}

template<class it>
struct ircd::buffer::buffer
:std::tuple<it, it>
{
	using value_type = it;

	operator string_view() const;
	operator std::string_view() const;
	explicit operator std::string() const;

	buffer(const it &start, const it &stop)
	:std::tuple<it, it>{start, stop}
	{}

	buffer(const it &start, const size_t &size)
	:buffer{start, start + size}
	{}

	template<size_t SIZE>
	buffer(typename std::remove_pointer<it>::type (&buf)[SIZE])
	:buffer{buf, SIZE}
	{}

	buffer()
	:buffer{nullptr, nullptr}
	{}
};

struct ircd::buffer::const_buffer
:buffer<const char *>
{
	operator boost::asio::const_buffer() const;

	using buffer<const char *>::buffer;
	const_buffer(const string_view &s)
	:buffer<const char *>{begin(s), end(s)}
	{}

	const_buffer() = default;
};

struct ircd::buffer::mutable_buffer
:buffer<char *>
{
	operator boost::asio::mutable_buffer() const;

	using buffer<char *>::buffer;
};

template<class T>
std::ostream &
ircd::buffer::operator<<(std::ostream &s, const buffers<T> &buffers)
{
	std::for_each(begin(buffers), end(buffers), [&s]
	(const auto &buffer)
	{
		s << buffer;
	});

	return s;
}

template<class it>
std::ostream &
ircd::buffer::operator<<(std::ostream &s, const buffer<it> &buffer)
{
	s.write(data(buffer), size(buffer));
	return s;
}

template<class T>
size_t
ircd::buffer::copy(char *const &dest,
                   const size_t &max,
                   const buffers<T> &buffers)
{
	size_t ret(0);
	for(const auto &buffer : buffers)
		ret += copy(dest + ret, max - ret, buffer);

	return ret;
}

template<class it>
size_t
ircd::buffer::copy(char *const &dest,
                   const size_t &max,
                   const buffer<it> &buffer)
{
	if(!max)
		return 0;

	char *out(dest);
	char *const stop(dest + max - 1);
	copy(out, stop, buffer);
	*out = '\0';
	return std::distance(dest, out);
}

template<class T>
char *
ircd::buffer::copy(char *&dest,
                   char *const &stop,
                   const buffers<T> &buffers)
{
	char *const ret(dest);
	for(const auto &buffer : buffers)
		copy(dest, stop, buffer);

	return ret;
}

template<class it>
char *
ircd::buffer::copy(char *&dest,
                   char *const &stop,
                   const buffer<it> &buffer)
{
	char *const ret(dest);
	const size_t remain(stop - dest);
	dest += std::min(size(buffer), remain);
	memcpy(ret, data(buffer), dest - ret);
	return ret;
}

template<class T>
size_t
ircd::buffer::consume(buffers<T> &buffers,
                      const size_t &bytes)
{
	size_t remain(bytes);
	for(auto it(begin(buffers)); it != end(buffers) && remain > 0; ++it)
	{
		using buffer = typename buffers<T>::value_type;
		using iterator = typename buffer::value_type;

		buffer &b(const_cast<buffer &>(*it));
		remain -= consume(b, remain);
	}

	return bytes - remain;
}

template<class it>
size_t
ircd::buffer::consume(buffer<it> &buffer,
                      const size_t &bytes)
{
	get<0>(buffer) += std::min(size(buffer), bytes);
	return bytes - size(buffer);
}

template<class it>
const it &
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
ircd::buffer::rend(const buffer<it> &buffer)
{
	return std::reverse_iterator<it>(get<0>(buffer));
}

template<class it>
it
ircd::buffer::rbegin(const buffer<it> &buffer)
{
	return std::reverse_iterator<it>(get<0>(buffer) + size(buffer));
}

template<class it>
it &
ircd::buffer::end(buffer<it> &buffer)
{
	return get<1>(buffer);
}

template<class it>
it &
ircd::buffer::begin(buffer<it> &buffer)
{
	return get<0>(buffer);
}

template<class it>
const it &
ircd::buffer::end(const buffer<it> &buffer)
{
	return get<1>(buffer);
}

template<class it>
const it &
ircd::buffer::begin(const buffer<it> &buffer)
{
	return get<0>(buffer);
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

template<class buffer,
         size_t align = 16>
struct ircd::buffer::unique_buffer
:buffer
{
	unique_buffer(std::unique_ptr<uint8_t[]> &&, const size_t &size);
	unique_buffer(const size_t &size);
	unique_buffer() = default;
	unique_buffer(unique_buffer &&) noexcept;
	unique_buffer(const unique_buffer &) = delete;
	~unique_buffer() noexcept;
};

template<class buffer,
         size_t alignment>
ircd::buffer::unique_buffer<buffer, alignment>::unique_buffer(std::unique_ptr<uint8_t[]> &&b,
                                                              const size_t &size)
:buffer
{
	typename buffer::value_type(b.release()), size
}
{
}

template<class buffer,
         size_t alignment>
ircd::buffer::unique_buffer<buffer, alignment>::unique_buffer(const size_t &size)
:unique_buffer<buffer, alignment>
{
	std::unique_ptr<uint8_t[]>
	{
		new __attribute__((aligned(alignment))) uint8_t[size]
	},
	size
}
{
}

template<class buffer,
         size_t alignment>
ircd::buffer::unique_buffer<buffer, alignment>::unique_buffer(unique_buffer &&other)
noexcept
:buffer
{
	std::move(other)
}
{
	get<0>(other) = nullptr;
}

template<class buffer,
         size_t alignment>
ircd::buffer::unique_buffer<buffer, alignment>::~unique_buffer()
noexcept
{
	delete[] data(*this);
}
