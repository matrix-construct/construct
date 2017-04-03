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
 */

#pragma once
#define HAVE_IRCD_MAPI_H

namespace ircd {
namespace mapi {

struct header;
using magic_t = uint16_t;
using version_t = uint16_t;
using metadata = std::map<std::string, std::string>;
using init_function = std::function<void ()>;
using fini_function = std::function<void ()>;

const char *const header_symbol_name
{
	"IRCD_MODULE"
};

constexpr const magic_t MAGIC
{
	0x4D41
};

struct header
{
	magic_t magic;                               // The magic must match MAGIC
	version_t version;                           // Version indicator
	int64_t timestamp;                           // Module's compile epoch
	init_function init;                          // Executed after dlopen()
	fini_function fini;                          // Executed before dlclose()
	metadata meta;                               // Various key-value metadata

	// get and set metadata
	auto &operator[](const std::string &s) const;
	auto &operator[](const std::string &s);

	header(const char *const &desc = "<no description>",
	       init_function = {},
	       fini_function = {});

	~header() noexcept;
};

// Used to communicate whether a module unload actually took place. dlclose() is allowed to return
// success but the actual static destruction of the module's contents doesn't lie. (mods.cc)
extern bool static_destruction;

inline
header::header(const char *const &desc,
               init_function init,
               fini_function fini)
:magic(MAGIC)
,version(4)
,timestamp(RB_DATECODE)
,init{std::move(init)}
,fini{std::move(fini)}
,meta
{
	{ "description", desc }
}
{
}

inline
header::~header()
noexcept
{
	static_destruction = true;
}

inline auto &
header::operator[](const std::string &key)
{
	return meta[key];
}

inline auto &
header::operator[](const std::string &key)
const
{
	return meta.at(key);
}

} // namespace mapi
} // namespace ircd
