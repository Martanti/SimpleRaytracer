export module Renderer;

import Shapes;
import SDLHandler;
import <thread>;
import <glm.hpp>;
import <vector>;
import <memory>;
import <algorithm>;
import <tuple>;
import <functional>;
import <chrono>;
import <format>;
import <atomic>;
import <iostream>;
import <span>;

using glm::vec3;
using glm::vec2;
using glm::uint;
using std::vector;

typedef std::chrono::steady_clock				Timer;
/**
 * @brief Multiple by this to convert degrees into radians
*/
constexpr float									TO_RADIANS = (3.14159265f / 180.0f);

export class Renderer
{
private:
	uint								const	mMaxExternalThreads;
	uint								const	mHeight;
	uint								const	mWidth;
	float 								const	mAspectRatio;
	float 								const	mFovTan;
	SDLHandler									mSdlHandler;
	std::atomic<bool>							mRunMultithread;
	std::atomic<uint>							mChunkIndex;
	std::atomic<int>							mHitItems;
	std::atomic<int>							mFinished;
	std::vector< std::pair<vec2, vec2>>			mSmallChunks;
	ShapeBase::SceneItems						mSceneElements;
	unsigned int								mUsedRays = 1u;
	ShapeBase*									mComplexMesh = nullptr;
	unsigned int								mDivider = 5u;
	vec3										mAreaLight[9]{vec3(20.f, 40.f, 20.f), vec3(30.f, 40.f, 20.f), vec3(40.f, 40.f, 20.f),
																vec3(20.f, 40.f, 30.f), vec3(30.f, 40.f, 30.f), vec3(40.f, 40.f, 30.f),
																vec3(20.f, 40.f, 40.f), vec3(30.f, 40.f, 40.f), vec3(40.f, 40.f, 40.f)};
private:

	/**
	 * @brief Get X portion of the camera ray
	 * @param imageWidth
	 * @param x
	 * @return Get appropriate Camera ray position on X axis
	*/
	inline float const& GetCameraX(float const& imageWidth, uint const& x) const
	{
		auto pixelNormalizedX = (x + 0.5f) / imageWidth;
		auto pixelRemappedX = (2 * pixelNormalizedX - 1) * mAspectRatio;
		return pixelRemappedX * mFovTan;
	}

	/**
	 * @brief Get Y portion of the camera ray
	 * @param imageHeight
	 * @param y
	 * @return Get appropriate Camera ray position on Y axis
	*/
	inline float const& GetCameraY(float const& imageHeight, uint const& y)
	{
		auto pixelNormalizedY = (y + 0.5f) / imageHeight;
		auto pixelRemappedY = 1 - 2 * pixelNormalizedY;
		return pixelRemappedY * mFovTan;
	}

	/**
	 * @brief A frame computation job.
	 * Keeps calculating chunks and iterating the index which points at the current chunk to work with.
	 * After iterator outnumber the chunk collection size will idle to wait for other chunks to finish.
	 * Slower than exiting immediately, but does not create artifacts.
	*/
	inline void ComputationWrapper()
	{
		auto chunkSize = mSmallChunks.size();
		while (mChunkIndex.load() < chunkSize)
		{
			auto index = mChunkIndex.load();
			mChunkIndex++;
			auto [startCoords, endCoords] = mSmallChunks[index];
			ComputeChunk(startCoords.x, endCoords.x, startCoords.y, endCoords.y);
		}

		// Wait for all of the chunks to finish
		// Can't leave it empty - it will be optimized out
		bool a = false;
		while (mFinished.load() < chunkSize)
			a != a;

	}

	/**
	 * @brief Convert from vec3 color to uint32 color.
	 * Taken from SDLPixel sample and then modified
	 * @param colour
	 * @return
	*/
	inline Uint32 ConvertColour(vec3 const& colour)
	{
		Uint8 r, g, b;

		auto temp = (int)(colour.r * 255);
		r = temp <= 255 ? temp : 255;

		temp = (int)(colour.g * 255);
		g = temp <= 255 ? temp : 255;

		temp = (int)(colour.b * 255);
		b = temp <= 255 ? temp : 255;

		Uint32 rgb;

		//check which order SDL is storing bytes
		rgb = (r << 16) + (g << 8) + b;

		return rgb;
	};

	/**
	 * @brief Get the closest object and the collision data
	 * @param rayOrigin
	 * @param rayDirection
	 * @return
	*/
	std::pair<ShapeBase const * const, IntersectionInfo> GetIntersectedShape(vec3 const& rayOrigin, vec3 const& rayDirection)
	{
		float t = FLT_MAX;

		IntersectionInfo closestInfo;
		ShapeBase* closestShape = nullptr;

		for (auto const& shape : *mSceneElements)
		{
			if (!shape->mBoundingBox.CheckForIntersection(rayOrigin, rayDirection))
				continue;

			std::optional<IntersectionInfo> intersectionInfo;
			shape->GetIntersectionInfo(rayDirection, rayOrigin, intersectionInfo);

			if (!intersectionInfo.has_value())
				continue;

			mHitItems++;
			auto value = intersectionInfo.value();
			if (value.t >= t)
				continue;

			t = value.t;
			closestShape = shape;
			closestInfo = value;
		}

		return { closestShape, closestInfo };
	}

	/**
	 * @brief Main calculations of the chunk rendering.
	 * @param startX
	 * @param endX
	 * @param startY
	 * @param endY
	*/
	inline void ComputeChunk(uint const& startX, uint const& endX, uint const& startY, uint const& endY)
	{
		for (uint y = startY; y <= endY; y++)
		{
			auto pixelCameraY = GetCameraY((float)mHeight, y);

			for (uint x = startX; x <= endX; x++)
			{
				auto pixelCameraX = GetCameraX((float)mWidth, x);

				vec3 cameraSpace(pixelCameraX, pixelCameraY, -1);
				vec3 rayOrigin(0.f, 0.f, 0.f);
				auto rayDirection = glm::normalize(cameraSpace - rayOrigin);

				vec3 resultingColor(0.f, 0.f, 0.f);
				for (unsigned int i = 1u; i <= mUsedRays; i++)
				{
					// This could be used as a closest colour, however, if it hits nothing, there's no data indicating so, and the reflection calculation would still be executed.
					auto [closestShape, closestInfo] = GetIntersectedShape(rayOrigin, rayDirection);

					if (!closestShape)
						break;

					vec3 color;
					closestShape->ComputeColor(closestInfo, std::span<vec3>(mAreaLight), rayDirection, color);

					auto coeficient = (1 / (float) i);

					resultingColor += color * coeficient;

					auto epsilon = (float)1e-4;
					rayOrigin = closestInfo.IntersectionPoint + closestInfo.NormalVector * epsilon;
					rayDirection = glm::normalize(rayDirection - 2 * glm::dot(rayDirection, closestInfo.NormalVector) * closestInfo.NormalVector);

				}
				// Is placed in a try-catch as there were some rare memory access errors - could not reproduce them
				try
				{
					// Set colour
					mSdlHandler.PutPixel32_nolock(x, y, ConvertColour(resultingColor));
				}
				catch (std::exception const& e)
				{
					std::cout << std::format("An exception occurred while writing the image data: {}", e.what()) << std::endl;
				}
				mFinished++;
			}
		}
	}

	/**
	 * @brief Populate the scene and the complex mesh holder
	*/
	inline void InitObjects()
	{
		mSceneElements = new  std::vector<ShapeBase*>();
		mSceneElements->push_back(new Sphere(vec3(0.f, 0.f, -20.f), ColourCollection(vec3(1.f, 0.32f, 0.36f)), 4, mSceneElements));
		mSceneElements->push_back(new Sphere(vec3(5.f, -1.f, -15.f), ColourCollection(vec3(0.90f, 0.76f, 0.46f)), 2, mSceneElements));
		mSceneElements->push_back(new Sphere(vec3(5.f, 0.f, -25.f), ColourCollection(vec3(0.65f, 0.77f, 0.97f)), 3, mSceneElements));
		mSceneElements->push_back(new Sphere(vec3(-5.5f, 0.f, -15.f), ColourCollection(vec3(0.90f, 0.90f, 0.90f)), 3, mSceneElements));
		mSceneElements->push_back(new Plane(vec3(0.f, -4.f, -20.f), vec3(0.f, 1.f, 0.f), ColourCollection((vec3(0.2f,0.2f, 0.2f))), mSceneElements, AABB(vec3(-FLT_MAX, -4.f , -FLT_MAX), vec3(FLT_MAX, -4.f, FLT_MAX))));

		VertexWithAll v1;
		v1.position = vec3(4.f, 20.f, -23.f);
		v1.normal = glm::normalize(vec3(0.f, 0.f, 1.0f));

		VertexWithAll v2;
		v2.position = vec3(4.f, 0.f, -23.f);
		v2.normal = glm::normalize(vec3(0.f, 0.f, 1.0f));

		VertexWithAll v3;
		v3.position = vec3(20.f, 0.f, -23.f);
		v3.normal = glm::normalize(vec3(0.f, 0.f, 1.0f));

		mSceneElements->push_back(new Triangle(ColourCollection(vec3(0.5f, 0.5f, 0.0f)), v1, v2, v3, mSceneElements));

		mComplexMesh = new Mesh(".\\Models\\teapot_simple_smooth.obj", vec3(vec3(17.f, -2.f, -19.f)), ColourCollection(vec3(0.5f, 0.5f, 0.0f)), mSceneElements);
	}

public:
	Renderer(uint const& width, uint const& height, float const& fov)
		: mHeight(height), mWidth(width),
		mAspectRatio(width / (float)height), mFovTan(glm::tan(fov* TO_RADIANS)),
		mMaxExternalThreads( std::thread::hardware_concurrency() == 0 ? 3 : (std::thread::hardware_concurrency() - 1))
	{
		InitObjects();

		mSdlHandler.InitHandler(mHeight, mWidth);
		auto chunckSize = mWidth / mDivider;

		mSmallChunks.reserve(mHeight * mDivider);

		for (uint i = 0; i < mHeight; i++)
			for (uint j = 0; j < mDivider; j++)
				mSmallChunks.emplace_back(std::make_pair(vec2(j * chunckSize, i), vec2((j + 1) * chunckSize - 1, i)));

		// So that the threads don't start immediately
		mChunkIndex = mSmallChunks.size();

		std::cout << "Controls:\nD - move lights to the right\nA - move lights to the left\nQ - Enabled/Disable Soft shadow\nE - Enabled/Disable reflections\nL - Enable/Disable logging into console\nN - Add complex mesh to the scene\nM - Remove the complex mesh from the scene\nESC - Quit the application\n";
	}

	// Will run forever and check to see if there are new chunks to work on

	/**
	 * @brief Meant for the additional threads to calculate the chunks.
	 * Depends on mRunMultihreading so that the thread does not exit early.
	 *
	*/
	void PooledChunkcCompute()
	{
		while (mRunMultithread.load())
		{
			ComputationWrapper();
		}
	}

	/**
	 * @brief Render the scene.
	*/
	void Render()
	{
		auto imageWidth = mWidth;
		auto imageHeight = mHeight;

		Timer::time_point newTime, oldTime;
		newTime = Timer::now();

		vector<std::thread> threads;
		threads.reserve(mMaxExternalThreads);

		mRunMultithread.store(true);
		for (uint i = 0; i < mMaxExternalThreads; i++)
			threads.emplace_back(std::thread(&Renderer::PooledChunkcCompute, this));

		mHitItems = 0;
		float xAxisVelocity = 0.f;
		int useShadows = 0;
		int useRefelctions = 0;
		int useLogging = 0;
		int removeComplex = 0;
		int addComplex = 0;
		while(mSdlHandler.RunTheLoop(xAxisVelocity, useShadows, useRefelctions, useLogging, addComplex, removeComplex))
		{
			//user input handling.
			ShapeBase::Shadows = useShadows == 1;
			mUsedRays = useRefelctions == 0 ? 1 : 2;

			if (addComplex == 1 && mComplexMesh)
			{
				std::cout << "Added a complex mesh\n";
				mSceneElements->emplace_back(std::exchange(mComplexMesh, nullptr));
			}
			addComplex = 0;

			if (removeComplex == 1 && !mComplexMesh)
			{
				std::cout << "Removed a complex mesh\n";
				mComplexMesh = mSceneElements->back();
				mSceneElements->pop_back();
			}
			removeComplex = 0;

			oldTime = newTime;
			newTime = Timer::now();
			auto frameTime = std::chrono::duration_cast<std::chrono::nanoseconds>(newTime - oldTime).count();
			float lastFrameInSeconds = frameTime * (float)1e-9;

			// Moving lights based on user interaction.
			for (size_t i = 0; i < 9; i++)
				mAreaLight[i].x += xAxisVelocity * lastFrameInSeconds * 100.f;
			mFinished = 0;
			mChunkIndex = 0u;
			// Do the same job as helper threads.
			ComputationWrapper();

			mSdlHandler.UpdateScreen();
			if (useLogging == 1)
			{
				std::cout << std::format("Time taken for frame: {} s \n", lastFrameInSeconds);
				std::cout << std::format("Hits in the frame {}\n", mHitItems.load());
			}
			mHitItems = 0;
		}

		mRunMultithread.store(false);
		for (uint i = 0; i < mMaxExternalThreads; i++)
			threads[i].join();
	}

	~Renderer()
	{
		for (auto  shape : *mSceneElements)
			delete shape;

		if (mComplexMesh)
			delete mComplexMesh;

		delete mSceneElements;
		mSdlHandler.CleanUp();
	}
};