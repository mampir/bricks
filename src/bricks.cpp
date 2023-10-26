/* Bricks Game
 *
 * Copyright (C) 2017 LibTec
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_mixer.h>

#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <iostream>
#include <fstream>
using namespace std;

#include "vectors.cpp"

#define array_len(arr) (sizeof (arr) / sizeof (*(arr)))

#define WINDOW_WIDTH 400
#define WINDOW_HEIGHT 400
#define DEFAULT_SFX_VOLUME 0.05
#define DEFAULT_MUSIC_VOLUME 0.3
#define BALLS_SPEED_INIT 1.2
#define BALLS_SPEED_INCREASE 0.3
#define BALLS_MAX 3
#define BULLETS_MAX 64
#define SHOOT_RATE 0.2
#define POWERUPS_MAX 3
#define BRICK_MAX_HEALTH 5
#define PADDLE_CURVE_FACTOR 7.5
#define PADDLE_PUSH_FORCE 4
#define DEFAULT_PADDLE_WIDTH 0.3
#define DEFAULT_PADDLE_HEIGHT 0.075
#define DEFAULT_PADDLE_SPEED 2
#define DEFAULT_BRICK_WIDTH 0.2
#define DEFAULT_BRICK_HEIGHT 0.1
#define DEFAULT_BALL_SIZE 0.05
#define DEFAULT_BULLET_SIZE 0.02
#define DEFAULT_BULLET_SPEED 1.5
#define DEFAULT_POWERUP_SIZE 0.1
#define DEFAULT_POWERUP_SPEED 0.75
#define DEFAULT_GAME_WAIT_TIME 2

typedef unsigned char uchar;
typedef unsigned int uint;

struct Image {
  GLuint id;
  V2 dim;
};

struct Ball {
  V2 pos;
  V2 dir;
  float size;
};

struct Bullet {
  V2 pos;
  float speed;
  float size;
};

enum EmotionType {
  EMOTION_HAPPY,
  EMOTION_SAD,
};

struct Paddle {
  V2 pos;
  V2 dim;
  float speed;
  float blink_duration;
  Ball *caught_ball;
};

enum PowerupType {
  POWERUP_SHOOTER,
  POWERUP_GLUE,
  POWERUP_SPLIT,
  POWERUP_ENUM_LENGTH,
};

struct Powerup {
  PowerupType type;
  V2 pos;
  V2 dim;
  V2 dir;
  float animation_time;
};

struct Brick {
  V2 pos;
  V2 dim;
  float health;
};

struct BricksArray {
  int max;
  int count;
  Brick *items;
};

struct SoundsArray {
  int count;
  Mix_Chunk *items[9];
};

enum GameMode {
  GAME_STARTING,
  GAME_STARTED,
  GAME_OVER,
  GAME_WIN,
};


struct GameState {
  float sfx_volume;
  float music_volume;
  GameMode game_mode;
  PowerupType active_powerup;
  float powerup_chances[POWERUP_ENUM_LENGTH];
  float powerup_time;
  float split_time_init;
  float glue_time_init;
  float shooter_time_init;
  float game_wait_time;
  float shoot_timeout;
  float balls_speed;
  int lives_count_init;
  int lives_count;
  int score;
  int input_shoot;
  int input_left;
  int input_right;

  Paddle paddle;
  int balls_count;
  Ball balls[BALLS_MAX];
  int bullets_count;
  Bullet bullets[BULLETS_MAX];
  int powerups_count;
  Powerup powerups[POWERUPS_MAX];
  BricksArray bricks_array;
};


static double
rand32 (void)
{
  return rand () / (RAND_MAX + 1.0);
}


// static float
// min (float a, float b)
// {
//   if (a < b)
//     {
//       return a;
//     }
//   else
//     {
//       return b;
//     }
// }


// static float
// max (float a, float b)
// {
//   if (a > b)
//     {
//       return a;
//     }
//   else
//     {
//       return b;
//     }
// }


static void
draw_rect (V2 pos, V2 dim)
{
  dim /= 2;

  glBegin (GL_TRIANGLE_STRIP);
  glVertex2f (pos.x - dim.x, pos.y - dim.y);
  glVertex2f (pos.x - dim.x, pos.y + dim.y);
  glVertex2f (pos.x + dim.x, pos.y - dim.y);
  glVertex2f (pos.x + dim.x, pos.y + dim.y);
  glEnd ();
}


static void
draw_circle (V2 pos, float r)
{
  int sides = 12;
  float angle_step = M_PI * 2 / sides;
  float angle = angle_step;

  glBegin (GL_TRIANGLE_FAN);
  glVertex2f (pos.x, pos.y);
  glVertex2f (pos.x + r, pos.y);
  for (int i = 0; i < sides; ++i)
    {
      glVertex2f (cosf (angle) * r + pos.x,
                  sinf (angle) * r + pos.y);
      angle += angle_step;
    }
  glVertex2f (pos.x + r, pos.y);
  glEnd ();
}


static void
draw_semi_circle (V2 pos, float r, float begin_angle, float end_angle)
{
  if (end_angle < begin_angle)
    {
      end_angle += M_PI * 2;
    }

  int sides = 12;
  float angle_step = (end_angle - begin_angle) / sides;
  float angle = begin_angle;

  glBegin (GL_TRIANGLE_FAN);
  glVertex2f (pos.x, pos.y);

  for (int i = 0; i <= sides; ++i)
    {
      glVertex2f (cosf (angle) * r + pos.x,
                  sinf (angle) * r + pos.y);
      angle += angle_step;
    }

  glVertex2f (pos.x + r, pos.y);
  glEnd ();
}


static void
draw_image (Image *image, V2 pos, V2 dim, V2 image_offset, V2 image_portion_dim)
{
  dim /= 2;

  V2 t0 = image_offset / image->dim;
  V2 t1 = t0 + image_portion_dim / image->dim;

  glBindTexture (GL_TEXTURE_2D, image->id);

  glEnable (GL_TEXTURE_2D);
  glBegin (GL_TRIANGLE_STRIP);
  glTexCoord2f (t0.x, t1.y);
  glVertex2f (pos.x - dim.x, pos.y - dim.y);

  glTexCoord2f (t0.x, t0.y);
  glVertex2f (pos.x - dim.x, pos.y + dim.y);

  glTexCoord2f (t1.x, t1.y);
  glVertex2f (pos.x + dim.x, pos.y - dim.y);

  glTexCoord2f (t1.x, t0.y);
  glVertex2f (pos.x + dim.x, pos.y + dim.y);
  glEnd ();
  glDisable (GL_TEXTURE_2D);
}


static void
draw_paddle (Paddle *paddle, V2 eyes_target, EmotionType emotion, double dt)
{
  glColor3f (0.8, 0.6, 1);
  draw_rect (paddle->pos, paddle->dim);

  switch (emotion)
    {
    case EMOTION_HAPPY:
      {
        glColor3f (0, 0, 0);
        draw_semi_circle (paddle->pos, 0.03, M_PI, M_PI * 2);
      } break;
    case EMOTION_SAD:
      {
        glColor3f (0, 0, 0);
        V2 mouth_pos = paddle->pos;
        mouth_pos.y -= paddle->dim.y * 0.4;
        draw_semi_circle (mouth_pos, 0.03, 0, M_PI);
      } break;
    }

  if (rand32 () < 0.12 * dt)
    {
      paddle->blink_duration = 1;
    }

  if (paddle->blink_duration <= 0)
    {
      V2 eye_pos = paddle->pos;
      eye_pos.x -= 0.1;
      for (int eye_index = 0;
           eye_index < 2;
           ++eye_index)
        {
          glColor3f (0,0,0);
          draw_circle (eye_pos, 0.05);

          glColor3f (1,1,1);
          draw_circle (eye_pos, 0.04);

          float angle = atan2 (eyes_target.y - eye_pos.y,
                               eyes_target.x - eye_pos.x);

          V2 pupil = eye_pos;
          pupil.x += cosf (angle) * 0.01;
          pupil.y += sinf (angle) * 0.01;

          glColor3f (0,0,0);
          draw_circle (pupil, 0.025);

          eye_pos.x += 0.2;
        }
    }
  else
    {
      paddle->blink_duration -= 5 * dt;
    }
}


static void
draw_lives (int lives_count)
{
  V2 live_dim = {0.05, 0.03};
  V2 live_pos;
  live_pos.x = -1 + live_dim.x * 1.5;
  live_pos.y = -0.95;

  glColor3f (0.8, 0.6, 1.0);

  for (int live_index = 0;
       live_index < lives_count;
       ++live_index)
    {
      draw_rect (live_pos, live_dim);
      live_pos.x += live_dim.x * 2;;
    }
}


static void
draw_score (int score)
{
  V2 score_dim = {0.05, 0.03};
  V2 score_pos;
  score_pos.x = 1 - score_dim.x * 1.5;
  score_pos.y = -0.95;

  glColor3f (1.0, 0.8, 0.8);

  for (int score_index = 0;
       score_index < score;
       ++score_index)
    {
      draw_rect (score_pos, score_dim);
      score_pos.y += score_dim.y * 2;;
    }
}


static void
draw_powerup (Powerup *powerup, Image *powerups_image, double dt)
{
  powerup->animation_time += dt * 100;

  int animation_frame = ((int) powerup->animation_time / 16) * 16;
  int animation_type = powerup->type;

  V2 image_offset;
  image_offset.x = animation_frame;
  image_offset.y = animation_type * 16;

  glColor3f (1, 1, 1);
  draw_image (powerups_image, powerup->pos, powerup->dim,
              image_offset, (V2) {16, 16});
}


static int
is_rect_in_rect (V2 pos0, V2 dim0, V2 pos1, V2 dim1)
{
  if (pos0.x + dim0.x / 2 > pos1.x - dim1.x / 2 &&
      pos0.x - dim0.x / 2 < pos1.x + dim1.x / 2 &&
      pos0.y + dim0.y / 2 > pos1.y - dim1.y / 2 &&
      pos0.y - dim0.y / 2 < pos1.y + dim1.y / 2)
    {
      return 1;
    }
  else
    {
      return 0;
    }
}


static int
is_circle_in_rect (V2 circle_pos, float circle_r, V2 rect_pos, V2 rect_dim)
{
  if (circle_pos.x + circle_r > rect_pos.x - rect_dim.x / 2 &&
      circle_pos.x - circle_r < rect_pos.x + rect_dim.x / 2 &&
      circle_pos.y + circle_r > rect_pos.y - rect_dim.y / 2 &&
      circle_pos.y - circle_r < rect_pos.y + rect_dim.y / 2)
    {
      return 1;
    }
  else
    {
      return 0;
    }
}


static V2
get_intersection (V2 p0, V2 p1, V2 l0, V2 l1)
{
  V2 p2 = p1 - p0;
  V2 intersection;

  if (l0.x == l1.x)
    {
      intersection.x = l0.x;
      intersection.y = (l0.x - p0.x) * (p2.y / p2.x) + p0.y;
    }
  else if (l0.y == l1.y)
    {
      intersection.x = (l0.y - p0.y) * (p2.x / p2.y) + p0.x;
      intersection.y = l0.y;
    }
  else
    {
      assert (!"Line should be horisontal or vertical");
    }

  return intersection;
}


static Image
load_image (const char* filepath, int width, int height)
{
  Image image = {};
  image.dim.x = width;
  image.dim.y = height;

  size_t image_size = width * height * 4;

  ifstream file (filepath, ifstream::binary);
  file.seekg (0, file.end);
  streampos file_size = file.tellg ();
  assert (file_size > 0);
  assert ((size_t) file_size == image_size);
  file.seekg (0, file.beg);

  char *data = new char[image_size];
  file.read (data, image_size);
  file.close ();

  glGenTextures (1, &image.id);
  glBindTexture (GL_TEXTURE_2D, image.id);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8,
                width, height,
                0,
                GL_RGBA, GL_UNSIGNED_BYTE, data);

  delete[] data;

  return image;
}


static SoundsArray
load_sounds (const char *filepath_pattern)
{
  size_t pattern_len = strlen (filepath_pattern);
  char pattern[pattern_len + 1];
  strcpy (pattern, filepath_pattern);

  int wildcard_index = -1;
  for (uint i = 0; i < pattern_len; ++i)
    {
      if (pattern[i] == '?')
        {
          wildcard_index = i;
          break;
        }
    }

  assert (wildcard_index != -1);

  SoundsArray sounds_array;
  sounds_array.count = 0;

  for (int sound_index = 0;
       sound_index <= 9;
       ++sound_index)
    {
      pattern[wildcard_index] = (sounds_array.count + 1) + '0';
      Mix_Chunk *sound = Mix_LoadWAV (pattern);

      if (sound)
        {
          // Mix_VolumeChunk (sound, (int) (MIX_MAX_VOLUME * 0.1));
          sounds_array.items[sounds_array.count++] = sound;
        }
      else
        {
          break;
        }
    }

  return sounds_array;
}


static void
free_sounds (SoundsArray *sounds_array)
{
  for (int sound_index = 0;
       sound_index < sounds_array->count;
       ++sound_index)
    {
      Mix_FreeChunk (sounds_array->items[sound_index]);
    }
}


static void
play_random_sound (SoundsArray *sounds_array)
{
  int sound_index = sounds_array->count * rand32 ();
  Mix_PlayChannel (-1, sounds_array->items[sound_index], 0);
}


static void
load_config (const char *filepath, GameState *game_state)
{
  ifstream config_file(filepath);

  if (!config_file)
    {
      cerr << "Error: Can't open config file \"" << filepath << "\": ";
      perror (0);
      exit (1);
    }

  string config_option;
  while (config_file >> config_option)
    {
      if (config_option == "split_time")
        {
          config_file >> game_state->split_time_init;
        }
      else if (config_option == "glue_time")
        {
          config_file >> game_state->glue_time_init;
        }
      else if (config_option == "shooter_time")
        {
          config_file >> game_state->shooter_time_init;
        }
      else if (config_option == "split_chance")
        {
          config_file >> game_state->powerup_chances[POWERUP_SPLIT];
        }
      else if (config_option == "glue_chance")
        {
          config_file >> game_state->powerup_chances[POWERUP_GLUE];
        }
      else if (config_option == "shooter_chance")
        {
          config_file >> game_state->powerup_chances[POWERUP_SHOOTER];
        }
      else if (config_option == "lives_count")
        {
          config_file >> game_state->lives_count_init;
        }
      // else if (config_option == "music_volume") /// FINISH THIS
      //   {
      //     config_file >> game_state->lives_count_init;
      //   }
      else
        {
          cerr << "Error: Invalid config option \"" << config_option << "\"."<< endl;
          exit (1);
        }
    }

  cout << "split_time: "     << game_state->split_time_init   << endl;
  cout << "glue_time: "      << game_state->glue_time_init    << endl;
  cout << "shooter_time: "   << game_state->shooter_time_init << endl;
  cout << "split_chance: "   << game_state->powerup_chances[POWERUP_SPLIT]   << endl;
  cout << "glue_chance: "    << game_state->powerup_chances[POWERUP_GLUE]    << endl;
  cout << "shooter_chance: " << game_state->powerup_chances[POWERUP_SHOOTER] << endl;
  cout << "lives_count: "    << game_state->lives_count_init  << endl;
}


static BricksArray
load_map (const char *filepath)
{
  ifstream map_file(filepath);

  BricksArray bricks_array;
  bricks_array.max = 64;
  bricks_array.count = 0;
  bricks_array.items = new Brick[bricks_array.max];

  float brick_spacing = 0.02;
  float map_begin = -1 + brick_spacing + DEFAULT_BRICK_WIDTH / 2;
  float map_x = map_begin;
  float map_y = 1 - brick_spacing - DEFAULT_BRICK_HEIGHT / 2;

  char map_tile;
  while (map_file.read(&map_tile, 1))
    {
      if (map_tile == '\n')
        {
          map_x = map_begin;
          map_y -= DEFAULT_BRICK_HEIGHT + brick_spacing;
        }
      else if (map_tile >= '1' && map_tile <= '0' + BRICK_MAX_HEALTH)
        {
          Brick brick;
          brick.dim.x = DEFAULT_BRICK_WIDTH;
          brick.dim.y = DEFAULT_BRICK_HEIGHT;
          brick.pos.x = map_x;
          brick.pos.y = map_y;
          brick.health = (map_tile - '0');

          if (bricks_array.count == bricks_array.max)
            {
              bricks_array.max *= 2;
              Brick *new_bricks = new Brick[bricks_array.max];
              for (int brick_index = 0;
                   brick_index < bricks_array.count;
                   ++brick_index)
                {
                  new_bricks[brick_index] = bricks_array.items[brick_index];
                  delete[] bricks_array.items;
                }
            }

          bricks_array.items[bricks_array.count++] = brick;
          map_x += DEFAULT_BRICK_WIDTH + brick_spacing;
        }
      else
        {
          map_x += DEFAULT_BRICK_WIDTH + brick_spacing;
        }
    }

  map_file.close();

  return bricks_array;
}


static Ball
new_ball (V2 pos=(V2){0,0}, V2 dir=(V2){0,1})
{
  Ball ball = {};
  ball.pos = pos;
  ball.dir = dir;
  ball.size = DEFAULT_BALL_SIZE;

  return ball;
}


static Powerup
new_powerup (PowerupType type, V2 pos)
{
  Powerup powerup;
  powerup.type = type;
  powerup.pos = pos;
  powerup.dim.x = DEFAULT_POWERUP_SIZE;
  powerup.dim.y = DEFAULT_POWERUP_SIZE;
  powerup.dir.x = 0;
  powerup.dir.y = -DEFAULT_POWERUP_SPEED;
  return powerup;
}


static void
hit_brick (GameState *game_state, int *brick_index, int damage,
           SoundsArray *sounds_array)
{
  play_random_sound (sounds_array);

  BricksArray *bricks_array = &game_state->bricks_array;
  Brick *brick = bricks_array->items + *brick_index;

  brick->health -= damage;

  if (brick->health <= 0)
    {
      V2 spawn_pos = brick->pos;
      PowerupType types[] = {POWERUP_SPLIT, POWERUP_GLUE, POWERUP_SHOOTER};

      for (uint type_index = 0;
           (type_index < array_len (types) &&
            game_state->powerups_count < POWERUPS_MAX);
           ++type_index)
        {
          PowerupType type = types[type_index];
          if (rand32 () < game_state->powerup_chances[type])
            {
              game_state->powerups[game_state->powerups_count++] =
                new_powerup (type, spawn_pos);
              spawn_pos.y -= DEFAULT_POWERUP_SIZE;
            }
        }

      bricks_array->items[(*brick_index)--] =
        bricks_array->items[--bricks_array->count];
    }
}


static void
new_level (GameState *game_state, const char *map_filepath)
{
  game_state->game_mode = GAME_STARTED;

  game_state->balls_count = 0;
  game_state->bullets_count = 0;
  game_state->powerups_count = 0;
  game_state->powerup_time = 0;

  if (game_state->bricks_array.items)
    {
      delete[] game_state->bricks_array.items;
    }

  game_state->bricks_array = load_map (map_filepath);
  game_state->balls[game_state->balls_count++] = new_ball ();

  game_state->paddle.caught_ball = game_state->balls;
  game_state->paddle.pos.x = 0;
  game_state->paddle.pos.y = -0.85;
  game_state->paddle.dim.x = DEFAULT_PADDLE_WIDTH;
  game_state->paddle.dim.y = DEFAULT_PADDLE_HEIGHT;
  game_state->paddle.speed = DEFAULT_PADDLE_SPEED;
}


static void
new_game (GameState *game_state)
{
  game_state->balls_speed = BALLS_SPEED_INIT;
  game_state->lives_count = game_state->lives_count_init;
  game_state->score = 0;
}


int
main (int argc, char *argv[])
{
  srand (time (0));
  const char *map_filepath = "res/map1.txt";

  if (argc == 2)
    {
      map_filepath = argv[1];
    }

  GameState game_state = {};
  game_state.sfx_volume = DEFAULT_SFX_VOLUME;
  game_state.music_volume = DEFAULT_MUSIC_VOLUME;

  load_config ("config.txt", &game_state);
  new_game (&game_state);
  new_level (&game_state, map_filepath);

  int sdl_init_error = SDL_Init (SDL_INIT_VIDEO | SDL_INIT_AUDIO);
  assert (!sdl_init_error);
  SDL_Window *window =
    SDL_CreateWindow ("Bricks",
                      SDL_WINDOWPOS_UNDEFINED,
                      SDL_WINDOWPOS_UNDEFINED,
                      WINDOW_WIDTH, WINDOW_HEIGHT,
                      SDL_WINDOW_OPENGL);
  assert (window);

  SDL_GLContext gl_context = SDL_GL_CreateContext (window);
  assert (gl_context);

  int open_audio_error = Mix_OpenAudio (44100, MIX_DEFAULT_FORMAT, 2, 2048);
  assert (!open_audio_error);
  Mix_Volume (-1, (int) (MIX_MAX_VOLUME * game_state.sfx_volume));
  Mix_VolumeMusic ((int) (MIX_MAX_VOLUME * game_state.music_volume));

  SoundsArray ball_hit_sounds  = load_sounds ("res/ball_hit_sounds/ball_hit?.wav");
  SoundsArray shoot_hit_sounds = load_sounds ("res/shoot_hit_sounds/shoot_hit?.wav");
  SoundsArray shoot_sounds     = load_sounds ("res/shoot_sounds/shoot?.wav");

  Mix_Music *music = Mix_LoadMUS ("res/happy_adventure.wav");
  assert (music);
  // TODO: Music config
  int play_music_error = Mix_PlayMusic (music, -1);  // -1 == loops forever
  assert (!play_music_error);

  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glClearColor (0.0, 0.1, 0.2, 1.0);

  Image powerups_image = load_image ("res/powerups.raw", 64, 48);
  Mix_Chunk *powerup_sound = Mix_LoadWAV ("res/powerup.wav");
  assert (powerup_sound);

  Paddle *paddle = &game_state.paddle;
  Ball *balls = game_state.balls;
  Bullet *bullets = game_state.bullets;
  Powerup *powerups = game_state.powerups;
  BricksArray *bricks_array = &game_state.bricks_array;

  int window_opened = 1;
  uint last_time = SDL_GetTicks ();
  int pause = 0;

  while (window_opened)
    {
      SDL_Event event;

      while (SDL_PollEvent (&event))
        {
          switch (event.type)
            {
            case SDL_QUIT:
              {
                window_opened = 0;
              } break;
            case SDL_KEYUP:
              {
                switch (event.key.keysym.sym)
                  {
                  case SDLK_ESCAPE: {window_opened = 0;} break;
                  case SDLK_SPACE:
                  case SDLK_j: {game_state.input_shoot = 0;} break;
                  case SDLK_LEFT:
                  case SDLK_a: {game_state.input_left  = 0;} break;
                  case SDLK_RIGHT:
                  case SDLK_d: {game_state.input_right = 0;} break;
                  case SDLK_m:
                    {
                      if (game_state.music_volume > 0)
                        {
                          Mix_PauseMusic ();
                          game_state.music_volume = 0;
                        }
                      else if (game_state.sfx_volume > 0)
                        {
                          game_state.sfx_volume = 0;
                          Mix_Volume (-1, 0);
                        }
                      else
                        {
                          Mix_ResumeMusic ();
                          game_state.music_volume = DEFAULT_MUSIC_VOLUME;
                          game_state.sfx_volume = DEFAULT_SFX_VOLUME;
                          Mix_Volume (-1, (int) (MIX_MAX_VOLUME *
                                                 game_state.sfx_volume));
                        }
                    } break;
                  }
              } break;
            case SDL_KEYDOWN:
              {
                switch (event.key.keysym.sym)
                  {
                  case SDLK_p: {pause = pause ? 0 : 1;}
                  case SDLK_SPACE:
                  case SDLK_j: {game_state.input_shoot = 1;} break;
                  case SDLK_LEFT:
                  case SDLK_a: {game_state.input_left  = 1;} break;
                  case SDLK_RIGHT:
                  case SDLK_d: {game_state.input_right = 1;} break;
                  }
              } break;
            }
        }

      uint current_time = SDL_GetTicks ();
      double dt = (current_time - last_time) / 1000.0;
      last_time = current_time;

      if (pause)
        {
          SDL_GL_SwapWindow (window);
          continue;
        }

      if (game_state.game_mode != GAME_STARTED)
        {
          game_state.game_wait_time -= dt;

          switch (game_state.game_mode)
            {
            case GAME_WIN:
              {
                glClearColor (0.0, 0.3, 0.4, 1.0);
                glClear (GL_COLOR_BUFFER_BIT);
                paddle->pos.y += dt * 1.1;
                draw_paddle (paddle, (V2) {0,1}, EMOTION_HAPPY, dt);
                draw_lives (game_state.lives_count);
              } break;
            case GAME_OVER:
              {
                glClearColor (0.0, 0.1, 0.2, 1.0);
                glClear (GL_COLOR_BUFFER_BIT);
                draw_paddle (paddle, (V2) {0,1}, EMOTION_SAD, dt);
              } break;
            case GAME_STARTING:
            case GAME_STARTED: {}
            }

          if (game_state.game_wait_time <= 0)
            {
              game_state.balls_speed += BALLS_SPEED_INCREASE;
              new_level (&game_state, map_filepath);
            }

          SDL_GL_SwapWindow (window);
          continue;
        }

      if (game_state.balls_count == 0)
        {
          if (game_state.lives_count > 0)
            {
              --game_state.lives_count;
              balls[game_state.balls_count++] = new_ball();
              paddle->caught_ball = balls;
            }
          else
            {
              game_state.game_mode = GAME_OVER;
              game_state.game_wait_time = DEFAULT_GAME_WAIT_TIME;
              new_game (&game_state);
              continue;
            }
        }
      else if (bricks_array->count == 0)
        {
          game_state.game_mode = GAME_WIN;
          game_state.game_wait_time = DEFAULT_GAME_WAIT_TIME;
          ++game_state.score;
          continue;
        }

      glClearColor (0.0, 0.1, 0.2, 1.0);
      glClear (GL_COLOR_BUFFER_BIT);

      if (game_state.powerup_time > 0)
        {
          game_state.powerup_time -= dt;

          if (game_state.powerup_time <= 0 &&
              game_state.active_powerup == POWERUP_SPLIT)
            {
              // Disable POWERUP_SPLIT's effect.
              game_state.balls_count = 1;
            }
        }

      if (game_state.input_shoot)
        {

          if (paddle->caught_ball)
            {
              game_state.input_shoot = 0;
              paddle->caught_ball->dir.x = 0;
              paddle->caught_ball->dir.y = game_state.balls_speed;
              paddle->caught_ball = 0;
            }
          else if (game_state.shoot_timeout <= 0 &&
                   game_state.powerup_time > 0 &&
                   game_state.active_powerup == POWERUP_SHOOTER &&
                   game_state.bullets_count < BULLETS_MAX - 1)
          {
            play_random_sound (&shoot_sounds);
            game_state.shoot_timeout += SHOOT_RATE;

            Bullet new_bullets[2];

            for (int new_bullet_index = 0;
                 new_bullet_index < 2;
                 ++new_bullet_index)
              {
                new_bullets[new_bullet_index].pos = paddle->pos;
                new_bullets[new_bullet_index].size = DEFAULT_BULLET_SIZE;
                new_bullets[new_bullet_index].speed = DEFAULT_BULLET_SPEED;
              }

            new_bullets[0].pos.x -= paddle->dim.x / 2;
            new_bullets[1].pos.x += paddle->dim.x / 2;
            bullets[game_state.bullets_count++] = new_bullets[0];
            bullets[game_state.bullets_count++] = new_bullets[1];
          }
        }

      float paddle_move_distance = 0;
      if (game_state.input_left)
        {
          paddle_move_distance = -paddle->speed * dt;
        }
      if (game_state.input_right)
        {
          paddle_move_distance = +paddle->speed * dt;
        }

      paddle->pos.x += paddle_move_distance;

      if (paddle->pos.x - paddle->dim.x / 2 < -1)
        {
          paddle->pos.x = -1 + paddle->dim.x / 2;
        }
      else if (paddle->pos.x + paddle->dim.x / 2 > 1)
        {
          paddle->pos.x = 1 - paddle->dim.x / 2;
        }

      for (int bullet_index = 0;
           bullet_index < game_state.bullets_count;
           ++bullet_index)
        {
          Bullet *bullet = bullets + bullet_index;
          bullet->pos.y += bullet->speed * dt;

          glColor3f (1, 1, 1);
          V2 bullet_dim = {bullet->size, bullet->size};
          draw_rect (bullet->pos, bullet_dim);

          if (bullet->pos.y - bullet->size > 1)
            {
              bullets[bullet_index--] = bullets[--game_state.bullets_count];
            }
          else
            {
              for (int brick_index = 0;
                   brick_index < bricks_array->count;
                   ++ brick_index)
                {
                  Brick brick = bricks_array->items[brick_index];

                  if (is_rect_in_rect (bullet->pos, bullet_dim,
                                       brick.pos, brick.dim))
                    {
                      hit_brick (&game_state, &brick_index, 1, &shoot_hit_sounds);
                      bullets[bullet_index--] =
                        bullets[--game_state.bullets_count];
                    }
                }
            }
        }

      for (int ball_index = 0;
           ball_index < game_state.balls_count;
           ++ball_index)
        {
          Ball *ball = balls + ball_index;

          if (ball->pos.y + ball->size < -1)
            {
              balls[ball_index--] = balls[--game_state.balls_count];
              continue;
            }

          if (ball == paddle->caught_ball)
            {
              ball->pos.x = paddle->pos.x;
              ball->pos.y = paddle->pos.y + paddle->dim.y / 2 + ball->size;
            }
          else
            {
              if (is_circle_in_rect (ball->pos, ball->size,
                                     paddle->pos, paddle->dim))
                {
                  ball->pos.x += paddle_move_distance;
                  ball->dir.x += paddle_move_distance * PADDLE_PUSH_FORCE;
                  ball->dir = normalize (ball->dir) * game_state.balls_speed;
                }
              else
                {
                  ball->pos += ball->dir * dt;
                }

              if (ball->pos.y + ball->size > 1)
                {
                  ball->pos.y = 1 - ball->size;
                  ball->dir.y = -ball->dir.y;
                }

              if (ball->pos.x + ball->size > 1)
                {
                  ball->pos.x = 1 - ball->size;
                  ball->dir.x = -ball->dir.x;
                }
              else if (ball->pos.x - ball->size < -1)
                {
                  ball->pos.x = -1 + ball->size;
                  ball->dir.x = -ball->dir.x;
                }

              float paddle_top   = paddle->pos.y + paddle->dim.y / 2;
              float paddle_left  = paddle->pos.x - paddle->dim.x / 2;
              float paddle_right = paddle->pos.x + paddle->dim.x / 2;

              if (ball->pos.y - ball->size < paddle_top &&
                  ball->pos.x + ball->size > paddle_left &&
                  ball->pos.x - ball->size < paddle_right)
                {
                  if (game_state.powerup_time > 0 &&
                      game_state.active_powerup == POWERUP_GLUE &&
                      !paddle->caught_ball)
                    {
                      ball->dir.x = 0;
                      ball->dir.y = 0;
                      paddle->caught_ball = ball;
                    }
                  else if (ball->dir.x == 0)
                    {
                      ball->dir.x += (ball->pos.x - paddle->pos.x) * PADDLE_CURVE_FACTOR;
                      ball->dir.y = -ball->dir.y;
                      ball->dir =
                        normalize (ball->dir) * game_state.balls_speed;
                      ball->pos.y = paddle_top + ball->size;
                    }
                  else
                    {
                      V2 top_intersection =
                        get_intersection (ball->pos,
                                          ball->pos + ball->dir,
                                          (V2) {paddle_left,  paddle_top},
                                          (V2) {paddle_right, paddle_top});

                      if (ball->dir.x > 0 && top_intersection.x < paddle_left)
                        {
                          ball->dir.x = -ball->dir.x;
                          ball->pos.x = paddle_left - ball->size;
                        }
                      else if (ball->dir.x < 0 && top_intersection.x > paddle_right)
                        {
                          ball->dir.x = -ball->dir.x;
                          ball->pos.x = paddle_right + ball->size;
                        }
                      else
                        {
                          ball->dir.x += (ball->pos.x - paddle->pos.x) * PADDLE_CURVE_FACTOR;
                          ball->dir.y = -ball->dir.y;
                          ball->dir =
                            normalize (ball->dir) * game_state.balls_speed;
                          ball->pos.y = paddle_top + ball->size;
                        }
                    }

                  ball->pos += ball->dir * dt;
                }

              for (int brick_index = 0;
                   brick_index < bricks_array->count;
                   ++brick_index)
                {
                  Brick *brick = bricks_array->items + brick_index;

                  if (is_circle_in_rect (ball->pos, ball->size,
                                         brick->pos, brick->dim))
                    {
                      if (ball->dir.x == 0)
                        {
                          ball->dir.y = -ball->dir.y;
                        }
                      else if (ball->dir.y == 0)
                        {
                          ball->dir.x = -ball->dir.x;
                        }
                      else
                        {
                          V2 bot;
                          bot.y = brick->pos.y - brick->dim.y / 2.0;;
                          V2 top;
                          top.y = brick->pos.y + brick->dim.y / 2.0;;

                          if (ball->dir.x > 0)
                            {
                              // Check brick's left side
                              bot.x = brick->pos.x - brick->dim.x / 2.0;
                              top.x = bot.x;
                            }
                          else
                            {
                              // Check brick's right side
                              bot.x = brick->pos.x + brick->dim.x / 2.0;
                              top.x = bot.x;
                            }

                          V2 side_intersection =
                            get_intersection (ball->pos, ball->pos + ball->dir,
                                              bot, top);

                          if ((ball->dir.y > 0 && side_intersection.y > bot.y) ||
                              (ball->dir.y < 0 && side_intersection.y < top.y))
                            {
                              ball->dir.x = -ball->dir.x;
                            }
                          else
                            {
                              ball->dir.y = -ball->dir.y;
                            }
                        }

                      ball->pos += ball->dir * dt;

                      hit_brick (&game_state, &brick_index, 2, &ball_hit_sounds);
                    }
                }
            }

          glColor3f (0.9, 0.2, 0.5);
          draw_circle (ball->pos, ball->size);
        }

      for (int brick_index = 0;
           brick_index < bricks_array->count;
           ++brick_index)
        {
          Brick brick = bricks_array->items[brick_index];
          float brick_color = 1 - brick.health / BRICK_MAX_HEALTH + (1.0 / BRICK_MAX_HEALTH);

          glColor3f (1, brick_color + 0.1, brick_color + 0.2);
          draw_rect (brick.pos, brick.dim);
        }

      if (game_state.powerup_time > 0)
        {
          switch (game_state.active_powerup)
            {
            case POWERUP_GLUE:
              {
                V2 pos = paddle->pos;
                pos.y += paddle->dim.y / 2;
                V2 dim = paddle->dim;
                dim.y /= 3;
                glColor3f (0.6, 1.0, 0.6);
                draw_rect (pos, dim);
              } break;
            case POWERUP_SHOOTER:
              {
                if (game_state.shoot_timeout < 0)
                  {
                    // Aways check < 0 first, so the line
                    // game_state.shoot_timeout += SHOOT_RATE;
                    // calculates the time correctly.
                    game_state.shoot_timeout = 0;
                  }
                else if (game_state.shoot_timeout > 0)
                  {
                    game_state.shoot_timeout -= dt;
                  }

                V2 dim;
                dim.x = 0.03;
                dim.y = 0.12;
                V2 pos = paddle->pos;
                pos.x -= paddle->dim.x / 2 ;
                pos.y += 0.03;

                glColor3f (0.3, 0.3, 0.6);

                for (int i = 0; i < 2; ++i)
                  {
                    draw_rect (pos, dim);
                    pos.x += paddle->dim.x;
                  }
              } break;
            case POWERUP_SPLIT:
            case POWERUP_ENUM_LENGTH: {}
            }
        }

      for (int powerup_index = 0;
           powerup_index < game_state.powerups_count;
           ++powerup_index)
        {
          Powerup *powerup = powerups + powerup_index;

          if (is_circle_in_rect (powerup->pos, powerup->dim.x / 2,
                                 paddle->pos, paddle->dim))
            {
              Mix_PlayChannel (-1, powerup_sound, 0);
              game_state.active_powerup = powerup->type;

              // Disable POWERUP_SPLIT's effect.
              if (game_state.balls_count > 1)
                {
                  game_state.balls_count = 1;
                }

              switch (powerup->type)
                {
                case POWERUP_SPLIT:
                  {
                    game_state.powerup_time = game_state.split_time_init;
                    if (game_state.balls_count > 0)
                      {
                        while (game_state.balls_count < BALLS_MAX)
                          {
                            V2 dir;
                            dir.x = (rand32 () + 1) / 2;
                            dir.y = rand32 () + 0.1;
                            dir = normalize (dir) * game_state.balls_speed;
                            balls[game_state.balls_count++] =
                              new_ball (balls[0].pos, dir);
                          }
                      }
                  } break;
                case POWERUP_GLUE:
                  {
                    game_state.powerup_time = game_state.glue_time_init;
                  } break;
                case POWERUP_SHOOTER:
                  {
                    game_state.shoot_timeout = 0;
                    game_state.powerup_time = game_state.shooter_time_init;
                  } break;
                case POWERUP_ENUM_LENGTH: {}
                }

              powerups[powerup_index--] = powerups[--game_state.powerups_count];
            }

          powerup->pos += powerup->dir * dt;

          draw_powerup (powerup, &powerups_image, dt);
        }

      draw_paddle (paddle, balls[0].pos, EMOTION_HAPPY, dt);
      draw_lives (game_state.lives_count);
      draw_score (game_state.score);

      SDL_GL_SwapWindow (window);
    }


  if (bricks_array->items)
    {
      delete[] bricks_array->items;
    }

  free_sounds (&ball_hit_sounds);
  free_sounds (&shoot_hit_sounds);
  free_sounds (&shoot_sounds);

  SDL_GL_DeleteContext (gl_context);
  SDL_Quit ();

  return 0;
}
