// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_DB_STATS_H

namespace ircd::db
{
	// Histogram (per database)
	struct histogram;
	extern const uint32_t histogram_max;
	string_view histogram_id(const uint32_t &id);
	uint32_t histogram_id(const string_view &key);
	const struct histogram &histogram(const database &, const uint32_t &id);
	const struct histogram &histogram(const database &, const string_view &key);

	// Ticker (per database)
	extern const uint32_t ticker_max;
	string_view ticker_id(const uint32_t &id);
	uint32_t ticker_id(const string_view &key);
	uint64_t ticker(const database &, const uint32_t &id);
	uint64_t ticker(const database &, const string_view &key);

	// Perf counts (global)
	uint perf_level();
	void perf_level(const uint &);
	const rocksdb::PerfContext &perf_current();
	std::string string(const rocksdb::PerfContext &, const bool &all = false);

	// IO counters (global)
	const rocksdb::IOStatsContext &iostats_current();
	std::string string(const rocksdb::IOStatsContext &, const bool &all = false);
}

/// Copy of rocksdb::HistogramData because it is not included here.
struct ircd::db::histogram
{
	double median {0};
	double pct95 {0};
	double pct99 {0};
	double avg {0};
	double stddev {0};
	double max {0};

	uint64_t hits {0};
	uint64_t time {0};
};
