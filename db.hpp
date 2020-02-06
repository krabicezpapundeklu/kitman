#pragma once

#include <optional>

#include "exception.hpp"
#include "sqlite3.h"

class database;

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
	void bind(Value &&value, Values &&...values)
	{
		reset();
		bind_values(1, value, values...);
	}

	template
	<
		typename ...Values
	>
	int exec(Values &&...values)
	{
		reset();
		bind_values(1, values...);
		step();

		return sqlite3_last_insert_rowid(sqlite3_db_handle(stmt_));
	}

	int get_int(int index) const;
	const char *get_text(int index) const;

	statement &prepare(sqlite3 *db, const char *sql);

	void reset();
	bool step();

	~statement();

private:
	sqlite3_stmt *stmt_ = nullptr;

	void bind_values(int)
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
			bind_value(index, *value);
		}
		else
		{
			check(sqlite3_bind_null(stmt_, index));
		}
	}

	template
	<
		typename Value, typename ...Values
	>
	void bind_values(int index, Value &&value, Values &&...values)
	{
		bind_value(index, value);
		bind_values(index + 1, values...);
	}

	void check(int result) const;
};

class transaction
{
public:
	explicit transaction(database &db);

	transaction(const transaction &) = delete;
	transaction &operator=(const transaction &) = delete;

	~transaction();

private:
	database &db_;
};

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
	friend class transaction;

	sqlite3 *db_ = nullptr;

	statement begin_transaction_;
	statement commit_transaction_;
	statement rollback_transaction_;
};
