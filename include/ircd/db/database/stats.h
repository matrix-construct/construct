//
// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
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
	std::array<rocksdb::HistogramData, rocksdb::HISTOGRAM_ENUM_MAX> histogram;

	uint64_t getTickerCount(const uint32_t tickerType) const override;
	void recordTick(const uint32_t tickerType, const uint64_t count) override;
	void setTickerCount(const uint32_t tickerType, const uint64_t count) override;
	void histogramData(const uint32_t type, rocksdb::HistogramData *) const override;
	void measureTime(const uint32_t histogramType, const uint64_t time) override;
	bool HistEnabledForType(const uint32_t type) const override;
	uint64_t getAndResetTickerCount(const uint32_t tickerType) override;

	stats(database *const &d)
	:d{d}
	{}
};
