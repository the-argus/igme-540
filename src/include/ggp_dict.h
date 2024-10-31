#pragma once

#include <unordered_map>

namespace ggp
{
	template <typename T>
	using dict = std::unordered_map<std::string, T>;
}