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
#define HAVE_IRCD_UTIL_INSTANCE_LIST_H

namespace ircd::util
{
	template<class T> struct instance_list;
}

/// The instance_list pattern is where every instance of a class registers
/// itself in a static list of all instances and removes itself on dtor.
/// IRCd Ex. All clients use instance_list so all clients can be listed for
/// an administrator or be interrupted and disconnected on server shutdown.
///
/// `struct myobj : ircd::instance_list<myobj> {};`
///
/// * The creator of the class no longer has to manually specify what is
/// defined here using unique_iterator; however, one still must provide
/// linkage for the static list.
///
/// * The container pointer used by unique_iterator is eliminated here
/// because of the static list.
///
template<class T>
struct ircd::util::instance_list
{
	static std::list<T *> list;

  protected:
	typename decltype(list)::iterator it;

	instance_list(typename decltype(list)::iterator it)
	:it{std::move(it)}
	{}

	instance_list()
	:it{list.emplace(end(list), static_cast<T *>(this))}
	{}

	instance_list(const instance_list &) = delete;
	instance_list(instance_list &&o) noexcept
	:it{std::move(o.it)}
	{
		o.it = end(list);
	}

	instance_list &operator=(const instance_list &) = delete;
	instance_list &operator=(instance_list &&o) noexcept
	{
		std::swap(it, o.it);
		return *this;
	}

	~instance_list() noexcept
	{
		if(it != end(list))
			list.erase(it);
	}
};
