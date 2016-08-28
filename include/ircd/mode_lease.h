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
#define HAVE_IRCD_MODE_LEASE_H

#ifdef __cplusplus
namespace ircd  {

template<class T,
         mode_table<T> &table>
class mode_lease
{
	using mask_t = typename mode_table<T>::mask_t;

	char c = '\0';

	virtual void release() noexcept              { table[c] = { 0 };                               }

  public:
	explicit operator const char &() const       { return c;                                       }
	operator const mask_t &() const              { return static_cast<const mask_t &>(table[c]);   }
	bool operator!() const                       { return !c;                                      }

	template<class... args> mode_lease(const char &c, args&&...);
	mode_lease() = default;
	mode_lease(mode_lease &&) noexcept;
	mode_lease(const mode_lease &) = delete;
	virtual ~mode_lease() noexcept;
};

template<class T,
         mode_table<T> &table>
template<class... args>
mode_lease<T, table>::mode_lease(const char &c,
                                 args&&... a)
:c(c)
{
	if(!c)
		return;

	if(!!table[c])
		throw mode_filled("Character [%c] is already leased", c);

	table[c] = { find_slot(table), std::forward<args>(a)... };
}

template<class T,
         mode_table<T> &table>
mode_lease<T, table>::mode_lease(mode_lease &&other)
noexcept
:c(other.c)
{
	other.c = '\0';
}

template<class T,
         mode_table<T> &table>
mode_lease<T, table>::~mode_lease()
noexcept
{
	if(!c)
		return;

	release();
}

}      // namespace ircd
#endif // __cplusplus
