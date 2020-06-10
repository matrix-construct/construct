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
#define HAVE_IRCD_FS_DEV_H

namespace ircd::fs::dev
{
	struct blk;

	using major_minor = std::pair<ulong, ulong>;
	using blk_closure = std::function<bool (const ulong &id, const blk &)>;

	// Convert device ID's with the major(3) / minor(3) / makedev(3)
	ulong id(const major_minor &);
	major_minor id(const ulong &id);

	// Convert device id integers into a string used as the directory name in sysfs(5)
	string_view sysfs_id(const mutable_buffer &out, const major_minor &);
	string_view sysfs_id(const mutable_buffer &out, const ulong &);

	// Read data for a device from sysfs; path is relative to /sys/dev/block/$id/...
	string_view sysfs(const mutable_buffer &out, const ulong &id, const string_view &path);

	// Read data for a device lexical_cast'ed to type
	template<class R = size_t,
	         size_t bufmax = 32>
	R
	sysfs(const ulong &id,
	      const string_view &path,
	      const R &def = 0);

	bool for_each(const string_view &devtype, const blk_closure &);
	bool for_each(const blk_closure &);
}

struct ircd::fs::dev::blk
{
	static const size_t SECTOR_SIZE;
	static const string_view BASE_PATH;

	static string_view devtype(const mutable_buffer &, const ulong &id);

	std::string type;
	std::string vendor;
	std::string model;
	std::string rev;
	size_t sector_size {0};
	size_t physical_block {0};
	size_t logical_block {0};
	size_t minimum_io {0};
	size_t optimal_io {0};
	size_t sectors {0};
	size_t queue_depth {0};
	size_t nr_requests {0};
	std::string scheduler;
	bool rotational {false};
	bool merges {false};

	blk(const ulong &id);
	blk() = default;
};

/// Return a lex_cast'able (an integer) from a sysfs target.
template<class T,
         size_t bufmax>
T
ircd::fs::dev::sysfs(const ulong &id,
                     const string_view &path,
                     const T &def)
{
	char buf[bufmax];
	const string_view val
	{
		sysfs(buf, id, path)
	};

	return lex_castable<T>(val)?
		lex_cast<T>(val):
		def;
}
