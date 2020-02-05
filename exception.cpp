#include "exception.hpp"

exception::exception(const std::string &message)
	: message_{message}
{
}

const char *exception::what() const noexcept
{
	return message_.data();
}
