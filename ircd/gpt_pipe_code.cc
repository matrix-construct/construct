// Tensor Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::gpt::pipe::code::default_path)
ircd::gpt::pipe::code::default_path
{
	{ "name", "ircd.gpt.pipe.code.path" },
};

decltype(ircd::gpt::pipe::code::cache_path)
ircd::gpt::pipe::code::cache_path
{
	{ "name", "ircd.gpt.pipe.code.cache.path" },
};

decltype(ircd::gpt::pipe::code::default_compile_opts)
ircd::gpt::pipe::code::default_compile_opts
{
	{ "name",     "ircd.gpt.pipe.code.opts.comp" },
	{ "default",  string_view
	{
		//" -fident"
		//" -fno-builtin"
		//" -ffast-math"
		//" -O3"

		" -cl-no-signed-zeros"
		" -cl-finite-math-only"
		" -cl-fp32-correctly-rounded-divide-sqrt"
		" -cl-single-precision-constant"

		" -cl-kernel-arg-info"
	}}
};

decltype(ircd::gpt::pipe::code::default_link_opts)
ircd::gpt::pipe::code::default_link_opts
{
	{ "name",     "ircd.gpt.pipe.code.opts.link" },
	{ "default",  string_view
	{
		//" -fident"
		//" -fno-builtin"
		//" -ffast-math"
		//" -O3"

		//" -cl-no-signed-zeros"
		//" -cl-finite-math-only"
		//" -cl-fp32-correctly-rounded-divide-sqrt"
		//" -cl-single-precision-constant"
	}}
};

//
// code::code
//

ircd::gpt::pipe::code::code()
:cl::code{[]() -> cl::code
{
	const auto comp_opts
	{
		fmt::snstringf
		{
			4096, "%s -I%s",
			string_view{default_compile_opts},
			string_view{fs::base::include},
		}
	};

	const string_view link_opts
	{
		default_link_opts
	};

	cl::code ret;

	if(!ret)
		ret = from_cache();

	if(!ret)
		ret = from_source(comp_opts, link_opts);

	if(!ret)
		ret = from_bitcode(link_opts);

	return ret;
}()}
{
	put_cache();
}

ircd::gpt::pipe::code::~code()
noexcept
{
}

bool
ircd::gpt::pipe::code::put_cache()
{
	const auto cache_path
	{
		make_cache_path(fs::path_scratch)
	};

	if(!cache_path)
		return false;

	if(fs::is_reg(cache_path))
		return false;

	set_cache(cache_path);
	return true;
}

void
ircd::gpt::pipe::code::set_cache(const string_view &path)
{
	const unique_mutable_buffer cache_buf
	{
		this->bins_size()
	};

	mutable_buffer _cache_bufs[1]
	{
		cache_buf
	};

	const auto cache_bufs
	{
		this->bin(_cache_bufs)
	};

	const fs::fd fd
	{
		path, fs::fd::opts
		{
			.mode = std::ios::out
		},
	};

	const auto written
	{
		fs::write(fd, cache_bufs[0])
	};

	assert(!empty(written));
}

extern const uint8_t
gpt_gpu_r600_barts_bc[],
gpt_gpu_spv[];

extern const uint
gpt_gpu_r600_barts_bc_len,
gpt_gpu_spv_len;

ircd::cl::code
ircd::gpt::pipe::code::from_bitcode(const string_view &link_opts)
{
	const const_buffer bitcode
	{
		reinterpret_cast<const char *>(gpt_gpu_r600_barts_bc),
		gpt_gpu_r600_barts_bc_len

		//reinterpret_cast<const char *>(gpt_gpu_spv),
		//gpt_gpu_spv_len
	};

	char pbuf[1][48];
	log::logf
	{
		log, log::level::DEBUG,
		"bitcode %p %s link_opts:%zu attempting...",
		data(bitcode),
		pretty(pbuf[0], si(size(bitcode))),
		size(link_opts),
	};

	cl::code ret
	{
		bitcode
	};

	ret.link(link_opts);
	return ret;
}

ircd::cl::code
ircd::gpt::pipe::code::from_source(const string_view &comp_opts,
                                   const string_view &link_opts)
{
	const string_view code_path
	{
		default_path
	};

	if(!code_path)
		return {};

	log::logf
	{
		log, log::level::DEBUG,
		"source code `%s'comp_opts:%zu link_opts:%zu attempting...",
		code_path,
		size(comp_opts),
		size(link_opts),
	};

	cl::code ret
	{
		cl::code::path, code_path
	};

	ret.compile(comp_opts);
	ret.link(link_opts);
	return ret;
}

ircd::cl::code
ircd::gpt::pipe::code::from_cache()
{
	const auto cache_path
	{
		make_cache_path(fs::path_scratch)
	};

	if(!cache_path)
		return {};

	if(!fs::is_reg(cache_path))
		return {};

	const fs::fd fd
	{
		cache_path
	};

	const std::string read
	{
		fs::read(fd)
	};

	const const_buffer bin
	{
		read
	};

	const vector_view<const const_buffer> bins
	(
		&bin, 1
	);

	char pbuf[1][48];
	log::logf
	{
		log, log::level::DEBUG,
		"cached nir `%s' %s attempting...",
		cache_path,
		pretty(pbuf[0], si(size(read))),
	};

	return cl::code
	{
		bins
	};
}

ircd::string_view
ircd::gpt::pipe::code::make_cache_path(const mutable_buffer &buf)
{
	if(!cache_path)
		return {};

	const string_view &src_path
	{
		default_path
	};

	const string_view &src_fname
	{
		fs::filename(fs::path_scratch, src_path)
	};

	const string_view &cache_fname
	{
		fs::extension(fs::name_scratch, src_fname, ".r600_barts.bc")
	};

	const string_view parts[]
	{
		string_view{cache_path},
		string_view{cache_fname}
	};

	return fs::path(buf, fs::path_views{parts});
}
