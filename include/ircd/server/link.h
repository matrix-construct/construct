// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#pragma once
#define HAVE_IRCD_SERVER_LINK_H

/// A single connection to a remote node.
///
struct ircd::server::link
{
	bool init {false};                           ///< link is connecting
	bool fini {false};                           ///< link is disconnecting
	std::shared_ptr<server::node> node;          ///< backreference to node
	std::shared_ptr<net::socket> socket;         ///< link's socket
	std::deque<tag> queue;                       ///< link's work queue

	template<class F> size_t accumulate_tags(F&&) const;

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
	bool connected() const noexcept;
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
	size_t tag_total() const;
	size_t tag_committed() const;
	size_t tag_uncommitted() const;

	// request panel
	tag cancel(request &);
	void submit(request &);

	// control panel
	bool close(const net::close_opts &);
	bool open(const net::open_opts &);

	link(server::node &);
	~link() noexcept;
};
