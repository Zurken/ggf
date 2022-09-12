#pragma once

typedef struct {
  u32 max_count;
  u32 count;

  vec4 begin_color;
  vec4 end_color;
  f32 lifetime;

  vec2 *positions;
  vec2 *velocities;
  f32 *time;
} particle_system_t;
