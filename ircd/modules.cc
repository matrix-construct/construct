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
#include "mods_mod.h"   // struct mod

namespace ircd {
namespace mods {

struct log::log log
{
	"modules", 'M'
};

const filesystem::path modroot
{
	path::get(path::MODULES)
};

std::forward_list<filesystem::path> _paths
{
	modroot
};

//TODO: XXX: This should all be folded away somewhere eventually
std::map<std::type_index, struct type_handlers> type_handlers
{{
	// Add a generic init function handler
	make_index<mapi::init>(),
	{
		[](mod &mod, const std::string &name)
		{
			get<mapi::init>(mod, name)();
		}
	}
},
{
	// Add a generic fini function handler
	make_index<mapi::fini>(),
	{
		nullptr,
		[](mod &mod, const std::string &name)
		{
			get<mapi::fini>(mod, name)();
		}
	}
}};

std::map<std::string, std::unique_ptr<mod>> mods;

filesystem::path prefix_if_relative(const filesystem::path &path);
filesystem::path postfixed(const filesystem::path &path);
std::string postfixed(const std::string &name);

template<class R, class F> R info(const filesystem::path &, F&& closure);
std::vector<std::string> sections(const filesystem::path &path);
std::vector<std::string> symbols(const filesystem::path &path);
std::vector<std::string> symbols(const filesystem::path &path, const std::string &section);

bool is_module(const filesystem::path &);
bool is_module(const filesystem::path &, std::string &why);
bool is_module(const filesystem::path &, std::nothrow_t);

// Associates exported RTTI at object location to a symbol name
using reflections = std::map<const void *, std::string>;
using associations = std::vector<std::pair<std::string, std::type_index>>;
reflections reflection(const mod &, const std::vector<std::string> &syms);
associations association(const mod &, const reflections &);

void unload_symbol(mod &mod, const std::string &name, const std::type_index &);
void load_symbol(mod &mod, const std::string &name, const std::type_index &);

} // namespace mods
} // namespace ircd

ircd::mods::init::init()
{
}

ircd::mods::init::~init()
noexcept
{
	unload();
}

void
ircd::mods::unload()
{
	log.info("Unloading %zu (all) modules...", mods.size());

	// Proper way to unload is by name; don't just clear the map.
	std::vector<std::string> names(mods.size());
	std::transform(begin(mods), end(mods), begin(names), []
	(const auto &pit)
	{
		return pit.first;
	});

	for(const auto &name : names)
		unload(name);

	log.info("Unloaded all modules.");
}

void
ircd::mods::autoload()
{
	for(const auto &name : available()) try
	{
		log.debug("Autoload module '%s'", name.c_str());
		load(name);
	}
	catch(const std::exception &e)
	{
		log.warning("Could not autoload '%s'", name.c_str());
	}
}

bool
ircd::mods::load(const std::string &name)
try
{
	using filesystem::path;

	std::vector<std::string> why;
	const path fullpath(search(name, why));
	if(fullpath.empty())
	{
		log.error("Failed to find valid module by name `%s'", name.c_str());
		for(const auto &str : why)
			log.error("candidate failed: %s", str.c_str());

		return false;
	}

	const auto filename(postfixed(name));
	auto it(mods.lower_bound(filename));
	if(it != end(mods) && mods::name(*it->second) == filename)
		throw error("Module '%s' is already loaded", filename.c_str());

	static const load_mode::type flags
	{
		load_mode::rtld_local |
		load_mode::rtld_now
	};

	auto mod(std::make_unique<struct mod>(fullpath, flags));
	log.info("Opened module '%s' @ `%s' version: %u",
	         mods::name(*mod).c_str(),
	         fullpath.string().c_str(),
	         version(*mod));

	const auto refs(reflection(*mod, symbols(location(*mod))));
	for(const auto &assoc : association(*mod, refs))
		load_symbol(*mod, assoc.first, assoc.second);

	const auto nombre(mods::name(*mod));
	const auto iit(mods.emplace_hint(it, nombre, std::move(mod)));
	{
		const auto &mod(iit->second);
		const auto &desc(mods::desc(*mod));
		log.info("Loaded module %s \"%s\"",
		         mods::name(*mod).c_str(),
		         desc.size()? desc.c_str() : "<no description>");
	}
	return true;
}
catch(const std::exception &e)
{
	log.error("Failed to load '%s': %s", name.c_str(), e.what());
	throw;
}

// Allows module to communicate static destruction is taking place when mapi::header
// destructs. This allows us to gauge if the module *really* unloaded on command. DSO's
// are allowed to silently refuse to unload for any reason. We prefer developers to do
// things that don't trigger such behavior and allow a clean unload.
bool ircd::mods::static_destruction;

bool
ircd::mods::unload(const std::string name)
{
	const auto filename(postfixed(name));
	const auto it(mods.find(filename));
	if(it == end(mods))
		return false;

	auto &mod(*it->second);
	for(const auto &pit : mod.handled)
	{
		const auto &name(pit.first);
		const auto &sym(pit.second);
		unload_symbol(mod, name, sym.type);
	}

	static_destruction = false;
	mods.erase(it);

	if(!static_destruction)
	{
		log.error("Module \"%s\" is stuck and failing to unload.", name.c_str());
		log.warning("Module \"%s\" may result in undefined behavior if not fixed.", name.c_str());
	} else {
		log.info("Module '%s' unloaded", filename.c_str());
	}

	return true;
}

bool
ircd::mods::reload(const std::string name)
{

	return true;
}

void
ircd::mods::load_symbol(mod &mod,
                        const std::string &name,
                        const std::type_index &type)
try
{
	const auto &handlers(type_handlers.at(type));
	log.debug("Found loader[%s] unloader[%s] reloader[%s] for \"%s\" in %s (type: %s)",
	          handlers.loader?   "yes" : "no",
	          handlers.unloader? "yes" : "no",
	          handlers.reloader? "yes" : "no",
	          name.c_str(),
	          mods::name(mod).c_str(),
	          type.name());

	if(handlers.loader)
		handlers.loader(mod, name);

	mod.handled.emplace(name, type);
}
catch(const std::out_of_range &e)
{
	if(~flags(mod) & mapi::RELAXED_INIT)
		throw invalid_export("symbol '%s' is an unhandled type named '%s'",
		                     name.c_str(),
		                     type.name());

	mod.unhandled.emplace(type, name);
	log.notice("Symbol \"%s\" in %s is loading in an unhandled state (type: %s)",
	           name.c_str(),
	           mods::name(mod).c_str(),
	           type.name());
}

void
ircd::mods::unload_symbol(mod &mod,
                          const std::string &name,
                          const std::type_index &type)
try
{
	const auto &handlers(type_handlers.at(type));
	if(!handlers.unloader)
		return;

	log.debug("Executing unloader for \"%s\" in %s (type: %s)",
	          name.c_str(),
	          mods::name(mod).c_str(),
	          type.name());

	handlers.unloader(mod, name);
}
catch(const std::out_of_range &e)
{
	throw invalid_export("symbol '%s' type handler was deleted prematurely (type named '%s')",
	                     name.c_str(),
	                     type.name());
}

ircd::mods::mod &
ircd::mods::get(const std::string &name)
try
{
	return *mods.at(postfixed(name));
}
catch(const std::out_of_range &e)
{
	throw error("module '%s' is not loaded", name.c_str());
}

bool
ircd::mods::loaded(const std::string &name)
{
	return mods.count(postfixed(name));
}

const decltype(ircd::mods::mods) &
ircd::mods::loaded()
{
	return mods;
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
	else for(const auto &dir : _paths)
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
	for(const auto &dir : _paths) try
	{
		for(directory_iterator it(dir); it != directory_iterator(); ++it)
			if(is_module(it->path(), std::nothrow))
				ret.emplace_front(relative(it->path(), dir).string());
	}
	catch(const filesystem::filesystem_error &e)
	{
		log.warning("Module path [%s]: %s",
		            dir.string().c_str(),
		            e.what());
		continue;
	}

	return ret;
}

ircd::mods::associations
ircd::mods::association(const mod &mod,
                        const reflections &refs)
{
	ircd::mods::associations ret;
	const auto &exp(exports(mod));
	std::for_each(begin(exports(mod)), end(exports(mod)), [&]
	(const auto &pit)
	{
		const auto &ptr(pit.first);
		const auto &type(pit.second);
		const auto it(refs.find(ptr));
		if(unlikely(it == end(refs)))
			throw invalid_export("Failed to associate type (%s) @ %p with a symbol name",
			                     type.name(),
			                     ptr);

		const auto &name(it->second);
		ret.emplace_back(name, type);
	});

	return ret;
}

ircd::mods::reflections
ircd::mods::reflection(const mod &mod,
                       const std::vector<std::string> &syms)
{
	ircd::mods::reflections ret;
	std::for_each(begin(syms), end(syms), [&mod, &ret]
	(const auto &name)
	{
		ret.emplace(mod.ptr(name), name);
	});

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
		throw filesystem_error("`%s' does not exist",
		                       path.string().c_str());

	if(!is_regular_file(path))
		throw filesystem_error("`%s' is not a file",
		                       path.string().c_str());

	const auto syms(symbols(path));
	const auto &header_name(mapi::header::sym_name);
	const auto it(std::find(begin(syms), end(syms), header_name));
	if(it == end(syms))
		throw error("`%s': has no MAPI header (%s)",
		            path.string().c_str(),
		            header_name);
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
		throw filesystem_error("`%s' does not exist",
		                       path.string().c_str());

	if(!is_regular_file(path))
		throw filesystem_error("`%s' is not a file",
		                       path.string().c_str());

	boost::dll::library_info info(path);
	return closure(info);
}

void
ircd::mods::path_clear()
{
	_paths.clear();
}

bool
ircd::mods::path_add(const std::string &dir,
                     std::nothrow_t)
try
{
	return path_add(dir);
}
catch(const std::exception &e)
{
	log.error("Failed to add path: %s", e.what());
	return false;
}

bool
ircd::mods::path_add(const std::string &dir)
{
	using filesystem::path;

	const path path(prefix_if_relative(dir));

	if(!exists(path))
		throw filesystem_error("path `%s' (%s) does not exist",
		                       dir.c_str(),
		                       path.string().c_str());

	if(!is_directory(path))
		throw filesystem_error("path `%s' (%s) is not a directory",
		                       dir.c_str(),
		                       path.string().c_str());

	if(std::find(begin(_paths), end(_paths), path) != end(_paths))
		return false;

	_paths.emplace_front(path);
	return true;
}

void
ircd::mods::path_del(const std::string &dir)
{
	using filesystem::path;

	_paths.remove(prefix_if_relative(dir));
}

bool
ircd::mods::path_added(const std::string &dir)
{
	return std::find(begin(_paths), end(_paths), dir) != end(_paths);
}

std::vector<std::string>
ircd::mods::paths()
{
	using filesystem::path;

	const auto num_paths(std::distance(begin(_paths), end(_paths)));
	std::vector<std::string> ret(num_paths);
	std::transform(begin(_paths), end(_paths), begin(ret), []
	(const path &path)
	{
		return path.string();
	});

	return ret;
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

bool
ircd::mods::has(const std::type_index &type)
{
	return type_handlers.count(type);
}

bool
ircd::mods::del(const std::type_index &type)
{
	if(!type_handlers.erase(type))
	{
		log.warning("Failed to remove non-existent handler for type '%s'.", type.name());
		return false;
	}

	log.debug("Removed handler for type '%s'.", type.name());
	return true;
}

bool
ircd::mods::add(const std::type_index &type,
                const struct type_handlers &handlers)
{
	if(!handlers.loader)
		log.warning("Handler for type '%s' has no loader function", type.name());

	const auto iit(type_handlers.emplace(type, handlers));
	if(!iit.second)
	{
		log.warning("Handler for type '%s' already exists", type.name());
		return false;
	}

	log.debug("Added handler for type '%s'", type.name());

	// Go through each module's unhandled list and load the symbol now
	for(const auto &pit : mods)
	{
		auto &mod(*pit.second);
		const auto ppit(mod.unhandled.equal_range(type));
		for(auto it(ppit.first); it != ppit.second; ++it)
		{
			const auto &symbol(it->second);
			load_symbol(mod, symbol, type);
		}

		if(ppit.first != ppit.second)
			mod.unhandled.erase(ppit.first, ppit.second);
	}

	return true;
}

/*{
	using boost::filesystem::path;
	using boost::filesystem::directory_iterator;

	const path modpath(path::get(path::MODULES));
	if(!exists(modpath) || !is_directory(modpath))
	{
		ilog(L_MAIN, "Could not load modules from %s", modpath.string().c_str());
		return;
	}

	for(directory_iterator it(modpath); it != directory_iterator(); ++it)
	{
		const auto &path(it->path());
		if(!is_regular_file(path))
			continue;

		const auto &filename(path.filename());
		load_one_module(filename.string(), 0, 0);
		break;
	}
}
catch(const std::exception &e)
{
	ilog(L_MAIN, "Loading all modules aborted: %s", e.what());
	throw;
}

bool
load_one_module(const std::string &name, int origin, bool coremodule)
try
{
	using boost::filesystem::path;

	inotice("loading module %s ...", name.c_str());

	if(coremodule)
		origin = MAPI_ORIGIN_CORE;

	const path dirpath(path::get(path::MODULES));
	if(!is_directory(dirpath))
		throw error("%s is not a valid directory containing modules.", dirpath.string().c_str());

	const path modpath(dirpath / name);
	if(!is_regular_file(modpath))
		throw error("%s is not a regular file (and cannot be a module).", name.c_str());

	boost::dll::library_info info(modpath);
	for(const auto &sect : info.sections())
		std::cout << "[sect]: " << sect << std::endl;

	for(const auto &sym : info.symbols())
		std::cout << "[symb]: " << sym << std::endl;

	boost::dll::shared_library lib(modpath);
	auto &mheader(lib.get<mapi_mheader_av2>("_mheader"));
	printf("ver: %d\n", MAPI_VERSION(mheader.mapi_version));

	//return load_a_module(modpath.string(), false, origin, false);
	return false;
}
catch(const boost::filesystem::filesystem_error &e)
{
	throw error("%s", e.what());
}


// ******************************************************************************
// INTERNAL API STACK
// driven by load_a_module() / unload_one_module() (bottom)

static
bool init_module_v1(struct module *const mod)
{
	struct mapi_mheader_av1 *mheader = (struct mapi_mheader_av1 *)mod->mapi_header;
	if(mheader->mapi_register && (mheader->mapi_register() == -1))
		return false;

	if(mheader->mapi_command_list)
	{
		struct Message **m;
		for (m = mheader->mapi_command_list; *m; ++m)
			mod_add_cmd(*m);
	}

	if(mheader->mapi_hook_list)
	{
		mapi_hlist_av1 *m;
		for (m = mheader->mapi_hook_list; m->hapi_name; ++m)
			*m->hapi_id = register_hook(m->hapi_name);
	}

	if(mheader->mapi_hfn_list)
	{
		mapi_hfn_list_av1 *m;
		for (m = mheader->mapi_hfn_list; m->hapi_name; ++m)
			add_hook(m->hapi_name, m->fn);
	}

	mod->version = mheader->mapi_module_version;
	return true;
}


static
void fini_module_v1(struct module *const mod)
{
	struct mapi_mheader_av1 *mheader = (mapi_mheader_av1 *)mod->mapi_header;
	if(mheader->mapi_command_list)
	{
		struct Message **m;
		for (m = mheader->mapi_command_list; *m; ++m)
			mod_del_cmd(*m);
	}

	// hook events are never removed, we simply lose the
	// ability to call them --fl
	if(mheader->mapi_hfn_list)
	{
		mapi_hfn_list_av1 *m;
		for (m = mheader->mapi_hfn_list; m->hapi_name; ++m)
			remove_hook(m->hapi_name, m->fn);
	}

	if(mheader->mapi_unregister)
		mheader->mapi_unregister();
}


static
bool init_module__cap(struct module *const mod,
                      mapi_cap_list_av2 *const m)
{
	using capability::index;

	index *idx;
	switch(m->cap_index)
	{
		case MAPI_CAP_CLIENT:  idx = &cli_capindex;    break;
		case MAPI_CAP_SERVER:  idx = &serv_capindex;   break;
		default:
			slog(L_MAIN, sno::GENERAL,
			     "Unknown/unsupported CAP index found of type %d on capability %s when loading %s",
			     m->cap_index,
			     m->cap_name,
			     mod->name);
			return false;
	}

	if(m->cap_id)
	{
		*(m->cap_id) = idx->put(m->cap_name, m->cap_ownerdata);
		sendto_local_clients_with_capability(CLICAP_CAP_NOTIFY, ":%s CAP * ADD :%s", me.name, m->cap_name);
	}

	return true;
}


static
void fini_module__cap(struct module *const mod,
                      mapi_cap_list_av2 *const m)
{
	using capability::index;

	index *idx;
	switch(m->cap_index)
	{
		case MAPI_CAP_CLIENT:  idx = &cli_capindex;    break;
		case MAPI_CAP_SERVER:  idx = &serv_capindex;   break;
		default:
			slog(L_MAIN, sno::GENERAL,
			     "Unknown/unsupported CAP index found of type %d on capability %s when unloading %s",
			     m->cap_index,
			     m->cap_name,
			     mod->name);
			return;
	}

	if(m->cap_id)
	{
		idx->orphan(m->cap_name);
		sendto_local_clients_with_capability(CLICAP_CAP_NOTIFY, ":%s CAP * DEL :%s", me.name, m->cap_name);
	}
}


static
bool init_module_v2(struct module *const mod)
{
	struct mapi_mheader_av2 *mheader = (struct mapi_mheader_av2 *)mod->mapi_header;
	if(mheader->mapi_register && (mheader->mapi_register() == -1))
		return false;

	// Basic date code checks
	//
	// Don't make them fatal, but do complain about differences within a certain time frame.
	// Later on if there are major API changes we can add fatal checks.
	// -- Elizafox
	if(mheader->mapi_datecode != info::configured_time && mheader->mapi_datecode > 0)
	{
		long int delta = info::configured_time - mheader->mapi_datecode;
		if (delta > MOD_WARN_DELTA)
		{
			delta /= 86400;
			iwarn("Module %s build date is out of sync with ircd build date by %ld days, expect problems",
				mod->name, delta);
			sendto_realops_snomask(sno::GENERAL, L_ALL,
				"Module %s build date is out of sync with ircd build date by %ld days, expect problems",
				mod->name, delta);
		}
	}

	if(mheader->mapi_command_list)
	{
		struct Message **m;
		for (m = mheader->mapi_command_list; *m; ++m)
			mod_add_cmd(*m);
	}

	if(mheader->mapi_hook_list)
	{
		mapi_hlist_av1 *m;
		for (m = mheader->mapi_hook_list; m->hapi_name; ++m)
			*m->hapi_id = register_hook(m->hapi_name);
	}

	if(mheader->mapi_hfn_list)
	{
		mapi_hfn_list_av1 *m;
		for (m = mheader->mapi_hfn_list; m->hapi_name; ++m)
			add_hook(m->hapi_name, m->fn);
	}

	// New in MAPI v2 - version replacement
	mod->version = mheader->mapi_module_version? mheader->mapi_module_version : info::ircd_version;
	mod->description = mheader->mapi_module_description;

	if(mheader->mapi_cap_list)
	{
		mapi_cap_list_av2 *m;
		for (m = mheader->mapi_cap_list; m->cap_name; ++m)
			if(!init_module__cap(mod, m))
				return false;
	}

	return true;
}


static
void fini_module_v2(struct module *const mod)
{
	struct mapi_mheader_av2 *mheader = (struct mapi_mheader_av2 *)mod->mapi_header;

	if(mheader->mapi_command_list)
	{
		struct Message **m;
		for (m = mheader->mapi_command_list; *m; ++m)
			mod_del_cmd(*m);
	}

	// hook events are never removed, we simply lose the
	// ability to call them --fl
	if(mheader->mapi_hfn_list)
	{
		mapi_hfn_list_av1 *m;
		for (m = mheader->mapi_hfn_list; m->hapi_name; ++m)
			remove_hook(m->hapi_name, m->fn);
	}

	if(mheader->mapi_unregister)
		mheader->mapi_unregister();

	if(mheader->mapi_cap_list)
	{
		mapi_cap_list_av2 *m;
		for (m = mheader->mapi_cap_list; m->cap_name; ++m)
			fini_module__cap(mod, m);
	}
}


static
bool require_value(struct module *const mod,
                   struct mapi_av3_attr *const attr)
{
	if(!attr->value)
	{
		module_log(mod, "key[%s] requires non-null value", attr->key);
		return false;
	}

	return true;
}

static
bool init_v3_module_attr(struct module *const mod,
                         struct mapi_mheader_av3 *const h,
                         struct mapi_av3_attr *const attr)
{
	if(EmptyString(attr->key))
	{
		module_log(mod, "Skipping a NULL or empty key (ignoring)");
		return true;
	}

	if(strncmp(attr->key, "time", MAPI_V3_KEY_MAXLEN) == 0)
	{
		//TODO: elizafox's warning
		return true;
	}

	if(strncmp(attr->key, "name", MAPI_V3_KEY_MAXLEN) == 0)
	{
		module_log(mod, "Changing the display unsupported (ignoring)");
		return true;
	}

	if(strncmp(attr->key, "mtab", MAPI_V3_KEY_MAXLEN) == 0)
	{
		if(!require_value(mod, attr))
			return false;

		struct Message *const v = (Message *)attr->value;
		mod_add_cmd(v);
		return true;
	}

	if(strncmp(attr->key, "hook", MAPI_V3_KEY_MAXLEN) == 0)
	{
		if(!require_value(mod, attr))
			return false;

		mapi_hlist_av1 *const v = (mapi_hlist_av1 *)attr->value;
		*v->hapi_id = register_hook(v->hapi_name);
		return true;
	}

	if(strncmp(attr->key, "hookfn", MAPI_V3_KEY_MAXLEN) == 0)
	{
		if(!require_value(mod, attr))
			return false;

		mapi_hfn_list_av1 *const v = (mapi_hfn_list_av1 *)attr->value;
		add_hook(v->hapi_name, v->fn);
		return true;
	}

	if(strncmp(attr->key, "cap", MAPI_V3_KEY_MAXLEN) == 0)
	{
		if(!require_value(mod, attr))
			return false;

		mapi_cap_list_av2 *const v = (mapi_cap_list_av2 *)attr->value;
		return init_module__cap(mod, v);
	}

	if(strncmp(attr->key, "description", MAPI_V3_KEY_MAXLEN) == 0)
	{
		mod->description = (const char *)attr->value;
		return true;
	}

	if(strncmp(attr->key, "version", MAPI_V3_KEY_MAXLEN) == 0)
	{
		mod->version = (const char *)attr->value;
		return true;
	}

	if(strncmp(attr->key, "init", MAPI_V3_KEY_MAXLEN) == 0)
		return attr->init();

	// Ignore fini on load
	if(strncmp(attr->key, "fini", MAPI_V3_KEY_MAXLEN) == 0)
		return true;

	// TODO: analysis.
	module_log(mod, "Unknown key [%s]. Host version does not yet support unknown keys.", attr->key);
	return false;
}


static
void fini_v3_module_attr(struct module *const mod,
                         struct mapi_mheader_av3 *const h,
                         struct mapi_av3_attr *const attr)
{
	if(EmptyString(attr->key))
	{
		module_log(mod, "Skipping a NULL or empty key (ignoring)");
		return;
	}

	if(strncmp(attr->key, "mtab", MAPI_V3_KEY_MAXLEN) == 0)
	{
		if(attr->value)
			mod_del_cmd((Message *)attr->value);

		return;
	}

	if(strncmp(attr->key, "hook", MAPI_V3_KEY_MAXLEN) == 0)
	{
		// ???
		return;
	}

	if(strncmp(attr->key, "hookfn", MAPI_V3_KEY_MAXLEN) == 0)
	{
		if(!attr->value)
			return;

		mapi_hfn_list_av1 *const v = (mapi_hfn_list_av1 *)attr->value;
		remove_hook(v->hapi_name, v->fn);
		return;
	}

	if(strncmp(attr->key, "cap", MAPI_V3_KEY_MAXLEN) == 0)
	{
		if(attr->value)
			fini_module__cap(mod, (mapi_cap_list_av2 *)attr->value);

		return;
	}

	if(strncmp(attr->key, "fini", MAPI_V3_KEY_MAXLEN) == 0)
	{
		if(attr->value)
			attr->fini();

		return;
	}
}


static
void fini_module_v3(struct module *const mod)
{
	struct mapi_mheader_av3 *const h = (struct mapi_mheader_av3 *)mod->mapi_header;
	if(!h->attrs)
	{
		module_log(mod, "(unload) has no attribute vector!");
		return;
	}

	ssize_t i = -1;
	struct mapi_av3_attr *attr;
	for(attr = h->attrs[++i]; attr; attr = h->attrs[++i]);
	for(attr = h->attrs[--i]; i > -1; attr = h->attrs[--i])
		fini_v3_module_attr(mod, h, attr);
}


static
bool init_module_v3(struct module *const mod)
{
	struct mapi_mheader_av3 *const h = (struct mapi_mheader_av3 *)mod->mapi_header;
	if(!h->attrs)
	{
		module_log(mod, "has no attribute vector!");
		return false;
	}

	size_t i = 0;
	for(struct mapi_av3_attr *attr = h->attrs[i]; attr; attr = h->attrs[++i])
	{
		if(!init_v3_module_attr(mod, h, attr))
		{
			h->attrs[i] = NULL;
			fini_module_v3(mod);
			return false;
		}
	}

	return true;
}


static
bool init_module(struct module *const mod)
{
	mod->mapi_header = lt_dlsym(mod->address, "_mheader");
	if(!mod->mapi_header)
	{
		module_log(mod, "has no MAPI header. (%s)", lt_dlerror());
		return false;
	}

	const int version_magic = *(const int *)mod->mapi_header;
	if(MAPI_MAGIC(version_magic) != MAPI_MAGIC_HDR)
	{
		module_log(mod, "has an invalid header (magic is [%x] mismatches [%x]).",
		           MAPI_MAGIC(version_magic),
		           MAPI_MAGIC_HDR);
		return false;
	}

	mod->mapi_version = MAPI_VERSION(version_magic);
	switch(mod->mapi_version)
	{
		case 1:  return init_module_v1(mod);
		case 2:  return init_module_v2(mod);
		case 3:  return init_module_v3(mod);
		default:
			module_log(mod, "has unknown/unsupported MAPI version %d.", mod->mapi_version);
			return false;
	}
}


static
const char *reflect_origin(const int origin)
{
	switch(origin)
	{
		case MAPI_ORIGIN_EXTENSION:  return "extension";
		case MAPI_ORIGIN_CORE:       return "core";
		default:                     return "unknown";
	}
}


static
void free_module(struct module **ptr)
{
	if(!ptr || !*ptr)
		return;

	struct module *mod = *ptr;
	if(mod->name)
		rb_free(mod->name);

	if(mod->path)
		rb_free(mod->path);

	rb_free(mod);
}


static
void close_handle(lt_dlhandle *handle)
{
	if(handle && *handle)
		lt_dlclose(*handle);
}


bool
load_a_module(const std::string &path, bool warn, int origin, bool core)
{
	char *const name RB_AUTO_PTR = rb_basename(path.c_str());

	// Trim off the ending for the display name if we have to
	char *c;
	if((c = rb_strcasestr(name, LT_MODULE_EXT)) != NULL)
		*c = '\0';

	lt_dlhandle handle RB_UNIQUE_PTR(close_handle) = lt_dlopenext(path.c_str());
	if(handle == NULL)
	{
		slog(L_MAIN, sno::GENERAL, "Error loading module %s: %s", name, lt_dlerror());
		return false;
	}

	struct module *mod RB_UNIQUE_PTR(free_module) = (module *)rb_malloc(sizeof(struct module));
	mod->name = rb_strdup(name);
	mod->path = rb_strdup(path.c_str());
	mod->address = handle;
	mod->origin = origin;
	mod->core = core;

	if(!init_module(mod))
	{
		slog(L_MAIN, sno::GENERAL, "Loading module %s aborted.", name);
		return false;
	}

	if(!mod->version)
		mod->version = "<unknown>";

	if(!mod->description)
		mod->description = "<no description>";

	if(warn)
		slog(L_MAIN, sno::GENERAL,
		     "Module %s [version: %s; MAPI version: %d; origin: %s; description: \"%s\"] loaded from [%s] to %p",
		     name,
		     mod->version,
		     mod->mapi_version,
		     reflect_origin(mod->origin),
		     mod->description,
		     mod->path,
		     (const void *)mod->address);

	// NULL the acquired resources after commitment to list ownership
	rb_dlinkAdd(mod, &mod->node, &module_list);
	mod = NULL;
	handle = NULL;
	return true;
}


bool
unload_one_module(const char *name, bool warn)
{
	struct module *mod;
	if((mod = findmodule_byname(name)) == NULL)
		return false;

	if(mod->core)
		return false;

	// XXX - The type system in C does not allow direct conversion between
	// data and function pointers, but as it happens, most C compilers will
	// safely do this, however it is a theoretical overlow to cast as we
	// must do here.  I have library functions to take care of this, but
	// despite being more "correct" for the C language, this is more
	// practical.  Removing the abuse of the ability to cast ANY pointer
	// to and from an integer value here will break some compilers.
	//          -jmallett
	//
	// Left the comment in but the code isn't here any more         -larne
	switch (mod->mapi_version)
	{
		case 1:  fini_module_v1(mod);  break;
		case 2:  fini_module_v2(mod);  break;
		case 3:  fini_module_v3(mod);  break;
		default:
			slog(L_MAIN, sno::GENERAL,
			     "Unknown/unsupported MAPI version %d when unloading %s!",
			     mod->mapi_version,
			     mod->name);
		break;
	}

	rb_dlinkDelete(&mod->node, &module_list);
	close_handle(&mod->address);

	if(warn)
		slog(L_MAIN, sno::GENERAL, "Module %s unloaded", name);

	// free after the unload message in case *name came from the mod struct.
	free_module(&mod);
	return true;
}


*/
