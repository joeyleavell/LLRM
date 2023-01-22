#include <string>
#include "Utill.h"
#include <filesystem>
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include "Windows.h"
#endif

namespace Ruby
{
    std::string GetProgramPath()
    {
#ifdef _WIN32
        char ModulePath[1024];
        (void)GetModuleFileNameA(NULL, ModulePath, std::size(ModulePath));
        return ModulePath;
#endif
    }

    std::filesystem::path GetDefaultProgramRoot()
    {
        // Assume product is installed like this:
        // Root
        //      bin
        //          App.exe
        std::filesystem::path Exe = GetProgramPath();
        return Exe.parent_path().parent_path();
    }

    std::string GetDefaultCompiledShadersPath(Ruby::RenderingAPI API)
    {
        std::filesystem::path Root = GetDefaultProgramRoot();
        std::filesystem::path Shaders = Root / "Shaders" / "Built";

		if(API == RenderingAPI::Vulkan)
		{
            return (Shaders / "Vulkan").string();
		}
    }

    std::string GetDefaultShadersPath()
    {
        std::filesystem::path Root = GetDefaultProgramRoot();
        std::filesystem::path Shaders = Root / "Shaders";

        return Shaders.string();
    }

    void WriteBinaryFile(std::string Path, std::vector<uint32_t>& OutBytes)
    {
        std::ofstream VertStream(Path, std::ios_base::binary);
        for (uint32_t Data : OutBytes)
        {
            VertStream.write(reinterpret_cast<char*>(&Data), 4);
        }
    }

    bool LoadBinaryFile(std::string Path, std::vector<uint32_t>& OutBytes)
    {
        uint32_t TmpBuf[1024];

    	std::ifstream Stream(Path, std::ios_base::binary);

        if (!Stream.is_open())
        {
            return false;
        }

        while (Stream.good())
        {
            OutBytes.reserve(1024 + OutBytes.capacity());

            Stream.read(reinterpret_cast<char*>(TmpBuf), std::size(TmpBuf) * sizeof(uint32_t));
            for (auto Index = 0; Index < Stream.gcount() / sizeof(uint32_t); Index++)
            {
                OutBytes.push_back(TmpBuf[Index]);
            }
        }

        return true;
    }

}
