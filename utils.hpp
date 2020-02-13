#pragma once

#include <ostream>
#include <string>
#include <vector>

struct upgrade;

std::tuple<int, const char *> get_version(const char *tag);
void sort_tags(std::vector<std::string> &tags, const std::string &last_tag = "");

std::ostream &operator<<(std::ostream &stream, const std::vector<upgrade> &upgrades);
