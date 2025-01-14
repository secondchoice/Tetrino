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

    TetrisSDL(unsigned int seed = 0) : Tetris{seed}, _last_frame_time{}, _font{} {
        _window = SDL_CreateWindow("Tetrino", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   nominal_screen_width, nominal_screen_height,
                                   SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
        _renderer =
            SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

        _update_geometry();
        _last_frame_time = (ssize_t)SDL_GetTicks64() * 1'000;
    }

    ~TetrisSDL() {
        if (_font) TTF_CloseFont(_font);
        SDL_DestroyRenderer(_renderer);
        SDL_DestroyWindow(_window);
    }

    bool tic() {
        ssize_t now = (ssize_t)SDL_GetTicks64() * 1'000;
        constexpr ssize_t two_frames = (ssize_t)(2 * 1'000'000) / 60;
        ssize_t elapsed = std::min(now - _last_frame_time, (ssize_t)20'000);
        _last_frame_time = now;

        ssize_t input_frame = current_frame() + 1;
        Tetris::Input::value_t command;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                _inputs.push({command, Tetris::Input::pressed, input_frame});
                _inputs.push({command, Tetris::Input::released, input_frame});
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
                _inputs.push({value, state, input_frame});
            }
        }
        return Tetris::tic(elapsed, _inputs);
    }

    void draw() {
        SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 255);
        SDL_RenderClear(_renderer);

        SDL_SetRenderDrawColor(_renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(_renderer, &_field);
        SDL_RenderDrawRect(_renderer, &_next);
        SDL_RenderDrawRect(_renderer, &_held);
        _draw_text("Next", _next.x + _next.w / 2, _next.y + _next.h + 2, true);
        _draw_text("Held", _held.x + _held.w / 2, _held.y + _held.h + 2, true);

        int crop = matrix_height - skyline;
        _draw_image(_matrix,      //
                    _field.x + 1, //
                    _field.y, _scale, crop);

        _draw_image(_ghost_block,                               //
                    _field.x + 1 + _ghost_block.pos.x * _scale, //
                    _field.y + (_ghost_block.pos.y - crop) * _scale, _scale);

        _draw_image(_block,                               //
                    _field.x + 1 + _block.pos.x * _scale, //
                    _field.y + (_block.pos.y - crop) * _scale, _scale);

        {
            // Hide everything above the skyline except for a few pixels.
            SDL_Rect hide = {_field.x + 1, 0, _field.w - 2, _field.y - _scale / 2};
            SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(_renderer, &hide);
        }

        _draw_image(_next_block, _next.x + 1, _next.y + 1, _scale);

        if (_held_block.type != Tetrimino::none) {
            _draw_image(_held_block, _held.x + 1, _held.y + 1, _scale);
        }

        _draw_text(std::string{"Score "} + std::to_string(_tally), _rscore.x, _rscore.y);

        for (int i = 0; i < 5; ++i) {
            int m = _messages.size() - i - 1;
            if (m < 0) break;
            _draw_text(_messages[m], _rscore.x, _rscore.y + _line_skip * (3 + i));
        }

        _draw_text(std::string{"Level "} + std::to_string(_level), _lscore.x, _lscore.y);

        _draw_text(std::string{"Cleared "} + std::to_string(_num_lines_cleared), _lscore.x,
                   _lscore.y + _line_skip);

        if (_game_state == welcome || _game_state == gameover) {
            SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(_renderer, &_info);

            SDL_SetRenderDrawColor(_renderer, 10, 200, 10, 255);
            SDL_RenderDrawRect(_renderer, &_info);

            const auto *msg = (_game_state == welcome) ? "Ready?\n"
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

            int line_skip = TTF_FontLineSkip(_font);
            int num_lines = std::count(msg, msg + strlen(msg), '\n') + 1;
            int text_height = _font_height * num_lines + (line_skip - _font_height) * (num_lines - 1);

            _draw_text(msg,
                       _info.x + _info.w / 2, _info.y + (_info.h - text_height) / 2, true);
        }
    }

    void present() { SDL_RenderPresent(_renderer); }

  protected:
    std::queue<Tetris::Input> _inputs;

    int _scale;
    int _screen_width;
    int _screen_height;
    SDL_Rect _field;
    SDL_Rect _info;
    SDL_Rect _next;
    SDL_Rect _held;
    SDL_Rect _rscore;
    SDL_Rect _lscore;

    SDL_Window *_window;
    SDL_Renderer *_renderer;
    static constexpr char _font_path[] = "font.ttf";
    TTF_Font *_font;
    int _font_size;
    int _font_height;
    int _line_skip;
    ssize_t _last_frame_time;

    void _update_geometry() {
        SDL_GetRendererOutputSize(_renderer, &_screen_width, &_screen_height);

        int dpscale = _screen_width / nominal_screen_width;
        _scale = nominal_scale * dpscale;
        int info_width = _screen_width / 2;
        int info_height = 12 * _scale;
        int pad = _scale + _scale / 2;

        _font_size = nominal_font_size * dpscale;
        if (_font) TTF_CloseFont(_font);
        _font = TTF_OpenFont(_font_path, _font_size);
        assert(_font);
        _font_height = TTF_FontHeight(_font);
        _line_skip = TTF_FontLineSkip(_font);

        _field = {.x = (_screen_width - (matrix_width * _scale + 2)) / 2, //
                  .y = (_screen_height - (skyline * _scale + 1)) / 2,     //
                  .w = matrix_width * _scale + 2,                         //
                  .h = skyline * _scale + 1};

        _info = {.x = (_screen_width - info_width) / 2,   //
                 .y = (_screen_height - info_height) / 2, //
                 .w = info_width,                         //
                 .h = info_height};

        _next = {.x = _field.x + _field.w + (_field.x - Tetrimino::size * _scale - 2) / 2, //
                 .y = _field.y,                                                            //
                 .w = Tetrimino::size * _scale + 2,                                        //
                 .h = Tetrimino::size * _scale + 2};

        _held = {.x = _field.x - _next.w - (_field.x - Tetrimino::size * _scale - 2) / 2, //
                 .y = _field.y,                                                           //
                 .w = _next.w,                                                            //
                 .h = _next.h};

        _rscore = {.x = _field.x + _field.w + pad,                       //
                   .y = _next.y + _next.h + 3 * _line_skip,              //
                   .w = _screen_width - (_field.x + _field.w + 2 * pad), //
                   .h = _screen_height - (_next.y + _next.h + 2 * pad)};

        _lscore = {.x = pad,                //
                   .y = _rscore.y,          //
                   .w = _field.x - 2 * pad, //
                   .h = _rscore.h};
    }

    std::array<uint8_t, 3> _get_tetrimino_color(Tetrimino::type_t type) const {
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
    void _draw_image(const Image<W, H> &image, int x, int y, int s, int crop_top = 0) {
        for (int r = crop_top; r < H; ++r) {
            for (int c = 0; c < W; ++c) {
                int type = image[{c, r}];
                if (type == 0) continue;
                SDL_Rect tile{.x = x + c * s, //
                              .y = y + (r - crop_top) * s,
                              .w = s,
                              .h = s};
                auto color = _get_tetrimino_color(static_cast<Tetrimino::type_t>(type));
                SDL_SetRenderDrawColor(_renderer, color[0], color[1], color[2], 255);
                SDL_RenderFillRect(_renderer, &tile);
            }
        }
    }

    // Cache text strings for efficiency.
    struct TextureDeleter {
        void operator()(SDL_Texture *t) { SDL_DestroyTexture(t); }
    };
    std::map<std::string, std::unique_ptr<SDL_Texture, TextureDeleter>> _strings;

    void _draw_text(const std::string &str, int x, int y, bool center = false) {
        if (_strings.count(str) == 0) {
            SDL_Color color = {255, 255, 255};
            SDL_Surface *surface = TTF_RenderUTF8_Solid_Wrapped(_font, str.c_str(), color, 0L);
            SDL_Texture *text = SDL_CreateTextureFromSurface(_renderer, surface);
            SDL_FreeSurface(surface);
            _strings[str] = std::unique_ptr<SDL_Texture, TextureDeleter>{text};
        }
        SDL_Texture *text = _strings[str].get();
        int w, h;
        SDL_QueryTexture(text, nullptr, nullptr, &w, &h);
        SDL_Rect rect{x - (center ? w / 2 : 0), y, w, h};
        SDL_RenderCopy(_renderer, text, NULL, &rect);
    }
};

#endif // __tetrino_sdl_hpp__
