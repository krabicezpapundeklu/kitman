#include "http_session.hpp"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>

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
	{&http_session::get_catalog, http::verb::get, std::regex{"/streams/([^/]+)/catalog(\\?.*)?"}},
	{&http_session::get_commits, http::verb::get, std::regex{"/streams/([^/]+)/commits(\\?.*)?"}},
	{&http_session::get_paths, http::verb::get, std::regex{"/streams/([^/]+)/paths"}},
	{&http_session::get_streams, http::verb::get, std::regex{"/streams"}},
	{&http_session::merge, http::verb::post, std::regex{"/streams/([^/]+)/merge"}}
};

template
<
	typename T
>
T get_query_param(const std::map<std::string, std::string> &params, const std::string &name, T default_value)
{
	const auto &it = params.find(name);

	if(it == params.cend())
	{
		return default_value;
	}

	T value;

	if(boost::conversion::try_lexical_convert(it->second, value))
	{
		return value;
	}

	return default_value;
}

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
	const auto &stream = params[1];
	const auto &comment = body.at("comment").get<std::string>();

	std::vector<file> files;

	for(const auto &file_json : body.at("files"))
	{
		files.emplace_back(file_json.at("path").get<std::string>(), file_json.value("delete", false));
	}

	kitman_.commit_files(stream, comment, files);

	send(http::status::created);
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
	const auto &stream = params[1];
	const auto &tag = body.at("name").get<std::string>();

	kitman_.create_tag(stream, tag);

	send(http::status::created);
}

void http_session::delete_stream(const std::cmatch &params, const nlohmann::json &body)
{
	kitman_.delete_stream(params[1]);
	send(http::status::ok);
}

void http_session::get_catalog(const std::cmatch &params, const nlohmann::json &body)
{
	const auto &stream = params[1];
	const auto &query_params = parse_query_string(params[2]);
	const auto &paths_param = get_query_param<std::string>(query_params, "paths", "");

	std::vector<std::string> paths;

	if(!paths_param.empty())
	{
		boost::split(paths, paths_param, boost::is_any_of(","));
	}

	const auto &upgrades = kitman_.get_catalog(stream, paths);

	std::stringstream ss;

	ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
	ss << "<upgrades>\n";

	auto first_upgrade = true;

	for(const auto &upgrade : upgrades)
	{
		if(first_upgrade)
		{
			first_upgrade = false;
		}
		else
		{
			ss << '\n';
		}

		ss << "\t<upgrade from=\"" << upgrade.from << "\" release=\"" << std::boolalpha << upgrade.is_release << "\">\n";

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
					ss << '\n';
				}

				last_comment = script.comment;
				ss << "\t\t<!-- " << script.comment << " -->\n";
			}

			ss << "\t\t<script>" << script.path << "</script>\n";
		}

		ss << "\t</upgrade>\n";
	}

	ss << "</upgrades>\n";

	http::response<http::string_body> response{http::status::ok, request_.version()};

	response.keep_alive(request_.keep_alive());
	response.set(http::field::content_disposition, "attachment; filename=\"catalogue.xml\"");
	response.set(http::field::content_type, "text/xml");
	response.body() = ss.str();
	response.prepare_payload();

	send(std::move(response));
}

void http_session::get_commits(const std::cmatch &params, const nlohmann::json &body)
{
	const auto &stream = params[1];
	const auto &query_params = parse_query_string(params[2]);
	const auto &sort = get_query_param<std::string>(query_params, "sort", "id");
	const auto &order = get_query_param<std::string>(query_params, "order", "desc");
	const auto page = get_query_param(query_params, "page", 0);
	const auto page_size = get_query_param(query_params, "pageSize", std::numeric_limits<int>::max());

	const auto &[total, commits] = kitman_.get_commits(stream, sort, order, page, page_size);

	auto response = json::object();

	response["total"] = total;
	auto &commits_json = response["commits"] = json::array();

	for(const auto &commit : commits)
	{
		auto &commit_json = commits_json.emplace_back(json::object());

		commit_json["id"] = commit.id;
		commit_json["comment"] = commit.comment;
		commit_json["date"] = commit.date;

		auto &tags_json = commit_json["tags"] = json::array();

		for(const auto &tag : commit.tags)
		{
			tags_json.emplace_back(tag);
		}

		commit_json["mergeFromTag"] = commit.merge_from_tag;

		auto &files_json = commit_json["files"] = json::array();

		for(const auto &file : commit.files)
		{
			auto &file_json = files_json.emplace_back(json::object());

			file_json["path"] = file.path;
			file_json["delete"] = file.is_delete;
		}
	}

	send_json(response);
}

void http_session::get_paths(const std::cmatch &params, const nlohmann::json &body)
{
	const auto &stream = params[1];
	const auto &paths = kitman_.get_paths(stream);

	auto response = json::array();

	for(const auto &path : paths)
	{
		response.emplace_back(path);
	}

	send_json(response);
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
	const auto &stream = params[1];
	const auto &from = body.at("from").get<std::string>();

	kitman_.merge(from, stream);

	send(http::status::ok);
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

std::map<std::string, std::string> http_session::parse_query_string(const std::string &query_string)
{
	std::map<std::string, std::string> params;

	std::string key;
	std::string value;

	auto in_key = true;

	for(auto i = 1u; i < query_string.size(); ++i)
	{
		auto c = query_string[i];

		switch(c)
		{
		case '&':
			params.emplace(std::make_pair(key, value));
			key.clear();
			value.clear();
			in_key = true;
			break;

		case '=':
			in_key = false;
			break;

		default:
			if(in_key)
			{
				key += c;
			}
			else
			{
				value += c;
			}

			break;
		}
	}

	if(!key.empty())
	{
		params.emplace(std::make_pair(key, value));
	}

	return params;
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
