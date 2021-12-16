import Renderer;

#define WIDTH 640u
#define HEIGHT 480u
#define FOV 35.0f

int main()
{
	Renderer renderer(WIDTH, HEIGHT, FOV);
	renderer.Render();
}