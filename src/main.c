#include "ggf.c"
#include "particle_system.c"

enum {
  BRICK_EMPTY = 0,
  BRICK_NORMAL,
};

typedef struct game_state_t game_state_t;

typedef game_state_t (*game_state_update_func_t)(game_state_t state);
typedef void (*game_state_destroy_func_t)(game_state_t state);

struct game_state_t {
  game_state_update_func_t update_func;
  game_state_destroy_func_t destroy_func;
  void *data;
};

typedef struct {
  vec2 pos;
  vec2 size;
  f32 spacing;
  vec2 area;
  u32 cols, rows;
  u8 *bricks;
} bricks_t;

#define MAX_BALLS 1024

typedef struct {
  vec2 size;
  f32 speed;
  u32 count;
  vec2 positions[MAX_BALLS];
  vec2 directions[MAX_BALLS];
} balls_t;

typedef struct {
  i32 input_x_dir;
  vec2 pos;
  vec2 size;
  f32 speed;
} player_t;

typedef struct {
  b32 active;
  f32 time_active;
  f32 duration;
  game_state_t destination;
} transition_t;

typedef struct {
  ggf_font_t font;
  ggf_asset_stage_index_t playing_asset_stage;
} global_state_t;

#define MAX_SINGLE_USE_PARTICLES 128

#define MAX_POWERUPS 128

enum {
  POWERUP_THREE_BALLS = 0,
  POWERUP_DOUBLE_BALLS,
  POWERUP_COUNT,
};

typedef struct {
  f32 chance;
  ggf_texture_t texture;
} powerup_info_t;

global_variable powerup_info_t powerup_infos[POWERUP_COUNT];

typedef struct {
  u32 type;
  vec2 pos;
} powerup_t;

typedef struct {
  f32 time;
  player_t player;
  bricks_t bricks;
  balls_t balls;
  u32 single_use_particles_count;
  particle_system_t single_use_particles[MAX_SINGLE_USE_PARTICLES];
  transition_t lose_transition;
  u32 powerup_count;
  powerup_t powerups[MAX_POWERUPS];
} playing_state_t;

typedef struct {
  f32 time;
  vec3 start_bg_color, end_bg_color;
  transition_t restart_trans;
} game_over_state_t;

typedef struct {
  particle_system_t p_system;
  f32 time;
} victory_state_t;

global_variable global_state_t global_state;

internal_func bricks_t create_bricks(u32 cols, u32 rows, vec2 area, f32 spacing,
                                     vec2 pos) {
  bricks_t result = {};

  glm_vec2_copy(pos, result.pos);
  glm_vec2_copy((vec2){(area[0] - spacing * (cols - 1)) / cols,
                       (area[1] - spacing * (rows - 1)) / rows},
                result.size);
  result.spacing = spacing;
  glm_vec2_copy(area, result.area);

  result.cols = cols;
  result.rows = rows;
  result.bricks =
      ggf_memory_alloc(sizeof(u8) * cols * rows, GGF_MEMORY_TAG_GAME);
  for (u32 i = 0; i < cols * rows; i++) {
    result.bricks[i] = BRICK_NORMAL;
  }

  return result;
}

internal_func void destroy_bricks(bricks_t *bricks) {
  ggf_memory_free(bricks->bricks);
}

internal_func void draw_bricks(bricks_t *bricks) {
  for (u32 row = 0; row < bricks->rows; row++) {
    for (u32 col = 0; col < bricks->cols; col++) {
      u8 brick_value = bricks->bricks[row * bricks->cols + col];

      if (brick_value != BRICK_EMPTY) {
        vec2 pos = {col * (bricks->size[0] + bricks->spacing) + bricks->pos[0],
                    row * (bricks->size[1] + bricks->spacing) + bricks->pos[1]};
        ggf_draw_quad_extent(
            pos, bricks->size, 0.0f,
            (vec4){1.0f, 0.0f,
                   sinf(col * GGF_PI / (f32)bricks->cols) * 0.5f + 0.2f, 1.0f},
            NULL);
      }
    }
  }
}

internal_func void add_ball(balls_t *balls, vec2 pos, vec2 dir) {
  if (balls->count + 1 >= MAX_BALLS)
    return;
  glm_vec2_copy(pos, balls->positions[balls->count]);
  glm_vec2_copy(dir, balls->directions[balls->count]);
  balls->count++;
}

internal_func void remove_ball(balls_t *balls, u32 idx) {
  balls->count--;
  glm_vec2_copy(balls->positions[balls->count], balls->positions[idx]);
  glm_vec2_copy(balls->directions[balls->count], balls->directions[idx]);
}

internal_func b32 aabb_test(vec2 p1, vec2 s1, vec2 p2, vec2 s2) {
  return !(p1[0] > p2[0] + s2[0] || p1[0] + s1[0] < p2[0] ||
           p1[1] + s1[1] < p2[1] || p1[1] > p2[1] + s2[1]);
}

internal_func b32 check_point_in_bricks(bricks_t *bricks, vec2 point,
                                        i32 *out_col, i32 *out_row) {
  if (point[0] < bricks->pos[0] || point[1] < bricks->pos[1])
    return FALSE;

  i32 col = (point[0] - bricks->pos[0]) / (bricks->size[0] + bricks->spacing);
  i32 row = (point[1] - bricks->pos[1]) / (bricks->size[1] + bricks->spacing);
  if (col >= 0 && col < bricks->cols && row >= 0 && row < bricks->rows) {
    *out_col = col;
    *out_row = row;
    return TRUE;
  }
  return FALSE;
}

internal_func void destroy_brick(playing_state_t *state, u32 idx, u32 col,
                                 u32 row) {
  bricks_t *bricks = &state->bricks;
  vec2 brick_pos = {bricks->pos[0] + col * (bricks->size[0] + bricks->spacing), bricks->pos[1] + row * (bricks->size[1] + bricks->spacing)};

  particle_system_t *p_system =
      state->single_use_particles + state->single_use_particles_count;
  if (state->single_use_particles_count + 1 < MAX_SINGLE_USE_PARTICLES) {
    ++state->single_use_particles_count;
  }
  vec4 particle_color = {
      1.0f, 0.0f, sinf(col * GGF_PI / (f32)bricks->cols) * 0.5f + 0.2f, 1.0f};
  glm_vec4_copy(particle_color, p_system->begin_color);
  glm_vec4_copy(particle_color, p_system->end_color);
  p_system->count = 0;

  bricks->bricks[idx] = BRICK_EMPTY;
  for (f32 x = 0.0f; x < bricks->size[0]; x += 3.0f) {
    vec2 location = {
        brick_pos[0] + x,
        brick_pos[1]};
    local_persist u32 seed = 0;
    ++seed;
    emit_particles(
        p_system, 1, location, 20.0f,
        (vec2){ggf_randnf(seed + 1) * 1.0f, (ggf_randf(seed) + 0.3f) * 4.0f});
  }

  i32 powerup_gotten = -1;
  f32 lowest_powerup_chance = 2.0f;
  f32 luck = ((f64)rand() / RAND_MAX);

  for (u32 i = 0; i < POWERUP_COUNT; i++) {
    powerup_info_t info = powerup_infos[i];
    if (luck < info.chance && info.chance < lowest_powerup_chance) {
      powerup_gotten = i;
      lowest_powerup_chance = info.chance;
    }
  }

  if (powerup_gotten != -1 && state->powerup_count < MAX_POWERUPS) {
    powerup_t *powerup = state->powerups + state->powerup_count;
    powerup->type = powerup_gotten;
    glm_vec2_copy(brick_pos, powerup->pos);
    state->powerup_count++;
  }
}

internal_func void draw_press_space_to_restart(f32 time, f32 alpha) {
  u32 size = 32;
  char *text = (char *)"press [space] to restart";
  f32 width = ggf_font_get_text_width(&global_state.font, text, size);
  f32 shade = sinf(time * 5.0f) * 0.5f + 0.8f;
  ggf_gfx_draw_text(text, (vec2){(1280.0f - width) / 2.0f, 720.0f - 32.0f},
                    size, (vec4){shade, shade, shade, alpha},
                    &global_state.font);
  f32 padding = 15.0f;
  f32 y = 720.0f - 32.0f - 32.0f - padding * 0.5f;
  ggf_draw_quad_extent((vec2){(1280.0f - width) / 2.0f - padding, y},
                       (vec2){width + padding * 2.0f, 32.0f + padding * 2.0f},
                       10.0f, (vec4){0.0f, 0.0f, 0.0f, alpha}, NULL);
}

internal_func void update_transition(transition_t *trans,
                                     game_state_t *game_state) {
  if (trans->active) {
    trans->time_active += 1.0f / 60.0f;
    if (trans->time_active >= trans->duration) {
      game_state->destroy_func(*game_state);
      *game_state = trans->destination;
    }
  }
}

internal_func game_state_t create_playing_state();
internal_func void destroy_playing_state(game_state_t);
internal_func game_state_t update_playing_state(game_state_t);

internal_func game_state_t create_game_over_state();
internal_func void destroy_game_over_state(game_state_t);
internal_func game_state_t update_game_over_state(game_state_t);

internal_func game_state_t create_victory_state();
internal_func void destroy_victory_state(game_state_t);
internal_func game_state_t update_victory_state(game_state_t);

internal_func game_state_t create_playing_state() {
  playing_state_t *state =
      ggf_memory_alloc(sizeof(playing_state_t), GGF_MEMORY_TAG_GAME);

  ggf_asset_stage_use(global_state.playing_asset_stage);

  player_t *p = &state->player;
  glm_vec2_copy((vec2){125.0f, 25.0f}, p->size);
  glm_vec2_copy((vec2){1280.0f / 2.0f - p->size[0] / 2.0f, 650.0f}, p->pos);
  p->speed = 10.0f;

  state->bricks =
      create_bricks(10, 10, (vec2){1100.0f, 400.0f}, 5.0f, (vec2){0.0f, 0.0f});

  glm_vec2_copy((vec2){10.0f, 10.0f}, state->balls.size);
  state->balls.speed = 5.0f;
  add_ball(&state->balls,
           (vec2){p->pos[0] + p->size[0] / 2.0f, p->pos[1] - 20.0f},
           (vec2){0.9f, -1.0f});
  add_ball(&state->balls, (vec2){p->pos[0], p->pos[1] - 20.0f},
           (vec2){-0.5f, -1.0f});

  for (u32 i = 0; i < MAX_SINGLE_USE_PARTICLES; i++) {
    // FIXME: this failes when count is a power of two
    create_particle_system(100, (vec4){0.0f, 0.0f, 0.0f, 0.0f},
                           (vec4){0.0f, 0.0f, 0.0f, 0.0f}, 1.5f,
                           &state->single_use_particles[i]);
  }

  powerup_infos[POWERUP_THREE_BALLS].chance = 0.2f;
  powerup_infos[POWERUP_DOUBLE_BALLS].chance = 0.1f; 

  game_state_t result;
  result.update_func = update_playing_state;
  result.destroy_func = destroy_playing_state;
  result.data = (void *)state;
  return result;
}

internal_func void destroy_playing_state(game_state_t game_state) {
  playing_state_t *state = game_state.data;
  destroy_bricks(&state->bricks);
  for (u32 i = 0; i < MAX_SINGLE_USE_PARTICLES; i++) {
    destroy_particle_system(&state->single_use_particles[i]);
  }
  ggf_memory_free(state);
}

internal_func game_state_t update_playing_state(game_state_t game_state) {
  playing_state_t *state = game_state.data;
  player_t *p = &state->player;
  balls_t *balls = &state->balls;

  if (!ggf_asset_stage_is_loaded(global_state.playing_asset_stage)) {
    char *text = "Loading...";
    ggf_gfx_draw_text(text, (vec2){(1280.0f / 2.0f) / 2.0f, 720.0f / 2.0f}, 32,
                      (vec4){1.0f, 1.0f, 1.0f, 1.0f},
                      &global_state.font);
    return game_state;
  } else if (powerup_infos[POWERUP_THREE_BALLS].texture.id == 0) {
    void *data = ggf_asset_get_data(ggf_asset_get_handle("three_ball_powerup"));
    ggf_texture_create(data, GGF_TEXTURE_FORMAT_RGBA8, 32, 32,
      GGF_TEXTURE_FILTER_LINEAR, GGF_TEXTURE_WRAP_CLAMP_TO_BORDER, 
      &powerup_infos[POWERUP_THREE_BALLS].texture);
  }

  state->time += 1.0f / 60.0f;

  p->input_x_dir =
      ggf_input_key_down(GGF_KEY_D) - ggf_input_key_down(GGF_KEY_A);

  p->pos[0] += (f32)p->input_x_dir * p->speed;

  if (state->lose_transition.active) {
    p->pos[1] += 5.0f;
  }

  for (i32 i = balls->count - 1; i >= 0; i--) {
    vec2 pos, dir, size;
    glm_vec2_copy(balls->positions[i], pos);
    glm_vec2_copy(balls->directions[i], dir);
    glm_vec2_copy(balls->size, size);

    vec2 vel;
    glm_vec2_scale(dir, balls->speed, vel);

    b32 paddle_collision = aabb_test(pos, balls->size, p->pos, p->size);

    pos[0] += vel[0];
    b32 x_collision = FALSE;
    if (dir[0] < 0.0f) {
      if (pos[0] < 0.0f) {
        x_collision = TRUE;
      }
    }
    if (dir[0] > 0.0f) {
      if (pos[0] + size[0] > 1280.0f) {
        x_collision = TRUE;
      }
    }
    i32 col, row;
    vec2 x_col_points[4] = {
        {pos[0], pos[1]},
        {pos[0] + balls->size[0], pos[1]},
        {pos[0] + balls->size[0], pos[1] + balls->size[1]},
        {pos[0], pos[1] + balls->size[1]},
    };
    for (u32 i = 0; i < GGF_ARRAY_COUNT(x_col_points); i++) {
      if (check_point_in_bricks(&state->bricks, x_col_points[i], &col, &row)) {
        u32 idx = row * state->bricks.cols + col;
        if (state->bricks.bricks[idx] != BRICK_EMPTY) {
          x_collision = TRUE;
          destroy_brick(state, idx, col, row);
        }
      }
    }
    if (x_collision) {
      dir[0] = -dir[0];
      pos[0] -= vel[0];
    }

    pos[1] += vel[1];
    b32 y_collision = FALSE;
    if (dir[1] < 0.0f) {
      if (pos[1] < 0.0f) {
        y_collision = TRUE;
      }
    }
    if (dir[1] > 0.0f) {
      if (aabb_test(pos, size, p->pos, p->size)) {
        pos[1] = p->pos[1] - balls->size[1];

        f32 max_angle = glm_rad(50.0f);
        f32 along = ((pos[0] + balls->size[0] / 2.0f) - p->pos[0]) / p->size[0];
        f32 angle = glm_clamp(along - 0.5f, -0.5f, 0.5f) * max_angle * 2.0f -
                    GGF_PI / 2.0f;
        vec2 new_dir = {cosf(angle), sinf(angle)};
        glm_vec2_copy(new_dir, dir);
      }
      if (pos[1] > 720.0f) {
        remove_ball(balls, i);
        continue;
      }
    }
    vec2 y_col_points[4] = {
        {pos[0], pos[1]},
        {pos[0] + balls->size[0], pos[1]},
        {pos[0] + balls->size[0], pos[1] + balls->size[1]},
        {pos[0], pos[1] + balls->size[1]},
    };
    for (u32 i = 0; i < GGF_ARRAY_COUNT(y_col_points); i++) {
      if (check_point_in_bricks(&state->bricks, y_col_points[i], &col, &row)) {
        u32 idx = row * state->bricks.cols + col;
        if (state->bricks.bricks[idx] != BRICK_EMPTY) {
          y_collision = TRUE;
          destroy_brick(state, idx, col, row);
        }
      }
    }
    if (y_collision) {
      dir[1] = -dir[1];
      pos[1] -= vel[1];
    }

    glm_vec2_copy(pos, balls->positions[i]);
    glm_vec2_copy(dir, balls->directions[i]);
  }

  for (i32 i = state->powerup_count - 1; i >= 0; --i) {
    powerup_t *powerup = state->powerups + i;
    powerup->pos[1] += 5.0f;

    if (aabb_test(powerup->pos, (vec2){25.0f, 25.0f}, p->pos, p->size)) {
      if (powerup->type == POWERUP_THREE_BALLS) {
        vec2 spawn_pos = {powerup->pos[0] + 25.0f / 2.0f,
                      p->pos[1] - balls->size[1] - 10.0f};

        add_ball(&state->balls, spawn_pos, (vec2){0.0f, -1.0f});
        add_ball(&state->balls, spawn_pos, (vec2){0.85f, -0.525f});
        add_ball(&state->balls, spawn_pos, (vec2){-0.85f, -0.525f});
      }
      state->powerup_count--;
      state->powerups[i] = state->powerups[state->powerup_count];
    }
  }

  for (i32 i = state->single_use_particles_count - 1; i >= 0; --i) {
    particle_system_t *system = state->single_use_particles + i;
    update_particle_system(system);
    if (system->count == 0) {
      particle_system_t temp = state->single_use_particles[i];
      state->single_use_particles[i] =
          state->single_use_particles[state->single_use_particles_count - 1];
      state->single_use_particles[state->single_use_particles_count - 1] = temp;
      --state->single_use_particles_count;
    }
  }

  ggf_gfx_set_clear_color((vec3){0.0f, 0.0f, 0.05f});

  ggf_draw_quad_extent(p->pos, p->size, 0.0f, (vec4){0.2f, 0.2f, 1.0f, 1.0f},
                       NULL);
  glm_vec2_copy((vec2){(1280.0f - state->bricks.area[0]) / 2.0f, 50.0f},
                state->bricks.pos);
  draw_bricks(&state->bricks);

  vec4 ball_color = {0.2f, 1.0f, 0.2f, 1.0f};
  for (u32 i = 0; i < balls->count; i++) {
    ggf_draw_quad_extent(balls->positions[i], balls->size, -1.0f, ball_color,
                         NULL);
  }

  for (u32 i = 0; i < state->single_use_particles_count; i++) {
    render_particle_system(state->single_use_particles + i);
  }

  for (u32 i = 0; i < state->powerup_count; ++i) {
    powerup_t *powerup = state->powerups + i;
    ggf_draw_quad_extent(powerup->pos, (vec2){25.0f, 25.0f}, 0.0f, (vec4){1.0f, 1.0f, 1.0f, 1.0f}, &powerup_infos[POWERUP_THREE_BALLS].texture);
  }

  if (balls->count == 0 && !state->lose_transition.active) {
    for (u32 row = 0; row < state->bricks.rows; row++) {
      for (u32 col = 0; col < state->bricks.cols; col++) {
        u32 idx = row * state->bricks.rows + col;
        if (state->bricks.bricks[idx] != BRICK_EMPTY) {
          destroy_brick(state, idx, col, row);
        }
      }
    }
    state->lose_transition.active = TRUE;
    state->lose_transition.time_active = 0.0f;
    state->lose_transition.duration = state->single_use_particles[0].lifetime;
    state->lose_transition.destination = create_game_over_state();
  }

  update_transition(&state->lose_transition, &game_state);

  if (!state->lose_transition.active) {
    b32 are_bricks_left = FALSE;
    for (u32 i = 0; i < state->bricks.cols * state->bricks.rows; i++) {
      if (state->bricks.bricks[i] != BRICK_EMPTY) {
        are_bricks_left = TRUE;
        break;
      }
    }
    if (!are_bricks_left) {
      game_state.destroy_func(game_state);
      game_state = create_victory_state();
    }
  }

  return game_state;
}

internal_func game_state_t create_game_over_state() {
  game_over_state_t *state =
      ggf_memory_alloc(sizeof(game_over_state_t), GGF_MEMORY_TAG_GAME);

  glm_vec3_copy((vec3){0.0f, 0.0f, 0.05f}, state->start_bg_color);
  glm_vec3_copy((vec3){0.1f, 0.0f, 0.0f}, state->end_bg_color);

  state->restart_trans.duration = 0.5f;
   
  game_state_t result;
  result.update_func = update_game_over_state;
  result.destroy_func = destroy_game_over_state;
  result.data = state;
  return result;
}

internal_func game_state_t update_game_over_state(game_state_t game_state) {
  game_over_state_t *state = game_state.data;

  state->time += 1.0f / 60.0f;

  f32 rtrans_percent = state->restart_trans.time_active / state->restart_trans.duration;

  vec3 color;
  glm_vec3_lerp(state->start_bg_color, state->end_bg_color,
                glm_min(state->time, 1.0f), color);
  if (state->restart_trans.active) {
    glm_vec3_lerp(state->end_bg_color, state->start_bg_color,
                  glm_min(rtrans_percent, 1.0f), color);
  }
  ggf_gfx_set_clear_color(color);

  u32 size = 128;
  char *text = (char *)"Game Over!";

  f32 width = ggf_font_get_text_width(&global_state.font, text, size);
  glm_vec3_lerp((vec3){0.0f, 0.0f, 0.05f}, (vec3){1.0f, 0.0f, 0.0f},
                glm_min(state->time, 1.0f), color);
  f32 y = 720.0f / 2.0f - state->restart_trans.time_active * 2.0f * 720.0f;
  ggf_gfx_draw_text(text, (vec2){(1280.0f - width) / 2.0f, y}, size,
                    (vec4){color[0], color[1], color[2], 1.0f},
                    &global_state.font);

  draw_press_space_to_restart(state->time, 1.0f - rtrans_percent);

  if (!state->restart_trans.active && ggf_input_key_pressed(GGF_KEY_SPACE)) {
    state->restart_trans.active = TRUE;
    state->restart_trans.time_active = 0.0f;
    state->restart_trans.destination = create_playing_state();
  }

  update_transition(&state->restart_trans, &game_state);

  return game_state;
}

internal_func void destroy_game_over_state(game_state_t game_state) {
  game_over_state_t *state = game_state.data;
  ggf_memory_free(state);
}

internal_func game_state_t create_victory_state() {
  victory_state_t *state =
      ggf_memory_alloc(sizeof(victory_state_t), GGF_MEMORY_TAG_GAME);

  ggf_gfx_set_clear_color((vec3){0.0f, 0.05f, 0.0f});

  create_particle_system(2048, (vec4){1.0f, 0.95f, 0.0f, 1.0f},
                         (vec4){0.1f, 1.0f, 0.1f, 1.0f}, 1.0f,
                         &state->p_system);

  game_state_t result;
  result.update_func = &update_victory_state;
  result.destroy_func = &destroy_victory_state;
  result.data = state;
  return result;
}

internal_func void destroy_victory_state(game_state_t game_state) {
  victory_state_t *state = game_state.data;
  destroy_particle_system(&state->p_system);
  ggf_memory_free(state);
}

internal_func game_state_t update_victory_state(game_state_t game_state) {
  victory_state_t *state = game_state.data;

  state->time += 1.0f / 60.0f;

  f32 radius = 50.0f;
  local_persist u32 seed = 0;
  for (f32 x = 0.0f; x < 1280.0f; x += radius) {
    vec2 location = {x, 720.0f + radius};
    ++seed;
    emit_particles(&state->p_system, 1, location, radius,
                   (vec2){0.0f, -(ggf_randf(seed) + 0.5f) * 9.0f});
  }

  update_particle_system(&state->p_system);
  render_particle_system(&state->p_system);

  u32 size = 128;
  char *text = (char *)"VICTORY!";

  f32 cx = 1280.0f / 2.0f;

  f32 width = ggf_font_get_text_width(&global_state.font, text, size);
  f32 y = glm_lerp(720.0f + size, 720.0f / 2.0f - 128.0f,
                   glm_min(state->time * 3.0f, 1.0f));
  ggf_gfx_draw_text(text, (vec2){cx - width / 2.0f, y}, size,
                    (vec4){0.0f, 0.8f, 0.0f, 1.0f}, &global_state.font);

  draw_press_space_to_restart(state->time, 1.0f);

  game_state_t result = game_state;
  if (ggf_input_key_pressed(GGF_KEY_SPACE)) {
    game_state.destroy_func(game_state);
    result = create_playing_state();
  }
  return result;
}

i32 main(i32 argc, char **argv) {
  ggf_init(argc, argv);
  ggf_window_t *window = ggf_window_create("brick breaker", 1280, 720);
  ggf_window_set_resizable(window, TRUE);

  ggf_gfx_init(1280, 720);

  ggf_font_load("test old.png", "test old.csv", &global_state.font);

  ggf_asset_description_t assets[] = {
    {
      GGF_ASSET_TYPE_TEXTURE,
      "three_ball_powerup",
      "three_ball_powerup.png",
    }
  };
  global_state.playing_asset_stage = ggf_asset_stage_create(GGF_ARRAY_COUNT(assets), assets);

  game_state_t state = create_playing_state();

  ggf_camera_t camera = {0};
  glm_ortho(0.0f, 1280.0f, 720.0f, 0.0f, -100.0f, 100.0f,
            camera.view_projection);
  ggf_gfx_set_camera(&camera);

  while (ggf_window_is_open(window)) {
    ggf_poll_events();

    ggf_gfx_begin_frame();
    state = state.update_func(state);
    ggf_gfx_flush();

    ggf_window_swap_buffers(window);
  }

  state.destroy_func(state);

  ggf_font_destroy(&global_state.font);

  ggf_gfx_shutdown();
  ggf_window_destroy(window);
  ggf_shutdown();

  return 0;
}
