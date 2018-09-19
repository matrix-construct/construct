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
#define HAVE_IRCD_DB_DATABASE_EVENTS_H

// This file is not part of the standard include stack because it requires
// RocksDB symbols which we cannot forward declare. It is used internally
// and does not need to be included by general users of IRCd.

struct ircd::db::database::events final
:std::enable_shared_from_this<struct ircd::db::database::events>
,rocksdb::EventListener
{
	database *d;

	void OnFlushBegin(rocksdb::DB *, const rocksdb::FlushJobInfo &) noexcept override;
	void OnFlushCompleted(rocksdb::DB *, const rocksdb::FlushJobInfo &) noexcept override;
	void OnCompactionCompleted(rocksdb::DB *, const rocksdb::CompactionJobInfo &) noexcept override;
	void OnTableFileDeleted(const rocksdb::TableFileDeletionInfo &) noexcept override;
	void OnTableFileCreated(const rocksdb::TableFileCreationInfo &) noexcept override;
	void OnTableFileCreationStarted(const rocksdb::TableFileCreationBriefInfo &) noexcept override;
	void OnMemTableSealed(const rocksdb::MemTableInfo &) noexcept override;
	void OnColumnFamilyHandleDeletionStarted(rocksdb::ColumnFamilyHandle *) noexcept override;
	void OnExternalFileIngested(rocksdb::DB *, const rocksdb::ExternalFileIngestionInfo &) noexcept override;
	void OnBackgroundError(rocksdb::BackgroundErrorReason, rocksdb::Status *) noexcept override;
	void OnStallConditionsChanged(const rocksdb::WriteStallInfo &) noexcept override;

	events(database *const &d)
	:d{d}
	{}
};
