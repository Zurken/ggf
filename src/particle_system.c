#include "particle_system.h"

internal_func void create_particle_system(u32 max_count, vec4 begin_color,
                                          vec4 end_color, f32 particle_lifetime,
                                          particle_system_t *out_system) {
  out_system->max_count = max_count;
  out_system->count = 0;
  glm_vec4_copy(begin_color, out_system->begin_color);
  glm_vec4_copy(end_color, out_system->end_color);
  out_system->lifetime = particle_lifetime;

  u8 *block =
      ggf_memory_alloc(sizeof(vec2) * max_count * 2 + sizeof(f32) * max_count,
                       GGF_MEMORY_TAG_GAME);
  out_system->positions = (vec2 *)block;
  out_system->velocities = (vec2 *)block + max_count;
  out_system->time = (f32 *)(block + sizeof(vec2) * max_count * 2);
}

internal_func void destroy_particle_system(particle_system_t *system) {
  ggf_memory_free(system->positions);
}

internal_func void emit_particles(particle_system_t *system, u32 count,
                                  vec2 location, f32 radius, vec2 direction) {
  if (system->count + count >= system->max_count) {
    GGF_WARN("Max particle limit met.");
    count = system->max_count - 1;
  }

  u32 start_idx = system->count;

  local_persist i32 seed = 0;

  for (u32 i = 0; i < count; ++i) {
    f32 length = ggf_randf(++seed) * radius;
    f32 angle = ggf_randf(++seed) * GGF_PI2;
    vec2 offset = {cosf(angle) * length, sinf(angle) * length};
    glm_vec2_add(location, offset, system->positions[start_idx + i]);
  }
  for (u32 i = 0; i < count; ++i) {
    glm_vec2_copy(direction, system->velocities[start_idx + i]);
  }
  for (u32 i = 0; i < count; ++i) {
    system->time[start_idx + i] = 0.0f;
  }
  system->count += count;
}

internal_func void update_particle_system(particle_system_t *system) {
  for (i32 i = system->count - 1; i >= 0; i--) {
    glm_vec2_add(system->positions[i], system->velocities[i],
                 system->positions[i]);

    system->time[i] += 1.0f / 60.0f;

    if (system->time[i] >= system->lifetime) {
      u32 j = system->count - 1;
      glm_vec2_copy(system->positions[j], system->positions[i]);
      glm_vec2_copy(system->velocities[j], system->velocities[i]);
      system->time[i] = system->time[j];

      system->count--;
    }
  }
}

internal_func void render_particle_system(particle_system_t *system) {
  for (u32 i = 0; i < system->count; i++) {
    f32 life = system->time[i] / system->lifetime;
    vec4 color;
    glm_vec4_lerp(system->begin_color, system->end_color, life, color);
    f32 size = (1.0f - life) * 15.0f;
    ggf_draw_quad_extent(system->positions[i], (vec2){size, size},
                         -(1.0f - life) * 99.0f, color, NULL);
  }
}
