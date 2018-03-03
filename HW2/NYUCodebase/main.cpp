#ifdef _WINDOWS
	#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "ShaderProgram.h"
#include <vector>
#include <unordered_map>
#include <math.h>

#ifdef _WINDOWS
	#define RESOURCE_FOLDER ""
#else
	#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif



SDL_Window* displayWindow;
Matrix projectionMatrix;
Matrix modelMatrix;
Matrix viewMatrix;

float elapsed;

//seconds since program started
float get_runtime(){
	float ticks = (float)SDL_GetTicks() / 1000.0f;
	return ticks;
}


GLuint LoadTexture(const char* filePath, int* width, int* height){
	int w, h, comp;
	unsigned char* image = stbi_load(filePath, &w, &h, &comp, STBI_rgb_alpha);

	if (image == NULL){
		std::cout << "Unable to load image. Make sure the path is correct\n";
		assert(false);
	}

	*width = w;
	*height = h;

	GLuint retTexture;
	glGenTextures(1, &retTexture);
	glBindTexture(GL_TEXTURE_2D, retTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	stbi_image_free(image);
	return retTexture;
}

class Sprite{
public:
	GLuint texture_id;
	int pixel_width;
	int pixel_height;
	float aspect_ratio;
	float x_size = 0.4f;
	float y_size = 0.4f;


	Sprite(const std::string& file_path){
		texture_id = LoadTexture(file_path.c_str(), &pixel_width, &pixel_height);

		aspect_ratio = (pixel_width*1.0f) / (pixel_height*1.0f);
		x_size *= aspect_ratio;
	}

	void set_size(int x_size_, int y_size_){
		x_size = x_size_;
		y_size = y_size_;
	}


	void set_size(int x_size_){
		x_size = x_size_;
		y_size = x_size;
		x_size = x_size_ * aspect_ratio;
	}



	void draw_sprite(Sprite& sprite, ShaderProgram& program){
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		//Draws sprites pixel perfect with no blur
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);


		float verts[] = {
			-x_size, -y_size,
			x_size, -y_size,
			x_size, y_size,
			-x_size, -y_size,
			x_size, y_size,
			-x_size, y_size
		};

		glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, verts);
		glEnableVertexAttribArray(program.positionAttribute);

		float texCoords[] = {
			0.0, 1.0,
			1.0, 1.0,
			1.0, 0.0,
			0.0, 1.0,
			1.0, 0.0,
			0.0, 0.0
		};
		glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
		glEnableVertexAttribArray(program.texCoordAttribute);

		glBindTexture(GL_TEXTURE_2D, sprite.texture_id);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisableVertexAttribArray(program.positionAttribute);
		glDisableVertexAttribArray(program.texCoordAttribute);
	}



	void draw(ShaderProgram& program){
		draw_sprite(*this, program);
	}
};



class Animation{
public:
	std::vector<Sprite> sprites;
	int animation_count;

	float last_change = 0;
	float interval = .085; //milliseconds (ms)
	int current_index = 0;

	Animation(){};

	Animation(const std::string& animation_name, int animation_count_){
		animation_count = animation_count_;
		for (int x = 0; x < animation_count_; x++){
			std::string file_path = RESOURCE_FOLDER"";
			file_path += "resources/" + animation_name + "_" + std::to_string(x + 1) + ".png";
			std::cout << "Loading file: " << file_path << std::endl;
			Sprite new_sprite(file_path);


			sprites.push_back(new_sprite);
		}
	}


	void update(){
		float current_runtime = get_runtime();
		if (current_runtime - last_change > interval){
			current_index += 1;
			if (current_index >= sprites.size()){
				current_index = 0;
			}
			last_change = get_runtime();
		}
	}


	


	void draw(ShaderProgram& program){
		Sprite current_sprite = sprites[current_index];
		current_sprite.draw(program);
	}
};




float radians_to_degrees(float radians) {
	return radians * (180.0 / M_PI);
}

float degrees_to_radians(float degrees){
	return ((degrees)* M_PI / 180.0);
}


class GameObject{
public:
	std::string name = "";
	float last_change = 0;
	float interval = .085; //milliseconds (ms)

	std::unordered_map<std::string, Animation> animations;
	std::string current_animation_name = "";
	float pos[3];
	float color[4];
	std::vector<float> verts;
	std::string draw_mode = "texture";
	float size[3];
	float velocity[3];
	float direction[3];
	float total_move_amount = 0;
	float movement_angle = degrees_to_radians (-180);

	GameObject(const std::string& name_){
		set_pos(0, 0);
		name = name_;
		set_animation("run");
	}

	void set_pos(float x, float y){
		pos[0] = x;
		pos[1] = y;
	}

	void set_pos(float x, float y, float z){
		set_pos(x, y);
		pos[2] = z;
	}

	void add_animation(const std::string& animation_name, int animation_count){
		Animation run_animation(name + "_" + animation_name, animation_count);
		animations[animation_name] = run_animation;
	}

	void set_animation(const std::string& animation_name){
		current_animation_name = animation_name;
	}

	void move_y(float delta_y){
		pos[1] += delta_y;
	}


	void move_x(float delta_x){
		pos[0] += delta_x;
	}

	void set_x(float x_){
		pos[0] = x_;
	}



	void update(){	
		//pos[0] += std::cosf(movement_angle) * elapsed * 1.0f;
		//pos[1] += std::sinf(movement_angle) * elapsed * 1.0f;


		pos[0] += direction[0] * elapsed * velocity[0];
		pos[1] += direction[1] * elapsed * velocity[1];

		if (animations.count(current_animation_name)){
			animations[current_animation_name].update();
		}
	}


	void set_direction_x(float x_){
		direction[0] = x_;
	}

	void set_direction_y(float y_){
		direction[1] = y_;
	}

	void set_direction(float x_, float y_){
		direction[0] = x_;
		direction[1] = y_;
	}

	float x(){
		return pos[0];
	}

	float y(){
		return pos[1];
	}

	float z(){
		return pos[2];
	}


	float top_left_x(){
		return pos[0] - (size[0] / 2);
	}

	float top_left_y(){
		return pos[1] - (size[1] / 2);
	}

	void set_verts(std::vector<float> arr){
		verts = arr;
	}


	void set_size(float width_, float height_){
		size[0] = width_;
		size[1] = height_;
	}

	void set_draw_mode(std::string mode_){
		draw_mode = mode_;
	}

	void set_velocity(float x_, float y_, float z_=0){
		velocity[0] = x_;
		velocity[1] = y_;
		velocity[2] = z_;
	}


	void set_color(float r, float g, float b, float a){
		color[0] = r;
		color[1] = g;
		color[2] = b;
		color[3] = a;
	}



	void draw(ShaderProgram& program){
		if (draw_mode == "texture"){
			if (animations.count(current_animation_name)){
				modelMatrix.Identity();
				modelMatrix.Translate(x(), y(), z());
				program.SetModelMatrix(modelMatrix);

				animations[current_animation_name].draw(program);
			}
		}
		else if (draw_mode == "shape"){
			modelMatrix.Identity();
			modelMatrix.Translate(x(), y(), z());
			program.SetModelMatrix(modelMatrix);	
			program.SetColor(color[0], color[1], color[2], color[3]);

			glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, verts.data());
			glEnableVertexAttribArray(program.positionAttribute);
			glDrawArrays(GL_QUADS, 0, 4);

			glDisableVertexAttribArray(program.positionAttribute);
		}


	}



	float width(){
		return size[0];
	}


	float height(){
		return size[1];
	}
};


bool check_box_collision(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2){

	if (
		y1 + h1 < y2 ||
		y1 > y2 + h2 ||
		x1 + w1 < x2 ||
		x1 > x2 + w2) {

		return false;
	}
	else{
		return true;
	}	
}

bool check_box_collision(GameObject& obj1, GameObject& obj2){
	return check_box_collision(obj1.top_left_x(), obj1.top_left_y(), obj1.width(), obj1.height(), obj2.top_left_x(), obj2.top_left_y(), obj2.width(), obj2.height());
}




std::vector<float> quad_verts(float width, float height){
	return {
		-width / 2, height / 2, //top left
		-width / 2, -height / 2, // bottom left
		width / 2, -height / 2, //bottom right
		width / 2, height / 2 //top right
	};
}


void reset_ball(GameObject& ball_obj){
	ball_obj.set_pos(0,0);
	ball_obj.set_direction(-1, 0);
}



int left_score = 0;
int right_score = 0;

int main(int argc, char *argv[]) {
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("Pong - afl294", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1000, 600, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
	#ifdef _WINDOWS
		glewInit();
	#endif



	glViewport(0, 0, 1000, 600);


	ShaderProgram tex_program;
	tex_program.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
	ShaderProgram shape_program;
	shape_program.Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");

	float lastFrameTicks = 0.0f;


	float padel_width = 0.15f;
	float padel_height = 0.5f;

	GameObject left_padel("left_padel");
	left_padel.set_pos(-3.55 + (padel_width/2), 0);
	left_padel.set_draw_mode("shape");
	left_padel.set_color(1, 1, 1, 1);
	left_padel.set_size(padel_width, padel_height);
	left_padel.set_verts(quad_verts(padel_width, padel_height));


	GameObject right_padel("right_padel");
	right_padel.set_pos(3.55 - (padel_width / 2), 0);
	right_padel.set_draw_mode("shape");
	right_padel.set_color(1, 1, 1, 1);
	right_padel.set_size(padel_width, padel_height);
	right_padel.set_verts(quad_verts(padel_width, padel_height));


	GameObject background("background");
	background.set_pos(0, 0);
	background.set_draw_mode("shape");
	background.set_color(0, 0, 0, 1);
	background.set_size(3.55 * 2, 2.0 * 2);
	background.set_verts(quad_verts(background.width(), background.height()));


	GameObject ball("ball");
	ball.set_pos(1, 0);
	ball.set_draw_mode("shape");
	ball.set_color(0.5f, 0, 0, 1);
	ball.set_size(0.2f, 0.2f);
	ball.set_verts(quad_verts(ball.width(), ball.height()));
	ball.set_direction(-1, 0.5f);
	ball.set_velocity(4.0f, 4.0f);


	float screen_left = -3.55;
	float screen_right = 3.55;
	float screen_top = 2.0f;
	float screen_bottom = -2.0f;
	projectionMatrix.SetOrthoProjection(screen_left, screen_right, screen_bottom, screen_top, -1.0f, 1.0f);


	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	SDL_Event event;
	bool done = false;
	while (!done) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}


			if (event.type == SDL_KEYDOWN){
				switch (event.key.keysym.sym) {
					case SDLK_LEFT:  
						break;
					case SDLK_RIGHT: 
						break;
					case SDLK_UP:    
						break;
					case SDLK_DOWN:  
						break;
				}
			}
		}


		float ticks = get_runtime();
		elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;

		Uint8* keysArray = const_cast <Uint8*> (SDL_GetKeyboardState(NULL));

		if (keysArray[SDL_SCANCODE_RETURN]){
			printf("MESSAGE: <RETURN> is pressed...\n");
		}	

		if (keysArray[SDL_SCANCODE_W]){
			left_padel.move_y(2.5f * elapsed);
		}
			
		if (keysArray[SDL_SCANCODE_S]){
			left_padel.move_y(-2.5f * elapsed);
		}


		if (keysArray[SDL_SCANCODE_UP]){
			right_padel.move_y(2.5f * elapsed);
		}

		if (keysArray[SDL_SCANCODE_DOWN]){
			right_padel.move_y(-2.5f * elapsed);
		}

		if (check_box_collision(left_padel, ball)){
			float y_angle_bounce = ((ball.y() - left_padel.y()) / left_padel.height());
			ball.set_direction_x(std::fabs(ball.direction[0]));
			ball.direction[1] = y_angle_bounce;
		}


		if (check_box_collision(right_padel, ball)){
			float y_angle_bounce = ((ball.y() - right_padel.y()) / right_padel.height());
			ball.set_direction_x(-std::fabs(ball.direction[0]));
			ball.direction[1] = y_angle_bounce;
		}



		//Check if ball hits bottom of screen
		if (ball.y() < screen_bottom + ball.height() / 2){
			ball.direction[1] = std::fabs(ball.direction[1]);
		}

		//Check if ball hits top of screen
		if (ball.y() > screen_top - ball.height() / 2){
			ball.direction[1] = -std::fabs(ball.direction[1]);
		}

		//Check if ball passes left side of screen
		if (ball.x() < screen_left){
			reset_ball(ball);
			right_score += 1;
		}

		//Check if ball passes right side of screen
		if (ball.x() > screen_right){
			reset_ball(ball);
			left_score += 1;
		}


		std::cout << "Left: " << left_score << "   -   Right Score: " << right_score << std::endl;


		ball.update();

		glClear(GL_COLOR_BUFFER_BIT);
		

		shape_program.SetModelMatrix(modelMatrix);
		shape_program.SetProjectionMatrix(projectionMatrix);
		shape_program.SetViewMatrix(viewMatrix);
		glUseProgram(shape_program.programID);


		background.draw(shape_program);
		left_padel.draw(shape_program);
		right_padel.draw(shape_program);
		ball.draw(shape_program);


		SDL_GL_SwapWindow(displayWindow);
	}

	SDL_Quit();
	return 0;
}
