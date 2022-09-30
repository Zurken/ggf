#include "ggf.c"

global_variable u32 seed = 239847;

enum CARDS {
  CARD_A = 0,
  CARD_2,
  CARD_3,
  CARD_4,
  CARD_5,
  CARD_6,
  CARD_7,
  CARD_8,
  CARD_9,
  CARD_10,
  CARD_J,
  CARD_D,
  CARD_K,
  CARD_COUNT,
};

i32 find_card_index(i32 *deck, b32 want_empty) {
  i32 index = (i32)(ggf_randf(++seed) * 52.0f);
  if ((want_empty && deck[index] != -1) || (!want_empty && deck[index] == -1)) {
    for (u32 k = index; k < 52; ++k) {
      if ((want_empty && deck[k] == -1) || (!want_empty && deck[index] != -1)) {
        return k;
      }
    }
    for (i32 k = index; k >= 0; --k) {
      if ((want_empty && deck[k] == -1) || (!want_empty && deck[index] != -1)) {
        return k;
      }
    }
  }

  return index;
}

void generate_deck(i32 *deck) {
  for (u32 i = 0; i < 52; ++i) {
    deck[i] = -1;
  }

  for (u32 i = 0; i < CARD_COUNT; ++i) {
    for (u32 j = 0; j < 4; ++j) {
      i32 index = find_card_index(deck, TRUE);
      deck[index] = i;
    }
  }
}

i32 get_card(i32 *deck) {
  i32 index = find_card_index(deck, false); 
  i32 card = deck[index];
  deck[index] = -1;
  return card;
}

i32 main(i32 argc, char **argv) {
  ggf_init(argc, argv);
  ggf_window_t *window = ggf_window_create("Blackjack", 1280, 720);
  ggf_window_set_resizable(window, FALSE);

  ggf_gfx_init(1280, 720);

  ggf_font_t font;
  ggf_font_load("test old.png", "test old.csv", &font);

  ggf_camera_t camera = {0};
  glm_ortho(0.0f, 1280.0f, 720.0f, 0.0f, -100.0f, 100.0f,
            camera.view_projection);
  ggf_gfx_set_camera(&camera);

  i32 card_worth[CARD_COUNT] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 10, 10};
  char *card_names[CARD_COUNT] = {
    "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "D", "K",
  };

  i32 deck[52];
  generate_deck(deck);

  i32 *user_cards = ggf_darray_create(4, sizeof(i32));
  i32 card = get_card(deck);
  ggf_darray_push(user_cards, &card);
  card = get_card(deck);
  ggf_darray_push(user_cards, &card);

  while (ggf_window_is_open(window)) {
    ggf_poll_events();

    ggf_gfx_begin_frame();
    ggf_gfx_draw_text("Hejasn", (vec2){200.0f, 200.0f}, 64, (vec4){1.0f, 1.0f, 1.0f, 1.0f}, &font); 

    for (u32 i = 0; i < ggf_darray_get_length(user_cards); ++i) {
      vec2 pos = {200.0f, 300.0f};
      vec2 card_size = {100.0f, 150.0f};
      f32 spacing = 25.0f;

      ggf_draw_quad_extent((vec2){pos[0] + (card_size[0] + spacing) * i, pos[1]},
                          card_size, 1.0f, (vec4){1.0f, 0.1f, 0.1f, 1.0f}, NULL);

      i32 card = user_cards[i];
      char *text = card_names[card];
      u32 text_size = 96;
      f32 text_width = ggf_font_get_text_width(&font, text, text_size);
      vec2 text_pos = {pos[0] + (card_size[0] + spacing) * i + (card_size[0] - text_width) / 2.0f, 
                      pos[1] + card_size[1] / 2.0f + text_size / 3.0f};
      ggf_gfx_draw_text(text, text_pos, text_size,
                        (vec4){1.0f, 1.0f, 1.0f, 1.0f}, &font); 
    }

    ggf_gfx_flush();

    ggf_window_swap_buffers(window);
  }

  ggf_darray_destroy(user_cards);

  ggf_font_destroy(&font);

  ggf_gfx_shutdown();
  ggf_window_destroy(window);
  ggf_shutdown();
}