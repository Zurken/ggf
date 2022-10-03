#include "ggf.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define GLAD_IMPL
#include <glad_impl.h>

#include <pthread.h>
#include <stdarg.h>

#ifdef GGF_OSX
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#elif GGF_WINDOWS
#include <Windows.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC(sz) ggf_memory_alloc(sz, GGF_MEMORY_TAG_IMAGE)
#define STBI_REALLOC(p, newsz)                                                 \
  ggf_memory_realloc(p, newsz, GGF_MEMORY_TAG_IMAGE)
#define STBI_FREE(p) ggf_memory_free(p)
#include <stb_image.h>

#define GGF_STRING_HASH_MAX_LEN 256

// GGF

typedef struct {
  u64 size;
  ggf_memory_tag_t tag;
} ggf_internal_memory_allocation_t;

typedef struct {
  // Platform
  u32 argc;
  char **argv;
  GLFWwindow
      *first_window; // first window created. used for context object sharing.

  // Memory
  struct {
    struct {
      u64 tagged_allocations[GGF_MEMORY_TAG_MAX];
      u64 total_allocated;
    } stats;
    ggf_hash_map_t alloc_map;
    u64 total_alloc_size;
    u64 allocator_memory_requirement;
    ggf_dynamic_allocator_t allocator;
    void *allocator_block;

    pthread_mutex_t mutex;
  } memory;

  u64 string_hash_pows[GGF_STRING_HASH_MAX_LEN];

  void *input;

  void *assets;

  void *gfx;
} ggf_t;

internal_func b32 ggf_input_system_init();
internal_func void ggf_input_system_shutdown();
internal_func void ggf_input_system_update();
internal_func void ggf_input_system_set_key_state(ggf_key_t key, b8 is_down);
internal_func void
ggf_input_system_set_mouse_button_state(ggf_mouse_button_t button, b8 is_down);
internal_func void ggf_input_system_set_mouse_position(i32 x, i32 y);
internal_func void ggf_input_system_set_mouse_wheel(i32 x, i32 y);

internal_func b32 ggf_asset_system_init();
internal_func void ggf_asset_system_shutdown();

internal_func void ggf_gfx_resize(u32 width, u32 height);

global_variable ggf_t *ggf_data = NULL;

internal_func b32 ggf_internal_memory_intptr_cmp(void *first, void *second) {
  return *(void **)first == *(void **)second;
}

internal_func u64 ggf_internal_memory_intptr_hash(void *val) {
  return (u64)(intptr_t) * (void **)val;
}

b32 ggf_init(i32 argc, char **argv) {
  GGF_DEBUG("GGF INIT");

  u32 config_total_alloc_size = GGF_GIGABYTES(1);
  u32 ggf_data_size = sizeof(ggf_t);

  // MEMORY

  u64 allocator_requirement = 0;
  ggf_dynamic_allocator_create(config_total_alloc_size, &allocator_requirement,
                               NULL, NULL);

  u64 alloc_map_requirement = 0;
  ggf_hash_map_create(10000, sizeof(intptr_t),
                      sizeof(ggf_internal_memory_allocation_t), NULL, NULL,
                      NULL, NULL, &alloc_map_requirement, NULL);

  void *block = ggf_platform_mem_alloc(ggf_data_size + allocator_requirement +
                                       alloc_map_requirement);
  GGF_ASSERT(block);
  ggf_platform_mem_zero(block, ggf_data_size + allocator_requirement +
                                   alloc_map_requirement);

  ggf_data = (ggf_t *)block;
  ggf_data->memory.total_alloc_size = config_total_alloc_size;
  ggf_data->memory.allocator_memory_requirement = allocator_requirement;
  ggf_data->memory.allocator_block = ((void *)block + ggf_data_size);

  GGF_ASSERT(ggf_dynamic_allocator_create(
      config_total_alloc_size, &ggf_data->memory.allocator_memory_requirement,
      ggf_data->memory.allocator_block, &ggf_data->memory.allocator));

  void *alloc_map_memory = block + ggf_data_size + allocator_requirement;
  u64 empty_key = 0;
  ggf_hash_map_create(10000, sizeof(intptr_t),
                      sizeof(ggf_internal_memory_allocation_t), &empty_key,
                      &ggf_internal_memory_intptr_cmp,
                      &ggf_internal_memory_intptr_hash, alloc_map_memory,
                      &alloc_map_requirement, &ggf_data->memory.alloc_map);

  pthread_mutex_init(&ggf_data->memory.mutex, NULL);

  // PLATFORM

  ggf_data->argc = argc;
  ggf_data->argv = argv;

  if (glfwInit() != GLFW_TRUE) {
    GGF_FATAL("Failed to init GLFW!");
    return FALSE;
  }

  if (!ggf_asset_system_init()) {
    GGF_FATAL("Failed to init assets system!");
    return FALSE;
  }

  // string has precompute
  const u32 p = 31;
  const u32 m = 1e9 + 9;
  u64 pow = 1;
  for (u64 i = 0; i < GGF_STRING_HASH_MAX_LEN; i++) {
    ggf_data->string_hash_pows[i] = pow;
    pow = (pow * p) % m;
  }

  ggf_input_system_init();
  return TRUE;
}

void ggf_shutdown() {
  GGF_DEBUG("GGF - SHUTDOWN");

  ggf_input_system_shutdown();

  ggf_asset_system_shutdown();

  // window
  glfwTerminate();

  // memory

  pthread_mutex_destroy(&ggf_data->memory.mutex);

  GGF_DEBUG("ggf_shutdown: %d bytes not freed.",
            ggf_data->memory.stats.total_allocated);
  if (ggf_data->memory.stats.total_allocated != 0) {
    char usage[2048];
    ggf_memory_get_usage_string(usage, 2048);
    GGF_DEBUG(usage);
  }

  ggf_dynamic_allocator_destroy(&ggf_data->memory.allocator);
  ggf_platform_mem_free(ggf_data);
}

// snap

f32 ggf_snapf(f32 f, f32 step_size) {
  if (f > 0)
    return (f32)((i32)(f / step_size + 0.5)) * step_size;
  else
    return (f32)((i32)(f / step_size - 0.5)) * step_size;
}

f64 ggf_snapf64(f64 f, f64 step_size) {
  if (f > 0)
    return (f64)((i32)(f / step_size + 0.5)) * step_size;
  else
    return (f64)((i32)(f / step_size - 0.5)) * step_size;
}

// rng

f32 ggf_randf(u32 seed) {
  seed = (seed << 13) ^ seed;
  return (((seed * (seed * seed * 15731 + 789221) + 1376312589) & 0x7fffffff) /
          1073741824.0f) *
         0.5f;
}

f64 ggf_randf64(u32 seed) {
  seed = (seed << 13) ^ seed;
  return (((seed * (seed * seed * 15731 + 789221) + 1376312589) & 0x7fffffff) /
          1073741824.0) *
         0.5;
}

float ggf_randnf(u32 seed) {
  seed = (seed << 13) ^ seed;
  return (((seed * (seed * seed * 15731 + 789221) + 1376312589) & 0x7fffffff) /
          1073741824.0f) -
         1.0f;
}

f64 ggf_randnf64(u32 seed) {
  seed = (seed << 13) ^ seed;
  return (((seed * (seed * seed * 15731 + 789221) + 1376312589) & 0x7fffffff) /
          1073741824.0) -
         1.0;
}

u32 ggf_randi(u32 seed) {
  seed = (seed << 13) ^ seed;
  return ((seed * (seed * seed * 15731 + 789221) + 1376312589) & 0x7fffffff);
}

// hash
u64 ggf_hash_string(const char *str) {
  const u32 p = 31;
  const u32 m = 1e9 + 9;
  u64 hash_value = 0;
  u64 pow_i = 0;
  for (const char *c = str; *c != 0; c++)
    hash_value =
        (hash_value + (*c - 'a' + 1) * ggf_data->string_hash_pows[pow_i++]) % m;
  return hash_value;
}

// platform layer

void *ggf_platform_mem_alloc(u64 size) { return malloc(size); }

void ggf_platform_mem_free(void *memory) { free(memory); }

void *ggf_platform_mem_virtual_alloc(u64 size) {
#ifdef GGF_OSX
  return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
              -1, 0);
#elif GGF_WINDOWS
  return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#endif
}

void ggf_platform_mem_virtual_free(void *memory, u64 size) {
#ifdef GGF_OSX
  munmap(memory, size);
#elifdef GGF_WINDOWS
  VirtualFree(memory, size, MEM_RELEASE);
#endif
}

void ggf_platform_mem_zero(void *memory, u64 size) { memset(memory, 0, size); }

void ggf_platform_mem_copy(void *dest, void *source, u64 size) {
  memcpy(dest, source, size);
}

void ggf_platform_mem_set(void *memory, i32 value, u64 size) {
  memset(memory, value, size);
}

void ggf_platform_console_write(ggf_platform_console_color_t fg_color,
                                const char *message, ...) {
  va_list args;
  va_start(args, message);
  char buffer[1024];
  vsnprintf(buffer, 1024, message, args);
  fprintf(stdout, "\033[%d;%dm%s\033[0m\n", 1, 30 + fg_color, buffer);
  va_end(args);
}

void ggf_platform_console_write_error(ggf_platform_console_color_t fg_color,
                                      const char *error_message, ...) {
  va_list args;
  va_start(args, error_message);
  char buffer[1024];
  vsnprintf(buffer, 1024, error_message, args);
  fprintf(stderr, "\033[%d;%dm%s\033[0m\n", 1, 30 + fg_color, buffer);
  va_end(args);
}

void ggf_poll_events() {
  ggf_input_system_update();

  glfwPollEvents();
}

// window

internal_func void ggf_window_key_callback(GLFWwindow *window, i32 key,
                                           i32 scancode, i32 action, i32 mods) {
  ggf_input_system_set_key_state(key, action != GLFW_RELEASE);
}

internal_func void ggf_window_mouse_button_callback(GLFWwindow *window,
                                                    i32 button, i32 action,
                                                    i32 mods) {
  ggf_input_system_set_mouse_button_state(button, action != GLFW_RELEASE);
}

internal_func void ggf_window_mouse_position_callback(GLFWwindow *window, f64 x,
                                                      f64 y) {
  ggf_input_system_set_mouse_position((i32)x, (i32)y);
}

internal_func void ggf_window_mouse_wheel_callback(GLFWwindow *window,
                                                   f64 xoffset, f64 yoffset) {
  ggf_input_system_set_mouse_wheel(xoffset, yoffset);
}

internal_func void ggf_window_close_callback(GLFWwindow *window) {
  glfwSetWindowShouldClose(window, GLFW_TRUE);
}

internal_func void ggf_window_resize_callback(GLFWwindow *handle, i32 width,
                                              i32 height) {
  ggf_window_t *window = (ggf_window_t *)glfwGetWindowUserPointer(handle);
  window->width = width;
  window->height = height;
  ggf_gfx_resize(width, height);
}

ggf_window_t *ggf_window_create(const char *title, u32 width, u32 height) {
#ifdef __APPLE__
  /* We need to explicitly ask for a 3.2 context on OS X */
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

  GLFWwindow *glfw_handle =
      glfwCreateWindow(width, height, title, NULL, ggf_data->first_window);
  if (!glfw_handle) {
    return NULL;
  }

  if (ggf_data->first_window == NULL) {
    ggf_data->first_window = glfw_handle;
  }

  glfwMakeContextCurrent(glfw_handle);
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

  glfwSwapInterval(1);

  ggf_window_t *window = (ggf_window_t *)ggf_memory_alloc(
      sizeof(ggf_window_t), GGF_MEMORY_TAG_WINDOW);
  window->width = width;
  window->height = height;
  window->internal_handle = (void *)glfw_handle;

  glfwSetWindowUserPointer(glfw_handle, (void *)window);

  glfwSetKeyCallback(glfw_handle, &ggf_window_key_callback);
  glfwSetMouseButtonCallback(glfw_handle, &ggf_window_mouse_button_callback);
  glfwSetCursorPosCallback(glfw_handle, &ggf_window_mouse_position_callback);
  glfwSetScrollCallback(glfw_handle, &ggf_window_mouse_wheel_callback);
  glfwSetWindowCloseCallback(glfw_handle, &ggf_window_close_callback);
  glfwSetWindowSizeCallback(glfw_handle, &ggf_window_resize_callback);

  glfwShowWindow(glfw_handle);

  return window;
}

void ggf_window_destroy(ggf_window_t *window) {
  glfwDestroyWindow((GLFWwindow *)window->internal_handle);
  ggf_memory_free(window);
}

void ggf_window_switch_context(ggf_window_t *window) {
  glfwMakeContextCurrent((GLFWwindow *)window->internal_handle);
}

b32 ggf_window_is_open(ggf_window_t *window) {
  return !glfwWindowShouldClose((GLFWwindow *)window->internal_handle);
}

void ggf_window_swap_buffers(ggf_window_t *window) {
  glfwSwapBuffers((GLFWwindow *)window->internal_handle);
}

void ggf_window_set_title(ggf_window_t *window, const char *title) {
  glfwSetWindowTitle((GLFWwindow *)window->internal_handle, title);
}

void ggf_window_set_size(ggf_window_t *window, u32 width, u32 height) {
  glfwSetWindowSize((GLFWwindow *)window->internal_handle, width, height);
}

void ggf_window_set_position(ggf_window_t *window, u32 x, u32 y) {
  glfwSetWindowPos((GLFWwindow *)window->internal_handle, x, y);
}

void ggf_window_set_visible(ggf_window_t *window, b32 visible) {
  glfwSetWindowAttrib((GLFWwindow *)window->internal_handle, GLFW_VISIBLE,
                      visible ? GLFW_TRUE : GLFW_FALSE);
}

void ggf_window_set_resizable(ggf_window_t *window, b32 resizable) {
  glfwSetWindowAttrib((GLFWwindow *)window->internal_handle, GLFW_RESIZABLE,
                      resizable ? GLFW_TRUE : GLFW_FALSE);
}

void ggf_window_set_fullscreen(ggf_window_t *window, b32 fullscreen) {
  GLFWwindow *handle = (GLFWwindow *)window->internal_handle;
  glfwSetWindowAttrib(handle, GLFW_DECORATED,
                      fullscreen ? GLFW_FALSE : GLFW_TRUE);

  if (fullscreen) {
    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    i32 x, y, width, height;
    glfwGetMonitorWorkarea(monitor, &x, &y, &width, &height);
    glfwSetWindowMonitor(handle, monitor, x, y, width, height, GLFW_DONT_CARE);
  } else {
    glfwSetWindowMonitor(handle, NULL, 0, 0, 1280, 720, GLFW_DONT_CARE);
  }
}

void ggf_window_set_always_on_top(ggf_window_t *window, b32 always_on_top) {
  glfwSetWindowAttrib((GLFWwindow *)window->internal_handle, GLFW_FLOATING,
                      always_on_top ? GLFW_TRUE : GLFW_FALSE);
}

// filesystem

b32 ggf_filesystem_file_exists(const char *file_path) {
#ifdef GGF_OSX
  struct stat buffer;
  return stat(file_path, &buffer) == 0;
#elif GGF_WINDOWS
  DWORD attrib = GetFileAttributesA(file_path);
  return (attrib != INVALID_FILE_ATTRIBUTES &&
          !(attrib & FILE_ATTRIBUTE_DIRECTORY));
#endif
}

ggf_file_handle_t ggf_file_open(const char *path,
                                ggf_filemode_flags_t mode_flags) {
  const char *mode_string;

  b32 is_binary = (mode_flags & GGF_FILE_MODE_BINARY);
  if (mode_flags & GGF_FILE_MODE_READ && mode_flags & GGF_FILE_MODE_WRITE) {
    mode_string = is_binary ? "w+b" : "w+";
  } else if (mode_flags & GGF_FILE_MODE_READ) {
    mode_string = is_binary ? "rb" : "r";
  } else if (mode_flags & GGF_FILE_MODE_WRITE) {
    mode_string = is_binary ? "wb" : "w";
  } else {
    GGF_ERROR("ERROR - ggf_file_open: Invalid file mode: '%s'", path);
    return NULL;
  }

  FILE *file = fopen(path, mode_string);
  if (file == NULL) {
    GGF_ERROR("ERROR - ggf_file_open: Failed to open file '%s'", path);
    return NULL;
  }

  return file;
}

void ggf_file_close(ggf_file_handle_t handle) {
  GGF_ASSERT(handle);
  fclose((FILE *)handle);
}

u64 ggf_file_get_size(ggf_file_handle_t handle) {
  GGF_ASSERT(handle);

  fseek((FILE *)handle, 0, SEEK_END);
  u64 size = ftell((FILE *)handle);
  fseek((FILE *)handle, 0, SEEK_SET);
  return size;
}

b32 ggf_file_read_line(ggf_file_handle_t handle, u64 max_length,
                       char **line_buffer, u64 *out_line_length) {
  GGF_ASSERT(handle && max_length > 0 && out_line_length);

  char *buffer = *line_buffer;
  if (fgets(buffer, max_length, (FILE *)handle) != 0) {
    *out_line_length = strlen(buffer);
    return TRUE;
  }

  return FALSE;
}

b32 ggf_file_write_line(ggf_file_handle_t handle, const char *line) {
  GGF_ASSERT(handle && line);

  i32 result = fputs(line, (FILE *)handle);
  if (result != EOF)
    result = fputc('\n', (FILE *)handle);

  fflush((FILE *)handle);
  return result != EOF;
}

b32 ggf_file_read(ggf_file_handle_t handle, u64 data_size, void *out_data,
                  u64 *out_bytes_read) {
  GGF_ASSERT(handle && data_size && out_data && out_bytes_read);

  FILE *file = (FILE *)handle;

  *out_bytes_read = fread(out_data, 1, data_size, file);
  if (*out_bytes_read != data_size) {
    return FALSE;
  }

  return TRUE;
}

b32 ggf_file_write(ggf_file_handle_t handle, u64 data_size, void *data,
                   u64 *out_bytes_written) {
  GGF_ASSERT(handle && data_size && data && out_bytes_written);

  FILE *file = (FILE *)handle;

  *out_bytes_written = fwrite(data, 1, data_size, file);
  if (*out_bytes_written != data_size) {
    return FALSE;
  }
  fflush(file);
  return TRUE;
}

// INPUT LAYER

typedef struct {
  b8 prev_keys_down[GGF_KEY_MAX];
  b8 keys_down[GGF_KEY_MAX];
  b8 prev_mouse_buttons_down[GGF_MOUSE_BUTTON_MAX];
  b8 mouse_buttons_down[GGF_MOUSE_BUTTON_MAX];
  i32 prev_mouse_x, prev_mouse_y;
  i32 mouse_x, mouse_y;
  i32 mouse_wheel_x, mouse_wheel_y;
} ggf_input_t;

b32 ggf_input_system_init() {
  ggf_data->input = ggf_memory_alloc(sizeof(ggf_input_t), GGF_MEMORY_TAG_INPUT);
  return TRUE;
}

void ggf_input_system_shutdown() { ggf_memory_free(ggf_data->input); }

void ggf_input_system_update() {
  ggf_input_t *input = (ggf_input_t *)ggf_data->input;
  for (u32 i = 0; i < GGF_KEY_MAX; i++) {
    input->prev_keys_down[i] = input->keys_down[i];
  }
  for (u32 i = 0; i < GGF_MOUSE_BUTTON_MAX; i++) {
    input->prev_mouse_buttons_down[i] = input->mouse_buttons_down[i];
  }
  input->prev_mouse_x = input->mouse_x;
  input->prev_mouse_y = input->mouse_y;
}

void ggf_input_system_set_key_state(ggf_key_t key, b8 is_down) {
  ggf_input_t *input = (ggf_input_t *)ggf_data->input;
  input->keys_down[key] = is_down;
}

void ggf_input_system_set_mouse_button_state(ggf_mouse_button_t button,
                                             b8 is_down) {
  ggf_input_t *input = (ggf_input_t *)ggf_data->input;
  input->mouse_buttons_down[button] = is_down;
}

void ggf_input_system_set_mouse_position(i32 x, i32 y) {
  ggf_input_t *input = (ggf_input_t *)ggf_data->input;
  input->mouse_x = x;
  input->mouse_y = y;
}

void ggf_input_system_set_mouse_wheel(i32 x, i32 y) {
  ggf_input_t *input = (ggf_input_t *)ggf_data->input;
  input->mouse_wheel_x = x;
  input->mouse_wheel_y = y;
}

b32 ggf_input_key_down(ggf_key_t key) {
  return ((ggf_input_t *)ggf_data->input)->keys_down[key];
}

b32 ggf_input_key_pressed(ggf_key_t key) {
  ggf_input_t *input = (ggf_input_t *)ggf_data->input;
  return input->keys_down[key] && !input->prev_keys_down[key];
}

b32 ggf_input_key_released(ggf_key_t key) {
  ggf_input_t *input = (ggf_input_t *)ggf_data->input;
  return !input->keys_down[key] && input->prev_keys_down[key];
}

b32 ggf_input_mouse_down(ggf_mouse_button_t button) {
  ggf_input_t *input = (ggf_input_t *)ggf_data->input;
  return input->mouse_buttons_down[button];
}

b32 ggf_input_mouse_pressed(ggf_mouse_button_t button) {
  ggf_input_t *input = (ggf_input_t *)ggf_data->input;
  return input->mouse_buttons_down[button] &&
         !input->prev_mouse_buttons_down[button];
}

b32 ggf_input_mouse_released(ggf_mouse_button_t button) {
  ggf_input_t *input = (ggf_input_t *)ggf_data->input;
  return !input->mouse_buttons_down[button] &&
         input->prev_mouse_buttons_down[button];
}

void ggf_input_get_mouse_position(i32 *x, i32 *y) {
  ggf_input_t *input = (ggf_input_t *)ggf_data->input;
  *x = input->mouse_x;
  *y = input->mouse_y;
}

void ggf_input_get_mouse_delta(i32 *x, i32 *y) {
  ggf_input_t *input = (ggf_input_t *)ggf_data->input;
  *x = input->mouse_x - input->prev_mouse_x;
  *y = input->mouse_y - input->prev_mouse_y;
}

void ggf_input_get_mouse_wheel_delta(i32 *x, i32 *y) {
  ggf_input_t *input = (ggf_input_t *)ggf_data->input;
  *x = input->mouse_wheel_x;
  *y = input->mouse_wheel_y;
}

// MEMORY Layer

void *ggf_memory_alloc(u64 size, ggf_memory_tag_t memory_tag) {

  if (memory_tag == GGF_MEMORY_TAG_UNKNOWN) {
    GGF_WARN("WARNING - ggf_memory_alloc: memory allocated with "
             "GGF_MEMORY_TAG_UNKNOWN.");
  }

  pthread_mutex_lock(&ggf_data->memory.mutex);

  ggf_data->memory.stats.total_allocated += size;
  ggf_data->memory.stats.tagged_allocations[memory_tag] += size;

  void *memory = ggf_dynamic_allocator_alloc(&ggf_data->memory.allocator, size);
  ggf_platform_mem_zero(memory, size);

  ggf_internal_memory_allocation_t allocation;
  allocation.size = size;
  allocation.tag = memory_tag;
  ggf_hash_map_insert(&ggf_data->memory.alloc_map, &memory, &allocation);

  pthread_mutex_unlock(&ggf_data->memory.mutex);

  return memory;
}

void *ggf_memory_realloc(void *memory, u64 new_size,
                         ggf_memory_tag_t memory_tag) {
  if (memory) {
    ggf_internal_memory_allocation_t *allocation =
        (ggf_internal_memory_allocation_t *)ggf_hash_map_value_from_iter(
            &ggf_data->memory.alloc_map,
            ggf_hash_map_find(&ggf_data->memory.alloc_map, &memory));

    void *new_mem = ggf_memory_alloc(new_size, memory_tag);
    ggf_memory_copy(new_mem, memory, allocation->size);
    ggf_memory_free(memory);
    return new_mem;
  }
  return ggf_memory_alloc(new_size, memory_tag);
}

void ggf_memory_free(void *memory) {
  if (!memory)
    return;

  pthread_mutex_lock(&ggf_data->memory.mutex);

  ggf_internal_memory_allocation_t *allocation =
      (ggf_internal_memory_allocation_t *)ggf_hash_map_value_from_iter(
          &ggf_data->memory.alloc_map,
          ggf_hash_map_find(&ggf_data->memory.alloc_map, &memory));

  ggf_data->memory.stats.total_allocated -= allocation->size;
  ggf_data->memory.stats.tagged_allocations[allocation->tag] -=
      allocation->size;

  ggf_dynamic_allocator_free(&ggf_data->memory.allocator, memory,
                             allocation->size);
  ggf_hash_map_erase(&ggf_data->memory.alloc_map,
                     ggf_hash_map_find(&ggf_data->memory.alloc_map, &memory));

  pthread_mutex_unlock(&ggf_data->memory.mutex);
}

void ggf_memory_zero(void *memory, u64 size) {
  ggf_platform_mem_zero(memory, size);
}

void ggf_memory_copy(void *dest, void *source, u64 size) {
  ggf_platform_mem_copy(dest, source, size);
}

void ggf_memory_set(void *memory, i32 value, u64 size) {
  ggf_platform_mem_set(memory, value, size);
}

u64 ggf_memory_get_alloc_size(void *memory) {
  return ((ggf_internal_memory_allocation_t *)ggf_hash_map_value_from_iter(
              &ggf_data->memory.alloc_map,
              ggf_hash_map_find(&ggf_data->memory.alloc_map, &memory)))
      ->size;
}

void ggf_memory_get_usage_string(char *out_string, u64 max_length) {
  local_persist const char *memory_tag_strings[GGF_MEMORY_TAG_MAX] = {
      "UNKNOWN", "WINDOW", "LINEAR ALLOCATOR", "GRAPHICS", "INPUT", "STRING",
      "ASSETS",  "GAME",   "HASH MAP",         "IMAGE",    "DARRAY"};

  const u64 gib = 1024 * 1024 * 1024;
  const u64 mib = 1024 * 1024;
  const u64 kib = 1024;

  char buffer[8000] = "System memory use (tagged):\n";
  u64 offset = strlen(buffer);
  for (u32 i = 0; i < GGF_MEMORY_TAG_MAX; ++i) {
    char unit[4] = "XiB";
    f32 amount = 1.0f;
    if (ggf_data->memory.stats.tagged_allocations[i] >= gib) {
      unit[0] = 'G';
      amount = ggf_data->memory.stats.tagged_allocations[i] / (f32)gib;
    } else if (ggf_data->memory.stats.tagged_allocations[i] >= mib) {
      unit[0] = 'M';
      amount = ggf_data->memory.stats.tagged_allocations[i] / (f32)mib;
    } else if (ggf_data->memory.stats.tagged_allocations[i] >= kib) {
      unit[0] = 'K';
      amount = ggf_data->memory.stats.tagged_allocations[i] / (f32)kib;
    } else {
      unit[0] = 'B';
      unit[1] = 0;
      amount = (f32)ggf_data->memory.stats.tagged_allocations[i];
    }

    i32 length = snprintf(buffer + offset, 8000, "  %s: %.2f%s\n",
                          memory_tag_strings[i], amount, unit);
    offset += length;
  }

  strncpy(out_string, buffer, max_length);
}

u64 ggf_memory_get_alloc_count() {
  return ggf_data->memory.alloc_map.buckets_count;
}

// linear allocator

b32 ggf_linear_allocator_create(u64 size, void *memory,
                                ggf_linear_allocator_t *out_allocator) {
  out_allocator->size = size;
  out_allocator->marker = 0;
  out_allocator->owns_memory = memory == NULL;
  if (out_allocator->owns_memory) {
    out_allocator->memory =
        ggf_memory_alloc(size, GGF_MEMORY_TAG_LINEAR_ALLOCATOR);
  } else {
    out_allocator->memory = memory;
  }
  return TRUE;
}

void ggf_linear_allocator_destroy(ggf_linear_allocator_t *allocator) {
  if (allocator->owns_memory) {
    ggf_memory_free(allocator->memory);
  }
}

void *ggf_linear_allocator_alloc(ggf_linear_allocator_t *allocator, u64 size) {
  GGF_ASSERT(allocator->marker + size <= allocator->size);
  void *memory = allocator->memory + allocator->marker;
  allocator->marker += size;
  return memory;
}

void ggf_linear_allocator_reset(ggf_linear_allocator_t *allocator) {
  allocator->marker = 0;
}

void *
ggf_linear_allocator_get_memory_at_marker(ggf_linear_allocator_t *allocator,
                                          u64 marker) {
  return allocator->memory + marker;
}

// dynamic allocator
typedef struct {
  u64 total_size;
  ggf_freelist_t freelist;
  void *freelist_block;
  void *memory_block;
} ggf_dynamic_allocator_internal_state_t;

b32 ggf_dynamic_allocator_create(u64 total_size, u64 *memory_requirement,
                                 void *memory,
                                 ggf_dynamic_allocator_t *out_allocator) {
  GGF_ASSERT(total_size > 0);
  GGF_ASSERT(memory_requirement != NULL);

  u64 freelist_requirement = 0;
  ggf_freelist_create(total_size, &freelist_requirement, NULL, NULL);

  *memory_requirement = freelist_requirement +
                        sizeof(ggf_dynamic_allocator_internal_state_t) +
                        total_size;

  if (!memory) {
    return TRUE;
  }

  out_allocator->internal_memory = memory;
  ggf_dynamic_allocator_internal_state_t *state =
      out_allocator->internal_memory;
  state->total_size = total_size;
  state->freelist_block =
      (void *)(out_allocator->internal_memory +
               sizeof(ggf_dynamic_allocator_internal_state_t));
  state->memory_block = (void *)(state->freelist_block + freelist_requirement);

  ggf_freelist_create(total_size, &freelist_requirement, state->freelist_block,
                      &state->freelist);

  ggf_memory_zero(state->memory_block, total_size);

  return TRUE;
}

void ggf_dynamic_allocator_destroy(ggf_dynamic_allocator_t *allocator) {
  GGF_ASSERT(allocator && allocator->internal_memory);

  ggf_dynamic_allocator_internal_state_t *state = allocator->internal_memory;
  ggf_freelist_destroy(&state->freelist);
  ggf_memory_zero(state->memory_block, state->total_size);
  state->total_size = 0;
  allocator->internal_memory = 0;
}

void *ggf_dynamic_allocator_alloc(ggf_dynamic_allocator_t *allocator,
                                  u64 size) {
  GGF_ASSERT(allocator && size);

  ggf_dynamic_allocator_internal_state_t *state = allocator->internal_memory;
  u64 offset = 0;
  if (ggf_freelist_allocate_block(&state->freelist, size, &offset)) {
    return (void *)(state->memory_block + offset);
  }

  GGF_ERROR("ERROR - ggf_dynamic_allocator_alloc: failed to allocate block. "
            "Out of memory.");

  return NULL;
}

b32 ggf_dynamic_allocator_free(ggf_dynamic_allocator_t *allocator, void *memory,
                               u64 size) {
  GGF_ASSERT(allocator && memory && size);

  ggf_dynamic_allocator_internal_state_t *state = allocator->internal_memory;
  if (memory < state->memory_block ||
      memory > state->memory_block + state->total_size) {
    GGF_ERROR("ERROR - ggf_dynamic_allocator_free: memory being freed is out "
              "of bounds. memory is not allocated by this allocator.");
    return FALSE;
  }
  u64 offset = memory - state->memory_block;
  if (!ggf_freelist_free_block(&state->freelist, size, offset)) {
    GGF_ERROR("ERROR - ggf_dynamic_allocator_free: failed to free block.");
    return FALSE;
  }

  return TRUE;
}

u64 ggf_dynamic_allocator_get_free_space(ggf_dynamic_allocator_t *allocator) {
  ggf_dynamic_allocator_internal_state_t *state = allocator->internal_memory;
  return ggf_freelist_get_free_space(&state->freelist);
}

// CONTAINERS

// dynamic array

typedef enum {
  GGF_DARRAY_FIELD_CAPACITY,
  GGF_DARRAY_FIELD_LENGTH,
  GGF_DARRAY_FIELD_STRIDE,
  GGF_DARRAY_FIELD_MAX,
} ggf_darray_field_t;

void *ggf_darray_create(u64 length, u64 stride) {
  u64 header_size = GGF_DARRAY_FIELD_MAX * sizeof(u64);
  u64 array_size = length * stride;
  u64 *new_array =
      ggf_memory_alloc(header_size + array_size, GGF_MEMORY_TAG_DARRAY);
  new_array[GGF_DARRAY_FIELD_CAPACITY] = length;
  new_array[GGF_DARRAY_FIELD_LENGTH] = 0;
  new_array[GGF_DARRAY_FIELD_STRIDE] = stride;
  return (void *)(new_array + GGF_DARRAY_FIELD_MAX);
}

void ggf_darray_destroy(void *array) {
  u64 *header = (u64 *)array - GGF_DARRAY_FIELD_MAX;
  u64 header_size = GGF_DARRAY_FIELD_MAX * sizeof(u64);
  u64 total_size = header_size + header[GGF_DARRAY_FIELD_CAPACITY] *
                                     header[GGF_DARRAY_FIELD_STRIDE];
  ggf_memory_free(header);
}

internal_func inline u64 ggf_internal_darray_field_get(void *array, u64 field) {
  u64 *header = (u64 *)array - GGF_DARRAY_FIELD_MAX;
  return header[field];
}

internal_func inline void ggf_internal_darray_field_set(void *array, u64 field,
                                                        u64 value) {
  u64 *header = (u64 *)array - GGF_DARRAY_FIELD_MAX;
  header[field] = value;
}

void ggf_darray_clear(void *array) {
  ggf_internal_darray_field_set(array, GGF_DARRAY_FIELD_LENGTH, 0);
}

u64 ggf_darray_get_capacity(void *array) {
  return ggf_internal_darray_field_get(array, GGF_DARRAY_FIELD_CAPACITY);
}

u64 ggf_darray_get_length(void *array) {
  return ggf_internal_darray_field_get(array, GGF_DARRAY_FIELD_LENGTH);
}

u64 ggf_darray_get_stride(void *array) {
  return ggf_internal_darray_field_get(array, GGF_DARRAY_FIELD_STRIDE);
}

void *ggf_darray_resize(void *array) {
  u64 length = ggf_darray_get_length(array);
  u64 stride = ggf_darray_get_stride(array);
  void *temp = ggf_darray_create((2 * ggf_darray_get_capacity(array)), stride);
  ggf_memory_copy(temp, array, length * stride);

  ggf_internal_darray_field_set(temp, GGF_DARRAY_FIELD_LENGTH, length);
  ggf_darray_destroy(array);
  return temp;
}

void *ggf_darray_push(void *array, void *value_ptr) {
  u64 length = ggf_darray_get_length(array);
  u64 stride = ggf_darray_get_stride(array);
  if (length >= ggf_darray_get_capacity(array)) {
    array = ggf_darray_resize(array);
  }

  u64 addr = (u64)array;
  addr += (length * stride);
  ggf_memory_copy((void *)addr, value_ptr, stride);
  ggf_internal_darray_field_set(array, GGF_DARRAY_FIELD_LENGTH, length + 1);
  return array;
}

void ggf_darray_pop(void *array, void *dest) {
  u64 length = ggf_darray_get_length(array);
  u64 stride = ggf_darray_get_stride(array);
  u64 addr = (u64)array;
  addr += ((length - 1) * stride);
  ggf_memory_copy(dest, (void *)addr, stride);
  ggf_internal_darray_field_set(array, GGF_DARRAY_FIELD_LENGTH, length - 1);
}

void *ggf_darray_pop_at(void *array, u64 index, void *dest) {
  u64 length = ggf_darray_get_length(array);
  u64 stride = ggf_darray_get_stride(array);
  if (index >= length) {
    GGF_ERROR(
        "Index outside the bounds of this array! Length: %i, index: %index",
        length, index);
    return array;
  }

  u64 addr = (u64)array;
  ggf_memory_copy(dest, (void *)(addr + (index * stride)), stride);

  // If not on the last element, snip out the entry and copy the rest inward.
  if (index != length - 1) {
    ggf_memory_copy((void *)(addr + (index * stride)),
                    (void *)(addr + ((index + 1) * stride)),
                    stride * (length - index));
  }

  ggf_internal_darray_field_set(array, GGF_DARRAY_FIELD_LENGTH, length - 1);
  return array;
}

void *_darray_insert_at(void *array, u64 index, void *value_ptr) {
  u64 length = ggf_darray_get_length(array);
  u64 stride = ggf_darray_get_stride(array);
  if (index >= length) {
    GGF_ERROR(
        "Index outside the bounds of this array! Length: %i, index: %index",
        length, index);
    return array;
  }
  if (length >= ggf_darray_get_capacity(array)) {
    array = ggf_darray_resize(array);
  }

  u64 addr = (u64)array;

  // If not on the last element, copy the rest outward.
  if (index != length - 1) {
    ggf_memory_copy((void *)(addr + ((index + 1) * stride)),
                    (void *)(addr + (index * stride)),
                    stride * (length - index));
  }

  // Set the value at the index
  ggf_memory_copy((void *)(addr + (index * stride)), value_ptr, stride);

  ggf_internal_darray_field_set(array, GGF_DARRAY_FIELD_LENGTH, length + 1);
  return array;
}

// freelist

typedef struct ggf_freelist_node_t {
  u64 offset;
  u64 size;
  struct ggf_freelist_node_t *next;
} ggf_freelist_node_t;

typedef struct {
  u64 total_size;
  u64 max_entries;
  ggf_freelist_node_t *head;
  ggf_freelist_node_t *nodes;
} ggf_freelist_internal_state_t;

internal_func void
ggf_internal_freelist_return_node(ggf_freelist_t *freelist,
                                  ggf_freelist_node_t *node) {
  node->offset = GGF_INVALID_ID;
  node->size = GGF_INVALID_ID;
  node->next = NULL;
}

internal_func ggf_freelist_node_t *
ggf_internal_freelist_get_node(ggf_freelist_t *freelist) {
  ggf_freelist_internal_state_t *state = freelist->internal_memory;
  for (u64 i = 1; i < state->max_entries; i++) {
    if (state->nodes[i].offset == GGF_INVALID_ID) {
      return &state->nodes[i];
    }
  }

  return NULL;
}

b32 ggf_freelist_create(u64 total_size, u64 *memory_requirement, void *memory,
                        ggf_freelist_t *out_freelist) {
  u64 max_entries = total_size / (sizeof(void *) * sizeof(ggf_freelist_node_t));
  *memory_requirement = sizeof(ggf_freelist_internal_state_t) +
                        sizeof(ggf_freelist_node_t) * max_entries;
  if (!memory) {
    return TRUE;
  }

  out_freelist->internal_memory = memory;

  ggf_memory_zero(out_freelist->internal_memory, *memory_requirement);
  ggf_freelist_internal_state_t *state = out_freelist->internal_memory;
  state->nodes = (void *)(out_freelist->internal_memory +
                          sizeof(ggf_freelist_internal_state_t));
  state->max_entries = max_entries;
  state->total_size = total_size;

  state->head = &state->nodes[0];
  state->head->offset = 0;
  state->head->size = total_size;
  state->head->next = NULL;

  for (u64 i = 1; i < state->max_entries; i++) {
    state->nodes[i].offset = GGF_INVALID_ID;
    state->nodes[i].size = GGF_INVALID_ID;
  }

  return TRUE;
}

void ggf_freelist_destroy(ggf_freelist_t *freelist) {
  if (freelist && freelist->internal_memory) {
    ggf_freelist_internal_state_t *state = freelist->internal_memory;
    ggf_memory_zero(state,
                    sizeof(ggf_freelist_internal_state_t) +
                        sizeof(ggf_freelist_node_t) * state->max_entries);
    freelist->internal_memory = NULL;
  }
}

b32 ggf_freelist_allocate_block(ggf_freelist_t *freelist, u64 size,
                                u64 *out_offset) {
  GGF_ASSERT(freelist && freelist->internal_memory && size && out_offset);

  ggf_freelist_internal_state_t *state = freelist->internal_memory;
  ggf_freelist_node_t *node = state->head;
  ggf_freelist_node_t *prev = NULL;
  while (node) {
    if (node->size == size) {
      *out_offset = node->offset;
      if (prev) {
        prev->next = node->next;
      } else {
        state->head = node->next;
      }
      ggf_internal_freelist_return_node(freelist, node);
      return TRUE;
    } else if (node->size > size) {
      *out_offset = node->offset;
      node->size -= size;
      node->offset += size;
      return TRUE;
    }

    prev = node->next;
    node = node->next;
  }

  GGF_WARN("WARNING - ggf_freelist_allocate_block: Failed to allocate block "
           "from freelist.");
  return FALSE;
}

b32 ggf_freelist_free_block(ggf_freelist_t *freelist, u64 size, u64 offset) {
  GGF_ASSERT(freelist && freelist->internal_memory && size);

  ggf_freelist_internal_state_t *state = freelist->internal_memory;
  ggf_freelist_node_t *node = state->head;
  ggf_freelist_node_t *prev = NULL;

  if (!node) {
    ggf_freelist_node_t *new_node = ggf_internal_freelist_get_node(freelist);
    new_node->offset = offset;
    new_node->size = size;
    new_node->next = NULL;
    state->head = new_node;
    return TRUE;
  }

  while (node) {
    if (node->offset == offset) {
      node->size += size;

      if (node->next && node->next->offset == node->offset + node->size) {
        node->size += node->next->size;
        ggf_freelist_node_t *next = node->next->next;
        node->next = next->next;
        ggf_internal_freelist_return_node(freelist, next);
      }
      return TRUE;
    } else if (node->offset > offset) {
      ggf_freelist_node_t *new_node = ggf_internal_freelist_get_node(freelist);
      new_node->offset = offset;
      new_node->size = size;

      if (prev) {
        prev->next = new_node;
        new_node->next = node;
      } else {
        new_node->next = node;
        state->head = new_node;
      }

      if (new_node->next &&
          new_node->offset + new_node->size == new_node->next->offset) {
        new_node->size += new_node->next->size;
        ggf_freelist_node_t *rubbish = new_node->next;
        new_node->next = rubbish->next;
        ggf_internal_freelist_return_node(freelist, rubbish);
      }

      if (prev && prev->offset + prev->size == new_node->offset) {
        prev->size += new_node->size;
        ggf_freelist_node_t *rubbish = new_node;
        prev->next = rubbish->next;
        ggf_internal_freelist_return_node(freelist, rubbish);
      }

      return TRUE;
    }

    prev = node;
    node = node->next;
  }

  GGF_WARN(
      "WARNING - ggf_freelist_free_block: Unable to find block to be freed!");
  return FALSE;
}

void ggf_freelist_clear(ggf_freelist_t *freelist) {
  GGF_ASSERT(freelist && freelist->internal_memory);

  ggf_freelist_internal_state_t *state = freelist->internal_memory;
  for (u64 i = 1; i < state->max_entries; i++) {
    state->nodes[i].offset = GGF_INVALID_ID;
    state->nodes[i].size = GGF_INVALID_ID;
  }

  state->head->offset = 0;
  state->head->size = state->total_size;
  state->head->next = NULL;
}

u64 ggf_freelist_get_free_space(ggf_freelist_t *freelist) {
  GGF_ASSERT(freelist && freelist->internal_memory);

  ggf_freelist_internal_state_t *state = freelist->internal_memory;
  ggf_freelist_node_t *node = state->head;
  u64 free_space = 0;
  while (node) {
    free_space += node->size;
    node = node->next;
  }

  return free_space;
}

// hash map

ggf_hash_map_iter_t ggf_hash_map_next(ggf_hash_map_t *map,
                                      ggf_hash_map_iter_t it) {
  it += map->key_size + map->value_size;
  ggf_hash_map_iter_t end = ggf_hash_map_end(map);
  while (it < end && map->key_cmp_func(it, map->empty_key)) {
    it += map->key_size + map->value_size;
  }

  return it >= end ? NULL : it;
}

void ggf_hash_map_create(u64 bucket_count, u64 key_size, u64 value_size,
                         void *empty_key,
                         ggf_hash_map_key_comp_func_t key_cmp_func,
                         ggf_hash_map_hash_func_t hash_func, void *memory,
                         u64 *memory_requirement, ggf_hash_map_t *out_map) {
  GGF_ASSERT(memory_requirement);

  u64 pow2 = 1;
  while (pow2 < bucket_count)
    pow2 <<= 1;

  u64 mem_requirement = key_size + pow2 * (key_size + value_size);
  if (!memory && memory_requirement) {
    *memory_requirement = mem_requirement;
    return;
  }
  GGF_ASSERT(memory && *memory_requirement == mem_requirement);

  out_map->key_size = key_size;
  out_map->value_size = value_size;
  out_map->empty_key = memory;
  ggf_memory_copy(out_map->empty_key, empty_key, key_size);
  out_map->key_cmp_func = key_cmp_func;
  out_map->hash_func = hash_func;

  out_map->buckets_reserved_count = pow2;
  out_map->buckets = (u8 *)memory + key_size;
  ggf_hash_map_clear(out_map);

  out_map->memory = memory;
}

void ggf_hash_map_destroy(ggf_hash_map_t *map) { ggf_memory_free(map->memory); }

void ggf_hash_map_clear(ggf_hash_map_t *map) {
  ggf_hash_map_iter_t it = ggf_hash_map_begin(map);
  while ((it = ggf_hash_map_next(map, it))) {
    void *key = ggf_hash_map_key_from_iter(map, it);
    if (!map->key_cmp_func(key, map->empty_key))
      ggf_memory_copy(key, map->empty_key, map->key_size);
  }
  map->buckets_count = 0;
}

internal_func void ggf_internal_hash_map_rehash(ggf_hash_map_t *map,
                                                u64 count) {
  count = GGF_MAX(count, map->buckets_count * 2);
  ggf_hash_map_t new_map;
  u64 new_requirement = 0;
  ggf_hash_map_create(count, map->key_size, map->value_size, 0, 0, 0, 0,
                      &new_requirement, 0);
  void *new_mem = ggf_memory_alloc(new_requirement, GGF_MEMORY_TAG_HASH_MAP);
  ggf_hash_map_create(count, map->key_size, map->value_size, map->empty_key,
                      map->key_cmp_func, map->hash_func, new_mem,
                      &new_requirement, &new_map);
  for (ggf_hash_map_iter_t it = ggf_hash_map_begin(map);
       (it = ggf_hash_map_next(map, it));) {
    ggf_hash_map_insert(&new_map, ggf_hash_map_key_from_iter(map, it),
                        ggf_hash_map_value_from_iter(map, it));
  }
  ggf_hash_map_destroy(map);
  *map = new_map;
}

void ggf_hash_map_reserve(ggf_hash_map_t *map, u64 count) {
  if (count * 2 > map->buckets_reserved_count) {
    ggf_internal_hash_map_rehash(map, count * 2);
  }
}

internal_func u64 ggf_internal_hash_map_key_to_idx(ggf_hash_map_t *map,
                                                   void *key) {
  const u64 mask = map->buckets_reserved_count - 1;
  return map->hash_func(key) & mask;
}

internal_func u64 ggf_internal_hash_map_probe_next(ggf_hash_map_t *map,
                                                   u64 idx) {
  const u64 mask = map->buckets_reserved_count - 1;
  return (idx + 1) & mask;
}

internal_func u64 ggf_internal_hash_map_diff(ggf_hash_map_t *map, u64 a,
                                             u64 b) {
  const u64 mask = map->buckets_reserved_count - 1;
  return (map->buckets_reserved_count + (a - b)) & mask;
}

ggf_hash_map_iter_t ggf_hash_map_find(ggf_hash_map_t *map, void *key) {
  for (u64 idx = ggf_internal_hash_map_key_to_idx(map, key);;
       idx = ggf_internal_hash_map_probe_next(map, idx)) {
    u8 *key_i = map->buckets + idx * (map->value_size + map->key_size);
    if (map->key_cmp_func((void *)key_i, key))
      return key_i;
    else if (map->key_cmp_func((void *)key_i, map->empty_key))
      return NULL;
  }
}

ggf_hash_map_iter_t ggf_hash_map_insert(ggf_hash_map_t *map, void *key,
                                        void *value) {
  ggf_hash_map_reserve(map, map->buckets_count + 1);
  for (u64 idx = ggf_internal_hash_map_key_to_idx(map, key);;
       idx = ggf_internal_hash_map_probe_next(map, idx)) {
    u8 *key_i = map->buckets + idx * (map->value_size + map->key_size);
    if (map->key_cmp_func((void *)key_i, map->empty_key)) {
      ggf_memory_copy((void *)key_i, key, map->key_size);
      ggf_memory_copy((void *)(key_i + map->key_size), value, map->value_size);
      map->buckets_count++;
      return key_i;
    } else if (map->key_cmp_func((void *)key_i, key))
      return key_i;
  }
}

void ggf_hash_map_erase(ggf_hash_map_t *map, ggf_hash_map_iter_t it) {
  u64 bucket = (it - map->buckets) / (map->value_size + map->key_size);
  for (u64 idx = ggf_internal_hash_map_probe_next(map, bucket);;
       idx = ggf_internal_hash_map_probe_next(map, idx)) {
    u8 *key_i = map->buckets + idx * (map->value_size + map->key_size);
    if (map->key_cmp_func(key_i, map->empty_key)) {
      ggf_memory_copy(it, map->empty_key, map->key_size);
      map->buckets_count--;
      return;
    }
    u64 ideal = ggf_internal_hash_map_key_to_idx(map, key_i);
    if (ggf_internal_hash_map_diff(map, bucket, ideal) <
        ggf_internal_hash_map_diff(map, idx, ideal)) {
      ggf_memory_copy(it, key_i, map->key_size);
      bucket = idx;
    }
  }
}

// ASSET Layer

typedef struct {
  ggf_asset_type_t type;
  u32 name_hash, path_hash;
  char *path;
  u64 data_size;
  void *data;
} ggf_asset_t;

typedef struct {
  ggf_asset_t *assets;
} ggf_asset_stage_t;

typedef struct {
  char assets_path[512];
  ggf_asset_t **assets_to_load;
  pthread_mutex_t assets_to_load_mutex;
  pthread_cond_t assets_available_cond;
  b32 keep_loading_thread_running;
  pthread_t loader_thread;
  ggf_asset_stage_index_t current_stage_idx;
  u32 stage_count;
  ggf_asset_stage_t stages[64];
} ggf_asset_system_t;

// TODO: don't reload assets with same path

internal_func void *ggf_internal_asset_system_loading_thread(void *usr) {
  ggf_asset_system_t *system = (ggf_asset_system_t *)usr;

  while (system->keep_loading_thread_running) {
    pthread_mutex_lock(&system->assets_to_load_mutex);
    while (system->keep_loading_thread_running &&
           ggf_darray_get_length(system->assets_to_load) == 0)
      pthread_cond_wait(&system->assets_available_cond,
                        &system->assets_to_load_mutex);
    pthread_mutex_unlock(&system->assets_to_load_mutex);
    if (!system->keep_loading_thread_running)
      break;

    ggf_asset_t **end =
        system->assets_to_load + ggf_darray_get_length(system->assets_to_load);
    for (ggf_asset_t **asset_ptr = system->assets_to_load; asset_ptr != end;
         asset_ptr++) {
      ggf_asset_t *asset = *asset_ptr;

      if (asset->type == GGF_ASSET_TYPE_TEXTURE) {
        i32 width, height, comp_count;
        stbi_uc *pixels =
            stbi_load(asset->path, &width, &height, &comp_count, 3);
        u64 pixels_size = width * height * 3;

        u64 size = sizeof(ggf_texture_asset_data_t) + pixels_size;
        void *memory = ggf_memory_alloc(size, GGF_MEMORY_TAG_ASSET);

        ggf_texture_asset_data_t *data = (ggf_texture_asset_data_t *)memory;
        data->width = width;
        data->height = height;
        data->comp_count = comp_count;
        ggf_memory_copy((u8 *)memory + sizeof(ggf_texture_asset_data_t), pixels,
                        pixels_size);
        stbi_image_free(pixels);

        asset->data_size = size;
        asset->data = memory;
      } else {
        ggf_file_handle_t file = ggf_file_open(asset->path, GGF_FILE_MODE_READ);
        u64 size = ggf_file_get_size(file);
        void *memory = ggf_memory_alloc(size, GGF_MEMORY_TAG_ASSET);
        u64 bytes_read = 0;
        ggf_file_read(file, size, memory, &bytes_read);
        ggf_file_close(file);

        asset->data_size = size;
        asset->data = memory;
      }
      GGF_DEBUG("ASSET LOADED: %llu bytes", asset->data_size);
    }

    pthread_mutex_lock(&system->assets_to_load_mutex);
    ggf_darray_clear(system->assets_to_load);
    pthread_mutex_unlock(&system->assets_to_load_mutex);
  }

  return NULL;
}

b32 ggf_asset_system_init() {
  ggf_data->assets =
      ggf_memory_alloc(sizeof(ggf_asset_system_t), GGF_MEMORY_TAG_ASSET);
  ggf_asset_system_t *system = (ggf_asset_system_t *)ggf_data->assets;

  // get launch dir
  char launch_dir[256] = {};
#ifdef GGF_OSX
  u32 launch_dir_length = strlen(ggf_data->argv[0]);
  GGF_ASSERT(launch_dir_length < sizeof(launch_dir));
  while (ggf_data->argv[0][launch_dir_length] != '/')
    launch_dir_length--;
  strncpy(launch_dir, ggf_data->argv[0], launch_dir_length + 1);
#endif
#ifdef GGF_WINDOWS
  GetCurrentDirectory(GGF_ARRAY_COUNT(launch_dir), launch_dir);
#endif
  // find asset folder
  const char *prefix[] = {".", "..", "bin"};
  for (u32 i = 0; i < GGF_ARRAY_COUNT(prefix); i++) {
    snprintf(system->assets_path, sizeof(system->assets_path), "%s/%s/assets",
             launch_dir, prefix[i]);

    if (ggf_filesystem_file_exists(system->assets_path)) {
      break;
    }
    if (i == GGF_ARRAY_COUNT(prefix) - 1) {
      GGF_WARN("WARNING - ggf_asset_system_init: asset folder not found!");
      // return FALSE;
    }
  }
#ifdef GGF_WINDOWS
  // TODO: IMPORTANT: Fix this mess
  snprintf(system->assets_path, sizeof(system->assets_path), "./assets/");
#endif

  system->assets_to_load = ggf_darray_create(128, sizeof(ggf_asset_t *));
  pthread_mutex_init(&system->assets_to_load_mutex, NULL);
  pthread_cond_init(&system->assets_available_cond, NULL);
  system->keep_loading_thread_running = TRUE;
  pthread_create(&system->loader_thread, NULL,
                 ggf_internal_asset_system_loading_thread, system);

  system->current_stage_idx = GGF_INVALID_ID;

  return TRUE;
}

void ggf_asset_system_shutdown() {
  ggf_asset_system_t *system = (ggf_asset_system_t *)ggf_data->assets;

  system->keep_loading_thread_running = FALSE;
  pthread_cond_signal(&system->assets_available_cond);
  pthread_join(system->loader_thread, NULL);
  pthread_cond_destroy(&system->assets_available_cond);
  pthread_mutex_destroy(&system->assets_to_load_mutex);
  ggf_darray_destroy(system->assets_to_load);

  for (ggf_asset_stage_t *stage = system->stages;
       stage != system->stages + system->stage_count; stage++) {
    ggf_asset_t *end_assets =
        stage->assets + ggf_darray_get_length(stage->assets);
    for (ggf_asset_t *asset = stage->assets; asset != end_assets; asset++) {
      ggf_memory_free(asset->path);
      if (asset->data)
        ggf_memory_free(asset->data);
    }
    ggf_darray_destroy(stage->assets);
  }

  ggf_memory_free(ggf_data->assets);
}

ggf_asset_stage_index_t
ggf_asset_stage_create(u64 desc_count, ggf_asset_description_t *descriptions) {
  ggf_asset_system_t *system = (ggf_asset_system_t *)ggf_data->assets;
  ggf_asset_stage_index_t stage_index = system->stage_count;
  system->stage_count += 1;

  system->stages[stage_index].assets =
      ggf_darray_create(desc_count, sizeof(ggf_asset_t));

  ggf_asset_description_t *end = descriptions + desc_count;
  for (ggf_asset_description_t *desc = descriptions; desc != end; desc++) {
    ggf_asset_t asset;
    asset.type = desc->type;
    asset.name_hash = ggf_hash_string(desc->name);
    asset.path_hash = ggf_hash_string(desc->path);
    char full_path[512];
    if (!ggf_asset_system_find_full_asset_path(asset.type, (char *)desc->path,
                                               sizeof(full_path), full_path)) {
      GGF_ERROR("ERROR - ggf_asset_stage_create: could not find '%s' with "
                "filename '%s'",
                desc->name, desc->path);
      continue;
    }
    u64 path_len = strlen(full_path) + 1;
    asset.path = (char *)ggf_memory_alloc(path_len, GGF_MEMORY_TAG_STRING);
    ggf_memory_copy(asset.path, (void *)full_path, path_len);
    asset.data_size = 0;
    asset.data = NULL;
    ggf_darray_push(system->stages[stage_index].assets, &asset);
  }

  return stage_index;
}

void ggf_asset_stage_use(ggf_asset_stage_index_t stage_idx) {
  GGF_ASSERT(stage_idx != GGF_INVALID_ID);
  ggf_asset_system_t *system = (ggf_asset_system_t *)ggf_data->assets;

  ggf_asset_stage_t *new_stage = system->stages + stage_idx;
  ggf_asset_t *new_asset_end =
      new_stage->assets + ggf_darray_get_length(new_stage->assets);

  if (system->current_stage_idx != GGF_INVALID_ID) {
    ggf_asset_stage_t *current_stage =
        system->stages + system->current_stage_idx;
    ggf_asset_t *current_asset_end =
        current_stage->assets + ggf_darray_get_length(current_stage->assets);

    // copy over assets that are going to be used in the new stage
    for (ggf_asset_t *old_asset = current_stage->assets;
         old_asset != current_asset_end; old_asset++) {
      for (ggf_asset_t *new_asset = new_stage->assets;
           new_asset != new_asset_end; new_asset++) {
        if (old_asset->data && old_asset->path_hash == new_asset->path_hash) {
          new_asset->data_size = old_asset->data_size;
          new_asset->data = old_asset->data;
          old_asset->data_size = 0;
          old_asset->data = NULL;
        }
      }
    }

    // free assets that are no longer used
    for (ggf_asset_t *asset = current_stage->assets; asset != current_asset_end;
         asset++) {
      if (asset->data) {
        ggf_memory_free(asset->data);
        asset->data = NULL;
        asset->data_size = 0;
      }
    }
  }

  // add to queue
  pthread_mutex_lock(&system->assets_to_load_mutex);
  for (ggf_asset_t *asset = new_stage->assets; asset != new_asset_end;
       asset++) {
    if (!asset->data) {
      ggf_darray_push(system->assets_to_load, &asset);
    }
  }
  if (ggf_darray_get_length(system->assets_to_load) != 0)
    pthread_cond_signal(&system->assets_available_cond);
  pthread_mutex_unlock(&system->assets_to_load_mutex);

  system->current_stage_idx = stage_idx;
}

b32 ggf_asset_stage_is_loaded(ggf_asset_stage_index_t stage_idx) {
  ggf_asset_system_t *system = (ggf_asset_system_t *)ggf_data->assets;

  ggf_asset_stage_t *stage = system->stages + stage_idx;
  ggf_asset_t *end = stage->assets + ggf_darray_get_length(stage->assets);
  for (ggf_asset_t *asset = end - 1; asset >= stage->assets; asset--) {
    if (!asset->data)
      return FALSE;
  }
  return TRUE;
}

b32 ggf_asset_system_find_full_asset_path(ggf_asset_type_t type,
                                          const char *filename,
                                          u64 out_buffer_len,
                                          char *out_buffer) {
  ggf_asset_system_t *system = (ggf_asset_system_t *)ggf_data->assets;

  const char *dir = NULL;
  if (type == GGF_ASSET_TYPE_TEXTURE)
    dir = "textures";
  else if (type == GGF_ASSET_TYPE_FONT)
    dir = "fonts";
  else if (type == GGF_ASSET_TYPE_SHADER)
    dir = "shaders";
  else if (type == GGF_ASSET_TYPE_AUDIO)
    dir = "audio";

  if (dir) {
    snprintf(out_buffer, out_buffer_len, "%s/%s/%s", system->assets_path, dir,
             filename);
  } else {
    // TODO: Implement a search for the file
    GGF_WARN("WARNING - ggf_asset_system_find_full_asset_path: called with "
             "unknown assset type!");
  }

  return ggf_filesystem_file_exists(out_buffer);
}

ggf_asset_handle_t ggf_asset_get_handle(const char *name) {
  ggf_asset_system_t *system = (ggf_asset_system_t *)ggf_data->assets;

  u64 name_hash = ggf_hash_string(name);

  ggf_asset_t *assets = system->stages[system->current_stage_idx].assets;
  for (u32 i = 0; i < ggf_darray_get_length(assets); i++) {
    if ((assets + i)->name_hash == name_hash) {
      return i;
    }
  }

  return GGF_INVALID_ID;
}

void *ggf_asset_get_data(ggf_asset_handle_t handle) {
  ggf_asset_system_t *system = (ggf_asset_system_t *)ggf_data->assets;
  ggf_asset_t *asset =
      system->stages[system->current_stage_idx].assets + handle;
  return asset->data;
}

// GRAPHICS Layer

#define GGF_GFX_MAX_TEXTURE_UNITS 16
#define GGF_GFX_MAX_NUM_TRIANGLES 128
#define GGF_GFX_MAX_NUM_QUADS 2048
#define GGF_GFX_MAX_NUM_CIRCLES 128
#define GGF_GFX_MAX_NUM_TEXT_CHARACTERS 512

typedef struct {
  vec3 position;
  vec4 color;
  vec3 uv;
} ggf_basic_vertex_t;

typedef struct {
  vec3 world_position;
  vec2 local_position;
  vec4 color;
  vec3 uv;
} ggf_circle_vertex_t;

typedef struct {
  vec3 clear_color;

  u32 basic_vao, circle_vbo, circle_vao, basic_vbo, quad_ibo;
  ggf_uniform_buffer_t camera_ubo;
  u32 bound_texture_count;
  u32 bound_textures[GGF_GFX_MAX_TEXTURE_UNITS];
  ggf_texture_t white_texture;
  ggf_shader_t basic_shader, circle_shader, text_shader;

  u32 num_triangles;
  ggf_basic_vertex_t triangle_vertices[3 * GGF_GFX_MAX_NUM_TRIANGLES];

  u32 num_quads;
  ggf_basic_vertex_t quad_vertices[4 * GGF_GFX_MAX_NUM_QUADS];

  u32 num_circles;
  ggf_circle_vertex_t circle_vertices[4 * GGF_GFX_MAX_NUM_CIRCLES];

  u32 num_text_characters;
  ggf_basic_vertex_t text_vertices[4 * GGF_GFX_MAX_NUM_TEXT_CHARACTERS];

} ggf_gfx_t;

internal_func b32 ggf_internal_gfx_load_shader(const char *filename,
                                               ggf_shader_t *out_shader) {
  char full_file_path[512] = {};
  if (!ggf_asset_system_find_full_asset_path(
          GGF_ASSET_TYPE_SHADER, (char *)filename, 512, full_file_path)) {
    GGF_ERROR("ERROR - ggf_internal_gfx_load_shader: unable to find shader "
              "file '%s' at '%s'",
              filename, full_file_path);
    return FALSE;
  }

  ggf_file_handle_t file = ggf_file_open(full_file_path, GGF_FILE_MODE_READ);
  if (!file) {
    GGF_ERROR(
        "ERROR - ggf_internal_gfx_load_shader: Unable to open shader file!");
    return FALSE;
  }
  u64 file_size = ggf_file_get_size(file);
  char *file_data = ggf_memory_alloc(file_size + 1, GGF_MEMORY_TAG_GRAPHICS);
  u64 bytes_read;
  ggf_file_read(file, file_size, file_data, &bytes_read);
  ggf_file_close(file);
  file_data[bytes_read] = 0;

  char *vert_data_ptr = NULL, *frag_data_ptr = NULL;
  for (u64 i = 0; i < file_size; i++) {
    if (file_data[i] == '#') {
      char *end = &file_data[i + 1];
      while (*(end++) != '\n')
        ;
      if (file_data[i + 1] == 'V') {
        file_data[i] = 0;
        vert_data_ptr = end;
      } else if (file_data[i + 1] == 'F') {
        file_data[i] = 0;
        frag_data_ptr = end;
      }
    }
  }
  if (!vert_data_ptr || !frag_data_ptr) {
    GGF_ERROR("ERROR - ggf_internal_gfx_load_shader: failed to find #VERTEX "
              "and #FRAGMENT in shader source file.");
    return FALSE;
  }

  ggf_shader_stage_t stages[2] = {GGF_SHADER_STAGE_VERTEX,
                                  GGF_SHADER_STAGE_FRAGMENT};
  const char *shaders[2] = {
      vert_data_ptr,
      frag_data_ptr,
  };
  ggf_shader_create(2, stages, shaders, out_shader);

  ggf_memory_free(file_data);

  return TRUE;
}

b32 ggf_gfx_init(u32 width, u32 height) {
  GGF_ASSERT(width && height);

  ggf_gfx_t *gfx = ggf_memory_alloc(sizeof(ggf_gfx_t), GGF_MEMORY_TAG_GRAPHICS);
  ggf_data->gfx = gfx;

  glm_vec3_copy((vec3){0.0f, 0.0f, 0.0f}, gfx->clear_color);

  // basic vbo
  glGenBuffers(1, &gfx->basic_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, gfx->basic_vbo);
  glBufferData(GL_ARRAY_BUFFER,
               sizeof(ggf_basic_vertex_t) *
                   (GGF_GFX_MAX_NUM_TRIANGLES * 3 + GGF_GFX_MAX_NUM_QUADS * 4 +
                    GGF_GFX_MAX_NUM_TEXT_CHARACTERS * 4),
               NULL, GL_DYNAMIC_DRAW);

  // circle vbo
  glGenBuffers(1, &gfx->circle_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, gfx->circle_vbo);
  glBufferData(GL_ARRAY_BUFFER,
               sizeof(ggf_circle_vertex_t) * GGF_GFX_MAX_NUM_CIRCLES, NULL,
               GL_DYNAMIC_DRAW);

  u32 quad_indices[6 * GGF_GFX_MAX_NUM_QUADS];
  u32 offset = 0;
  for (u32 i = 0; i < GGF_GFX_MAX_NUM_QUADS; i++) {
    quad_indices[i * 6 + 0] = offset + 0;
    quad_indices[i * 6 + 1] = offset + 1;
    quad_indices[i * 6 + 2] = offset + 2;

    quad_indices[i * 6 + 3] = offset + 2;
    quad_indices[i * 6 + 4] = offset + 3;
    quad_indices[i * 6 + 5] = offset + 0;

    offset += 4;
  }
  glGenBuffers(1, &gfx->quad_ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gfx->quad_ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               sizeof(u32) * GGF_ARRAY_COUNT(quad_indices), quad_indices,
               GL_STATIC_DRAW);

  // basic vao
  glGenVertexArrays(1, &gfx->basic_vao);
  glBindVertexArray(gfx->basic_vao);
  glBindBuffer(GL_ARRAY_BUFFER, gfx->basic_vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gfx->quad_ibo);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ggf_basic_vertex_t),
                        (void *)offsetof(ggf_basic_vertex_t, position));
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(ggf_basic_vertex_t),
                        (void *)offsetof(ggf_basic_vertex_t, color));
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(ggf_basic_vertex_t),
                        (void *)offsetof(ggf_basic_vertex_t, uv));

  // circle vao
  glGenVertexArrays(1, &gfx->circle_vao);
  glBindVertexArray(gfx->circle_vao);
  glBindBuffer(GL_ARRAY_BUFFER, gfx->circle_vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gfx->quad_ibo);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ggf_circle_vertex_t),
                        (void *)offsetof(ggf_circle_vertex_t, world_position));
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ggf_circle_vertex_t),
                        (void *)offsetof(ggf_circle_vertex_t, local_position));
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ggf_circle_vertex_t),
                        (void *)offsetof(ggf_circle_vertex_t, color));
  glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(ggf_circle_vertex_t),
                        (void *)offsetof(ggf_circle_vertex_t, uv));

  ggf_internal_gfx_load_shader("basic.glsl", &gfx->basic_shader);
  ggf_internal_gfx_load_shader("circle.glsl", &gfx->circle_shader);
  ggf_internal_gfx_load_shader("text.glsl", &gfx->text_shader);

  ggf_uniform_buffer_create(sizeof(mat4), NULL, &gfx->camera_ubo);

  ggf_shader_bind_uniform_buffer(&gfx->basic_shader, "camera",
                                 &gfx->camera_ubo);
  ggf_shader_bind_uniform_buffer(&gfx->circle_shader, "camera",
                                 &gfx->camera_ubo);
  ggf_shader_bind_uniform_buffer(&gfx->text_shader, "camera", &gfx->camera_ubo);

  char sampler_uniform_name[64];
  for (u32 i = 0; i < GGF_GFX_MAX_TEXTURE_UNITS; i++) {
    snprintf(sampler_uniform_name, sizeof(sampler_uniform_name), "textures[%d]",
             i);

    glUseProgram(gfx->basic_shader.id);
    i32 location =
        glGetUniformLocation(gfx->basic_shader.id, sampler_uniform_name);
    glUniform1i(location, i);

    glUseProgram(gfx->circle_shader.id);
    location =
        glGetUniformLocation(gfx->circle_shader.id, sampler_uniform_name);
    glUniform1i(location, i);

    glUseProgram(gfx->text_shader.id);
    location = glGetUniformLocation(gfx->text_shader.id, sampler_uniform_name);
    glUniform1i(location, i);
  }

  glEnable(GL_FRAMEBUFFER_SRGB);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  u32 white_pixel_data = 0xFFFFFFFF;
  ggf_texture_create(&white_pixel_data, GGF_TEXTURE_FORMAT_RGBA8, 1, 1,
                     GGF_TEXTURE_FILTER_NEAREST, GGF_TEXTURE_WRAP_REPEAT,
                     &gfx->white_texture);

  gfx->bound_texture_count = 1;
  gfx->bound_textures[0] = gfx->white_texture.id;

  i32 err = glGetError();
  if (err != GL_NO_ERROR) {
    GGF_ERROR("ERROR - ggf_gfx_init: OpenGL error: %i", err);
    return FALSE;
  }

  return TRUE;
}

void ggf_gfx_shutdown() {
  ggf_gfx_t *gfx = ggf_data->gfx;

  ggf_texture_destroy(&gfx->white_texture);

  glDeleteBuffers(1, &gfx->quad_ibo);
  glDeleteBuffers(1, &gfx->circle_vbo);
  glDeleteBuffers(1, &gfx->basic_vbo);
  glDeleteVertexArrays(1, &gfx->circle_vao);
  glDeleteVertexArrays(1, &gfx->basic_vao);

  ggf_shader_destroy(&gfx->basic_shader);

  ggf_memory_free(gfx);
}

void ggf_gfx_resize(u32 width, u32 height) {
  glViewport(0, 0, width, height);
}

void ggf_gfx_set_clear_color(vec3 color) {
  ggf_gfx_t *gfx = ggf_data->gfx;
  glm_vec3_copy(color, gfx->clear_color);
}

void ggf_gfx_begin_frame() {
  ggf_gfx_t *gfx = ggf_data->gfx;
  glClearColor(gfx->clear_color[0], gfx->clear_color[1], gfx->clear_color[2], 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void ggf_gfx_flush() {
  ggf_gfx_t *gfx = ggf_data->gfx;

  for (u32 i = 0; i < gfx->bound_texture_count; i++) {
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, gfx->bound_textures[i]);
  }
  gfx->bound_texture_count = 1;

  // basic drawing - quads
  glUseProgram(gfx->basic_shader.id);
  glBindVertexArray(gfx->basic_vao);
  glBindBuffer(GL_ARRAY_BUFFER, gfx->basic_vbo);
  if (gfx->num_triangles > 0) {
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    sizeof(ggf_basic_vertex_t) * gfx->num_triangles * 3,
                    (void *)gfx->triangle_vertices);
    glDrawArrays(GL_TRIANGLES, 0, gfx->num_triangles * 3);
    gfx->num_triangles = 0;
  }
  if (gfx->num_quads > 0) {
    glBufferSubData(GL_ARRAY_BUFFER,
                    sizeof(ggf_basic_vertex_t) * gfx->num_triangles * 3,
                    sizeof(ggf_basic_vertex_t) * gfx->num_quads * 4,
                    (void *)gfx->quad_vertices);
    glDrawElementsBaseVertex(GL_TRIANGLES, gfx->num_quads * 6, GL_UNSIGNED_INT,
                             0, gfx->num_triangles * 3);
    gfx->num_quads = 0;
  }
  if (gfx->num_text_characters > 0) {
    glDisable(GL_DEPTH_TEST);
    glUseProgram(gfx->text_shader.id);
    glBufferSubData(GL_ARRAY_BUFFER,
                    sizeof(ggf_basic_vertex_t) *
                        (gfx->num_triangles * 3 + gfx->num_quads * 4),
                    sizeof(ggf_basic_vertex_t) * gfx->num_text_characters * 4,
                    (void *)gfx->text_vertices);
    glDrawElementsBaseVertex(GL_TRIANGLES, gfx->num_text_characters * 6,
                             GL_UNSIGNED_INT, 0,
                             gfx->num_triangles * 3 + gfx->num_quads * 4);
    gfx->num_text_characters = 0;
    glEnable(GL_DEPTH_TEST);
  }

  // circle drawing
  if (gfx->num_circles > 0) {
    glUseProgram(gfx->circle_shader.id);
    glBindVertexArray(gfx->circle_vao);
    glBindBuffer(GL_ARRAY_BUFFER, gfx->circle_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    sizeof(ggf_circle_vertex_t) * gfx->num_circles * 4,
                    (void *)gfx->circle_vertices);
    glDrawElements(GL_TRIANGLES, gfx->circle_vao * 6, GL_UNSIGNED_INT, NULL);
    gfx->num_circles = 0;
  }
}

void ggf_gfx_set_camera(ggf_camera_t *camera) {
  ggf_gfx_t *gfx = ggf_data->gfx;

  ggf_uniform_buffer_set_data(&gfx->camera_ubo, 0, sizeof(mat4),
                              &camera->view_projection[0][0]);
}

internal_func inline f32
ggf_internal_gfx_get_texture_index(ggf_texture_t *texture) {
  ggf_gfx_t *gfx = ggf_data->gfx;

  u32 texture_index = 0;
  if (texture != NULL && texture->id != 0) {
    for (u32 i = 1; i < gfx->bound_texture_count; i++) {
      if (gfx->bound_textures[i] == texture->id) {
        texture_index = i;
        break;
      }
    }
    if (texture_index == 0) {
      if (gfx->bound_texture_count + 1 >= GGF_GFX_MAX_TEXTURE_UNITS) {
        ggf_gfx_flush();
      }
      gfx->bound_textures[gfx->bound_texture_count] = texture->id;
      texture_index = gfx->bound_texture_count;
      gfx->bound_texture_count++;
    }
  }

  return (f32)texture_index;
}

void ggf_gfx_draw_triangle(vec2 a, vec2 b, vec2 c, f32 depth, vec4 color,
                           ggf_texture_t *texture) {
  ggf_gfx_t *gfx = ggf_data->gfx;
  if (gfx->num_triangles + 1 >= GGF_GFX_MAX_NUM_TRIANGLES) {
    GGF_WARN("WARNING: ggf_gfx_draw_triangle - Max num triangles surpassed. "
             "Flusing graphics buffers which may cause artifacts. IMPORTANT! - "
             "INCREASE [GGF_GFX_MAX_NUM_TRIANGLES]");
    ggf_gfx_flush();
  }

  f32 texture_index = ggf_internal_gfx_get_texture_index(texture);

  ggf_basic_vertex_t vertices[3] = {
      {{a[0], a[1], depth},
       {color[0], color[1], color[2], color[3]},
       {0.0f, 0.0f, texture_index}},
      {{b[0], b[1], depth},
       {color[0], color[1], color[2], color[3]},
       {0.5f, 1.0f, texture_index}},
      {{c[0], c[1], depth},
       {color[0], color[1], color[2], color[3]},
       {1.0f, 0.0f, texture_index}},
  };

  ggf_memory_copy(&gfx->triangle_vertices[gfx->num_triangles * 3], vertices,
                  sizeof(ggf_basic_vertex_t) * 3);

  gfx->num_triangles++;
}

void ggf_gfx_draw_quad(vec2 tl, vec2 tr, vec2 br, vec2 bl, f32 depth,
                       vec4 color, ggf_texture_t *texture) {
  ggf_gfx_t *gfx = ggf_data->gfx;
  if (gfx->num_quads + 1 >= GGF_GFX_MAX_NUM_QUADS) {
    /*GGF_WARN("WARNING: ggf_gfx_draw_quad - Max num quads surpassed. Flusing "
             "graphics buffers which may cause artifacts. IMPORTANT! - "
             "INCREASE [GGF_GFX_MAX_NUM_QUADS]");*/
    ggf_gfx_flush();
  }

  f32 texture_index = ggf_internal_gfx_get_texture_index(texture);

  ggf_basic_vertex_t vertices[4] = {
      {{tl[0], tl[1], depth},
       {color[0], color[1], color[2], color[3]},
       {0.0f, 1.0f, texture_index}},
      {{tr[0], tr[1], depth},
       {color[0], color[1], color[2], color[3]},
       {1.0f, 1.0f, texture_index}},
      {{br[0], br[1], depth},
       {color[0], color[1], color[2], color[3]},
       {1.0f, 0.0f, texture_index}},
      {{bl[0], bl[1], depth},
       {color[0], color[1], color[2], color[3]},
       {0.0f, 0.0f, texture_index}},
  };

  ggf_memory_copy(&gfx->quad_vertices[gfx->num_quads * 4], vertices,
                  sizeof(vertices));

  gfx->num_quads++;
}

void ggf_draw_quad_extent(vec2 pos, vec2 size, f32 depth, vec4 color,
                          ggf_texture_t *texture) {
  ggf_gfx_draw_quad(pos, (vec2){pos[0] + size[0], pos[1]},
                    (vec2){pos[0] + size[0], pos[1] + size[1]},
                    (vec2){pos[0], pos[1] + size[1]}, depth, color, texture);
}

void ggf_gfx_draw_line(vec2 p1, vec2 p2, f32 depth, f32 width, vec4 color,
                       ggf_texture_t *texture) {
  ggf_gfx_t *gfx = ggf_data->gfx;

  vec3 along = {p2[0] - p1[0], p2[1] - p1[1], 0.0f};
  glm_vec3_cross(along, (vec3){0.0f, 0.0f, 1.0f}, along);
  glm_vec3_normalize(along);
  ggf_gfx_draw_quad((vec2){p1[0] + along[0] * width, p1[1] + along[1] * width},
                    (vec2){p1[0] - along[0] * width, p1[1] - along[1] * width},
                    (vec2){p2[0] - along[0] * width, p2[1] - along[1] * width},
                    (vec2){p2[0] + along[0] * width, p2[1] + along[1] * width},
                    depth, color, texture);
}

void ggf_gfx_draw_point(vec2 point, f32 depth, f32 size, vec4 color,
                        ggf_texture_t *texture) {
  ggf_gfx_draw_quad((vec2){point[0] - size, point[1] - size},
                    (vec2){point[0] + size, point[1] - size},
                    (vec2){point[0] + size, point[1] + size},
                    (vec2){point[0] - size, point[1] + size}, depth, color,
                    texture);
}

void ggf_gfx_draw_circle(vec2 center, f32 depth, f32 radius, vec4 color,
                         ggf_texture_t *texture) {
  ggf_gfx_t *gfx = ggf_data->gfx;

  // FIXME: stops working after two circles.

  if (gfx->num_circles + 1 >= GGF_GFX_MAX_NUM_CIRCLES) {
    GGF_WARN("WARNING: ggf_gfx_draw_circle - Max num circles surpassed. "
             "Flusing graphics buffers which may cause artifacts. IMPORTANT! - "
             "INCREASE [GGF_GFX_MAX_NUM_CIRCLES]");
    ggf_gfx_flush();
  }

  f32 texture_index = ggf_internal_gfx_get_texture_index(texture);

  ggf_circle_vertex_t vertices[4] = {
      {{center[0] - radius, center[1] - radius, depth},
       {-1.0f, -1.0f},
       {color[0], color[1], color[2], color[3]},
       {0.0f, 1.0f, texture_index}},
      {{center[0] + radius, center[1] - radius, depth},
       {1.0f, -1.0f},
       {color[0], color[1], color[2], color[3]},
       {1.0f, 1.0f, texture_index}},
      {{center[0] + radius, center[1] + radius, depth},
       {1.0f, 1.0f},
       {color[0], color[1], color[2], color[3]},
       {1.0f, 0.0f, texture_index}},
      {{center[0] - radius, center[1] + radius, depth},
       {-1.0f, 1.0f},
       {color[0], color[1], color[2], color[3]},
       {0.0f, 0.0f, texture_index}},
  };

  ggf_memory_copy(&gfx->circle_vertices[gfx->num_circles * 4], vertices,
                  sizeof(vertices));

  gfx->num_circles++;
}

internal_func void ggf_internal_gfx_utf8_to_unicode(char *str, u32 *out_unicode,
                                                    u32 *out_len) {
  b32 start = true;
  i32 rBytes = 0;
  u32 cp = 0;

  for (char *c = str; *c; ++c) {
    if (rBytes > 0) {
      --rBytes;
      if ((*c & 0xc0) == 0x80)
        cp |= (*c & 0x3f) << (6 * rBytes);
      // else error
    } else if (!(*c & 0x80)) {
      cp = *c;
      rBytes = 0;
    } else if (*c & 0x40) {
      i32 block;
      for (block = 0; ((unsigned char)*c << block) & 0x40 && block < 4; ++block)
        ;
      if (block < 4) {
        cp = (*c & (0x3f >> block)) << (6 * block);
        rBytes = block;
      } else
        continue; // error
    } else
      continue; // error
    if (!rBytes) {
      if (!(start && cp == 0xfeff)) // BOM
      {
        out_unicode[*out_len] = cp;
        *out_len += 1;
      }
      start = false;
    }
  }
}

void ggf_gfx_draw_text(char *text, vec2 pos, u32 size, vec4 color,
                       ggf_font_t *font) {
  ggf_gfx_t *gfx = ggf_data->gfx;

  f32 texture_index = ggf_internal_gfx_get_texture_index(&font->sdf_texture);

  u32 unicode[512];
  u32 len = 0;
  ggf_internal_gfx_utf8_to_unicode(text, unicode, &len);

  f32 screen_px_range = (f32)size / 32.0f * 2.0f;

  f32 x_off = 0.0f;
  f32 y_off = 0.0f;
  for (u32 i = 0; i < len; i++) {
    if (unicode[i] == 10) {
      y_off += 1.171875f * (f32)size;
      x_off = 0.0f;
      continue;
    }
    if (gfx->num_text_characters + 1 >= GGF_GFX_MAX_NUM_TEXT_CHARACTERS) {
      GGF_WARN("WARNING: ggf_gfx_draw_text - Max num text characters "
               "surpassed. Flusing graphics buffers which may cause artifacts. "
               "IMPORTANT! - INCREASE [GGF_GFX_MAX_NUM_TEXT_CHARACTERS]");
      ggf_gfx_flush();
    }

    ggf_hash_map_iter_t iter = ggf_hash_map_find(&font->glyphs, &unicode[i]);
    if (!iter) {
      u32 u = 9744;
      iter = ggf_hash_map_find(&font->glyphs, &u);
      if (!iter)
        continue;
    }
    ggf_glyph_t *glyph =
        (ggf_glyph_t *)ggf_hash_map_value_from_iter(&font->glyphs, iter);
    f32 top = y_off + pos[1] - glyph->plane_bounds.top * (f32)size;
    f32 bottom = y_off + pos[1] - glyph->plane_bounds.bottom * (f32)size;
    f32 left = x_off + pos[0] + glyph->plane_bounds.left * (f32)size;
    f32 right = x_off + pos[0] + glyph->plane_bounds.right * (f32)size;
    x_off += glyph->advance * (f32)size;

    f32 uv_l = (f32)glyph->atlas_bounds.left / font->sdf_texture.width;
    f32 uv_r = (f32)glyph->atlas_bounds.right / font->sdf_texture.width;
    f32 uv_t = (f32)glyph->atlas_bounds.top / font->sdf_texture.height;
    f32 uv_b = (f32)glyph->atlas_bounds.bottom / font->sdf_texture.height;

    ggf_basic_vertex_t vertices[4] = {
        {{left, top, screen_px_range},
         {color[0], color[1], color[2], color[3]},
         {uv_l, uv_t, texture_index}},
        {{right, top, screen_px_range},
         {color[0], color[1], color[2], color[3]},
         {uv_r, uv_t, texture_index}},
        {{right, bottom, screen_px_range},
         {color[0], color[1], color[2], color[3]},
         {uv_r, uv_b, texture_index}},
        {{left, bottom, screen_px_range},
         {color[0], color[1], color[2], color[3]},
         {uv_l, uv_b, texture_index}},
    };
    ggf_memory_copy(&gfx->text_vertices[gfx->num_text_characters * 4], vertices,
                    sizeof(vertices));
    gfx->num_text_characters++;
  }
}

b32 ggf_shader_create(u8 stage_count, ggf_shader_stage_t *stages,
                      const char **source_codes, ggf_shader_t *out_shader) {
  u32 program = glCreateProgram();

  GLenum shader_stages[GGF_SHADER_STAGE_MAX] = {
      GL_VERTEX_SHADER,       GL_FRAGMENT_SHADER,        GL_GEOMETRY_SHADER,
      GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER, GL_COMPUTE_SHADER,
  };

  // Create shaders
  u32 shaders[stage_count];
  for (u32 i = 0; i < stage_count; i++) {
    u32 shader = glCreateShader(shader_stages[stages[i]]);
    glShaderSource(shader, 1, &source_codes[i], NULL);
    glCompileShader(shader);

    i32 compile_status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
    if (compile_status == GL_FALSE) {
      GLint log_length;
      glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
      char *log = (char *)ggf_memory_alloc(log_length, GGF_MEMORY_TAG_GRAPHICS);
      glGetShaderInfoLog(shader, log_length, &log_length, log);
      GGF_WARN("WARNING - ggf_shader_create: Shader compilation failed!\n%s",
               log);
      ggf_memory_free(log);
      glDeleteShader(shader);
      return FALSE;
    }
    glAttachShader(program, shader);
    shaders[i] = shader;
  }

  // Link program
  glLinkProgram(program);

  i32 link_status;
  glGetProgramiv(program, GL_LINK_STATUS, &link_status);
  if (link_status == GL_FALSE) {
    GLint log_length;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
    char *log = (char *)ggf_memory_alloc(log_length, GGF_MEMORY_TAG_GRAPHICS);
    glGetProgramInfoLog(program, log_length, &log_length, log);
    GGF_WARN("WARNING - ggf_shader_create: Shader linking failed!\n%s", log);
    ggf_memory_free(log);
    glDeleteProgram(program);
    return FALSE;
  }

  // delete shaders
  for (u32 i = 0; i < stage_count; i++) {
    glDeleteShader(shaders[i]);
  }

  out_shader->id = program;

  return TRUE;
}

void ggf_shader_destroy(ggf_shader_t *shader) { glDeleteProgram(shader->id); }

void ggf_shader_bind_uniform_buffer(ggf_shader_t *shader, const char *name,
                                    ggf_uniform_buffer_t *buffer) {
  glUseProgram(shader->id);
  u32 location = glGetUniformBlockIndex(shader->id, name);
  glUniformBlockBinding(shader->id, location, buffer->index);
}

global_variable const struct {
  GLenum internal_format;
  GLenum format;
  GLenum type;
} gl_formats[GGF_TEXTURE_FORMAT_MAX] = {
    {GL_ZERO, GL_ZERO, GL_ZERO},
    {GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE},
    {GL_R8, GL_RED, GL_UNSIGNED_BYTE},
    {GL_R8I, GL_RED_INTEGER, GL_BYTE},
    {GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE},
    {GL_R8_SNORM, GL_RED, GL_BYTE},
    {GL_R16, GL_RED, GL_UNSIGNED_SHORT},
    {GL_R16I, GL_RED_INTEGER, GL_SHORT},
    {GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT},
    {GL_R16F, GL_RED, GL_HALF_FLOAT},
    {GL_R16_SNORM, GL_RED, GL_SHORT},
    {GL_R32I, GL_RED_INTEGER, GL_INT},
    {GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT},
    {GL_R32F, GL_RED, GL_FLOAT},
    {GL_RG8, GL_RG, GL_UNSIGNED_BYTE},
    {GL_RG8I, GL_RG_INTEGER, GL_BYTE},
    {GL_RG8UI, GL_RG_INTEGER, GL_UNSIGNED_BYTE},
    {GL_RG8_SNORM, GL_RG, GL_BYTE},
    {GL_RG16, GL_RG, GL_UNSIGNED_SHORT},
    {GL_RG16I, GL_RG_INTEGER, GL_SHORT},
    {GL_RG16UI, GL_RG_INTEGER, GL_UNSIGNED_SHORT},
    {GL_RG16F, GL_RG, GL_HALF_FLOAT},
    {GL_RG16_SNORM, GL_RG, GL_SHORT},
    {GL_RG32I, GL_RG_INTEGER, GL_INT},
    {GL_RG32UI, GL_RG_INTEGER, GL_UNSIGNED_INT},
    {GL_RG32F, GL_RG, GL_FLOAT},
    {GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE},
    {GL_RGB8I, GL_RGB_INTEGER, GL_BYTE},
    {GL_RGB8UI, GL_RGB_INTEGER, GL_UNSIGNED_BYTE},
    {GL_RGB8_SNORM, GL_RGB, GL_BYTE},
    {GL_RGB9_E5, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV},
    {GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE},
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE},
    {GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE},
    {GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE},
    {GL_RGBA8_SNORM, GL_RGBA, GL_BYTE},
    {GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT},
    {GL_RGBA16I, GL_RGBA_INTEGER, GL_SHORT},
    {GL_RGBA16UI, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT},
    {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT},
    {GL_RGBA16_SNORM, GL_RGBA, GL_SHORT},
    {GL_RGBA32I, GL_RGBA_INTEGER, GL_INT},
    {GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT},
    {GL_RGBA32F, GL_RGBA, GL_FLOAT},
    {GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5},
    {GL_RGBA4, GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4_REV},
    {GL_RGB5_A1, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV},
    {GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV},
    {GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV},
};

b32 ggf_texture_create(void *data, ggf_texture_format_t format, u32 width,
                       u32 height, ggf_texture_filter_t filter,
                       ggf_texture_wrap_t wrap, ggf_texture_t *out_texture) {
  GLint gl_min_filter, gl_mag_filter;
  if (filter == GGF_TEXTURE_FILTER_NEAREST) {
    gl_min_filter = GL_NEAREST;
    gl_mag_filter = GL_NEAREST;
  } else if (filter == GGF_TEXTURE_FILTER_LINEAR) {
    gl_min_filter = GL_LINEAR;
    gl_mag_filter = GL_LINEAR;
  } else {
    GGF_ERROR("ERROR - ggf_texture_create: Invalid texture filter!");
    return FALSE;
  }

  GLint gl_wrap;
  switch (wrap) {
  case GGF_TEXTURE_WRAP_REPEAT:
    gl_wrap = GL_REPEAT;
    break;
  case GGF_TEXTURE_WRAP_CLAMP_TO_EDGE:
    gl_wrap = GL_CLAMP_TO_EDGE;
    break;
  case GGF_TEXTURE_WRAP_CLAMP_TO_BORDER:
    gl_wrap = GL_CLAMP_TO_BORDER;
    break;
  case GGF_TEXTURE_WRAP_MIRRORED_REPEAT:
    gl_wrap = GL_MIRRORED_REPEAT;
    break;
  case GGF_TEXTURE_WRAP_MIRRORED_CLAMP_TO_EDGE:
    gl_wrap = GL_MIRROR_CLAMP_TO_EDGE;
    break;
  default: {
    GGF_ERROR("ERROR - ggf_texture_create: Invalid texture wrap!");
    return FALSE;
  }
  }

  glGenTextures(1, &out_texture->id);
  if (out_texture->id == 0) {
    GGF_ERROR("ERROR - ggf_texture_create: Failed to generate texture!");
    return FALSE;
  }
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, out_texture->id);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_min_filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_mag_filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl_wrap);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl_wrap);

  glTexImage2D(GL_TEXTURE_2D, 0, gl_formats[format].internal_format, width,
               height, 0, gl_formats[format].format, gl_formats[format].type,
               data);

  out_texture->width = width;
  out_texture->height = height;
  out_texture->format = format;

  return TRUE;
}

void ggf_texture_destroy(ggf_texture_t *texture) {
  glDeleteTextures(1, &texture->id);
}

b32 ggf_texture_set_data(ggf_texture_t *texture, void *data,
                         ggf_texture_format_t format) {
  if (texture->format != format) {
    GGF_ERROR("ERROR - ggf_texture_set_data: Texture format mismatch!");
    return FALSE;
  }

  glBindTexture(GL_TEXTURE_2D, texture->id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture->width, texture->height,
                  gl_formats[texture->format].format,
                  gl_formats[texture->format].type, data);

  return TRUE;
}

b32 ggf_uniform_buffer_create(u64 size, void *data,
                              ggf_uniform_buffer_t *out_buffer) {
  glGenBuffers(1, &out_buffer->id);
  glBindBuffer(GL_UNIFORM_BUFFER, out_buffer->id);
  glBufferData(GL_UNIFORM_BUFFER, size, data, GL_DYNAMIC_DRAW);

  local_persist u32 index = 0;
  glBindBufferBase(GL_UNIFORM_BUFFER, index, out_buffer->id);
  out_buffer->index = index;
  index++;

  return TRUE;
}

void ggf_uniform_buffer_destroy(ggf_uniform_buffer_t *buffer) {
  glDeleteBuffers(1, &buffer->id);
}

void ggf_uniform_buffer_set_data(ggf_uniform_buffer_t *buffer, u64 offset,
                                 u64 size, void *data) {
  glBindBuffer(GL_UNIFORM_BUFFER, buffer->id);
  glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
}

internal_func b32 ggf_internal_font_u32_cmp(void *first, void *second) {
  return *(u32 *)first == *(u32 *)second;
}

internal_func u64 ggf_internal_font_u32_hash(void *val) {
  return (u64) * (u32 *)val;
}

internal_func char *ggf_internal_font_read_csv_value(char *line, f64 *out_val) {
  char val_str[32];
  u32 i = 0;
  while (*line != ',' && *line != '\n') {
    val_str[i++] = *line;
    line++;
  }
  val_str[i] = 0;
  *out_val = atof(val_str);
  return line + 1;
}

b32 ggf_font_load(const char *sdf_filename, const char *csv_filename,
                  ggf_font_t *out_font) {
  char path[512];

  // load image
  if (!ggf_asset_system_find_full_asset_path(GGF_ASSET_TYPE_FONT, sdf_filename,
                                             sizeof(path), path)) {
    GGF_ERROR("ERROR - ggf_font_load: could not find sdf file '%s'",
              sdf_filename);
    return FALSE;
  }
  i32 sdf_image_w, sdf_image_h, sdf_image_comp;
  stbi_set_flip_vertically_on_load(TRUE);
  stbi_uc *sdf_data =
      stbi_load(path, &sdf_image_w, &sdf_image_h, &sdf_image_comp, 3);
  ggf_texture_create(sdf_data, GGF_TEXTURE_FORMAT_RGB8, sdf_image_w,
                     sdf_image_h, GGF_TEXTURE_FILTER_LINEAR,
                     GGF_TEXTURE_WRAP_REPEAT, &out_font->sdf_texture);
  stbi_image_free(sdf_data);

  // load glpyhs from csv
  if (!ggf_asset_system_find_full_asset_path(GGF_ASSET_TYPE_FONT, csv_filename,
                                             sizeof(path), path)) {
    GGF_ERROR("ERROR - ggf_font_load: could not find csv file '%s'",
              csv_filename);
    return FALSE;
  }
  GGF_HM_CREATE(u32, ggf_glyph_t, 0, ggf_internal_font_u32_cmp,
                ggf_internal_font_u32_hash, out_font->glyphs);

  ggf_file_handle_t file = ggf_file_open(path, GGF_FILE_MODE_READ);

  u64 line_len = 0;
  char *line = ggf_memory_alloc(512, GGF_MEMORY_TAG_STRING);
  while (ggf_file_read_line(file, 512, &line, &line_len)) {
    char *val = line;

    f64 unicode_f;
    val = ggf_internal_font_read_csv_value(val, &unicode_f);

    ggf_glyph_t glyph;
    // advance
    val = ggf_internal_font_read_csv_value(val, &glyph.advance);
    // plane bounds
    val = ggf_internal_font_read_csv_value(val, &glyph.plane_bounds.left);
    val = ggf_internal_font_read_csv_value(val, &glyph.plane_bounds.bottom);
    val = ggf_internal_font_read_csv_value(val, &glyph.plane_bounds.right);
    val = ggf_internal_font_read_csv_value(val, &glyph.plane_bounds.top);
    // atlas bounds
    val = ggf_internal_font_read_csv_value(val, &glyph.atlas_bounds.left);
    val = ggf_internal_font_read_csv_value(val, &glyph.atlas_bounds.bottom);
    val = ggf_internal_font_read_csv_value(val, &glyph.atlas_bounds.right);
    val = ggf_internal_font_read_csv_value(val, &glyph.atlas_bounds.top);

    u32 unicode = (u32)unicode_f;
    ggf_hash_map_insert(&out_font->glyphs, &unicode, &glyph);
  }
  ggf_memory_free(line);

  ggf_file_close(file);

  return TRUE;
}

void ggf_font_destroy(ggf_font_t *font) {
  GGF_HM_DESTROY(font->glyphs);
  ggf_texture_destroy(&font->sdf_texture);
}

f32 ggf_font_get_text_width(ggf_font_t *font, char *text, u32 size) {
  f32 width = 0.0f;
  for (char *c = text; *c != 0; c++) {
    u32 key = *c;
    ggf_glyph_t *glyph = (ggf_glyph_t *)ggf_hash_map_value_from_iter(
        &font->glyphs, ggf_hash_map_find(&font->glyphs, &key));
    width += glyph->advance * (f32)size;
  }
  return width;
}
