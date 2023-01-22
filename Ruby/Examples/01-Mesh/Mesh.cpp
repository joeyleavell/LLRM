#include "Ruby.h"
#include "ShaderManager.h"

int main()
{
	Ruby::ContextParams Params{};
	Ruby::CreateContext(Params);

	Ruby::FrameBuffer Buffer = Ruby::CreateFrameBuffer(800, 600, false);

	llrm::ShaderProgram Program = Ruby::LoadRasterShader("Example", "Example");

	return 0;
}