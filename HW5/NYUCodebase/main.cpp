#ifdef _WINDOWS
	#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "ShaderProgram.h"
#include "SatCollision.h"
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

	float length(){
		return sqrt(pow(x, 2) + pow(y, 2)) + pow(z, 2);
	}

	void normalize(){
		float len = length();
		x /= len;
		y /= len;
		z /= len;
	}

	Vector3 normalized(){
		float len = length();
		Vector3 v3(x/len, y/len, z/len);
		return v3;
	}

	Vector3 operator * (const Vector3 &v){

	}

	Vector3 operator * (const Matrix &m){
		float x_val = (m.m[0][0] * x) + (m.m[1][0] * y) + (m.m[2][0] * z);
		float y_val = (m.m[0][1] * x) + (m.m[1][1] * y) + (m.m[2][1] * z);
		float z_val = (m.m[0][2] * x) + (m.m[1][2] * y) + (m.m[2][2] * z);


		//float x_val = (m.m[0][0] * x) + (m.m[0][1] * y) + (m.m[0][2] * z);
		//float y_val = (m.m[1][0] * x) + (m.m[1][1] * y) + (m.m[1][2] * z);
		//float z_val = (m.m[2][0] * x) + (m.m[2][1] * y) + (m.m[2][2] * z);

		Vector3 new_v3(x_val, y_val, z_val);
		return new_v3;
	}

	void clear(){
		init(0, 0, 0);
	}

};



std::string random_string(size_t length) {
	auto randchar = []() -> char
	{
		const char charset[] =
			"0123456789"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz";
		const size_t max_index = (sizeof(charset) - 1);
		return charset[rand() % max_index];
	};
	std::string str(length, 0);
	std::generate_n(str.begin(), length, randchar);
	return str;
}

class GameObject{
public:
	std::string name = "";
	float last_change = 0;
	float interval = .085; //milliseconds (ms)

	std::unordered_map<std::string, Animation> animations;
	std::unordered_map<std::string, std::string> strings;
	std::string current_animation_name = "";
	//float pos[3];
	Vector3 pos;
	Vector3 start_pos;
	float color[4];
	std::vector<float> verts;
	std::string draw_mode = "texture";
	bool apply_velocity = true;
	Vector3 size;
	//float velocity[3];
	Vector3 velocity;
	Vector3 acceleration;
	Matrix matrix;

	std::string id;

	bool colliding = false;

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
		id = random_string(8);

		
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
		pos.x = x;
		pos.y = y;
	}

	void destroy(){
		destroyed = true;
	}

	void set_pos(float x, float y, float z, bool initial=false){
		set_pos(x, y);
		pos.z = z;

		if (initial){
			start_pos.x = x;
			start_pos.y = y;
			start_pos.z = z;
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
		pos.y += delta_y;
	}
	void move_x(float delta_x){
		pos.x += delta_x;
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
		velocity.x = -3.0f;
	}


	void move_right(){
		direction[0] = 1;
		//move_x(velocity[0] * elapsed);
		velocity.x = 3.0f;
	}

	void stop_moving(){
		acceleration.x = 0;
		velocity.x = 0;
	}


	std::vector<float> get_verts(){

		std::vector<float> verts = {
			0 - size.x, 0 - size.y,
			0 + size.x, 0 - size.y,
			0 + size.x, 0 + size.y,
			0 - size.x, 0 - size.y,
			0 + size.x, 0 + size.y,
			0 - size.x, 0 + size.y
		};


		return verts;
	}

	std::vector<Vector3> get_points(){
		std::vector<Vector3> points;
		//std::vector<float> v_floats = {
		//	x() - size.x / 2, y() + size.y / 2, //top left
		//	x() - size.x / 2, y() - size.y / 2, // bottom left
		//	x() + size.x / 2, y() - size.y / 2, //bottom right
		//	x() + size.x / 2, y() + size.y / 2 //top right
		//};

		std::vector<float> v_floats = {
			x() - size.x / 2, y() + size.y / 2, //top left
			x() - size.x / 2, y() - size.y / 2, // bottom left
			x() + size.x / 2, y() - size.y / 2, //bottom right
			x() + size.x / 2, y() + size.y / 2 //top right
		};

		for (int x = 0; x < v_floats.size(); x += 2){
			Vector3 new_v(v_floats[x], v_floats[x + 1], 0);
			points.push_back(new_v);
		}

		return points;
	}



	void flip_x_direction(){
		direction[0] *= -1.0f;
		velocity.x = fabs(velocity.x) * direction[0];
	}

	void set_x(float x_){
		pos.x = x_;
	}


	void set_y(float y_){
		pos.y = y_;
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


	double distance(float x1, float y1, float x2, float y2) {
		const double x_diff = x1 - x2;
		const double y_diff = y1 - y2;
		return std::sqrt(x_diff * x_diff + y_diff * y_diff);
	}

	void check_for_collisions(){
		
	}
	

	bool jumping = false;
	bool check_collisions = true;

	bool constant_x_velocity = false;
	virtual void update(){	
		//pos.x += std::cosf(movement_angle) * elapsed * 1.0f;
		//pos.y += std::sinf(movement_angle) * elapsed * 1.0f;
		

		if (apply_velocity){
			float gravity = -11.0f;

			
			if (constant_x_velocity){
				velocity.x = velocity.x;
			}
			else{
				velocity.x += acceleration.x * elapsed;
			}

			if (apply_gravity){
				velocity.y += gravity * elapsed;
			}
			
		
			pos.y += elapsed * velocity.y;
			if (check_collisions){
				check_for_collisions();
			}
			
			pos.x += elapsed * velocity.x; //velocity contains direction
			if (check_collisions){
				check_for_collisions();
			}
			
		}


		Matrix m1;
		m1.Identity();
		m1.SetScale(size.x, size.y, 1);

		Matrix m2;
		m2.Identity();
		m2.SetRotation(rotation);

		Matrix m3;
		m3.Identity();
		m3.SetPosition(x(), y(), z());

		//matrix = m1 * m3 * m2;
		matrix = m3 * m1  * m2;


		if (animations.count(current_animation_name)){
			animations[current_animation_name].update();
			animations[current_animation_name].update_size(size.x, size.y);
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
		return pos.x;
	}

	float y(){
		return pos.y;
	}

	float z(){
		return pos.z;
	}


	float top_left_x(){
		return pos.x - (size.x / 2);
	}

	float top_left_y(){
		return pos.y - (size.y / 2);
	}

	void set_verts(std::vector<float> arr){
		verts = arr;
	}


	void set_size(float width_, float height_){
		size.x = width_;
		size.y = height_;

		//matrix.SetScale(size.x, size.y, 0);
	}

	void set_draw_mode(std::string mode_){
		draw_mode = mode_;
	}

	void set_velocity(float x_, float y_, float z_=0){
		velocity.x = x_;
		velocity.y = y_;
		velocity.z = z_;
	}


	void set_color(float r, float g, float b, float a){
		color[0] = r;
		color[1] = g;
		color[2] = b;
		color[3] = a;
	}

	float rotation = 0;

	void draw(){
		if (destroyed){
			return;
		}
		
		if (rotation >= 6.28){
			rotation = 0;
		}

		//matrix.Identity();


		//matrix.Translate(x(), y(), z());
		


		if (draw_mode == "texture"){
			
			tex_program->SetProjectionMatrix(projectionMatrix);
						
			if (animations.count(current_animation_name)){
				tex_program->SetModelMatrix(matrix);
				tex_program->SetViewMatrix(viewMatrix);
				tex_program->SetColor(0,1,0,1);


				glUseProgram(tex_program->programID);

				animations[current_animation_name].draw();
			}
		}
		else if (draw_mode == "shape"){
			shape_program->SetModelMatrix(matrix);
			shape_program->SetProjectionMatrix(projectionMatrix);
			shape_program->SetViewMatrix(viewMatrix);
			glUseProgram(shape_program->programID);


			shape_program->SetModelMatrix(matrix);
			shape_program->SetColor(color[0], color[1], color[2], color[3]);

			glVertexAttribPointer(shape_program->positionAttribute, 2, GL_FLOAT, false, 0, verts.data());
			glEnableVertexAttribArray(shape_program->positionAttribute);
			glDrawArrays(GL_QUADS, 0, 4);

			glDisableVertexAttribArray(shape_program->positionAttribute);
		}


	}



	float width(){
		return size.x;
	}


	float height(){
		return size.y;
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

	GameObject* player;

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


	
	GameLevel(){
		sprite_sheet_texture = LoadTexture("resources/sheet.png", &sheet_width, &sheet_height);
		tile_texture = LoadTexture("resources/sheet2.png", &tile_sheet_width, &tile_sheet_height);

		float player_width = 0.6;
		float player_height = 0.6f;


		player = new GameObject();
		player->set_name("zero");
		player->set_pos(-2, -1.0f, 0);
		player->set_draw_mode("shape");
		player->set_velocity(0, 0);
		player->apply_velocity = true;
		player->set_color(0.0f, 0.5f, 0.23f, 1);
		player->set_size(player_width, player_height);
		player->set_verts(quad_verts(player_width, player_height));
		player->set_direction(1, 0.0f);
		player->constant_x_velocity = true;
		player->id = "1";
		player->apply_gravity = false;

		

		GameObject* box = new GameObject();
		box->set_name("box");
		box->set_pos(-1.0f, 0, 0);
		box->set_draw_mode("shape");
		box->set_color(0.0f, 0.3f, 0.3f, 1);
		box->set_size(0.6f, 0.6f);
		box->set_verts(quad_verts(0.6f, 0.6f));
		box->apply_gravity = false;
		box->id = "2";

		objects.push_back(box);


		GameObject* box2 = new GameObject();
		box2->set_name("box2");
		box2->set_pos(1.0f, 0, 0);
		box2->set_draw_mode("shape");
		box2->set_color(0.0f, 0.3f, 0.67f, 1);
		box2->set_size(0.55, 0.85f);
		box2->set_verts(quad_verts(0.55, 0.85f));
		box2->apply_gravity = false;
		box2->id = "3";
		objects.push_back(box2);

		GameObject* box3 = new GameObject();
		box3->set_name("box3");
		box3->set_pos(-2.0f, 0, 0);
		box3->set_draw_mode("shape");
		box3->set_color(0.0f, 0.1f, 0.46f, 1);
		box3->set_size(player->width(), player->height());
		box3->set_verts(quad_verts(player->width(), player->height()));
		box3->apply_gravity = false;
		box3->id = "4";

		objects.push_back(box3);
		objects.push_back(player);
		

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



	float box_dir = 1;
	float box_speed = 2;
	float last_box_dir_change = 0;

	void update(){
		player->update();
		
		if (get_runtime() - last_box_dir_change > 1.5f){
			last_box_dir_change = get_runtime();
			box_dir *= -1.0f;
		}

		for (int i = 0; i < objects.size(); i++) {
			if (i > 1){ objects[i]->update(); continue; }

			float box_dir2 = box_dir;
			if (i > 0){
				box_dir2 *= -1.0f;
			}

			objects[i]->rotation += 1 * elapsed;
			//objects[i]->rotation = 2;
			objects[i]->move_x(box_speed * box_dir2 * elapsed);
			objects[i]->update();
		}


		handle_collisions();
	}



	void render(){
		viewMatrix.Identity();
		viewMatrix.Translate(0, 0, 0);

		for (int i = 0; i < objects.size(); i++) {
			objects[i]->draw();
		}

		draw_text("move with wasd", -2.9f, 1.75f, font_texture, 0.4, 0.165f);
		//draw_text("lives: " + std::to_string(player.lives), 0.5f, -0.8f, font_texture, 0.4, 0.165f);
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
		player->lives -= 1;
		if (player->lives < 0){
			game_over();
		}


	}


	void handle_collisions(){
		for (int x = 0; x < objects.size(); x++){
			objects[x]->colliding = false;
		}

		for (int x = 0; x < objects.size(); x++){
			for (int y = 0; y < objects.size(); y++){
				if (x == y){
					continue; //dont collide with self
				}

				std::pair<float, float> penetration;

				std::vector<std::pair<float, float>> e1Points;
				std::vector<std::pair<float, float>> e2Points;

				for (int i = 0; i < objects[x]->get_points().size(); i++) {
					Vector3 point = objects[x]->get_points()[i] * objects[x]->matrix;
					e1Points.push_back(std::make_pair(point.x, point.y));
				}


				for (int i = 0; i < objects[y]->get_points().size(); i++) {
					Vector3 point = objects[y]->get_points()[i] * objects[y]->matrix;
					e2Points.push_back(std::make_pair(point.x, point.y));
				}

				bool sat_collided = CheckSATCollision(e1Points, e2Points, penetration);
				if (sat_collided){
					objects[x]->colliding = true;
					objects[y]->colliding = true;


					if (objects[x]->id == "1"){
						objects[x]->pos.x += (penetration.first);
						objects[x]->pos.y += (penetration.second);
					} else if (objects[y]->id == "1"){
						objects[y]->pos.x += (penetration.first);
						objects[y]->pos.y += (penetration.second);
					}
				}
			}
		}


		for (int x = 0; x < objects.size(); x++){
			if (objects[x]->colliding){
				objects[x]->color[0] = 1.0f;
			}
			else{
				objects[x]->color[0] = 0.0f;
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
			player->move_up();
		}

		if (keysArray[SDL_SCANCODE_S]){
			player->move_down();
		}


		bool moving = false;
		if (keysArray[SDL_SCANCODE_D]){
			last_key = "D";
			player->set_animation("run");
			player->move_right();
			moving = true;
		}
		
		if (keysArray[SDL_SCANCODE_A]){
			last_key = "A";
			player->set_animation("run");
			player->move_left();
			moving = true;
		}


		if(!moving){
			player->set_animation("idle");
			player->stop_moving();
		}



		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}


			if (event.type == SDL_KEYDOWN){
				if (event.key.keysym.sym == SDLK_SPACE){
					//player->jump();
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
	displayWindow = SDL_CreateWindow("SAT Collisions HW5 - AFL294@NYU.EDU", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1000, 600, SDL_WINDOW_OPENGL);
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
