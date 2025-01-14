#ifndef __tetrino_cnosole_hpp__
#define __tetrino_cnosole_hpp__

#include "tetrino.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <thread>

#include <signal.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

class VT100 {
  public:
    static std::string clear() { return "\e[2J"; }
    static std::string cursor_to_origin() { return "\e[H"; }
    static std::string cursor_to(int r, int c) {
        return "\e[" + std::to_string(r) + ";" + std::to_string(c) + "H";
    }
    static std::string color(int c) { return "\e[" + std::to_string(c) + "m"; }
    static std::string cursor(bool on) { return std::string{"\e[?25"} + (on ? 'h' : 'l'); }
    static std::string reset() { return "\e[0m"; }

    using time_t = std::chrono::steady_clock::time_point;

    VT100() {
        // Configure TTY.
        tcgetattr(STDIN_FILENO, &_original_tty);
        _tty = _original_tty;
        _tty.c_lflag &= ~(ICANON | ECHO); // raw mode
        _tty.c_cc[VMIN] = 0;              // min input char (non-blocking)
        tcsetattr(STDIN_FILENO, TCSANOW, &_tty);

        // Time reference.
        _time_origin = std::chrono::steady_clock::now();
    }

    ~VT100() {
        tcsetattr(STDIN_FILENO, TCSANOW, &_original_tty);
        std::cout << VT100::cursor(true) << std::flush;
    }

    ssize_t now() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(now - _time_origin).count();
    }

    int nextc() {
        if (_pos >= _count) {
            _count = read(STDIN_FILENO, _buffer.data(), _buffer.size());
            _pos = 0;
        }
        if (_pos >= _count) {
            return EOF;
        }
        return _buffer[_pos++];
    }

  private:
    struct termios _original_tty;
    struct termios _tty;
    time_t _time_origin;

    std::array<char, 32> _buffer;
    int _pos;
    int _count;
};

struct Box {
    int x, y, width, height;
    Point pos() const { return {x, y}; }
};

class TetrisConsole : public Tetris {
  public:
    static constexpr int intro_width = 36;
    static constexpr int intro_height = 15;
    static constexpr int xscale = 2;

    static constexpr Box held{1, 3, Tetrimino::size *xscale + 2, Tetrimino::size + 2};
    static constexpr Box field{held.x + held.width + 7, held.y, matrix_width *xscale + 2,
                               skyline + 1};
    static constexpr Box next{field.x + field.width + 2, held.y, held.width, held.height};
    static constexpr Box tally{next.x, next.y + next.height + 1, 10};
    static constexpr Box info{held.x, held.y + held.height + 1, held.width + 4};

    static constexpr int screen_width = tally.x + tally.width;
    static constexpr int screen_height = field.y + field.height;

    static constexpr Box intro{(screen_width - intro_width) / 2, (screen_height - intro_height) / 2,
                               intro_width, intro_height};

    enum Tiles {
        border_v = 512,
        border_h,
        border_tl,
        border_tr,
        border_bl,
        border_br,
    };

    TetrisConsole(unsigned int seed = 0) : Tetris(seed) {
        std::cout << VT100::clear() << VT100::cursor_to_origin() << VT100::cursor(false)
                  << std::flush;
        _last_frame_time = console.now();
        _last_sync_time = never;
    }

    ~TetrisConsole() { std::cout << VT100::cursor(true) << std::flush; }

    const Image<screen_width, screen_height> &screen() const { return _screen; }

    bool tic() {
        ssize_t now = console.now();
        constexpr ssize_t two_frames = (ssize_t)(2 * 1'000'000) / 60;
        ssize_t elapsed = std::min(now - _last_frame_time, two_frames);
        _last_frame_time = now;

        // Read input buffer
        ssize_t input_frame = current_frame() + 1;
        Tetris::Input::value_t command;
        int c;
        while ((c = console.nextc()) != EOF) {
            switch (c) {
            case '\e':
                if ((c = console.nextc()) == '[') {
                    switch (c = console.nextc()) {
                    case 'D': command = Tetris::Input::move_left; break;
                    case 'C': command = Tetris::Input::move_right; break;
                    case 'B': command = Tetris::Input::soft_drop; break;
                    default: continue;
                    }
                } else {
                    continue;
                }
                break;
            case 'z': command = Tetris::Input::rotate_left; break;
            case 'x': command = Tetris::Input::rotate_right; break;
            case ' ': command = Tetris::Input::hard_drop; break;
            case 'q': command = Tetris::Input::quit; break;
            case 'c': command = Tetris::Input::hold; break;
            case 'r': _old_screen.clear(0); continue; // redraw
            default: continue;
            }
            _inputs.push({command, Tetris::Input::pressed, input_frame});
            _inputs.push({command, Tetris::Input::released, input_frame});
        }

        return Tetris::tic(elapsed, _inputs);
    }

    void throttle() {
        ssize_t now = console.now();
        constexpr ssize_t one_frame = (ssize_t)(1'000'000) / 60;
        ssize_t idle = std::max(now - (_last_sync_time + one_frame), (ssize_t)0);
        _last_sync_time = now;
        std::this_thread::sleep_for(std::chrono::microseconds(idle));
    }

    void draw() {
        _screen.clear();

        _draw_box(field, true);
        _draw_box(held);
        _draw_box(next);
        _draw_text("Next", next.pos() + Point{3, next.height - 1});
        _draw_text("Held", held.pos() + Point{3, held.height - 1});
        for (int i = 0; i < skyline; ++i) {
            _draw_text(std::to_string(i + 1), field.pos() + Point{-3, field.height - 2 - i}, 2);
        }

        _draw_text(std::string{"Score "} + std::to_string(_tally), tally.pos(), tally.width);

        for (int i = 0; i < 5; ++i) {
            int m = _messages.size() - i - 1;
            if (m < 0) break;
            _draw_text(_messages[m], tally.pos() + shift_down * (2 + i), tally.width);
        }

        _draw_text(std::string{"Level "} + std::to_string(_level),
                   info.pos() + shift_down * info.height, info.width);

        _draw_text(std::string{"Cleared "} + std::to_string(_num_lines_cleared),
                   info.pos() + shift_down * (info.height + 1), info.width);

        int ycrop = matrix_height - skyline;
        _matrix.paste(_screen, field.pos() + shift_right, xscale, ycrop);
        if (_ghost_block.type != Tetrimino::none) {
            _ghost_block.paste(
                _screen,
                field.pos() + Point{_ghost_block.pos.x * xscale + 1, _ghost_block.pos.y - ycrop},
                xscale);
        }
        int cr = std::max(ycrop - 1 - _block.pos.y, 0);
        _block.paste(_screen,
                     field.pos() + Point{1 + _block.pos.x * xscale, _block.pos.y + cr - ycrop},
                     xscale, cr);
        _next_block.paste(_screen, next.pos() + Point{1, 1}, xscale);
        if (_held_block.type != Tetrimino::none) {
            _held_block.paste(_screen, held.pos() + Point{1, 1}, xscale);
        }

        if (_game_state == gameover || _game_state == welcome) {
            _draw_box(intro);
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

            int num_lines = std::count(msg, msg + strlen(msg), '\n') + 1;

            _draw_text(msg, intro.pos() + Point{4, (intro.height - num_lines) / 2});
        }
    }

    void present() {
        for (int y = 0; y < _screen.height; y++) {
            auto row = _screen[y];
            auto old_row = _old_screen[y];
            auto same = equal(begin(row), end(row), begin(old_row));
            if (same) continue;
            copy(begin(row), end(row), begin(old_row));
            if (_cursor_y != y) {
                std::cout << VT100::cursor_to(y + 1, 1);
                _cursor_y = y;
            }
            std::cout << VT100::color(39);
            int current_color = 39; // default
            for (auto tile : row) {
                int color;
                std::string glyph;
#undef CASE
#define CASE(T, C, G)                                                                              \
    case T:                                                                                        \
        color = C;                                                                                 \
        glyph = G;                                                                                 \
        break;
                switch (tile) {
                    CASE(Tetrimino::I, 96, "█");
                    CASE(Tetrimino::J, 94, "█");
                    CASE(Tetrimino::L, 91, "█");
                    CASE(Tetrimino::O, 93, "█");
                    CASE(Tetrimino::S, 92, "█");
                    CASE(Tetrimino::T, 95, "█");
                    CASE(Tetrimino::Z, 31, "█");
                    CASE(Tetrimino::G, 37, "░");
                    CASE(border_v, 39, "│");
                    CASE(border_h, 39, "─");
                    CASE(border_tl, 39, "╭");
                    CASE(border_tr, 39, "╮");
                    CASE(border_bl, 39, "╰");
                    CASE(border_br, 39, "╯");
                default:
                    color = 39;
                    glyph = (char)tile;
                    break;
                }
                if (color != current_color) {
                    std::cout << VT100::color(color);
                    current_color = color;
                }
                std::cout << glyph;
            }
            std::cout << '\n';
            _cursor_y++;
        }
        std::cout << std::flush;
    }

  protected:
    VT100 console;
    std::queue<Tetris::Input> _inputs;
    Image<screen_width, screen_height> _screen;
    Image<screen_width, screen_height> _old_screen;
    int _cursor_y;
    ssize_t _last_frame_time;
    ssize_t _last_sync_time;

    void _draw_box(const Box &box, bool open_top = false) {
        int y = box.y;
        auto draw_line = [&](int left, int middle, int right) {
            for (int x = box.x; x < box.x + box.width; ++x) {
                _screen[{x, y}] =
                    (x == box.x) ? left : ((x == box.x + box.width - 1) ? right : middle);
            }
            y += 1;
        };
        if (!open_top) draw_line(border_tl, border_h, border_tr);
        while (y < box.y + box.height - 1) {
            draw_line(border_v, ' ', border_v);
        }
        draw_line(border_bl, border_h, border_br);
    }

    void _draw_text(const std::string &text, Point p, int width = 0) {
        int i = 0;
        Point q = p;
        while (i < text.size()) {
            for (; i < text.size(); ++i, ++q.x) {
                if (text[i] == '\n') {
                    i += 1;
                    goto next_line;
                }
                _screen[q] = text[i];
            }
            for (; q.x < p.x + width; ++q.x) {
                _screen[q] = ' ';
            }
        next_line:;
            q.x = p.x;
            q.y += 1;
        }
    }
};

#endif // __tetrino_cnosole_hpp__
