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
#define HAVE_IRCD_DB_DELTA_H

namespace ircd::db
{
	enum op
	{
		GET,                     // no-op sentinel, do not use (debug asserts)
		SET,                     // (batch.Put)
		MERGE,                   // (batch.Merge)
		DELETE,                  // (batch.Delete)
		DELETE_RANGE,            // (batch.DeleteRange)
		SINGLE_DELETE,           // (batch.SingleDelete)
	};

	// Indicates an op uses both a key and value for its operation. Some only use
	// a key name so an empty value argument in a delta is okay when false.
	bool value_required(const op &op);

	using merge_delta = std::pair<string_view, string_view>;
	using merge_closure = std::function<std::string (const string_view &key, const merge_delta &)>;
	using update_closure = std::function<std::string (const string_view &key, merge_delta &)>;

	struct comparator;
}

struct ircd::db::comparator
{
	std::string name;
	std::function<bool (const string_view &, const string_view &)> less;
	std::function<bool (const string_view &, const string_view &)> equal;
};
