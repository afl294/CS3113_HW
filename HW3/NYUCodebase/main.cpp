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


enum GameMode { STATE_MAIN_MENU, STATE_GAME_LEVEL, STATE_GAME_OVER, STATE_GAME_WON };
GameMode mode;

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
	}


	void set_size(int x_size_){
		x_size = x_size_;
		y_size = x_size;
		x_size = x_size_ * aspect_ratio;
	}



	void draw(){
		glUseProgram(tex_program->programID);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		//Draws sprites pixel perfect with no blur
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);


		std::vector<float> verts = {
			-x_size, -y_size,
			x_size, -y_size,
			x_size, y_size,
			-x_size, -y_size,
			x_size, y_size,
			-x_size, y_size
		};

		std::vector<float> texCoords = {
			0.0f, 1.0f,
			1.0f, 1.0f,
			1.0f, 0.0f,
			0.0f, 1.0f,
			1.0f, 0.0f,
			0.0f, 0.0f
		};

		if (sheet){
			texCoords = {
				u, v + height,
				u + width, v,
				u, v,
				u + width, v,
				u, v + height,
				u + width, v + height
			};
			float aspect = width / height;
			verts = {
				-0.5f * size * aspect, -0.5f * size,
				0.5f * size * aspect, 0.5f * size,
				-0.5f * size * aspect, 0.5f * size,
				0.5f * size * aspect, 0.5f * size,
				-0.5f * size * aspect, -0.5f * size,
				0.5f * size * aspect, -0.5f * size };
		}




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
			std::cout << "Loading file: " << file_path << std::endl;
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

	float direction[3];
	float total_move_amount = 0;
	float movement_angle = degrees_to_radians (-180);
	float life = 10;
	int lives = 2;
	bool destroyed = false;


	float created_at = 0;

	void init(){
		created_at = get_runtime();
		set_pos(0, 0);
		set_animation("idle");
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
		move_y(velocity[1] * elapsed);
	}

	void move_down(){
		move_y(-velocity[1] * elapsed);
	}

	void move_left(){
		move_x(-velocity[0] * elapsed);
	}

	void move_right(){
		move_x(velocity[0] * elapsed);
	}



	void set_x(float x_){
		pos[0] = x_;
	}


	void set_y(float y_){
		pos[1] = y_;
	}

	virtual void update(){	
		//pos[0] += std::cosf(movement_angle) * elapsed * 1.0f;
		//pos[1] += std::sinf(movement_angle) * elapsed * 1.0f;

		if (apply_velocity){


			pos[0] += direction[0] * elapsed * velocity[0];
			pos[1] += direction[1] * elapsed * velocity[1];
		}


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
		newBullet.set_velocity(0, 3.0f);
		newBullet.set_direction(direction[0], direction[1]);
		newBullet.set_draw_mode("texture");
		newBullet.set_size(0.1, 0.1);
		newBullet.set_verts(quad_verts(0.1, 0.1));
		newBullet.strings["shooter_name"] = name;

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

		draw_text("Space Invaders", -1.5f, 1.0f, font_texture, 0.4, 0.165f);
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
	
	float sheet_width = 0;
	float sheet_height = 0;

	std::vector<GameObject*> objects;
	std::vector<Barrier*> barriers;
	int score = 0;
	int enemies_per_row = 11;

	GameLevel(){
		sprite_sheet_texture = LoadTexture("resources/sheet.png", &sheet_width, &sheet_height);
		

		float player_width = 0.35;
		float player_height = 0.35;

		player.set_name("hero");
		player.set_pos(0, -1.68f);
		player.set_draw_mode("texture");
		player.set_velocity(3, 3);
		player.apply_velocity = false;
		player.set_size(player_width, player_height);
		player.set_verts(quad_verts(player_width, player_height));
		player.set_direction(0, 1.0f);

		Animation player_animation;
		Sprite player_sprite(sprite_sheet_texture, 0.0f / sheet_width, 0.0f / sheet_height, 16.0f / sheet_width, 16.0f / sheet_height, 0.35);
		
		player_animation.add_sprite(player_sprite);

		player.add_animation("idle", player_animation);
		player.set_animation("idle");


		float enemy_spawn_start_x = -2.44f;
		float enemy_spawn_start_y = 1.5f;
		float enemy_spawn_spacing = (3.5 * 2) / 13;
		float enemy_spawn_y_spacing = 0.46f;
		float sheet_x_offsets[] = {16.0f, 32.0f, 32.0f, 48.0f, 48.0f};
		int current_row_index = 0;
		for (int x = 0; x < enemies_per_row * 5; x++){
			int x_relative = x % enemies_per_row;
			if (x != 0 && x % enemies_per_row == 0){
				current_row_index += 1;
			}


			float this_spawn_x = enemy_spawn_start_x + (x_relative * enemy_spawn_spacing);
			float this_spawn_y = enemy_spawn_start_y - (current_row_index * enemy_spawn_y_spacing);
			GameObject new_enemy;
			new_enemy.set_pos(this_spawn_x, this_spawn_y, 0, true);
			new_enemy.set_draw_mode("texture");
			new_enemy.set_velocity(0, 0);
			new_enemy.apply_velocity = false;
			new_enemy.set_size(0.44, 0.44);
			new_enemy.set_verts(quad_verts(player_width, player_height));
			new_enemy.set_direction(0, -1.0f);

			Animation enemy_animation;
			Sprite enemy_sprite(sprite_sheet_texture, sheet_x_offsets[current_row_index] / sheet_width, 0.0f / sheet_height, 16.0f / sheet_width, 16.0f / sheet_height, 0.44);

			enemy_animation.add_sprite(enemy_sprite);

			new_enemy.add_animation("idle", enemy_animation);
			new_enemy.set_animation("idle");
			enemies.push_back(new_enemy);
			
		}



		GameObject* background = new GameObject("background");
		background->set_pos(0, 0);
		background->set_draw_mode("texture");
		background->set_color(0.5, 0.3, 0, 1);
		background->set_size(3.55 * 1.2f, 2.0 * 1.2f);
		background->set_verts(quad_verts(background->width(), background->height()));

		Animation background_animation;
		Sprite background_sprite("resources/space.jpg");
		background_sprite.set_size(background->width(), background->height());
		background_animation.add_sprite(background_sprite);

		background->add_animation("idle", background_animation);
		background->set_animation("idle");
		objects.push_back(background);




		//Barriers
		float barrier_x_spacing = 2.23f;
		for (int z = 0; z < 3; z++){
			Barrier* barrier_1 = new Barrier();
			barrier_1->set_pos(-2.3f + (barrier_x_spacing * z), -1.3f);
			barrier_1->set_draw_mode("texture");
			barrier_1->set_size(1, 0.5f);
			barrier_1->set_verts(quad_verts(background->width(), background->height()));

			Animation barrier_animation;
			float barrier_sheet_y = 48;
			for (int x = 0; x < 5; x++){
				Sprite barrier_sprite_1(sprite_sheet_texture, (32 * x) / sheet_width, barrier_sheet_y / sheet_height, 32.0f / sheet_width, 16.0f / sheet_height, 0.44);
				barrier_animation.add_sprite(barrier_sprite_1);
			}


			barrier_1->add_animation("idle", barrier_animation);
			barrier_1->set_animation("idle");


			barriers.push_back(barrier_1);
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

	void update(){
		player.update();

		if (get_runtime() - last_movement > 0.2f){
			int x_start = enemies.size() - 1 - (row_index * enemies_per_row);
			int x_end = enemies.size() - 1 - (row_index * enemies_per_row) - 10;

			for (int x = x_start; x >= x_end; x--){
				enemies[x].move_x(0.1f * enemy_movement_direction);

				enemies[x].set_y(enemies[x].start_pos[1] - ((row_change_count / 5) * 0.05f));
			}

			
			last_movement = get_runtime();
			row_index += 1;
			if (row_index >= enemies.size() / enemies_per_row){
				row_change_count += 1;
				row_index = 0;

				if (row_change_count % 5 == 0){
					enemy_movement_direction *= -1.0f;
				}			
			}
		}


		if (get_runtime() - last_attack > attack_interval){
			std::vector<int> enemies_capable_of_attacking;
			for (int x = 0; x < enemies.size(); x++){
				if (enemies[x].destroyed){
					continue;
				}

				//If enemy is on bottom row and isnt destroyed, it is always capable of attacking
				if (x >= enemies.size() - enemies_per_row){
					enemies_capable_of_attacking.push_back(x);
					continue;
				}


				bool all_enemies_below_are_destroyed = true;
				for (int y = x + 11; y < enemies.size(); y += 11){
					if (enemies[y].destroyed == false){
						all_enemies_below_are_destroyed = false;
						break;
					}
				}


				if (all_enemies_below_are_destroyed){
					enemies_capable_of_attacking.push_back(x);
				}
			}

			if (enemies_capable_of_attacking.size() > 0){
				srand(time(NULL));

				// Create ran int between 0 and size()
				int random_enemy_index = rand() % enemies_capable_of_attacking.size();
				enemy_shoot(enemies[enemies_capable_of_attacking[random_enemy_index]]);
				last_attack = get_runtime();
			}
			else{
				game_won();
			}
		}
		




		bullets.erase(std::remove_if(bullets.begin(), bullets.end(), shouldRemoveBullet), bullets.end());

		for (int i = 0; i < bullets.size(); i++) {
			bullets[i].update();
		}


		for (int i = 0; i < objects.size(); i++) {
			objects[i]->update();
		}


		barriers.erase(std::remove_if(barriers.begin(), barriers.end(), shouldRemoveBarrier), barriers.end());

		for (int i = 0; i < barriers.size(); i++) {
			barriers[i]->update();
		}

		handle_collisions();
	}

	void enemy_shoot(GameObject& enemy){
		Animation bullet_animation;
		Sprite bullet_sprite(sprite_sheet_texture, 128.0f / sheet_width, 0.0f / sheet_height, 16.0f / sheet_width, 16.0f / sheet_height, 0.35);

		bullet_animation.add_sprite(bullet_sprite);


		bullets.push_back(enemy.shoot(bullet_animation));
	}

	void render(){
		for (int i = 0; i < objects.size(); i++) {
			objects[i]->draw();
		}

		for (int x = 0; x < enemies.size(); x++){
			enemies[x].draw();
		}


		for (int x = 0; x < bullets.size(); x++){
			bullets[x].draw();
		}


		for (int i = 0; i < barriers.size(); i++) {
			barriers[i]->draw();
		}


		
		player.draw();

		draw_text("points: " + std::to_string(score), -3.4f, 1.859f, font_texture, 0.4, 0.165f);
		draw_text("lives: " + std::to_string(player.lives), -3.45f, -1.849f, font_texture, 0.4, 0.165f);

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

		player.set_pos(0, -1.68f);
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

			for (int z = 0; z < barriers.size(); z++){
				if (check_box_collision(bullets[x], *barriers[z])){
					barrier_take_hit(barriers[z]);
					bullets[x].destroy();
					continue_to_next_loop = true;
					break;
				}
			}
			
			
		}
	}




	void process_input(){
		Uint8* keysArray = const_cast <Uint8*> (SDL_GetKeyboardState(NULL));

		if (keysArray[SDL_SCANCODE_RETURN]){
			printf("MESSAGE: <RETURN> is pressed...\n");
		}

		if (keysArray[SDL_SCANCODE_W]){
			//player.move_up();
		}

		if (keysArray[SDL_SCANCODE_S]){
			//player.move_down();
		}

		if (keysArray[SDL_SCANCODE_D]){
			player.move_right();
		}

		if (keysArray[SDL_SCANCODE_A]){
			player.move_left();
		}



		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}


			if (event.type == SDL_KEYDOWN){
				if (event.key.keysym.sym == SDLK_SPACE){
					Animation bullet_animation;
					Sprite bullet_sprite(sprite_sheet_texture, 112.0f / sheet_width, 0.0f / sheet_height, 16.0f / sheet_width, 16.0f / sheet_height, 0.35);

					bullet_animation.add_sprite(bullet_sprite);


					bullets.push_back(player.shoot(bullet_animation));
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
	displayWindow = SDL_CreateWindow("Space Invaders <> AFL294@NYU.EDU", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1000, 600, SDL_WINDOW_OPENGL);
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




	float screen_left = -3.55;
	float screen_right = 3.55;
	float screen_top = 2.0f;
	float screen_bottom = -2.0f;
	projectionMatrix.SetOrthoProjection(screen_left, screen_right, screen_bottom, screen_top, -1.0f, 1.0f);


	mode = STATE_MAIN_MENU;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	while (!done) {
		float ticks = get_runtime();
		elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;

		glClear(GL_COLOR_BUFFER_BIT);


		process_input();
		update_game();
		render_game();

		SDL_GL_SwapWindow(displayWindow);
	}

	//Cleanup
	delete mainMenu;
	delete gameLevel;
	delete tex_program;
	delete shape_program;

	SDL_Quit();
	return 0;
}
