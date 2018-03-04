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
#define HAVE_IRCD_SERVER_LINK_H

/// A single connection to a remote node.
///
struct ircd::server::link
{
	static conf::item<size_t> tag_max_default;
	static conf::item<size_t> tag_commit_max_default;

	bool init {false};                           ///< link is connecting
	bool fini {false};                           ///< link is disconnecting
	bool exclude {false};                        ///< link is excluded
	bool waiting_write {false};                  ///< async operation state
	bool waiting_read {false};                   ///< async operation state
	int8_t handles {0};                          ///< async operation state
	std::shared_ptr<server::node> node;          ///< backreference to node
	std::shared_ptr<net::socket> socket;         ///< link's socket
	std::list<tag> queue;                        ///< link's work queue

	template<class F> size_t accumulate_tags(F&&) const;

	void inc_handles();
	void dec_handles();

	void discard_read();
	const_buffer process_read_next(const const_buffer &, tag &, bool &done);
	bool process_read(const_buffer &);
	void handle_readable_success();
	void handle_readable(const error_code &);
	void wait_readable();

	const_buffer process_write_next(const const_buffer &);
	bool process_write(tag &);
	void handle_writable_success();
	void handle_writable(const error_code &);
	void wait_writable();

	void handle_close(std::exception_ptr);
	void handle_open(std::exception_ptr);

  public:
	// config related
	size_t tag_max() const;
	size_t tag_commit_max() const;

	// indicator lights
	bool opened() const noexcept;
	bool ready() const;
	bool busy() const;

	// stats for upload-side bytes across all tags
	size_t write_total() const;
	size_t write_completed() const;
	size_t write_remaining() const;

	// stats for download-side bytes ~across all tags~; note: this is not
	// accurate except for the one tag at the front of the queue having
	// its response processed.
	size_t read_total() const;         // see: tag::read_total() notes
	size_t read_completed() const;     // see: tag::read_completed() notes
	size_t read_remaining() const;     // see: tag::read_remaining() notes

	// stats for tags
	size_t tag_count() const;
	size_t tag_committed() const;
	size_t tag_uncommitted() const;

	// request panel
	void submit(request &);

	// control panel
	bool close(const net::close_opts & = net::close_opts_default);
	bool open(const net::open_opts &);

	link(server::node &);
	~link() noexcept;
};
