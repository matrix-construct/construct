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
#define HAVE_IRCD_DB_OBJECT_H

namespace ircd {
namespace db   {

template<database *const &d>
struct transaction
{
	string_view index;
	database::snapshot snapshot;
	//something transaction;

	transaction(const string_view &index = {})
	:index{index}
	,snapshot{*d}
	{}
};

template<class T,
         database *const &d>
struct value
{
};

template<database *const &d>
struct value<void, d>
{
	mutable column h;
	transaction<d> *t;

	value(const string_view &name,
	      transaction<d> &t)
	:h{!name.empty()? column{*d, name} : column{}}
	,t{&t}
	{}

	value()
	:t{nullptr}
	{}
};

template<database *const &d,
         const char *const &prefix>
struct object
{
	struct iterator;

	using key_type = string_view;
	using mapped_type = value<void, d>;
	using value_type = std::pair<key_type, mapped_type>;
	using pointer = value_type *;
	using reference = value_type &;
	using size_type = size_t;
	using difference_type = ptrdiff_t;

	transaction<d> *t;

	iterator begin();
	iterator end();

	object(transaction<d> &t)
	:t{&t}
	{}

	object()
	:t{nullptr}
	{}
};


template<database *const &d,
         const char *const &prefix>
struct object<d, prefix>::iterator
{
	using key_type = string_view;
	using mapped_type = value<void, d>;
	using value_type = std::pair<key_type, mapped_type>;
	using pointer = value_type *;
	using reference = value_type &;
	using size_type = size_t;
	using difference_type = ptrdiff_t;

	friend class object<d, prefix>;

  protected:
	transaction<d> *t;
	decltype(database::columns)::iterator it;
	value_type last;
	value_type val;

	void seek_next();

  public:
	const value_type *operator->() const         { return &val;                                    }
	const value_type &operator*() const          { return *operator->();                           }

	bool operator==(const iterator &o) const     { return it == o.it;                              }
	bool operator!=(const iterator &o) const     { return it != o.it;                              }
	bool operator<(const iterator &o) const      { return it < o.it;                               }

	iterator &operator++()
	{
		++it;
		seek_next();
		return *this;
	}

	iterator(transaction<d> &t)
	:t{&t}
	{}

	iterator()
	:t{nullptr}
	{}
};

template<database *const &d,
         const char *const &prefix>
typename object<d, prefix>::iterator
object<d, prefix>::end()
{
	iterator ret{};
	ret.it = std::end(d->columns);
	return ret;
}

template<database *const &d,
         const char *const &prefix>
typename object<d, prefix>::iterator
object<d, prefix>::begin()
{
	iterator ret{*t};
	ret.it = std::begin(d->columns);
	ret.seek_next();
	return ret;
}

template<database *const &d,
          const char *const &prefix>
void
object<d, prefix>::iterator::seek_next()
{
	const auto ptc(tokens_count(prefix, "."));
	while(it != std::end(d->columns))
	{
		const auto &pair(*it);
		if(!startswith(pair.first, prefix))
		{
			++it;
			continue;
		}

		const auto ktc(tokens_count(pair.first, "."));
		if(ktc != ptc + 1)
		{
			const auto com(std::min(tokens_count(last.first, "."), ptc + 1));
			if(!com || token(last.first, ".", com - 1) == token(pair.first, ".", com - 1))
			{
				++it;
				continue;
			}
		}

		bool bad(false);
		const auto com(std::min(ktc, ptc));
		if(com)
			for(size_t i(0); i < com - 1 && !bad; i++)
				if(token(prefix, ".", i) != token(pair.first, ".", i))
					bad = true;
		if(bad)
		{
			++it;
			continue;
		}

		val.first = pair.first;
		last.first = pair.first;
		val.first = lstrip(val.first, prefix);
		val.first = lstrip(val.first, '.');
		val.first = split(val.first, '.').first;
		val.second = value<void, d>{pair.first, *t};
		break;
	}
}

/*
template<database *const &d>
struct value<string_view ,d>
:value<void, d>
{
	// hold iterator

	operator string_view() const
	{
		std::cout << "read [" << this->name << "] " << std::endl;
		return {};
	}

	value &operator=(const string_view &val)
	{
		std::cout << "write [" << this->name << "] " << val << std::endl;
		return *this;
	}

	value(const string_view &name)
	:value<void, d>{name}
	{}

	friend std::ostream &operator<<(std::ostream &s, const value<string_view, d> &v)
	{
		s << string_view{v};
		return s;
	}
};
*/

template<database *const &d>
struct value<int64_t, d>
:value<void, d>
{
	int64_t def;

	operator int64_t() const try
	{
		const auto val(read(this->h, this->t->index));
		return lex_cast<int64_t>(val);
	}
	catch(const not_found &e)
	{
		return def;
	}

	value &operator=(const int64_t &val)
	{
		write(this->h, this->t->index, lex_cast(val));
		return *this;
	}

	value(const string_view &name,
	      transaction<d> &t,
	      const int64_t &def = 0)
	:value<void, d>{name, t}
	,def{def}
	{}
};

} // namespace db
} // namespace ircd
