// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_DB_ERROR_H

namespace ircd::db
{
	struct error;
}

/// Database error. For most catchers of this error outside of the db:: system,
/// the formatted what() message will be sufficient. The codes are not useful
/// outside of db::. A common error `not_found` has its own subtype to be
/// caught independently;
///
struct ircd::db::error
:ircd::error
{
	struct not_found;

  protected:
	static const rocksdb::Status _no_code_;

  public:
	uint8_t code {0};
	uint8_t subcode {0};
	uint8_t severity {0};

	error(generate_skip_t,
	      const rocksdb::Status &);

	explicit
	error(const rocksdb::Status &);

	IRCD_OVERLOAD(internal)
	error(internal_t,
	      const rocksdb::Status &s,
	      const string_view &fmt,
	      const va_rtti &ap);

	template<class... args>
	error(const rocksdb::Status &s,
	      const string_view &fmt,
	      args&&... a)
	:error
	{
		internal, s, fmt, va_rtti{std::forward<args>(a)...}
	}{}

	template<class... args>
	error(const string_view &fmt,
	      args&&... a)
	:error
	{
		internal, _no_code_, fmt, va_rtti{std::forward<args>(a)...}
	}{}
};

namespace ircd::db
{
	using not_found = error::not_found;
}

/// Common error `not_found` has its own subtype to be caught independently;
/// it may contain a more limited what() (or none at all) as an optimization.
///
struct ircd::db::error::not_found
:error
{
  protected:
	static const rocksdb::Status _not_found_;

  public:
	template<class... args>
	not_found(const string_view &fmt,
	          args&&... a)
	:error
	{
		_not_found_, fmt, std::forward<args>(a)...
	}{}

	not_found();
};
