/*
 *  charybdis: A slightly useful ircd.
 *  supported.h: isupport (005) numeric
 *
 *  Entirely rewritten, August 2006 by Jilles Tjoelker
 *  Copyright (C) 2006 Jilles Tjoelker
 *  Copyright (C) 2016 Charybdis Development Team
 *  Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  1.Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  2.Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  3.The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 *  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 *  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#define HAVE_IRCD_SUPPORTED_H

#ifdef __cplusplus
namespace ircd      {
namespace supported {

using client::client;

// Additional types can be supported here eventually
enum class type
{
	BOOLEAN,                  // boolean value (always true if key exists in map)
	INTEGER,                  // integer is copied here as the value
	STRING,                   // string is copied here as the value
	FUNC_BOOLEAN,             // function returns a boolean value
	FUNC_STREAM,              // function argument is an output stream
};

struct value
{
	enum type type; union
	{
		int64_t integer;
		std::string string;
		std::function<bool ()> func_boolean;
		std::function<void (ostream &)> func_stream;
	};

	ostream &operator()(const std::string &key, ostream &buf) const;

	value();
	value(const int64_t &);
	value(const std::string &);
	value(const std::function<bool ()> &);
	value(const std::function<void (ostream &)> &);
	~value() noexcept;
};

extern std::map<std::string, value> map;

template<class value_t> void add(const std::string &key, value_t&& v);
void add(const std::string &key);
bool del(const std::string &key);
void show(client &);
void init();

} // namespace supported
} // namespace ircd

inline
void ircd::supported::add(const std::string &key)
{
	//TODO: XXX
	//value can't be reassigned unless there's an explicit move constructor
	//elaborating the entire unrestricted union etc etc for another time.

	map.erase(key);
	map.emplace(std::piecewise_construct,
	            std::make_tuple(key),
	            std::make_tuple());
}

template<class value_t>
void ircd::supported::add(const std::string &key,
                          value_t&& v)
{
	map.erase(key);
	map.emplace(std::piecewise_construct,
	            std::make_tuple(key),
	            std::make_tuple(std::forward<value_t>(v)));
}

#endif // __cplusplus
