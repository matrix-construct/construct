// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <boost/filesystem.hpp>
#include <boost/dll.hpp>

namespace filesystem = boost::filesystem;
namespace load_mode = boost::dll::load_mode;

#include <ircd/asio.h>
#include <ircd/mapi.h>  // Module's internal API
#include "mods.h"

ircd::log::log
ircd::mods::log
{
	"modules", 'M'
};

const filesystem::path
ircd::mods::suffix
{
	boost::dll::shared_library::suffix()
};

std::stack<ircd::mods::mod *>
ircd::mods::mod::loading
{};

std::map<std::string, ircd::mods::mod *>
ircd::mods::mod::loaded
{};

//
// mod (internal)
//

ircd::mods::mod::mod(const filesystem::path &path,
                     const load_mode::type &mode)
try
:path{path}
,mode{mode}
//,mangles{mods::mangles(path)}
,handle{[this, &path, &mode]
{
	const auto theirs
	{
		std::get_terminate()
	};

	const auto ours([]
	{
		log.critical("std::terminate() called during the static construction of a module.");

		if(std::current_exception()) try
		{
			std::rethrow_exception(std::current_exception());
		}
		catch(const std::exception &e)
		{
			log.error("%s", e.what());
		}
	});

	const unwind reset{[this, &theirs]
	{
		assert(loading.top() == this);
		loading.pop();
		std::set_terminate(theirs);
	}};

	loading.push(this);
	std::set_terminate(ours);
	return boost::dll::shared_library{path, mode};
}()}
,_name
{
	unpostfixed(handle.location().filename().string())
}
,_location
{
	handle.location().string()
}
,header
{
	&handle.get<mapi::header>(mapi::header_symbol_name)
}
{
	log.debug("Loaded static segment of '%s' @ `%s' with %zu symbols",
	          name(),
	          location(),
	          mangles.size());

	if(unlikely(!header))
		throw error("Unexpected null header");

	if(header->magic != mapi::MAGIC)
		throw error("Bad magic [%04x] need: [%04x]",
		            header->magic,
		            mapi::MAGIC);

	// Tell the module where to find us.
	header->self = this;

	// Set some basic metadata
	auto &meta(header->meta);
	meta["name"] = name();
	meta["location"] = location();

	if(!loading.empty())
	{
		const auto &m(mod::loading.top());
		m->children.emplace_back(this);
		log.debug("Module '%s' recursively loaded by '%s'",
		          name(),
		          m->path.filename().string());
	}

	// If init throws an exception from here the loading process will back out.
	if(header->init)
		header->init();

	log.info("Loaded module %s v%u \"%s\"",
	         name(),
	         header->version,
	         description().size()? description() : "<no description>"s);

	// Without init exception, the module is now considered loaded.
	loaded.emplace(name(), this);
}
catch(const boost::system::system_error &e)
{
	switch(e.code().value())
	{
		case boost::system::errc::bad_file_descriptor:
		{
			const string_view what(e.what());
			const auto pos(what.find("undefined symbol: "));
			if(pos == std::string_view::npos)
				break;

			const string_view msg(what.substr(pos));
			const std::string mangled(between(msg, ": ", ")"));
			const std::string demangled(demangle(mangled));
			throw error("undefined symbol: '%s' (%s)",
			            demangled,
			            mangled);
		}

		default:
			break;
	}

	throw error("%s", string(e));
}

// Allows module to communicate static destruction is taking place when mapi::header
// destructs. If dlclose() returns without this being set, dlclose() lied about really
// unloading the module. That is considered a "stuck" module.
bool ircd::mapi::static_destruction;

ircd::mods::mod::~mod()
noexcept try
{
	unload();
}
catch(const std::exception &e)
{
	log::critical("Module @%p unload: %s", (const void *)this, e.what());

	if(!ircd::debugmode)
		return;
}

bool
ircd::mods::mod::unload()
{
	if(!handle.is_loaded())
		return false;

	const auto name(this->name());
	log.debug("Attempting unload module '%s' @ `%s'", name, location());
	const size_t erased(loaded.erase(name));
	assert(erased == 1);

	if(header->fini)
		header->fini();

	// Save the children! dlclose() does not like to be called recursively during static
	// destruction of a module. The mod ctor recorded all of the modules loaded while this
	// module was loading so we can reverse the record and unload them here.
	// Note: If the user loaded more modules from inside their module they will have to dtor them
	// before 'static destruction' does. They can also do that by adding them to this vector.
	std::for_each(rbegin(this->children), rend(this->children), []
	(mod *const &ptr)
	{
		if(shared_from(*ptr).use_count() <= 2)
			ptr->unload();
	});

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

	return true;
}

const std::string &
ircd::mods::mod::mangle(const std::string &name)
const
{
	const auto it(mangles.find(name));
	if(it == end(mangles))
		return name;

	const auto &mangled(it->second);
	return mangled;
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

///////////////////////////////////////////////////////////////////////////////
//
// module
//

ircd::mods::module::module(const std::string &name)
try
:std::shared_ptr<mod>{[&name]
{
	// Search for loaded module and increment the reference counter for this handle if loaded.
	auto it(mod::loaded.find(name));
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

	const auto path(fullpath(name));
	log.debug("Attempting to load '%s' @ `%s'", name, path.string());
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

const std::string &
ircd::mods::module::path()
const
{
	if(unlikely(!*this))
	{
		static const std::string empty;
		return empty;
	}

	auto &mod(**this);
	return mod.location();
}

const std::string &
ircd::mods::module::name()
const
{
	if(unlikely(!*this))
	{
		static const std::string empty;
		return empty;
	}

	auto &mod(**this);
	return mod.name();
}

template<> uint8_t *
ircd::mods::module::ptr<uint8_t>(const std::string &name)
{
	auto &mod(**this);
	return mod.ptr<uint8_t>(mangle(name));
}

template<>
const uint8_t *
ircd::mods::module::ptr<const uint8_t>(const std::string &name)
const
{
	const auto &mod(**this);
	return mod.ptr<const uint8_t>(mangle(name));
}

bool
ircd::mods::module::has(const std::string &name)
const
{
	if(unlikely(!*this))
		return false;

	const auto &mod(**this);
	return mod.has(mangle(name));
}

const std::string &
ircd::mods::module::mangle(const std::string &name)
const
{
	if(unlikely(!*this))
	{
		static const std::string empty;
		return empty;
	}

	const auto &mod(**this);
	return mod.mangle(name);
}


///////////////////////////////////////////////////////////////////////////////
//
// sym_ptr
//

ircd::mods::sym_ptr::sym_ptr(const std::string &modname,
                             const std::string &symname)
:sym_ptr
{
	module(modname), symname
}
{
}

ircd::mods::sym_ptr::sym_ptr(module module,
                             const std::string &symname)
:std::weak_ptr<mod>
{
	module
}
,ptr{[this, &symname]
{
	const life_guard<mods::mod> mod{*this};
	const auto &mangled(mod->mangle(symname));
	if(unlikely(!mod->has(mangled)))
		throw undefined_symbol
		{
			"Could not find symbol '%s' (%s) in module '%s'",
			symname,
			mangled,
			mod->name()
		};

	return mod->ptr(mangled);
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
	return mod::loaded.count(name);
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
	const auto path(fullpath(name));
	if(path.empty())
		return false;

	const auto syms(symbols(path));
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
				ret.emplace_front(unpostfixed(relative(it->path(), dir).string()));
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
	const auto syms(symbols(path));
	const auto &header_name(mapi::header_symbol_name);
	const auto it(std::find(begin(syms), end(syms), header_name));
	if(it == end(syms))
		throw error("`%s': has no MAPI header (%s)", path.string(), header_name);

	return true;
}

std::unordered_map<std::string, std::string>
ircd::mods::mangles(const std::string &fullpath)
{
	return mangles(filesystem::path(fullpath));
}

std::unordered_map<std::string, std::string>
ircd::mods::mangles(const std::string &fullpath,
                    const std::string &section)
{
	return mangles(filesystem::path(fullpath), section);
}

std::unordered_map<std::string, std::string>
ircd::mods::mangles(const filesystem::path &path)
{
	return mangles(mods::symbols(path));
}

std::unordered_map<std::string, std::string>
ircd::mods::mangles(const filesystem::path &path,
                    const std::string &section)
{
	return mangles(mods::symbols(path, section));
}

std::unordered_map<std::string, std::string>
ircd::mods::mangles(const std::vector<std::string> &symbols)
{
	std::unordered_map<std::string, std::string> ret;
	for(const auto &sym : symbols) try
	{
		ret.emplace(demangle(sym), sym);
	}
	catch(const not_mangled &e)
	{
		ret.emplace(sym, sym);
	}

	return ret;
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

std::vector<std::string>
ircd::mods::sections(const std::string &fullpath)
{
	return sections(filesystem::path(fullpath));
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

namespace ircd::mods
{
	const filesystem::path modroot
	{
		ircd::fs::get(ircd::fs::MODULES)
	};

	struct paths paths;
}

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
ircd::mods::unpostfixed(const std::string &name)
{
	return unpostfixed(filesystem::path(name)).string();
}

std::string
ircd::mods::postfixed(const std::string &name)
{
	return postfixed(filesystem::path(name)).string();
}

filesystem::path
ircd::mods::unpostfixed(const filesystem::path &path)
{
	if(extension(path) != suffix)
		return path;

	return filesystem::path(path).replace_extension();
}

filesystem::path
ircd::mods::postfixed(const filesystem::path &path)
{
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
