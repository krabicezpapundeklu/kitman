#pragma once

#include <vector>

#include "db.hpp"

struct file;

struct commit
{
	int id;
	std::string comment;
	std::string date;
	std::vector<std::string> tags;
	std::string merge_from_tag;
	std::vector<file> files;

	commit(int id, const std::string &comment, const std::string &date)
		: id{id}, comment{comment}, date{date}
	{
	}
};

struct file
{
	std::string path;
	bool is_delete;

	file(const std::string &path, bool is_delete)
		: path{path}, is_delete{is_delete}
	{
	}
};

struct path_commit
{
	int id;
	int parent;
	int merge_from;

	path_commit() = default;

	path_commit(int id, int parent, int merge_from)
		: id{id}, parent{parent}, merge_from{merge_from}
	{
	}
};

struct script
{
	std::string path;
	std::string comment;

	script(const std::string &path, const std::string &comment)
		: path{path}, comment{comment}
	{
	}
};

struct stream
{
	int id;
	std::string name;
	std::string tag;
	std::string parent;
	std::vector<std::string> children;

	stream(int id, const std::string &name, const std::string &tag)
		: id{id}, name{name}, tag{tag}
	{
	}
};

struct upgrade
{
	std::string from;
	bool is_release;
	std::vector<script> scripts;

	upgrade(const std::string &from)
		: from{from}
	{
	}
};

struct upgrade_path
{
	std::string from;
	int commit_id;
	std::vector<int> shortest_path;

	upgrade_path(const std::string &from, int commit_id)
		: from{from}, commit_id{commit_id}
	{
	}
};

class kitman
{
public:
	explicit kitman(const char *db_path);

	void commit_files(const std::string &stream, const std::string &comment, const std::vector<file> &files);
	void create_stream(const std::string &name, const std::string &parent, const std::string &tag);
	void create_tag(const std::string &stream, const std::string &tag);
	void delete_stream(const std::string &name);
	std::vector<upgrade> get_catalog(const std::string &stream, std::vector<std::string> &paths);
	int get_commit(const std::string &tag);
	std::vector<path_commit> get_commits(int head);
	std::tuple<int, std::vector<commit>> get_commits(const std::string &stream, const std::string &sort, const std::string &order, int page, int page_size);
	std::vector<file> get_files(int commit_id);
	int get_head(const std::string &stream);
	std::string get_last_tag(int commit_id);
	std::vector<std::string> get_paths(const std::string &stream);
	std::vector<stream> get_streams();
	std::vector<std::string> get_tags(int commit_id);
	void merge(const std::string &from, const std::string &to);

private:
	database db_;

	statement delete_stream_;
	statement delete_tags_;
	statement insert_commit_;
	statement insert_commit_file_;
	statement insert_create_commit_;
	statement insert_merge_commit_;
	statement insert_stream_;
	statement insert_tag_;
	statement insert_tag_for_stream_;
	statement select_commits_comment_asc_;
	statement select_commits_comment_desc_;
	statement select_commits_id_asc_;
	statement select_commits_id_desc_;
	statement select_commit_files_;
	statement select_last_tag_;
	statement select_paths_;
	statement select_path_commits_;
	statement select_stream_;
	statement select_streams_;
	statement select_tags_;
	statement select_tag_commit_;
	statement update_stream_;

	static std::tuple<int, const char *> get_version(const char *tag);
	static void sort_tags(std::vector<std::string> &tags, const std::string &last_tag = "");

	void init_db();
	void prepare_statements();
};
