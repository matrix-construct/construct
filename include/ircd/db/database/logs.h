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
#define HAVE_IRCD_DB_DATABASE_LOGS_H

// This file is not part of the standard include stack because it requires
// RocksDB symbols which we cannot forward declare. It is used internally
// and does not need to be included by general users of IRCd.

struct ircd::db::database::logs final
:std::enable_shared_from_this<struct database::logs>
,rocksdb::Logger
{
	database *d;

	void Logv(const rocksdb::InfoLogLevel level, const char *fmt, va_list ap) noexcept override;
	void Logv(const char *fmt, va_list ap) noexcept override;
	void LogHeader(const char *fmt, va_list ap) noexcept override;

	rocksdb::Status Close() noexcept override;

	logs(database *const &d);
	~logs() noexcept override;
};
