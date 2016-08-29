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

#ifdef __cplusplus
namespace ircd {
namespace mapi {

struct init
:std::function<void ()>
{
	using std::function<void ()>::function;
};

struct fini
:std::function<void ()>
{
	using std::function<void ()>::function;
};

enum flags
{
	NO_FLAGS          = 0x00,
	RELAXED_INIT      = 0x01,
};

// Associates RTTI keyed by object location
using export_vector = std::vector<std::pair<const void *, std::type_index>>;

struct exports
:export_vector
{
	template<class... List> exports(List&&... list);
};

struct header
{
	static const char *const sym_name;
	static constexpr const magic_t MAGIC
	{
		0x4D41
	};

	magic_t magic;           // The magic must match MAGIC
	version_t version;       // Version indicator
	enum flags flags;        // Option flags
	int64_t timestamp;       // Module's compile epoch
	const char *desc;
	struct exports exports;

	template<class... Exports>
	header(const char *const &desc,
	       const enum flags &flags,
	       Exports&&... exports);
};

template<class... Exports>
header::header(const char *const &desc,
               const enum flags &flags,
               Exports&&... exports)
:magic(MAGIC)
,version(4)
,flags(flags)
,timestamp(RB_DATECODE)
,desc(desc)
,exports{std::forward<Exports>(exports)...}
{
}

const char *const
header::sym_name
{
	"IRCD_MODULE"
};

template<class... List>
exports::exports(List&&... list)
{
	const std::vector<std::type_index> types
	{
		typeid(List)...
	};

	const std::vector<const void *> ptrs
	{
		std::forward<List>(list)...
	};

	assert(types.size() == ptrs.size());
	for(size_t i(0); i < types.size(); ++i)
		this->emplace_back(ptrs.at(i), types.at(i));
}

} // namespace mapi
} // namespace ircd
#endif // __cplusplus
