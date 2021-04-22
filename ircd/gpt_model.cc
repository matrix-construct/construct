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
	init_h_attn_proj_bias(decoder &, const string_view &, const size_t &, const json::array &),
	init_h_attn_bias(decoder &, const string_view &, const size_t &, const json::array &);

	static bool init_dataset(const string_view &);
	static bool init_from_cache(const string_view &);
	static void init_from_json_handle(decoder &, const init_handler &, const size_t &);
	static void init_from_json(const string_view &, const string_view &);
	static void init(), fini() noexcept;

	extern const init_handler
	manifest[],
	manifest_h[];

	extern conf::item<std::string>
	path,
	cache_path,
	dataset_path;

	static fs::map
	default_model_shm,
	default_dataset_shm;

	static std::unique_ptr<decoder> default_model_res;
}

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
	{ "h.%u.attn.bias.json",              init_h_attn_bias,         },
};

decltype(ircd::gpt::model::manifest)
ircd::gpt::model::manifest
{
	{ "ln_f.weight.json",   init_f_weight,  },
	{ "ln_f.bias.json",     init_f_bias,    },
	{ "wpe.weight.json",    init_wpe_weight },
	{ "wte.weight.json",    init_wte_weight },
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

decltype(ircd::gpt::model::default_dataset)
ircd::gpt::model::default_dataset;

decltype(ircd::gpt::model::default_data)
ircd::gpt::model::default_data;

void
ircd::gpt::model::init()
{
	if(!model::path)
		return;

	if(!init_from_cache(model::cache_path))
		init_from_json(model::cache_path, model::path);

	if(model::dataset_path)
		init_dataset(model::dataset_path);
}

void
ircd::gpt::model::fini()
noexcept
{
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

	const auto size
	{
		fs::size(cache_path)
	};

	if(unlikely(size != sizeof(model::decoder)))
		throw error
		{
			"Cached model `%s' size %zu differs from %zu.",
			cache_path,
			size,
			sizeof(model::decoder),
		};

	const fs::fd fd
	{
		cache_path, std::ios::in //| std::ios::out
	};

	fs::map::opts map_opts
	{
		std::ios::in | std::ios::out
	};

	map_opts.huge2mb = true;
	map_opts.locked = false;
	map_opts.shared = false;
	default_model_shm = fs::map
	{
		fd, map_opts, sizeof(decoder)
	};

	default_model = reinterpret_cast<decoder *>
	(
		data(default_model_shm)
	);

	char pbuf[48];
	log::info
	{
		log, "model(%p) mapped cached model `%s' %s",
		data(default_model_shm),
		cache_path,
		pretty(pbuf, iec(size)),
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
		for(size_t j(0); j < 13; ++j)
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

	default_model_res = std::move(decoder);
	default_model = default_model_res.get();
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

	fs::fd::opts fdopts;
	fdopts.sequential = true;
	const fs::fd fd
	{
		path, fdopts
	};

	// mmap of the file
	const fs::map map
	{
		fd
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

	const fs::fd fd
	{
		path
	};

	fs::map::opts map_opts;
	map_opts.huge2mb = true;
	default_dataset_shm = fs::map
	{
		fd, map_opts, size
	};

	default_dataset = string_view
	(
		default_dataset_shm
	);

	size_t checkpoint(0);
	default_data.resize(260000); //TODO: XXX
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
			d.word.pos[i][j++] = lex_cast<float>(elem);

		always_assert(j == sizeof(d.word.pos[i]) / sizeof(float));
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
			d.word.token[i][j++] = lex_cast<float>(elem);

		always_assert(j == sizeof(d.word.token[i]) / sizeof(float));
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
		d.f.weight[i++] = lex_cast<float>(elem);

	always_assert(i == sizeof(d.f.weight) / sizeof(float));
}

void
ircd::gpt::model::init_f_bias(decoder &d,
                              const string_view &name,
                              const size_t &layer,
                              const json::array &vec)
{
	size_t i(0);
	for(const auto &elem : vec)
		d.f.bias[i++] = lex_cast<float>(elem);

	always_assert(i == sizeof(d.f.bias) / sizeof(float));
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
			d.layer[layer].ffnn.fc_weight[i][j++] = lex_cast<float>(elem);

		always_assert(j == sizeof(d.layer[layer].ffnn.fc_weight[i]) / sizeof(float));
		++i;
	}

	always_assert
	(
		i == sizeof(d.layer[layer].ffnn.fc_weight)
		/ sizeof(d.layer[layer].ffnn.fc_weight[0])
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
		d.layer[layer].ffnn.fc_bias[i++] = lex_cast<float>(elem);

	always_assert(i == sizeof(d.layer[layer].ffnn.fc_bias) / sizeof(float));
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
			d.layer[layer].ffnn.proj_weight[i][j++] = lex_cast<float>(elem);

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
		d.layer[layer].ffnn.proj_bias[i++] = lex_cast<float>(elem);

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
		d.layer[layer].ln1.weight[i++] = lex_cast<float>(elem);

	always_assert(i == sizeof(d.layer[layer].ln1.weight) / sizeof(float));
}

void
ircd::gpt::model::init_h_ln_1_bias(decoder &d,
                                   const string_view &name,
                                   const size_t &layer,
                                   const json::array &vec)
{
	size_t i(0);
	for(const auto &elem : vec)
		d.layer[layer].ln1.bias[i++] = lex_cast<float>(elem);

	always_assert(i == sizeof(d.layer[layer].ln1.bias) / sizeof(float));
}

void
ircd::gpt::model::init_h_ln_2_weight(decoder &d,
                                     const string_view &name,
                                     const size_t &layer,
                                     const json::array &vec)
{
	size_t i(0);
	for(const auto &elem : vec)
		d.layer[layer].ln2.weight[i++] = lex_cast<float>(elem);

	always_assert(i == sizeof(d.layer[layer].ln2.weight) / sizeof(float));
}

void
ircd::gpt::model::init_h_ln_2_bias(decoder &d,
                                   const string_view &name,
                                   const size_t &layer,
                                   const json::array &vec)
{
	size_t i(0);
	for(const auto &elem : vec)
		d.layer[layer].ln2.bias[i++] = lex_cast<float>(elem);

	always_assert(i == sizeof(d.layer[layer].ln2.bias) / sizeof(float));
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
			d.layer[layer].attn.attn_weight[i][j++] = lex_cast<float>(elem);

		always_assert(j == sizeof(d.layer[layer].attn.attn_weight[i]) / sizeof(float));
		++i;
	}

	always_assert
	(
		i == sizeof(d.layer[layer].attn.attn_weight)
		/ sizeof(d.layer[layer].attn.attn_weight[0])
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
		d.layer[layer].attn.attn_bias[i++] = lex_cast<float>(elem);

	always_assert(i == sizeof(d.layer[layer].attn.attn_bias) / sizeof(float));
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
			d.layer[layer].attn.proj_weight[i][j++] = lex_cast<float>(elem);

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
		d.layer[layer].attn.proj_bias[i++] = lex_cast<float>(elem);

	always_assert(i == sizeof(d.layer[layer].attn.proj_bias) / sizeof(float));
}

void
ircd::gpt::model::init_h_attn_bias(decoder &d,
                                   const string_view &name,
                                   const size_t &layer,
                                   const json::array &mat)
{
	for(const json::array dim0 : mat)
	{
		for(const json::array dim1 : dim0)
		{
			size_t k(0);
			for(const json::array dim2 : dim1)
			{
				size_t l(0);
				for(const auto &elem : dim2)
				{
					always_assert(elem == "1.0" || elem == "0.0");
					d.layer[layer].attn.bias[k][l++] = startswith(elem, '1');
				}

				++k;
			}

			always_assert
			(
				k == sizeof(d.layer[layer].attn.bias)
				/ sizeof(d.layer[layer].attn.bias[0])
			);
		}
	}
}
