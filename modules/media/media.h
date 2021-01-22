// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::media
{
	struct magick;

	static void init();
	static void fini();

	extern log::log log;
	extern conf::item<bool> blocks_cache_enable;
	extern conf::item<bool> blocks_cache_comp_enable;
	extern conf::item<size_t> blocks_cache_size;
	extern conf::item<size_t> blocks_cache_comp_size;
	extern conf::item<size_t> blocks_prefetch;
	extern conf::item<size_t> events_prefetch;
	extern const db::descriptor blocks_descriptor;
	extern const db::description description;
	extern std::shared_ptr<db::database> database;
	extern db::column blocks;

	extern conf::item<seconds> download_timeout;
	extern std::set<m::room::id> downloading;
	extern ctx::dock downloading_dock;
}

namespace ircd::m::media::thumbnail
{
	extern conf::item<bool> enable;
	extern conf::item<bool> enable_remote;
	extern conf::item<bool> animation_enable;
	extern conf::item<size_t> width_min;
	extern conf::item<size_t> width_max;
	extern conf::item<size_t> height_min;
	extern conf::item<size_t> height_max;
	extern conf::item<std::string> mime_whitelist;
	extern conf::item<std::string> mime_blacklist;
}
