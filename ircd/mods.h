// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::mods
{
	struct mod;

	struct log::log extern log;
	extern const filesystem::path suffix;

	filesystem::path prefix_if_relative(const filesystem::path &path);
	filesystem::path postfixed(const filesystem::path &path);
	filesystem::path unpostfixed(const filesystem::path &path);

	template<class R, class F> R info(const filesystem::path &, F&& closure);
	std::vector<std::string> sections(const filesystem::path &path);
	std::vector<std::string> symbols(const filesystem::path &path);
	std::vector<std::string> symbols(const filesystem::path &path, const string_view &section);
	std::unordered_map<std::string, std::string> mangles(const std::vector<std::string> &);
	std::unordered_map<std::string, std::string> mangles(const filesystem::path &path);
	std::unordered_map<std::string, std::string> mangles(const filesystem::path &path, const string_view &section);

	// Get the full path of a [valid] available module by name
	filesystem::path fullpath(const string_view &name);

	// Checks if loadable module containing a mapi header (does not verify the magic)
	bool is_module(const filesystem::path &);
	bool is_module(const filesystem::path &, std::string &why);
	bool is_module(const filesystem::path &, std::nothrow_t);
}

/// Internal module representation
struct ircd::mods::mod
:std::enable_shared_from_this<mod>
{
	static std::stack<mod *> loading; // State of current dlopen() recursion.
	static std::map<string_view, mod *, std::less<>> loaded;

	filesystem::path path;
	load_mode::type mode;
	std::deque<mod *> children;
	std::unordered_map<std::string, std::string> mangles;
	boost::dll::shared_library handle;
	const std::string _name;
	const std::string _location;
	mapi::header *header;

	// Metadata
	auto &operator[](const std::string &s) const { return header->meta.operator[](s);              }
	auto &operator[](const std::string &s)       { return header->meta.operator[](s);              }

	auto &name() const                           { return _name;                                   }
	auto &location() const                       { return _location;                               }
	auto &version() const                        { return header->version;                         }
	auto &description() const                    { return (*this)["description"];                  }

	bool unload();

	mod(const filesystem::path &,
	    const load_mode::type & = load_mode::rtld_local | load_mode::rtld_now);

	~mod() noexcept;
};
