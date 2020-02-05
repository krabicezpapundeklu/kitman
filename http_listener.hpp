#pragma once

#include <boost/asio.hpp>

class kitman;

class http_listener
{
public:
	http_listener(boost::asio::io_context &io, unsigned short port, const std::string &web_root, kitman &kitman);

	void run();

private:
	boost::asio::io_context &io_;
	boost::asio::ip::tcp::acceptor acceptor_;
	std::string web_root_;
	kitman &kitman_;

	void accept();
	void on_accept(const std::error_code &ec, boost::asio::ip::tcp::socket socket);
};
