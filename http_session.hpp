#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

class kitman;

class http_session : public std::enable_shared_from_this<http_session>
{
public:
	http_session(boost::asio::ip::tcp::socket &&socket, const std::string &web_root, kitman &kitman);

	void run();

private:
	using request = boost::beast::http::request<boost::beast::http::string_body>;

	boost::beast::tcp_stream stream_;
	std::string web_root_;
	kitman &kitman_;
	boost::beast::flat_buffer buffer_;
	request request_;
	std::shared_ptr<void> response_;

	void close();
	bool handle_request(const request &request);
	bool handle_web_root(const request &request);
	void on_read(const boost::beast::error_code &ec, std::size_t);
	void on_write(bool close, const boost::beast::error_code &ec, std::size_t);
	void read();
	void send(boost::beast::http::status status, const std::string &body = "");
	void send(boost::beast::http::status status, boost::beast::http::file_body::value_type &&body, const char *content_type);

	template
	<
		typename Response
	>
	void send(Response &&response)
	{
		auto response_ptr = std::make_shared<Response>(std::move(response));
		response_ = response_ptr;
		http::async_write(stream_, *response_ptr, beast::bind_front_handler(&http_session::on_write, shared_from_this(), response_ptr->need_eof()));
	}
};
