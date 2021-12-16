export module Shapes;

import <glm.hpp>;
import <optional>;
using glm::vec3;
import "OBJloader.h";
export import "vertex.h";
export import <string>;
import <format>;
import <memory>;
import <vector>;
import AABB;
import <span>;

using std::span;
/**
 * @brief A small collection of data about intersection
*/
export struct IntersectionInfo
{
	vec3			IntersectionPoint;
	vec3			NormalVector;
	/**
	 * @brief A coefficient which is used in multiplication with the direction vector to get a collision point
	*/
	float			t;

	/**
	 * @brief Used for meshes. In order to cahce which triangle was hit, instead of having to find it.
	*/
	unsigned int	HitTriangleIndex;
	IntersectionInfo(float const& t, vec3 const& intPt, vec3 const& normal) : t(t), IntersectionPoint(intPt), NormalVector(normal) {};
	IntersectionInfo() {};
};

export struct ColourCollection
{
	vec3	ambientColour;
	vec3	ambientIntensity{ 0.1f, 0.1f, 0.1f };
	vec3	difuseColour;
	vec3	specularColour;
	int		shineness;

	/**
	 * @brief Base constructor that initializes some values automatically. Use for quick inits
	 * @param baseColour
	*/
	ColourCollection(vec3 const& baseColour)
	{
		ambientColour = baseColour;
		difuseColour = baseColour;
		specularColour = vec3(0.7f, 0.7f, 0.7f);
		shineness = 64;
	}
};

/**
 * @brief A base class for all of the shapes and meshes
*/
export class ShapeBase
{
public:
	typedef  std::vector<ShapeBase*>*	SceneItems;

	/**
	 * @brief Scene Items meant for reflection checks
	*/
	SceneItems							mSceneItems;
	ColourCollection					mColourData;
	AABB								mBoundingBox;
	vec3								mPosition;

	/**
	 * @brief A static check if shadows should be checked - should be faster than passing in a value
	*/
	static bool							Shadows;
public:
	ShapeBase(vec3 const& pos, ColourCollection const& colorData, SceneItems const sceneItems) : mPosition(pos), mColourData(colorData), mSceneItems(sceneItems) {};

	/**
	 * @brief Get the intersection information from a ray to this object.
	 * @param rayDirection Direction of the ray.
	 * @param rayOrigin Position where the ray originates from. Camera position.
	 * @param intersectionResult Referenced object to get the intersection information. If there was no intersection - empty, check with has_value().
	 * @param selfShadowCheck A way to internally check if the object that is shooting the ray is the object itself. Mainly used for shadows and for meshes
	*/
	virtual void GetIntersectionInfo(vec3 const& rayDirection, vec3 const& rayOrigin, std::optional<IntersectionInfo>& intersectionResult, ShapeBase const *const selfShadowCheck = nullptr) const
	{
		printf("Base Intersection calculation\n");
	};

	/**
	 * @brief Calculate the colour of the intersected point on the object
	 * @param intersectionInfo
	 * @param lightPts Array of lights in the scene
	 * @param rayDirection
	 * @param resultingColor
	*/
	virtual void ComputeColor(IntersectionInfo const& intersectionInfo, span<vec3> const& lightPts, vec3 const& rayDirection, vec3& resultingColor) const
	{
		printf("Base color calculation\n");
	};

protected:
	/**
	 * @brief Phong shading model calulation meant for internal calculations
	 * @param intersectionInfo
	 * @param lightPositions Array of light positions within the scene for soft shadows
	 * @param itemPosition
	 * @param rayDirection
	 * @return
	*/
	inline float const& PhongCalculation(IntersectionInfo const& intersectionInfo, span<vec3> const& lightPositions, vec3 const& itemPosition, vec3 const& rayDirection) const
	{
		//		Ca
		auto ambientIntensity = glm::dot(mColourData.ambientColour, mColourData.ambientIntensity);

		auto intersectionPoint = intersectionInfo.IntersectionPoint;

		// It's computationally cheaper just to have one check than having to do some weird multiple checks - makes code larger, but simpler
		if(Shadows)
		{
			auto sizeInv = 1 / (float)lightPositions.size();
			float average = 0;
			for (auto light : lightPositions)
			{
				// l										s					p
				auto lightToIntersectionDir = glm::normalize(light - intersectionPoint);
				// n
				auto surfaceNormal = intersectionInfo.NormalVector;

				float const epsilon = 1e-8;
				//  To avoid the self shadowing effect
				auto modedPoint = intersectionPoint + surfaceNormal * (float)epsilon;

				// Check for intersections.
				for (auto item : *mSceneItems)
				{
					if (this == item)
						continue;
					if (!item->mBoundingBox.CheckForIntersection(modedPoint, lightToIntersectionDir))
						continue;

					std::optional<IntersectionInfo> shadowIntersection;
					// This is why I disabled the optimization flag - the method does is somewhat skipped
					item->GetIntersectionInfo(lightToIntersectionDir, modedPoint, shadowIntersection, this);

					if (shadowIntersection.has_value())
					{
						average += (ambientIntensity);
						goto endcalc; // Avoid unnecessary bool flags and comparisons and this is not security critical software
					}
				}

				{
					auto lightReflection = 2.0f * (glm::dot(lightToIntersectionDir, surfaceNormal)) * surfaceNormal - lightToIntersectionDir;
					auto temp = glm::max(0.0f, glm::dot(lightReflection, -rayDirection));
					auto specularColor = glm::pow(temp, mColourData.shineness) * 0.7f;
					auto diffuseColor = glm::max(0.0f, glm::dot(lightToIntersectionDir, surfaceNormal));
					average += (ambientIntensity + diffuseColor + specularColor);
				};

			endcalc:
				;
			}

			return average * sizeInv;
		}

		auto lightToIntersectionDir = glm::normalize(lightPositions[0] - intersectionPoint);
		// n
		auto surfaceNormal = glm::normalize( intersectionInfo.NormalVector);

		float const epsilon = 1e-4;
		//  To avoid the self shadowing effect
		//     p'
		auto modedPoint = intersectionPoint + surfaceNormal * epsilon;

		//		r
		auto lightReflection = 2.0f * (glm::dot(lightToIntersectionDir, surfaceNormal)) * surfaceNormal - lightToIntersectionDir;
		//		v'				v
		auto reflectingRay = rayDirection - 2 * glm::dot(rayDirection, surfaceNormal) * surfaceNormal;



		auto diffuseColor = glm::max(0.0f, glm::dot(lightToIntersectionDir, surfaceNormal));

		auto temp = glm::max(0.0f, glm::dot(lightReflection, -rayDirection));
		//		Cs					max(...)			p			Ks i
		auto specularColor = glm::pow(temp, mColourData.shineness) * 0.7f;

		// Do the same compute chunk calculation with all of the items
		return ambientIntensity + diffuseColor + specularColor;
	}
};

/**
 * @brief A basic sphere object.
*/
export class Sphere : public ShapeBase
{
private:
	float radius;
public:
	Sphere(vec3 const& pos, ColourCollection const& colourData, float const& radius, SceneItems const sceneItems) : ShapeBase(pos, colourData, sceneItems), radius(radius)
	{
		mBoundingBox.pMin = vec3(mPosition.x - radius, mPosition.y - radius, mPosition.z - radius);
		mBoundingBox.pMax = vec3(mPosition.x + radius, mPosition.y + radius, mPosition.z + radius);
	};

	void ComputeColor(IntersectionInfo const& intersectionInfo, span<vec3> const& lightPts, vec3 const& rayDirection, vec3& resultingColor) const override
	{
		auto phongCol = PhongCalculation(intersectionInfo, lightPts, mPosition, rayDirection);
		resultingColor = mColourData.ambientColour * phongCol;
	};


	void GetIntersectionInfo(vec3 const& rayDirection, vec3 const& rayOrigin, std::optional<IntersectionInfo>& intersectionResult, ShapeBase const* const selfShadowCheck = nullptr) const override
	{
		auto poVector = mPosition - rayOrigin;
		auto poProjection = glm::dot(poVector, rayDirection);

		intersectionResult = {};

		if (poProjection < 0)
			return;

		auto sSq = glm::dot(poVector, poVector) - poProjection * poProjection;
		auto radiusSq = radius * radius;

		if (sSq > radiusSq)
			return;

		// The distance from the center of the sphere to the closest point of the poProjection
		auto thc = glm::sqrt(radiusSq - sSq);

		auto t0 = poProjection - thc;
		auto t1 = poProjection + thc;

		if (t0 > t1)
			std::swap(t0, t1);

		if (t0 < 0)
		{
			t0 = t1;
			if (t0 < 0)
				return;
		}

		auto intersectionPoint = t0 * rayDirection;
		auto normal = glm::normalize(intersectionPoint - mPosition);
		intersectionResult = IntersectionInfo(t0, intersectionPoint, normal);
	}
};

/**
 * @brief Basic Triangle. Requires the vertex.h
*/
export class Triangle : public ShapeBase
{
private:
	VertexWithAll mVertices[3];
private:

public:
public:

	Triangle(ColourCollection const& colorData, VertexWithAll  const& p1, VertexWithAll const& p2, VertexWithAll  const& p3, SceneItems const sceneItems) : ShapeBase(vec3((p1.position.x + p2.position.x + p3.position.x) / 3.f, (p1.position.y + p2.position.y + p3.position.y) / 3.f, (p1.position.z + p2.position.z + p3.position.z) / 3.f), colorData, sceneItems)
	{
		mVertices[0] = p1;
		mVertices[1] = p2;
		mVertices[2] = p3;

		vec3 min(FLT_MAX, FLT_MAX, FLT_MAX);
		vec3 max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		for (auto p : mVertices)
		{
			auto x = p.position.x;
			if (x < min.x)
				min.x = x;

			if (x > max.x)
				max.x = x;

			auto y = p.position.y;
			if (y < min.y)
				min.y = y;

			if (y > max.y)
				max.y = y;

			auto z = p.position.z;
			if (z < min.z)
				min.z = z;

			if (z > max.z)
				max.z = z;
		}

		mBoundingBox.pMin = min;
		mBoundingBox.pMax = max;
	}

	~Triangle() {};

	void GetIntersectionInfo(vec3 const& rayDirection, vec3 const& rayOrigin, std::optional<IntersectionInfo>& intersectionResult, ShapeBase const* const selfShadowCheck = nullptr) const override
	{

		// Based on scracthpixel.com lessons and the github lecture
		intersectionResult = {};
		vec3 v0v1 = mVertices[1].position - mVertices[0].position; //E1 = B - A
		vec3 v0v2 = mVertices[2].position - mVertices[0].position; //E2 = C - A

		auto pvec = glm::cross(rayDirection, v0v2);
		auto det = glm::dot(v0v1, pvec);

		// Back-face culling
		if (det < 1e-8)
			return;

		auto inversedDet = 1 / det;

		auto tvec = rayOrigin - mVertices[0].position;

		float u = (glm::dot(tvec, pvec)) * inversedDet;

		if (u < 0 || u > 1)
			return;

		auto qvec = glm::cross(tvec, v0v1);
		float v = glm::dot(rayDirection, qvec) * inversedDet;

		if (v < 0 || (u + v) > 1)
			return;

		auto w = 1.0f - u - v;

		// And then project tD on D to get coefficient
		auto t = glm::dot(v0v2, qvec) * inversedDet;

		if (t < 0)
			return;

		auto tD = t * rayDirection;


		auto normal = w * mVertices[0].normal + u * mVertices[1].normal + v * mVertices[2].normal;
		normal = glm::normalize(normal);

		intersectionResult = IntersectionInfo(t, tD, normal);
	}

	void ComputeColor(IntersectionInfo const& intersectionInfo, span<vec3> const& lightPts, vec3 const& rayDirection, vec3& resultingColor) const override
	{
		auto phongCol = PhongCalculation(intersectionInfo, lightPts, mPosition, rayDirection);

		resultingColor = mColourData.ambientColour * phongCol;
	};
};

/**
 * @brief An infinite plane
*/
export class Plane : public ShapeBase
{
private:
	vec3 normal;

public:

	Plane(vec3 const& position, vec3 const& _normal, ColourCollection const& colorData, SceneItems const sceneItems, AABB const& bounds) : ShapeBase(position, colorData, sceneItems)
	{
		normal = glm::normalize(_normal);
		mBoundingBox = bounds;
	}

	~Plane() {}
	virtual void ComputeColor(IntersectionInfo const& intersectionInfo, span<vec3> const& lightPts, vec3 const& rayDirection, vec3& resultingColor) const override
	{
		auto phongCol = PhongCalculation(intersectionInfo, lightPts, mPosition, rayDirection);

		resultingColor = mColourData.ambientColour * phongCol;
	};

	void GetIntersectionInfo(vec3 const& rayDirection, vec3 const& rayOrigin, std::optional<IntersectionInfo>& intersectionResult, ShapeBase const* const selfShadowCheck = nullptr)  const override
	{
		intersectionResult = {};
		auto denom = glm::dot(normal, rayDirection);

		// The normal and ray dir need to oppose each other
		if (denom > -1e-6)
			return;

		auto p0l0 = mPosition - rayOrigin;
		auto inversedDenom = 1 / denom;
		auto t = glm::dot(p0l0, normal) * inversedDenom;
		if (t < 0)
			return;

		intersectionResult = IntersectionInfo(t, t * rayDirection, normal);
	}
};

/**
 * @brief A basic mesh implementation - uses OBJloader.h and vertex.h.
 * Basically contains a collection of triangles.
*/
export class Mesh : public ShapeBase
{
private:
	std::vector<Triangle> meshTriangles;
private:
public:
public:

	Mesh(std::string const& pathToObj, vec3 const& pos, ColourCollection const& colorData, SceneItems const sceneItems) : ShapeBase(pos, colorData, sceneItems)
	{
		auto vertices = loader::loadOBJ(pathToObj.c_str());

		vec3 min(FLT_MAX, FLT_MAX, FLT_MAX);
		vec3 max(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		for (auto p : vertices)
		{
			auto x = p.position.x;
			if (x < min.x)
				min.x = x;

			if (x > max.x)
				max.x = x;

			auto y = p.position.y;
			if (y < min.y)
				min.y = y;

			if (y > max.y)
				max.y = y;

			auto z = p.position.z;
			if (z < min.z)
				min.z = z;

			if (z > max.z)
				max.z = z;
		}
		mBoundingBox.pMin = min + pos;
		mBoundingBox.pMax = max + pos;

		meshTriangles.reserve(vertices.size()/3);
		for (unsigned int i = 0; i < vertices.size(); i += 3)
		{
			auto v1 = vertices[i];
			auto v2 = vertices[i + 1];
			auto v3 = vertices[i + 2];
			v1.position += pos;
			v2.position += pos;
			v3.position += pos;
			Triangle triangle(colorData, v1, v2, v3, sceneItems);

			meshTriangles.emplace_back(triangle);
		}
	}

	void GetIntersectionInfo(vec3 const& rayDirection, vec3 const& rayOrigin, std::optional<IntersectionInfo>& intersectionResult, ShapeBase const* const selfShadowCheck = nullptr) const override
	{
		intersectionResult = {};
		float smallestT = FLT_MAX;
		for (unsigned int i = 0; i < meshTriangles.size(); ++i)
		{
			if (&(meshTriangles[i]) == selfShadowCheck)
				continue;

			if(!meshTriangles[i].mBoundingBox.CheckForIntersection(rayOrigin, rayDirection))
				continue;

			std::optional<IntersectionInfo> _intersectionResult;
			meshTriangles[i].GetIntersectionInfo(rayDirection, rayOrigin, _intersectionResult);
			if (_intersectionResult.has_value())
			{
				auto t = _intersectionResult.value().t;

				if (t < smallestT)
				{
					smallestT = t;
					auto result = _intersectionResult.value();
					result.HitTriangleIndex = i;
					intersectionResult = result;
				}
			}
		}
	}

	void ComputeColor(IntersectionInfo const& intersectionInfo, span<vec3> const& lightPts, vec3 const& rayDirection, vec3& resultingColor) const override
	{
		meshTriangles[intersectionInfo.HitTriangleIndex].ComputeColor(intersectionInfo, lightPts, rayDirection, resultingColor);
	}
};

bool ShapeBase::Shadows = false;