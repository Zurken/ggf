#pragma once

#include <stdlib.h>
#include <cglm/cglm.h>

// defines

#define GGF_PI 3.14159265358979f
#define GGF_PI2 6.28318530717958f

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed char i8;
typedef signed short i16;
typedef signed int i32;
typedef signed long long i64;
typedef float f32;
typedef double f64;
typedef u32 b32;
typedef u8 b8;

#define TRUE 1
#define FALSE 0

#define GGF_INVALID_ID 0xFFFFFFFFU
#define GGF_INVALID_ID64 0xFFFFFFFFFFFFFFFFU

#define global_variable static
#define local_persist static
#define internal_func static

#define GGF_ARRAY_COUNT(a) (sizeof(a) / sizeof(a[0]))

#define GGF_MIN(a, b) ((a) < (b) ? (a) : (b))
#define GGF_MAX(a, b) ((a) > (b) ? (a) : (b))
#define GGF_LERP(a, b, f) (a + f * (b - a))
#define GGF_KILOBYTES(number) ((number)*1024ull)
#define GGF_MEGABYTES(number) (GGF_KILOBYTES(number) * 1024ull)
#define GGF_GIGABYTES(number) (GGF_MEGABYTES(number) * 1024ull)
#define GGF_TERABYTES(number) (GGF_GIGABYTES(number) * 1024ull)

#ifdef GGF_ENABLE_ASSERTIONS
#define GGF_ASSERT(expression)                                                           \
    {                                                                                    \
        if (!(expression))                                                               \
        {                                                                                \
            GGF_FATAL("%s:%d ASSERTION FAILED - '%s'", __FILE__, __LINE__, #expression); \
            __builtin_trap();                                                            \
        }                                                                                \
    }
#define GGF_ASSERT_MSG(expression, message) GGF_ASSERT(expression)
#else
#define GGF_ASSERT(expression)
#define GGF_ASSERT_MSG(expression, message)
#endif

#define GGF_INVALID_CODE_PATH GGF_ASSERT_MSG(FALSE, "Invalid code path")

#define GGF_INFO(message, ...) ggf_platform_console_write(GGF_PLATFORM_CONSOLE_COLOR_GREEN, message, ##__VA_ARGS__)
#define GGF_WARN(message, ...) ggf_platform_console_write(GGF_PLATFORM_CONSOLE_COLOR_YELLOW, message, ##__VA_ARGS__)
#define GGF_DEBUG(message, ...) ggf_platform_console_write(GGF_PLATFORM_CONSOLE_COLOR_BLUE, message, ##__VA_ARGS__)
#define GGF_ERROR(message, ...) ggf_platform_console_write_error(GGF_PLATFORM_CONSOLE_COLOR_RED, message, ##__VA_ARGS__)
#define GGF_FATAL(message, ...) ggf_platform_console_write_error(GGF_PLATFORM_CONSOLE_COLOR_RED + 10, message, ##__VA_ARGS__)

// GGF - Great game framework

// initialize ggf and layers - returns TRUE if successful
b32 ggf_init(i32 argc, char **argv);
// shutdown ggf
void ggf_shutdown();

// snap
f32 ggf_snapf(f32 value, f32 step_size);
f64 ggf_snapf64(f64 value, f64 step_size);

// RNG
f32 ggf_randf(u32 seed);    /* ranged 0 to 1*/
f64 ggf_randf64(u32 seed);  /* ranged 0 to 1*/
f32 ggf_randnf(u32 seed);   /* ranged -1 to 1*/
f64 ggf_randnf64(u32 seed); /* ranged -1 to 1*/
u32 ggf_randi(u32 seed);    /* integer version */

// hash
u64 ggf_hash_string(const char *str);

// PLATFORM LAYER

void *ggf_platform_mem_alloc(u64 size);
void ggf_platform_mem_free(void *memory);
void *ggf_platform_mem_virtual_alloc(u64 size);
void ggf_platform_mem_virtual_free(void *memory, u64 size);
void ggf_platform_mem_zero(void *memory, u64 size);
void ggf_platform_mem_copy(void *dest, void *source, u64 size);
void ggf_platform_mem_set(void *memory, i32 value, u64 size);

typedef enum
{
    GGF_PLATFORM_CONSOLE_COLOR_GRAY = 0,
    GGF_PLATFORM_CONSOLE_COLOR_RED,
    GGF_PLATFORM_CONSOLE_COLOR_GREEN,
    GGF_PLATFORM_CONSOLE_COLOR_YELLOW,
    GGF_PLATFORM_CONSOLE_COLOR_BLUE,
    GGF_PLATFORM_CONSOLE_COLOR_MAGENTA,
    GGF_PLATFORM_CONSOLE_COLOR_CYAN,
    GGF_PLATFORM_CONSOLE_COLOR_WHITE,
    GGF_PLATFORM_CONSOLE_COLOR_LIGHT_GRAY,
} ggf_platform_console_color_t;

void ggf_platform_console_write(ggf_platform_console_color_t fg_color, const char *message, ...);
void ggf_platform_console_write_error(ggf_platform_console_color_t fg_color, const char *error_message, ...);

// poll for system events
void ggf_poll_events();

// GGF Window
typedef struct
{
    u32 width, height;
    void *internal_handle;
} ggf_window_t;

// create a window - returns a valid pointer if successful
ggf_window_t *ggf_window_create(const char *title, u32 width, u32 height);
// destroy a window
void ggf_window_destroy(ggf_window_t *window);
// switches the context to the provided window
void ggf_window_switch_context(ggf_window_t *window);
// returns TRUE if the provided window is open
b32 ggf_window_is_open(ggf_window_t *window);
// swap window buffers
void ggf_window_swap_buffers(ggf_window_t *window);
// set the window title
void ggf_window_set_title(ggf_window_t *window, const char *title);
// set the window size
void ggf_window_set_size(ggf_window_t *window, u32 width, u32 height);
// set the window position
void ggf_window_set_position(ggf_window_t *window, u32 x, u32 y);
// set the window to be visible
void ggf_window_set_visible(ggf_window_t *window, b32 visible);
// set the window to be resizable
void ggf_window_set_resizable(ggf_window_t *window, b32 resizable);
// set the window to be fullscreen
void ggf_window_set_fullscreen(ggf_window_t *window, b32 fullscreen);
// set the window to be always on top
void ggf_window_set_always_on_top(ggf_window_t *window, b32 always_on_top);

// filesystem

typedef void *ggf_file_handle_t;

typedef enum
{
    GGF_FILE_MODE_NONE = 0, // INVALID
    GGF_FILE_MODE_READ = 1 << 0,
    GGF_FILE_MODE_WRITE = 1 << 1,
    GGF_FILE_MODE_BINARY = 1 << 2,
} ggf_filemode_flags_t;

// check if the provided path exists
b32 ggf_filesystem_file_exists(const char *file_path);
// open a file at the provided path - returns TRUE on success.
ggf_file_handle_t ggf_file_open(const char *path, ggf_filemode_flags_t mode_flags);
// close a file
void ggf_file_close(ggf_file_handle_t handle);
// get the size of a file in bytes - returns TRUE on success.
u64 ggf_file_get_size(ggf_file_handle_t handle);
// read line from file - returns TRUE on success.
b32 ggf_file_read_line(ggf_file_handle_t handle, u64 max_line_length, char **line_buffer, u64 *out_line_length);
// write line to file - returns TRUE on success.
b32 ggf_file_write_line(ggf_file_handle_t handle, const char *line);
// read from file into buffer - returns TRUE on success.
b32 ggf_file_read(ggf_file_handle_t handle, u64 data_size, void *out_data, u64 *out_bytes_read);
// write to file
b32 ggf_file_write(ggf_file_handle_t handle, u64 data_size, void *data, u64 *out_bytes_written);

// INPUT layer

typedef enum
{
    GGF_MOUSE_BUTTON_LEFT = 0,
    GGF_MOUSE_BUTTON_RIGHT = 1,
    GGF_MOUSE_BUTTON_MIDDLE = 2,

    GGF_MOUSE_BUTTON_MAX,
} ggf_mouse_button_t;

#define GGF_DEF_KEY(name, code) GGF_KEY_##name = code
typedef enum
{
    GGF_DEF_KEY(SPACE, 32),
    GGF_DEF_KEY(APOSTROPHE, 39),
    GGF_DEF_KEY(COMMA, 44),
    GGF_DEF_KEY(MINUS, 45),
    GGF_DEF_KEY(PERIOD, 46),
    GGF_DEF_KEY(SLASH, 47),
    GGF_DEF_KEY(0, 48),
    GGF_DEF_KEY(1, 49),
    GGF_DEF_KEY(2, 50),
    GGF_DEF_KEY(3, 51),
    GGF_DEF_KEY(4, 52),
    GGF_DEF_KEY(5, 53),
    GGF_DEF_KEY(6, 54),
    GGF_DEF_KEY(7, 55),
    GGF_DEF_KEY(8, 56),
    GGF_DEF_KEY(9, 57),
    GGF_DEF_KEY(SEMICOLON, 59),
    GGF_DEF_KEY(EQUAL, 61),
    GGF_DEF_KEY(A, 65),
    GGF_DEF_KEY(B, 66),
    GGF_DEF_KEY(C, 67),
    GGF_DEF_KEY(D, 68),
    GGF_DEF_KEY(E, 69),
    GGF_DEF_KEY(F, 70),
    GGF_DEF_KEY(G, 71),
    GGF_DEF_KEY(H, 72),
    GGF_DEF_KEY(I, 73),
    GGF_DEF_KEY(J, 74),
    GGF_DEF_KEY(K, 75),
    GGF_DEF_KEY(L, 76),
    GGF_DEF_KEY(M, 77),
    GGF_DEF_KEY(N, 78),
    GGF_DEF_KEY(O, 79),
    GGF_DEF_KEY(P, 80),
    GGF_DEF_KEY(Q, 81),
    GGF_DEF_KEY(R, 82),
    GGF_DEF_KEY(S, 83),
    GGF_DEF_KEY(T, 84),
    GGF_DEF_KEY(U, 85),
    GGF_DEF_KEY(V, 86),
    GGF_DEF_KEY(W, 87),
    GGF_DEF_KEY(X, 88),
    GGF_DEF_KEY(Y, 89),
    GGF_DEF_KEY(Z, 90),
    GGF_DEF_KEY(LEFT_BRACKET, 91),
    GGF_DEF_KEY(BACKSLASH, 92),
    GGF_DEF_KEY(RIGHT_BRACKET, 93),
    GGF_DEF_KEY(GRAVE_ACCENT, 96),
    GGF_DEF_KEY(WORLD_1, 161),
    GGF_DEF_KEY(WORLD_2, 162),
    GGF_DEF_KEY(ESCAPE, 256),
    GGF_DEF_KEY(ENTER, 257),
    GGF_DEF_KEY(TAB, 258),
    GGF_DEF_KEY(BACKSPACE, 259),
    GGF_DEF_KEY(INSERT, 260),
    GGF_DEF_KEY(DELETE, 261),
    GGF_DEF_KEY(RIGHT, 262),
    GGF_DEF_KEY(LEFT, 263),
    GGF_DEF_KEY(DOWN, 264),
    GGF_DEF_KEY(UP, 265),
    GGF_DEF_KEY(PAGE_UP, 266),
    GGF_DEF_KEY(PAGE_DOWN, 267),
    GGF_DEF_KEY(HOME, 268),
    GGF_DEF_KEY(END, 269),
    GGF_DEF_KEY(CAPS_LOCK, 280),
    GGF_DEF_KEY(SCROLL_LOCK, 281),
    GGF_DEF_KEY(NUM_LOCK, 282),
    GGF_DEF_KEY(PRINT_SCREEN, 283),
    GGF_DEF_KEY(PAUSE, 284),
    GGF_DEF_KEY(F1, 290),
    GGF_DEF_KEY(F2, 291),
    GGF_DEF_KEY(F3, 292),
    GGF_DEF_KEY(F4, 293),
    GGF_DEF_KEY(F5, 294),
    GGF_DEF_KEY(F6, 295),
    GGF_DEF_KEY(F7, 296),
    GGF_DEF_KEY(F8, 297),
    GGF_DEF_KEY(F9, 298),
    GGF_DEF_KEY(F10, 299),
    GGF_DEF_KEY(F11, 300),
    GGF_DEF_KEY(F12, 301),
    GGF_DEF_KEY(F13, 302),
    GGF_DEF_KEY(F14, 303),
    GGF_DEF_KEY(F15, 304),
    GGF_DEF_KEY(F16, 305),
    GGF_DEF_KEY(F17, 306),
    GGF_DEF_KEY(F18, 307),
    GGF_DEF_KEY(F19, 308),
    GGF_DEF_KEY(F20, 309),
    GGF_DEF_KEY(F21, 310),
    GGF_DEF_KEY(F22, 311),
    GGF_DEF_KEY(F23, 312),
    GGF_DEF_KEY(F24, 313),
    GGF_DEF_KEY(F25, 314),
    GGF_DEF_KEY(KP_0, 320),
    GGF_DEF_KEY(KP_1, 321),
    GGF_DEF_KEY(KP_2, 322),
    GGF_DEF_KEY(KP_3, 323),
    GGF_DEF_KEY(KP_4, 324),
    GGF_DEF_KEY(KP_5, 325),
    GGF_DEF_KEY(KP_6, 326),
    GGF_DEF_KEY(KP_7, 327),
    GGF_DEF_KEY(KP_8, 328),
    GGF_DEF_KEY(KP_9, 329),
    GGF_DEF_KEY(KP_DECIMAL, 330),
    GGF_DEF_KEY(KP_DIVIDE, 331),
    GGF_DEF_KEY(KP_MULTIPLY, 332),
    GGF_DEF_KEY(KP_SUBTRACT, 333),
    GGF_DEF_KEY(KP_ADD, 334),
    GGF_DEF_KEY(KP_ENTER, 335),
    GGF_DEF_KEY(KP_EQUAL, 336),
    GGF_DEF_KEY(LEFT_SHIFT, 340),
    GGF_DEF_KEY(LEFT_CONTROL, 341),
    GGF_DEF_KEY(LEFT_ALT, 342),
    GGF_DEF_KEY(LEFT_SUPER, 343),
    GGF_DEF_KEY(RIGHT_SHIFT, 344),
    GGF_DEF_KEY(RIGHT_CONTROL, 345),
    GGF_DEF_KEY(RIGHT_ALT, 346),
    GGF_DEF_KEY(RIGHT_SUPER, 347),
    GGF_DEF_KEY(MENU, 348),

    GGF_KEY_MAX,
} ggf_key_t;

// returns TRUE if the provided key is held down. key is a GGF_KEY_* constant
b32 ggf_input_key_down(ggf_key_t key);
// returns TRUE if the provided key was just pressed. key is a GGF_KEY_* constant
b32 ggf_input_key_pressed(ggf_key_t key);
// returns TRUE if the provided key was just released. key is a GGF_KEY_* constant
b32 ggf_input_key_released(ggf_key_t key);

// returns TRUE if the provided mouse button is held down. button is a GGF_MOUSE_BUTTON* constant
b32 ggf_input_mouse_down(ggf_mouse_button_t button);
// returns TRUE if the provided mouse button was just pressed. button is a GGF_MOUSE_BUTTON_* constant
b32 ggf_input_mouse_pressed(ggf_mouse_button_t button);
// returns TRUE if the provided mouse button was just released. button is a GGF_MOUSE_BUTTON_* constant
b32 ggf_input_mouse_released(ggf_mouse_button_t button);
// get mouse position
void ggf_input_get_mouse_position(i32 *x, i32 *y);
// get mouse delta
void ggf_input_get_mouse_delta(i32 *x, i32 *y);
// get mouse scroll wheel delta
void ggf_input_get_mouse_wheel_delta(i32 *x, i32 *y);

// MEMORY system

typedef enum
{
    GGF_MEMORY_TAG_UNKNOWN,
    GGF_MEMORY_TAG_WINDOW,
    GGF_MEMORY_TAG_LINEAR_ALLOCATOR,
    GGF_MEMORY_TAG_GRAPHICS,
    GGF_MEMORY_TAG_INPUT,
    GGF_MEMORY_TAG_STRING,
    GGF_MEMORY_TAG_ASSET,
    GGF_MEMORY_TAG_GAME,
    GGF_MEMORY_TAG_HASH_MAP,
    GGF_MEMORY_TAG_IMAGE,
    GGF_MEMORY_TAG_DARRAY,

    GGF_MEMORY_TAG_MAX,
} ggf_memory_tag_t;

void *ggf_memory_alloc(u64 size, ggf_memory_tag_t memory_tag);
void *ggf_memory_realloc(void *memory, u64 new_size, ggf_memory_tag_t memory_tag);
void ggf_memory_free(void *memory);
void ggf_memory_zero(void *memory, u64 size);
void ggf_memory_copy(void *dest, void *source, u64 size);
void ggf_memory_set(void *memory, i32 value, u64 size);

// returns the size of the allocation at the provided memory address.
u64 ggf_memory_get_alloc_size(void *memory);
void ggf_memory_get_usage_string(char *buffer, u64 buffer_size);
u64 ggf_memory_get_alloc_count();

// linear allocator
typedef struct
{
    void *memory;
    u64 size;
    u64 marker;
    b32 owns_memory;
} ggf_linear_allocator_t;

b32 ggf_linear_allocator_create(u64 size, void *memory, ggf_linear_allocator_t *out_allocator);
void ggf_linear_allocator_destroy(ggf_linear_allocator_t *allocator);
void *ggf_linear_allocator_alloc(ggf_linear_allocator_t *allocator, u64 size);
void ggf_linear_allocator_reset(ggf_linear_allocator_t *allocator);
void *ggf_linear_allocator_get_memory_at_marker(ggf_linear_allocator_t *allocator, u64 marker);

// dynamic allocator
typedef struct
{
    void *internal_memory;
} ggf_dynamic_allocator_t;

b32 ggf_dynamic_allocator_create(u64 total_size, u64 *memory_requirement, void *memory, ggf_dynamic_allocator_t *out_allocator);
void ggf_dynamic_allocator_destroy(ggf_dynamic_allocator_t *allocator);
void *ggf_dynamic_allocator_alloc(ggf_dynamic_allocator_t *allocator, u64 size);
b32 ggf_dynamic_allocator_free(ggf_dynamic_allocator_t *allocator, void *memory, u64 size);
u64 ggf_dynamic_allocator_get_free_space(ggf_dynamic_allocator_t *allocator);

// CONTAINERS

// dynamic array

void *ggf_darray_create(u64 length, u64 stride);
void ggf_darray_destroy(void *array);
void ggf_darray_clear(void *array);
u64 ggf_darray_get_capacity(void *array);
u64 ggf_darray_get_length(void *array);
u64 ggf_darray_get_stride(void *array);
void *ggf_darray_resize(void *array);
void *ggf_darray_push(void *array, void *value_ptr);
void ggf_darray_pop(void *array, void *dest);
void *ggf_darray_pop_at(void *array, u64 index, void *dest);
void *ggf_darray_insert_at(void *array, u64 index, void *value_ptr);

// freelist
typedef struct
{
    void *internal_memory;
} ggf_freelist_t;

// creates a freelist. returns TRUE if the operation was successful.
b32 ggf_freelist_create(u64 size, u64 *memory_requirement, void *memory, ggf_freelist_t *out_freelist);
void ggf_freelist_destroy(ggf_freelist_t *freelist);
b32 ggf_freelist_allocate_block(ggf_freelist_t *freelist, u64 size, u64 *out_offset);
b32 ggf_freelist_free_block(ggf_freelist_t *freelist, u64 size, u64 offset);
void ggf_freelist_clear(ggf_freelist_t *freelist);
u64 ggf_freelist_get_free_space(ggf_freelist_t *freelist);

// hash map
typedef b32 (*ggf_hash_map_key_comp_func_t)(void *, void *);
typedef u64 (*ggf_hash_map_hash_func_t)(void *);

typedef struct
{
    void *memory;
    u64 key_size;
    u64 value_size;
    void *empty_key;
    ggf_hash_map_key_comp_func_t key_cmp_func;
    ggf_hash_map_hash_func_t hash_func;
    u64 buckets_count;
    u64 buckets_reserved_count;
    u8 *buckets;
} ggf_hash_map_t;

typedef u8 *ggf_hash_map_iter_t;

static inline ggf_hash_map_iter_t ggf_hash_map_begin(ggf_hash_map_t *map) { return map->buckets; }
static inline ggf_hash_map_iter_t ggf_hash_map_end(ggf_hash_map_t *map) { return map->buckets + map->buckets_reserved_count * (map->key_size + map->value_size); }
ggf_hash_map_iter_t ggf_hash_map_next(ggf_hash_map_t *map, ggf_hash_map_iter_t current);
static inline void *ggf_hash_map_key_from_iter(ggf_hash_map_t *map, ggf_hash_map_iter_t iter) { return (void *)iter; }
static inline void *ggf_hash_map_value_from_iter(ggf_hash_map_t *map, ggf_hash_map_iter_t iter) { return (void *)(iter + map->key_size); }

void ggf_hash_map_create(u64 bucket_count, u64 key_size, u64 value_size, void *empty_key,
                         ggf_hash_map_key_comp_func_t key_cmp_func, ggf_hash_map_hash_func_t hash_func, void *memory, u64 *memory_requirement, ggf_hash_map_t *out_map);
void ggf_hash_map_destroy(ggf_hash_map_t *map);
void ggf_hash_map_clear(ggf_hash_map_t *map);
void ggf_hash_map_reserve(ggf_hash_map_t *map, u64 count);
ggf_hash_map_iter_t ggf_hash_map_find(ggf_hash_map_t *map, void *key);
ggf_hash_map_iter_t ggf_hash_map_insert(ggf_hash_map_t *map, void *key, void *value);
void ggf_hash_map_erase(ggf_hash_map_t *map, ggf_hash_map_iter_t it);

#define GGF_HM_CREATE(key_type, value_type, empty_key, key_cmp_func, hash_func, out_map)                                             \
    {                                                                                                                                \
        key_type ek = empty_key;                                                                                                     \
        u64 requirement = 0;                                                                                                         \
        ggf_hash_map_create(512, sizeof(key_type), sizeof(value_type), 0, 0, 0, 0, &requirement, 0);                                 \
        void *mem = ggf_memory_alloc(requirement, GGF_MEMORY_TAG_HASH_MAP);                                                          \
        ggf_hash_map_create(512, sizeof(key_type), sizeof(value_type), &ek, &key_cmp_func, &hash_func, mem, &requirement, &out_map); \
    }

#define GGF_HM_DESTROY(map) ggf_hash_map_destroy(&map)

#define GGF_HM_INSERT(map, key, value)     \
    {                                      \
        __typeof(key) k = key;             \
        __typeof(value) v = value;         \
        ggf_hash_map_insert(&map, &k, &v); \
    }

#define GGF_HM_GET(map, key, out)                                                               \
    {                                                                                           \
        __typeof(key) k = key;                                                                  \
        out = (__typeof(out) *)ggf_hash_map_value_from_iter(&map, ggf_hash_map_find(&map, &k)); \
    }

// Asset Layer

/*
    STAGE:
        a part of your program that includes certain assets.
        when creating one you provide the name of all the assets and their associated filepaths.
*/

typedef u32 ggf_asset_handle_t;
typedef u32 ggf_asset_stage_index_t;

typedef enum
{
    GGF_ASSET_TYPE_UNKNOWN = 0,
    GGF_ASSET_TYPE_AUDIO,
    GGF_ASSET_TYPE_TEXTURE,
    GGF_ASSET_TYPE_SHADER,
    GGF_ASSET_TYPE_FONT,
    GGF_ASSET_TYPE_MAX,
} ggf_asset_type_t;

typedef struct
{
    ggf_asset_type_t type;
    const char *name;
    const char *path;
} ggf_asset_description_t;

typedef struct
{
    u32 width, height;
    u32 comp_count;
} ggf_texture_asset_data_t;

ggf_asset_stage_index_t ggf_asset_stage_create(u64 desc_count, ggf_asset_description_t *descriptions);
void ggf_asset_stage_use(ggf_asset_stage_index_t stage);
b32 ggf_asset_stage_is_loaded(ggf_asset_stage_index_t stage);
b32 ggf_asset_system_find_full_asset_path(ggf_asset_type_t type, const char *filename, u64 out_buffer_len, char *out_buffer);
ggf_asset_handle_t ggf_asset_get_handle(const char *name);
void *ggf_asset_get_data(ggf_asset_handle_t handle);

// GRAPHICS layer

// shader stages
typedef enum
{
    GGF_SHADER_STAGE_VERTEX,
    GGF_SHADER_STAGE_FRAGMENT,
    GGF_SHADER_STAGE_GEOMETRY,
    GGF_SHADER_STAGE_TESSELATION_CONTROL,
    GGF_SHADER_STAGE_TESSELATION_EVALUATION,
    GGF_SHADER_STAGE_COMPUTE,
    GGF_SHADER_STAGE_MAX,
} ggf_shader_stage_t;

// shader
typedef struct
{
    u32 id;
} ggf_shader_t;

typedef struct
{
    mat4 view_projection;
} ggf_camera_t;

typedef enum
{
    GGF_TEXTURE_FILTER_NEAREST,
    GGF_TEXTURE_FILTER_LINEAR,
    GGF_TEXTURE_FILTER_MAX,
} ggf_texture_filter_t;

typedef enum
{
    GGF_TEXTURE_WRAP_REPEAT,
    GGF_TEXTURE_WRAP_CLAMP_TO_EDGE,
    GGF_TEXTURE_WRAP_CLAMP_TO_BORDER,
    GGF_TEXTURE_WRAP_MIRRORED_REPEAT,
    GGF_TEXTURE_WRAP_MIRRORED_CLAMP_TO_EDGE,
    GGF_TEXTURE_WRAP_MAX,
} ggf_texture_wrap_t;

typedef enum
{
    GGF_TEXTURE_FORMAT_R1,
    GGF_TEXTURE_FORMAT_A8,
    GGF_TEXTURE_FORMAT_R8,
    GGF_TEXTURE_FORMAT_R8I,
    GGF_TEXTURE_FORMAT_R8U,
    GGF_TEXTURE_FORMAT_R8S,
    GGF_TEXTURE_FORMAT_R16,
    GGF_TEXTURE_FORMAT_R16I,
    GGF_TEXTURE_FORMAT_R16U,
    GGF_TEXTURE_FORMAT_R16F,
    GGF_TEXTURE_FORMAT_R16S,
    GGF_TEXTURE_FORMAT_R32I,
    GGF_TEXTURE_FORMAT_R32U,
    GGF_TEXTURE_FORMAT_R32F,
    GGF_TEXTURE_FORMAT_RG8,
    GGF_TEXTURE_FORMAT_RG8I,
    GGF_TEXTURE_FORMAT_RG8U,
    GGF_TEXTURE_FORMAT_RG8S,
    GGF_TEXTURE_FORMAT_RG16,
    GGF_TEXTURE_FORMAT_RG16I,
    GGF_TEXTURE_FORMAT_RG16U,
    GGF_TEXTURE_FORMAT_RG16F,
    GGF_TEXTURE_FORMAT_RG16S,
    GGF_TEXTURE_FORMAT_RG32I,
    GGF_TEXTURE_FORMAT_RG32U,
    GGF_TEXTURE_FORMAT_RG32F,
    GGF_TEXTURE_FORMAT_RGB8,
    GGF_TEXTURE_FORMAT_RGB8I,
    GGF_TEXTURE_FORMAT_RGB8U,
    GGF_TEXTURE_FORMAT_RGB8S,
    GGF_TEXTURE_FORMAT_RGB9E5F,
    GGF_TEXTURE_FORMAT_BGRA8,
    GGF_TEXTURE_FORMAT_RGBA8,
    GGF_TEXTURE_FORMAT_RGBA8I,
    GGF_TEXTURE_FORMAT_RGBA8U,
    GGF_TEXTURE_FORMAT_RGBA8S,
    GGF_TEXTURE_FORMAT_RGBA16,
    GGF_TEXTURE_FORMAT_RGBA16I,
    GGF_TEXTURE_FORMAT_RGBA16U,
    GGF_TEXTURE_FORMAT_RGBA16F,
    GGF_TEXTURE_FORMAT_RGBA16S,
    GGF_TEXTURE_FORMAT_RGBA32I,
    GGF_TEXTURE_FORMAT_RGBA32U,
    GGF_TEXTURE_FORMAT_RGBA32F,
    GGF_TEXTURE_FORMAT_R5G6B5,
    GGF_TEXTURE_FORMAT_RGBA4,
    GGF_TEXTURE_FORMAT_RGB5A1,
    GGF_TEXTURE_FORMAT_RGB10A2,
    GGF_TEXTURE_FORMAT_RG11B10F,

    GGF_TEXTURE_FORMAT_MAX,

} ggf_texture_format_t;

typedef struct
{
    u32 id;
    u32 width, height;
    ggf_texture_format_t format;
} ggf_texture_t;

typedef struct
{
    u32 id;
    u32 index;
} ggf_uniform_buffer_t;

typedef struct
{
    f64 left, bottom, right, top;
} ggf_glyph_bounds_t;

typedef struct
{
    f64 advance;
    ggf_glyph_bounds_t plane_bounds;
    ggf_glyph_bounds_t atlas_bounds;
} ggf_glyph_t;

typedef struct
{
    ggf_texture_t sdf_texture;
    ggf_hash_map_t glyphs;
} ggf_font_t;

// TODO: render targets
// TODO: custom shaders
// TODO: Improve FONTS API - make glyphs dynamic array instead of hash map

// initialize graphics, assumes a valid OpenGL context is in use - returns TRUE if successful
b32 ggf_gfx_init(u32 width, u32 height);
// shutdown graphics
void ggf_gfx_shutdown();

// set clear color
void ggf_gfx_set_clear_color(vec3 color);

// begin new frame. clears the screen.
void ggf_gfx_begin_frame();
// flush frame
void ggf_gfx_flush();

// use a custom shader
void ggf_gfx_set_shader(ggf_shader_t *shader);

// set camera
void ggf_gfx_set_camera(ggf_camera_t *camera);

// draw a triangle
void ggf_gfx_draw_triangle(vec2 p1, vec2 p2, vec2 p3, f32 depth, vec4 color, ggf_texture_t *texture);
// draw a quad
void ggf_gfx_draw_quad(vec2 tl, vec2 tr, vec2 br, vec2 bl, f32 depth, vec4 color, ggf_texture_t *texture);
void ggf_draw_quad_extent(vec2 pos, vec2 size, f32 depth, vec4 color,
                                    ggf_texture_t *texture);
// draw a line
void ggf_gfx_draw_line(vec2 p1, vec2 p2, f32 depth, f32 width, vec4 color, ggf_texture_t *texture);
// draw a point
void ggf_gfx_draw_point(vec2 point, f32 depth, f32 size, vec4 color, ggf_texture_t *texture);
// draw a circle
void ggf_gfx_draw_circle(vec2 center, f32 depth, f32 radius, vec4 color, ggf_texture_t *texture);
// draw text. no depth (always on top)
void ggf_gfx_draw_text(char *text, vec2 pos, u32 size, vec4 color, ggf_font_t *font);

// create a shader. returns TRUE if creation was successful.
b32 ggf_shader_create(u8 stage_count, ggf_shader_stage_t *stages, const char **source_codes, ggf_shader_t *out_shader);
// destroy a shader
void ggf_shader_destroy(ggf_shader_t *shader);
// bind uniform buffer
void ggf_shader_bind_uniform_buffer(ggf_shader_t *shader, const char *name, ggf_uniform_buffer_t *buffer);

// create a texture. returns TRUE if creation was successful.
b32 ggf_texture_create(void *data, ggf_texture_format_t format, u32 width, u32 height, ggf_texture_filter_t filter, ggf_texture_wrap_t wrap, ggf_texture_t *out_texture);
// destroy a texture
void ggf_texture_destroy(ggf_texture_t *texture);
// set texture data. returns TRUE if the operation was successful.
b32 ggf_texture_set_data(ggf_texture_t *texture, void *data, ggf_texture_format_t format);

b32 ggf_uniform_buffer_create(u64 size, void *data, ggf_uniform_buffer_t *out_buffer);
void ggf_uniform_buffer_destroy(ggf_uniform_buffer_t *buffer);
void ggf_uniform_buffer_set_data(ggf_uniform_buffer_t *buffer, u64 offset, u64 size, void *data);

b32 ggf_font_load(const char *sdf_filename, const char *csv_filename, ggf_font_t *out_font);
void ggf_font_destroy(ggf_font_t *font);
f32 ggf_font_get_text_width(ggf_font_t *font, char *text, u32 size);
