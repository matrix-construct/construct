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

namespace ircd {
namespace mods {

struct mod
:std::enable_shared_from_this<mod>
{
	static std::map<std::string, mod *> loaded;

	boost::dll::shared_library handle;
	mapi::header *header;

	// Metadata
	auto &operator[](const std::string &s) const { return header->meta.operator[](s);              }
	auto &operator[](const std::string &s)       { return header->meta.operator[](s);              }

	auto name() const                            { return handle.location().filename().string();   }
	auto location() const                        { return handle.location().string();              }
	auto &version() const                        { return header->version;                         }
	auto &description() const                    { return (*this)["description"];                  }

	bool has(const std::string &name) const;
	template<class T> const T &get(const std::string &name) const;
	template<class T> T &get(const std::string &name);
	template<class T = uint8_t> const T *ptr(const std::string &name) const;
	template<class T = uint8_t> T *ptr(const std::string &name);

	mod(const filesystem::path &,
	    const load_mode::type & = load_mode::rtld_local | load_mode::rtld_now);

	~mod() noexcept;
};

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

inline bool
mod::has(const std::string &name)
const
{
	return handle.has(name);
}

} // namespace mods
} // namespace ircd
