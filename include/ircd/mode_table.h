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
#define HAVE_IRCD_MODE_TABLE_H

#ifdef __cplusplus
namespace ircd  {

IRCD_EXCEPTION(error, mode_filled)

/* The mode_table is a generic template to replace the arrays mapping characters to mode flags
 * (or other structures) like `extern uint umode_table[256]` or `struct chm chmode_table[256]`.
 * Instead an `mode_table<uint>` or `mode_table<chm>` etc can be used.
 *
 * mask() overloads converting from a character string to a bitmask accept '+' and '-' in the
 * string to alter the mask. mask() overloads converting from a bitmask to a string DO NOT place
 * a leading '+' in the string. To do that you need a delta()...
 *
 * delta(table, before, after, buf) - Create a character string with '+' and '-' describing
 * the difference between the before and after bitmasks. It is common to use 0 as the before
 * argument to just get a complete mask() with the leading '+'.
 */
template<class T>
struct mode_table
:std::array<T, 128>
{
	using value_t = T;
	using mask_t = uint64_t;

	const T &operator[](const uint8_t &c) const;
	T &operator[](const uint8_t &c);
};

template<class T,
         class mask_t>
mask_t &
mask(const mode_table<T> &table,
     const char *buf,
     mask_t &val)
{
	mask_t add(-1);
	if(buf) for(; *buf; ++buf) switch(*buf)
	{
		case '+':  add = -1;  continue;
		case '-':  add = 0;   continue;
		default:
			val ^= (table[*buf] & add) ^ (table[*buf] & val);
			continue;
	}

	return val;
}

template<class T,
         class mask_t = typename mode_table<T>::mask_t>
auto
mask(const mode_table<T> &table,
     const char *const &buf,
     const mask_t &val = 0)
{
	mask_t ret(val);
	return mask(table, buf, ret);
}

template<class T,
         class mask_t>
char *
mask(const mode_table<T> &table,
     const mask_t &val,
     char *const &buf)
{
	char *p(buf);
	for(uint8_t i(0); i < table.size(); ++i)
		if(table[i] & val)
			*p++ = char(i);

	*p = '\0';
	return buf;
}

template<class T,
         class mask_t>
void
mask(const mode_table<T> &table,
     const mask_t &val,
     const std::function<void (const char &)> &closure)
{
	for(uint8_t i(0); i < table.size(); ++i)
		if(table[i] & val)
			closure(i);
}

template<class T,
         class mask_t>
std::ostream &
mask(const mode_table<T> &table,
     const mask_t &val,
     std::ostream &s)
{
	mask(table, val, [&s](const char &c)
	{
		s << c;
	});

	return s;
}

template<class T,
         class mask_t>
char *
delta(const mode_table<T> &table,
      const mask_t &before,
      const mask_t &after,
      char *const &buf)
{
	char *p(buf);
	auto current('\0');
	mask(table, before ^ after, [&](const char &c)
	{
		const auto what(table[c] & after? '+' : '-');
		if(current != what)
		{
			current = what;
			*p++ = what;
		}

		*p++ = c;
	});

	if(!current)
		*p++ = '+';     // no change; still require placeholding character

	*p = '\0';
	return buf;
}

template<class T,
         class mask_t>
char *
delta(const mode_table<T> &table,
      const mask_t &before,
      const char *const &after,
      char *const &buf)
{
	return delta(table, before, mask(table, after), buf);
}

template<class T,
         class mask_t>
char *
delta(const mode_table<T> &table,
      const char *const &before,
      const mask_t &after,
      char *const &buf)
{
	return delta(table, mask(table, before), after, buf);
}

template<class T,
         class mask_t>
auto
delta(const mode_table<T> &table,
      const mask_t &before,
      const char *const &after)
{
	return mask(table, after, before);
}

template<class T>
char
find(const mode_table<T> &table,
     const std::function<bool (const T &)> &func)
{
	const auto it(std::find_if(begin(table), end(table), func));
	return int8_t(std::distance(begin(table), it));
}

template<class T>
auto
find_slot(const mode_table<T> &table,
          const std::nothrow_t)
{
	using mask_t = typename mode_table<T>::mask_t;

	mask_t mask(0);
	std::for_each(begin(table), end(table), [&mask]
	(const T &elem)
	{
		mask |= elem;
	});

	for(mask_t i(1); i; i <<= 1)
		if(~mask & i)
			return i;

	return mask_t(0);
}

template<class T>
auto
find_slot(const mode_table<T> &table)
{
	const auto ret(find_slot(table, std::nothrow));
	return ret?: throw mode_filled("No bits left on mode mask");
}

template<class T>
T &
mode_table<T>::operator[](const uint8_t &c)
{
	return this->std::array<T, 128>::operator[](c & 0x7f);
}

template<class T>
const T &
mode_table<T>::operator[](const uint8_t &c)
const
{
	return this->std::array<T, 128>::operator[](c & 0x7f);
}

}      // namespace ircd
#endif // __cplusplus
