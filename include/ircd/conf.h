/*
 * Copyright (C) 2016 Charybdis Development Team
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
#define HAVE_IRCD_CONF_H

namespace ircd {

extern struct conf conf;

} // namespace ircd

struct ircd::conf
{
	struct init;
	struct item_base;
	template<class T> struct item;

	std::map<std::string, item_base> items;
};

struct ircd::conf::item_base
{
	std::string buf;
};

template<class T>
struct ircd::conf::item
:item_base
{
	operator const T &() const
	{
		assert(buf.size() == sizeof(T));
		return reinterpret_cast<T *>(buf.data());
	};

	operator T &()
	{
		assert(buf.size() == sizeof(T));
		return reinterpret_cast<T *>(buf.data());
	};

	item(T t)
	:item_base{[&t]
	{
		std::string ret{sizeof(T), char{}};
		*reinterpret_cast<T *>(ret.data()) = std::move(t);
	}()}
	{}
};

namespace ircd {

template<> struct conf::item<std::string>;

}

template<>
struct ircd::conf::item<std::string>
:item_base
{
	operator const std::string &() const
	{
		return buf;
	};

	operator std::string &()
	{
		return buf;
	};

	item(std::string string)
	:item_base{std::move(string)}
	{}
};
