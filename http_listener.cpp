#include "http_listener.hpp"
#include "http_session.hpp"

namespace asio = boost::asio;
namespace beast = boost::beast;

http_listener::http_listener(asio::io_context &io, unsigned short port, const std::string &web_root, kitman &kitman)
	: io_{io}, acceptor_{io_}, web_root_{web_root}, kitman_{kitman}
{
	asio::ip::tcp::endpoint endpoint{asio::ip::make_address("0.0.0.0"), port};

	acceptor_.open(endpoint.protocol());
	acceptor_.set_option(asio::socket_base::reuse_address(true));
	acceptor_.bind(endpoint);
	acceptor_.listen(asio::socket_base::max_connections);
}

void http_listener::accept()
{
	acceptor_.async_accept(io_, beast::bind_front_handler(&http_listener::on_accept, this));
}

void http_listener::on_accept(const std::error_code &ec, boost::asio::ip::tcp::socket socket)
{
	if(ec)
	{
		return;
	}

	std::make_shared<http_session>(std::move(socket), web_root_, kitman_)->run();

	accept();
}

void http_listener::run()
{
	accept();
}
