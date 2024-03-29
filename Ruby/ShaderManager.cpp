#include "ShaderManager.h"
#include <filesystem>
#include <fstream>
#include <iostream>

#include "Utill.h"
#include "glslang/Include/ResourceLimits.h"
#include "glslang/Public/ShaderLang.h"
#include "SPIRV/GlslangToSpv.h"
#include "SPIRV/SpvTools.h"

const TBuiltInResource DefaultTBuiltInResource = {
    /* .MaxLights = */ 32,
    /* .MaxClipPlanes = */ 6,
    /* .MaxTextureUnits = */ 32,
    /* .MaxTextureCoords = */ 32,
    /* .MaxVertexAttribs = */ 64,
    /* .MaxVertexUniformComponents = */ 4096,
    /* .MaxVaryingFloats = */ 64,
    /* .MaxVertexTextureImageUnits = */ 32,
    /* .MaxCombinedTextureImageUnits = */ 80,
    /* .MaxTextureImageUnits = */ 32,
    /* .MaxFragmentUniformComponents = */ 4096,
    /* .MaxDrawBuffers = */ 32,
    /* .MaxVertexUniformVectors = */ 128,
    /* .MaxVaryingVectors = */ 8,
    /* .MaxFragmentUniformVectors = */ 16,
    /* .MaxVertexOutputVectors = */ 16,
    /* .MaxFragmentInputVectors = */ 15,
    /* .MinProgramTexelOffset = */ -8,
    /* .MaxProgramTexelOffset = */ 7,
    /* .MaxClipDistances = */ 8,
    /* .MaxComputeWorkGroupCountX = */ 65535,
    /* .MaxComputeWorkGroupCountY = */ 65535,
    /* .MaxComputeWorkGroupCountZ = */ 65535,
    /* .MaxComputeWorkGroupSizeX = */ 1024,
    /* .MaxComputeWorkGroupSizeY = */ 1024,
    /* .MaxComputeWorkGroupSizeZ = */ 64,
    /* .MaxComputeUniformComponents = */ 1024,
    /* .MaxComputeTextureImageUnits = */ 16,
    /* .MaxComputeImageUniforms = */ 8,
    /* .MaxComputeAtomicCounters = */ 8,
    /* .MaxComputeAtomicCounterBuffers = */ 1,
    /* .MaxVaryingComponents = */ 60,
    /* .MaxVertexOutputComponents = */ 64,
    /* .MaxGeometryInputComponents = */ 64,
    /* .MaxGeometryOutputComponents = */ 128,
    /* .MaxFragmentInputComponents = */ 128,
    /* .MaxImageUnits = */ 8,
    /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
    /* .MaxCombinedShaderOutputResources = */ 8,
    /* .MaxImageSamples = */ 0,
    /* .MaxVertexImageUniforms = */ 0,
    /* .MaxTessControlImageUniforms = */ 0,
    /* .MaxTessEvaluationImageUniforms = */ 0,
    /* .MaxGeometryImageUniforms = */ 0,
    /* .MaxFragmentImageUniforms = */ 8,
    /* .MaxCombinedImageUniforms = */ 8,
    /* .MaxGeometryTextureImageUnits = */ 16,
    /* .MaxGeometryOutputVertices = */ 256,
    /* .MaxGeometryTotalOutputComponents = */ 1024,
    /* .MaxGeometryUniformComponents = */ 1024,
    /* .MaxGeometryVaryingComponents = */ 64,
    /* .MaxTessControlInputComponents = */ 128,
    /* .MaxTessControlOutputComponents = */ 128,
    /* .MaxTessControlTextureImageUnits = */ 16,
    /* .MaxTessControlUniformComponents = */ 1024,
    /* .MaxTessControlTotalOutputComponents = */ 4096,
    /* .MaxTessEvaluationInputComponents = */ 128,
    /* .MaxTessEvaluationOutputComponents = */ 128,
    /* .MaxTessEvaluationTextureImageUnits = */ 16,
    /* .MaxTessEvaluationUniformComponents = */ 1024,
    /* .MaxTessPatchComponents = */ 120,
    /* .MaxPatchVertices = */ 32,
    /* .MaxTessGenLevel = */ 64,
    /* .MaxViewports = */ 16,
    /* .MaxVertexAtomicCounters = */ 0,
    /* .MaxTessControlAtomicCounters = */ 0,
    /* .MaxTessEvaluationAtomicCounters = */ 0,
    /* .MaxGeometryAtomicCounters = */ 0,
    /* .MaxFragmentAtomicCounters = */ 8,
    /* .MaxCombinedAtomicCounters = */ 8,
    /* .MaxAtomicCounterBindings = */ 1,
    /* .MaxVertexAtomicCounterBuffers = */ 0,
    /* .MaxTessControlAtomicCounterBuffers = */ 0,
    /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
    /* .MaxGeometryAtomicCounterBuffers = */ 0,
    /* .MaxFragmentAtomicCounterBuffers = */ 1,
    /* .MaxCombinedAtomicCounterBuffers = */ 1,
    /* .MaxAtomicCounterBufferSize = */ 16384,
    /* .MaxTransformFeedbackBuffers = */ 4,
    /* .MaxTransformFeedbackInterleavedComponents = */ 64,
    /* .MaxCullDistances = */ 8,
    /* .MaxCombinedClipAndCullDistances = */ 8,
    /* .MaxSamples = */ 4,
    /* .maxMeshOutputVerticesNV = */ 256,
    /* .maxMeshOutputPrimitivesNV = */ 512,
    /* .maxMeshWorkGroupSizeX_NV = */ 32,
    /* .maxMeshWorkGroupSizeY_NV = */ 1,
    /* .maxMeshWorkGroupSizeZ_NV = */ 1,
    /* .maxTaskWorkGroupSizeX_NV = */ 32,
    /* .maxTaskWorkGroupSizeY_NV = */ 1,
    /* .maxTaskWorkGroupSizeZ_NV = */ 1,
    /* .maxMeshViewCountNV = */ 4,
    /* .maxMeshOutputVerticesEXT = */ 256,
    /* .maxMeshOutputPrimitivesEXT = */ 256,
    /* .maxMeshWorkGroupSizeX_EXT = */ 128,
    /* .maxMeshWorkGroupSizeY_EXT = */ 128,
    /* .maxMeshWorkGroupSizeZ_EXT = */ 128,
    /* .maxTaskWorkGroupSizeX_EXT = */ 128,
    /* .maxTaskWorkGroupSizeY_EXT = */ 128,
    /* .maxTaskWorkGroupSizeZ_EXT = */ 128,
    /* .maxMeshViewCountEXT = */ 4,
    /* .maxDualSourceDrawBuffersEXT = */ 1,

    /* .limits = */ {
        /* .nonInductiveForLoops = */ 1,
        /* .whileLoops = */ 1,
        /* .doWhileLoops = */ 1,
        /* .generalUniformIndexing = */ 1,
        /* .generalAttributeMatrixVectorIndexing = */ 1,
        /* .generalVaryingIndexing = */ 1,
        /* .generalSamplerIndexing = */ 1,
        /* .generalVariableIndexing = */ 1,
        /* .generalConstantMatrixVectorIndexing = */ 1,
    }
};

#define FORCE_BUILD_SHADERS 1
constexpr bool gForceBuildShaders = bool(FORCE_BUILD_SHADERS);

struct ShaderCompileResult
{
    std::vector<uint32_t> OutVertShader;
    std::vector<uint32_t> OutFragShader;
};

bool LoadShaderSource(std::string Name, std::string& OutSrc)
{
    // Load HLSL

    std::filesystem::path Path = Ruby::GetProgramPath();
    std::filesystem::path Shader = Path.parent_path().parent_path() / "Shaders" / Name;

    std::ifstream Input(Shader.string());

    if (!Input.is_open())
        return false;

    std::string Line;
    std::ostringstream Output;
    while (std::getline(Input, Line))
    {
        Output << Line + "\n";
    }

    OutSrc = Output.str();
    return true;
}

class CustomIncluder : public glslang::TShader::Includer
{
public:

    IncludeResult* includeSystem(const char* HeaderName, const char* /* Includer name (not used here) */, size_t Depth) override
    {
        return includeLocal(HeaderName, "", Depth);
    }

    IncludeResult* includeLocal(const char* HeaderName, const char* /* Includer name (not used here) */, size_t Depth) override
    {
        if (Depth > 30)
        {
            static std::string Err = "Max include depth reached. May indicate a cyclical include";
            return new IncludeResult{ "", Err.c_str(), Err.size(), nullptr };
        }

        if (IncludeCache.contains(HeaderName))
        {
            std::string* IncludeData = IncludeCache[HeaderName];
            return new IncludeResult{ HeaderName, IncludeData->c_str(), IncludeData->size(), nullptr };
        }

        std::string IncludeContents;
        std::string Err;
        bool Success = LoadShaderSource(HeaderName, IncludeContents);
        if(!Success)
        {
            std::string* NewError = new std::string(Err);
            Errors.push_back(NewError);
        	return new IncludeResult{ "", NewError->c_str(), NewError->size(), nullptr };
        }
        else
        {
            std::string* NewData = new std::string(IncludeContents);
            IncludeCache.emplace(HeaderName, NewData);
        	return new IncludeResult{ HeaderName, NewData->c_str(), NewData->size(), nullptr };
        }
    }

    void releaseInclude(IncludeResult* Result) override
    {
        delete Result;
    }

private:

    std::unordered_map<std::string, std::string*> IncludeCache;
    std::vector<std::string*> Errors;

} Includer;

bool HlslToSpv(const std::string& VertSrc, const std::string& FragSrc, ShaderCompileResult& OutResult)
{
    glslang::TProgram ShaderProgram;

    const char* VertSourcesArray[] = { VertSrc.c_str() };
    const char* FragSourcesArray[] = { FragSrc.c_str() };

    glslang::TShader VertexShader(EShLanguage::EShLangVertex);
    glslang::TShader FragmentShader(EShLanguage::EShLangFragment);


    // Add vertex shader if present
    if(!VertSrc.empty())
    {
        VertexShader.setEnvInput(glslang::EShSource::EShSourceHlsl, EShLanguage::EShLangVertex, glslang::EShClientVulkan, glslang::EShTargetClientVersion::EShTargetVulkan_1_2);
        VertexShader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_2);
        VertexShader.setStrings(VertSourcesArray, 1);
        VertexShader.setEntryPoint("main");

        if (!VertexShader.parse(&DefaultTBuiltInResource,
            0,
            EProfile::ECoreProfile,
            false,
            false,
            EShMessages::EShMsgDefault,
            Includer)
            )
        {
            std::cerr << "Vertex compile error: " << VertexShader.getInfoLog() << std::endl;
            return false;
        }

        ShaderProgram.addShader(&VertexShader);
    }

    // Add fragment shader if present
    if(!FragSrc.empty())
    {
        FragmentShader.setEnvInput(glslang::EShSource::EShSourceHlsl, EShLanguage::EShLangFragment, glslang::EShClientVulkan, glslang::EShTargetClientVersion::EShTargetVulkan_1_2);
        FragmentShader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_2);
        FragmentShader.setStrings(FragSourcesArray, 1);
        FragmentShader.setEntryPoint("main");

        if (!FragmentShader.parse(&DefaultTBuiltInResource,
            0,
            EProfile::ECoreProfile,
            false,
            false,
            EShMessages::EShMsgVulkanRules,
            Includer)
            )
        {
            std::cerr << "Fragment compile error: " << FragmentShader.getInfoLog() << std::endl;
            return false;
        }

        ShaderProgram.addShader(&FragmentShader);
    }

    // Link program
    if (!ShaderProgram.link(EShMessages::EShMsgVulkanRules))
    {
        std::cerr << "Link error: " << ShaderProgram.getInfoLog() << std::endl;
        return false;
    }

    // Translate intermediate representation to SpirV
    glslang::SpvOptions Opts;
    Opts.disableOptimizer = false;
    Opts.validate = true;
    Opts.stripDebugInfo = false;
    Opts.optimizeSize = false;
    Opts.generateDebugInfo = true;

    // Get Spv for each shader stage
    if(!VertSrc.empty())
    {
        glslang::GlslangToSpv(*ShaderProgram.getIntermediate(EShLanguage::EShLangVertex), OutResult.OutVertShader, &Opts);
    }

    if(!FragSrc.empty())
    {
        glslang::GlslangToSpv(*ShaderProgram.getIntermediate(EShLanguage::EShLangFragment), OutResult.OutFragShader, &Opts);
    }

    return true;
}

static std::string VertExt = ".vert";
static std::string FragExt = ".frag";

bool CompileVertShader(std::string Vert, ShaderCompileResult& OutResult)
{
    std::string VertSrc{};
    if (!LoadShaderSource(Vert + VertExt + ".hlsl", VertSrc))
        return false;

#ifdef LLRM_VULKAN
    return HlslToSpv(VertSrc, "", OutResult);
#endif
}

bool CompileFragShader(std::string Frag, ShaderCompileResult& OutResult)
{
    std::string FragSrc{};
    if (!LoadShaderSource(Frag + FragExt + ".hlsl", FragSrc))
        return false;

#ifdef LLRM_VULKAN
    return HlslToSpv("", FragSrc, OutResult);
#endif
}

bool CompileRasterProgram(std::string Vert, std::string Frag, ShaderCompileResult& OutResult)
{
    std::string VertSrc{}, FragSrc{};
    if (!LoadShaderSource(Vert + VertExt + ".hlsl", VertSrc))
        return false;
    if (!LoadShaderSource(Frag + FragExt + ".hlsl", FragSrc))
        return false;

#ifdef LLRM_VULKAN
    return HlslToSpv(VertSrc, FragSrc, OutResult);
#endif
}

namespace Ruby
{

#ifdef LLRM_VULKAN
    static std::string CompiledShaderExt = ".spv";
#endif

    void InitShaderCompilation()
    {
        glslang::InitializeProcess();
    }

    void FinishShaderCompilation()
    {
        glslang::FinalizeProcess();
    }

    std::string GetShaderExt(RenderingAPI API)
    {
        switch(API)
        {
        case RenderingAPI::Vulkan:
            return ".spv";
        }
        return "";
    }

    enum ShaderType
    {
	    Vertex,
        Fragment
    };

    void GenerateUberPerms(std::string ShaderName, ShaderType Type, std::vector<UberVar> Ubers, uint32_t UberIndex, std::vector<std::pair<std::string, uint32_t>> UberValues, std::vector<std::string>& OutShaders)
    {
        if(Ubers.size() == 0)
        {
	        // Copy over single shader
            OutShaders.push_back(ShaderName);
        }
        else
        {
            // Generate uber permutations
            if (UberIndex == Ubers.size())
            {
                // Terminal, generate shader

            	const std::string& Ext = (Type == Vertex) ? VertExt : FragExt;

                std::filesystem::path Root = GetProgramPath();
                std::filesystem::path Shaders = Root.parent_path().parent_path() / "Shaders";

                std::ostringstream NewUberSrc;
                std::ostringstream NewUberFileName;
                NewUberFileName << ShaderName;
                for (const auto& UberVal : UberValues)
                {
                    NewUberSrc << std::string("#define ") << UberVal.first << " " << UberVal.second << "\n";
                    NewUberFileName << std::string("_") << UberVal.second;
                }

                NewUberSrc << std::string("#include \"") << (ShaderName + Ext + ".hlsl") << "\"\n";

                // Create uber permutation file
                std::filesystem::path Path = Shaders / (NewUberFileName.str() + Ext + ".hlsl");
                std::ofstream OutStream(Path);
                OutStream << NewUberSrc.str();

                OutShaders.push_back(NewUberFileName.str());
            }
            else
            {
                UberVar& CurUber = Ubers[UberIndex];
                for (uint32_t Value = CurUber.MinValue; Value <= CurUber.MaxValue; Value++)
                {
                    std::vector<std::pair<std::string, uint32_t>> NewValues = UberValues;
                    NewValues.push_back(std::make_pair(CurUber.Name, Value));
                    GenerateUberPerms(ShaderName, Type, Ubers, UberIndex + 1, NewValues, OutShaders);
                }
            }
        }

    }

    bool CompileRasterProgram(std::string VertName, std::string FragName, std::vector<UberVar> VertUbers, std::vector<UberVar> FragUbers)
    {
        std::filesystem::path Root = Ruby::GContext.CompiledShaders;

        // Generate uber perms
        std::vector<std::string> OutVertShaders;
        std::vector<std::string> OutFragShaders;

    	GenerateUberPerms(VertName, Vertex, VertUbers, 0, {}, OutVertShaders);
        GenerateUberPerms(FragName, Fragment, FragUbers, 0, {}, OutFragShaders);

        // Match the ubers
        for(const std::string& VertUber : OutVertShaders)
        {
            for (const std::string& FragUber : OutFragShaders)
            {
                std::filesystem::path VertPath = Root / (VertUber + VertExt + CompiledShaderExt);
                std::filesystem::path FragPath = Root / (FragUber + FragExt + CompiledShaderExt);

                ShaderCompileResult Result{};
                if (!CompileRasterProgram(VertUber, FragUber, Result))
                {
                    // Error
                    return false;
                }

                std::filesystem::create_directories(VertPath.parent_path());
                std::filesystem::create_directories(FragPath.parent_path());

                // Cache the compiled shaders, only write out first vertex shader
                WriteBinaryFile(VertPath.string(), Result.OutVertShader);
            	WriteBinaryFile(FragPath.string(), Result.OutFragShader);
            }
        }

        return true;

    }

    llrm::ShaderProgram LoadRasterShader(std::string VertName, std::string FragName)
    {
        std::filesystem::path Root = Ruby::GContext.CompiledShaders;
        std::filesystem::path VertPath = Root / (VertName + VertExt + CompiledShaderExt);
        std::filesystem::path FragPath = Root / (FragName + FragExt + CompiledShaderExt);

        ShaderCompileResult Result{};

        if(gForceBuildShaders || (!std::filesystem::exists(VertPath) || !std::filesystem::exists(FragPath)))
        {
            if(!CompileRasterProgram(VertName, FragName, Result))
            {
                // Error
                return nullptr;
            }

            std::filesystem::create_directories(VertPath.parent_path());
            std::filesystem::create_directories(FragPath.parent_path());

            // Cache the compiled shaders
            WriteBinaryFile(VertPath.string(), Result.OutVertShader);
            WriteBinaryFile(FragPath.string(), Result.OutFragShader);
        }
        else
        {
	        // Load cached shaders
            LoadBinaryFile(VertPath.string(), Result.OutVertShader);
            LoadBinaryFile(FragPath.string(), Result.OutFragShader);
        }

        return llrm::CreateRasterProgram(Result.OutVertShader, Result.OutFragShader);
    }
}
