#pragma once

#include "Ruby.h"
#include <string>

namespace Ruby
{

	struct UberVar
	{
		std::string Name;
		uint32_t MinValue;
		uint32_t MaxValue;
	};

	llrm::ShaderProgram LoadRasterShader(std::string VertName, std::string FragName);

	void InitShaderCompilation();
	void FinishShaderCompilation();

	bool CompileRasterProgram(std::string VertName, std::string FragName, std::vector<UberVar> VertUbers, std::vector<UberVar> FragUbers);

}
