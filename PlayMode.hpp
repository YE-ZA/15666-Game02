#include "Mode.hpp"

#include "Scene.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up, restart;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	Scene::Transform *craft = nullptr;
	glm::vec3 craft_position;
	float craft_collision_r = 1.0f;
	bool crash = false;
	bool escape = false;
	glm::vec3 start_pos;
	glm::vec3 camera_forward = glm::vec3(1.0f, 0.0f, 0.0f);
	float time = 0.0f;

	struct Planet
	{
		Scene::Transform *id = nullptr;
		float M;
		float radius;
		float speed;
		glm::vec3 axis;
		glm::quat rotation;
		glm::vec3 dir;
		float r = 100.0f;
	} planet[25];
	
	//universal gravitation, not a real space scale value
	float G = 6.67f;
	glm::vec3 a = glm::vec3(0.0f);
	glm::vec3 cur_a = glm::vec3(0.0f);
	glm::vec3 pre_a = glm::vec3(0.0f);
	float max_a = 0.0f;
	Planet *gravity_star = nullptr;

	glm::vec3 axis;
	float radian;
	
	//camera:
	Scene::Camera *camera = nullptr;

};
