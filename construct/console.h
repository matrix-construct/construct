// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

struct construct::console
{
	static ircd::conf::item<size_t> stack_sz;
	static ircd::conf::item<size_t> input_max;
	static ircd::conf::item<size_t> ratelimit_bytes;
	static ircd::conf::item<ircd::milliseconds> ratelimit_sleep;

	static const ircd::string_view generic_message;
	static const ircd::string_view console_message;
	static std::once_flag seen_message;
	static std::deque<std::string> queue;

	std::string line;
	std::string record_path;
	ircd::module *module {nullptr};
	ircd::context context;
	ircd::run::changed runlevel_changed;
	std::deque<std::string> history;

	void show_message() const;
	void on_runlevel(const enum ircd::run::level &);
	bool wait_running() const;
	bool next_command();
	bool esc_handle_bra();
	bool esc_handle();
	void wait_input();

	bool cmd__record();
	bool cmd__watch();
	int handle_line_bymodule();
	bool handle_line();
	void main();

	console();

	static bool active();
	static bool interrupt();
	static bool terminate();
	static bool execute(std::string cmd);
	static bool spawn();
};
