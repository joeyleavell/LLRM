#pragma once

#include <vector>
#include <string>

struct ShaderCompileResult
{
    std::vector<uint32_t> OutVertShader;
    std::vector<uint32_t> OutFragShader;
};

void InitShaderCompilation();
void FinishShaderCompilation();
bool CompileShader(std::string Vert, std::string Frag, ShaderCompileResult& OutResult);