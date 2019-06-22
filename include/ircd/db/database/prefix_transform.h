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
#define HAVE_IRCD_DB_DATABASE_PREFIX_TRANSFORM_H

// This file is not part of the standard include stack because it requires
// RocksDB symbols which we cannot forward declare. It is used internally
// and does not need to be included by general users of IRCd.

struct ircd::db::database::prefix_transform final
:rocksdb::SliceTransform
{
	using Slice = rocksdb::Slice;

	database *d;
	db::prefix_transform user;

	const char *Name() const noexcept override;
	bool InDomain(const Slice &key) const noexcept override;
	bool InRange(const Slice &key) const noexcept override;
	Slice Transform(const Slice &key) const noexcept override;

	prefix_transform(database *const &d,
	                 db::prefix_transform user)
	noexcept
	:d{d}
	,user{std::move(user)}
	{}
};
