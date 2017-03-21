/*
 *  ircd-ratbox: A slightly useful ircd.
 *  modules.c: A module loader.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
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

#include <boost/filesystem.hpp>
#include <boost/dll.hpp>

namespace filesystem = boost::filesystem;
namespace load_mode = boost::dll::load_mode;

#include <ircd/mapi.h>  // Module's internal API

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

filesystem::path prefix_if_relative(const filesystem::path &path);
filesystem::path postfixed(const filesystem::path &path);
std::string postfixed(const std::string &name);

template<class R, class F> R info(const filesystem::path &, F&& closure);
std::vector<std::string> sections(const filesystem::path &path);
std::vector<std::string> symbols(const filesystem::path &path);
std::vector<std::string> symbols(const filesystem::path &path, const std::string &section);

// Get the full path of a [valid] available module by name
filesystem::path fullpath(const std::string &name);

// Checks if loadable module containing a mapi header (does not verify the magic)
bool is_module(const filesystem::path &);
bool is_module(const filesystem::path &, std::string &why);
bool is_module(const filesystem::path &, std::nothrow_t);
bool is_module(const std::string &fullpath, std::string &why);
bool is_module(const std::string &fullpath, std::nothrow_t);
bool is_module(const std::string &fullpath);

struct log::log log
{
	"modules", 'M'
};

} // namespace mods
} // namespace ircd

ircd::mods::init::init()
{
}

ircd::mods::init::~init()
noexcept
{
}

///////////////////////////////////////////////////////////////////////////////
//
// module
//

namespace ircd {
namespace mods {

} // namespace mods
} // namespace ircd

ircd::mods::module::module(const std::string &name)
try
:std::shared_ptr<mod>{[&name]
{
	const auto path(fullpath(name));
	const auto filename(postfixed(name));

	// Search for loaded module and increment the reference counter for this handle if loaded.
	auto it(mod::loaded.find(filename));
	if(it != end(mod::loaded))
	{
		auto &mod(*it->second);
		return shared_from(mod);
	}

	static const load_mode::type flags
	{
		load_mode::rtld_local |
		load_mode::rtld_now
	};

	log.debug("Attempting to load '%s' @ `%s'", filename, path.string());
	return std::make_shared<mod>(path, flags);
}()}
{
}
catch(const std::exception &e)
{
	log.error("Failed to load '%s': %s", name, e.what());
	throw;
}

ircd::mods::module::~module()
noexcept
{
}

std::string
ircd::mods::module::path()
const
{
	if(unlikely(!*this))
		return std::string{};

	auto &mod(**this);
	return mod.location();
}

std::string
ircd::mods::module::name()
const
{
	if(unlikely(!*this))
		return std::string{};

	auto &mod(**this);
	return mod.name();
}

template<> uint8_t *
ircd::mods::module::ptr<uint8_t>(const std::string &name)
{
	auto &mod(**this);
	return mod.ptr<uint8_t>(name);
}

template<>
const uint8_t *
ircd::mods::module::ptr<const uint8_t>(const std::string &name)
const
{
	const auto &mod(**this);
	return mod.ptr<const uint8_t>(name);
}

bool
ircd::mods::module::has(const std::string &name)
const
{
	if(unlikely(!*this))
		return false;

	const auto &mod(**this);
	return mod.has(name);
}

///////////////////////////////////////////////////////////////////////////////
//
// sym_ptr
//

ircd::mods::sym_ptr::sym_ptr(const std::string &modname,
                             const std::string &symname)
:std::weak_ptr<mod>
{
	module(modname)
}
,ptr{[this, &modname, &symname]
{
	const life_guard<mods::mod> mod(*this);

	if(unlikely(!mod->has(symname)))
		throw undefined_symbol("Could not find symbol '%s' in module '%s'", symname, mod->name());

	return mod->ptr(symname);
}()}
{
}

ircd::mods::sym_ptr::~sym_ptr()
noexcept
{
}

///////////////////////////////////////////////////////////////////////////////
//
// misc
//

namespace ircd {
namespace mods {

} // namespace mods
} // namespace ircd

bool
ircd::mods::loaded(const std::string &name)
{
	return mod::loaded.count(postfixed(name));
}

bool
ircd::mods::available(const std::string &name)
{
	using filesystem::path;

	std::vector<std::string> why;
	return !search(name, why).empty();
}

std::string
ircd::mods::search(const std::string &name)
{
	std::vector<std::string> why;
	return search(name, why);
}

std::vector<std::string>
ircd::mods::find_symbol(const std::string &symbol)
{
	std::vector<std::string> ret;
	const auto av(available());
	std::copy_if(begin(av), end(av), std::back_inserter(ret), [&symbol]
	(const auto &name)
	{
		return has_symbol(name, symbol);
	});

	return ret;
}

bool
ircd::mods::has_symbol(const std::string &name,
                       const std::string &symbol)
{
	const auto fullpath(search(name));
	if(fullpath.empty())
		return false;

	const auto syms(symbols(fullpath));
	return std::find(begin(syms), end(syms), symbol) != end(syms);
}

filesystem::path
ircd::mods::fullpath(const std::string &name)
{
	std::vector<std::string> why;
	const filesystem::path path(search(name, why));
	if(path.empty())
	{
		for(const auto &str : why)
			log.error("candidate for module '%s' failed: %s", name, str);

		throw error("No valid module by name `%s'", name);
	}

	return path;
}

std::string
ircd::mods::search(const std::string &name,
                   std::vector<std::string> &why)
{
	using filesystem::path;

	const path path(postfixed(name));
	if(!path.is_relative())
	{
		why.resize(why.size() + 1);
		return is_module(path, why.back())? name : std::string{};
	}
	else for(const auto &dir : paths)
	{
		why.resize(why.size() + 1);
		if(is_module(dir/path, why.back()))
			return (dir/path).string();
	}

	return {};
}

std::forward_list<std::string>
ircd::mods::available()
{
	using filesystem::path;
	using filesystem::directory_iterator;

	std::forward_list<std::string> ret;
	for(const auto &dir : paths) try
	{
		for(directory_iterator it(dir); it != directory_iterator(); ++it)
			if(is_module(it->path(), std::nothrow))
				ret.emplace_front(relative(it->path(), dir).string());
	}
	catch(const filesystem::filesystem_error &e)
	{
		log.warning("Module path [%s]: %s", dir, e.what());
		continue;
	}

	return ret;
}

bool
ircd::mods::is_module(const std::string &fullpath)
{
	return is_module(filesystem::path(fullpath));
}

bool
ircd::mods::is_module(const std::string &fullpath,
                      std::nothrow_t)
{
	return is_module(filesystem::path(fullpath), std::nothrow);
}

bool
ircd::mods::is_module(const std::string &fullpath,
                      std::string &why)
{
	return is_module(filesystem::path(fullpath), why);
}

bool
ircd::mods::is_module(const filesystem::path &path,
                      std::nothrow_t)
try
{
	return is_module(path);
}
catch(const std::exception &e)
{
	return false;
}

bool
ircd::mods::is_module(const filesystem::path &path,
                      std::string &why)
try
{
	return is_module(path);
}
catch(const std::exception &e)
{
	why = e.what();
	return false;
}

bool
ircd::mods::is_module(const filesystem::path &path)
{
	if(!exists(path))
		throw filesystem_error("`%s' does not exist", path.string());

	if(!is_regular_file(path))
		throw filesystem_error("`%s' is not a file", path.string());

	const auto syms(symbols(path));
	const auto &header_name(mapi::header_symbol_name);
	const auto it(std::find(begin(syms), end(syms), header_name));
	if(it == end(syms))
		throw error("`%s': has no MAPI header (%s)", path.string(), header_name);

	return true;
}

std::vector<std::string>
ircd::mods::sections(const std::string &fullpath)
{
	return sections(filesystem::path(fullpath));
}

std::vector<std::string>
ircd::mods::symbols(const std::string &fullpath)
{
	return symbols(filesystem::path(fullpath));
}

std::vector<std::string>
ircd::mods::symbols(const std::string &fullpath,
                    const std::string &section)
{
	return symbols(filesystem::path(fullpath), section);
}

std::vector<std::string>
ircd::mods::sections(const filesystem::path &path)
{
	return info<std::vector<std::string>>(path, []
	(boost::dll::library_info &info)
	{
		return info.sections();
	});
}

std::vector<std::string>
ircd::mods::symbols(const filesystem::path &path)
{
	return info<std::vector<std::string>>(path, []
	(boost::dll::library_info &info)
	{
		return info.symbols();
	});
}

std::vector<std::string>
ircd::mods::symbols(const filesystem::path &path,
                    const std::string &section)
{
	return info<std::vector<std::string>>(path, [&section]
	(boost::dll::library_info &info)
	{
		return info.symbols(section);
	});
}

template<class R,
         class F>
R
ircd::mods::info(const filesystem::path &path,
                 F&& closure)
{
	if(!exists(path))
		throw filesystem_error("`%s' does not exist", path.string());

	if(!is_regular_file(path))
		throw filesystem_error("`%s' is not a file", path.string());

	boost::dll::library_info info(path);
	return closure(info);
}

///////////////////////////////////////////////////////////////////////////////
//
// paths
//

namespace ircd {
namespace mods {

const filesystem::path modroot
{
	ircd::path::get(ircd::path::MODULES)
};

struct paths paths;

} // namespace mods
} // namespace ircd

ircd::mods::paths::paths()
:std::vector<std::string>
{{
	modroot.string()
}}
{
}

bool
ircd::mods::paths::add(const std::string &dir)
{
	using filesystem::path;

	const path path(prefix_if_relative(dir));

	if(!exists(path))
		throw filesystem_error("path `%s' (%s) does not exist", dir, path.string());

	if(!is_directory(path))
		throw filesystem_error("path `%s' (%s) is not a directory", dir, path.string());

	if(added(dir))
		return false;

	emplace(begin(), dir);
	return true;
}

bool
ircd::mods::paths::add(const std::string &dir,
                       std::nothrow_t)
try
{
	return add(dir);
}
catch(const std::exception &e)
{
	log.error("Failed to add path: %s", e.what());
	return false;
}

bool
ircd::mods::paths::del(const std::string &dir)
{
	std::remove(begin(), end(), prefix_if_relative(dir).string());
	return true;
}

bool
ircd::mods::paths::added(const std::string &dir)
const
{
	return std::find(begin(), end(), dir) != end();
}

std::string
ircd::mods::postfixed(const std::string &name)
{
	return postfixed(filesystem::path(name)).string();
}

filesystem::path
ircd::mods::postfixed(const filesystem::path &path)
{
	static const auto suffix(boost::dll::shared_library::suffix());

	if(extension(path) == suffix)
		return path;

	filesystem::path ret(path);
	return ret += suffix;
}

filesystem::path
ircd::mods::prefix_if_relative(const filesystem::path &path)
{
	return path.is_relative()? (modroot / path) : path;
}

///////////////////////////////////////////////////////////////////////////////
//
// mod (internal)
//

namespace ircd {
namespace mods {

std::map<std::string, mod *> mod::loaded;

} // namespace mods
} // namespace ircd

ircd::mods::mod::mod(const filesystem::path &path,
                     const load_mode::type &type)
try
:handle
{
	path, type
}
,header
{
	&handle.get<mapi::header>(mapi::header_symbol_name)
}
{
	log.debug("Loaded static segment of '%s' @ `%s'", name(), path.string());

	if(unlikely(!header))
		throw error("Unexpected null header");

	if(header->magic != mapi::MAGIC)
		throw error("Bad magic [%04x] need: [%04x]", header->magic, mapi::MAGIC);

	// Set some basic metadata
	auto &meta(header->meta);
	meta["name"] = name();
	meta["location"] = location();

	// If init throws an exception from here the loading process will back out.
	if(header->init)
		header->init();

	// Without init exception, the module is now considered loaded.
	loaded.emplace(name(), this);
	log.info("Loaded module %s v%u \"%s\"",
	         name(),
	         header->version,
	         description().size()? description() : "<no description>"s);
}
catch(const boost::system::system_error &e)
{
	throw error("%s", e.what());
}

// Allows module to communicate static destruction is taking place when mapi::header
// destructs. If dlclose() returns without this being set, dlclose() lied about really
// unloading the module. That is considered a "stuck" module.
bool ircd::mapi::static_destruction;

ircd::mods::mod::~mod()
noexcept try
{
	const auto name(this->name());
	log.debug("Attempting unload module '%s' @ `%s'", name, location());

	const size_t erased(loaded.erase(name));
	assert(erased == 1);

	if(header->fini)
		header->fini();

	log.debug("Attempting static unload for '%s' @ `%s'", name, location());
	mapi::static_destruction = false;
	handle.unload();
	assert(!handle.is_loaded());
	if(!mapi::static_destruction)
	{
		log.error("Module \"%s\" is stuck and failing to unload.", name);
		log.warning("Module \"%s\" may result in undefined behavior if not fixed.", name);
	} else {
		log.info("Unloaded '%s'", name);
	}
}
catch(const std::exception &e)
{
	log::critical("Module @%p unload: %s", (const void *)this, e.what());

	if(!ircd::debugmode)
		return;
}

template<class T>
T *
ircd::mods::mod::ptr(const std::string &name)
{
	return &handle.get<T>(name);
}

template<class T>
const T *
ircd::mods::mod::ptr(const std::string &name)
const
{
	return &handle.get<T>(name);
}

template<class T>
T &
ircd::mods::mod::get(const std::string &name)
{
	handle.get<T>(name);
}

template<class T>
const T &
ircd::mods::mod::get(const std::string &name)
const
{
	handle.get<T>(name);
}

bool
ircd::mods::mod::has(const std::string &name)
const
{
	return handle.has(name);
}
