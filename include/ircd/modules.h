/*
 *  IRCd (Charybdis): Pushing the envelope since '88
 *  modules.h: A header for the modules functions.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
 *  Copyright (C) 2016 Charybdis Development Team
 *  Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 */

#pragma once
#define HAVE_IRCD_MODULES_H

#ifdef __cplusplus
namespace ircd {
namespace mapi {

	using magic_t = uint16_t;
	using version_t = uint16_t;
	struct header;

} // namespace mapi

namespace mods {

using mapi::magic_t;
using mapi::version_t;

IRCD_EXCEPTION(ircd::error, error)
IRCD_EXCEPTION(error, filesystem_error)
IRCD_EXCEPTION(error, invalid_export)

struct mod;
bool has(const mod &mod, const std::string &symbol);
const uint8_t *ptr(const mod &mod, const std::string &symbol);
uint8_t *ptr(mod &mod, const std::string &symbol);
template<class T> const T *ptr(const mod &mod, const std::string &symbol);
template<class T> const T &get(const mod &mod, const std::string &symbol);
template<class T> T *ptr(mod &mod, const std::string &symbol);
template<class T> T &get(mod &mod, const std::string &symbol);
const mapi::header &header(const mod &);
const std::string &meta(const mod &, const std::string &key);
const version_t &version(const mod &);
const int64_t &timestamp(const mod &);
const std::string &desc(const mod &);
std::string location(const mod &);
std::string name(const mod &);

extern struct log::log log;

// Symbol handlers
struct type_handlers
{
	std::function<void (mod &, const std::string &sym)> loader;
	std::function<void (mod &, const std::string &sym)> unloader;
	std::function<void (mod &, const std::string &sym)> reloader;
};
template<class T> std::type_index make_index();
bool add(const std::type_index &, const type_handlers &handlers);
bool del(const std::type_index &);
bool has(const std::type_index &);
template<class T, class... type_handlers> bool add(type_handlers&&... handlers);
template<class T> bool del();
template<class T> bool has();

// The search path vector
std::vector<std::string> paths();
bool path_added(const std::string &dir);
void path_del(const std::string &dir);
bool path_add(const std::string &dir, std::nothrow_t);  // logs errors and returns false
bool path_add(const std::string &dir);                  // false if exists, throws other errors
void path_clear();

// Dump object data
std::vector<std::string> symbols(const std::string &fullpath, const std::string &section);
std::vector<std::string> symbols(const std::string &fullpath);
std::vector<std::string> sections(const std::string &fullpath);

// Checks if loadable module containing a mapi header (does not verify the magic)
bool is_module(const std::string &fullpath, std::string &why);
bool is_module(const std::string &fullpath, std::nothrow_t);
bool is_module(const std::string &fullpath);

// returns dir/name of first dir containing 'name' (and this will be a loadable module)
// Unlike libltdl, the reason each individual candidate failed is presented in a vector.
std::string search(const std::string &name, std::vector<std::string> &why);
std::string search(const std::string &name);

// Potential modules available to load
std::forward_list<std::string> available();
bool available(const std::string &name);

// Find module names where symbol resides
bool has_symbol(const std::string &name, const std::string &symbol);
std::vector<std::string> find_symbol(const std::string &symbol);

// Modules currently loaded
const std::map<std::string, std::unique_ptr<mod>> &loaded();
bool loaded(const std::string &name);
mod &get(const std::string &name);

bool reload(const std::string name);
bool unload(const std::string name);
bool load(const std::string &name);
void autoload();
void unload();


template<class T>
bool
has()
{
	return has(make_index<T>());
}

template<class T>
bool
del()
{
	return del(make_index<T>());
}

template<class T,
         class... type_handlers>
bool
add(type_handlers&&... handlers)
{
	return add(make_index<T>(), {std::forward<type_handlers>(handlers)...});
}

template<class T>
std::type_index
make_index()
{
	return typeid(typename std::add_pointer<T>::type);
}

template<class T>
T &
get(mod &mod,
    const std::string &symbol)
{
	return *reinterpret_cast<T *>(ptr(mod, symbol));
}

template<class T>
T *
ptr(mod &mod,
    const std::string &symbol)
{
	return reinterpret_cast<T *>(ptr(mod, symbol));
}

template<class T>
const T &
get(const mod &mod,
    const std::string &symbol)
{
	return *reinterpret_cast<const T *>(ptr(mod, symbol));
}

template<class T>
const T *
ptr(const mod &mod,
    const std::string &symbol)
{
	return reinterpret_cast<const T *>(ptr(mod, symbol));
}

}      // namespace mods
}      // namespace ircd
#endif // __cplusplus
