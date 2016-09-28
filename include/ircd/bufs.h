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
#define HAVE_IRCD_BUFS_H

#ifdef __cplusplus
#include <boost/asio/buffer.hpp>

namespace ircd {
inline namespace bufs {

using boost::asio::buffer_cast;
using boost::asio::const_buffer;
using boost::asio::const_buffers_1;
using boost::asio::mutable_buffer;
using boost::asio::mutable_buffers_1;

const mutable_buffer null_buffer
{
	nullptr, 0
};

const std::array<mutable_buffer, 1> null_buffers
{{
	null_buffer
}};

size_t size(const mutable_buffer &buf);
size_t size(const const_buffer &buf);
template<class mutable_buffers> size_t size(const mutable_buffers &iov);
template<class const_buffers> size_t size(const const_buffers &iov);

template<class T = uint8_t *> T data(const mutable_buffer &buf);
template<class T = uint8_t *> T begin(const mutable_buffer &buf);
template<class T = uint8_t *> T end(const mutable_buffer &buf);
template<class T = uint8_t *> std::reverse_iterator<T> rbegin(const mutable_buffer &buf);
template<class T = uint8_t *> std::reverse_iterator<T> rend(const mutable_buffer &buf);
template<class T = const uint8_t *> T data(const const_buffer &buf);
template<class T = const uint8_t *> T begin(const const_buffer &buf);
template<class T = const uint8_t *> T end(const const_buffer &buf);
template<class T = const uint8_t *> std::reverse_iterator<T> rbegin(const const_buffer &buf);
template<class T = const uint8_t *> std::reverse_iterator<T> rend(const const_buffer &buf);

size_t copy(const const_buffer &src, const mutable_buffer &dst);
template<class buffers> size_t copy(const buffers &iov, const mutable_buffer &buf);
template<class mutable_buffers> size_t copy(const mutable_buffer &buf, const mutable_buffers &iov);
template<class mutable_buffers> size_t copy(const const_buffer &buf, const mutable_buffers &iov);

void fill(const mutable_buffer &buf, const uint8_t &val = 0);
template<class mutable_buffers> void fill(const mutable_buffers &buf, const uint8_t &val = 0);

std::string string(const mutable_buffer &);
std::string string(const const_buffer &);

template<class buffer>
struct unique_buffer
:buffer
{
	template<class T> unique_buffer(std::unique_ptr<T[]> &&, const size_t &size);
	unique_buffer(const size_t &size);
	~unique_buffer() noexcept;
};

template<class buffer>
template<class T>
unique_buffer<buffer>::unique_buffer(std::unique_ptr<T[]> &&b,
                                     const size_t &size)
:buffer{b.release(), size}
{
}

template<class buffer>
unique_buffer<buffer>::unique_buffer(const size_t &size)
:unique_buffer
{
	std::unique_ptr<uint8_t[]>{new __attribute__((aligned(16))) uint8_t[size]},
	size
}
{
}

template<class buffer>
unique_buffer<buffer>::~unique_buffer()
noexcept
{
	delete[] data(*this);
}

template<class buffer>
std::string
string(const unique_buffer<buffer> &buf)
{
	return string(static_cast<const buffer &>(buf));
}

inline std::string
string(const const_buffer &buf)
{
	return { data<const char *>(buf), size(buf) };
}

inline std::string
string(const mutable_buffer &buf)
{
	return { data<const char *>(buf), size(buf) };
}

template<class mutable_buffers>
void
fill(const mutable_buffers &bufs,
     const uint8_t &val)
{
	for(auto &buf : bufs)
		fill(buf, val);
}

inline void
fill(const mutable_buffer &buf,
     const uint8_t &val)
{
	memset(data(buf), val, size(buf));
}

template<class mutable_buffers>
size_t
copy(const const_buffer &buf,
     const mutable_buffers &iov)
{
	size_t ret(0);
	for(auto &dst : iov)
	{
		const auto remain(buffer_size(buf) - ret);
		const auto cp_sz(std::min(buffer_size(dst), remain));
		const auto src(data(buf) + ret);
		memcpy(data(dst), src, cp_sz);
		ret += cp_sz;
	}

	return ret;
}

template<class mutable_buffers>
size_t
copy(const mutable_buffer &buf,
     const mutable_buffers &iov)
{
	size_t ret(0);
	for(auto &dst : iov)
	{
		const auto remain(buffer_size(buf) - ret);
		const auto cp_sz(std::min(buffer_size(dst), remain));
		const auto src(data(buf) + ret);
		memcpy(data(dst), src, cp_sz);
		ret += cp_sz;
	}

	return ret;
}

template<class buffers>
size_t
copy(const buffers &iov,
     const mutable_buffer &buf)
{
	size_t ret(0);
	for(const auto &src : iov)
	{
		const auto remain(buffer_size(buf) - ret);
		const auto cp_sz(std::min(buffer_size(src), remain));
		const auto dst(data(buf) + ret);
		memcpy(dst, data(src), cp_sz);
		ret += cp_sz;
	}

	return ret;
}

inline size_t
copy(const const_buffer &src,
     const mutable_buffer &dst)
{
	const auto cp_sz(std::min(buffer_size(src), buffer_size(dst)));
	memcpy(data(dst), data(src), cp_sz);
	return cp_sz;
}

template<class T>
std::reverse_iterator<T>
rend(const const_buffer &buf)
{
	return begin<T>(buf);
}

template<class T>
std::reverse_iterator<T>
rbegin(const const_buffer &buf)
{
	return std::reverse_iterator<T>(begin<T>(buf) + size(buf));
}

template<class T>
T
end(const const_buffer &buf)
{
	return begin<T>(buf) + size(buf);
}

template<class T>
T
begin(const const_buffer &buf)
{
	return data<T>(buf);
}

template<class T>
T
data(const const_buffer &buf)
{
	return buffer_cast<T>(buf);
}

template<class T>
std::reverse_iterator<T>
rend(const mutable_buffer &buf)
{
	return begin<T>(buf);
}

template<class T>
std::reverse_iterator<T>
rbegin(const mutable_buffer &buf)
{
	return std::reverse_iterator<T>(begin<T>(buf) + size(buf));
}

template<class T>
T
end(const mutable_buffer &buf)
{
	return begin<T>(buf) + size(buf);
}

template<class T>
T
begin(const mutable_buffer &buf)
{
	return data<T>(buf);
}

template<class T>
T
data(const mutable_buffer &buf)
{
	return buffer_cast<T>(buf);
}

template<class buffer>
size_t
size(const unique_buffer<buffer> &buf)
{
	return size(static_cast<const buffer &>(buf));
}

template<class buffers>
size_t
size(const buffers &iov)
{
	return std::accumulate(begin(iov), end(iov), size_t(0), []
	(auto ret, const auto &buffer)
	{
		return ret += size(buffer);
	});
}

inline size_t
size(const mutable_buffer &buf)
{
	return buffer_size(buf);
}

inline size_t
size(const const_buffer &buf)
{
	return buffer_size(buf);
}

}       // namespace bufs
}       // namespace ircd
#endif  // __cplusplus
