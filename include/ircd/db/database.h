/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once
#define HAVE_IRCD_DB_DATABASE_H

namespace ircd {
namespace db   {

struct database
:std::enable_shared_from_this<struct database>
{
	struct options;
    struct events;
    struct stats;
    struct logs;
    struct mergeop;
	struct snapshot;
	struct comparator;
	struct column;

	struct descriptor
	{
		using typing = std::pair<std::type_index, std::type_index>;

		std::string name;
		typing type          { typeid(string_view), typeid(string_view) };
		std::string options  {};
		db::comparator cmp   {};
	};

    static std::map<string_view, database *> dbs; // open databases

    std::string name;
    std::string path;
    std::shared_ptr<struct logs> logs;
    std::shared_ptr<struct stats> stats;
    std::shared_ptr<struct events> events;
    std::shared_ptr<struct mergeop> mergeop;
    std::shared_ptr<rocksdb::Cache> cache;
	std::map<string_view, std::shared_ptr<column>> columns;
    custom_ptr<rocksdb::DB> d;

  public:
	operator const rocksdb::DB &() const         { return *d;                                      }
	operator rocksdb::DB &()                     { return *d;                                      }

	const column &operator[](const string_view &) const;
	column &operator[](const string_view &);

    database(const std::string &name,
             const std::string &options = {},
             std::initializer_list<descriptor> = {});

	database() = default;
	database(database &&) = delete;
	database(const database &) = delete;
    ~database() noexcept;

	static const database &get(const column &);
	static database &get(column &);
};

// options <-> string
struct database::options
:std::string
{
	struct map;

	// Output of options structures from this string
	explicit operator rocksdb::Options() const;
	operator rocksdb::DBOptions() const;
	operator rocksdb::ColumnFamilyOptions() const;
	operator rocksdb::PlainTableOptions() const;
	operator rocksdb::BlockBasedTableOptions() const;

	// Input of options structures output to this string
	explicit options(const rocksdb::ColumnFamilyOptions &);
	explicit options(const rocksdb::DBOptions &);
	explicit options(const database::column &);
	explicit options(const database &);

	// Input of options string from user
	options(std::string string)
	:std::string{std::move(string)}
	{}
};

// options <-> map
struct database::options::map
:std::unordered_map<std::string, std::string>
{
	// Output of options structures from map
	operator rocksdb::DBOptions() const;
	operator rocksdb::ColumnFamilyOptions() const;
	operator rocksdb::PlainTableOptions() const;
	operator rocksdb::BlockBasedTableOptions() const;

	// Convert option string to map
	map(const options &);

	// Input of options map from user
	map(std::unordered_map<std::string, std::string> m)
	:std::unordered_map<std::string, std::string>{std::move(m)}
	{}
};

struct database::snapshot
{
	std::weak_ptr<database> d;
	std::shared_ptr<const rocksdb::Snapshot> s;

  public:
	operator const database &() const            { return *d.lock();                               }
	operator const rocksdb::Snapshot *() const   { return s.get();                                 }

	operator database &()                        { return *d.lock();                               }

	operator bool() const                        { return bool(s);                                 }
	bool operator !() const                      { return !s;                                      }

	snapshot(database &);
	snapshot() = default;
	~snapshot() noexcept;
};

// Get property data from all columns in DB. Only integer properties supported.
template<class R = uint64_t> R property(database &, const string_view &name);
template<> uint64_t property(database &, const string_view &name);

const std::string &name(const database::column &);
uint32_t id(const database::column &);
void drop(database::column &);                   // Request to erase column from db

uint64_t sequence(const database::snapshot &);   // Sequence of a snapshot
uint64_t sequence(const database &);             // Latest sequence number

void sync(database &);                           // Sync the write log (all columns)

} // namespace db

using database = db::database;

} // namespace ircd
