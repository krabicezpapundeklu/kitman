#include "utils.hpp"

#include <algorithm>

#include "kitman.hpp"

std::tuple<int, const char *> get_version(const char *tag)
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

void sort_tags(std::vector<std::string> &tags, const std::string &last_tag)
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

std::ostream &operator<<(std::ostream &stream, const std::vector<upgrade> &upgrades)
{
	stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
	stream << "<upgrades>\n";

	auto first_upgrade = true;

	for(const auto &upgrade : upgrades)
	{
		if(first_upgrade)
		{
			first_upgrade = false;
		}
		else
		{
			stream << '\n';
		}

		stream << "\t<upgrade from=\"" << upgrade.from << "\" release=\"" << std::boolalpha << upgrade.is_release << "\">\n";

		auto first_comment = true;
		std::string last_comment;

		for(const auto &script : upgrade.scripts)
		{
			if(script.comment != last_comment)
			{
				if(first_comment)
				{
					first_comment = false;
				}
				else
				{
					stream << '\n';
				}

				last_comment = script.comment;
				stream << "\t\t<!-- " << script.comment << " -->\n";
			}

			stream << "\t\t<script>" << script.path << "</script>\n";
		}

		stream << "\t</upgrade>\n";
	}

	stream << "</upgrades>\n";

	return stream;
}
