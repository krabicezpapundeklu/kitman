#include "http_session.hpp"
#include "kitman.hpp"
#include "mime.hpp"

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = boost::beast::http;

using json = nlohmann::json;

const http_session::route http_session::routes_[]
{
	{&http_session::commit_files, http::verb::post, std::regex{"/streams/([^/]+)/commits"}},
	{&http_session::create_stream, http::verb::post, std::regex{"/streams"}},
	{&http_session::create_tag, http::verb::post, std::regex{"/streams/([^/]+)/tags"}},
	{&http_session::delete_stream, http::verb::delete_, std::regex{"/streams/([^/]+)"}},
	{&http_session::get_catalog, http::verb::get, std::regex{"/streams/([^/]+)/catalog"}},
	{&http_session::get_commits, http::verb::get, std::regex{"/streams/([^/]+)/commits"}},
	{&http_session::get_paths, http::verb::get, std::regex{"/streams/([^/]+)/paths"}},
	{&http_session::get_streams, http::verb::get, std::regex{"/streams"}},
	{&http_session::merge, http::verb::post, std::regex{"/streams/([^/]+)/merge"}}
};

http_session::http_session(asio::ip::tcp::socket &&socket, const std::string &web_root, kitman &kitman)
	: stream_{std::move(socket)}, web_root_{web_root}, kitman_{kitman}
{
}

void http_session::close()
{
	beast::error_code ec;
	stream_.socket().shutdown(asio::ip::tcp::socket::shutdown_send, ec);
}

void http_session::commit_files(const std::cmatch &params, const nlohmann::json &body)
{
}

void http_session::create_stream(const std::cmatch &params, const nlohmann::json &body)
{
	const auto &name = body.at("name").get<std::string>();
	const auto &parent = body.value("parent", "");
	const auto &tag = body.at("tag").get<std::string>();

	kitman_.create_stream(name, parent, tag);

	send(http::status::created);
}

void http_session::create_tag(const std::cmatch &params, const nlohmann::json &body)
{

}

void http_session::delete_stream(const std::cmatch &params, const nlohmann::json &body)
{
	kitman_.delete_stream(params[1]);
	send(http::status::ok);
}

void http_session::get_catalog(const std::cmatch &params, const nlohmann::json &body)
{

}

void http_session::get_commits(const std::cmatch &params, const nlohmann::json &body)
{

}

void http_session::get_paths(const std::cmatch &params, const nlohmann::json &body)
{

}

void http_session::get_streams(const std::cmatch &params, const nlohmann::json &body)
{
	const auto &streams = kitman_.get_streams();

	auto response = json::array();

	for(const auto &stream : streams)
	{
		auto &stream_json = response.emplace_back(json::object());

		stream_json["name"] = stream.name;
		stream_json["tag"] = stream.tag;
		stream_json["parent"] = stream.parent;

		auto &children_json = stream_json["children"] = json::array();

		for(const auto &child : stream.children)
		{
			children_json.emplace_back(child);
		}
	}

	send_json(response);
}

bool http_session::handle_request()
{
	if(handle_rest())
	{
		return true;
	}

	if(!web_root_.empty() && handle_web_root())
	{
		return true;
	}

	return false;
}

bool http_session::handle_rest()
{
	const auto &url = request_.target();

	std::cmatch params;

	for(const auto &[handler, method, pattern] : routes_)
	{
		if(request_.method() != method)
		{
			continue;
		}

		if(!std::regex_match(url.cbegin(), url.cend(), params, pattern))
		{
			continue;
		}

		try
		{
			json body;

			if(request_.method() == http::verb::post && !request_.body().empty())
			{
				body = json::parse(request_.body());
			}

			(this->*handler)(params, body);
		}
		catch(const json::exception &e)
		{
			send(http::status::bad_request, e.what());
		}
		catch(const std::exception &e)
		{
			send(http::status::internal_server_error, e.what());
		}

		return true;
	}

	return false;
}

bool http_session::handle_web_root()
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

void http_session::merge(const std::cmatch &params, const nlohmann::json &body)
{

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

	if(!handle_request())
	{
		send(http::status::not_found);
	}
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

void http_session::send(http::status status, const std::string &body, const char *content_type)
{
	http::response<http::string_body> response{status, request_.version()};

	response.keep_alive(request_.keep_alive());
	response.set(http::field::content_type, content_type ? content_type : "text/plain");
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

void http_session::send_json(const nlohmann::json &response)
{
	send(http::status::ok, response.dump(2), "application/json");
}
