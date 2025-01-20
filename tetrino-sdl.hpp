#ifndef __tetrino_sdl_hpp__
#define __tetrino_sdl_hpp__

#include "tetrino.hpp"

#include <SDL.h>
#include <SDL_ttf.h>
#include <algorithm>
#include <map>

class TetrisSDL : public Tetris {
  public:
    static constexpr int nominal_scale = 32;
    static constexpr int nominal_screen_width =
        matrix_width * nominal_scale * 3 + matrix_width * nominal_scale / 2;
    static constexpr int nominal_screen_height =
        skyline * nominal_scale + skyline * nominal_scale / 3;
    static constexpr int nominal_font_size = nominal_scale / 2;

    TetrisSDL(const TetrisSDL &) = delete;
    TetrisSDL &operator=(const TetrisSDL &) = delete;

    TetrisSDL(unsigned int seed = 0) : Tetris{seed}, last_frame_time{}, font{} {
        window = SDL_CreateWindow("Tetrino", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  nominal_screen_width, nominal_screen_height,
                                  SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
        renderer =
            SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

        update_geometry();
        last_frame_time = (ssize_t)SDL_GetTicks64() * 1'000;
    }

    ~TetrisSDL() {
        if (font) TTF_CloseFont(font);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
    }

    bool tic() {
        ssize_t now = (ssize_t)SDL_GetTicks64() * 1'000;
        constexpr ssize_t two_frames = (ssize_t)(2 * 1'000'000) / 60;
        ssize_t elapsed = std::min(now - last_frame_time, (ssize_t)20'000);
        last_frame_time = now;

        ssize_t input_frame = current_frame() + 1;
        Tetris::Input::value_t command;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                inputs.push({command, Tetris::Input::pressed, input_frame});
                inputs.push({command, Tetris::Input::released, input_frame});
            } else if ((event.type == SDL_KEYUP || event.type == SDL_KEYDOWN) &&
                       event.key.repeat == 0) {
                Tetris::Input::value_t value;
                auto state =
                    (event.type == SDL_KEYDOWN) ? Tetris::Input::pressed : Tetris::Input::released;
                switch (event.key.keysym.sym) {
                case SDLK_LEFT: value = Tetris::Input::move_left; break;
                case SDLK_RIGHT: value = Tetris::Input::move_right; break;
                case SDLK_DOWN: value = Tetris::Input::soft_drop; break;
                case SDLK_SPACE: value = Tetris::Input::hard_drop; break;
                case SDLK_z: value = Tetris::Input::rotate_left; break;
                case SDLK_x: value = Tetris::Input::rotate_right; break;
                case SDLK_c: value = Tetris::Input::hold; break;
                case SDLK_q: value = Tetris::Input::quit; break;
                default: continue;
                }
                inputs.push({value, state, input_frame});
            }
        }
        return Tetris::tic(elapsed, inputs);
    }

    void draw() {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &field_box);
        SDL_RenderDrawRect(renderer, &next_box);
        SDL_RenderDrawRect(renderer, &held_box);
        draw_text("Next", next_box.x + next_box.w / 2, next_box.y + next_box.h + 2, true);
        draw_text("Held", held_box.x + held_box.w / 2, held_box.y + held_box.h + 2, true);

        int crop = matrix_height - skyline;
        draw_image(matrix,          //
                   field_box.x + 1, //
                   field_box.y, scale, crop);

        draw_image(ghost_block,                                 //
                   field_box.x + 1 + ghost_block.pos.x * scale, //
                   field_box.y + (ghost_block.pos.y - crop) * scale, scale);

        draw_image(block,                                 //
                   field_box.x + 1 + block.pos.x * scale, //
                   field_box.y + (block.pos.y - crop) * scale, scale);

        {
            // Hide everything above the skyline except for a few pixels.
            SDL_Rect hide = {field_box.x + 1, 0, field_box.w - 2, field_box.y - scale / 2};
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &hide);
        }

        draw_image(next_block, next_box.x + 1, next_box.y + 1, scale);

        if (held_block.type != Tetrimino::none) {
            draw_image(held_block, held_box.x + 1, held_box.y + 1, scale);
        }

        draw_text(std::string{"Score "} + std::to_string(tally), right_score_box.x,
                  right_score_box.y);

        for (int i = 0; i < 5; ++i) {
            int m = messages.size() - i - 1;
            if (m < 0) break;
            draw_text(messages[m], right_score_box.x, right_score_box.y + line_skip * (3 + i));
        }

        draw_text(std::string{"Level "} + std::to_string(level), left_score_box.x,
                  left_score_box.y);

        draw_text(std::string{"Cleared "} + std::to_string(num_lines_cleared), left_score_box.x,
                  left_score_box.y + line_skip);

        if (game_state == WELCOME || game_state == GAME_OVER) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &info_box);

            SDL_SetRenderDrawColor(renderer, 10, 200, 10, 255);
            SDL_RenderDrawRect(renderer, &info_box);

            const auto *msg = (game_state == WELCOME) ? "Ready?\n"
                                                        "Press space to start\n\n"
                                                        "z:     rotate left\n"
                                                        "x:     rotate right\n"
                                                        "c:     hold\n"
                                                        "left:  move left\n"
                                                        "right: move right\n"
                                                        "down:  soft drop\n"
                                                        "space: hard drop\n"
                                                        "q:     quit"
                                                      : "Game Over";

            int line_skip = TTF_FontLineSkip(font);
            int num_lines = std::count(msg, msg + strlen(msg), '\n') + 1;
            int text_height = font_height * num_lines + (line_skip - font_height) * (num_lines - 1);

            draw_text(msg, info_box.x + info_box.w / 2, info_box.y + (info_box.h - text_height) / 2,
                      true);
        }
    }

    void present() { SDL_RenderPresent(renderer); }

  protected:
    std::queue<Tetris::Input> inputs;

    int scale;
    int screen_width;
    int screen_height;
    SDL_Rect field_box;
    SDL_Rect info_box;
    SDL_Rect next_box;
    SDL_Rect held_box;
    SDL_Rect right_score_box;
    SDL_Rect left_score_box;

    SDL_Window *window;
    SDL_Renderer *renderer;
    static constexpr char font_path[] = "font.ttf";
    TTF_Font *font;
    int font_size;
    int font_height;
    int line_skip;
    ssize_t last_frame_time;

    void update_geometry() {
        SDL_GetRendererOutputSize(renderer, &screen_width, &screen_height);

        int dpscale = screen_width / nominal_screen_width;
        scale = nominal_scale * dpscale;
        int info_width = screen_width / 2;
        int info_height = 12 * scale;
        int pad = scale + scale / 2;

        font_size = nominal_font_size * dpscale;
        if (font) TTF_CloseFont(font);
        font = TTF_OpenFont(font_path, font_size);
        assert(font);
        font_height = TTF_FontHeight(font);
        line_skip = TTF_FontLineSkip(font);

        field_box = {.x = (screen_width - (matrix_width * scale + 2)) / 2, //
                     .y = (screen_height - (skyline * scale + 1)) / 2,     //
                     .w = matrix_width * scale + 2,                        //
                     .h = skyline * scale + 1};

        info_box = {.x = (screen_width - info_width) / 2,   //
                    .y = (screen_height - info_height) / 2, //
                    .w = info_width,                        //
                    .h = info_height};

        next_box = {.x = field_box.x + field_box.w +
                         (field_box.x - Tetrimino::size * scale - 2) / 2, //
                    .y = field_box.y,                                     //
                    .w = Tetrimino::size * scale + 2,                     //
                    .h = Tetrimino::size * scale + 2};

        held_box = {.x = field_box.x - next_box.w -
                         (field_box.x - Tetrimino::size * scale - 2) / 2, //
                    .y = field_box.y,                                     //
                    .w = next_box.w,                                      //
                    .h = next_box.h};

        right_score_box = {.x = field_box.x + field_box.w + pad,                      //
                           .y = next_box.y + next_box.h + 3 * line_skip,              //
                           .w = screen_width - (field_box.x + field_box.w + 2 * pad), //
                           .h = screen_height - (next_box.y + next_box.h + 2 * pad)};

        left_score_box = {.x = pad,                   //
                          .y = right_score_box.y,     //
                          .w = field_box.x - 2 * pad, //
                          .h = right_score_box.h};
    }

    std::array<uint8_t, 3> get_tetrimino_color(Tetrimino::type_t type) const {
        std::array<uint8_t, 3> color{};
        switch (type) {
        case Tetrimino::I: color = {0, 255, 255}; break;
        case Tetrimino::J: color = {0, 0, 255}; break;
        case Tetrimino::L: color = {255, 127, 0}; break;
        case Tetrimino::O: color = {255, 255, 0}; break;
        case Tetrimino::S: color = {0, 255, 0}; break;
        case Tetrimino::T: color = {128, 0, 128}; break;
        case Tetrimino::Z: color = {255, 0, 0}; break;
        case Tetrimino::G: color = {127, 127, 127}; break;
        default: break;
        }
        return color;
    }

    template <int W, int H>
    void draw_image(const Image<W, H> &image, int x, int y, int s, int crop_top = 0) {
        for (int r = crop_top; r < H; ++r) {
            for (int c = 0; c < W; ++c) {
                int type = image[{c, r}];
                if (type == 0) continue;
                SDL_Rect tile{.x = x + c * s, //
                              .y = y + (r - crop_top) * s,
                              .w = s,
                              .h = s};
                auto color = get_tetrimino_color(static_cast<Tetrimino::type_t>(type));
                SDL_SetRenderDrawColor(renderer, color[0], color[1], color[2], 255);
                SDL_RenderFillRect(renderer, &tile);
            }
        }
    }

    // Cache text strings for efficiency.
    struct TextureDeleter {
        void operator()(SDL_Texture *t) { SDL_DestroyTexture(t); }
    };
    std::map<std::string, std::unique_ptr<SDL_Texture, TextureDeleter>> strings;

    void draw_text(const std::string &str, int x, int y, bool center = false) {
        if (strings.count(str) == 0) {
            SDL_Color color = {255, 255, 255};
            SDL_Surface *surface = TTF_RenderUTF8_Solid_Wrapped(font, str.c_str(), color, 0L);
            SDL_Texture *text = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
            strings[str] = std::unique_ptr<SDL_Texture, TextureDeleter>{text};
        }
        SDL_Texture *text = strings[str].get();
        int w, h;
        SDL_QueryTexture(text, nullptr, nullptr, &w, &h);
        SDL_Rect rect{x - (center ? w / 2 : 0), y, w, h};
        SDL_RenderCopy(renderer, text, NULL, &rect);
    }
};

#endif // __tetrino_sdl_hpp__
