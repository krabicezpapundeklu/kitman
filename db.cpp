#include "db.hpp"

database::database(const char *db_path)
{
	if(sqlite3_open(db_path, &db_))
	{
		std::string error = sqlite3_errmsg(db_);
		sqlite3_close(db_);
		throw exception{error};
	}
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
	if(sqlite3_bind_int(stmt_, index, value))
	{
		throw exception{sqlite3_errmsg(sqlite3_db_handle(stmt_))};
	}
}

void statement::bind_value(int index, const char *value)
{
	if(sqlite3_bind_text(stmt_, index, value, -1, nullptr))
	{
		throw exception{sqlite3_errmsg(sqlite3_db_handle(stmt_))};
	}
}

void statement::bind_value(int index, const std::string &value)
{
	if(sqlite3_bind_text(stmt_, index, value.data(), value.size(), nullptr))
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

void statement::prepare(sqlite3 *db, const char *sql)
{
	if(stmt_)
	{
		sqlite3_finalize(stmt_);
		stmt_ = nullptr;
	}

	if(sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr))
	{
		throw exception{sqlite3_errmsg(db)};
	}
}

void statement::reset()
{
	if(sqlite3_reset(stmt_))
	{
		throw exception{sqlite3_errmsg(sqlite3_db_handle(stmt_))};
	}
}

bool statement::step()
{
	switch(sqlite3_step(stmt_))
	{
	case SQLITE_ROW:
		return true;

	case SQLITE_DONE:
		return false;

	default:
		throw exception{sqlite3_errmsg(sqlite3_db_handle(stmt_))};
	}
}

statement::~statement()
{
	sqlite3_finalize(stmt_);
}

transaction::transaction(sqlite3 *db)
	: db_{db}
{
	statement::exec(db_, "BEGIN TRANSACTION");
}

transaction::~transaction()
{
	if(std::uncaught_exceptions())
	{
		statement::exec(db_, "ROLLBACK TRANSACTION");
	}
	else
	{
		statement::exec(db_, "COMMIT TRANSACTION");
	}
}
