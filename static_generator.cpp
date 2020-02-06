#include <filesystem>
#include <fstream>
#include <iostream>

#include <boost/beast/zlib/deflate_stream.hpp>

#include "mime.hpp"

namespace beast = boost::beast;
namespace fs = std::filesystem;
namespace zlib = boost::beast::zlib;

int main(int argc, char **argv)
{
	if(argc != 3)
	{
		return EXIT_FAILURE;
	}

	std::ofstream output{argv[1]};

	output << "#pragma once\n\n#include <array>\n";

	std::vector<fs::path> files;

	for(fs::directory_iterator i{argv[2]}, end; i != end; ++i)
	{
		files.emplace_back(i->path());
	}

	std::sort(files.begin(), files.end());

	for(auto file_index = 0u; file_index < files.size(); ++file_index)
	{
		const auto &name = files[file_index].filename();

		output << "\n// " << name << '\n';
		output << "const unsigned char static_file_" << file_index << "_body[]\n{\n\t";

		std::ifstream input{files[file_index], std::ios_base::binary};

		std::vector<char> content
		{
			std::istreambuf_iterator{input}, {}
		};

		const auto compressed_size = zlib::deflate_upper_bound(content.size());

		std::vector<unsigned char> compressed;
		compressed.resize(compressed_size);

		zlib::deflate_stream stream;
		zlib::z_params params;

		params.avail_in = content.size();
		params.avail_out = compressed_size;
		params.next_in = content.data();
		params.next_out = compressed.data();

		beast::error_code ec;

		stream.write(params, zlib::Flush::finish, ec);

		if(ec && ec != zlib::error::end_of_stream)
		{
			std::cerr << ec.message() << '\n';
			return EXIT_FAILURE;
		}

		output << std::hex;

		for(auto i = 0u; i < params.total_out; ++i)
		{
			output << "0x" << std::setfill('0') << std::setw(2) << static_cast<unsigned int>(compressed[i]) << ",";

			if(i < params.total_out - 1)
			{
				if((i + 1) % 20)
				{
					output << ' ';
				}
				else
				{
					output << "\n\t";
				}
			}
		}

		output << "\n};\n\n";

		output << std::dec;

		output << "const std::size_t static_file_" << file_index << "_size = " << params.total_out << ";\n";
		output << "const char static_file_" << file_index << "_type[] = \"";
		output << get_mime_type(name.generic_string());
		output << "\";\n";
	}

	output << "\nstruct static_file\n{\n\tconst char *name;\n\tconst unsigned char *body;\n\tconst std::size_t size;\n\tconst char *type;\n};\n";

	output << "\nconst std::array<static_file, " << files.size() << "> static_files\n{\n";

	for(auto file_index = 0u; file_index < files.size(); ++file_index)
	{
		output << "\tstatic_file{" << files[file_index].filename() << ", static_file_" << file_index
			<< "_body, static_file_" << file_index << "_size, static_file_" << file_index << "_type},\n";
	}

	output << "};\n";

	return EXIT_SUCCESS;
}
