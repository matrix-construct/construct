// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

struct construct::signals
{
	std::unique_ptr<boost::asio::signal_set> signal_set;
	ircd::run::changed runlevel_changed;

	void set_handle();
	void on_signal(const boost::system::error_code &, int) noexcept;
	void on_runlevel(const enum ircd::run::level &);

  public:
	signals(boost::asio::io_context &ios);
};
