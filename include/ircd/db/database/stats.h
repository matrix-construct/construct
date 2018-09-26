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
#define HAVE_IRCD_DB_DATABASE_STATS_H

// This file is not part of the standard include stack because it requires
// RocksDB symbols which we cannot forward declare. It is used internally
// and does not need to be included by general users of IRCd.

struct ircd::db::database::stats final
:std::enable_shared_from_this<struct ircd::db::database::stats>
,rocksdb::Statistics
{
	database *d;
	std::array<uint64_t, rocksdb::TICKER_ENUM_MAX> ticker {{0}};
	std::array<struct db::histogram, rocksdb::HISTOGRAM_ENUM_MAX> histogram;

	uint64_t getTickerCount(const uint32_t tickerType) const noexcept override;
	void recordTick(const uint32_t tickerType, const uint64_t count) noexcept override;
	void setTickerCount(const uint32_t tickerType, const uint64_t count) noexcept override;
	void histogramData(const uint32_t type, rocksdb::HistogramData *) const noexcept override;
	void measureTime(const uint32_t histogramType, const uint64_t time) noexcept override;
	bool HistEnabledForType(const uint32_t type) const noexcept override;
	uint64_t getAndResetTickerCount(const uint32_t tickerType) noexcept override;
	rocksdb::Status Reset() noexcept override;

	stats(database *const &d);
};
