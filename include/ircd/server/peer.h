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
#define HAVE_IRCD_SERVER_PEER_H

/// Remote entity.
///
struct ircd::server::peer
:std::enable_shared_from_this<ircd::server::peer>
{
	struct err;

	static conf::item<size_t> link_min_default;
	static conf::item<size_t> link_max_default;

	net::remote remote;
	std::list<link> links;
	std::unique_ptr<err> e;
	std::string server_name;
	bool ready {true};

	template<class F> size_t accumulate_links(F&&) const;
	template<class F> size_t accumulate_tags(F&&) const;

	void handle_resolve(std::weak_ptr<peer>, std::exception_ptr, const ipport &);
	void resolve(const hostport &);

	void disperse_uncommitted(link &);
	void disperse(link &);
	void del(link &);

	void handle_head_recv(const link &, const tag &, const http::response::head &);
	void handle_link_done(link &);
	void handle_tag_done(link &, tag &) noexcept;
	void handle_finished(link &);
	void handle_error(link &, const boost::system::system_error &);
	void handle_error(link &, std::exception_ptr);
	void handle_close(link &, std::exception_ptr);
	void handle_open(link &, std::exception_ptr);

  public:
	// config related
	size_t link_min() const;
	size_t link_max() const;

	// stats for all links in peer
	size_t link_count() const;
	size_t link_busy() const;
	size_t link_ready() const;

	// stats for all tags in all links in peer
	size_t tag_count() const;
	size_t tag_committed() const;
	size_t tag_uncommitted() const;

	// stats for all upload-side bytes in all tags in all links
	size_t write_total() const;
	size_t write_completed() const;
	size_t write_remaining() const;

	// stats for download-side bytes in all tags in all links (note:
	// see notes in link.h/tag.h about inaccuracy here).
	size_t read_total() const;
	size_t read_completed() const;
	size_t read_remaining() const;

	// link control panel
	link &link_add(const size_t &num = 1);
	link *link_get(const request &);

	// request panel
	void submit(request &);

	// Error related
	bool err_has() const;
	string_view err_msg() const;
	template<class... A> void err_set(A&&...);
	void err_clear();

	// control panel
	void interrupt();
	void close(const net::close_opts & = net::close_opts_default);

	peer();
	~peer() noexcept;
};

struct ircd::server::peer::err
{
	std::exception_ptr eptr;
	steady_point etime
	{
		now<steady_point>()
	};

	err(std::exception_ptr eptr)
	:eptr{std::move(eptr)}
	{}
};
