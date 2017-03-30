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

namespace ircd {
namespace db   {

enum op
{
	GET,
	SET,
	MERGE,
	DELETE,
	DELETE_RANGE,
	SINGLE_DELETE,
};

struct delta
:std::tuple<op, string_view, string_view>
{
	delta(const enum op &op, const string_view &key, const string_view &val = {})
	:std::tuple<enum op, string_view, string_view>{op, key, val}
	{}

	delta(const string_view &key, const string_view &val, const enum op &op = op::SET)
	:std::tuple<enum op, string_view, string_view>{op, key, val}
	{}
};

using merge_delta = std::pair<string_view, string_view>;
using merge_closure = std::function<std::string (const string_view &key, const merge_delta &)>;
using update_closure = std::function<std::string (const string_view &key, merge_delta &)>;
using view_closure = std::function<void (const string_view &)>;

struct comparator
{
	std::string name;
	std::function<bool (const string_view &, const string_view &)> less;
	std::function<bool (const string_view &, const string_view &)> equal;
};

} // namespace db
} // namespace ircd
