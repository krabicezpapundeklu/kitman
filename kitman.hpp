#pragma once

#include "db.hpp"

class kitman
{
public:
	explicit kitman(const char *db_path);

private:
	database db_;
};
