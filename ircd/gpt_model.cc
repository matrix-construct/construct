// Tensor Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::gpt::model
{
	using init_func = void (*)(decoder &, const string_view &, const size_t &, const json::array &);
	using init_handler = std::pair<string_view, init_func>;

	static void
	init_f_weight(decoder &, const string_view &, const size_t &, const json::array &),
	init_f_bias(decoder &, const string_view &, const size_t &, const json::array &),
	init_wpe_weight(decoder &, const string_view &, const size_t &, const json::array &),
	init_wte_weight(decoder &, const string_view &, const size_t &, const json::array &),
	init_h_ffnn_fc_weight(decoder &, const string_view &, const size_t &, const json::array &),
	init_h_ffnn_fc_bias(decoder &, const string_view &, const size_t &, const json::array &),
	init_h_ffnn_proj_weight(decoder &, const string_view &, const size_t &, const json::array &),
	init_h_ffnn_proj_bias(decoder &, const string_view &, const size_t &, const json::array &),
	init_h_ln_1_weight(decoder &, const string_view &, const size_t &, const json::array &),
	init_h_ln_1_bias(decoder &, const string_view &, const size_t &, const json::array &),
	init_h_ln_2_weight(decoder &, const string_view &, const size_t &, const json::array &),
	init_h_ln_2_bias(decoder &, const string_view &, const size_t &, const json::array &),
	init_h_attn_attn_weight(decoder &, const string_view &, const size_t &, const json::array &),
	init_h_attn_attn_bias(decoder &, const string_view &, const size_t &, const json::array &),
	init_h_attn_proj_weight(decoder &, const string_view &, const size_t &, const json::array &),
	init_h_attn_proj_bias(decoder &, const string_view &, const size_t &, const json::array &);

	static bool init_dataset(const string_view &);
	static bool init_from_cache(const string_view &);
	static void init_from_json_handle(decoder &, const init_handler &, const size_t &);
	static void init_from_json(const string_view &, const string_view &);
	static void init(conf::item<void> &), fini() noexcept;

	extern const init_handler
	manifest[],
	manifest_h[];

	extern conf::item<bool>
	cache_mapped,
	cache_locked,
	cache_shared,
	cache_hugepage;

	extern conf::item<std::string>
	path,
	cache_path,
	dataset_path;

	static fs::map
	default_model_shm,
	default_dataset_shm;
}

constexpr const char
*const ircd::gpt::model::prop::ended,
*const ircd::gpt::model::prop::id,
*const ircd::gpt::model::prop::length,
*const ircd::gpt::model::prop::text;

decltype(ircd::gpt::model::manifest_h)
ircd::gpt::model::manifest_h
{
	{ "h.%u.mlp.c_fc.weight.json",        init_h_ffnn_fc_weight,    },
	{ "h.%u.mlp.c_fc.bias.json",          init_h_ffnn_fc_bias,      },
	{ "h.%u.mlp.c_proj.weight.json",      init_h_ffnn_proj_weight,  },
	{ "h.%u.mlp.c_proj.bias.json",        init_h_ffnn_proj_bias,    },
	{ "h.%u.ln_1.weight.json",            init_h_ln_1_weight,       },
	{ "h.%u.ln_1.bias.json",              init_h_ln_1_bias,         },
	{ "h.%u.ln_2.weight.json",            init_h_ln_2_weight,       },
	{ "h.%u.ln_2.bias.json",              init_h_ln_2_bias,         },
	{ "h.%u.attn.c_attn.weight.json",     init_h_attn_attn_weight,  },
	{ "h.%u.attn.c_attn.bias.json",       init_h_attn_attn_bias,    },
	{ "h.%u.attn.c_proj.weight.json",     init_h_attn_proj_weight,  },
	{ "h.%u.attn.c_proj.bias.json",       init_h_attn_proj_bias     },
};

decltype(ircd::gpt::model::manifest)
ircd::gpt::model::manifest
{
	{ "ln_f.weight.json",   init_f_weight,  },
	{ "ln_f.bias.json",     init_f_bias,    },
	{ "wpe.weight.json",    init_wpe_weight },
	{ "wte.weight.json",    init_wte_weight },
};

decltype(ircd::gpt::model::cache_mapped)
ircd::gpt::model::cache_mapped
{
	{ "name",     "ircd.gpt.model.cache.mapped" },
	{ "default",  true                          },
};

decltype(ircd::gpt::model::cache_locked)
ircd::gpt::model::cache_locked
{
	{ "name",     "ircd.gpt.model.cache.locked" },
	{ "default",  false                         },
};

decltype(ircd::gpt::model::cache_shared)
ircd::gpt::model::cache_shared
{
	{ "name",     "ircd.gpt.model.cache.shared" },
	{ "default",  false                         },
};

decltype(ircd::gpt::model::cache_hugepage)
ircd::gpt::model::cache_hugepage
{
	{ "name",     "ircd.gpt.model.cache.hugepage" },
	{ "default",  false                           },
};

decltype(ircd::gpt::model::cache_path)
ircd::gpt::model::cache_path
{
	{ "name",     "ircd.gpt.model.cache.path" },
	{ "default",  "model.cache.localhost"     },
};

decltype(ircd::gpt::model::dataset_path)
ircd::gpt::model::dataset_path
{
	{ "name",     "ircd.gpt.model.dataset.path" },
	{ "default",  string_view{}                 },
};

decltype(ircd::gpt::model::path)
ircd::gpt::model::path
{
	{
		{ "name",     "ircd.gpt.model.path" },
		{ "default",  string_view{}         },
	},
	init
};

decltype(ircd::gpt::model::default_model)
ircd::gpt::model::default_model;

decltype(ircd::gpt::model::default_moment)
ircd::gpt::model::default_moment;

decltype(ircd::gpt::model::default_checkpoint)
ircd::gpt::model::default_checkpoint;

decltype(ircd::gpt::model::default_dataset)
ircd::gpt::model::default_dataset;

decltype(ircd::gpt::model::default_data)
ircd::gpt::model::default_data;

void
ircd::gpt::model::init(conf::item<void> &)
{
	if(!model::path)
		return;

	if(model::dataset_path)
		init_dataset(model::dataset_path);

	if(likely(init_from_cache(model::cache_path)))
		return;

	init_from_json(model::cache_path, model::path);
	if(unlikely(!init_from_cache(model::cache_path)))
		throw error
		{
			"Failed to find and/or initialize model."
		};
}

void
ircd::gpt::model::fini()
noexcept
{
	default_checkpoint[2] = nullptr;
	default_checkpoint[1] = nullptr;
	default_checkpoint[0] = nullptr;

	default_moment[1] = nullptr;
	default_moment[0] = nullptr;

	if(!cache_mapped)
		delete default_model;

	default_model = nullptr;
	default_model_shm = {};

	default_dataset = nullptr;
	default_data.clear();
	default_dataset_shm = {};
}

bool
ircd::gpt::model::init_from_cache(const string_view &cache_path)
{
	if(!fs::is_reg(cache_path))
		return false;

	const auto file_size
	{
		fs::size(cache_path)
	};

	const auto decoder_size
	{
		sizeof(model::decoder)
	};

	const bool has_params
	{
		file_size >= decoder_size
	};

	const bool has_moments
	{
		file_size >= decoder_size * 6
	};

	if(unlikely(!has_params))
		throw error
		{
			"Cached model `%s' size %zu insufficient for decoder size %zu.",
			cache_path,
			file_size,
			decoder_size,
		};

	const auto mode
	{
		cache_shared?
			std::ios::in | std::ios::out:
			std::ios::in
	};

	const fs::fd fd
	{
		cache_path, fs::fd::opts
		{
			.mode = mode,
		},
	};

	const bool map_moments
	{
		has_moments || cache_shared
	};

	if(!has_moments && map_moments)
	{
		fs::truncate(fd, decoder_size * 6);
		fs::allocate(fd, decoder_size * 5, decoder_size);
	}

	const auto map_size
	{
		map_moments?
			decoder_size * 6:
			decoder_size
	};

	fs::map::opts map_opts
	{
		.alignment = alignof(model::decoder),
		.shared = bool(cache_shared),
		.locked = bool(cache_locked),
		.huge2mb = bool(cache_hugepage),
	};
	map_opts.mode = mode;

	// amdgpu requires both anon and shms to be read-write even if we
	// open the fd read-only and use read-only cl_mems.
	if(cache_mapped)
		map_opts.mode |= std::ios::out;

	default_model_shm = fs::map
	{
		fd, map_size, map_opts,
	};

	default_model = reinterpret_cast<decoder *>
	(
		cache_mapped?
			data(default_model_shm):
			allocator::allocate(info::page_size, map_size)
	);

	if(map_moments)
	{
		default_moment[0] = reinterpret_cast<float *>(default_model + 1);
		default_moment[1] = reinterpret_cast<float *>(default_model + 2);
		default_checkpoint[0] = reinterpret_cast<float *>(default_model + 3);
		default_checkpoint[1] = reinterpret_cast<float *>(default_model + 4);
		default_checkpoint[2] = reinterpret_cast<float *>(default_model + 5);
	}

	if(cache_mapped)
		fs::prefetch(default_model_shm, sizeof(decoder));

	if(!cache_mapped)
		memcpy(default_model, data(default_model_shm), map_size);

	if(!cache_mapped && !cache_shared)
		default_model_shm = {};

	char pbuf[48];
	log::info
	{
		log, "model(%p) %s cached model `%s' shared:%b params:%b moments:%b align:%u %s",
		default_model,
		cache_mapped?
			"mapped"_sv:
			"loaded"_sv,
		cache_path,
		bool(cache_shared),
		has_params,
		has_moments,
		map_opts.alignment,
		pretty(pbuf, iec(map_size)),
	};

	return true;
}

void
ircd::gpt::model::init_from_json(const string_view &cache_path,
                                 const string_view &model_path)
{
	util::timer stopwatch;

	auto decoder(std::make_unique<model::decoder>());
	memset(decoder.get(), 0x0, sizeof(model::decoder));

	// Load the top level files, vocab etc
	for(size_t i(0); i < 4; ++i)
		init_from_json_handle(*decoder, manifest[i], 0);

	// Load the transformer files by layer
	const size_t layers {12};
	for(size_t i(0); i < layers; ++i)
		for(size_t j(0); j < 12; ++j)
			init_from_json_handle(*decoder, manifest_h[j], i);

	const const_buffer src
	{
		reinterpret_cast<char *>(decoder.get()), sizeof(model::decoder)
	};

	const auto wrote
	{
		fs::write(cache_path, src)
	};

	char pbuf[2][48];
	log::info
	{
		log, "model(%p) parsed `%s' cached %s to `%s' in %s",
		decoder.get(),
		model_path,
		pretty(pbuf[0], iec(size(wrote))),
		cache_path,
		stopwatch.pretty(pbuf[1]),
	};
}

void
ircd::gpt::model::init_from_json_handle(decoder &d,
                                        const init_handler &handler,
                                        const size_t &layer)
{
	const auto &[fmt, func]
	{
		handler
	};

	char namebuf[128];
	const string_view path_part[2]
	{
		model::path, fmt::sprintf
		{
			namebuf, fmt, layer
		}
	};

	const auto path
	{
		fs::path(fs::path_scratch, path_part)
	};

	const fs::fd::opts fd_opts
	{
		.mode = std::ios::in,
		.sequential = true,
	};

	const fs::fd fd
	{
		path, fd_opts
	};

	// mmap of the file
	const fs::map map
	{
		fd, size(fd), fs::map::opts{fd_opts},
	};

	// Each file is a JSON array at the top level.
	const json::array matrix
	{
		map
	};

	// Readable name for logging
	const auto &name
	{
		path_part[1]
	};

	if(likely(func))
		func(d, name, layer, matrix);

	// Check for interrupt after long operation
	ctx::interruption_point();

	log::info
	{
		log, "model(%p) loaded layer:%zu :%s",
		&d,
		layer,
		name,
	};
}

bool
ircd::gpt::model::init_dataset(const string_view &path)
{
	if(!fs::is_reg(path))
		return false;

	const auto size
	{
		fs::size(path)
	};

	const fs::fd::opts fd_opts
	{
		.mode = std::ios::in,
	};

	const fs::fd fd
	{
		path, fd_opts,
	};

	fs::map::opts map_opts{fd_opts};
	map_opts.huge2mb = bool(cache_hugepage);
	default_dataset_shm = fs::map
	{
		fd, size, map_opts
	};

	default_dataset = string_view
	(
		default_dataset_shm
	);

	size_t checkpoint(0);
	default_data.resize(260000); //TODO: XXX
	fs::prefetch(default_dataset_shm, size);
	ircd::tokens(default_dataset, '\n', [&checkpoint]
	(const string_view &line)
	{
		default_data.at(checkpoint++) = line;
	});

	char pbuf[48];
	log::info
	{
		log, "dataset(%p) mapped `%s' %s @%lu",
		data(default_dataset_shm),
		path,
		pretty(pbuf, iec(size)),
		checkpoint,
	};

	fs::evict(default_dataset_shm, size);
	return true;
}

void
ircd::gpt::model::init_wpe_weight(decoder &d,
                                  const string_view &name,
                                  const size_t &layer,
                                  const json::array &mat)
{
	size_t i(0);
	for(const json::array vec : mat)
	{
		size_t j(0);
		for(const auto &elem : vec)
			d.embed.pos[i].elem[j++] = lex_cast<float>(elem);

		always_assert(j == sizeof(d.embed.pos[i]) / sizeof(float));
		++i;
	}
}

void
ircd::gpt::model::init_wte_weight(decoder &d,
                                  const string_view &name,
                                  const size_t &layer,
                                  const json::array &mat)
{
	size_t i(0);
	for(const json::array vec : mat)
	{
		size_t j(0);
		for(const auto &elem : vec)
			d.embed.token[i].elem[j++] = lex_cast<float>(elem);

		always_assert(j == sizeof(d.embed.token[i]) / sizeof(float));
		++i;
	}
}

void
ircd::gpt::model::init_f_weight(decoder &d,
                                const string_view &name,
                                const size_t &layer,
                                const json::array &vec)
{
	size_t i(0);
	for(const auto &elem : vec)
		d.embed.norm.weight.elem[i++] = lex_cast<float>(elem);

	always_assert(i == sizeof(d.embed.norm.weight) / sizeof(float));
}

void
ircd::gpt::model::init_f_bias(decoder &d,
                              const string_view &name,
                              const size_t &layer,
                              const json::array &vec)
{
	size_t i(0);
	for(const auto &elem : vec)
		d.embed.norm.bias.elem[i++] = lex_cast<float>(elem);

	always_assert(i == sizeof(d.embed.norm.bias) / sizeof(float));
}

void
ircd::gpt::model::init_h_ffnn_fc_weight(decoder &d,
                                        const string_view &name,
                                        const size_t &layer,
                                        const json::array &mat)
{
	size_t i(0);
	for(const json::array vec : mat)
	{
		size_t j(0);
		for(const auto &elem : vec)
			d.layer[layer].ffnn.fcon_weight[i].fcon[j++] = lex_cast<float>(elem);

		always_assert(j == sizeof(d.layer[layer].ffnn.fcon_weight[i]) / sizeof(float));
		++i;
	}

	always_assert
	(
		i == sizeof(d.layer[layer].ffnn.fcon_weight)
		/ sizeof(d.layer[layer].ffnn.fcon_weight[0])
	);
}

void
ircd::gpt::model::init_h_ffnn_fc_bias(decoder &d,
                                      const string_view &name,
                                      const size_t &layer,
                                      const json::array &vec)
{
	size_t i(0);
	for(const auto &elem : vec)
		d.layer[layer].ffnn.fcon_bias.fcon[i++] = lex_cast<float>(elem);

	always_assert(i == sizeof(d.layer[layer].ffnn.fcon_bias) / sizeof(float));
}

void
ircd::gpt::model::init_h_ffnn_proj_weight(decoder &d,
                                          const string_view &name,
                                          const size_t &layer,
                                          const json::array &mat)
{
	size_t i(0);
	for(const json::array vec : mat)
	{
		size_t j(0);
		for(const auto &elem : vec)
			d.layer[layer].ffnn.proj_weight[i].elem[j++] = lex_cast<float>(elem);

		always_assert(j == sizeof(d.layer[layer].ffnn.proj_weight[i]) / sizeof(float));
		++i;
	}

	always_assert
	(
		i == sizeof(d.layer[layer].ffnn.proj_weight)
		/ sizeof(d.layer[layer].ffnn.proj_weight[0])
	);
}

void
ircd::gpt::model::init_h_ffnn_proj_bias(decoder &d,
                                        const string_view &name,
                                        const size_t &layer,
                                        const json::array &vec)
{
	size_t i(0);
	for(const auto &elem : vec)
		d.layer[layer].ffnn.proj_bias.elem[i++] = lex_cast<float>(elem);

	always_assert(i == sizeof(d.layer[layer].ffnn.proj_bias) / sizeof(float));
}

void
ircd::gpt::model::init_h_ln_1_weight(decoder &d,
                                     const string_view &name,
                                     const size_t &layer,
                                     const json::array &vec)
{
	size_t i(0);
	for(const auto &elem : vec)
		d.layer[layer].attn.norm.weight.elem[i++] = lex_cast<float>(elem);

	always_assert(i == sizeof(d.layer[layer].attn.norm.weight) / sizeof(float));
}

void
ircd::gpt::model::init_h_ln_1_bias(decoder &d,
                                   const string_view &name,
                                   const size_t &layer,
                                   const json::array &vec)
{
	size_t i(0);
	for(const auto &elem : vec)
		d.layer[layer].attn.norm.bias.elem[i++] = lex_cast<float>(elem);

	always_assert(i == sizeof(d.layer[layer].attn.norm.bias) / sizeof(float));
}

void
ircd::gpt::model::init_h_ln_2_weight(decoder &d,
                                     const string_view &name,
                                     const size_t &layer,
                                     const json::array &vec)
{
	size_t i(0);
	for(const auto &elem : vec)
		d.layer[layer].ffnn.norm.weight.elem[i++] = lex_cast<float>(elem);

	always_assert(i == sizeof(d.layer[layer].ffnn.norm.weight) / sizeof(float));
}

void
ircd::gpt::model::init_h_ln_2_bias(decoder &d,
                                   const string_view &name,
                                   const size_t &layer,
                                   const json::array &vec)
{
	size_t i(0);
	for(const auto &elem : vec)
		d.layer[layer].ffnn.norm.bias.elem[i++] = lex_cast<float>(elem);

	always_assert(i == sizeof(d.layer[layer].ffnn.norm.bias) / sizeof(float));
}

void
ircd::gpt::model::init_h_attn_attn_weight(decoder &d,
                                          const string_view &name,
                                          const size_t &layer,
                                          const json::array &mat)
{
	size_t i(0);
	for(const json::array vec : mat)
	{
		size_t j(0);
		for(const auto &elem : vec)
			d.layer[layer].attn.fcon_weight[i].fcon[j++] = lex_cast<float>(elem);

		always_assert(j == sizeof(d.layer[layer].attn.fcon_weight[i]) / sizeof(float));
		++i;
	}

	always_assert
	(
		i == sizeof(d.layer[layer].attn.fcon_weight)
		/ sizeof(d.layer[layer].attn.fcon_weight[0])
	);
}

void
ircd::gpt::model::init_h_attn_attn_bias(decoder &d,
                                        const string_view &name,
                                        const size_t &layer,
                                        const json::array &vec)
{
	size_t i(0);
	for(const auto &elem : vec)
		d.layer[layer].attn.fcon_bias.fcon[i++] = lex_cast<float>(elem);

	always_assert(i == sizeof(d.layer[layer].attn.fcon_bias) / sizeof(float));
}

void
ircd::gpt::model::init_h_attn_proj_weight(decoder &d,
                                          const string_view &name,
                                          const size_t &layer,
                                          const json::array &mat)
{
	size_t i(0);
	for(const json::array vec : mat)
	{
		size_t j(0);
		for(const auto &elem : vec)
			d.layer[layer].attn.proj_weight[i].elem[j++] = lex_cast<float>(elem);

		always_assert(j == sizeof(d.layer[layer].attn.proj_weight[i]) / sizeof(float));
		++i;
	}

	always_assert
	(
		i == sizeof(d.layer[layer].attn.proj_weight)
		/ sizeof(d.layer[layer].attn.proj_weight[0])
	);
}

void
ircd::gpt::model::init_h_attn_proj_bias(decoder &d,
                                        const string_view &name,
                                        const size_t &layer,
                                        const json::array &vec)
{
	size_t i(0);
	for(const auto &elem : vec)
		d.layer[layer].attn.proj_bias.elem[i++] = lex_cast<float>(elem);

	always_assert(i == sizeof(d.layer[layer].attn.proj_bias) / sizeof(float));
}
