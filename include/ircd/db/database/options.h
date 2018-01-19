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
#define HAVE_IRCD_DB_DATABASE_OPTIONS_H

/// options <-> string
struct ircd::db::database::options
:std::string
{
	struct map;

	// Output of options structures from this string
	explicit operator rocksdb::Options() const;
	operator rocksdb::DBOptions() const;
	operator rocksdb::ColumnFamilyOptions() const;
	operator rocksdb::PlainTableOptions() const;
	operator rocksdb::BlockBasedTableOptions() const;

	// Input of options structures output to this string
	explicit options(const rocksdb::ColumnFamilyOptions &);
	explicit options(const rocksdb::DBOptions &);
	explicit options(const database::column &);
	explicit options(const database &);

	// Input of options string from user
	options(std::string string)
	:std::string{std::move(string)}
	{}
};

/// options <-> map
struct ircd::db::database::options::map
:std::unordered_map<std::string, std::string>
{
	// Output of options structures from map
	operator rocksdb::DBOptions() const;
	operator rocksdb::ColumnFamilyOptions() const;
	operator rocksdb::PlainTableOptions() const;
	operator rocksdb::BlockBasedTableOptions() const;

	// Convert option string to map
	map(const options &);

	// Input of options map from user
	map(std::unordered_map<std::string, std::string> m)
	:std::unordered_map<std::string, std::string>{std::move(m)}
	{}
};
