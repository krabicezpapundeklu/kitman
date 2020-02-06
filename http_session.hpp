#pragma once

#include <regex>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "json.hpp"

class kitman;

class http_session : public std::enable_shared_from_this<http_session>
{
public:
	http_session(boost::asio::ip::tcp::socket &&socket, const std::string &web_root, kitman &kitman);

	void run();

private:
	using handler = void(http_session::*)(const std::cmatch &, const nlohmann::json &);
	using route = std::tuple<handler, boost::beast::http::verb, std::regex>;

	static const route routes_[];

	boost::beast::tcp_stream stream_;
	std::string web_root_;
	kitman &kitman_;
	boost::beast::flat_buffer buffer_;
	boost::beast::http::request<boost::beast::http::string_body> request_;
	std::shared_ptr<void> response_;

	void close();
	bool handle_request();
	bool handle_rest();
	bool handle_web_root();
	void on_read(const boost::beast::error_code &ec, std::size_t);
	void on_write(bool close, const boost::beast::error_code &ec, std::size_t);
	void read();
	void send(boost::beast::http::status status, const std::string &body = "", const char *content_type = nullptr);
	void send(boost::beast::http::status status, boost::beast::http::file_body::value_type &&body, const char *content_type);
	void send_json(const nlohmann::json &response);

	void commit_files(const std::cmatch &params, const nlohmann::json &body);
	void create_stream(const std::cmatch &params, const nlohmann::json &body);
	void create_tag(const std::cmatch &params, const nlohmann::json &body);
	void delete_stream(const std::cmatch &params, const nlohmann::json &body);
	void get_catalog(const std::cmatch &params, const nlohmann::json &body);
	void get_commits(const std::cmatch &params, const nlohmann::json &body);
	void get_paths(const std::cmatch &params, const nlohmann::json &body);
	void get_streams(const std::cmatch &params, const nlohmann::json &body);
	void merge(const std::cmatch &params, const nlohmann::json &body);

	static std::map<std::string, std::string> parse_query_string(const std::string &query_string);

	template
	<
		typename Response
	>
	void send(Response &&response)
	{
		namespace beast = boost::beast;
		namespace http = boost::beast::http;

		auto response_ptr = std::make_shared<Response>(std::move(response));
		response_ = response_ptr;
		http::async_write(stream_, *response_ptr, beast::bind_front_handler(&http_session::on_write, shared_from_this(), response_ptr->need_eof()));
	}
};
