#include "mime.hpp"

#include <boost/algorithm/string/predicate.hpp>

const char *get_mime_type(const std::string &file_name)
{
	if(boost::ends_with(file_name, ".css"))
	{
		return "text/css";
	}

	if(boost::ends_with(file_name, ".html"))
	{
		return "text/html";
	}

	if(boost::ends_with(file_name, ".ico"))
	{
		return "image/x-icon";
	}

	if(boost::ends_with(file_name, ".js"))
	{
		return "application/javascript";
	}

	return "text/plain";
}
