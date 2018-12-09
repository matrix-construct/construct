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
#define HAVE_IRCD_DB_DATABASE_COMPARATOR_H

// This file is not part of the standard include stack because it requires
// RocksDB symbols which we cannot forward declare. It is used internally
// and does not need to be included by general users of IRCd.

struct ircd::db::database::comparator final
:rocksdb::Comparator
{
	using Slice = rocksdb::Slice;

	database *d;
	db::comparator user;

	bool CanKeysWithDifferentByteContentsBeEqual() const noexcept override;
	bool IsSameLengthImmediateSuccessor(const Slice &s, const Slice &t) const noexcept override;
	void FindShortestSeparator(std::string *start, const Slice &limit) const noexcept override;
	void FindShortSuccessor(std::string *key) const noexcept override;
	int Compare(const Slice &a, const Slice &b) const noexcept override;
	bool Equal(const Slice &a, const Slice &b) const noexcept override;
	const char *Name() const noexcept override;

	comparator(database *const &d, db::comparator user);
};
