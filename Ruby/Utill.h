#pragma once
#include "Ruby.h"

namespace Ruby
{
	std::string GetProgramPath();

	std::string GetDefaultCompiledShadersPath(Ruby::RenderingAPI API);
	std::string GetDefaultShadersPath();

	void WriteBinaryFile(std::string Path, std::vector<uint32_t>& OutBytes);
	bool LoadBinaryFile(std::string Path, std::vector<uint32_t>& OutBytes);

}
