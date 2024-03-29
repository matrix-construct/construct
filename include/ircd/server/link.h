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

/// A single connection to a remote peer.
///
struct ircd::server::link
{
	static conf::item<size_t> tag_max_default;
	static conf::item<size_t> tag_commit_max_default;
	static uint64_t ids;

	uint64_t id {++ids};                         ///< unique identifier of link.
	server::peer *peer;                          ///< backreference to peer
	std::shared_ptr<net::socket> socket;         ///< link's socket
	std::list<tag> queue;                        ///< link's work queue
	size_t tag_done {0L};                        ///< total tags processed
	time_t synack_ts {0L};                       ///< time socket was estab
	time_t read_ts {0L};                         ///< time of last read
	time_t write_ts {0L};                        ///< time of last write
	bool op_init {false};                        ///< link is connecting
	bool op_fini {false};                        ///< link is disconnecting
	bool op_open {false};
	bool op_write {false};                       ///< async operation state
	bool op_read {false};                        ///< async operation state
	bool exclude {false};                        ///< link is excluded

	template<class F> size_t accumulate_tags(F&&) const;

	void discard_read();
	const_buffer read(const mutable_buffer &buf);
	const_buffer process_read_next(const const_buffer &, tag &, bool &done);
	bool process_read(const_buffer &, unique_buffer<mutable_buffer> &);
	void handle_readable_success();
	void handle_readable(const error_code &) noexcept;
	void wait_readable();

	const_buffer process_write_next(const const_buffer &);
	bool process_write(tag &);
	void handle_writable_success();
	void handle_writable(const error_code &) noexcept;
	void wait_writable();

	void handle_close(std::exception_ptr);
	void handle_open(std::exception_ptr);
	void cleanup_canceled();

  public:
	// config related
	size_t tag_max() const;
	size_t tag_commit_max() const;

	// indicator lights
	bool finished() const;
	bool opened() const noexcept;
	bool ready() const;
	bool busy() const;

	// stats for upload-side bytes across all tags
	size_t write_size() const;
	size_t write_completed() const;
	size_t write_remaining() const;

	// stats for download-side bytes ~across all tags~; note: this is not
	// accurate except for the one tag at the front of the queue having
	// its response processed.
	size_t read_size() const;          // see: tag::read_total() notes
	size_t read_completed() const;     // see: tag::read_completed() notes
	size_t read_remaining() const;     // see: tag::read_remaining() notes

	// stats accumulated
	size_t write_total() const;
	size_t read_total() const;

	// stats for tags
	size_t tag_count() const;
	size_t tag_committed() const;
	size_t tag_uncommitted() const;

	// request panel
	void cancel_uncommitted(std::exception_ptr);
	void cancel_committed(std::exception_ptr);
	void cancel_all(std::exception_ptr);
	void submit(request &);

	// control panel
	bool close(const net::close_opts &);
	bool close(const net::dc = net::dc::SSL_NOTIFY);
	bool open(const net::open_opts &);

	link(server::peer &);
	link(link &&) = delete;
	link(const link &) = delete;
	~link() noexcept;
};
