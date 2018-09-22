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
#define HAVE_IRCD_DB_DATABASE_FILEINFO_H

namespace ircd::db
{
	void sst_dump(const vector_view<const string_view> &args);
}

/// Database snapshot object. Maintaining this object will maintain a
/// consistent state of access to the database at the sequence number
/// from when it's acquired.
struct ircd::db::database::fileinfo
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

	fileinfo() = default;
	fileinfo(rocksdb::LiveFileMetaData &&);
	fileinfo(const database &, const string_view &filename);
};

struct ircd::db::database::fileinfo::vector
:std::vector<fileinfo>
{
	vector() = default;
	explicit vector(const database &);
};
