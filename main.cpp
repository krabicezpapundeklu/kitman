#include <iostream>

#include <boost/program_options.hpp>

#include "http_listener.hpp"
#include "kitman.hpp"

extern "C"
{
	int SQLITE_CDECL shell_main(int argc, char **argv);
}

namespace asio = boost::asio;
namespace po = boost::program_options;

int main(int argc, char **argv)
{
	std::string db_path;
	unsigned short port;
	std::string web_root;

	po::options_description options{"Options"};

	options.add_options()
		("db", po::value(&db_path)->default_value("kitman.db"), "database file to use")
		("port", po::value(&port)->default_value(8080), "port to listen on")
		("shell", "start sql shell")
		("web-root", po::value(&web_root)->default_value(""), "web-site root");

	po::variables_map vm;

	try
	{
		po::store(po::parse_command_line(argc, argv, options), vm);
		po::notify(vm);
	}
	catch(const std::exception &e)
	{
		std::cout << "Usage: kitman [options]\n\n" << options << '\n' << e.what() << '\n';
		return EXIT_FAILURE;
	}

	if(vm.find("shell") != vm.cend())
	{
		return shell_main(1, argv);
	}

	asio::io_context io;
	asio::signal_set signals{io, SIGINT, SIGTERM};

	signals.async_wait([&io](const std::error_code &, int)
	{
		io.stop();
	});

	kitman kitman{db_path.data()};

	http_listener http_listener{io, port, web_root, kitman};
	http_listener.run();

	std::cout << "Listening on port " << port << ", using " << db_path << " as database.\n";
	std::cout << "Press Ctrl-C to stop.\n";

	io.run();

	return EXIT_SUCCESS;
}
