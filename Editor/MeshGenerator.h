#include <vector>

#include "Ruby.h"
#include "glm/glm.hpp"

inline Ruby::Tesselation TesselateRectPrism(glm::vec3 Size)
{
	Ruby::Tesselation Result;
	// Convert to half size
	Size /= 2.0f;

	auto AddQuad = [&Result](std::vector<Ruby::MeshVertex> Verts, bool Reverse = false)
	{
		uint32_t Base = Result.mVerts.size();

		Result.mVerts.insert(Result.mVerts.end(), Verts.begin(), Verts.end());

		if(Reverse)
		{
			Result.mIndicies.insert(Result.mIndicies.end(), { Base + 0, Base + 1, Base + 2 });
			Result.mIndicies.insert(Result.mIndicies.end(), { Base + 2, Base + 3, Base + 0});
		}
		else
		{
			Result.mIndicies.insert(Result.mIndicies.end(), { Base + 2, Base + 1, Base + 0 });
			Result.mIndicies.insert(Result.mIndicies.end(), { Base + 0, Base + 3, Base + 2 });
		}
	};

	// Back face
	{
		glm::vec3 Normal = { 0.0f, 0.0f, 1.0f };
		AddQuad({
			{glm::vec3{-Size.x, -Size.y, Size.z}, Normal},
			{glm::vec3{-Size.x, Size.y, Size.z}, Normal},
			{glm::vec3{Size.x, Size.y, Size.z}, Normal},
			{glm::vec3{Size.x, -Size.y, Size.z}, Normal},
		});
	}

	// Front face
	{
		glm::vec3 Normal = { 0.0f, 0.0f, -1.0f };
		AddQuad({
			{glm::vec3{-Size.x, -Size.y, -Size.z}, Normal},
			{glm::vec3{-Size.x, Size.y, -Size.z}, Normal},
			{glm::vec3{Size.x, Size.y, -Size.z}, Normal},
			{glm::vec3{Size.x, -Size.y, -Size.z}, Normal},
		}, true);
	}

	// Left face
	{
		glm::vec3 Normal = { -1.0f, 0.0f, 0.0f };
		AddQuad({
			{glm::vec3{-Size.x, -Size.y, -Size.z}, Normal},
			{glm::vec3{-Size.x, Size.y, -Size.z}, Normal},
			{glm::vec3{-Size.x, Size.y, Size.z}, Normal},
			{glm::vec3{-Size.x, -Size.y, Size.z}, Normal},
		});
	}

	// Right face
	{
		glm::vec3 Normal = { 1.0f, 0.0f, 0.0f };
		AddQuad({
			{glm::vec3{Size.x, -Size.y, -Size.z}, Normal},
			{glm::vec3{Size.x, Size.y, -Size.z}, Normal},
			{glm::vec3{Size.x, Size.y, Size.z}, Normal},
			{glm::vec3{Size.x, -Size.y, Size.z}, Normal},
			}, true);
	}

	// Bottom face
	{
		glm::vec3 Normal = { 0.0f, -1.0f, 0.0f };
		AddQuad({
			{glm::vec3{-Size.x, -Size.y, Size.z}, Normal},
			{glm::vec3{-Size.x, -Size.y, -Size.z}, Normal},
			{glm::vec3{Size.x, -Size.y, -Size.z}, Normal},
			{glm::vec3{Size.x, -Size.y, Size.z}, Normal},
			}, true);
	}

	// Top face
	{
		glm::vec3 Normal = { 0.0f, 1.0f, 0.0f };
		AddQuad({
			{glm::vec3{-Size.x, Size.y, Size.z}, Normal},
			{glm::vec3{-Size.x, Size.y, -Size.z}, Normal},
			{glm::vec3{Size.x, Size.y, -Size.z}, Normal},
			{glm::vec3{Size.x, Size.y, Size.z}, Normal},
			});
	}
	
	return Result;
}