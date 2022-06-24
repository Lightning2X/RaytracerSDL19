#include "precomp.h"
#include "time.h" 
 
#include <stdio.h>


// Window
Window* window = NULL;
Screen* screen = NULL; 
RaytracerRenderer* renderer = NULL;
Scene* defaultScene = NULL;
Camera* camera = NULL;
int iteration; 
FILE* f = fopen("out.txt", " w");


bool initializeSDLEnvironment()
{
	bool success = true;
	// Initialize SDL2
	SDL_SetMainReady();
	SDL_Init(SDL_INIT_EVERYTHING);
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
		success = false;
	}
	SDL_GL_SetSwapInterval(0);
	return success;
}

void SetupRaytracer()
{  
	window = new Window("Raytracer", SCREEN_POSX, SCREEN_POSY, SCREEN_WIDTH, SCREEN_HEIGHT);
	screen = new Screen(window, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	screen->prepareScreen();
	renderer = new RaytracerRenderer(screen);
	camera = new Camera(Vec3Df(0), Vec3Df(0, 0, 1));
}

void LoadConfig()
{ 
	defaultScene = new Scene();
	renderer->setScene(defaultScene);
	renderer->setCamera(camera);
}

void close()
{
	printf("Closing application");
	//Destroy window
	delete window;

	//Quit SDL subsystems
	SDL_Quit();
}

void loop()
{
	printf("Started raytracing \n");
	bool cont = true;
	while (true)
	{
		// used for performance
		#ifdef CHECK_PERFORMANCE
		const Uint64 start = SDL_GetPerformanceCounter();
		#endif

		// Actual update
		renderer->draw(iteration);
		renderer->flush();
		iteration++;

		// used for performance
		#ifdef CHECK_PERFORMANCE
		const Uint64 end = SDL_GetPerformanceCounter();
		const static Uint64 freq = SDL_GetPerformanceFrequency();
		const double seconds = (end - start) / static_cast<double>(freq);
		printf("Frame time: %f FPS: % f\n",seconds, 1 / seconds);
		fprintf(f, "%f\n", 1 / seconds);
		#endif 
		if ((clock() / CLOCKS_PER_SEC) > 20) {
			fclose(f);
			exit(0);
		}
	}
}

int main(int argc, char** argv) {
	// INIT
	bool success = true;
	success = initializeSDLEnvironment();
	if (!success)
	{
		close();
		return 1;
	}
	printf("Loaded SDL environment \n");
	// SETUP
	SetupRaytracer();
	LoadConfig();
	printf("Created rendering environment \n");
	// LOOP
	loop();
	// CLOSE
	close();
	return 0;
}

