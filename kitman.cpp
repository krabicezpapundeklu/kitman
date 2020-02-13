#include "kitman.hpp"

#include <boost/format.hpp>

#include "catalog_generator.hpp"

kitman::kitman(const char *db_path)
	: db_{db_path}
{
	init_db();
	prepare_statements();
}

void kitman::commit_files(const std::string &stream, const std::string &comment, const std::vector<file> &files)
{
	transaction tx{db_};

	const auto head = get_head(stream);
	const auto commit_id = insert_commit_.exec(head, comment);

	for(auto i = 0u; i < files.size(); ++i)
	{
		insert_commit_file_.exec(commit_id, i, files[i].path, files[i].is_delete);
	}

	update_stream_.exec(commit_id, stream);
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

	const auto commit_id = insert_create_commit_.exec(parent_head, comment);

	insert_stream_.exec(name, parent_id, commit_id);

	if(!tag.empty())
	{
		insert_tag_.exec(tag, commit_id);
	}
}

void kitman::create_tag(const std::string &stream, const std::string &tag)
{
	insert_tag_for_stream_.exec(tag, stream);
}

void kitman::delete_stream(const std::string &name)
{
	transaction tx{db_};

	delete_tags_.exec(name);
	delete_stream_.exec(name);
}

std::vector<upgrade> kitman::get_catalog(const std::string &stream, std::vector<std::string> &paths)
{
	const auto head = get_head(stream);
	const auto &last_tag = get_last_tag(head);

	if(std::find(paths.cbegin(), paths.cend(), last_tag) == paths.cend())
	{
		paths.emplace_back(last_tag);
	}

	sort_tags(paths, last_tag);

	catalog_generator generator{*this, head};

	auto upgrades = generator.generate(paths);

	for(auto &upgrade : upgrades)
	{
		upgrade.is_release = true;
	}

	upgrades.back().is_release = false;

	return upgrades;
}

int kitman::get_commit(const std::string &tag)
{
	select_tag_commit_.bind(tag);
	select_tag_commit_.step();

	return select_tag_commit_.get_int(0);
}

std::vector<path_commit> kitman::get_commits(int head)
{
	std::vector<path_commit> commits;

	select_path_commits_.bind(head);

	while(select_path_commits_.step())
	{
		commits.emplace_back
		(
			select_path_commits_.get_int(0), select_path_commits_.get_int(1), select_path_commits_.get_int(2)
		);
	}

	return commits;
}

std::tuple<int, std::vector<commit>> kitman::get_commits(const std::string &stream, const std::string &sort, const std::string &order, int page, int page_size)
{
	statement *stmt;

	if(sort == "comment")
	{
		if(order == "asc")
		{
			stmt = &select_commits_comment_asc_;
		}
		else
		{
			stmt = &select_commits_comment_desc_;
		}
	}
	else
	{
		if(order == "asc")
		{
			stmt = &select_commits_id_asc_;
		}
		else
		{
			stmt = &select_commits_id_desc_;
		}
	}

	auto total = 0;
	std::vector<commit> commits;

	stmt->bind(stream, page_size, page * page_size);

	while(stmt->step())
	{
		total = stmt->get_int(0);

		auto &commit = commits.emplace_back(stmt->get_int(1), stmt->get_text(3), stmt->get_text(4));

		if(const auto merge_from = stmt->get_int(2))
		{
			commit.merge_from_tag = get_last_tag(merge_from);
		}

		commit.tags = get_tags(commit.id);
		commit.files = get_files(commit.id);
	}

	return {total, commits};
}

std::vector<file> kitman::get_files(int commit_id)
{
	std::vector<file> files;

	select_commit_files_.bind(commit_id);

	while(select_commit_files_.step())
	{
		files.emplace_back(select_commit_files_.get_text(0), select_commit_files_.get_int(1));
	}

	return files;
}

int kitman::get_head(const std::string &stream)
{
	select_stream_.exec(stream);
	return select_stream_.get_int(1);
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

std::vector<std::string> kitman::get_paths(const std::string &stream)
{
	std::vector<std::string> paths;

	const auto head = get_head(stream);

	select_paths_.bind(head);

	while(select_paths_.step())
	{
		paths.emplace_back(select_paths_.get_text(0));
	}

	sort_tags(paths);

	return paths;
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

std::vector<std::string> kitman::get_tags(int commit_id)
{
	std::vector<std::string> tags;

	select_tags_.bind(commit_id);

	while(select_tags_.step())
	{
		tags.emplace_back(select_tags_.get_text(0));
	}

	return tags;
}

std::tuple<int, const char *> kitman::get_version(const char *tag)
{
	auto version = 0;
	auto value = 0;

	for(; *tag; ++tag)
	{
		const auto c = *tag;

		if(c >= '0' && c <= '9')
		{
			value *= 10;
			value += c - '0';
			continue;
		}

		if(c == '.')
		{
			version = 100 * version + value;
			value = 0;
			continue;
		}

		return {version, tag};
	}

	return {value, tag};
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

void kitman::merge(const std::string &from, const std::string &to)
{
	transaction tx{db_};

	const auto from_head = get_head(from);
	const auto to_head = get_head(to);

	const auto &comment = (boost::format("Merge from %1%  (ID %2%)") % from % from_head).str();

	const auto commit_id = insert_merge_commit_.exec(to_head, from_head, comment);

	update_stream_.exec(commit_id, to);
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

	insert_commit_.prepare(db_, "INSERT INTO commits (parent, comment) VALUES (?, ?)");
	insert_commit_file_.prepare(db_, "INSERT INTO commit_files (commit_id, seq, path, is_delete) VALUES (?, ?, ?, ?)");
	insert_create_commit_.prepare(db_, "INSERT INTO commits (merge_from, comment) VALUES (?, ?)");
	insert_merge_commit_.prepare(db_, "INSERT INTO commits (parent, merge_from, comment) VALUES (?, ?, ?)");
	insert_stream_.prepare(db_, "INSERT INTO streams (name, parent, head) VALUES (?, ?, ?)");
	insert_tag_.prepare(db_, "INSERT INTO tags (name, commit_id) VALUES (?, ?)");
	insert_tag_for_stream_.prepare(db_, "INSERT INTO tags (name, commit_id) SELECT ?, head FROM streams WHERE name = ?");

	const auto select_commits = R"(
		WITH sorted_commits AS (
			WITH RECURSIVE in_path(id) AS (
				SELECT head FROM streams WHERE name = ?
				UNION ALL
				SELECT
					c.parent
				FROM
					commits c
					JOIN in_path ip ON (ip.id = c.id)
				WHERE
					c.parent IS NOT NULL
			)
			SELECT
				c.id, c.parent, c.merge_from, c.comment, c.date
			FROM
				in_path ip
				JOIN commits c ON (c.id = ip.id)
			ORDER BY
				c.%1% %2%
		)
		SELECT
			(SELECT COUNT(id) FROM sorted_commits), id, merge_from, comment, date
		FROM
			sorted_commits LIMIT ? OFFSET ?
	)";

	select_commits_comment_asc_.prepare(db_, (boost::format(select_commits) % "comment" % "asc").str().data());
	select_commits_comment_desc_.prepare(db_, (boost::format(select_commits) % "comment" % "desc").str().data());
	select_commits_id_asc_.prepare(db_, (boost::format(select_commits) % "id" % "asc").str().data());
	select_commits_id_desc_.prepare(db_, (boost::format(select_commits) % "id" % "desc").str().data());

	select_commit_files_.prepare(db_, "SELECT path, is_delete FROM commit_files WHERE commit_id = ? ORDER BY seq");

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

	select_paths_.prepare(db_, R"(
		WITH RECURSIVE in_path(id, parent, merge_from) AS (
			SELECT
				id, parent, merge_from
			FROM
				commits
			WHERE
				id = ?
			UNION
			SELECT
				c.id, c.parent, c.merge_from
			FROM
				commits c
				JOIN in_path ip ON (ip.parent = c.id OR ip.merge_from = c.id)
		)
		SELECT
			t.name
		FROM
			in_path ip
			JOIN tags t ON (t.commit_id = ip.id)
	)");

	select_path_commits_.prepare(db_, R"(
		WITH RECURSIVE in_path(id, parent, merge_from) AS (
			SELECT id, parent, merge_from FROM commits WHERE id = ?
			UNION
			SELECT
				c.id, c.parent, c.merge_from
			FROM
				commits c
				JOIN in_path ip ON (ip.parent = c.id OR ip.merge_from = c.id)
		)
		SELECT id, parent, merge_from FROM in_path
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

	select_tags_.prepare(db_, "SELECT name FROM tags WHERE commit_id = ? ORDER BY id");
	select_tag_commit_.prepare(db_, "SELECT commit_id FROM tags WHERE name = ?");
	update_stream_.prepare(db_, "UPDATE streams SET head = ? WHERE name = ?");
}

void kitman::sort_tags(std::vector<std::string> &tags, const std::string &last_tag)
{
	std::sort(tags.begin(), tags.end(), [last_tag](const std::string &x, const std::string &y)
	{
		if(x == last_tag)
		{
			return false;
		}

		if(y == last_tag)
		{
			return true;
		}

		const auto [x_prefix, x_rest] = get_version(x.data());
		const auto [y_prefix, y_rest] = get_version(y.data());

		if(x_prefix != y_prefix)
		{
			return x_prefix < y_prefix;
		}

		auto x_stream_length = 0u;

		while(x_rest[x_stream_length] && x_rest[x_stream_length] != '.')
		{
			++x_stream_length;
		}

		auto y_stream_length = 0u;

		while(y_rest[y_stream_length] && y_rest[y_stream_length] != '.')
		{
			++y_stream_length;
		}

		std::string_view x_stream{x_rest, x_stream_length};
		std::string_view y_stream{y_rest, y_stream_length};

		const auto r = x_stream.compare(y_stream);

		if(r != 0)
		{
			return r < 0;
		}

		const auto [x_suffix, x_rest_1] = get_version(x_rest + x_stream_length);
		const auto [y_suffix, y_rest_1] = get_version(y_rest + y_stream_length);

		return x_suffix < y_suffix;
	});
}
