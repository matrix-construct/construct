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
#define HAVE_IRCD_UTIL_BSWAP_H

namespace ircd::util
{
	template<class T> T &bswap(T *const &val);
	template<class T> T bswap(const T &val);
}

/// Reverse endian of data pointed to; return reference
template<class T>
T &
ircd::util::bswap(T *const &val)
{
	assert(val != nullptr);
	const auto ptr{reinterpret_cast<uint8_t *>(val)};
	std::reverse(ptr, ptr + sizeof(T));
	return *val;
}

/// Reverse endian of T returning value copy
template<class T>
T
ircd::util::bswap(const T &val)
{
	T ret;
	const auto valptr{reinterpret_cast<const uint8_t *>(&val)};
	const auto retptr{reinterpret_cast<uint8_t *>(&ret)};
	std::reverse_copy(valptr, valptr + sizeof(T), retptr);
	return ret;
}
