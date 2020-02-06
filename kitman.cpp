#include "kitman.hpp"

#include <boost/format.hpp>

kitman::kitman(const char *db_path)
	: db_{db_path}
{
	init_db();
	prepare_statements();
}

void kitman::create_stream(const std::string &name, const std::string &parent, const std::string &tag)
{
	transaction tx{db_};

	std::optional<int> parent_id;
	std::optional<int> parent_head;
	std::string comment;

	if(!parent.empty())
	{
		select_stream_.exec(parent);

		parent_id = select_stream_.get_int(0);
		parent_head = select_stream_.get_int(1);

		comment = (boost::format("Add stream (from %1%, ID %2%)") % parent % *parent_head).str();
	}
	else
	{
		comment = "Add stream";
	}

	const auto commit_id = insert_commit_.exec(parent_head, comment);

	insert_stream_.exec(name, parent_id, commit_id);

	if(!tag.empty())
	{
		insert_tag_.exec(tag, commit_id);
	}
}

void kitman::delete_stream(const std::string &name)
{
	transaction tx{db_};

	delete_tags_.exec(name);
	delete_stream_.exec(name);
}

void kitman::init_db()
{
	transaction tx{db_};

	statement stmt;

	stmt.prepare(db_, "PRAGMA foreign_keys = ON").exec();

	stmt.prepare(db_, R"(
		CREATE TABLE IF NOT EXISTS commits (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			parent INTEGER REFERENCES commits(id),
			merge_from INTEGER REFERENCES commits(id),
			comment TEXT,
			date TIMESTAMP DEFAULT CURRENT_TIMESTAMP
		)
	)").exec();

	stmt.prepare(db_, R"(
		CREATE TABLE IF NOT EXISTS commit_files (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			commit_id INTEGER NOT NULL REFERENCES commits(id),
			seq INTEGER NOT NULL,
			path TEXT NOT NULL,
			is_delete INTEGER NOT NULL,

			UNIQUE(commit_id, seq),
			UNIQUE(commit_id, path)
		)
	)").exec();

	stmt.prepare(db_, R"(
		CREATE TABLE IF NOT EXISTS streams (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			name TEXT NOT NULL UNIQUE,
			parent INTEGER REFERENCES streams(id),
			head INTEGER NOT NULL REFERENCES commits(id)
		)
	)").exec();

	stmt.prepare(db_, R"(
		CREATE TABLE IF NOT EXISTS tags (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			name TEXT NOT NULL UNIQUE,
			commit_id INTEGER NOT NULL REFERENCES commits(id)
		)
	)").exec();
}

std::string kitman::get_last_tag(int commit_id)
{
	select_last_tag_.bind(commit_id);

	if(select_last_tag_.step())
	{
		return select_last_tag_.get_text(0);
	}

	return {};
}

std::vector<stream> kitman::get_streams()
{
	std::vector<stream> streams;

	select_streams_.reset();

	while(select_streams_.step())
	{
		const auto id = select_streams_.get_int(0);
		const auto name = select_streams_.get_text(1);
		const auto head = select_streams_.get_int(2);
		const auto parent = select_streams_.get_text(3);
		const auto child = select_streams_.get_text(4);

		if(streams.empty() || streams.back().id != id)
		{
			auto &stream = streams.emplace_back(id, name, get_last_tag(head));

			if(parent)
			{
				stream.parent = parent;
			}

			if(child)
			{
				stream.children.emplace_back(child);
			}
		}
		else
		{
			streams.back().children.emplace_back(child);
		}
	}

	return streams;
}

void kitman::prepare_statements()
{
	delete_stream_.prepare(db_, "DELETE FROM streams WHERE name = ?");

	delete_tags_.prepare(db_, R"(
		WITH RECURSIVE in_path(commit_id, tag) AS (
			SELECT head, NULL FROM streams WHERE name = ?
			UNION ALL
			SELECT
				c.parent, t.name
			FROM
				commits c
				JOIN in_path ip ON (ip.commit_id = c.id)
				LEFT JOIN tags t on (t.commit_id = c.id)
		)
		DELETE FROM tags WHERE name IN (
			SELECT tag FROM in_path
		)
	)");

	insert_commit_.prepare(db_, "INSERT INTO commits (merge_from, comment) VALUES (?, ?)");
	insert_stream_.prepare(db_, "INSERT INTO streams (name, parent, head) VALUES (?, ?, ?)");
	insert_tag_.prepare(db_, "INSERT INTO tags (name, commit_id) VALUES (?, ?)");

	select_last_tag_.prepare(db_, R"(
		WITH RECURSIVE in_path(commit_id, tag_id, tag) AS (
			SELECT ?, NULL, NULL
			UNION ALL
			SELECT
				c.parent, t.id, t.name
			FROM
				commits c
				JOIN in_path ip ON (ip.commit_id = c.id)
				LEFT JOIN tags t ON (t.commit_id = c.id)
			WHERE
				ip.tag IS NULL
		)
		SELECT tag FROM in_path WHERE tag IS NOT NULL ORDER BY tag_id DESC LIMIT 1
	)");

	select_stream_.prepare(db_, "SELECT id, head FROM streams WHERE name = ?");

	select_streams_.prepare(db_, R"(
		SELECT
			s.id, s.name, s.head, ps.name, cs.name
		FROM
			streams s
			LEFT JOIN streams ps ON (ps.id = s.parent)
			LEFT JOIN streams cs ON (cs.parent = s.id)
		ORDER BY
			s.name, cs.name
	)");
}
