#include "Ruby.h"

#include "llrm.h"

namespace Ruby
{
	RubyContext GContext;

	RubyContext CreateContext(const ContextParams& Params)
	{
		RubyContext NewContext{};
		NewContext.CompiledShaders = Params.CompiledShaders;
		NewContext.ShadersRoot = Params.ShadersRoot;

		GContext = NewContext;

		return NewContext;
	}

	void DestroyContext(const RubyContext& Context)
	{

	}

	void SetContext(const RubyContext& Context)
	{
		GContext = Context;
	}
}
