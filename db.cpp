#include "db.hpp"

database::database(const char *db_path)
{
	if(sqlite3_open(db_path, &db_))
	{
		std::string error = sqlite3_errmsg(db_);
		sqlite3_close(db_);
		throw exception{error};
	}

	begin_transaction_.prepare(db_, "BEGIN TRANSACTION");
	commit_transaction_.prepare(db_, "COMMIT TRANSACTION");
	rollback_transaction_.prepare(db_, "ROLLBACK TRANSACTION");
}

database::operator sqlite3 *()
{
	return db_;
}

int database::get_last_id() const
{
	return sqlite3_last_insert_rowid(db_);
}

database::~database()
{
	sqlite3_close(db_);
}

void statement::bind_value(int index, int value)
{
	check(sqlite3_bind_int(stmt_, index, value));
}

void statement::bind_value(int index, const char *value)
{
	check(sqlite3_bind_text(stmt_, index, value, -1, nullptr));
}

void statement::bind_value(int index, const std::string &value)
{
	check(sqlite3_bind_text(stmt_, index, value.data(), value.size(), nullptr));
}

void statement::check(int result) const
{
	if(result)
	{
		throw exception{sqlite3_errmsg(sqlite3_db_handle(stmt_))};
	}
}

int statement::get_int(int index) const
{
	return sqlite3_column_int(stmt_, index);
}

const char *statement::get_text(int index) const
{
	return reinterpret_cast<const char *>(sqlite3_column_text(stmt_, index));
}

statement &statement::prepare(sqlite3 *db, const char *sql)
{
	if(stmt_)
	{
		sqlite3_finalize(stmt_);
		stmt_ = nullptr;
	}

	check(sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr));

	return *this;
}

void statement::reset()
{
	check(sqlite3_reset(stmt_));
}

bool statement::step()
{
	const auto result = sqlite3_step(stmt_);

	switch(result)
	{
	case SQLITE_ROW:
		return true;

	case SQLITE_DONE:
		return false;

	default:
		check(result);
		return false;
	}
}

statement::~statement()
{
	sqlite3_finalize(stmt_);
}

transaction::transaction(database &db)
	: db_{db}
{
	db_.begin_transaction_.exec();
}

transaction::~transaction()
{
	if(std::uncaught_exceptions())
	{
		db_.rollback_transaction_.exec();
	}
	else
	{
		db_.commit_transaction_.exec();
	}
}
