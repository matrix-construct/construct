// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::conf
{
	std::string _config;
	static std::string read_json_file(string_view filename);
}

//TODO: X
decltype(ircd::conf::config)
ircd::conf::config
{
	_config
};

// The configuration file is a user-converted Synapse homeserver.yaml
// converted into JSON. The configuration file is only truly necessary
// the first time IRCd is ever run. It does not have to be passed again
// to subsequent executions of IRCd if the database can be found. If
// the database is found, data passed in the configfile will be used
// to override the databased values. In this case, the complete config
// is not required to be specified in the file; only what is present
// will be used to override.
//
// *NOTE* This expects *canonical JSON* right now. That means converting
// your homeserver.yaml may be a two step process: 1. YAML to JSON, 2.
// whitespace-stripping the JSON. Tools to do both of these things are
// first hits in a google search.
void
ircd::conf::init(const string_view &filename)
{
	_config = read_json_file(filename);
}

//
// item
//

template<>
decltype(ircd::instance_list<ircd::conf::item<>>::list)
ircd::instance_list<ircd::conf::item<>>::list
{};

/// Conf item abstract constructor.
ircd::conf::item<void>::item(const json::members &opts)
:feature_
{
	opts
}
,feature
{
	feature_
}
,name
{
	feature.has("name")? unquote(feature["name"]) : "<unnamed>"_sv
}
{
}

ircd::conf::item<void>::~item()
noexcept
{
}

bool
ircd::conf::item<void>::refresh()
{
	return false;
}

//
// misc
//

std::string
ircd::conf::read_json_file(string_view filename)
try
{
	if(!filename.empty())
		log::debug
		{
			"User supplied a configuration file path: `%s'", filename
		};

	if(filename.empty())
		filename = fs::CPATH;

	if(!fs::exists(filename))
		return {};

	std::string read
	{
		fs::read(filename)
	};

	// ensure any trailing cruft is removed to not set off the validator
	if(endswith(read, '\n'))
		read.pop_back();

	if(endswith(read, '\r'))
		read.pop_back();

	// grammar check; throws on error
	json::valid(read);

	const json::object object{read};
	const size_t key_count{object.count()};
	log::info
	{
		"Using configuration from: `%s': JSON object with %zu members in %zu bytes",
		filename,
		key_count,
		read.size()
	};

	return read;
}
catch(const std::exception &e)
{
	log::error
	{
		"Configuration @ `%s': %s", filename, e.what()
	};

	throw;
}
