#pragma once

#include "Ruby.h"
#include <string>

namespace Ruby
{
	llrm::ShaderProgram LoadRasterShader(std::string VertName, std::string FragName);

	void InitShaderCompilation();
	void FinishShaderCompilation();

}
