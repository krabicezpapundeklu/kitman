#pragma once

#include <vector>

#include "db.hpp"

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

class kitman
{
public:
	explicit kitman(const char *db_path);

	void create_stream(const std::string &name, const std::string &parent, const std::string &tag);
	void delete_stream(const std::string &name);
	std::string get_last_tag(int commit_id);
	std::vector<stream> get_streams();

private:
	database db_;

	statement delete_stream_;
	statement delete_tags_;
	statement insert_commit_;
	statement insert_stream_;
	statement insert_tag_;
	statement select_last_tag_;
	statement select_stream_;
	statement select_streams_;

	void init_db();
	void prepare_statements();
};
