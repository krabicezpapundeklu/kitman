#pragma once

#include <optional>

#include "exception.hpp"
#include "sqlite3.h"

class database
{
public:
	explicit database(const char *db_path);

	database(const database &) = delete;
	database &operator=(const database &) = delete;

	operator sqlite3 *();

	int get_last_id() const;

	~database();

private:
	sqlite3 *db_ = nullptr;
};

class statement
{
public:
	statement() = default;

	statement(const statement &) = delete;
	statement &operator=(const statement &) = delete;

	template
	<
		typename Value, typename ...Values
	>
	void bind(Value value, Values ...values)
	{
		reset();
		bind_value(1, value, values...);
	}

	template
	<
		typename ...Values
	>
	static void exec(sqlite3 *db, const char *sql, Values ...values)
	{
		statement stmt;

		stmt.prepare(db, sql);
		stmt.bind_value(1, values...);
		stmt.step();
	}

	int get_int(int index) const;
	const char *get_text(int index) const;

	void prepare(sqlite3 *db, const char *sql);
	void reset();
	bool step();

	~statement();

private:
	sqlite3_stmt *stmt_ = nullptr;

	void bind_value(int)
	{
	}

	void bind_value(int index, int value);
	void bind_value(int index, const char *value);
	void bind_value(int index, const std::string &value);

	template
	<
		typename Value
	>
	void bind_value(int index, const std::optional<Value> &value)
	{
		if(value)
		{
			bind(index, *value);
			return;
		}

		if(sqlite3_bind_null(stmt_, index))
		{
			throw exception{sqlite3_errmsg(sqlite3_db_handle(stmt_))};
		}
	}

	template
	<
		typename Value, typename ...Values
	>
	void bind_value(int index, Value value, Values ...values)
	{
		bind_value(index, value);
		bind_value(index + 1, values...);
	}
};

class transaction
{
public:
	explicit transaction(sqlite3 *db);

	transaction(const transaction &) = delete;
	transaction &operator=(const transaction &) = delete;

	~transaction();

private:
	sqlite3 *db_;
};
