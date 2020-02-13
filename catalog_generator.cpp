#include "catalog_generator.hpp"

#include <queue>
#include <unordered_set>

#include <boost/format.hpp>

catalog_generator::catalog_generator(kitman &kitman, int head)
	: kitman_{kitman}, head_{head}, commits_{get_commits(head)}
{
}

std::vector<upgrade> catalog_generator::generate(const std::vector<std::string> &paths)
{
	std::vector<upgrade> upgrades;

	std::vector<int> replay_path;
	std::vector<script> scripts;

	for(const auto &p : paths)
	{
		const auto &upgrade_path = get_upgrade_path(p);

		replay_path.clear();
		scripts.clear();

		auto path = get_direct_path(upgrade_path.commit_id);
		const auto replay_from = path.size();

		path.insert(path.end(), upgrade_path.shortest_path.cbegin() + 1, upgrade_path.shortest_path.cend());
		replay(replay_path, path, replay_from, path.size());

		auto &upgrade = upgrades.emplace_back(upgrade_path.from);

		for(auto commit_id : replay_path)
		{
			update_scripts(upgrade.scripts, commit_id);
		}
	}

	return upgrades;
}

std::unordered_map<int, path_commit> catalog_generator::get_commits(int head)
{
	std::unordered_map<int, path_commit> commits;

	for(const auto &commit : kitman_.get_commits(head))
	{
		commits.emplace(std::make_pair(commit.id, commit));
	}

	return commits;
}

std::vector<int> catalog_generator::get_direct_path(int to)
{
	std::vector<int> path;

	int commit_id = to;

	do
	{
		path.emplace_back(commit_id);
		commit_id = commits_[commit_id].parent;
	}
	while(commit_id);

	std::reverse(path.begin(), path.end());

	return path;
}

const std::vector<file> &catalog_generator::get_files(int commit_id)
{
	auto it = files_.find(commit_id);

	if(it == files_.cend())
	{
		it = files_.emplace(commit_id, kitman_.get_files(commit_id)).first;
	}

	return it->second;
}

const std::string &catalog_generator::get_last_tag(int commit_id)
{
	auto it = tags_.find(commit_id);

	if(it == tags_.cend())
	{
		const auto &tag = kitman_.get_last_tag(commit_id);
		it = tags_.emplace(commit_id, tag.empty() ? "DELETED" : tag).first;
	}

	return it->second;
}

upgrade_path catalog_generator::get_upgrade_path(const std::string &path)
{
	upgrade_path upgrade_path{path, kitman_.get_commit(path)};

	std::unordered_map<int, int> from_to;
	std::unordered_set<int> visited;
	std::queue<int> work;;

	work.emplace(head_);

	while(!work.empty())
	{
		const auto &commit = commits_[work.front()];
		work.pop();

		if(!visited.emplace(commit.id).second)
		{
			continue;
		}

		if(commit.parent)
		{
			from_to[commit.parent] = commit.id;
			work.emplace(commit.parent);
		}

		if(commit.merge_from)
		{
			from_to[commit.merge_from] = commit.id;
			work.emplace(commit.merge_from);
		}

		if(from_to.find(upgrade_path.commit_id) != from_to.cend())
		{
			break;
		}
	}

	auto commit_id = upgrade_path.commit_id;

	do
	{
		upgrade_path.shortest_path.emplace_back(commit_id);
		commit_id = from_to[commit_id];
	}
	while(commit_id);

	return upgrade_path;
}

void catalog_generator::merge(std::vector<int> &replay_path, const std::vector<int> &from_path, const std::vector<int> &to_path, std::size_t current_index)
{
	for(int i = static_cast<int>(from_path.size()) - 2; i >= 0; --i)
	{
		const auto &from_commit = commits_[from_path[i]];

		for(int j = static_cast<int>(current_index) - 1; j >= 0; --j)
		{
			const auto &to_commit = commits_[to_path[j]];

			if(/* from_commit.merge_from == to_commit.id || */ to_commit.merge_from == from_commit.id)
			{
				replay(replay_path, from_path, i + 1, from_path.size());
				return;
			}
		}
	}

	replay(replay_path, from_path, 1, from_path.size());
}

void catalog_generator::replay(std::vector<int> &replay_path, const std::vector<int> &path, std::size_t from_index, std::size_t to_index)
{
	for(auto current_index = from_index; current_index < to_index; ++current_index)
	{
		const auto &commit = commits_[path[current_index]];

		if(commit.merge_from)
		{
			if(current_index > 0 && commit.merge_from == path[current_index - 1])
			{
				auto merge_from_path = get_direct_path(commit.id);
				merge(replay_path, merge_from_path, path, current_index);

				from_index = merge_from_path.size();
				merge_from_path.insert(merge_from_path.end(), path.cbegin() + current_index + 1, path.cend());

				replay(replay_path, merge_from_path, from_index, merge_from_path.size());
				return;
			}

			if(std::find(replay_path.cbegin(), replay_path.cend(), commit.merge_from) == replay_path.cend())
			{
				const auto &merge_from_path = get_direct_path(commit.merge_from);
				merge(replay_path, merge_from_path, path, current_index);
			}
		}

		replay_path.emplace_back(commit.id);
	}
}

void catalog_generator::update_scripts(std::vector<script> &scripts, int commit_id)
{
	const auto &files = get_files(commit_id);

	if(files.empty())
	{
		return;
	}

	const auto &tag = get_last_tag(commit_id);

	for(const auto &file : files)
	{
		auto it = std::find_if(scripts.begin(), scripts.end(), [file](const script &script)
		{
			return script.path == file.path;
		});

		if(!file.is_delete)
		{
			if(it == scripts.end())
			{
				scripts.emplace_back(file.path, (boost::format("from %1% (ID %2%)") % tag % commit_id).str());
			}
			else
			{
				it->comment += (boost::format(", %1% (ID %2%)") % tag % commit_id).str();
			}
		}
		else
		{
			if(it != scripts.end())
			{
				scripts.erase(it);
			}
		}
	}
}
