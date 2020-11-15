// The Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_SYS_STAT_H
#include <RB_INC_SYS_STATFS_H
#include <RB_INC_SYS_STATVFS_H
#include <boost/filesystem.hpp>

/// Default maximum path string length (for all filesystems & platforms).
decltype(ircd::fs::NAME_MAX_LEN)
ircd::fs::NAME_MAX_LEN
{
	#ifdef NAME_MAX
		NAME_MAX
	#elif defined(_POSIX_NAME_MAX)
		_POSIX_NAME_MAX
	#else
		255
	#endif
};

/// Default maximum path string length (for all filesystems & platforms).
decltype(ircd::fs::PATH_MAX_LEN)
ircd::fs::PATH_MAX_LEN
{
	#ifdef PATH_MAX
		PATH_MAX
	#elif defined(_POSIX_PATH_MAX)
		_POSIX_PATH_MAX
	#else
		4096
	#endif
};

// Convenience scratch buffers for path making.
namespace ircd::fs
{
	thread_local char _name_scratch[2][NAME_MAX_LEN];
	thread_local char _path_scratch[2][PATH_MAX_LEN];
}

// External mutable_buffer to the scratch
decltype(ircd::fs::path_scratch)
ircd::fs::path_scratch
{
	_path_scratch[0]
};

// External mutable_buffer to the scratch
decltype(ircd::fs::name_scratch)
ircd::fs::name_scratch
{
	_name_scratch[0]
};

/// e.g. / default=RB_PREFIX
/// env=ircd_fs_base_prefix
decltype(ircd::fs::base::prefix)
ircd::fs::base::prefix
{
	{ "name",        "ircd.fs.base.prefix"       },
	{ "persist",     false                       },
	{ "help",        "directory prefix"          },
	{
		"default",
		getenv("IRCD_PREFIX")?
			getenv("IRCD_PREFIX"):
			RB_PREFIX
	},
};

/// e.g. /usr/bin default=RB_BIN_DIR
/// env=ircd_fs_base_bin
decltype(ircd::fs::base::bin)
ircd::fs::base::bin
{
	{ "name",        "ircd.fs.base.bin"          },
	{ "persist",     false                       },
	{ "help",        "binary directory"          },
	{
		"default",
		getenv("IRCD_BIN_DIR")?
			getenv("IRCD_BIN_DIR"):
			RB_BIN_DIR
	},
};

/// e.g. /etc default=RB_CONF_DIR
/// env=$ircd_fs_base_etc env=$CONFIGURATION_DIRECTORY
decltype(ircd::fs::base::etc)
ircd::fs::base::etc
{
	{ "name",        "ircd.fs.base.etc"          },
	{ "persist",     false                       },
	{ "help",        "configuration directory"   },
	{
		"default",
		getenv("CONFIGURATION_DIRECTORY")?
			getenv("CONFIGURATION_DIRECTORY"):
		getenv("IRCD_CONF_DIR")?
			getenv("IRCD_CONF_DIR"):
			RB_CONF_DIR
	},
};

/// e.g. /usr/lib default=RB_LIB_DIR
/// env=$ircd_fs_base_lib
decltype(ircd::fs::base::lib)
ircd::fs::base::lib
{
	{ "name",        "ircd.fs.base.lib"          },
	{ "persist",     false                       },
	{ "help",        "library directory"         },
	{
		"default",
		getenv("IRCD_LIB_DIR")?
			getenv("IRCD_LIB_DIR"):
			RB_LIB_DIR
	},
};

/// e.g. /usr/lib/modules/construct default=RB_MODULE_DIR
/// env=$ircd_fs_base_modules
decltype(ircd::fs::base::modules)
ircd::fs::base::modules
{
	{ "name",        "ircd.fs.base.modules"      },
	{ "persist",     false                       },
	{ "help",        "modules directory"         },
	{
		"default",
		getenv("IRCD_MODULE_DIR")?
			getenv("IRCD_MODULE_DIR"):
			RB_MODULE_DIR
	},
};

/// e.g. /usr/share/construct default=RB_DATA_DIR
/// env=$ircd_fs_base_share
decltype(ircd::fs::base::share)
ircd::fs::base::share
{
	{ "name",        "ircd.fs.base.share"        },
	{ "persist",     false                       },
	{ "help",        "read-only data directory"  },
	{
		"default",
		getenv("IRCD_DATA_DIR")?
			getenv("IRCD_DATA_DIR"):
			RB_DATA_DIR
	},
};

/// e.g. /var/run/construct default=RB_RUN_DIR
/// env=$ircd_fs_base_run env=$RUNTIME_DIRECTORY
decltype(ircd::fs::base::run)
ircd::fs::base::run
{
	{ "name",        "ircd.fs.base.run"             },
	{ "persist",     false                          },
	{ "help",        "runtime directory"            },
	{
		"default",
		getenv("RUNTIME_DIRECTORY")?
			getenv("RUNTIME_DIRECTORY"):
		getenv("IRCD_RUN_DIR")?
			getenv("IRCD_RUN_DIR"):
			RB_RUN_DIR
	},
};

/// e.g. /var/log/construct default=RB_LOG_DIR
/// env=$ircd_fs_base_log env=$LOGS_DIRECTORY
decltype(ircd::fs::base::log)
ircd::fs::base::log
{
	{ "name",        "ircd.fs.base.log"          },
	{ "persist",     false                       },
	{ "help",        "logging directory"         },
	{
		"default",
		getenv("LOGS_DIRECTORY")?
			getenv("LOGS_DIRECTORY"):
		getenv("IRCD_LOG_DIR")?
			getenv("IRCD_LOG_DIR"):
			RB_LOG_DIR
	},
};

/// e.g. /var/db/construct default=RB_DB_DIR
/// env=$ircd_fs_base_db env=$STATE_DIRECTORY
decltype(ircd::fs::base::db)
ircd::fs::base::db
{
	{ "name",        "ircd.fs.base.db"           },
	{ "persist",     false                       },
	{ "help",        "database directory"        },
	{
		"default",
		getenv("STATE_DIRECTORY")?
			getenv("STATE_DIRECTORY"):
		getenv("IRCD_DB_DIR")?
			getenv("IRCD_DB_DIR"):
			RB_DB_DIR
	},
};

//
// tools
//

ircd::string_view
ircd::fs::canonical(const mutable_buffer &buf,
                    const string_view &p)
try
{
	return path(buf, canonical(_path(p)));
}
catch(const filesystem::filesystem_error &e)
{
	throw error
	{
		e, "%s",
		rsplit(e.what(), ':').second,
	};
}

ircd::string_view
ircd::fs::canonical(const mutable_buffer &buf,
                    const string_view &root,
                    const string_view &p)
try
{
	return path(buf, canonical(_path(p), _path(root)));
}
catch(const filesystem::filesystem_error &e)
{
	throw error
	{
		e, "%s",
		rsplit(e.what(), ':').second,
	};
}

ircd::string_view
ircd::fs::relative(const mutable_buffer &buf,
                   const string_view &root,
                   const string_view &p)
try
{
	return path(buf, relative(_path(p), _path(root)));
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

ircd::string_view
ircd::fs::absolute(const mutable_buffer &buf,
                   const string_view &root,
                   const string_view &p)
try
{
	return path(buf, absolute(_path(p), _path(root)));
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

ircd::string_view
ircd::fs::parent(const mutable_buffer &buf,
                 const string_view &p)
try
{
	return path(buf, _path(p).parent_path());
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

ircd::string_view
ircd::fs::filename(const mutable_buffer &buf,
                   const string_view &p)
try
{
	return path(buf, _path(p).filename());
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

ircd::string_view
ircd::fs::extension(const mutable_buffer &buf,
                    const string_view &p)
try
{
	return path(buf, _path(p).extension());
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

ircd::string_view
ircd::fs::extension(const mutable_buffer &buf,
                    const string_view &p,
                    const string_view &replace)
try
{
	return path(buf, _path(p).replace_extension(_path(replace)));
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

bool
ircd::fs::is_relative(const string_view &p)
{
	return _path(p).is_relative();
}

bool
ircd::fs::is_absolute(const string_view &p)
{
	return _path(p).is_absolute();
}

//
// utils
//

std::string
ircd::fs::cwd()
try
{
	const auto &cur
	{
		filesystem::current_path()
	};

	return cur.string();
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

ircd::string_view
ircd::fs::cwd(const mutable_buffer &buf)
try
{
	const auto &cur
	{
		filesystem::current_path()
	};

	return strlcpy(buf, cur.native());
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

#ifdef _PC_PATH_MAX
size_t
ircd::fs::path_max_len(const string_view &path)
{
	return pathconf(path, _PC_PATH_MAX);
}
#else
size_t
ircd::fs::path_max_len(const string_view &path)
{
	return PATH_MAX_LEN;
}
#endif

#ifdef _PC_NAME_MAX
size_t
ircd::fs::name_max_len(const string_view &path)
{
	return pathconf(path, _PC_NAME_MAX);
}
#elif defined(HAVE_SYS_STATFS_H)
size_t
ircd::fs::name_max_len(const string_view &path)
{
	struct statfs f{0};
	syscall(::statfs, path_cstr(path), &f);
	return f.f_namelen;
}
#else
size_t
ircd::fs::name_max_len(const string_view &path)
{
	return NAME_MAX_LEN;
}
#endif

long
ircd::fs::pathconf(const string_view &path,
                   const int &arg)
{
	return syscall(::pathconf, path_cstr(path), arg);
}

//
// fs::path_cstr()
//

namespace ircd::fs
{
	static const size_t _PATH_CSTR_BUFS {4};
	thread_local char _path_cstr[_PATH_CSTR_BUFS][PATH_MAX_LEN];
	thread_local size_t _path_cstr_pos;
}

const char *
ircd::fs::path_cstr(const string_view &s)
{
	const auto pos
	{
		++_path_cstr_pos %= _PATH_CSTR_BUFS
	};

	strlcpy(_path_cstr[pos], s);
	return _path_cstr[pos];
}

//
// fs::path()
//

ircd::string_view
ircd::fs::path(const mutable_buffer &buf,
               const string_view &base,
               const path_views &list)
{
	// If no base is supplied the result is just as unsafe as using the
	// other path() overloads. As a precaution we assume an empty base
	// argument is the result of an attack on the input somehow.
	if(unlikely(!base))
		throw std::system_error
		{
			make_error_code(std::errc::invalid_argument)
		};

	const string_view supplied_path
	{
		path(_path_scratch[1], list)
	};

	// Generate a canonical result into the caller's buffer prefixed by the
	// base path. N.B. if the caller used '../' this result *will* have escaped
	// the base path, and is now an absolute path to somewhere else.
	const string_view ret
	{
		canonical(buf, base, supplied_path)
	};

	const string_view canonical_base
	{
		canonical(_path_scratch[1], base)
	};

	// Given two absolute and fully resolved paths (canonical), if the result
	// is not prefixed by the base it is incontrovertibly not under the base.
	//
	// Alternatively, we could make an effort to force-smash the supplied path
	// onto the base and let other code determine it doesn't exist; however now
	// this should only throw for truly malformed and malicious paths; best to
	// just throw here.
	if(!startswith(ret, canonical_base))
		throw std::system_error
		{
			make_error_code(std::errc::no_such_file_or_directory)
		};

	return ret;
}

ircd::string_view
ircd::fs::path(const mutable_buffer &buf,
               const filesystem::path &path)
{
	return strlcpy(buf, path.c_str());
}

ircd::string_view
ircd::fs::path(const mutable_buffer &buf,
               const path_strings &list)
{
	return strlcpy(buf, _path(list).c_str());
}

ircd::string_view
ircd::fs::path(const mutable_buffer &buf,
               const path_views &list)
{
	return strlcpy(buf, _path(list).c_str());
}

//
// fs::_path()
//

boost::filesystem::path
ircd::fs::_path(const path_strings &list)
try
{
	filesystem::path ret;
	for(const auto &s : list)
		ret /= s;

	return ret.string();
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

boost::filesystem::path
ircd::fs::_path(const path_views &list)
try
{
	filesystem::path ret;
	for(const auto &s : list)
		ret /= _path(s);

	return ret.string();
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

boost::filesystem::path
ircd::fs::_path(const string_view &s)
try
{
	return _path(std::string{s});
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

boost::filesystem::path
ircd::fs::_path(std::string s)
try
{
	return filesystem::path{std::move(s)};
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}
