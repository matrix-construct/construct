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

size_t copy(const const_buffer &src, const mutable_buffer &dst);
template<class buffers> size_t copy(const buffers &iov, const mutable_buffer &buf);
template<class mutable_buffers> size_t copy(const mutable_buffer &buf, const mutable_buffers &iov);
template<class mutable_buffers> size_t copy(const const_buffer &buf, const mutable_buffers &iov);

void fill(const mutable_buffer &buf, const uint8_t &val = 0);
template<class mutable_buffers> void fill(const mutable_buffers &buf, const uint8_t &val = 0);


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
	memset(buffer_cast<uint8_t *>(buf), val, size(buf));
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
		const auto src(buffer_cast<const uint8_t *>(buf) + ret);
		memcpy(buffer_cast<uint8_t *>(dst), src, cp_sz);
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
		const auto src(buffer_cast<const uint8_t *>(buf) + ret);
		memcpy(buffer_cast<uint8_t *>(dst), src, cp_sz);
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
		const auto dst(buffer_cast<uint8_t *>(buf) + ret);
		memcpy(dst, buffer_cast<const uint8_t *>(src), cp_sz);
		ret += cp_sz;
	}

	return ret;
}

inline size_t
copy(const const_buffer &src,
     const mutable_buffer &dst)
{
	const auto cp_sz(std::min(buffer_size(src), buffer_size(dst)));
	memcpy(buffer_cast<uint8_t *>(dst), buffer_cast<const uint8_t *>(src), cp_sz);
	return cp_sz;
}

template<class buffers>
size_t
size(const buffers &iov)
{
	return std::accumulate(begin(iov), end(iov), size_t(0), []
	(auto ret, const auto &buffer)
	{
		return ret += buffer_size(buffer);
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
