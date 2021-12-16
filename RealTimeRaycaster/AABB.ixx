export module AABB;
import <glm.hpp>;

/**
 * @brief Axis Aligned bounding box to reduce the time on checks for other items
*/
export class AABB
{
public:
	/**
	 * @brief The minimum point of the AABB
	*/
	glm::vec3 pMin;

	/**
	 * @brief The maximum point of the AABB
	*/
	glm::vec3 pMax;
public:
	AABB() = default;
	AABB(glm::vec3 min, glm::vec3 max): pMin(min), pMax(max){}

	/**
	 * @brief Check if a ray is intersecting this AABB
	 * @param rayOrigin
	 * @param rayDirection Has to be normalized
	 * @return
	*/
	bool const CheckForIntersection(glm::vec3 rayOrigin, glm::vec3 rayDirection)
		const
	{
		auto epsilon = 0.0001f;

		// Creating variables in order to sacrifice memory for speed
		auto rayOx = rayOrigin.x;
		auto inversedRayDirXEpsilon = 1 / (rayDirection.x + epsilon);
		auto tmin = (pMin.x - rayOx) * inversedRayDirXEpsilon;
		auto tmax = (pMax.x - rayOx) * inversedRayDirXEpsilon;

		if (tmin > tmax)
			std::swap(tmin, tmax);

		auto rayOy = rayOrigin.y;
		auto inversedRayDirYEpsilon = 1 / (rayDirection.y + epsilon);
		auto tymin = (pMin.y - rayOy) * inversedRayDirYEpsilon;
		auto tymax = (pMax.y - rayOy) * inversedRayDirYEpsilon;

		if (tymin > tymax)
			std::swap(tymin, tymax);

		if ((tmin > tymax) || (tymin > tmax))
			return false;

		if (tymin > tmin)
			tmin = tymin;

		if (tymax < tmax)
			tmax = tymax;

		auto rayOZ = rayOrigin.z;
		auto inversedRayDirZEpsilon = 1 / (rayDirection.z + epsilon);
		auto tzmin = (pMin.z - rayOZ) * inversedRayDirZEpsilon;
		auto tzmax = (pMax.z - rayOZ) * inversedRayDirZEpsilon;

		if (tzmin > tzmax)
			std::swap(tzmin, tzmax);

		if ((tmin > tzmax) || (tzmin > tmax))
			return false;

		return true;
	}
};