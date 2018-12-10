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
#define HAVE_IRCD_DB_DATABASE_WAL_H

struct ircd::db::database::wal
{
	struct info;
};

/// Get info about a WAL file
///
struct ircd::db::database::wal::info
{
	struct vector;

	std::string name;
	uint64_t number {0};
	uint64_t seq {0};
	size_t size {0};
	bool alive {false};

	info(const database &, const string_view &filename);
	info() = default;

	info &operator=(const rocksdb::LogFile &);
};

struct ircd::db::database::wal::info::vector
:std::vector<info>
{
	vector() = default;
	explicit vector(const database &);
};
