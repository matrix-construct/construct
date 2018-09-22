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
#define HAVE_IRCD_DB_DATABASE_SST_H

struct ircd::db::database::sst
{
	struct info;
	struct dump;

	static void tool(const vector_view<const string_view> &args);
};

/// Get info about an SST file.
///
struct ircd::db::database::sst::info
{
	struct vector;

	std::string name;
	std::string path;
	std::string column;
	size_t size {0};
	uint64_t min_seq {0};
	uint64_t max_seq {0};
	std::string min_key;
	std::string max_key;
	uint64_t num_reads {0};
	int level {-1};
	bool compacting {false};
	int32_t version {-1};
	uint64_t entries {0};

	info() = default;
	info(rocksdb::LiveFileMetaData &&);
	info(const database &, const string_view &filename);
};

struct ircd::db::database::sst::info::vector
:std::vector<info>
{
	vector() = default;
	explicit vector(const database &);
};

struct ircd::db::database::sst::dump
{
	using key_range = std::pair<string_view, string_view>;

	sst::info info;

	dump(db::column, const key_range & = {}, const string_view &path = {});
	dump(dump &&) = delete;
	dump(const dump &) = delete;
};
