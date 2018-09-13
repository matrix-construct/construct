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
#include <ircd/mods/mapi.h>  // Module's internal API
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

///////////////////////////////////////////////////////////////////////////////
//
// mods/mods.h (misc util)
//

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
		log::warning
		{
			log, "Module path [%s]: %s",
			dir,
			e.what()
		};

		continue;
	}

	return ret;
}

filesystem::path
ircd::mods::fullpath(const string_view &name)
{
	std::vector<std::string> why;
	const filesystem::path path(search(name, why));
	if(path.empty())
	{
		for(const auto &str : why)
			log::error
			{
				log, "candidate for module '%s' failed: %s",
				name,
				str
			};

		throw error
		{
			"No valid module by name `%s'", name
		};
	}

	return path;
}

std::string
ircd::mods::search(const string_view &name)
{
	std::vector<std::string> why;
	return search(name, why);
}

std::string
ircd::mods::search(const string_view &name,
                   std::vector<std::string> &why)
{
	using filesystem::path;

	const path path
	{
		postfixed(name)
	};

	if(!path.is_relative())
	{
		why.resize(why.size() + 1);
		return is_module(path, why.back())?
			std::string{name}:
			std::string{};
	}
	else for(const auto &dir : paths)
	{
		why.resize(why.size() + 1);
		if(is_module(dir/path, why.back()))
			return (dir/path).string();
	}

	return {};
}

bool
ircd::mods::is_module(const string_view &fullpath)
{
	return is_module(filesystem::path(std::string{fullpath}));
}

bool
ircd::mods::is_module(const string_view &fullpath,
                      std::nothrow_t)
{
	return is_module(filesystem::path(std::string{fullpath}), std::nothrow);
}

bool
ircd::mods::is_module(const string_view &fullpath,
                      std::string &why)
{
	return is_module(filesystem::path(std::string{fullpath}), why);
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
		throw error
		{
			"`%s': has no MAPI header (%s)", path.string(), header_name
		};

	return true;
}

bool
ircd::mods::available(const string_view &name)
{
	using filesystem::path;

	std::vector<std::string> why;
	return !search(name, why).empty();
}

bool
ircd::mods::loaded(const string_view &name)
{
	return mod::loaded.count(name);
}

///////////////////////////////////////////////////////////////////////////////
//
// mods/sym_ptr.h
//

ircd::mods::sym_ptr::sym_ptr(const string_view &modname,
                             const string_view &symname)
:sym_ptr
{
	module(modname), symname
}
{
}

ircd::mods::sym_ptr::sym_ptr(mod &mod,
                             const string_view &symname)
:sym_ptr
{
	module(shared_from(mod)), symname
}
{
}

ircd::mods::sym_ptr::sym_ptr(module module,
                             const string_view &symname)
:std::weak_ptr<mod>
{
	module
}
,ptr{[this, &module, &symname]
{
	if(unlikely(!module.has(symname)))
		throw undefined_symbol
		{
			"Could not find symbol '%s' (%s) in module '%s'",
			demangle(symname),
			symname,
			module.name()
		};

	return module.ptr(symname);
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// mods/module.h
//

ircd::mods::module::module(const string_view &name)
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
	log::debug
	{
		log, "Attempting to load '%s' @ `%s'",
		name,
		path.string()
	};

	const auto ret
	{
		std::make_shared<mod>(path, flags)
	};

	// Call the user-supplied init function well after fully loading and
	// construction of the module. This way the init function sees the module
	// as loaded and can make shared_ptr references, etc.
	if(ret->header->init)
		ret->header->init();

	log::info
	{
		log, "Loaded module %s v%u \"%s\"",
		ret->name(),
		ret->header->version,
		!ret->description().empty()?
			ret->description():
			"<no description>"_sv
	};

	return ret;
}()}
{
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Failed to load '%s': %s",
		name,
		e.what()
	};
}

ircd::string_view
ircd::mods::module::path()
const
{
	const mod &mod(*this);
	return mods::path(mod);
}

ircd::string_view
ircd::mods::module::name()
const
{
	const mod &mod(*this);
	return mods::name(mod);
}

bool
ircd::mods::module::has(const string_view &name)
const
{
	const mod &mod(*this);
	return mods::has(mod, name);
}

///////////////////////////////////////////////////////////////////////////////
//
// mods/symbols.h
//

std::vector<std::string>
ircd::mods::find_symbol(const string_view &symbol)
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
ircd::mods::has_symbol(const string_view &name,
                       const string_view &symbol)
{
	const auto path(fullpath(name));
	if(path.empty())
		return false;

	const auto syms(symbols(path));
	return std::find(begin(syms), end(syms), symbol) != end(syms);
}

std::unordered_map<std::string, std::string>
ircd::mods::mangles(const string_view &fullpath)
{
	return mangles(filesystem::path(std::string{fullpath}));
}

std::unordered_map<std::string, std::string>
ircd::mods::mangles(const string_view &fullpath,
                    const string_view &section)
{
	return mangles(filesystem::path(std::string{fullpath}), section);
}

std::unordered_map<std::string, std::string>
ircd::mods::mangles(const filesystem::path &path)
{
	return mangles(mods::symbols(path));
}

std::unordered_map<std::string, std::string>
ircd::mods::mangles(const filesystem::path &path,
                    const string_view &section)
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
ircd::mods::symbols(const string_view &fullpath)
{
	return symbols(filesystem::path(std::string{fullpath}));
}

std::vector<std::string>
ircd::mods::symbols(const string_view &fullpath,
                    const string_view &section)
{
	return symbols(filesystem::path(std::string{fullpath}), std::string{section});
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
                    const string_view &section)
{
	return info<std::vector<std::string>>(path, [&section]
	(boost::dll::library_info &info)
	{
		return info.symbols(std::string{section});
	});
}

std::vector<std::string>
ircd::mods::sections(const string_view &fullpath)
{
	return sections(filesystem::path(std::string{fullpath}));
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
		throw filesystem_error
		{
			"`%s' does not exist", path.string()
		};

	if(!is_regular_file(path))
		throw filesystem_error
		{
			"`%s' is not a file", path.string()
		};

	boost::dll::library_info info(path);
	return closure(info);
}

///////////////////////////////////////////////////////////////////////////////
//
// mods/paths.h
//

namespace ircd::mods
{
	extern const filesystem::path modroot;
}

decltype(ircd::mods::modroot)
ircd::mods::modroot
{
	std::string{ircd::fs::get(ircd::fs::MODULES)}
};

decltype(ircd::mods::paths)
ircd::mods::paths
{};

std::string
ircd::mods::unpostfixed(const string_view &name)
{
	return unpostfixed(std::string{name});
}

std::string
ircd::mods::unpostfixed(std::string name)
{
	return unpostfixed(filesystem::path(std::move(name))).string();
}

std::string
ircd::mods::postfixed(const string_view &name)
{
	return postfixed(std::string{name});
}

std::string
ircd::mods::postfixed(std::string name)
{
	return postfixed(filesystem::path(std::move(name))).string();
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

//
// paths::paths
//

ircd::mods::paths::paths()
:std::vector<std::string>
{{
	modroot.string()
}}
{
}

bool
ircd::mods::paths::add(const string_view &dir)
{
	using filesystem::path;

	const path path
	{
		prefix_if_relative(std::string{dir})
	};

	if(!exists(path))
		throw filesystem_error
		{
			"path `%s' (%s) does not exist", dir, path.string()
		};

	if(!is_directory(path))
		throw filesystem_error
		{
			"path `%s' (%s) is not a directory", dir, path.string()
		};

	if(added(dir))
		return false;

	emplace(begin(), dir);
	return true;
}

bool
ircd::mods::paths::add(const string_view &dir,
                       std::nothrow_t)
try
{
	return add(dir);
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Failed to add path: %s", e.what()
	};

	return false;
}

bool
ircd::mods::paths::del(const string_view &dir)
{
	std::remove(begin(), end(), prefix_if_relative(std::string{dir}).string());
	return true;
}

bool
ircd::mods::paths::added(const string_view &dir)
const
{
	return std::find(begin(), end(), dir) != end();
}

///////////////////////////////////////////////////////////////////////////////
//
// mods/mods.h
//

template<> uint8_t *
ircd::mods::ptr<uint8_t>(mod &mod,
                         const string_view &sym)
{
	return &mod.handle.get<uint8_t>(std::string{sym});
}

template<>
const uint8_t *
ircd::mods::ptr<const uint8_t>(const mod &mod,
                               const string_view &sym)
{
	return &mod.handle.get<const uint8_t>(std::string{sym});
}

bool
ircd::mods::has(const mod &mod,
                const string_view &name)
{
	return mod.handle.has(std::string{name});
}

ircd::string_view
ircd::mods::name(const mod &mod)
{
	return mod.name();
}

ircd::string_view
ircd::mods::path(const mod &mod)
{
	return mod.location();
}

///////////////////////////////////////////////////////////////////////////////
//
// (internal) mods.h
//

decltype(ircd::mods::mod::loading)
ircd::mods::mod::loading
{};

decltype(ircd::mods::mod::loaded)
ircd::mods::mod::loaded
{};

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
		log::critical
		{
			log, "std::terminate() called during the static construction of a module."
		};

		if(std::current_exception()) try
		{
			std::rethrow_exception(std::current_exception());
		}
		catch(const std::exception &e)
		{
			log::error
			{
				log, "%s", e.what()
			};
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
	log::debug
	{
		log, "Loaded static segment of '%s' @ `%s' with %zu symbols",
		name(),
		location(),
		mangles.size()
	};

	if(unlikely(!header))
		throw error
		{
			"Unexpected null header"
		};

	if(header->magic != mapi::MAGIC)
		throw error
		{
			"Bad magic [%04x] need: [%04x]", header->magic, mapi::MAGIC
		};

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
		log::debug
		{
			log, "Module '%s' recursively loaded by '%s'",
			name(),
			m->path.filename().string()
		};
	}

	// Without init exception, the module is now considered loaded.
	assert(!loaded.count(name()));
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
			throw error
			{
				"undefined symbol: '%s' (%s)", demangled, mangled
			};
		}

		default:
			break;
	}

	throw error
	{
		"%s", string(e)
	};
}

// Allows module to communicate static destruction is taking place when mapi::header
// destructs. If dlclose() returns without this being set, dlclose() lied about really
// unloading the module. That is considered a "stuck" module.
bool ircd::mapi::static_destruction;

ircd::mods::mod::~mod()
noexcept try
{
	log::debug
	{
		log, "Attempting unload module '%s' @ `%s'",
		name(),
		location()
	};

	unload();

	const size_t erased(loaded.erase(name()));
	assert(erased == 1);
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "Module @%p unload: %s",
		(const void *)this,
		e.what()
	};

	if(!ircd::debugmode)
		return;
}

bool
ircd::mods::mod::unload()
{
	if(!handle.is_loaded())
		return false;

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

	log::debug
	{
		log, "Attempting static unload for '%s' @ `%s'",
		name(),
		location()
	};

	mapi::static_destruction = false;
	handle.unload();
	assert(!handle.is_loaded());

	if(!mapi::static_destruction)
	{
		log.critical("Module \"%s\" is stuck and failing to unload.", name());
		log.critical("Module \"%s\" may result in undefined behavior if not fixed.", name());
	}
	else log::info
	{
		log, "Unloaded '%s'", name()
	};

	return true;
}
