#pragma once

#include <string>

class exception : public std::exception
{
public:
	explicit exception(const std::string &message);

	const char *what() const noexcept override;

private:
	std::string message_;
};
