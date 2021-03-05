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
	init_h_attn_bias(decoder &, const string_view &, const size_t &, const json::array &),
	init() noexcept;

	extern conf::item<std::string> path;
	extern const std::pair
	<
		string_view,
		void (*)(decoder &, const string_view &, const size_t &, const json::array &)
	>
	manifest[],
	manifest_h[],
	manifest_td[];
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

decltype(ircd::gpt::model::manifest_td)
ircd::gpt::model::manifest_td
{
	{ "test.jsonl",    nullptr,  },
	{ "valid.jsonl",   nullptr,  },
	{ "train.jsonl",   nullptr,  },
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

//TODO: XXX
namespace ircd::gpt
{
	extern const std::unique_ptr<model::decoder> device;
}

void
ircd::gpt::model::init()
noexcept
{
	if(!model::path)
		return;

	const size_t layers
	{
		12
	};

	const auto handle{[]
	(const auto &a, const auto &b, const auto &i)
	{
		const auto &[fmt, handler]
		{
			a[b]
		};

		char namebuf[128] {0};
		const string_view path_part[2]
		{
			model::path, fmt::sprintf
			{
				namebuf, fmt, i
			}
		};

		const fs::fd fd
		{
			fs::path(fs::path_scratch, path_part)
		};

		fs::map::opts map_opts;
		const fs::map map
		{
			fd, map_opts
		};

		const json::array mat
		{
			map
		};

		assert(gpt::device);
		handler(*gpt::device, path_part[1], i, mat);
		log::logf
		{
			log, log::level::DEBUG,
			"Model init [%2d][%2d] :%s",
			i,
			b,
			path_part[1],
		};
	}};

	ircd::timer sw;
	size_t read(0), wrote(0);
	if(fs::exists("model"))
	{
		const auto _read
		{
			fs::read(fs::fd{"model"}, mutable_buffer{(char *)(gpt::device.get()), sizeof(model::decoder)})
		};

		read = size(_read);
	} else {
		memset(device.get(),  0x0, sizeof(model::decoder));

		handle(manifest, 0, 0);
		handle(manifest, 1, 0);
		handle(manifest, 2, 0);
		handle(manifest, 3, 0);
		for(size_t i(0); i < layers; ++i)
			for(size_t j(0); j < 13; ++j)
				handle(manifest_h, j, i);

		const auto _wrote
		{
			fs::write("model", const_buffer{(const char *)(gpt::device.get()), sizeof(model::decoder)})
		};

		wrote = size(_wrote);
	}

	char pbuf[3][48];
	log::logf
	{
		log, log::level::DEBUG,
		"Model init completed in %s read %s wrote %s",
		sw.pretty(pbuf[0]),
		pretty(pbuf[1], iec(size(read))),
		pretty(pbuf[2], iec(size(wrote))),
	};
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
			d.wpe[i][j++] = lex_cast<float>(elem);

		always_assert(j == sizeof(d.wpe[i]) / sizeof(float));
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
			d.wte[i][j++] = lex_cast<float>(elem);

		always_assert(j == sizeof(d.wte[i]) / sizeof(float));
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
