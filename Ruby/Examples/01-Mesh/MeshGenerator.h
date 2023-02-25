#include <vector>

#include "Ruby.h"
#include "glm/glm.hpp"

inline Ruby::Tesselation TesselateRectPrism(glm::vec3 Pos, glm::vec3 Size)
{
	Ruby::Tesselation Result;
	// Convert to half size
	Size /= 2.0f;

	glm::vec3 BackLeftBottom = Pos + glm::vec3{-Size.x, -Size.y, -Size.z};
	glm::vec3 BackLeftTop = Pos + glm::vec3{ -Size.x, Size.y, -Size.z };
	glm::vec3 BackRightTop = Pos + glm::vec3{ Size.x, Size.y, -Size.z };
	glm::vec3 BackRightBottom = Pos + glm::vec3{ Size.x, -Size.y, -Size.z };

	glm::vec3 FrontLeftBottom = Pos + glm::vec3{ -Size.x, -Size.y, Size.z };
	glm::vec3 FrontLeftTop = Pos + glm::vec3{ -Size.x, Size.y, Size.z };
	glm::vec3 FrontRightTop = Pos + glm::vec3{ Size.x, Size.y, Size.z };
	glm::vec3 FrontRightBottom = Pos + glm::vec3{ Size.x, -Size.y, Size.z };

	// Now triangulate
	Result.mVerts.emplace_back(BackLeftBottom); // 0
	Result.mVerts.emplace_back(BackLeftTop); // 1
	Result.mVerts.emplace_back(BackRightTop); // 2
	Result.mVerts.emplace_back(BackRightBottom); // 3

	Result.mVerts.emplace_back(FrontLeftBottom); // 4
	Result.mVerts.emplace_back(FrontLeftTop); // 5
	Result.mVerts.emplace_back(FrontRightTop); // 6
	Result.mVerts.emplace_back(FrontRightBottom); // 7

	// Back face
	Result.mIndicies.insert(Result.mIndicies.end(), { 2, 1, 0 });
	Result.mIndicies.insert(Result.mIndicies.end(), { 0, 3, 2 });

	return Result;
}