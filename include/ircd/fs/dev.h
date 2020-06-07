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
	struct init;
	struct blkdev;
	using major_minor = std::pair<ulong, ulong>;

	// Convert device ID's with the major(3) / minor(3) / makedev(3)
	ulong id(const major_minor &);
	major_minor id(const ulong &id);

	// Convert device id integers into a string used as the directory name in sysfs(5)
	string_view sysfs_id(const mutable_buffer &out, const major_minor &);
	string_view sysfs_id(const mutable_buffer &out, const ulong &);

	// Read data for a device from sysfs; path is relative to /sys/dev/block/$id/...
	string_view sysfs(const mutable_buffer &out, const ulong &id, const string_view &path);
	template<class T = size_t, size_t bufmax = 32> T sysfs(const ulong &id, const string_view &path, const T &def = 0);

	extern std::map<major_minor, blkdev> block;
}

struct ircd::fs::dev::blkdev
{
	bool is_device {false};
	bool is_queue {false};
	std::string type;
	std::string vendor;
	std::string model;
	std::string rev;
	size_t size {0};
	size_t queue_depth {0};
	size_t nr_requests {0};
	bool rotational {false};

	blkdev(const ulong &id);
	blkdev() = default;
};

struct ircd::fs::dev::init
{
	init();
	~init() noexcept;
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
