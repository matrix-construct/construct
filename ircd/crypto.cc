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

///////////////////////////////////////////////////////////////////////////////
//
// hash.h
//

ircd::crh::hash::~hash()
noexcept
{
}

ircd::crh::hash &
ircd::crh::hash::operator+=(const const_raw_buffer &buf)
{
	update(buf);
	return *this;
}

void
ircd::crh::hash::operator()(const mutable_raw_buffer &out,
                            const const_raw_buffer &in)
{
	update(in);
	finalize(out);
}

///////////////////////////////////////////////////////////////////////////////
//
// rand.h
//

decltype(ircd::rand::device) ircd::rand::device
{
	// on linux: uses RDRND or /dev/urandom
	// on windows: TODO: verify construction source
};

decltype(ircd::rand::mt) ircd::rand::mt
{
	device()
};

decltype(ircd::rand::dict::alnum) ircd::rand::dict::alnum
{
	"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
};

decltype(ircd::rand::dict::alpha) ircd::rand::dict::alpha
{
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
};

decltype(ircd::rand::dict::upper) ircd::rand::dict::upper
{
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
};

decltype(ircd::rand::dict::lower) ircd::rand::dict::lower
{
	"abcdefghijklmnopqrstuvwxyz"
};

decltype(ircd::rand::dict::numeric) ircd::rand::dict::numeric
{
	"0123456789"
};

std::string
ircd::rand::string(const std::string &dict,
                   const size_t &len)
{
	std::string ret(len, char());
	string(dict, len, reinterpret_cast<uint8_t *>(&ret.front()));
	return ret;
}

ircd::string_view
ircd::rand::string(const std::string &dict,
                   const size_t &len,
                   char *const &buf,
                   const size_t &max)
{
	if(unlikely(!max))
		return { buf, max };

	const auto size
	{
		std::min(len, max - 1)
	};

	buf[size] = '\0';
	return string(dict, size, reinterpret_cast<uint8_t *>(buf));
}

ircd::string_view
ircd::rand::string(const std::string &dict,
                   const size_t &len,
                   uint8_t *const &buf)
{
	std::uniform_int_distribution<size_t> dist
	{
		0, dict.size() - 1
	};

	std::generate(buf, buf + len, [&dict, &dist]
	() -> uint8_t
	{
		return dict.at(dist(mt));
	});

	return string_view
	{
		reinterpret_cast<const char *>(buf), len
	};
}
