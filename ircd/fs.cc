// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include <RB_INC_BOOST_FILESYSTEM_HPP
#include <ircd/asio.h>

#ifdef IRCD_USE_AIO
	#include "aio.h"
#endif

namespace ircd::fs
{
	using namespace boost::filesystem;

	enum { NAME, PATH };
	using ent = std::pair<std::string, std::string>;
	extern const std::array<ent, num_of<index>()> paths;
}

/// Non-null when aio is available for use
decltype(ircd::fs::aioctx)
ircd::fs::aioctx
{};

decltype(ircd::fs::paths)
ircd::fs::paths
{{
	{ "prefix",            DPATH         },
	{ "binary dir",        BINPATH       },
	{ "config",            ETCPATH       },
	{ "log",               LOGPATH       },
	{ "libexec dir",       PKGLIBEXECDIR },
	{ "modules",           MODPATH       },
	{ "ircd.conf",         CPATH         },
	{ "ircd binary",       SPATH         },
	{ "db",                DBPATH        },
}};

ircd::fs::init::init()
{
	#ifdef IRCD_USE_AIO
		assert(!aioctx);
		aioctx = new aio{};
	#else
		log::warning
		{
			"No support for asynchronous local filesystem IO..."
		};
	#endif
}

ircd::fs::init::~init()
noexcept
{
	#ifdef IRCD_USE_AIO
		delete aioctx;
		aioctx = nullptr;
	#endif

	assert(!aioctx);
}

///////////////////////////////////////////////////////////////////////////////
//
// fs/read.h
//

namespace ircd::fs
{
	string_view read__std(const string_view &path, const mutable_raw_buffer &, const read_opts &);
	std::string read__std(const string_view &path, const read_opts &);
}

ircd::fs::read_opts
const ircd::fs::read_opts_default
{};

//
// ircd::fs interface linkage
//

std::string
ircd::fs::read(const string_view &path,
               const read_opts &opts)
{
	#ifdef IRCD_USE_AIO
	if(likely(aioctx))
		return read__aio(path, opts);
	#endif

	return read__std(path, opts);
}

ircd::string_view
ircd::fs::read(const string_view &path,
               const mutable_raw_buffer &buf,
               const read_opts &opts)
{
	#ifdef IRCD_USE_AIO
	if(likely(aioctx))
		return read__aio(path, buf, opts);
	#endif

	return read__std(path, buf, opts);
}

//
// std read
//

std::string
ircd::fs::read__std(const string_view &path,
                    const read_opts &opts)
{
	std::ifstream file{std::string{path}};
	std::noskipws(file);
	std::istream_iterator<char> b{file};
	std::istream_iterator<char> e{};
	return std::string{b, e};
}

ircd::string_view
ircd::fs::read__std(const string_view &path,
                    const mutable_raw_buffer &buf,
                    const read_opts &opts)
{
	std::ifstream file{std::string{path}};
	file.exceptions(file.failbit | file.badbit);
	file.seekg(opts.offset, file.beg);
	file.read(reinterpret_cast<char *>(data(buf)), size(buf));
	return
	{
		reinterpret_cast<const char *>(data(buf)), size_t(file.gcount())
	};
}


///////////////////////////////////////////////////////////////////////////////
//
// fs/write.h
//

bool
ircd::fs::write(const std::string &path,
                const const_raw_buffer &buf)
{
	if(fs::exists(path))
		return false;

	return overwrite(path, buf);
}

bool
ircd::fs::overwrite(const string_view &path,
                    const const_raw_buffer &buf)
{
	return overwrite(std::string{path}, buf);
}

bool
ircd::fs::overwrite(const std::string &path,
                    const const_raw_buffer &buf)
{
	std::ofstream file{path, std::ios::trunc};
	file.write(reinterpret_cast<const char *>(data(buf)), size(buf));
	return true;
}

bool
ircd::fs::append(const std::string &path,
                 const const_raw_buffer &buf)
{
	std::ofstream file{path, std::ios::app};
	file.write(reinterpret_cast<const char *>(data(buf)), size(buf));
	return true;
}

void
ircd::fs::chdir(const std::string &path)
try
{
	fs::current_path(path);
}
catch(const fs::filesystem_error &e)
{
	throw filesystem_error("%s", e.what());
}

bool
ircd::fs::mkdir(const std::string &path)
try
{
	return fs::create_directory(path);
}
catch(const fs::filesystem_error &e)
{
	throw filesystem_error("%s", e.what());
}

std::string
ircd::fs::cwd()
try
{
	return fs::current_path().string();
}
catch(const fs::filesystem_error &e)
{
	throw filesystem_error("%s", e.what());
}

std::vector<std::string>
ircd::fs::ls_recursive(const std::string &path)
try
{
	const fs::recursive_directory_iterator end;
	fs::recursive_directory_iterator it(path);

	std::vector<std::string> ret;
	std::transform(it, end, std::back_inserter(ret), []
	(const auto &ent)
	{
		return ent.path().string();
	});

	return ret;
}
catch(const fs::filesystem_error &e)
{
	throw filesystem_error("%s", e.what());
}

std::vector<std::string>
ircd::fs::ls(const std::string &path)
try
{
	static const fs::directory_iterator end;
	fs::directory_iterator it(path);

	std::vector<std::string> ret;
	std::transform(it, end, std::back_inserter(ret), []
	(const auto &ent)
	{
		return ent.path().string();
	});

	return ret;
}
catch(const fs::filesystem_error &e)
{
	throw filesystem_error("%s", e.what());
}

size_t
ircd::fs::size(const string_view &path)
{
	return ircd::fs::size(std::string{path});
}

size_t
ircd::fs::size(const std::string &path)
{
	return fs::file_size(path);
}

bool
ircd::fs::is_reg(const std::string &path)
try
{
	return fs::is_regular_file(path);
}
catch(const fs::filesystem_error &e)
{
	throw filesystem_error("%s", e.what());
}

bool
ircd::fs::is_dir(const std::string &path)
try
{
	return fs::is_directory(path);
}
catch(const fs::filesystem_error &e)
{
	throw filesystem_error("%s", e.what());
}

bool
ircd::fs::exists(const std::string &path)
try
{
	return boost::filesystem::exists(path);
}
catch(const fs::filesystem_error &e)
{
	throw filesystem_error("%s", e.what());
}

std::string
ircd::fs::make_path(const std::initializer_list<std::string> &list)
{
	fs::path ret;
	for(const auto &s : list)
		ret /= fs::path(s);

	return ret.string();
}

const char *
ircd::fs::get(index index)
noexcept try
{
	return std::get<PATH>(paths.at(index)).c_str();
}
catch(const std::out_of_range &e)
{
	return nullptr;
}

const char *
ircd::fs::name(index index)
noexcept try
{
	return std::get<NAME>(paths.at(index)).c_str();
}
catch(const std::out_of_range &e)
{
	return nullptr;
}
