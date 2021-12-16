export module SDLHandler;
import <SDL.h>;
import <cstdlib>;
import <stdio.h>;

/**
 * @brief Class used for handling various output window related things
*/
export class SDLHandler
{
private:
	SDL_Window*		mWindow;
	SDL_Surface*	mSurface;
	SDL_Event		event;
public:

	/**
	 * @brief Initialize the output window
	 * @param height Height of the window in pixels
	 * @param width Width of the window in pixels
	*/
	inline void InitHandler( unsigned int const& height, unsigned int const& width)
	{
		if (SDL_Init(SDL_INIT_VIDEO) < 0)
		{
			printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
			std::abort();
		}

		mWindow = SDL_CreateWindow("Realtime Raytracer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN);

		if (!mWindow)
		{
			printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
			std::abort();
		}
		mSurface = SDL_GetWindowSurface(mWindow);
	}

	/**
	 * @brief Check if a frame should be rendered
	 * @param xAxisVelocity Input from keyboard. -1 Move left, 0 Doesn't move, 1 move to right
	 * @param useShadows Input from keyboard. 1 use shadows, 0 no shadows
	 * @param useReflections Input from keyboard. 1 enable reflections, 0 disable reflections
	 * @param useLogging Input from keyboard. 1 print out logging data, 0 do not print out
	 * @param addComplex Input from keyboard. 1 add a complex mesh into the scene, 0 do nothing
	 * @param removeComplex Input from keyboard. 1 remove the complex mesh from the scene, 0 do nothing
	 * @return True if the rendering should still continue
	*/
	inline bool const RunTheLoop(float& xAxisVelocity, int& useShadows, int& useReflections, int& useLogging, int& addComplex, int& removeComplex)
	{
		// Uses ints because could not do a reference return for bools.
		xAxisVelocity = 0.f;
		if (!SDL_PollEvent(&event)) return true;

		if (event.type == SDL_QUIT) [[unlikely]]
			return false;
		else if (event.type == SDL_KEYDOWN) [[unlikely]]
		{
			switch (event.key.keysym.sym)
			{
				case SDLK_d :
					xAxisVelocity = 1.f;
					break;

				case SDLK_a :
					xAxisVelocity = -1.f;
					break;

				[[unlikely]] case SDLK_q:
					useShadows = useShadows == 0 ? 1 : 0;
					break;

				[[unlikely]] case SDLK_e:
					useReflections = useReflections == 0 ? 1 : 0;
					break;

				[[unlikely]] case SDLK_l:
					useLogging = useLogging == 0 ? 1 : 0;
					break;

				[[unlikely]] case SDLK_n:
					addComplex = addComplex == 0 ? 1 : 0;
					break;

				[[unlikely]] case SDLK_m:
					removeComplex = removeComplex == 0 ? 1 : 0;
					break;

				[[unlikely]] case SDLK_ESCAPE:
					return false;
					break;
			}
		}
		return true;
	}

	/**
	 * @brief Write colour value to a specific section of the screen
	 * @param x X coordinates of the screen
	 * @param y Y coordinates of the screen
	 * @param colour Colour of the pixel
	*/
	inline void PutPixel32_nolock( int const x, int const y, Uint32 const& colour)
	{
		auto* pixelArr = (Uint8*)mSurface->pixels;
		// Move the pointer iterator.
		pixelArr += (y * mSurface->pitch) + (x * sizeof(Uint32));
		*((Uint32*)pixelArr) = colour;
	};

	/**
	 * @brief Update the frame
	*/
	inline void UpdateScreen()
	{
		SDL_UpdateWindowSurface(mWindow);
	}

	/**
	 * @brief Clean up the used SDL library elements
	*/
	inline void CleanUp()
	{
		SDL_DestroyWindow(mWindow);
		SDL_Quit();
	}
};