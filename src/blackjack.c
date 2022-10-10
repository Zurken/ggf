#include "ggf.c"
#include <sys/time.h>
#include <time.h>

// bredd och höjd på fönstret som öppnas
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
// global variabel som vuisar vad varje kort är värt. J D K är värda 10.
global_variable i32 card_worth[CARD_COUNT] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 10, 10};

// de olika stadie som knappar kan ha.
enum {
  BUTTON_STATE_REST = 0, // rör inte knappen
  BUTTON_STATE_HOVER, // håller musen över knappen
  BUTTON_STATE_DOWN, // håller nere knappen
  BUTTON_STATE_RELEASED, // har släppt knappen (utlöser den)

  BUTTON_STATE_COUNT,
};

// de olika stadier som spelet består av.
enum {
  GAME_STATE_BETTING, // spelaren väljer hur mycket dem vill satsa
  GAME_STATE_DEALING, // dealer och spelaren får sina två första kort.
  GAME_STATE_PLAYER_TURN, // spelaren väljer om den vill ha kort eller inte.
  GAME_STATE_DEALER_TURN, // dealer tar sina kort.
  GAME_STATE_RESULT, // spelets resultat visas för spelaren.
};

// de olika resultat varje hand kan få
enum {
  RESULT_NONE, // handen är i spel

  RESULT_PUSH, // handen är lika med dealerns hand
  RESULT_PLAYER_WIN, // spelarens hand är högre
  RESULT_PLAYER_LOSE, // spelarens hand är lägre

  RESULT_COUNT,
};

// en hand
typedef struct {
  i32 result;
  i32 *cards; // en array med kort
  u32 worth; // det totala värdet på korten i handen
} hand_t;

// slumpar fram ett nummer från 0 till max med hjälp av klockan.
u32 get_rand(u32 max) {
  local_persist u32 offset = 0;
  struct timespec seed;
  clock_gettime(CLOCK_MONOTONIC, &seed);
  u32 result =
      (u32)(ggf_randf(seed.tv_sec * 5425 + seed.tv_nsec + ++offset) * (f32)max);
  return result;
}

// ger index på en slot där ett kort ännu inte har blivit plaserad. används vid fyllnad av kortlek.
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

// tar in en array och kastar runt innehållet.
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

// fyller en kortlek med 52 kort
void generate_deck(i32 *deck) {
  for (u32 i = 0; i < 52; ++i) { // töm hela kortleken genom att sätta varje kort till -1
    deck[i] = -1;
  }

  // gå igenom varje kort och lägg till 4 av dess i kortleken. Hjärter klöver spader och ruter.
  for (u32 i = 0; i < CARD_COUNT; ++i) {
    for (u32 j = 0; j < 4; ++j) {
      i32 index = find_empty_card_index(deck);
      deck[index] = i;
    }
  }

  // blanda kortleken en gång för att garantera att den är slumpad. 
  shuffle(deck, 52);
}

// ger dig kortet högst upp i kortleken.
i32 get_card(i32 *deck) {
  // gå neråt i kortleken tills ett kort träffas på
  i32 index = 51;
  for (; index >= 0; --index) {
    if (deck[index] != -1) {
      break;
    }
  }

  // spara kortet och sätt sedan kortet på platsen i kortleken till -1.
  i32 card = deck[index];
  deck[index] = -1;
  return card;
}

b32 is_point_in_rect(vec2 point, vec2 pos, vec2 extent) {
  b32 result = point[0] >= pos[0] && point[1] >= pos[1] &&
               point[0] <= pos[0] + extent[0] && point[1] <= pos[1] + extent[1];
  return result;
}

// måla upp en array med kort
void render_cards(i32 *cards, ggf_font_t *font, vec2 center, vec2 card_size,
                  f32 spacing, vec4 color) {
  char *card_names[CARD_COUNT] = {
      "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "D", "K",
  };

  u32 card_count = ggf_darray_get_length(cards);

  // räkna ut bredden på alla korten efter varndra med mellanrum i åtanke.
  f32 cards_width = (card_size[0] * card_count + spacing * (card_count - 1));
  // räkna ut positionen
  vec2 pos = {center[0] - cards_width / 2.0f,
              center[1] - card_size[1] / 2.0f};
  for (u32 i = 0; i < card_count; ++i) {
    // för varje kort, måla en bakgrundsfyrkant och text i mitten.

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

// måla och processera en knapp
b32 button(char *text, ggf_font_t *font, vec2 pos, vec2 size, vec2 mouse_pos,
           i32 *out_state) {
  b32 result = FALSE;

  if (is_point_in_rect(mouse_pos, pos, size)) {
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
  u32 result = 0;

  u32 ace_count = 0;
  u32 card_count = ggf_darray_get_length(cards);
  for (u32 i = 0; i < card_count; ++i) {
    ace_count += (cards[i] == CARD_ACE);
    result += card_worth[cards[i]];
  }

  // om det går så läggs 10 på för varje A.
  while (21 - result >= 10 && ace_count > 0) {
    result += 10;
    --ace_count;
  }

  return result;
}

void create_hand(hand_t *hand) {
  hand->cards = ggf_darray_create(4, sizeof(i32)); // skapa en dynamisk array.
  hand->worth = 0;
}

void add_card_to_hand(hand_t *hand, i32 card) {
  // lägg till det givna handen till handes kort och omkalkulera handens värde.
  hand->cards = ggf_darray_push(hand->cards, &card);
  hand->worth = calculate_cards_worth(hand->cards);
}

i32 main(i32 argc, char **argv) {
  // starta ggf och skapa en fönster med titlen "Blackjack".
  ggf_init(argc, argv);
  ggf_window_t *window = ggf_window_create("Blackjack", WIDTH, HEIGHT);
  ggf_window_set_resizable(window, FALSE);
  ggf_gfx_init(WIDTH, HEIGHT);

  // ladda in en font från filerna "test old"
  ggf_font_t font;
  ggf_font_load("test old.png", "test old.csv", &font);

  // skapa en kamera
  ggf_camera_t camera = {0};
  glm_ortho(0.0f, WIDTH, HEIGHT, 0.0f, -100.0f, 100.0f, camera.view_projection);
  ggf_gfx_set_camera(&camera);

  // kortleken som används i spelet
  i32 deck[52];
  generate_deck(deck);

  u32 player_hand_index = 0; 
  u32 player_hand_count = 1;
  hand_t player_hands[8] = {}; // spelarens händer. maximalt 8 st. bORde inte bli fler.
  for (u32 i = 0; i < 8; ++i) {
    create_hand(player_hands + i);
  }

  hand_t dealer_hand = {};
  create_hand(&dealer_hand);

  // utgå från att alla knappar är rest.
  i32 stand_button_state = BUTTON_STATE_REST;
  i32 hit_button_state = BUTTON_STATE_REST;

  i32 plus_button_state = BUTTON_STATE_REST;
  i32 minus_button_state = BUTTON_STATE_REST;
  i32 deal_button_state = BUTTON_STATE_REST;
  i32 double_button_state = BUTTON_STATE_REST;
  i32 split_button_state = BUTTON_STATE_REST;

  // börja med $1000 och $1 i bet
  i32 money = 1000;
  i32 current_bet = 1;

  // starta i betting stadiet
  i32 game_state = GAME_STATE_BETTING;

  f32 dealer_timer = 0.0f; // en timer som används för att fördröja dealerns utdelning av kort

  f32 dt = 1.0f / 60.0f; // antalet sekunder per frame.
  while (ggf_window_is_open(window)) {
    ggf_poll_events();

    ggf_gfx_set_clear_color((vec3){0.1f, 0.3f, 0.1f}); // bakgrunsfärg

    ggf_gfx_begin_frame(); // starta rendering

    i32 m_x, m_y; // musens position
    ggf_input_get_mouse_position(&m_x, &m_y);
    vec2 mouse_pos = {(f32)m_x, (f32)m_y}; 

    if (game_state == GAME_STATE_BETTING) {
      // om spelstadiet är i betting

      f32 side_margin = 250.0f;
      vec2 button_size = {100.0f, 100.0f};
      vec2 minus_button_pos = {side_margin, (HEIGHT - button_size[1]) / 2.0f};
      vec2 plus_button_pos = {WIDTH - button_size[0] - side_margin,
                              (HEIGHT - button_size[1]) / 2.0f};
      
      u32 change_amount = 1;
      if (current_bet >= 5)
        change_amount = 5;
      if (current_bet >= 50)
        change_amount = 10;
      if (current_bet >= 100)
        change_amount = 50;
      if (current_bet >= 300)
        change_amount = 100;

      if (button("-", &font, minus_button_pos, button_size, mouse_pos, &minus_button_state)) {
        current_bet -= change_amount;
      }
      if (button("+", &font, plus_button_pos, button_size, mouse_pos, &plus_button_state)) {
        current_bet += change_amount;
      }
      // se till så att bet aldrig blir mer än mängden pengar och inte mindre än 0
      current_bet = (current_bet > money) ? money
                    : (current_bet < 0)   ? 0
                                          : current_bet;

      vec2 deal_button_size = {300.0f, 100.0f};
      vec2 deal_button_pos = {(WIDTH - deal_button_size[0]) / 2.0f,
                              HEIGHT - 250.0f};
      if (current_bet > 0 &&
          (button("DEAL", &font, deal_button_pos, deal_button_size, mouse_pos, &deal_button_state) ||
           ggf_input_key_released(GGF_KEY_SPACE))) {
        // när spelaren vill starta spelet så byts stadiet till "dealing".
        game_state = GAME_STATE_DEALING;
      }

    } else if (game_state == GAME_STATE_DEALING) {
      dealer_timer += dt;

      // börja med att ge ett kort till spelaren.
      u32 card_count = ggf_darray_get_length(player_hands[player_hand_index].cards);
      if ((dealer_timer > 0.5f && card_count == 0) ||
          (dealer_timer - 0.5f > 1.0f && card_count == 1)) {
        add_card_to_hand(player_hands + player_hand_index, get_card(deck));
      }
      // när timern når ett värde ska även dealern få ett kort
      if (dealer_timer > (player_hand_count + 1) * 0.5f &&
          ggf_darray_get_length(dealer_hand.cards) == 0) {
        add_card_to_hand(&dealer_hand, get_card(deck));
      }
      // ge det andra kortet till spelaren
      if (dealer_timer > player_hand_count * 1.0f + 0.5f) {
        dealer_timer = 0.0f;
        if (player_hands[player_hand_index].worth == 21) {
          game_state = GAME_STATE_RESULT;
        } else {
          game_state = GAME_STATE_PLAYER_TURN;
        }
      }

    } else if (game_state == GAME_STATE_PLAYER_TURN) {
      // spelarens tur

      hand_t *hand = player_hands + player_hand_index;
      u32 card_count = ggf_darray_get_length(hand->cards);

      // om spealren splittar så har den bara ett kort i handen. Lägg därför till ett kort om detta sker.
      if (card_count == 1) {
        dealer_timer += dt;
        if (dealer_timer >= 0.5f) {
          add_card_to_hand(hand, get_card(deck));
          dealer_timer = 0.0f;
        }
      }

      b32 can_stand = card_count >= 2; // spelaren får bara stoppa om handen har minst två kort. (undviker att spelaren stoppar precis efter att dem splittat)
      b32 can_split = card_count == 2 && card_worth[hand->cards[0]] == card_worth[hand->cards[1]] && current_bet * 2 <= money;
      b32 can_double = card_count == 2 && current_bet * 2 <= money;

      f32 button_side_margin = 150.0f; // avstånd från vänster och höger sida av fönstret.
      vec2 button_size = {200.0f, 100.0f};

      f32 y_offset = -250.0f;
      vec2 stand_button_pos = {WIDTH - button_side_margin - button_size[0],
                               (HEIGHT - button_size[1]) / 2.0f + y_offset};
      vec2 hit_button_pos = {button_side_margin,
                             (HEIGHT - button_size[1]) / 2.0f + y_offset};
      vec2 split_button_pos = {hit_button_pos[0],
                               hit_button_pos[1] + button_size[1] + 10.0f};
      vec2 double_button_pos = {
          hit_button_pos[0], can_split
                                 ? split_button_pos[1] + button_size[1] + 10.0f
                                 : split_button_pos[1]};

      if (can_stand && button("STAND", &font, stand_button_pos, button_size, mouse_pos,
                 &stand_button_state)) {
        if (player_hand_index == player_hand_count - 1) { // om detta är sista handen är det dealerns tur.
          game_state = GAME_STATE_DEALER_TURN;
          player_hand_index = 0;
        } else { // annars byt till nästa hand. (sker bara när spelaren har splittat)
          ++player_hand_index;
        }
      }

      if (button("HIT", &font, hit_button_pos, button_size, mouse_pos, &hit_button_state)) {
        add_card_to_hand(hand, get_card(deck));
      }

      if (can_split && button("SPLIT", &font, split_button_pos, button_size, mouse_pos, &split_button_state)) {
        i32 card;
        ggf_darray_pop(hand->cards, &card); // ta bort koretet från nuvarande hand

        hand_t *new_hand = player_hands + player_hand_count;
        add_card_to_hand(new_hand, card); // lägg till kortet i den nya handen

        ++player_hand_count;
      }

      if (can_double && button("DOUBLE", &font, double_button_pos, button_size,
                               mouse_pos, &double_button_state)) {
        current_bet *= 2;
        add_card_to_hand(hand, get_card(deck));

        // efter att ha dubblat kan spelaren inte fortsätta på samma hand.
        if (player_hand_index == player_hand_count - 1) {
          game_state = GAME_STATE_DEALER_TURN;
          player_hand_index = 0;
        } else {
          ++player_hand_index;
        }
      }

      // om spelarens hand gått över 21 så är handen slut.
      if (hand->worth >= 21) {
        if (player_hand_index == player_hand_count - 1) {
          game_state = GAME_STATE_RESULT;
        } else {
          ++player_hand_index;
        }
      }

    } else if (game_state == GAME_STATE_DEALER_TURN) {
      // lägg till ett kort på dealerns hand varje sekund.

      dealer_timer += dt;
      if (dealer_timer >= 1.0f) {
        add_card_to_hand(&dealer_hand, get_card(deck));

        if (dealer_hand.worth >= 17) {
          game_state = GAME_STATE_RESULT;
        }

        dealer_timer = 0.0f;
      }
    } else if (game_state == GAME_STATE_RESULT) {
      while (ggf_darray_get_length(dealer_hand.cards) < 2) { // garantera att dealern har två kort på handen.
        add_card_to_hand(&dealer_hand, get_card(deck));
      }

      // räkna ut resultatet för varje hand.
      for (u32 i = 0; i < player_hand_count; ++i) {
        hand_t *hand = player_hands + i;

        if (hand->result == RESULT_NONE) { // bara om handen inte har ett resultat

          if (hand->worth > 21) {
            hand->result = RESULT_PLAYER_LOSE;
            money -= current_bet;
          } else if (dealer_hand.worth > 21 ||
                     hand->worth > dealer_hand.worth) {
            hand->result = RESULT_PLAYER_WIN;
            money += current_bet;
            if (hand->worth == 21) { // dubbel vinst vid blackjack
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

      // om spelaren trycker på space eller klickar så avslutas rundan.
      if (ggf_input_key_released(GGF_KEY_SPACE) || ggf_input_mouse_released(GGF_MOUSE_BUTTON_LEFT)) {
        for (u32 i = 0; i < player_hand_count; ++i) { // nollställ alla händer.
          hand_t *hand = player_hands + i;
          ggf_darray_clear(hand->cards);
          hand->worth = 0;
          hand->result = RESULT_NONE;
        }
        player_hand_count = 1;
        player_hand_index = 0;
        ggf_darray_clear(dealer_hand.cards);
        dealer_hand.worth = 0;

        generate_deck(deck); // generera fram en ny fullständig kortlek.

        game_state = GAME_STATE_BETTING; // gå till baka till betting stadiet.
      }
    }

    // text som visar mängden pengar
    char money_text[128];
    snprintf(money_text, 128, "$%d", game_state == GAME_STATE_RESULT ? money : money - current_bet);
    ggf_gfx_draw_text(money_text, (vec2){25.0f, 50.0f}, 32,
                      (vec4){0.0f, 0.0f, 0.0f, 1.0f}, &font);

    // visa bara kort m.m om spelaren inte är i betting stadiet.
    if (game_state != GAME_STATE_BETTING) {
      u32 info_text_size = 52; // typsnitts storlek på texten ovanför varje hand.
      char cards_info[128];

      f32 spacing = 15.0f; // avstånd mellan varje kort.
      vec2 card_size = {150.0f, 225.0f};

      // SPELARENS HÄNDER
      f32 section_size = WIDTH / player_hand_count; // dela upp hela skärmen i lika många delar som spelarens händer. 
      for (u32 i = 0; i < player_hand_count; ++i) {
        hand_t *p_hand = player_hands + i;
        u32 card_count = ggf_darray_get_length(p_hand->cards);
        vec2 pos = {section_size * i + section_size / 2.0f, HEIGHT - 130.0f};

        // visa bara den gula bakgrunden på den handen som för nuvarande spelas. Eller på alla om spelstadiet är i dealer eller resultat.
        if ((i == player_hand_index && card_count > 0) || 
            game_state == GAME_STATE_DEALER_TURN || game_state == GAME_STATE_RESULT) {
          f32 w = card_count * card_size[0] + spacing * (card_count - 1);
          ggf_gfx_draw_quad_corners(
              (vec2){pos[0] - w / 2.0f - spacing, HEIGHT - 300.0f},
              (vec2){pos[0] + w / 2.0f + spacing, HEIGHT}, -0.1f,
              (vec4){0.35f, 0.3f, 0.04f, 1.0f}, NULL);
        }

        // visa resultatet över händerna om stadiet är i resultat.
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

        // visa text över varje hand
        f32 cards_info_width = ggf_font_get_text_width(&font, cards_info, info_text_size);
        ggf_gfx_draw_text(
            cards_info,
            (vec2){pos[0] - cards_info_width / 2.0f, HEIGHT - 250.0f},
            info_text_size, (vec4){1.0f, 0.5f, 0.5f, 1.0f}, &font);

        // visa korten i denna hand
        render_cards(p_hand->cards, &font, pos, card_size, spacing,
                     (vec4){1.0f, 0.1f, 0.1f, 1.0f});
      }

      // DEALERNS HAND

      // text
      snprintf(cards_info, 128, "%d", dealer_hand.worth);
      f32 cards_info_width = ggf_font_get_text_width(&font, cards_info, info_text_size);
      ggf_gfx_draw_text(cards_info,
                        (vec2){(WIDTH - cards_info_width) / 2.0f, 285.0f},
                        info_text_size, (vec4){0.5f, 0.5f, 1.0f, 1.0f}, &font);

      // kort
      render_cards(dealer_hand.cards, &font, (vec2){WIDTH / 2.0f, 130.0f},
                   card_size, spacing, (vec4){0.1f, 0.1f, 0.9f, 1.0f});
    }

    // visa bara betting mängden när spel stadiet inte är resultat.
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
