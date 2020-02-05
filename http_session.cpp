#include "http_session.hpp"
#include "kitman.hpp"
#include "mime.hpp"

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = boost::beast::http;

http_session::http_session(asio::ip::tcp::socket &&socket, const std::string &web_root, kitman &kitman)
	: stream_{std::move(socket)}, web_root_{web_root}, kitman_{kitman}
{
}

void http_session::close()
{
	beast::error_code ec;
	stream_.socket().shutdown(asio::ip::tcp::socket::shutdown_send, ec);
}

bool http_session::handle_request(const request &request)
{
	if(!web_root_.empty() && handle_web_root(request))
	{
		return true;
	}

	return false;
}

bool http_session::handle_web_root(const request &request)
{
	auto url = request_.target();

	if(url == "/")
	{
		url = "/index.html";
	}

	if(url.find("..") != beast::string_view::npos)
	{
		send(http::status::bad_request);
		return true;
	}

	beast::error_code ec;
	http::file_body::value_type body;

	const auto &file_path = web_root_ + url.to_string();

	body.open(file_path.c_str(), beast::file_mode::scan, ec);

	if(ec)
	{
		if(ec == beast::errc::no_such_file_or_directory)
		{
			return false;
		}

		send(http::status::internal_server_error, ec.message());

		return true;
	}

	send(http::status::ok, std::move(body), get_mime_type(file_path));

	return true;
}

void http_session::on_read(const boost::beast::error_code &ec, std::size_t)
{
	if(ec)
	{
		if(ec == http::error::end_of_stream)
		{
			close();
		}

		return;
	}

	if(handle_request(request_))
	{
		return;
	}

	send(http::status::not_found);
}

void http_session::on_write(bool close, const beast::error_code &ec, std::size_t)
{
	if(ec)
	{
		if(close)
		{
			this->close();
		}

		return;
	}

	response_ = nullptr;

	read();
}

void http_session::read()
{
	request_ = {};
	http::async_read(stream_, buffer_, request_, beast::bind_front_handler(&http_session::on_read, shared_from_this()));
}

void http_session::run()
{
	read();
}

void http_session::send(http::status status, const std::string &body)
{
	http::response<http::string_body> response{status, request_.version()};

	response.keep_alive(request_.keep_alive());
	response.body() = body;
	response.prepare_payload();

	send(std::move(response));
}

void http_session::send(http::status status, http::file_body::value_type &&body, const char *content_type)
{
	const auto size = body.size();

	http::response<http::file_body> response{http::status::ok, request_.version()};

	response.body() = std::move(body);
	response.content_length(size);
	response.set(http::field::content_type, content_type);
	response.keep_alive(request_.keep_alive());

	send(std::move(response));
}
