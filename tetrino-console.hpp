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
    static std::string reversed(bool on) { return on ? "\e[7m" : "\e[27m"; }
    static std::string cursor(bool on) { return std::string{"\e[?25"} + (on ? 'h' : 'l'); }
    static std::string reset() { return "\e[0m"; }

    using time_t = std::chrono::steady_clock::time_point;

    VT100() {
        // Configure TTY.
        tcgetattr(STDIN_FILENO, &original_tty);
        tty = original_tty;
        tty.c_lflag &= ~(ICANON | ECHO); // raw mode
        tty.c_cc[VMIN] = 0;              // min input char (non-blocking)
        tcsetattr(STDIN_FILENO, TCSANOW, &tty);

        // Time reference.
        time_origin = std::chrono::steady_clock::now();
    }

    ~VT100() {
        tcsetattr(STDIN_FILENO, TCSANOW, &original_tty);
        std::cout << VT100::cursor(true) << std::flush;
    }

    ssize_t now() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(now - time_origin).count();
    }

    int nextc() {
        if (pos >= count) {
            count = read(STDIN_FILENO, buffer.data(), buffer.size());
            pos = 0;
        }
        if (pos >= count) {
            return EOF;
        }
        return buffer[pos++];
    }

  private:
    struct termios original_tty;
    struct termios tty;
    time_t time_origin;

    std::array<char, 32> buffer;
    int pos;
    int count;
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

    static constexpr Box held_box{1, 3, Tetrimino::size *xscale + 2, Tetrimino::size + 2};
    static constexpr Box field_box{held_box.x + held_box.width + 7, held_box.y,
                                   matrix_width *xscale + 2, skyline + 1};
    static constexpr Box next_box{field_box.x + field_box.width + 2, held_box.y, held_box.width,
                                  held_box.height};
    static constexpr Box tally_box{next_box.x, next_box.y + next_box.height + 1, 10};
    static constexpr Box info_box{held_box.x, held_box.y + held_box.height + 1, held_box.width + 4};

    static constexpr int screen_width = tally_box.x + tally_box.width;
    static constexpr int screen_height = field_box.y + field_box.height;

    static constexpr Box intro_box{(screen_width - intro_width) / 2,
                                   (screen_height - intro_height) / 2, intro_width, intro_height};

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
        last_frame_time = console.now();
        last_sync_time = never;
    }

    ~TetrisConsole() { std::cout << VT100::cursor(true) << std::flush; }

    const Image<screen_width, screen_height> &get_screen() const { return screen; }

    bool tic() {
        ssize_t now = console.now();
        constexpr ssize_t two_frames = (ssize_t)(2 * 1'000'000) / 60;
        ssize_t elapsed = std::min(now - last_frame_time, two_frames);
        last_frame_time = now;

        // Read input buffer
        ssize_t input_frame = current_frame() + 1;
        Tetris::Input::Value command;
        int c;
        while ((c = console.nextc()) != EOF) {
            switch (c) {
            case '\e':
                if ((c = console.nextc()) == '[') {
                    switch (c = console.nextc()) {
                    case 'D': command = Tetris::Input::Value::move_left; break;
                    case 'C': command = Tetris::Input::Value::move_right; break;
                    case 'B': command = Tetris::Input::Value::soft_drop; break;
                    default: continue;
                    }
                } else {
                    continue;
                }
                break;
            case 'z': command = Tetris::Input::Value::rotate_left; break;
            case 'x': command = Tetris::Input::Value::rotate_right; break;
            case ' ': command = Tetris::Input::Value::hard_drop; break;
            case 'q': command = Tetris::Input::Value::quit; break;
            case 'c': command = Tetris::Input::Value::hold; break;
            case 'r': old_screen.clear(0); continue; // redraw
            default: continue;
            }
            inputs.push({command, Tetris::Input::State::pressed, input_frame});
            inputs.push({command, Tetris::Input::State::released, input_frame});
        }

        return Tetris::tic(elapsed, inputs);
    }

    void throttle() {
        ssize_t now = console.now();
        constexpr ssize_t one_frame = (ssize_t)(1'000'000) / 60;
        ssize_t idle = std::max(now - (last_sync_time + one_frame), (ssize_t)0);
        last_sync_time = now;
        std::this_thread::sleep_for(std::chrono::microseconds(idle));
    }

    void draw() {
        screen.clear();

        draw_box(field_box, true);
        draw_box(held_box);
        draw_box(next_box);
        draw_text("Next", next_box.pos() + Point{3, next_box.height - 1});
        draw_text("Held", held_box.pos() + Point{3, held_box.height - 1});
        for (int i = 0; i < skyline; ++i) {
            draw_text(std::to_string(i + 1), field_box.pos() + Point{-3, field_box.height - 2 - i},
                      2);
        }

        draw_text(std::string{"Score "} + std::to_string(tally), tally_box.pos(), tally_box.width);

        for (int i = 0; i < 5; ++i) {
            int m = messages.size() - i - 1;
            if (m < 0) break;
            draw_text(messages[m], tally_box.pos() + shift_down * (2 + i), tally_box.width);
        }

        draw_text(std::string{"Level "} + std::to_string(level),
                  info_box.pos() + shift_down * info_box.height, info_box.width);

        draw_text(std::string{"Cleared "} + std::to_string(num_lines_cleared),
                  info_box.pos() + shift_down * (info_box.height + 1), info_box.width);

        int ycrop = matrix_height - skyline;
        matrix.paste(screen, field_box.pos() + shift_right, xscale, ycrop);
        if (ghost_block.type != Tetrimino::none) {
            ghost_block.paste(screen,
                              field_box.pos() +
                                  Point{ghost_block.pos.x * xscale + 1, ghost_block.pos.y - ycrop},
                              xscale);
        }
        int cr = std::max(ycrop - 1 - block.pos.y, 0);
        block.paste(screen,
                    field_box.pos() + Point{1 + block.pos.x * xscale, block.pos.y + cr - ycrop},
                    xscale, cr);
        next_block.paste(screen, next_box.pos() + Point{1, 1}, xscale);
        if (held_block.type != Tetrimino::none) {
            held_block.paste(screen, held_box.pos() + Point{1, 1}, xscale);
        }

        if (game_state == GameState::GAME_OVER || game_state == GameState::WELCOME) {
            draw_box(intro_box);
            const auto *msg = (game_state == GameState::WELCOME) ? "Ready?\n"
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

            draw_text(msg, intro_box.pos() + Point{4, (intro_box.height - num_lines) / 2});
        }
    }

    void present() {
        for (int y = 0; y < screen.height; y++) {
            auto row = screen[y];
            auto old_row = old_screen[y];
            auto same = equal(begin(row), end(row), begin(old_row));
            if (same) continue;
            copy(begin(row), end(row), begin(old_row));
            if (cursor_y != y) {
                std::cout << VT100::cursor_to(y + 1, 1);
                cursor_y = y;
            }
            std::cout << VT100::color(39) << VT100::reversed(false);
            int current_color = 39; // default
            bool current_reversed = false;
            for (auto tile : row) {
                int color;
                int reversed;
                std::string glyph;
#undef CASE
#define CASE(T, C, R, G)                                                                           \
    case T:                                                                                        \
        color = C;                                                                                 \
        glyph = G;                                                                                 \
        reversed = R;                                                                              \
        break;
                switch (tile) {
                    // We use reversed space to draw the tetriminos as some
                    // console fonts implement the block character
                    // incorrectly
                    CASE(Tetrimino::I, 96, true, " "); 
                    CASE(Tetrimino::J, 94, true, " ");
                    CASE(Tetrimino::L, 91, true, " ");
                    CASE(Tetrimino::O, 93, true, " ");
                    CASE(Tetrimino::S, 92, true, " ");
                    CASE(Tetrimino::T, 95, true, " ");
                    CASE(Tetrimino::Z, 31, true, " ");
                    CASE(Tetrimino::G, 97, true, " ");
                    CASE(border_v, 39, false, "│");
                    CASE(border_h, 39, false, "─");
                    CASE(border_tl, 39, false, "╭");
                    CASE(border_tr, 39, false, "╮");
                    CASE(border_bl, 39, false, "╰");
                    CASE(border_br, 39, false, "╯");
                default:
                    color = 39;
                    reversed = false;
                    glyph = (char)tile;
                    break;
                }
                if (color != current_color) {
                    std::cout << VT100::color(color);
                    current_color = color;
                }
                if (reversed != current_reversed) {
                    std::cout << VT100::reversed(reversed);
                    current_reversed = reversed;
                }
                std::cout << glyph;
            }
            std::cout << '\n';
            cursor_y++;
        }
        std::cout << std::flush;
    }

  protected:
    VT100 console;
    std::queue<Tetris::Input> inputs;
    Image<screen_width, screen_height> screen;
    Image<screen_width, screen_height> old_screen;
    int cursor_y;
    ssize_t last_frame_time;
    ssize_t last_sync_time;

    void draw_box(const Box &box, bool open_top = false) {
        int y = box.y;
        auto draw_line = [&](int left, int middle, int right) {
            for (int x = box.x; x < box.x + box.width; ++x) {
                screen[{x, y}] =
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

    void draw_text(const std::string &text, Point p, int width = 0) {
        int i = 0;
        Point q = p;
        while (i < text.size()) {
            for (; i < text.size(); ++i, ++q.x) {
                if (text[i] == '\n') {
                    i += 1;
                    goto next_line;
                }
                screen[q] = text[i];
            }
            for (; q.x < p.x + width; ++q.x) {
                screen[q] = ' ';
            }
        next_line:;
            q.x = p.x;
            q.y += 1;
        }
    }
};

#endif // __tetrino_cnosole_hpp__
