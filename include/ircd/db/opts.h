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
 *
 */

#pragma once
#define HAVE_IRCD_DB_OPTS_H

namespace ircd {
namespace db   {

template<class T>
struct optval
:std::pair<T, ssize_t>
{
	optval(const T &key, const ssize_t &val = std::numeric_limits<ssize_t>::min());
};

template<class T> using optlist = std::initializer_list<optval<T>>;
template<class T> bool has_opt(const optlist<T> &, const T &);
template<class T> ssize_t opt_val(const optlist<T> &, const T &);

} // namespace db
} // namespace ircd

template<class T>
ssize_t
ircd::db::opt_val(const optlist<T> &list,
                  const T &opt)
{
	for(const auto &p : list)
		if(p.first == opt)
			return p.second;

	return std::numeric_limits<ssize_t>::min();
}

template<class T>
bool
ircd::db::has_opt(const optlist<T> &list,
                  const T &opt)
{
	for(const auto &p : list)
		if(p.first == opt)
			return true;

	return false;
}

template<class T>
ircd::db::optval<T>::optval(const T &key,
                            const ssize_t &val)
:std::pair<T, ssize_t>{key, val}
{
}
