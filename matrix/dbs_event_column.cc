// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

/// Linkage for a cache of the columns of the events database which directly
/// correspond to a property in the matrix event object. This array allows
/// for constant time access to a column the same way one can make constant
/// time access to a property in m::event.
decltype(ircd::m::dbs::event_column)
ircd::m::dbs::event_column;

decltype(ircd::m::dbs::desc::_event__comp)
ircd::m::dbs::desc::_event__comp
{
	{ "name",     "ircd.m.dbs.__event.comp" },
	{ "default",  "default"                 },
};

decltype(ircd::m::dbs::desc::_event__bloom__bits)
ircd::m::dbs::desc::_event__bloom__bits
{
	{ "name",     "ircd.m.dbs.__event.bloom.bits" },
	{ "default",  0L                              },
};

//
// event_id
//

decltype(ircd::m::dbs::desc::event_id__comp)
ircd::m::dbs::desc::event_id__comp
{
	{ "name",     "ircd.m.dbs.event_id.comp" },
	{ "default",  string_view{_event__comp}  },
};

decltype(ircd::m::dbs::desc::event_id__block__size)
ircd::m::dbs::desc::event_id__block__size
{
	{ "name",     "ircd.m.dbs.event_id.block.size"  },
	{ "default",  512L                              },
};

decltype(ircd::m::dbs::desc::event_id__meta_block__size)
ircd::m::dbs::desc::event_id__meta_block__size
{
	{ "name",     "ircd.m.dbs.event_id.meta_block.size"  },
	{ "default",  512L                                   },
};

decltype(ircd::m::dbs::desc::event_id__cache__size)
ircd::m::dbs::desc::event_id__cache__size
{
	{
		{ "name",     "ircd.m.dbs.event_id.cache.size"  },
		{ "default",  long(48_MiB)                      },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "event_id"_>()));
		const size_t &value{event_id__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::event_id__cache_comp__size)
ircd::m::dbs::desc::event_id__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.event_id.cache_comp.size"  },
		{ "default",  long(16_MiB)                           },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "event_id"_>()));
		const size_t &value{event_id__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::event_id
{
	// name
	"event_id",

	// explanation
	R"(Stores the event_id property of an event.

	As with all direct event columns the key is an event_idx and the value
	is the data for the event. It should be mentioned for this column
	specifically that event_id's are already saved in the _event_idx column
	however that is a mapping of event_id to event_idx whereas this is a
	mapping of event_idx to event_id.

	10.4
	MUST NOT exceed 255 bytes.

	)",

	// typing (key, value)
	{
		typeid(uint64_t), typeid(string_view)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	{},

	// drop column
	false,

	// cache size
	bool(cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(_event__bloom__bits),

	// expect queries hit
	true,

	// block size
	size_t(event_id__block__size),

	// meta_block size
	size_t(event_id__meta_block__size),

	// compression
	string_view{event_id__comp},
};

//
// type
//

decltype(ircd::m::dbs::desc::type__comp)
ircd::m::dbs::desc::type__comp
{
	{ "name",     "ircd.m.dbs.type.comp"     },
	{ "default",  string_view{_event__comp}  },
};

decltype(ircd::m::dbs::desc::type__block__size)
ircd::m::dbs::desc::type__block__size
{
	{ "name",     "ircd.m.dbs.type.block.size"  },
	{ "default",  512L                          },
};

decltype(ircd::m::dbs::desc::type__meta_block__size)
ircd::m::dbs::desc::type__meta_block__size
{
	{ "name",     "ircd.m.dbs.type.meta_block.size"  },
	{ "default",  512L                               },
};

decltype(ircd::m::dbs::desc::type__cache__size)
ircd::m::dbs::desc::type__cache__size
{
	{
		{ "name",     "ircd.m.dbs.type.cache.size"  },
		{ "default",  long(64_MiB)                  },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "type"_>()));
		const size_t &value{type__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::type__cache_comp__size)
ircd::m::dbs::desc::type__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.type.cache_comp.size"  },
		{ "default",  long(16_MiB)                       },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "type"_>()));
		const size_t &value{type__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::type
{
	// name
	"type",

	// explanation
	R"(Stores the type property of an event.

	10.1
	The type of event. This SHOULD be namespaced similar to Java package naming conventions
	e.g. 'com.example.subdomain.event.type'.

	10.4
	MUST NOT exceed 255 bytes.

	### developer note:
	key is event_idx number.
	)",

	// typing (key, value)
	{
		typeid(uint64_t), typeid(string_view)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	{},

	// drop column
	false,

	// cache size
	bool(cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(_event__bloom__bits),

	// expect queries hit
	true,

	// block size
	size_t(type__block__size),

	// meta_block size
	size_t(type__meta_block__size),

	// compression
	string_view{type__comp},
};

//
// content
//

decltype(ircd::m::dbs::desc::content__comp)
ircd::m::dbs::desc::content__comp
{
	{ "name",     "ircd.m.dbs.content.comp"  },
	{ "default",  string_view{_event__comp}  },
};

decltype(ircd::m::dbs::desc::content__block__size)
ircd::m::dbs::desc::content__block__size
{
	{ "name",     "ircd.m.dbs.content.block.size"  },
	{ "default",  long(1_KiB)                      },
};

decltype(ircd::m::dbs::desc::content__meta_block__size)
ircd::m::dbs::desc::content__meta_block__size
{
	{ "name",     "ircd.m.dbs.content.meta_block.size"  },
	{ "default",  512L                                  },
};

decltype(ircd::m::dbs::desc::content__cache__size)
ircd::m::dbs::desc::content__cache__size
{
	{
		{ "name",     "ircd.m.dbs.content.cache.size"  },
		{ "default",  long(64_MiB)                     },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "content"_>()));
		const size_t &value{content__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::content__cache_comp__size)
ircd::m::dbs::desc::content__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.content.cache_comp.size"  },
		{ "default",  long(16_MiB)                          },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "content"_>()));
		const size_t &value{content__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

decltype(ircd::m::dbs::desc::content__file__size__max)
ircd::m::dbs::desc::content__file__size__max
{
	{ "name",     "ircd.m.dbs.content.file.size.max"  },
	{ "default",  long(256_MiB)                       },
};

const ircd::db::descriptor
ircd::m::dbs::desc::content
{
	// name
	"content",

	// explanation
	R"(Stores the content property of an event.

	10.1
	The fields in this object will vary depending on the type of event. When interacting
	with the REST API, this is the HTTP body.

	### developer note:
	Since events must not exceed 64 KiB the maximum size for the content is the remaining
	space after all the other fields for the event are rendered.

	key is event_idx number.
	)",

	// typing (key, value)
	{
		typeid(uint64_t), typeid(string_view)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	{},

	// drop column
	false,

	// cache size
	bool(cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(_event__bloom__bits),

	// expect queries hit
	true,

	// block size
	size_t(content__block__size),

	// meta_block size
	size_t(content__meta_block__size),

	// compression
	string_view{content__comp},

	// compactor
	{},

	// compaction priority algorithm,
	"Universal"s,

	// target_file_size
	{
		size_t(content__file__size__max),
		1L,
	},
};

//
// room_id
//

decltype(ircd::m::dbs::desc::room_id__comp)
ircd::m::dbs::desc::room_id__comp
{
	{ "name",     "ircd.m.dbs.room_id.comp"  },
	{ "default",  string_view{_event__comp}  },
};

decltype(ircd::m::dbs::desc::room_id__block__size)
ircd::m::dbs::desc::room_id__block__size
{
	{ "name",     "ircd.m.dbs.room_id.block.size"  },
	{ "default",  512L                             },
};

decltype(ircd::m::dbs::desc::room_id__meta_block__size)
ircd::m::dbs::desc::room_id__meta_block__size
{
	{ "name",     "ircd.m.dbs.room_id.meta_block.size"  },
	{ "default",  512L                                  },
};

decltype(ircd::m::dbs::desc::room_id__cache__size)
ircd::m::dbs::desc::room_id__cache__size
{
	{
		{ "name",     "ircd.m.dbs.room_id.cache.size"  },
		{ "default",  long(32_MiB)                     },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "room_id"_>()));
		const size_t &value{room_id__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::room_id__cache_comp__size)
ircd::m::dbs::desc::room_id__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.room_id.cache_comp.size"  },
		{ "default",  long(16_MiB)                          },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "room_id"_>()));
		const size_t &value{room_id__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::room_id
{
	// name
	"room_id",

	// explanation
	R"(Stores the room_id property of an event.

	10.2 (apropos room events)
	Required. The ID of the room associated with this event.

	10.4
	MUST NOT exceed 255 bytes.

	### developer note:
	key is event_idx number.
	)",

	// typing (key, value)
	{
		typeid(uint64_t), typeid(string_view)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	{},

	// drop column
	false,

	// cache size
	bool(cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(_event__bloom__bits),

	// expect queries hit
	true,

	// block size
	size_t(room_id__block__size),

	// meta_block size
	size_t(room_id__meta_block__size),

	// compression
	string_view{room_id__comp},
};

//
// sender
//

decltype(ircd::m::dbs::desc::sender__comp)
ircd::m::dbs::desc::sender__comp
{
	{ "name",     "ircd.m.dbs.sender.comp"   },
	{ "default",  string_view{_event__comp}  },
};

decltype(ircd::m::dbs::desc::sender__block__size)
ircd::m::dbs::desc::sender__block__size
{
	{ "name",     "ircd.m.dbs.sender.block.size"  },
	{ "default",  512L                            },
};

decltype(ircd::m::dbs::desc::sender__meta_block__size)
ircd::m::dbs::desc::sender__meta_block__size
{
	{ "name",     "ircd.m.dbs.sender.meta_block.size"  },
	{ "default",  512L                                 },
};

decltype(ircd::m::dbs::desc::sender__cache__size)
ircd::m::dbs::desc::sender__cache__size
{
	{
		{ "name",     "ircd.m.dbs.sender.cache.size"  },
		{ "default",  long(32_MiB)                    },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "sender"_>()));
		const size_t &value{sender__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::sender__cache_comp__size)
ircd::m::dbs::desc::sender__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.sender.cache_comp.size"  },
		{ "default",  long(16_MiB)                         },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "sender"_>()));
		const size_t &value{sender__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::sender
{
	// name
	"sender",

	// explanation
	R"(Stores the sender property of an event.

	10.2 (apropos room events)
	Required. Contains the fully-qualified ID of the user who sent this event.

	10.4
	MUST NOT exceed 255 bytes.

	### developer note:
	key is event_idx number.
	)",

	// typing (key, value)
	{
		typeid(uint64_t), typeid(string_view)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	{},

	// drop column
	false,

	// cache size
	bool(cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(_event__bloom__bits),

	// expect queries hit
	true,

	// block size
	size_t(sender__block__size),

	// meta_block size
	size_t(sender__meta_block__size),

	// compression
	string_view{sender__comp},
};

//
// state_key
//

decltype(ircd::m::dbs::desc::state_key__comp)
ircd::m::dbs::desc::state_key__comp
{
	{ "name",     "ircd.m.dbs.state_key.comp"  },
	{ "default",  string_view{_event__comp}    },
};

decltype(ircd::m::dbs::desc::state_key__block__size)
ircd::m::dbs::desc::state_key__block__size
{
	{ "name",     "ircd.m.dbs.state_key.block.size"  },
	{ "default",  512L                               },
};

decltype(ircd::m::dbs::desc::state_key__meta_block__size)
ircd::m::dbs::desc::state_key__meta_block__size
{
	{ "name",     "ircd.m.dbs.state_key.meta_block.size"  },
	{ "default",  512L                                    },
};

decltype(ircd::m::dbs::desc::state_key__cache__size)
ircd::m::dbs::desc::state_key__cache__size
{
	{
		{ "name",     "ircd.m.dbs.state_key.cache.size"  },
		{ "default",  long(32_MiB)                       },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "state_key"_>()));
		const size_t &value{state_key__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::state_key__cache_comp__size)
ircd::m::dbs::desc::state_key__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.state_key.cache_comp.size"  },
		{ "default",  long(16_MiB)                            },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "state_key"_>()));
		const size_t &value{state_key__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::state_key
{
	// name
	"state_key",

	// explanation
	R"(Stores the state_key property of an event.

	10.3 (apropos room state events)
	A unique key which defines the overwriting semantics for this piece of room state.
	This value is often a zero-length string. The presence of this key makes this event a
	State Event. The key MUST NOT start with '_'.

	10.4
	MUST NOT exceed 255 bytes.

	### developer note:
	key is event_idx number.
	)",

	// typing (key, value)
	{
		typeid(uint64_t), typeid(string_view)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	{},

	// drop column
	false,

	// cache size
	bool(cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(_event__bloom__bits),

	// expect queries hit
	true,

	// block size
	size_t(state_key__block__size),

	// meta_block size
	size_t(state_key__meta_block__size),

	// compression
	string_view{state_key__comp},
};

//
// origin_server_ts
//

decltype(ircd::m::dbs::desc::origin_server_ts__comp)
ircd::m::dbs::desc::origin_server_ts__comp
{
	{ "name",     "ircd.m.dbs.origin_server_ts.comp"  },
	{ "default",  string_view{_event__comp}           },
};

decltype(ircd::m::dbs::desc::origin_server_ts__block__size)
ircd::m::dbs::desc::origin_server_ts__block__size
{
	{ "name",     "ircd.m.dbs.origin_server_ts.block.size"  },
	{ "default",  256L                                      },
};

decltype(ircd::m::dbs::desc::origin_server_ts__meta_block__size)
ircd::m::dbs::desc::origin_server_ts__meta_block__size
{
	{ "name",     "ircd.m.dbs.origin_server_ts.meta_block.size"  },
	{ "default",  512L                                           },
};

decltype(ircd::m::dbs::desc::origin_server_ts__cache__size)
ircd::m::dbs::desc::origin_server_ts__cache__size
{
	{
		{ "name",     "ircd.m.dbs.origin_server_ts.cache.size"  },
		{ "default",  long(32_MiB)                              },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "origin_server_ts"_>()));
		const size_t &value{origin_server_ts__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::origin_server_ts__cache_comp__size)
ircd::m::dbs::desc::origin_server_ts__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.origin_server_ts.cache_comp.size"  },
		{ "default",  long(16_MiB)                                   },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "origin_server_ts"_>()));
		const size_t &value{origin_server_ts__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::origin_server_ts
{
	// name
	"origin_server_ts",

	// explanation
	R"(Stores the origin_server_ts property of an event.

	FEDERATION 4.1
	Timestamp in milliseconds on origin homeserver when this PDU was created.

	### developer note:
	key is event_idx number.
	value is a machine integer (binary)

	TODO: consider unsigned rather than time_t because of millisecond precision

	)",

	// typing (key, value)
	{
		typeid(uint64_t), typeid(time_t)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	{},

	// drop column
	false,

	// cache size
	bool(cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(_event__bloom__bits),

	// expect queries hit
	true,

	// block size
	size_t(origin_server_ts__block__size),

	// meta_block size
	size_t(origin_server_ts__meta_block__size),

	// compression
	string_view{origin_server_ts__comp},
};

//
// depth
//

decltype(ircd::m::dbs::desc::depth__comp)
ircd::m::dbs::desc::depth__comp
{
	{ "name",     "ircd.m.dbs.depth.comp"   },
	{ "default",  string_view{_event__comp} },
};

decltype(ircd::m::dbs::desc::depth__block__size)
ircd::m::dbs::desc::depth__block__size
{
	{ "name",     "ircd.m.dbs.depth.block.size"  },
	{ "default",  256L                           },
};

decltype(ircd::m::dbs::desc::depth__meta_block__size)
ircd::m::dbs::desc::depth__meta_block__size
{
	{ "name",     "ircd.m.dbs.depth.meta_block.size"  },
	{ "default",  512L                                },
};

decltype(ircd::m::dbs::desc::depth__cache__size)
ircd::m::dbs::desc::depth__cache__size
{
	{
		{ "name",     "ircd.m.dbs.depth.cache.size"  },
		{ "default",  long(16_MiB)                   },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "depth"_>()));
		const size_t &value{depth__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::depth__cache_comp__size)
ircd::m::dbs::desc::depth__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.depth.cache_comp.size"  },
		{ "default",  long(16_MiB)                        },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "depth"_>()));
		const size_t &value{depth__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::depth
{
	// name
	"depth",

	// explanation
	R"(Stores the depth property of an event.

	### developer note:
	key is event_idx number. value is long integer
	)",

	// typing (key, value)
	{
		typeid(uint64_t), typeid(int64_t)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	{},

	// drop column
	false,

	// cache size
	bool(cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(_event__bloom__bits),

	// expect queries hit
	true,

	// block size
	size_t(depth__block__size),

	// meta_block size
	size_t(depth__meta_block__size),

	// compression
	string_view{depth__comp},
};

void
ircd::m::dbs::_index_event_cols(db::txn &txn,
                                const event &event,
                                const write_opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_COLS));
	assert(opts.event_idx);
	const byte_view<string_view> key
	{
		opts.event_idx
	};

	size_t i{0};
	for_each(event, [&txn, &opts, &key, &i]
	(const auto &, auto&& val)
	{
		auto &column
		{
			event_column.at(i++)
		};

		if(!column)
			return;

		if(value_required(opts.op) && !defined(json::value(val)))
			return;

		// If an already-strung json::object is carried by the event we
		// re-stringify it into a temporary buffer. This is the common case
		// because the original source might be crap JSON w/ spaces etc.
		constexpr bool canonizable
		{
			std::is_same<decltype(val), json::object>() ||
			std::is_same<decltype(val), json::array>() ||
			std::is_same<decltype(val), json::string>()
		};

		if constexpr(canonizable)
			if(opts.op == db::op::SET && !opts.json_source)
				val = json::stringify(mutable_buffer{event::buf[0]}, val);

		db::txn::append
		{
			txn, column, db::column::delta
			{
				opts.op,
				string_view{key},
				value_required(opts.op)?
					byte_view<string_view>{val}:
					byte_view<string_view>{}
			}
		};
	});
}
