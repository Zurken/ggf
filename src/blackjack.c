#include "ggf.c"
#include <sys/time.h>
#include <time.h>

#define WIDTH 1280
#define HEIGHT 720

enum {
  CARD_ACE = 0,
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

enum {
  BUTTON_STATE_REST = 0,
  BUTTON_STATE_HOVER,
  BUTTON_STATE_DOWN,
  BUTTON_STATE_RELEASED,
  BUTTON_STATE_COUNT,
};

enum {
  GAME_STATE_BETTING,
  GAME_STATE_DEALING,
  GAME_STATE_PLAYER_TURN,
  GAME_STATE_DEALER_TURN,
  GAME_STATE_RESULT,
};

enum {
  RESULT_NONE,
  RESULT_PUSH,
  RESULT_PLAYER_WIN,
  RESULT_PLAYER_LOSE,
  RESULT_COUNT,
};

typedef struct {
  i32 result;
  i32 *cards;
  u32 worth;
} hand_t;

u32 get_rand(u32 max) {
  local_persist u32 offset = 0;
  struct timespec seed;
  clock_gettime(CLOCK_MONOTONIC, &seed);
  u32 result =
      (u32)(ggf_randf(seed.tv_sec * 5425 + seed.tv_nsec + ++offset) * (f32)max);
  return result;
}

i32 find_empty_card_index(i32 *deck) {
  i32 index = get_rand(52);
  if (deck[index] != -1) {
    for (i32 k = index + 1; k < 52; ++k) {
      if (deck[k] == -1) {
        return k;
      }
      if (k == 51)
        k = -1;
    }
  }

  return index;
}

void shuffle(int *array, size_t n) {
  time_t seed = time(NULL);

  if (n > 1) {
    for (i32 i = n - 1; i > 0; --i) {
      u32 j = get_rand(i);
      i32 t = array[j];
      array[j] = array[i];
      array[i] = t;
    }
  }
}

void generate_deck(i32 *deck) {
  for (u32 i = 0; i < 52; ++i) {
    deck[i] = -1;
  }

  for (u32 i = 0; i < CARD_COUNT; ++i) {
    for (u32 j = 0; j < 4; ++j) {
      i32 index = find_empty_card_index(deck);
      deck[index] = i;
    }
  }
  shuffle(deck, 52);
}

i32 get_card(i32 *deck) {
  i32 index = 51;
  for (; index >= 0; --index) {
    if (deck[index] != -1) {
      break;
    }
  }
  i32 card = deck[index];
  deck[index] = -1;
  return card;
}

b32 point_in_rect(vec2 point, vec2 pos, vec2 extent) {
  b32 result = point[0] >= pos[0] && point[1] >= pos[1] &&
               point[0] <= pos[0] + extent[0] && point[1] <= pos[1] + extent[1];
  return result;
}

void render_cards(i32 *cards, ggf_font_t *font, vec2 center, vec4 color) {
  char *card_names[CARD_COUNT] = {
      "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "D", "K",
  };

  u32 card_count = ggf_darray_get_length(cards);
  vec2 card_size = {150.0f, 225.0f};
  f32 spacing = 15.0f;
  vec2 pos = { 
          center[0] - (card_size[0] * card_count + spacing * (card_count - 1)) / 2.0f,
          center[1] - card_size[1] / 2.0f};
  for (u32 i = 0; i < card_count; ++i) {

    ggf_draw_quad_extent((vec2){pos[0] + (card_size[0] + spacing) * i, pos[1]},
                         card_size, 1.0f, color, NULL);

    i32 card = cards[i];
    char *text = card_names[card];
    u32 text_size = 96;
    f32 text_width = ggf_font_get_text_width(font, text, text_size);
    vec2 text_pos = {pos[0] + (card_size[0] + spacing) * i +
                         (card_size[0] - text_width) / 2.0f,
                     pos[1] + card_size[1] / 2.0f + text_size / 3.0f};
    ggf_gfx_draw_text(text, text_pos, text_size, (vec4){1.0f, 1.0f, 1.0f, 1.0f},
                      font);
  }
}

b32 button(char *text, ggf_font_t *font, vec2 pos, vec2 size, vec2 mouse_pos,
           i32 *out_state) {
  b32 result = FALSE;

  if (point_in_rect(mouse_pos, pos, size)) {
    if (*out_state != BUTTON_STATE_RELEASED) {
      *out_state = BUTTON_STATE_HOVER;
    }
    if (ggf_input_mouse_down(GGF_MOUSE_BUTTON_LEFT)) {
      *out_state = BUTTON_STATE_DOWN;
    }
    if (ggf_input_mouse_released(GGF_MOUSE_BUTTON_LEFT)) {
      *out_state = BUTTON_STATE_RELEASED;
      result = TRUE;
    }
  } else {
    *out_state = BUTTON_STATE_REST;
  }

  vec4 button_colors[BUTTON_STATE_COUNT] = {
      {1.0f, 1.0f, 1.0f, 1.0f}, // Rest
      {0.6f, 0.6f, 0.6f, 1.0f}, // Hover
      {1.0f, 1.0f, 1.0f, 1.0f}, // Down
      {1.0f, 1.0f, 1.0f, 1.0f}, // Released
  };

  ggf_draw_quad_extent(pos, size, 0.0f, button_colors[*out_state], NULL);
  ggf_gfx_draw_text(
      text,
      (vec2){pos[0] +
                 (size[0] - ggf_font_get_text_width(font, text, 52)) / 2.0f,
             pos[1] + size[1] - 32.0f},
      52, (vec4){0.0f, 0.0f, 0.0f, 1.0f}, font);

  return result;
}

u32 calculate_cards_worth(i32 *cards) {
  i32 card_worth[CARD_COUNT] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 10, 10};

  u32 result = 0;

  u32 ace_count = 0;
  u32 card_count = ggf_darray_get_length(cards);
  for (u32 i = 0; i < card_count; ++i) {
    ace_count += (cards[i] == CARD_ACE);
    result += card_worth[cards[i]];
  }
  while (21 - result >= 10 && ace_count > 0) {
    result += 10;
    --ace_count;
  }

  return result;
}

void create_hand(hand_t *hand) {
  hand->cards = ggf_darray_create(4, sizeof(i32));
  hand->worth = 0;
}

void add_card_to_hand(hand_t *hand, i32 card) {
  i32 new_card = card;
  hand->cards = ggf_darray_push(hand->cards, &new_card);
  hand->worth = calculate_cards_worth(hand->cards);
}

i32 main(i32 argc, char **argv) {
  ggf_init(argc, argv);
  ggf_window_t *window = ggf_window_create("Blackjack", WIDTH, HEIGHT);
  ggf_window_set_resizable(window, FALSE);

  ggf_gfx_init(WIDTH, HEIGHT);

  ggf_font_t font;
  ggf_font_load("test old.png", "test old.csv", &font);

  ggf_camera_t camera = {0};
  glm_ortho(0.0f, WIDTH, HEIGHT, 0.0f, -100.0f, 100.0f, camera.view_projection);
  ggf_gfx_set_camera(&camera);

  i32 deck[52];
  generate_deck(deck);

  u32 player_hand_index = 0;
  u32 player_hand_count = 1;
  hand_t player_hands[2] = {};
  create_hand(player_hands + 0);
  create_hand(player_hands + 1);

  hand_t dealer_hand = {};
  create_hand(&dealer_hand);

  i32 stand_button_state = BUTTON_STATE_REST;
  i32 hit_button_state = BUTTON_STATE_REST;

  i32 plus_button_state = BUTTON_STATE_REST;
  i32 minus_button_state = BUTTON_STATE_REST;
  i32 deal_button_state = BUTTON_STATE_REST;
  i32 double_button_state = BUTTON_STATE_REST;
  i32 split_button_state = BUTTON_STATE_REST;

  i32 money = 1000;
  i32 current_bet = 1;

  i32 game_state = GAME_STATE_BETTING;

  f32 dealer_timer = 0.0f;

  f32 dt = 1.0f / 60.0f;
  while (ggf_window_is_open(window)) {
    ggf_poll_events();

    ggf_gfx_set_clear_color((vec3){0.1f, 0.3f, 0.1f});

    ggf_gfx_begin_frame();

    i32 m_x, m_y;
    ggf_input_get_mouse_position(&m_x, &m_y);
    vec2 mouse_pos = {(f32)m_x, (f32)m_y};

    if (game_state == GAME_STATE_BETTING) {
      f32 side_margin = 250.0f;
      vec2 button_size = {100.0f, 100.0f};
      vec2 minus_button_pos = {side_margin, (HEIGHT - button_size[1]) / 2.0f};
      vec2 plus_button_pos = {WIDTH - button_size[0] - side_margin,
                              (HEIGHT - button_size[1]) / 2.0f};

      if (button("-", &font, minus_button_pos, button_size, mouse_pos,
                 &minus_button_state)) {
        u32 amount = 1;
        if (current_bet > 5)
          amount = 5;
        if (current_bet > 50)
          amount = 10;
        if (current_bet > 100)
          amount = 50;
        if (current_bet > 300)
          amount = 100;
        current_bet -= amount;
      }
      if (button("+", &font, plus_button_pos, button_size, mouse_pos,
                 &plus_button_state)) {
        u32 amount = 1;
        if (current_bet >= 5)
          amount = 5;
        if (current_bet >= 50)
          amount = 10;
        if (current_bet >= 100)
          amount = 50;
        if (current_bet >= 300)
          amount = 100;
        current_bet += amount;
      }
      current_bet = (current_bet > money) ? money
                    : (current_bet < 0)   ? 0
                                          : current_bet;

      vec2 deal_button_size = {300.0f, 100.0f};
      vec2 deal_button_pos = {(WIDTH - deal_button_size[0]) / 2.0f,
                              HEIGHT - 250.0f};
      if (current_bet > 0 &&
          (button("DEAL", &font, deal_button_pos, deal_button_size, mouse_pos,
                  &deal_button_state) ||
           ggf_input_key_released(GGF_KEY_SPACE))) {
        game_state = GAME_STATE_DEALING;
      }
    } else if (game_state == GAME_STATE_DEALING) {
      dealer_timer += dt;

      for (u32 i = 0; i < player_hand_count; ++i) {
        hand_t *hand = player_hands + i;
        u32 card_count = ggf_darray_get_length(hand->cards);
        if ((dealer_timer > (i+1) * 0.5f && card_count == 0) || (dealer_timer - 0.5f > (i+1) * 1.0f && card_count == 1)) {
          add_card_to_hand(hand, CARD_9);
        }
      }
      if (dealer_timer > (player_hand_count+1) * 0.5f && ggf_darray_get_length(dealer_hand.cards) == 0) {
        add_card_to_hand(&dealer_hand, get_card(deck));
      }
      if (dealer_timer > player_hand_count * 1.0f + 0.5f) {
        dealer_timer = 0.0f;
        game_state = GAME_STATE_PLAYER_TURN;
      }

    } else if (game_state == GAME_STATE_PLAYER_TURN) {

      hand_t *hand = player_hands + player_hand_index;
      u32 card_count = ggf_darray_get_length(hand->cards);

      if (card_count == 1) {
        dealer_timer += dt;
        if (dealer_timer >= 0.5f) {
          add_card_to_hand(hand, get_card(deck));
          dealer_timer = 0.0f;
        }
      }

      // TODO: Only allow one split per round
      b32 can_split = card_count == 2 && hand->cards[0] == hand->cards[1];

      f32 button_side_margin = 150.0f;
      vec2 button_size = {200.0f, 100.0f};

      vec2 stand_button_pos = {WIDTH - button_side_margin - button_size[0],
                               (HEIGHT - button_size[1]) / 2.0f};
      vec2 hit_button_pos = {button_side_margin,
                             (HEIGHT - button_size[1]) / 2.0f};
      vec2 split_button_pos = {hit_button_pos[0],
                                hit_button_pos[1] + button_size[1] + 5.0f};

      if (button("STAND", &font, stand_button_pos, button_size, mouse_pos,
                 &stand_button_state)) {
        if (player_hand_index == player_hand_count - 1) {
          game_state = GAME_STATE_DEALER_TURN;
          player_hand_index = 0;
        } else {
          ++player_hand_index;
        }
      }

      if (button("HIT", &font, hit_button_pos, button_size, mouse_pos,
                 &hit_button_state)) {
        add_card_to_hand(hand, get_card(deck));
      }

      if (can_split && button("SPLIT", &font, split_button_pos, button_size, mouse_pos, &split_button_state)) {
        i32 card;
        ggf_darray_pop(hand->cards, &card);
        
        hand_t *new_hand = player_hands + player_hand_count;
        ggf_darray_push(new_hand->cards, &card);
        new_hand->worth = calculate_cards_worth(new_hand->cards);
        ++player_hand_count;
      }

      if (hand->worth == 21) {
        if (player_hand_index == player_hand_count - 1) {
          if (dealer_hand.worth == 10 || dealer_hand.worth == 11) {
            game_state = GAME_STATE_DEALER_TURN;
          } else {
            game_state = GAME_STATE_RESULT;
          }
        } else {
          ++player_hand_index;
        }
      } else if (hand->worth > 21) {
        if (player_hand_index == player_hand_count - 1) {
          game_state = GAME_STATE_RESULT;
        } else {
          ++player_hand_index;
        }
      }

      /*if (ggf_darray_get_length(player_hand.cards) == 2 &&
          current_bet * 2 <= money) {
        if (button("DOUBLE", &font, double_button_pos, button_size, mouse_pos,
                   &double_button_state)) {
          current_bet *= 2;
          add_card_to_hand(&player_hand, get_card(deck));
          if (player_hand.worth == 21) {
            if (dealer_hand.worth == 10 || dealer_hand.worth == 11) {
              game_state = GAME_STATE_DEALER_TURN;
            } else {
              game_state = GAME_STATE_RESULT;
            }
          } else if (player_hand.worth > 21) {
            game_state = GAME_STATE_RESULT;
          } else {
            game_state = GAME_STATE_DEALER_TURN;
          }
        }
      }*/
    } else if (game_state == GAME_STATE_DEALER_TURN) {
      dealer_timer += dt;
      if (dealer_timer >= 1.0f) {
        add_card_to_hand(&dealer_hand, get_card(deck));

        if (dealer_hand.worth >= 17) {
          game_state = GAME_STATE_RESULT;
        }

        dealer_timer = 0.0f;
      }
    } else if (game_state == GAME_STATE_RESULT) {
      while (ggf_darray_get_length(dealer_hand.cards) < 2) {
        add_card_to_hand(&dealer_hand, get_card(deck));
      }

      for (u32 i = 0; i < player_hand_count; ++i) {
        hand_t *hand = player_hands + i;
        if (hand->result == RESULT_NONE) {
          if (hand->worth > 21) {
            hand->result = RESULT_PLAYER_LOSE;
            money -= current_bet;
          } else if (dealer_hand.worth > 21 ||
                     hand->worth > dealer_hand.worth) {
            hand->result = RESULT_PLAYER_WIN;
            money += current_bet;
            if (hand->worth == 21) {
              money += current_bet;
            }
          } else if (dealer_hand.worth == hand->worth) {
            hand->result = RESULT_PUSH;
          } else if (dealer_hand.worth > hand->worth) {
            hand->result = RESULT_PLAYER_LOSE;
            money -= current_bet;
          }
        }
      }

      if (ggf_input_key_released(GGF_KEY_SPACE) ||
          ggf_input_mouse_released(GGF_MOUSE_BUTTON_LEFT)) {
        for (u32 i = 0; i < player_hand_count; ++i) {
          hand_t *hand = player_hands + i;
          ggf_darray_clear(hand->cards);
          hand->worth = 0;
          hand->result = RESULT_NONE;
        }
        player_hand_count = 1;
        player_hand_index = 0;
        ggf_darray_clear(dealer_hand.cards);
        dealer_hand.worth = 0;

        generate_deck(deck);

        game_state = GAME_STATE_BETTING;
      }
    }

    char money_text[128];
    snprintf(money_text, 128, "$%d", money - current_bet);
    ggf_gfx_draw_text(money_text, (vec2){25.0f, 50.0f}, 32,
                      (vec4){0.0f, 0.0f, 0.0f, 1.0f}, &font);

    if (game_state != GAME_STATE_BETTING) {
      u32 info_text_size = 52;
      char cards_info[128];

      // PLAYER HANDs

      f32 section_size = WIDTH / player_hand_count;
      for (u32 i = 0; i < player_hand_count; ++i) {
        hand_t *p_hand = player_hands + i;
        vec2 pos = {section_size * i + section_size / 2.0f, HEIGHT - 115.0f};

        if (game_state == GAME_STATE_RESULT) {
          if (p_hand->result == RESULT_PLAYER_WIN) {
            snprintf(cards_info, 128, "WIN");
          } else if (p_hand->result == RESULT_PLAYER_LOSE) {
            snprintf(cards_info, 128, "LOSE");
          } else if (p_hand->result == RESULT_PUSH) {
            snprintf(cards_info, 128, "PUSH");
          }
        } else {
          snprintf(cards_info, 128, "%d", p_hand->worth);
        }
        f32 cards_info_width =
            ggf_font_get_text_width(&font, cards_info, info_text_size);
        ggf_gfx_draw_text(
            cards_info,
            (vec2){pos[0] - cards_info_width / 2.0f, HEIGHT - 250.0f},
            info_text_size, (vec4){1.0f, 0.5f, 0.5f, 1.0f}, &font);

        render_cards(p_hand->cards, &font, pos, (vec4){1.0f, 0.1f, 0.1f, 1.0f});
      }

      // DEALER HAND

      snprintf(cards_info, 128, "%d", dealer_hand.worth);
      f32 cards_info_width =
          ggf_font_get_text_width(&font, cards_info, info_text_size);
      ggf_gfx_draw_text(cards_info,
                        (vec2){(WIDTH - cards_info_width) / 2.0f, 285.0f},
                        info_text_size, (vec4){0.5f, 0.5f, 1.0f, 1.0f}, &font);

      render_cards(dealer_hand.cards, &font, 
      (vec2){WIDTH / 2.0f, 115.0f}, (vec4){0.1f, 0.1f, 0.9f, 1.0f});
    }

    if (game_state != GAME_STATE_RESULT) {
      char bet_text[128];
      snprintf(bet_text, 128, "$%d", current_bet);
      u64 text_size = 64;
      f32 text_width = ggf_font_get_text_width(&font, bet_text, text_size);
      ggf_gfx_draw_text(
          bet_text,
          (vec2){(WIDTH - text_width) / 2.0f, HEIGHT / 2.0f + text_size / 3.0f},
          text_size, (vec4){0.0f, 0.0f, 0.0f, 1.0f}, &font);
    }

    ggf_gfx_flush();

    ggf_window_swap_buffers(window);
  }

  ggf_darray_destroy(dealer_hand.cards);
  for (u32 i = 0; i < GGF_ARRAY_COUNT(player_hands); ++i) {
    ggf_darray_destroy(player_hands[i].cards);
  }

  ggf_font_destroy(&font);

  ggf_gfx_shutdown();
  ggf_window_destroy(window);
  ggf_shutdown();
}
