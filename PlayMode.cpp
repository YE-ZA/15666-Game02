#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint my_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > my_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("stars.pnct"));
	my_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > my_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("stars.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = my_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = my_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

PlayMode::PlayMode() : scene(*my_scene) {
	
	for (auto &transform : scene.transforms) {
		if (transform.name == "spacecraft") craft = &transform;
		for (uint32_t i = 0; i < 9; ++i)
		{
			if (transform.name == "Mars.00" + std::to_string(i+1)) planet[i].id = &transform;
			if (transform.name == "Stars.00" + std::to_string(i+1)) planet[i+5].id = &transform;
		}
		for (uint32_t i = 9; i < 20; ++i)
			if (transform.name == "Stars.0" + std::to_string(i+1)) planet[i+5].id = &transform;
	}

	//initialize attributes
	for(uint32_t i = 0; i < 25; ++i)
	{
		planet[i].speed = (std::rand() % (100 - 20)) + 20.0f;
		planet[i].axis = glm::normalize(glm::vec3(std::rand(), std::rand(), std::rand()));
		planet[i].rotation = planet[i].id->rotation;
		
		if (i < 5)
		{
			planet[i].M = 50000.0f;
			planet[i].radius = 15.0f;
		}
		else
		{
			planet[i].M = 20000.0f;
			planet[i].radius = 2.5f;
		}
	}

	craft_position = craft->position;

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	start_pos = camera->transform->position;
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_r) {
			restart.downs += 1;
			restart.pressed = true;
			crash = false;
			escape = false;
			camera->transform->position = start_pos;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_r) {
			restart.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	} else if (evt.type == SDL_MOUSEMOTION && !crash) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			camera->transform->rotation = glm::normalize(
				camera->transform->rotation
				* glm::angleAxis(-motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f))
				* glm::angleAxis(motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f))
			);
			camera_forward = camera->transform->rotation * camera_forward;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	// overall time
	time += elapsed;

	if(glm::length(camera->transform->position) > 190.0f)
		escape = true;

	if(!crash)
	{
		//floating craft
		craft->position = craft_position + 0.2f * glm::vec3(0.0f, std::sin(2 * time), 0.0f);

		max_a = 0.0f;

		//calculate acceleration and update position
		for(auto &p : planet)
		{
			p.dir = p.id->position - craft->make_local_to_world() * glm::vec4(craft->position, 1.0f);
			p.r = std::max(glm::length(p.dir), 1.0f);

			//determine which planet has the biggest impact to the spacecraft
			float delta_a = glm::length(std::min((G * p.M / p.r / p.r), 120.0f) * (p.id->position - camera->transform->position));
			if (delta_a > max_a)
			{
				max_a = delta_a;
				gravity_star = &p;
			}

			//planetary rotation
			p.id->rotation = p.rotation * glm::angleAxis(glm::radians(p.speed * time), p.axis);
		}
		
		a = std::min((G * gravity_star->M / gravity_star->r / gravity_star->r), 15.0f) * (gravity_star->id->position - camera->transform->position);
		camera->transform->position += 0.5f * a * elapsed * elapsed;

		//rotate camera by gravity
		axis = glm::normalize(glm::cross(camera_forward, gravity_star->dir));
		radian = glm::radians(glm::acos(glm::dot(camera_forward, glm::normalize(gravity_star->dir))));
		camera->transform->rotation = glm::normalize(camera->transform->rotation * glm::angleAxis(radian * 4.0f / gravity_star->r, axis));
		
		for(auto p : planet)
			if(p.r < craft_collision_r + p.radius)
				crash = true;

		//move camera:
		{
			//combine inputs into a move:
			constexpr float PlayerSpeed = 30.0f;
			glm::vec2 move = glm::vec2(0.0f);
			// if (left.pressed && !right.pressed) move.x =-1.0f;
			// if (!left.pressed && right.pressed) move.x = 1.0f;
			if (down.pressed && !up.pressed) move.y =-1.0f;
			if (!down.pressed && up.pressed) move.y = 1.0f;

			//make it so that moving diagonally doesn't go faster:
			if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

			glm::mat4x3 frame = camera->transform->make_local_to_parent();
			glm::vec3 frame_right = frame[0];
			//glm::vec3 up = frame[1];
			glm::vec3 frame_forward = -frame[2];

			camera->transform->position += move.x * frame_right + move.y * frame_forward;
		}
	}
	
	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
	restart.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUniform3fv(lit_color_texture_program->CAMERA_DIRECTION_vec3, 1, glm::value_ptr(glm::normalize(camera->transform->rotation)));
	glUniform1f(lit_color_texture_program->TIME, time);
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	GL_ERRORS(); //print any errors produced by this setup code

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("Mouse motion rotates camera; WS moves; R to restart; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Mouse motion rotates camera; WS moves; R to restart; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));

		if(crash)
		{
			lines.draw_text("You are crashed! R to restart",
			glm::vec3(-1.0, 0.0, 0.0),
			glm::vec3(2*H, 0.0f, 0.0f), glm::vec3(0.0f, 2*H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}

		if(escape)
		{
			lines.draw_text("Congratulations! You have escaped",
			glm::vec3(-1.0, 0.0, 0.0),
			glm::vec3(2*H, 0.0f, 0.0f), glm::vec3(0.0f, 2*H, 0.0f),
			glm::u8vec4(0xff, 0x00, 0x00, 0x00));

			lines.draw_text("Congratulations! You have escaped",
			glm::vec3(-1.0 + ofs, 0.0 + ofs, 0.0),
			glm::vec3(2*H, 0.0f, 0.0f), glm::vec3(0.0f, 2*H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0x00, 0x00));
		}
	}
}
