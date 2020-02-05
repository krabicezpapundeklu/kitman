#include "kitman.hpp"

kitman::kitman(const char *db_path)
	: db_{db_path}
{
	transaction tx{db_};

	statement::exec(db_, "PRAGMA foreign_keys = ON");

	statement::exec(db_, R"(
		CREATE TABLE IF NOT EXISTS commits (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			parent INTEGER REFERENCES commits(id),
			merge_from INTEGER REFERENCES commits(id),
			comment TEXT,
			date TIMESTAMP DEFAULT CURRENT_TIMESTAMP
		)
	)");

	statement::exec(db_, R"(
		CREATE TABLE IF NOT EXISTS commit_files (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			commit_id INTEGER NOT NULL REFERENCES commits(id),
			seq INTEGER NOT NULL,
			path TEXT NOT NULL,
			is_delete INTEGER NOT NULL,

			UNIQUE(commit_id, seq),
			UNIQUE(commit_id, path)
		)
	)");

	statement::exec(db_, R"(
		CREATE TABLE IF NOT EXISTS config (
			name TEXT NOT NULL UNIQUE,
			value TEXT
		)
	)");

	statement::exec(db_, R"(
		CREATE TABLE IF NOT EXISTS streams (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			name TEXT NOT NULL UNIQUE,
			parent INTEGER REFERENCES streams(id),
			head INTEGER NOT NULL REFERENCES commits(id)
		)
	)");

	statement::exec(db_, R"(
		CREATE TABLE IF NOT EXISTS tags (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			name TEXT NOT NULL UNIQUE,
			commit_id INTEGER NOT NULL REFERENCES commits(id)
		)
	)");
}
