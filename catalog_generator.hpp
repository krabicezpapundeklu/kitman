#pragma once

#include <unordered_map>

#include "kitman.hpp"

class catalog_generator
{
public:
	catalog_generator(kitman &kitman, int head, int stop_commit);

	std::vector<upgrade> generate(const std::vector<std::string> &paths);

private:
	kitman &kitman_;
	int head_;
	int stop_commit_;

	std::unordered_map<int, path_commit> commits_;
	std::unordered_map<int, std::vector<file>> files_;
	std::unordered_map<int, std::string> tags_;

	std::unordered_map<int, path_commit> get_commits(int head);
	std::vector<int> get_direct_path(int to);
	std::vector<upgrade_path> get_upgrade_paths(const std::vector<std::string> &paths);

	const std::vector<file> &get_files(int commit_id);
	const std::string &get_last_tag(int commit_id);

	void merge(std::vector<int> &replay_path, const std::vector<int> &from_path, const std::vector<int> &to_path, std::size_t current_index);
	void replay(std::vector<int> &replay_path, const std::vector<int> &path, std::size_t from_index, std::size_t to_index);
	void update_scripts(std::vector<script> &scripts, int commit_id);
};
