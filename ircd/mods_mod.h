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
#define HAVE_IRCD_MODS_MOD_H

#ifdef __cplusplus
namespace ircd {
namespace mods {

struct sym
{
	std::type_index type;

	sym(const std::type_index &type)
	:type{type}
	{
	}
};

struct mod
{
	boost::dll::shared_library handle;
	mapi::header *header;
	std::map<std::string, sym> handled;
	std::multimap<std::type_index, std::string> unhandled;

	bool has(const std::string &name) const;
	template<class T> const T &get(const std::string &name) const;
	template<class T> T &get(const std::string &name);
	template<class T = uint8_t> const T *ptr(const std::string &name) const;
	template<class T = uint8_t> T *ptr(const std::string &name);

	mod(const filesystem::path &, const load_mode::type &);
	~mod() noexcept;
};

mod::mod(const filesystem::path &path,
         const load_mode::type &type)
try
:handle
{
	path, type
}
,header
{
	&handle.get<mapi::header>(mapi::header::sym_name)
}
{
	if(unlikely(!header))
		throw error("Unexpected null header");

	if(header->magic != mapi::header::MAGIC)
		throw error("Bad magic [%04x] need: [%04x]",
		            header->magic,
		            mapi::header::MAGIC);

	// Set some basic metadata
	auto &meta(header->meta);
	meta["name"] = name(*this);
	meta["location"] = location(*this);
}
catch(const boost::system::system_error &e)
{
	throw error("%s", e.what());
}

mod::~mod()
noexcept try
{
	handle.unload();
	assert(!handle.is_loaded());
}
catch(const std::exception &e)
{
	log::critical("Module @%p unload: %s",
	              (const void *)this,
	              e.what());

	if(!ircd::debugmode)
		return;
}

template<class T>
T *
mod::ptr(const std::string &name)
{
	return &handle.get<T>(name);
}

template<class T>
const T *
mod::ptr(const std::string &name)
const
{
	return &handle.get<T>(name);
}

template<class T>
T &
mod::get(const std::string &name)
{
	handle.get<T>(name);
}

template<class T>
const T &
mod::get(const std::string &name)
const
{
	handle.get<T>(name);
}

bool
mod::has(const std::string &name)
const
{
	return handle.has(name);
}

const std::string &
desc(const mod &mod)
try
{
	return meta(mod, "description");
}
catch(const std::out_of_range &e)
{
	static const std::string empty;
	return empty;
}

std::string
name(const mod &mod)
{
	return mod.handle.location().filename().string();
}

std::string
location(const mod &mod)
{
	return mod.handle.location().string();
}

const version_t &
version(const mod &mod)
{
	return header(mod).version;
}

const mapi::exports &
exports(const mod &mod)
{
	return header(mod).exports;
}

const mapi::flags &
flags(const mod &mod)
{
	return header(mod).flags;
}

const std::string &
meta(const mod &mod,
     const std::string &key)
{
	return header(mod)[key];
}

const mapi::header &
header(const mod &mod)
{
	if(unlikely(!mod.header))
		throw error("Header unavailable");

	return *mod.header;
}

uint8_t *
ptr(mod &mod,
    const std::string &name)
{
	return mod.ptr<uint8_t>(name);
}

const uint8_t *
ptr(const mod &mod,
    const std::string &name)
{
	return mod.ptr<uint8_t>(name);
}

bool
has(const mod &mod,
    const std::string &name)
{
	return mod.has(name);
}

}      // namespace mods
}      // namespace ircd
#endif // __cplusplus
