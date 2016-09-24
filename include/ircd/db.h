/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once
#define HAVE_IRCD_DB_H

namespace rocksdb
{
	struct DB;
}

namespace ircd {
namespace db   {

IRCD_EXCEPTION(ircd::error, error)
IRCD_EXCEPTION(error, not_found)
IRCD_EXCEPTION(error, corruption)
IRCD_EXCEPTION(error, not_supported)
IRCD_EXCEPTION(error, invalid_argument)
IRCD_EXCEPTION(error, io_error)
IRCD_EXCEPTION(error, merge_in_progress)
IRCD_EXCEPTION(error, incomplete)
IRCD_EXCEPTION(error, shutdown_in_progress)
IRCD_EXCEPTION(error, timed_out)
IRCD_EXCEPTION(error, aborted)
IRCD_EXCEPTION(error, busy)
IRCD_EXCEPTION(error, expired)
IRCD_EXCEPTION(error, try_again)

std::string path(const std::string &name);

struct opts
{
	bool create_if_missing = true;
};

struct read_opts
{
};

struct write_opts
{
};

class handle
{
	std::unique_ptr<struct meta> meta;
	std::unique_ptr<rocksdb::DB> d;

  public:
	using char_closure = std::function<void (const char *, size_t)>;
	using string_closure = std::function<void (const std::string &)>;

	bool has(const std::string &key, const read_opts & = {});
	void get(const std::string &key, const char_closure &, const read_opts & = {});
	void set(const std::string &key, const std::string &value, const write_opts & = {});

	handle(const std::string &name, const opts &opts = {});
	~handle() noexcept;
};

struct init
{
	init();
	~init() noexcept;
};

} // namespace db
} // namespace ircd
