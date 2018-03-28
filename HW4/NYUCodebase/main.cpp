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
#include <algorithm> //std::remove_if
#include <time.h>  
#ifdef _WINDOWS
	#define RESOURCE_FOLDER ""
#else
	#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif
#include "FlareMap.h"


SDL_Window* displayWindow;
Matrix projectionMatrix;
Matrix modelMatrix;
Matrix viewMatrix;
ShaderProgram* tex_program;
ShaderProgram* shape_program;
GLuint font_texture;
float elapsed;
bool done = false;
float font_sheet_width = 0;
float font_sheet_height = 0;

float screen_left = -3.55;
float screen_right = 3.55;
float screen_top = 2.0f;
float screen_bottom = -2.0f;

float screen_width = screen_right - screen_left;
float screen_height = screen_top - screen_bottom;

int TILE_SIZE = 8; //pixel size
float tile_world_size = 0.18f;

enum GameMode { STATE_MAIN_MENU, STATE_GAME_LEVEL, STATE_GAME_OVER, STATE_GAME_WON };
GameMode mode;

FlareMap map;

//seconds since program started
float get_runtime(){
	float ticks = (float)SDL_GetTicks() / 1000.0f;
	return ticks;
}


GLuint LoadTexture(const char* filePath, float* width, float* height){
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

std::vector<float> quad_verts(float width, float height){
	return{
		-width / 2, height / 2, //top left
		-width / 2, -height / 2, // bottom left
		width / 2, -height / 2, //bottom right
		width / 2, height / 2 //top right
	};
}



void draw_text(std::string text, float x, float y, int fontTexture, float size, float spacing){



	float texture_size = 1.0 / 16.0f;

	std::vector<float> vertexData;
	std::vector<float> texCoordData;

	for (int i = 0; i < text.size(); i++){
		int spriteIndex = (int)text[i];
		float texture_x = (float)(spriteIndex % 16) / 16.0f;
		float texture_y = (float)(spriteIndex / 16) / 16.0f;

		vertexData.insert(vertexData.end(), {
			((spacing * i) + (-0.5f * size)), 0.5f * size,
			((spacing * i) + (-0.5f * size)), -0.5f * size,
			((spacing * i) + (0.5f * size)), 0.5f * size,
			((spacing * i) + (0.5f * size)), -0.5f * size,
			((spacing * i) + (0.5f * size)), 0.5f * size,
			((spacing * i) + (-0.5f * size)), -0.5f * size,
		});


		texCoordData.insert(texCoordData.end(), {
			texture_x, texture_y,
			texture_x, texture_y + texture_size,
			texture_x + texture_size, texture_y,
			texture_x + texture_size, texture_y + texture_size,
			texture_x + texture_size, texture_y,
			texture_x, texture_y + texture_size
		});
	}



	modelMatrix.Identity();
	modelMatrix.Translate(x, y, 0);
	tex_program->SetModelMatrix(modelMatrix);
	tex_program->SetProjectionMatrix(projectionMatrix);
	tex_program->SetViewMatrix(viewMatrix);
	glUseProgram(tex_program->programID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	//Draws sprites pixel perfect with no blur
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glVertexAttribPointer(tex_program->positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
	glEnableVertexAttribArray(tex_program->positionAttribute);

	glVertexAttribPointer(tex_program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
	glEnableVertexAttribArray(tex_program->texCoordAttribute);

	glBindTexture(GL_TEXTURE_2D, fontTexture);
	glDrawArrays(GL_TRIANGLES, 0, text.size() * 6);

	glDisableVertexAttribArray(tex_program->positionAttribute);
	glDisableVertexAttribArray(tex_program->texCoordAttribute);

}


class Sprite{
public:
	GLuint texture_id;
	float width;
	float height;
	float aspect_ratio;
	float x_size = 0.4f;
	float y_size = 0.4f;
	float size = 0.4f;
	float u;
	float v;
	bool sheet = false;


	float x = 0;
	float y = 0;
	float z = 0;
	bool update_position = false;

	Sprite(const std::string& file_path){
		texture_id = LoadTexture(file_path.c_str(), &width, &height);

		aspect_ratio = (width*1.0f) / (height*1.0f);
		x_size *= aspect_ratio;

		//std::cout << "Loading sprite " << width << " " << height << std::endl;
	}



	Sprite(GLuint texture_id_, float u_, float v_, float width_, float height_, float size_){
		u = u_;
		v = v_;
		width = width_; 
		height = height_;
		size = size_;

		texture_id = texture_id_;

		aspect_ratio = (width*1.0f) / (height*1.0f);
		sheet = true;
	}


	void set_size(int x_size_, int y_size_){
		x_size = x_size_;
		y_size = y_size_;
		size = x_size;

	}



	void set_size(int x_size_){
		x_size = x_size_;
		y_size = x_size;
		x_size = x_size_ * aspect_ratio;
	}

	void set_pos(float x_, float y_, float z_){
		x = x_;
		y = y_;
		z = z_;
		update_position = true;
	}


	void set_vert_pos(float x_, float y_, float z_){
		x = x_;
		y = y_;
		z = z_;
		update_position = true;
	}


	std::vector<float> get_verts(){

		//std::vector<float> verts = {
		//	-x_size, -y_size,
		//	x_size, -y_size,
		//	x_size, y_size,
		//	-x_size, -y_size,
		//	x_size, y_size,
		//	-x_size, y_size
		//};


		std::vector<float> verts = {
			x-x_size, y-y_size,
			x+x_size, y-y_size,
			x+x_size, y+y_size,
			x-x_size, y-y_size,
			x+x_size, y+y_size,
			x-x_size, y+y_size
		};


		//if (sheet){
		//	float aspect = width / height;
		//	verts = {
		//		-0.5f * size * aspect, -0.5f * size,
		//		0.5f * size * aspect, 0.5f * size,
		//		-0.5f * size * aspect, 0.5f * size,
		//		0.5f * size * aspect, 0.5f * size,
		//		-0.5f * size * aspect, -0.5f * size,
		//		0.5f * size * aspect, -0.5f * size };
		//}


		if (sheet){
			float aspect = width / height;
			verts = {
				x - 0.5f * size * aspect, y - 0.5f * size,
				x + 0.5f * size * aspect, y + 0.5f * size,
				x - 0.5f * size * aspect, y + 0.5f * size,
				x + 0.5f * size * aspect, y + 0.5f * size,
				x - 0.5f * size * aspect, y - 0.5f * size,
				x + 0.5f * size * aspect, y - 0.5f * size };
		}



		return verts;
	}

	std::vector<float> get_tex_coords(){
		std::vector<float> tex_coords = {
			0.0f, 1.0f,
			1.0f, 1.0f,
			1.0f, 0.0f,
			0.0f, 1.0f,
			1.0f, 0.0f,
			0.0f, 0.0f
		};

		if (sheet){
			tex_coords = {
				u, v + height,
				u + width, v,
				u, v,
				u + width, v,
				u, v + height,
				u + width, v + height
			};
		}

		return tex_coords;
	}



	void draw(){
		if (update_position){
			tex_program->SetModelMatrix(modelMatrix);
			tex_program->SetProjectionMatrix(projectionMatrix);
			tex_program->SetViewMatrix(viewMatrix);

			modelMatrix.Identity();
			modelMatrix.Translate(x,y,z);
			tex_program->SetModelMatrix(modelMatrix);
			tex_program->SetViewMatrix(viewMatrix);

			glUseProgram(tex_program->programID);
		}


		glUseProgram(tex_program->programID);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		//Draws sprites pixel perfect with no blur
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		std::vector<float> verts = get_verts();
		std::vector<float> texCoords = get_tex_coords();

		glVertexAttribPointer(tex_program->positionAttribute, 2, GL_FLOAT, false, 0, verts.data());
		glEnableVertexAttribArray(tex_program->positionAttribute);

		glVertexAttribPointer(tex_program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords.data());
		glEnableVertexAttribArray(tex_program->texCoordAttribute);

		glBindTexture(GL_TEXTURE_2D, texture_id);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisableVertexAttribArray(tex_program->positionAttribute);
		glDisableVertexAttribArray(tex_program->texCoordAttribute);

	}


};



void batch_draw(int texture_id, std::vector<float>& verts, std::vector<float>& texCoords){
	tex_program->SetModelMatrix(modelMatrix);
	tex_program->SetProjectionMatrix(projectionMatrix);
	tex_program->SetViewMatrix(viewMatrix);

	modelMatrix.Identity();
	modelMatrix.Translate(0, 0, 0);
	tex_program->SetModelMatrix(modelMatrix);
	tex_program->SetViewMatrix(viewMatrix);

	glUseProgram(tex_program->programID);

	glUseProgram(tex_program->programID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	//Draws sprites pixel perfect with no blur
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);


	glVertexAttribPointer(tex_program->positionAttribute, 2, GL_FLOAT, false, 0, verts.data());
	glEnableVertexAttribArray(tex_program->positionAttribute);

	glVertexAttribPointer(tex_program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords.data());
	glEnableVertexAttribArray(tex_program->texCoordAttribute);

	glBindTexture(GL_TEXTURE_2D, texture_id);
	glDrawArrays(GL_TRIANGLES, 0, verts.size() / 2);

	glDisableVertexAttribArray(tex_program->positionAttribute);
	glDisableVertexAttribArray(tex_program->texCoordAttribute);

}



class Animation{
public:
	std::vector<Sprite> sprites;
	int animation_count = 0;

	float last_change = 0;
	float interval = .085; //milliseconds (ms)
	int current_index = 0;

	Animation(){};

	Animation(const std::string& animation_name, int animation_count_){
		animation_count = animation_count_;
		for (int x = 0; x < animation_count_; x++){
			std::string file_path = RESOURCE_FOLDER"";
			file_path += "resources/" + animation_name + "_" + std::to_string(x + 1) + ".png";
			//std::cout << "Loading file: " << file_path << std::endl;
			Sprite new_sprite(file_path);


			sprites.push_back(new_sprite);
		}
	}


	void add_sprite(Sprite new_sprite){
		sprites.push_back(new_sprite);
	}



	void advance(){
		current_index += 1;
		if (current_index >= sprites.size()){
			current_index = 0;
		}
	}

	void update(){
		float current_runtime = get_runtime();
		if (current_runtime - last_change > interval){
			advance();
			last_change = get_runtime();
		}
	}


	void update_size(float width_, float height_){
		for (int x = 0; x < sprites.size(); x++){
			//sprites[x].set_size(width_, height_);
		}
	}
	

	void draw(){
		sprites[current_index].draw();
	}

};




float radians_to_degrees(float radians) {
	return radians * (180.0 / M_PI);
}

float degrees_to_radians(float degrees){
	return ((degrees)* M_PI / 180.0);
}


class Vector3 {
public:

	float x;
	float y;
	float z;

	void init(float x_, float y_, float z_){
		x = x_;
		y = y_;
		z = z_;
	}

	Vector3(float x_, float y_, float z_){
		init(x_, y_, z_);
	}

	Vector3(){
		init(0, 0, 0);
	}


	void clear(){
		init(0, 0, 0);
	}

};

class GameObject{
public:
	std::string name = "";
	float last_change = 0;
	float interval = .085; //milliseconds (ms)

	std::unordered_map<std::string, Animation> animations;
	std::unordered_map<std::string, std::string> strings;
	std::string current_animation_name = "";
	float pos[3];
	float start_pos[3];
	float color[4];
	std::vector<float> verts;
	std::string draw_mode = "texture";
	bool apply_velocity = true;
	float size[3];
	float velocity[3];
	Vector3 acceleration;

	float direction[3];
	float total_move_amount = 0;
	float movement_angle = degrees_to_radians (-180);
	float life = 10;
	int lives = 5;
	bool destroyed = false;
	bool isStatic = false; //If static, no gravity, no movement, no collision checking
	bool apply_gravity = true;
	float created_at = 0;

	void init(){
		created_at = get_runtime();
		set_pos(0, 0);
		set_animation("idle");
		direction[0] = 1;
	}


	GameObject(){
		init();
	}

	GameObject(const std::string& name_){
		init();
		
		name = name_;

		
	}


	void set_name(const std::string& str_){
		name = str_;
	}


	void set_pos(float x, float y){
		pos[0] = x;
		pos[1] = y;
	}

	void destroy(){
		destroyed = true;
	}

	void set_pos(float x, float y, float z, bool initial=false){
		set_pos(x, y);
		pos[2] = z;

		if (initial){
			start_pos[0] = x;
			start_pos[1] = y;
			start_pos[2] = z;
		}
	}

	float timeAlive(){
		return get_runtime() - created_at;
	}

	void add_animation(const std::string& animation_name, int animation_count){
		Animation run_animation(name + "_" + animation_name, animation_count);
		animations[animation_name] = run_animation;
	}


	void add_animation(const std::string& animation_name, Animation animation){
		animations[animation_name] = animation;
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


	void move_up(){
		move_y(3 * elapsed);
	}

	void move_down(){
		move_y(-3 * elapsed);
	}

	void move_left(){
		direction[0] = -1;
		//move_x(-velocity[0] * elapsed);
		velocity[0] = -5.0f;
	}


	void move_right(){
		direction[0] = 1;
		//move_x(velocity[0] * elapsed);
		velocity[0] = 5.0f;
	}

	void stop_moving(){
		acceleration.x = 0;
		velocity[0] = 0;
	}



	void jump(){
		if (is_colliding_bottom()){
			velocity[1] = 4.5f;
			move_y(0.1f);
			jumping = true;
		}
	}

	void flip_x_direction(){
		direction[0] *= -1.0f;
		velocity[0] = fabs(velocity[0]) * direction[0];
	}

	void set_x(float x_){
		pos[0] = x_;
	}


	void set_y(float y_){
		pos[1] = y_;
	}


	void worldToTileCoord(float worldX, float worldY, int* gridX, int* gridY){
		*gridX = (int)(worldX / TILE_SIZE);
		*gridY = (int)(-worldY / TILE_SIZE);
	}

	bool collidedTop = false;
	bool collidedBottom = false;
	bool collidedLeft = false;
	bool collidedRight = false;

	int last_grid_x = 0;
	int last_grid_y = 0;
	int colliding_bottom(){
		//Check for collision on feet with ground
		float test_x = x();
		float test_y = y() - height() / 2;

		int grid_x = (int)(test_x / tile_world_size);
		int grid_y = (int)(-(test_y) / tile_world_size);


		//layer index 3 = platform
		//worldToTileCoord(x(), y() + (0.1f), &grid_x, &grid_y);
		try{
			int data = map.layers[3][grid_y][grid_x];
			if (data > 0){
				last_grid_x = grid_x;
				last_grid_y = grid_y;
				return data;
			}
		}
		catch (std::exception& e){
			std::cout << e.what() << std::endl;
			return 0;
		}

		/////////
		grid_x = (int)(test_x / tile_world_size) - 1;
		try{
			int data = map.layers[3][grid_y][grid_x];
			if (data > 0){
				last_grid_x = grid_x;
				last_grid_y = grid_y;
				return data;
			}
		}
		catch (std::exception& e){
			std::cout << e.what() << std::endl;
			return 0;
		}
		//////////////////////
		grid_x = (int)(test_x / tile_world_size) + 1;
		try{
			int data = map.layers[3][grid_y][grid_x];
			if (data > 0){
				last_grid_x = grid_x;
				last_grid_y = grid_y;
				return data;
			}
		}
		catch (std::exception& e){
			std::cout << e.what() << std::endl;
			return 0;
		}



		return 0;
	}

	bool is_colliding_bottom(){
		int col_data = colliding_bottom();
		if (col_data > 0){
			return true;
		}
		else{
			return false;
		}
	}

	double distance(float x1, float y1, float x2, float y2) {
		const double x_diff = x1 - x2;
		const double y_diff = y1 - y2;
		return std::sqrt(x_diff * x_diff + y_diff * y_diff);
	}

	void check_for_collisions(){
		//Prevent object from going off left side of screen
		if (x() - (width() / 2) < 0){
			pos[0] = 0 + width() / 2;
		}

		//Prevent object from going off right side of screen
		if (x() + (width() / 2) > map.mapWidth * tile_world_size){
			pos[0] = (map.mapWidth * tile_world_size) - (width() / 2);
		}

		//Prevent object from going off top
		if (y() + (height() / 2) > 0){
			pos[1] = 0 - height() / 2;
		}

		//Prevent object from going off bottom
		if (y() - (height() / 2) < -map.mapHeight * tile_world_size){
			pos[1] += 2.8f;
		}


		if (is_colliding_bottom()){
			jumping = false;
			velocity[1] = 0;
			pos[1] += fabs((last_grid_y * tile_world_size) + (y() - height() / 2.0f));
			collidedBottom = true;
		}
		else{
			collidedBottom = false;
		}


		
		//check for head collision
		int grid_x_2 = (int)(x() / 0.18f);
		int grid_y_2 = (int)fabs(-(y() + height() / 2.6f) / 0.18f);
		int data2 = map.layers[3][grid_y_2][grid_x_2];
		if (data2 > 0){
			//std::cout << "Tile at heroes head: " << data2 << "   grid_x, grid_y" << grid_x_2 << "," << grid_y_2 << std::endl;
			velocity[1] = 0.0f;
			pos[1] -= (y() + height() / 2.6f) - ((-tile_world_size * grid_y_2) - tile_world_size);
			collidedTop = true;
		}
		else{
			collidedTop = false;
		}
		//Head right side
		grid_x_2 = (int)(x() / 0.18f) + 1;
		data2 = map.layers[3][grid_y_2][grid_x_2];
		if (data2 > 0){
			//std::cout << "Tile at heroes head: " << data2 << "   grid_x, grid_y" << grid_x_2 << "," << grid_y_2 << std::endl;
			velocity[1] = 0.0f;
			pos[1] -= (y() + height() / 2.6f) - ((-tile_world_size * grid_y_2) - tile_world_size);
			collidedTop = true;
		}
		else{
			collidedTop = false;
		}
		//Head left side
		grid_x_2 = (int)(x() / 0.18f) - 1;
		data2 = map.layers[3][grid_y_2][grid_x_2];
		if (data2 > 0){
			//std::cout << "Tile at heroes head: " << data2 << "   grid_x, grid_y" << grid_x_2 << "," << grid_y_2 << std::endl;
			velocity[1] = 0.0f;
			pos[1] -= (y() + height() / 2.6f) - ((-tile_world_size * grid_y_2) - tile_world_size);
			collidedTop = true;
		}
		else{
			collidedTop = false;
		}



		//Check for collision on left:
		int grid_x_3 = (int)((x() - (width() / 2)) / 0.18f);
		int grid_y_3 = (int)(-(y()) / 0.18f);
		int data3 = map.layers[3][grid_y_3][grid_x_3];

		if (data3 > 0){
			//std::cout << "Tile at heroes head: " << data2 << "   grid_x, grid_y" << grid_x_2 << "," << grid_y_2 << std::endl;
			pos[0] -= (x() - width() / 2) - ((tile_world_size * grid_x_3) + tile_world_size);
			acceleration.x = 0;
			//velocity[0] = 0;
			collidedLeft = true;
		}
		else{
			collidedLeft = false;
		}

		//Collision on left upper
		grid_y_3 = (int)(-(y()) / 0.18f) + 1;
		data3 = map.layers[3][grid_y_3][grid_x_3];

		if (data3 > 0){
			//std::cout << "Tile at heroes head: " << data2 << "   grid_x, grid_y" << grid_x_2 << "," << grid_y_2 << std::endl;
			pos[0] -= (x() - width() / 2) - ((tile_world_size * grid_x_3) + tile_world_size);
			acceleration.x = 0;
			//velocity[0] = 0;
			collidedLeft = true;
		}
		else{
			collidedLeft = false;
		}

		//Collision on left lower
		grid_y_3 = (int)(-(y()) / 0.18f) - 1;
		data3 = map.layers[3][grid_y_3][grid_x_3];

		if (data3 > 0){
			//std::cout << "Tile at heroes head: " << data2 << "   grid_x, grid_y" << grid_x_2 << "," << grid_y_2 << std::endl;
			pos[0] -= (x() - width() / 2) - ((tile_world_size * grid_x_3) + tile_world_size);
			acceleration.x = 0;
			//velocity[0] = 0;
			collidedLeft = true;
		}
		else{
			collidedLeft = false;
		}


		//Check for collision on right:
		int grid_x_4 = (int)((x() + (width() / 2)) / 0.18f);
		int grid_y_4 = (int)(-(y()) / 0.18f);
		int data4 = map.layers[3][grid_y_4][grid_x_4];

		if (data4 > 0){
			//std::cout << "Tile at heroes head: " << data2 << "   grid_x, grid_y" << grid_x_2 << "," << grid_y_2 << std::endl;
			acceleration.x = 0;
			//velocity[0] = 0;
			pos[0] -= fabs((x() + width() / 2) - ((tile_world_size * grid_x_4)));
			collidedRight = true;
		}
		else{
			collidedRight = false;
		}

		//Check for collision on upper right:
		grid_y_4 = (int)(-(y()) / 0.18f) + 1;
		data4 = map.layers[3][grid_y_4][grid_x_4];

		if (data4 > 0){
			//std::cout << "Tile at heroes head: " << data2 << "   grid_x, grid_y" << grid_x_2 << "," << grid_y_2 << std::endl;
			acceleration.x = 0;
			//velocity[0] = 0;
			pos[0] -= fabs((x() + width() / 2) - ((tile_world_size * grid_x_4)));
			collidedRight = true;
		}
		else{
			collidedRight = false;
		}

		//Check for collision on lower right:
		grid_y_4 = (int)(-(y()) / 0.18f) - 1;
		data4 = map.layers[3][grid_y_4][grid_x_4];

		if (data4 > 0){
			//std::cout << "Tile at heroes head: " << data2 << "   grid_x, grid_y" << grid_x_2 << "," << grid_y_2 << std::endl;
			acceleration.x = 0;
			//velocity[0] = 0;
			pos[0] -= fabs((x() + width() / 2) - ((tile_world_size * grid_x_4)));
			collidedRight = true;
		}
		else{
			collidedRight = false;
		}
	}
	

	bool jumping = false;
	bool check_collisions = true;

	bool constant_x_velocity = false;
	virtual void update(){	
		//pos[0] += std::cosf(movement_angle) * elapsed * 1.0f;
		//pos[1] += std::sinf(movement_angle) * elapsed * 1.0f;
		

		if (apply_velocity){
			float gravity = -11.0f;

			
			if (constant_x_velocity){
				velocity[0] = velocity[0];
			}
			else{
				velocity[0] += acceleration.x * elapsed;
			}

			if (apply_gravity){
				velocity[1] += gravity * elapsed;
			}
			
		
			pos[1] += elapsed * velocity[1];
			if (check_collisions){
				check_for_collisions();
			}
			
			pos[0] += elapsed * velocity[0]; //velocity contains direction
			if (check_collisions){
				check_for_collisions();
			}
			
		}


		if (animations.count(current_animation_name)){
			animations[current_animation_name].update();
			animations[current_animation_name].update_size(size[0], size[1]);
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



	void draw(){
		if (destroyed){
			return;
		}
		
		if (draw_mode == "texture"){
			
			tex_program->SetModelMatrix(modelMatrix);
			tex_program->SetProjectionMatrix(projectionMatrix);
			tex_program->SetViewMatrix(viewMatrix);
					
			
			if (animations.count(current_animation_name)){
				modelMatrix.Identity();
				modelMatrix.Translate(x(), y(), z());
				modelMatrix.SetScale(direction[0], 1, 1);
				tex_program->SetModelMatrix(modelMatrix);
				tex_program->SetViewMatrix(viewMatrix);
				tex_program->SetColor(0,1,0,1);


				glUseProgram(tex_program->programID);

				animations[current_animation_name].draw();
			}
		}
		else if (draw_mode == "shape"){
			shape_program->SetModelMatrix(modelMatrix);
			shape_program->SetProjectionMatrix(projectionMatrix);
			shape_program->SetViewMatrix(viewMatrix);
			glUseProgram(shape_program->programID);


			modelMatrix.Identity();
			modelMatrix.Translate(x(), y(), z());
			shape_program->SetModelMatrix(modelMatrix);	
			shape_program->SetColor(color[0], color[1], color[2], color[3]);

			glVertexAttribPointer(shape_program->positionAttribute, 2, GL_FLOAT, false, 0, verts.data());
			glEnableVertexAttribArray(shape_program->positionAttribute);
			glDrawArrays(GL_QUADS, 0, 4);

			glDisableVertexAttribArray(shape_program->positionAttribute);
		}


	}



	float width(){
		return size[0];
	}


	float height(){
		return size[1];
	}



	GameObject shoot(Animation animation){
		GameObject newBullet;
		newBullet.set_pos(x(), y());
		newBullet.set_velocity(direction[0] * 3.0f, 0);
		newBullet.set_direction(direction[0], direction[1]);
		newBullet.set_draw_mode("texture");
		newBullet.set_size(0.1, 0.1);
		newBullet.set_verts(quad_verts(0.1, 0.1));
		newBullet.strings["shooter_name"] = name;
		newBullet.apply_gravity = false;
		newBullet.check_collisions = false;

		newBullet.add_animation("idle", animation);
		newBullet.set_animation("idle");

		return newBullet;
	}




};


class Barrier : public GameObject {
public:
	Barrier():GameObject() {

	}


	virtual void update() override {

	}


	void advance_active_animation(){
		if (animations[current_animation_name].current_index >= 4){
			destroyed = true;
		}
		else{
			animations[current_animation_name].advance();
		}		
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
	if (obj1.destroyed){
		return false;
	}

	if (obj2.destroyed){
		return false;
	}

	return check_box_collision(obj1.top_left_x(), obj1.top_left_y(), obj1.width(), obj1.height(), obj2.top_left_x(), obj2.top_left_y(), obj2.width(), obj2.height());
}






class GameState {
public:

	GameState(){

	}

	GameObject player;

	std::vector<GameObject> enemies;
	std::vector<GameObject> bullets;

	int score;


	void render(){

	}

	void update(){

	}

	void process_input(){

	}
};


class MainMenu : GameState {
public:
	void render(){

		draw_text("Platformer", -1.5f, 1.0f, font_texture, 0.4, 0.165f);
		draw_text("Press Spacebar to play", -1.5f, 0.0f, font_texture, 0.4, 0.165f);
	}

	void update(){

	}

	void process_input(){

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}


			if (event.type == SDL_KEYDOWN){
				if (event.key.keysym.sym == SDLK_SPACE){
					mode = STATE_GAME_LEVEL;
				}
			}
		}
	}

};


bool shouldRemoveBullet(GameObject bullet) {
	if (bullet.timeAlive() > 2) {
		return true;
	}


	if (bullet.destroyed){
		return true;
	}

	return false;
	
}


bool shouldRemoveEnemy(GameObject enemy) {
	if (enemy.destroyed){
		return true;
	}

	if (enemy.life == 0){
		return true;
	}

	return false;
}

bool shouldRemoveBarrier(Barrier* barrier){
	if (barrier->destroyed){
		return true;
	}

	return false;
}


class GameLevel : GameState {
public:
	GLuint sprite_sheet_texture;
	GLuint tile_texture;


	float sheet_width = 0;
	float sheet_height = 0;
	float tile_sheet_width = 0;
	float tile_sheet_height = 0;

	std::vector<GameObject*> objects;
	std::vector<GameObject> enemies;
	std::vector<Barrier*> barriers;
	int score = 0;
	int enemies_per_row = 11;
	

	

	std::vector<float> verts;
	std::vector<float> tex_coords;


	GameObject box;
	GameObject box2;
	GameLevel(){
		sprite_sheet_texture = LoadTexture("resources/sheet.png", &sheet_width, &sheet_height);
		tile_texture = LoadTexture("resources/sheet2.png", &tile_sheet_width, &tile_sheet_height);

		float player_width = 0.55;
		float player_height = 0.85f;

		player.set_name("zero");
		player.set_pos(1,-2.8f, 0);
		player.set_draw_mode("texture");
		player.set_velocity(0, 0);
		player.apply_velocity = true;
		player.set_size(player_width, player_height);
		player.set_verts(quad_verts(player_width, player_height));
		player.set_direction(1, 0.0f);
		player.constant_x_velocity = true;
		player.add_animation("run", 14);
		player.add_animation("idle", 1);
		player.set_animation("idle");



		float padel_width = 0.08f;
		float padel_height = 0.08f;

		box.set_name("box");
		box.set_pos(player.x(), player.y(), player.z());
		box.set_draw_mode("shape");
		box.set_color(1.0f, 0.3f, 0.3f, 1);
		box.set_size(padel_width, padel_height);
		box.set_verts(quad_verts(padel_width, padel_height));



		box2.set_name("box2");
		box2.set_pos(player.x(), player.y(), player.z());
		box2.set_draw_mode("shape");
		box2.set_color(0.89f, 0.3f, 0.67f, 1);
		box2.set_size(player.width(), player.height());
		box2.set_verts(quad_verts(player.width(), player.height()));



		//Create an enemy
		GameObject enemy("greymon");
		enemy.set_pos(7.0f, -2.8f, 0);
		enemy.set_draw_mode("texture");
		enemy.set_velocity(0, 0);
		enemy.apply_velocity = true;
		enemy.set_size(player_width, player_height);
		enemy.set_verts(quad_verts(player_width, player_height));
		enemy.set_direction(-1, 0.0f);
		enemy.add_animation("idle", 1);
		enemy.set_animation("idle");
		enemy.constant_x_velocity = true;
		enemy.acceleration.x = 2.0f;

		enemies.push_back(enemy);


		//Animation player_animation;
		//Sprite player_sprite(sprite_sheet_texture, 0.0f / sheet_width, 0.0f / sheet_height, 16.0f / sheet_width, 16.0f / sheet_height, 0.35);
		//
		//player_animation.add_sprite(player_sprite);

		//player.add_animation("idle", player_animation);
		//player.set_animation("idle");


		//load tile map
		map.Load("resources/hw4_map.txt");
		for (int i = 0; i < map.entities.size(); i++) {
			PlaceEntity(map.entities[i].type, map.entities[i].x * TILE_SIZE, map.entities[i].y * -TILE_SIZE);
		}



		for (int z = 0; z < map.layers.size(); z++){
			//for (int z = 0; z < 1; z++){
			for (int y = 0; y < map.mapHeight; y++) {
				for (int x = 0; x < map.mapWidth; x++) {
					// do something with map.mapData[y][x] 
					//std::cout << map.mapData[y][x] << std::endl;
					std::vector<float> this_verts;
					std::vector<float> this_tex_coords;

					load_tile(map.layers[z][y][x], x, y, this_verts, this_tex_coords);

					verts.insert(verts.end(), this_verts.begin(), this_verts.end());
					tex_coords.insert(tex_coords.end(), this_tex_coords.begin(), this_tex_coords.end());
				}
			}
		}


	}





	~GameLevel(){
		for (int x = 0; x < objects.size(); x++){
			delete objects[x];
		}

		for (int x = 0; x < barriers.size(); x++){
			delete barriers[x];
		}
		
	}

	float enemy_movement_direction = -1;
	float last_movement_direction_change = 0;
	float movement_change_interval = 1;
	int movement_change_count = 0;

	float last_movement = 0;
	float last_attack = 0;
	float attack_interval = 1.0f;

	int row_index = 0;
	int row_change_count = 0;


	void PlaceEntity(std::string type, float x, float y) {
		// place your entity at x, y based on type string
	}



	void worldToTileCoord(float worldX, float worldY, int *gridX, int *gridY){
		*gridX = (int)(worldX / TILE_SIZE);
		*gridY = (int)(-worldY / TILE_SIZE);
	}



	void enemy_shoot(GameObject& enemy){
		Animation bullet_animation;
		Sprite bullet_sprite("resources/blast.png");

		bullet_animation.add_sprite(bullet_sprite);


		bullets.push_back(enemy.shoot(bullet_animation));
	}

	void update(){
		player.update();
		

		bullets.erase(std::remove_if(bullets.begin(), bullets.end(), shouldRemoveBullet), bullets.end());

		for (int i = 0; i < bullets.size(); i++) {
			bullets[i].update();
		}


		for (int i = 0; i < enemies.size(); i++) {
			enemies[i].update();

			if (get_runtime() - last_attack > 3.0f){
				enemies[i].flip_x_direction();
				last_attack = get_runtime();
				enemy_shoot(enemies[i]);
			}
		}


		for (int i = 0; i < objects.size(); i++) {
			objects[i]->update();
		}


		handle_collisions();
	}



	void render(){



		//Move the camera to follow player
		viewMatrix.Identity();

		float clamped_screen_x = screen_x_clamp(player.x(), 0.0f, (map.mapWidth * tile_world_size) - tile_world_size);
		float clamped_screen_y = screen_y_clamp(player.y() - 1.0f, 0.0f, (map.mapHeight * tile_world_size));
		viewMatrix.Translate(-clamped_screen_x, -clamped_screen_y, 0);

		//Draw tilemap
		batch_draw(tile_texture, verts, tex_coords);

		box.set_pos(player.x(), player.y());
		box2.set_pos(player.x(), player.y());

		for (int i = 0; i < enemies.size(); i++) {
			enemies[i].draw();
		}

		for (int i = 0; i < bullets.size(); i++) {
			bullets[i].draw();
		}

	
		//draw player
		player.draw();


		//box2.draw();
		//box.draw();
		
		draw_text("points: " + std::to_string(score), 0.5f, -0.5f, font_texture, 0.4, 0.165f);
		draw_text("lives: " + std::to_string(player.lives), 0.5f, -0.8f, font_texture, 0.4, 0.165f);
	}

	float screen_y_clamp(float val, float top, float bottom){
		if (val - (screen_height / 2) < top){
			return top - (screen_height / 2);
		}
		else if (val + (screen_height / 2) > bottom){
			return bottom + (screen_height / 2);
		}

		return val;
	}


	float screen_x_clamp(float val, float left, float right){
		if (val - (screen_width / 2) < left){
			return left + (screen_width / 2);
		}
		else if (val + (screen_width / 2) > right){
			return right - (screen_width / 2);
		} 

		return val;
	}



	void load_tile(int tile_id, int pos_x, int pos_y, std::vector<float>& verts, std::vector<float>& tex_coords){
		if (tile_id == 0){
			return;
		}

		//Convert tile_id to x,y
		//Starts from top left at 0 -> sheet_width
		//then goes to next row starting from left
		int tile_sheet_x_width = tile_sheet_width / TILE_SIZE; //x grid size
		int tile_sheet_y_height = tile_sheet_height / TILE_SIZE; //x grid size
		int tile_x = (tile_id % tile_sheet_x_width) - 1;
		int tile_y = tile_id / tile_sheet_x_width;


		//float tile_world_x = (pos_x * tile_size) - (screen_width / 2.0f);
		//float tile_world_y = -1.0f*(pos_y * tile_size) + (screen_height / 2.0f);
		float tile_world_x = (pos_x * tile_world_size);
		float tile_world_y = -1.0f*(pos_y * tile_world_size);


		Sprite tile_sprite(tile_texture, (tile_x * TILE_SIZE) / tile_sheet_width, (tile_y * TILE_SIZE) / tile_sheet_height, TILE_SIZE / tile_sheet_width, TILE_SIZE / tile_sheet_height, tile_world_size);
		
		tile_sprite.set_pos(tile_world_x, tile_world_y, 0);
		tile_sprite.set_vert_pos(tile_world_x, tile_world_y, 0);
		verts = tile_sprite.get_verts();
		tex_coords = tile_sprite.get_tex_coords();
	}



	void game_won(){
		std::cout << "YOU WON" << std::endl;
		mode = STATE_GAME_WON;
	}


	void game_over(){
		std::cout << "GAME OVER" << std::endl;
		mode = STATE_GAME_OVER;
	}


	void player_got_hit(){
		player.lives -= 1;
		if (player.lives < 0){
			game_over();
		}

		//player.set_pos(0, -1.68f);
	}


	void barrier_take_hit(Barrier* this_barrier){
		this_barrier->advance_active_animation();
	}


	void handle_collisions(){

		for (int x = 0; x < bullets.size(); x++){
			bool continue_to_next_loop = false;

			if (bullets[x].strings["shooter_name"] == "hero"){
				for (int y = 0; y < enemies.size(); y++){
					if (check_box_collision(bullets[x], enemies[y])){
						bullets[x].destroy();
						enemies[y].destroy();
						score += 10;
						continue_to_next_loop = true;
						break;
					}
				}
			}
			else{
				if (check_box_collision(bullets[x], player)){
					player_got_hit();
					bullets[x].destroy();
					continue_to_next_loop = true;
					break;
				}
			}


			if (continue_to_next_loop){
				continue;
			}
			
		}
	}


	std::string last_key = "";

	void process_input(){
		Uint8* keysArray = const_cast <Uint8*> (SDL_GetKeyboardState(NULL));

		if (keysArray[SDL_SCANCODE_RETURN]){
			printf("MESSAGE: <RETURN> is pressed...\n");
		}

		if (keysArray[SDL_SCANCODE_W]){
			player.move_up();
		}

		if (keysArray[SDL_SCANCODE_S]){
			player.move_down();
		}


		bool moving = false;
		if (keysArray[SDL_SCANCODE_D]){
			last_key = "D";
			player.set_animation("run");
			player.move_right();
			moving = true;
		}
		
		if (keysArray[SDL_SCANCODE_A]){
			last_key = "A";
			player.set_animation("run");
			player.move_left();
			moving = true;
		}


		if(!moving){
			player.set_animation("idle");
			player.stop_moving();
		}



		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}


			if (event.type == SDL_KEYDOWN){
				if (event.key.keysym.sym == SDLK_SPACE){
					player.jump();
				}
			}
		}


	}



};


MainMenu* mainMenu;
GameLevel* gameLevel;


void render_game() {
	switch (mode) {
		case STATE_MAIN_MENU:
			mainMenu->render();
			break;
		case STATE_GAME_LEVEL:
			gameLevel->render();
			break;
		case STATE_GAME_OVER:
			glClear(GL_COLOR_BUFFER_BIT);
			draw_text("GAME OVER", -1.0f, 0, font_texture, 0.5, 0.3);
			break;
		case STATE_GAME_WON:
			glClear(GL_COLOR_BUFFER_BIT);
			draw_text("YOU WON", -1.0f, 0, font_texture, 0.5, 0.3);
			break;
	}
}

void update_game() {
	switch (mode) {
		case STATE_MAIN_MENU:
			mainMenu->update();
			break;
		case STATE_GAME_LEVEL:
			gameLevel->update();
			break;
	}
}

void process_input() {
	switch (mode) {
		case STATE_MAIN_MENU:
			mainMenu->process_input();
			break;
		case STATE_GAME_LEVEL:
			gameLevel->process_input();
			break;
		default:
			SDL_Event event;
			while (SDL_PollEvent(&event)) {
				if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
					done = true;
				}
			}
			break;
	}
}





int left_score = 0;
int right_score = 0;

int main(int argc, char *argv[]) {
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("Platformer HW4 - AFL294@NYU.EDU", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1000, 600, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
	#ifdef _WINDOWS
		glewInit();
	#endif



	font_texture = LoadTexture("resources/font.png", &font_sheet_width, &font_sheet_height);


	glViewport(0, 0, 1000, 600);

	tex_program = new ShaderProgram();

	tex_program->Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");


	shape_program = new ShaderProgram();
	
	shape_program->Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");


	mainMenu = new MainMenu();
	gameLevel = new GameLevel();

	float lastFrameTicks = 0.0f;





	projectionMatrix.SetOrthoProjection(screen_left, screen_right, screen_bottom, screen_top, -1.0f, 1.0f);


	//mode = STATE_MAIN_MENU;
	mode = STATE_GAME_LEVEL;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


	float FIXED_TIMESTEP = 0.0166666f;
	float MAX_TIMESTEPS = 6;

	float accumulator = 0.0f;
	float elapsed2 = 0.0f;

	//FIXED TIMESTEP:
	while (!done) {
		float ticks = get_runtime();
		elapsed2 = ticks - lastFrameTicks;
		lastFrameTicks = ticks;

		elapsed2 += accumulator;
		if (elapsed2 < FIXED_TIMESTEP){
			accumulator = elapsed2;
			continue;
		}

		glClear(GL_COLOR_BUFFER_BIT);

		process_input();
		while (elapsed2 >= FIXED_TIMESTEP){
			elapsed = FIXED_TIMESTEP;
			update_game();
			elapsed2 -= FIXED_TIMESTEP;
		}
		accumulator = elapsed2;
		render_game();

		SDL_GL_SwapWindow(displayWindow);
	}

	//VARIABLE TIMESTEP:
	//while (!done) {
	//	float ticks = get_runtime();
	//	elapsed = ticks - lastFrameTicks;
	//	lastFrameTicks = ticks;

	//	glClear(GL_COLOR_BUFFER_BIT);

	//	process_input();
	//	update_game();
	//	render_game();

	//	SDL_GL_SwapWindow(displayWindow);
	//}

	//Cleanup
	delete mainMenu;
	delete gameLevel;
	delete tex_program;
	delete shape_program;

	SDL_Quit();
	return 0;
}
