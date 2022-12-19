#pragma once

#include <string>

namespace Ruby
{
	struct RubyContext
	{
		std::string ShadersRoot;
		std::string CompiledShaders;
	};

	struct ContextParams
	{
		std::string ShadersRoot;
		std::string CompiledShaders;
	};

	RubyContext CreateContext(const ContextParams& Params);
	void DestroyContext(const RubyContext& Context);
	void SetContext(const RubyContext& Context);

	extern RubyContext GContext;

	struct Mesh
	{
		
	};

	class Scene
	{
	public:

	private:

	};

	class Renderer
	{
	public:

		Renderer(Scene& Scene)
		{
			Rendering = Scene;
		}

		void Render()
		{
			
		}

	private:
		Scene& Rendering;
	};

}
