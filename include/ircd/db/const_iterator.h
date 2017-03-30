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
#define HAVE_IRCD_DB_CONST_ITERATOR_H

namespace ircd {
namespace db   {

struct column::const_iterator
{
	struct state;

	using key_type = string_view;
	using mapped_type = string_view;
	using value_type = std::pair<key_type, mapped_type>;
	using pointer = value_type *;
	using reference = value_type &;
	using difference_type = size_t;
	using iterator_category = std::bidirectional_iterator_tag;

  private:
	gopts opts;
	database::column *c;
	std::unique_ptr<rocksdb::Iterator> it;
	mutable value_type val;

	friend class column;
	const_iterator(database::column &, std::unique_ptr<rocksdb::Iterator> &&, gopts = {});

  public:
	operator const database::column &() const    { return *c;                                   }
	operator const database::snapshot &() const  { return opts.snapshot;                        }
	explicit operator const gopts &() const      { return opts;                                 }

	operator database::column &()                { return *c;                                   }
	explicit operator database::snapshot &()     { return opts.snapshot;                        }

	operator bool() const;
	bool operator!() const;
	bool operator<(const const_iterator &) const;
	bool operator>(const const_iterator &) const;
	bool operator==(const const_iterator &) const;
	bool operator!=(const const_iterator &) const;
	bool operator<=(const const_iterator &) const;
	bool operator>=(const const_iterator &) const;

	const value_type *operator->() const;
	const value_type &operator*() const;

	const_iterator &operator++();
	const_iterator &operator--();

	const_iterator();
	const_iterator(const_iterator &&) noexcept;
	const_iterator &operator=(const_iterator &&) noexcept;
	~const_iterator() noexcept;

	template<class pos> friend void seek(column::const_iterator &, const pos &);
	friend void seek(column::const_iterator &, const string_view &key);
};

} // namespace db
} // namespace ircd
