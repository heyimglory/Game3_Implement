#include "load_save_png.hpp"
#include "GL.hpp"
#include "Meshes.hpp"
#include "Scene.hpp"
#include "read_chunk.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/closest_point.hpp>

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <fstream>

static GLuint compile_shader(GLenum type, std::string const &source);
static GLuint link_program(GLuint vertex_shader, GLuint fragment_shader);

// detect the collision between a spinning stuff and a pillar
bool spin_collide_pillars(Scene::Object * spin) {
	if(distance(spin->transform.position, glm::vec3(2.0f, 0.0f, 0.16f))<0.38f || distance(spin->transform.position, glm::vec3(-2.0f, 0.0f, 0.16f))<0.38f) {
		return true;
	}
	else {
		return false;
	}
}

// detect the collision between two spinning stuff
bool spins_collide(Scene::Object * spin1, Scene::Object * spin2) {
	if(distance(spin1->transform.position, spin2->transform.position)<0.55) {
		return true;
	}
	else {
		return false;
	}
}

// detect the collision between the spin stuff and the ball
bool spin_collide_ball(Scene::Object * spin, Scene::Object * ball) {
	glm::vec3 n = glm::normalize(glm::mat4_cast(spin->transform.rotation) * glm::vec4(-1.0f, 0.0f, 0.0f, 1.0f));
	glm::vec3 v = glm::vec3(-1.0f * n.y, n.x, 0.0f);
	glm::vec3 p1 = spin->transform.position + (0.005f * n);
	glm::vec3 p2 = p1 + (0.32f * v);
	glm::vec3 p3 = p2 - (0.01f * n);
	glm::vec3 p4 = spin->transform.position - (0.005f * n);
	glm::vec3 p12 = closestPointOnLine(ball->transform.position, p1, p2);
	glm::vec3 p34 = closestPointOnLine(ball->transform.position, p3, p4);
	glm::vec3 p14 = closestPointOnLine(ball->transform.position, p1, p4);
	glm::vec3 p23 = closestPointOnLine(ball->transform.position, p2, p3);
	float d12 = distance(ball->transform.position, p12);
	float d34 = distance(ball->transform.position, p34);
	float d14 = distance(ball->transform.position, p14);
	float d23 = distance(ball->transform.position, p23);
	
	if(d12 + d34 <= 0.2 && d14 + d23 <= 3.0) {
		return true;
	}
	else {
		return false;
	}
}

int main(int argc, char **argv) {
	//Configuration:
	struct {
		std::string title = "Game3: Spin";
		glm::uvec2 size = glm::uvec2(1024, 512);
	} config;

	//------------  initialization ------------

	//Initialize SDL library:
	SDL_Init(SDL_INIT_VIDEO);

	//Ask for an OpenGL context version 3.3, core profile, enable debug:
	SDL_GL_ResetAttributes();
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	//create window:
	SDL_Window *window = SDL_CreateWindow(
		config.title.c_str(),
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		config.size.x, config.size.y,
		SDL_WINDOW_OPENGL /*| SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI*/
	);

	if (!window) {
		std::cerr << "Error creating SDL window: " << SDL_GetError() << std::endl;
		return 1;
	}

	//Create OpenGL context:
	SDL_GLContext context = SDL_GL_CreateContext(window);

	if (!context) {
		SDL_DestroyWindow(window);
		std::cerr << "Error creating OpenGL context: " << SDL_GetError() << std::endl;
		return 1;
	}

	#ifdef _WIN32
	//On windows, load OpenGL extensions:
	if (!init_gl_shims()) {
		std::cerr << "ERROR: failed to initialize shims." << std::endl;
		return 1;
	}
	#endif

	//Set VSYNC + Late Swap (prevents crazy FPS):
	if (SDL_GL_SetSwapInterval(-1) != 0) {
		std::cerr << "NOTE: couldn't set vsync + late swap tearing (" << SDL_GetError() << ")." << std::endl;
		if (SDL_GL_SetSwapInterval(1) != 0) {
			std::cerr << "NOTE: couldn't set vsync (" << SDL_GetError() << ")." << std::endl;
		}
	}

	//Hide mouse cursor (note: showing can be useful for debugging):
	//SDL_ShowCursor(SDL_DISABLE);

	//------------ opengl objects / game assets ------------

	//shader program:
	GLuint program = 0;
	GLuint program_Position = 0;
	GLuint program_Normal = 0;
	GLuint program_Color = 0;
	GLuint program_mvp = 0;
	GLuint program_itmv = 0;
	GLuint program_to_light = 0;
	{ //compile shader program:
		GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER,
			"#version 330\n"
			"uniform mat4 mvp;\n"
			"uniform mat3 itmv;\n"
			"in vec4 Position;\n"
			"in vec3 Normal;\n"
			"in vec3 Color;\n"
			"out vec3 normal;\n"
			"out vec3 color;\n"
			"void main() {\n"
			"	gl_Position = mvp * Position;\n"
			"	normal = itmv * Normal;\n"
			"	color = Color;\n"
			"}\n"
		);

		GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER,
			"#version 330\n"
			"uniform vec3 to_light;\n"
			"in vec3 normal;\n"
			"in vec3 color;\n"
			"out vec4 fragColor;\n"
			"void main() {\n"
			"	float light = max(0.0, dot(normalize(normal), to_light));\n"
			"	fragColor = vec4(light * color, 1.0);\n"
			"}\n"
		);

		program = link_program(fragment_shader, vertex_shader);

		//look up attribute locations:
		program_Position = glGetAttribLocation(program, "Position");
		if (program_Position == -1U) throw std::runtime_error("no attribute named Position");
		program_Normal = glGetAttribLocation(program, "Normal");
		if (program_Normal == -1U) throw std::runtime_error("no attribute named Normal");
		program_Color = glGetAttribLocation(program, "Color");
		if (program_Color == -1U) throw std::runtime_error("no attribute named Color");
		//look up uniform locations:
		program_mvp = glGetUniformLocation(program, "mvp");
		if (program_mvp == -1U) throw std::runtime_error("no uniform named mvp");
		program_itmv = glGetUniformLocation(program, "itmv");
		if (program_itmv == -1U) throw std::runtime_error("no uniform named itmv");

		program_to_light = glGetUniformLocation(program, "to_light");
		if (program_to_light == -1U) throw std::runtime_error("no uniform named to_light");
	}

	//------------ meshes ------------

	Meshes meshes;

	{ //add meshes to database:
		Meshes::Attributes attributes;
		attributes.Position = program_Position;
		attributes.Normal = program_Normal;
		attributes.Color = program_Color;

		meshes.load("meshes_spin.blob", attributes);
	}

	//------------ scene ------------
	Scene scene;
	//set up camera parameters based on window:
	scene.camera.fovy = glm::radians(40.0f);
	scene.camera.aspect = float(config.size.x) / float(config.size.y);
	scene.camera.near = 0.01f;
	//(transform will be handled in the update function below)
	
	//add some objects from the mesh library:
	auto add_object = [&](std::string const &name, glm::vec3 const &position, glm::quat const &rotation, glm::vec3 const &scale) -> Scene::Object & {
		Mesh const &mesh = meshes.get(name);
		scene.objects.emplace_back();
		Scene::Object &object = scene.objects.back();
		object.transform.position = position;
		object.transform.rotation = rotation;
		object.transform.scale = scale;
		object.vao = mesh.vao;
		object.start = mesh.start;
		object.count = mesh.count;
		object.program = program;
		object.program_mvp = program_mvp;
		object.program_itmv = program_itmv;
		return object;
	};


	{ //read objects to add from "scene.blob":
		std::ifstream file("scene_spin.blob", std::ios::binary);

		std::vector< char > strings;
		//read strings chunk:
		read_chunk(file, "str0", &strings);

		{ //read scene chunk, add meshes to scene:
			struct SceneEntry {
				uint32_t name_begin, name_end;
				glm::vec3 position;
				glm::quat rotation;
				glm::vec3 scale;
			};
			static_assert(sizeof(SceneEntry) == 48, "Scene entry should be packed");

			std::vector< SceneEntry > data;
			read_chunk(file, "scn0", &data);

			for (auto const &entry : data) {
				if (!(entry.name_begin <= entry.name_end && entry.name_end <= strings.size())) {
					throw std::runtime_error("index entry has out-of-range name begin/end");
				}
				std::string name(&strings[0] + entry.name_begin, &strings[0] + entry.name_end);
				add_object(name, entry.position, entry.rotation, entry.scale);
			}
		}
	}
	
	// spins
	std::vector< Scene::Object * > spin_stack;
	spin_stack.emplace_back( &add_object("Spin", glm::vec3(-1.2f, 0.0f, 0.16f), glm::quat(0.0f, 0.0f, 0.0f, 1.0f), glm::vec3(0.05f)) );
	spin_stack.emplace_back( &add_object("Spin", glm::vec3(1.2f, 0.0f, 0.16f), glm::quat(0.0f, 0.0f, 0.0f, -1.0f), glm::vec3(0.05f)) );

	std::vector< float > spin_angle(spin_stack.size(), 0.0f);
	std::vector< float > spin_cloclwise(spin_stack.size(), -1.0f);
	std::vector< bool > spin_changing(spin_stack.size(), false);
	std::vector< glm::vec3 > spin_normal(spin_stack.size(), glm::vec3(0.0f));
	std::vector< bool > hit_ball(spin_stack.size(), false);
	
	spin_angle[1] = (float)(1.0f * M_PI);
	
	std::vector< Scene::Object * > ball_stack;
	ball_stack.emplace_back( &add_object("Ball", glm::vec3(0.0f, 0.0f, 0.2f), glm::quat(0.0f, 0.0f, 0.0f, 1.0f), glm::vec3(0.08f)) );
	
	std::vector< glm::vec3 > ball_velocity(ball_stack.size(), glm::vec3(0.0f));
	std::vector< glm::vec3 > ball_accel(ball_stack.size(), glm::vec3(0.0f));
	
	glm::vec2 mouse = glm::vec2(0.0f, 0.0f); //mouse position in [-1,1]x[-1,1] coordinates

	struct {
		float radius = 6.5f;
		float elevation = (float)(0.38f * M_PI);
		float azimuth = (float)(0.5f * M_PI);
		glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);
	} camera;
	
	//------------ game loop ------------
	
	const Uint8 *keystate = SDL_GetKeyboardState(NULL);

	bool should_quit = false;
	while (true) {
		static SDL_Event evt;
		while (SDL_PollEvent(&evt) == 1) {
			//handle input:
			if (evt.type == SDL_MOUSEMOTION) {
				glm::vec2 old_mouse = mouse;
				mouse.x = (evt.motion.x + 0.5f) / float(config.size.x) * 2.0f - 1.0f;
				mouse.y = (evt.motion.y + 0.5f) / float(config.size.y) *-2.0f + 1.0f;
				if (evt.motion.state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
					camera.elevation += -2.0f * (mouse.y - old_mouse.y);
					camera.azimuth += -2.0f * (mouse.x - old_mouse.x);
				}
			} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
			} else if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_ESCAPE) {
				should_quit = true;
			} else if (evt.type == SDL_QUIT) {
				should_quit = true;
				break;
			}
		}
		if (should_quit) break;

		auto current_time = std::chrono::high_resolution_clock::now();
		static auto previous_time = current_time;
		float elapsed = std::chrono::duration< float >(current_time - previous_time).count();
		previous_time = current_time;

		{ //update game state:
			//spin stuff
			// right player
			if(keystate[SDL_SCANCODE_RIGHT]) {	
				if(spin_stack[0]->transform.position.x >= -2.95f) {
					spin_stack[0]->transform.position.x -= 1.2f * elapsed;
					if(spin_collide_pillars(spin_stack[0]) || spins_collide(spin_stack[0], spin_stack[1])) {
						spin_stack[0]->transform.position.x += 1.2f * elapsed;
					}
				}
			} else if(keystate[SDL_SCANCODE_LEFT]) {
				if(spin_stack[0]->transform.position.x <= 2.95f) {
					spin_stack[0]->transform.position.x += 1.2f * elapsed;
					if(spin_collide_pillars(spin_stack[0]) || spins_collide(spin_stack[0], spin_stack[1])) {
						spin_stack[0]->transform.position.x -= 1.2f * elapsed;
					}
				}
			}
			if(keystate[SDL_SCANCODE_UP]) {	
				if(spin_stack[0]->transform.position.y >= -1.4f) {
					spin_stack[0]->transform.position.y -= 1.2f * elapsed;
					if(spin_collide_pillars(spin_stack[0]) || spins_collide(spin_stack[0], spin_stack[1])) {
						spin_stack[0]->transform.position.y += 1.2f * elapsed;
					}
				}
			} else if(keystate[SDL_SCANCODE_DOWN]) {
				if(spin_stack[0]->transform.position.y <= 1.4f) {
					spin_stack[0]->transform.position.y += 1.2f * elapsed;
					if(spin_collide_pillars(spin_stack[0]) || spins_collide(spin_stack[0], spin_stack[1])) {
						spin_stack[0]->transform.position.y -= 1.2f * elapsed;
					}
				}
			}
			// handle changing the direction of the spinning
			if(keystate[SDL_SCANCODE_SLASH]) {
				if(!spin_changing[0]) {
					spin_cloclwise[0] *= -1.0f;
				}
				spin_changing[0] = true;
			} else {
				spin_changing[0] = false;
			}
			// left player
			if(keystate[SDL_SCANCODE_D]) {	
				if(spin_stack[1]->transform.position.x >= -2.95f) {
					spin_stack[1]->transform.position.x -= 1.2f * elapsed;
					if(spin_collide_pillars(spin_stack[1]) || spins_collide(spin_stack[0], spin_stack[1])) {
						spin_stack[1]->transform.position.x += 1.2f * elapsed;
					}
				}
			} else if(keystate[SDL_SCANCODE_A]) {
				if(spin_stack[1]->transform.position.x <= 2.95f) {
					spin_stack[1]->transform.position.x += 1.2f * elapsed;
					if(spin_collide_pillars(spin_stack[1]) || spins_collide(spin_stack[0], spin_stack[1])) {
						spin_stack[1]->transform.position.x -= 1.2f * elapsed;
					}
				}
			}
			if(keystate[SDL_SCANCODE_W]) {	
				if(spin_stack[1]->transform.position.y >= -1.4f) {
					spin_stack[1]->transform.position.y -= 1.2f * elapsed;
					if(spin_collide_pillars(spin_stack[1]) || spins_collide(spin_stack[0], spin_stack[1])) {
						spin_stack[1]->transform.position.y += 1.2f * elapsed;
					}
				}
			} else if(keystate[SDL_SCANCODE_S]) {
				if(spin_stack[1]->transform.position.y <= 1.4f) {
					spin_stack[1]->transform.position.y += 1.2f * elapsed;
					if(spin_collide_pillars(spin_stack[1]) || spins_collide(spin_stack[0], spin_stack[1])) {
						spin_stack[1]->transform.position.y -= 1.2f * elapsed;
					}
				}
			}
			
			// handle changing the direction of the spinning
			if(keystate[SDL_SCANCODE_Q]) {
				if(!spin_changing[1]) {
					spin_cloclwise[1] *= -1.0f;
				}
				spin_changing[1] = true;
			} else {
				spin_changing[1] = false;
			}				
			
			for(uint32_t i = 0; i < spin_stack.size(); i++) {
				// update rotation
				spin_angle[i] += 5.0f * spin_cloclwise[i] * elapsed;
				if(spin_angle[i] > 2 * M_PI) {
					spin_angle[i] -= (float)(2.0f * M_PI);
				} else if(spin_angle[i] < -2 * M_PI) {
					spin_angle[i] += (float)(2.0f * M_PI);
				}
				spin_stack[i]->transform.rotation = glm::angleAxis(spin_angle[i], glm::vec3(0.0f, 0.0f, 1.0f));
				// update nornal
				spin_normal[i] =  -1.0f * spin_cloclwise[i] * glm::normalize(glm::mat4_cast(spin_stack[i]->transform.rotation) * glm::vec4(-1.0f, 0.0f, 0.0f, 1.0f));

				// detect collision with ball
				if(spin_collide_ball(spin_stack[i], ball_stack[0])) {
					if(!hit_ball[i]) {
						ball_velocity[0] += 2.6f * spin_normal[i];
					}
					hit_ball[i] = true;
				} else {
					hit_ball[i] = false;
				}
			}
			
			// handle friction on different region
			if(distance(ball_stack[0]->transform.position, glm::vec3(0.0f)) < 1.0f) {
				if(std::abs(ball_velocity[0].x) > std::abs(0.001f * normalize(ball_velocity[0]).x)) {
					ball_velocity[0] -= 0.003f * normalize(ball_velocity[0]);
				} else {
					ball_velocity[0] = glm::vec3(0.0f);
				}
			} else {
				if(std::abs(ball_velocity[0].x) > std::abs(0.01f * normalize(ball_velocity[0]).x)) {
					ball_velocity[0] -= 0.02f * normalize(ball_velocity[0]);
				} else {
					ball_velocity[0] = glm::vec3(0.0f);
				}
			}
			
			// detect collision between the ball and the pillars
			if(distance(ball_stack[0]->transform.position, glm::vec3(2.0f, 0.0f, 0.2f))<0.2f) {
				glm::vec3 n = normalize(ball_stack[0]->transform.position - glm::vec3(2.0f, 0.0f, 0.2f));
				float magnitude = ball_velocity[0].x / normalize(ball_velocity[0]).x;
				ball_velocity[0] *= 0.8f;
				ball_velocity[0] += magnitude * n;
			} else if(distance(ball_stack[0]->transform.position, glm::vec3(-2.0f, 0.0f, 0.2f))<0.2f) {
				glm::vec3 n = normalize(ball_stack[0]->transform.position - glm::vec3(-2.0f, 0.0f, 0.2f));
				float magnitude = ball_velocity[0].x / normalize(ball_velocity[0]).x;
				ball_velocity[0] *= 0.8f;
				ball_velocity[0] += magnitude * n;
			}
			ball_stack[0]->transform.position += ball_velocity[0] * elapsed;
			// detect collision between the ball and walls
			if(ball_stack[0]->transform.position.y >= 1.52f || ball_stack[0]->transform.position.y <= -1.52) {
				ball_velocity[0].y *= -1.0f;
			}
			
			// determine winning player
			if(ball_stack[0]->transform.position.x >= 3.1f) {
				ball_velocity[0] = glm::vec3(0.0f);
				ball_stack[0]->transform.position = glm::vec3(0.0f, 0.0f, -1.0f);
				std::vector< Scene::Object * > win_stack;
				win_stack.emplace_back( &add_object("R_win", glm::vec3(0.0f, 0.8f, 1.8f), glm::quat(0.0f, 0.0f, 0.0f, 1.0f), glm::vec3(2.0f, 1.0f, 1.0f)) );
				win_stack[0]->transform.rotation = glm::angleAxis(-0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
			} else if(ball_stack[0]->transform.position.x <= -3.1f) {
				ball_velocity[0] = glm::vec3(0.0f);
				ball_stack[0]->transform.position = glm::vec3(0.0f, 0.0f, -1.0f);
				std::vector< Scene::Object * > win_stack;
				win_stack.emplace_back( &add_object("L_win", glm::vec3(0.0f, 0.8f, 1.8f), glm::quat(0.0f, 0.0f, 0.0f, 1.0f), glm::vec3(2.0f, 1.0f, 1.0f)) );
				win_stack[0]->transform.rotation = glm::angleAxis(-0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
			}

			//camera:
			scene.camera.transform.position = camera.radius * glm::vec3(
				std::cos(camera.elevation) * std::cos(camera.azimuth),
				std::cos(camera.elevation) * std::sin(camera.azimuth),
				std::sin(camera.elevation)) + camera.target;

			glm::vec3 out = -glm::normalize(camera.target - scene.camera.transform.position);
			glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);
			up = glm::normalize(up - glm::dot(up, out) * out);
			glm::vec3 right = glm::cross(up, out);
			
			scene.camera.transform.rotation = glm::quat_cast(
				glm::mat3(right, up, out)
			);
			scene.camera.transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);
		}

		//draw output:
		glClearColor(0.5, 0.5, 0.5, 0.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


		{ //draw game state:
			glUseProgram(program);
			glUniform3fv(program_to_light, 1, glm::value_ptr(glm::normalize(glm::vec3(0.0f, 0.0f, 2.0f))));
			scene.render();
		}


		SDL_GL_SwapWindow(window);
	}


	//------------  teardown ------------

	SDL_GL_DeleteContext(context);
	context = 0;

	SDL_DestroyWindow(window);
	window = NULL;

	return 0;
}



static GLuint compile_shader(GLenum type, std::string const &source) {
	GLuint shader = glCreateShader(type);
	GLchar const *str = source.c_str();
	GLint length = source.size();
	glShaderSource(shader, 1, &str, &length);
	glCompileShader(shader);
	GLint compile_status = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
	if (compile_status != GL_TRUE) {
		std::cerr << "Failed to compile shader." << std::endl;
		GLint info_log_length = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector< GLchar > info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetShaderInfoLog(shader, info_log.size(), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		glDeleteShader(shader);
		throw std::runtime_error("Failed to compile shader.");
	}
	return shader;
}

static GLuint link_program(GLuint fragment_shader, GLuint vertex_shader) {
	GLuint program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);
	GLint link_status = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &link_status);
	if (link_status != GL_TRUE) {
		std::cerr << "Failed to link shader program." << std::endl;
		GLint info_log_length = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector< GLchar > info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetProgramInfoLog(program, info_log.size(), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		throw std::runtime_error("Failed to link program");
	}
	return program;
}
