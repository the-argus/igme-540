#pragma once

#include "Mesh.h"
#include <fstream>

namespace ggp::MapParser
{
	std::vector<Mesh> parse(std::ifstream& file);
}