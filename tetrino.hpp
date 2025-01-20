#ifndef __tetris_hpp__
#define __tetris_hpp__

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <iostream>
#include <limits>
#include <queue>
#include <random>
#include <span>

struct Point {
    int x;
    int y;
    constexpr Point operator+(Point p) const { return {x + p.x, y + p.y}; }
    constexpr Point operator-(Point p) const { return {x - p.x, y - p.y}; }
    Point &operator+=(Point p) {
        *this = *this + p;
        return *this;
    }
    Point operator*(int s) const { return {x * s, y * s}; }
};

constexpr Point shift_down{0, 1};
constexpr Point shift_left{-1, 0};
constexpr Point shift_right{1, 0};

template <int W, int H = W> class Image {
  public:
    static constexpr int width = W;
    static constexpr int height = H;
    std::array<int, width * height> data;

    Image() { clear(); }

    int &operator[](Point p) { return data[p.x + p.y * width]; }
    const int &operator[](Point p) const { return data[p.x + p.y * width]; }

    std::span<int, width> operator[](int y) {
        return std::span{data}.subspan(y * width).template first<width>();
    }

    std::span<const int, width> operator[](int y) const {
        return std::span{data}.subspan(y * width).template first<width>();
    }

    void clear(int value = ' ') { data.fill(value); }

    template <int OW, int OH>
    bool can_paste(const Image<OW, OH> &image, Point p, int xscale = 1, int crop_top = 0) const {
        return paste<true, OW, OH>(image, p, xscale, crop_top);
    }

    template <int OW, int OH>
    bool paste(Image<OW, OH> &image, Point p, int xscale = 1, int crop_top = 0) const {
        return paste<false, OW, OH>(image, p, xscale, crop_top);
    }

    void rotate_clockwise(int window) {
        assert(0 <= window && window <= width && window <= height);
        for (int y = 0; y < window; ++y) {
            for (int x = 0; x < y; ++x) {
                std::swap((*this)[{x, y}], (*this)[{y, x}]);
            }
        }
        for (int y = 0; y < window; ++y) {
            for (int x = 0; x < window / 2; ++x) {
                std::swap((*this)[{x, y}], (*this)[{window - 1 - x, y}]);
            }
        }
    }

    bool occupied(Point p) const {
        if (p.x < 0 || p.x >= width || p.y < 0 || p.y >= height) return true;
        return (*this)[p] != ' ';
    }

  private:
    template <bool CM, int OW, int OH, class I>
    bool paste(I &image, Point p, int xscale, int crop_top = 0) const {
        for (int iy = crop_top; iy < height; iy++) {
            for (int ix = 0; ix < width * xscale; ix++) {
                int c = (*this)[{ix / xscale, iy}];
                if (c == 0) continue;
                auto o = p + Point{ix, iy - crop_top};
                bool inside = (0 <= o.x) && (o.x < OW) && (0 <= o.y) && (o.y < OH);
                if constexpr (CM) {
                    if (!inside || image[o] != ' ') return false;
                } else {
                    if (inside) image[o] = c;
                }
            }
        }
        return true;
    }
};

class Tetrimino : public Image<4> {
  public:
    enum type_t { none = 0, I = 256, L, O, T, J, Z, S, G } type;
    Point pos;
    int rot;

    static constexpr int size = 4;

    Tetrimino(type_t type = I) : type{type}, pos{0, 0} { rotate(0); }

    void rotate(int r) {
        rot = r;
        auto index = index_from_type(type);
        this->data = Tetrimino::defaults.shape[index][rot].data;
    }

    void recolor(type_t value) {
        std::transform(begin(this->data), end(this->data), begin(this->data),
                       [=](int c) { return c ? value : 0; });
    }

    inline static const std::array<type_t, 7> all_types{I, L, O, T, J, Z, S};
    inline static constexpr int num_tetriminoes = all_types.size();

  private:
    inline static const int index_from_type(type_t type) {
        switch (type) {
        case I: return 0;
        case L: return 1;
        case O: return 2;
        case T: return 3;
        case J: return 4;
        case Z: return 5;
        case S: return 6;
        case G: return 7;
        default: return -1;
        }
    }

    inline static const struct Defaults {
        std::array<std::array<Image<size>, 4>, num_tetriminoes> shape;

        void make(type_t type, int window, const char (&support)[size * size + 1]) {
            int index = index_from_type(type);
            std::transform(
                std::begin(support), std::begin(support) + Tetrimino::size * Tetrimino::size,
                begin(shape[index][0].data), [=](char c) { return (c == ' ') ? 0 : (int)type; });
            for (int r = 1; r < 4; ++r) {
                shape[index][r].data = shape[index][r - 1].data;
                shape[index][r].rotate_clockwise(window);
            }
        }

        Defaults() {
            make(I, 4,
                 "    "
                 "####"
                 "    "
                 "    ");
            make(J, 3,
                 "#   "
                 "### "
                 "    "
                 "    ");
            make(L, 3,
                 "  # "
                 "### "
                 "    "
                 "    ");
            make(O, 0,
                 " ## "
                 " ## "
                 "    "
                 "    ");
            make(S, 3,
                 " ## "
                 "##  "
                 "    "
                 "    ");
            make(T, 3,
                 " #  "
                 "### "
                 "    "
                 "    ");
            make(Z, 3,
                 "##  "
                 " ## "
                 "    "
                 "    ");
        }
    } defaults;
};

// [type (other or I)][direction (L or R)][base rotation][kick number]
Point wall_kicks[2][2][4][5] = {
    // J, L, T, S, Z
    {
        {{{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}},      // 0>>3
         {{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}},     // 1>>0
         {{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}},   // 2>>1
         {{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}}}, // 3>>2

        {{{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}},   // 0>>1
         {{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}},     // 1>>2
         {{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}},      // 2>>3
         {{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}}}, // 3>>0
    },
    // I
    {
        {{{0, 0}, {-1, 0}, {2, 0}, {-1, -2}, {2, 1}},  // 0>>3
         {{0, 0}, {2, 0}, {-1, 0}, {2, -1}, {-1, 2}},  // 1>>0
         {{0, 0}, {1, 0}, {-2, 0}, {1, 2}, {-2, -1}},  // 2>>1
         {{0, 0}, {-2, 0}, {1, 0}, {-2, 1}, {1, -2}}}, // 3>>2

        {{{0, 0}, {-2, 0}, {1, 0}, {-2, 1}, {1, -2}},  // 0>>1
         {{0, 0}, {-1, 0}, {2, 0}, {-1, -2}, {2, 1}},  // 1>>2
         {{0, 0}, {2, 0}, {-1, 0}, {2, -1}, {-1, 2}},  // 2>>3
         {{0, 0}, {1, 0}, {-2, 0}, {1, 2}, {-2, -1}}}, // 3>>0
    }};

//   "A#B "
//   "### "
//   "C D "
//   "    "

Point tspin_corners[4][4] = {
    // A B C D
    {{0, 0}, {2, 0}, {0, 2}, {2, 2}},
    {{0, 2}, {2, 2}, {0, 0}, {2, 0}},
    {{2, 0}, {2, 2}, {0, 0}, {0, 2}},
    {{0, 0}, {0, 2}, {2, 0}, {2, 2}},
};

static constexpr ssize_t never = std::numeric_limits<ssize_t>::max() / 2;

class Tetris {
  public:
    static constexpr int matrix_width = 10;
    static constexpr int matrix_height = 40;
    static constexpr int skyline = 20;

    struct Input {
        enum value_t {
            rotate_left,
            rotate_right,
            move_left,
            move_right,
            hard_drop,
            soft_drop,
            hold,
            quit
        } value;
        enum state_t { pressed, released } state;
        ssize_t frame;
    };

    Tetris(unsigned int seed = 0)
        : rng{seed}, alive{true}, game_state{WELCOME}, controller_state{}, command_state{} {}

    void set_level(int level) {
        level = level;
        normal_fall_period = (ssize_t)(1e6 * pow(0.8 - (level - 1) * 0.0007, level - 1));
        short_fall_period = normal_fall_period / 20;
    }

    ssize_t current_frame() const { return (game_time + frame_period - 1) / frame_period; }

    void new_game(int level) {
        assert(1 <= level && level <= max_level);
        game_time = 0;
        tally = 0;
        num_lines_cleared = 0;
        scheduled_drop_is_soft = false;

        matrix.clear();

        sample_next_block();
        block = next_block;
        held_block.type = Tetrimino::none;
        sample_next_block();

        set_level(level);

        can_hold = true;
        repeat_translate_time = never;

        respawn(0, block);

        game_state = PLAY;
    }

    void lock(ssize_t time) {
        block.paste(matrix, block.pos);

        if (block.pos.y < matrix_height - skyline) {
            game_state = GAME_OVER;
        } else {
            clear_rows();
            can_hold = true;
            block = next_block;
            sample_next_block();
            respawn(time, block);
        }
    }

    void respawn(ssize_t time, Tetrimino &block) {
        block.pos = {.x = (matrix_width - Tetrimino::size) / 2, .y = matrix_height - skyline - 2};
        fall_time = never;
        lock_time = never;
        scheduled_drop_is_soft = false;
        lowest_y = block.pos.y;
        num_moves_left = max_num_moves;
        last_move = NORMAL;
        back_to_back = 0;
    }

    bool can_fall(const Tetrimino &block) const {
        return block.can_paste(matrix, block.pos + shift_down);
    }
    bool can_fit(const Tetrimino &block) const { return block.can_paste(matrix, block.pos); }
    int drop(const Tetrimino &block) const {
        int y = block.pos.y, oky = y;
        for (; block.can_paste(matrix, {block.pos.x, y}); oky = y++)
            ;
        return oky;
    }

    void sample_next_block() {
        if (queue.empty()) {
            auto bag = Tetrimino::all_types;
            std::shuffle(begin(bag), end(bag), rng);
            for (auto i : bag) {
                queue.push(i);
            }
        }
        next_block = Tetrimino(queue.front());
        queue.pop();
    }

    bool tic(ssize_t time, std::queue<Input> &inputs) {
        using IN = Tetris::Input;

        game_time += time;

        // Other screens.
        if (game_state == WELCOME || game_state == GAME_OVER) {
            while (!inputs.empty()) {
                auto key = inputs.front();
                inputs.pop();
                switch (key.value) {
                case Input::hard_drop: {
                    if (key.state == IN::released) break;
                    if (game_state == WELCOME) {
                        new_game(1);
                    } else if (game_state == GAME_OVER) {
                        game_state = WELCOME;
                    } else {
                        // Ignore must have started already
                    }
                    break;
                }
                case Input::quit: alive = false; break;
                default: break;
                }
            }
            return alive;
        }

        // Run all events behind the current frame time.
        while (game_state != GAME_OVER && alive) {

            // After a successful move, apply extended locking rules.
            auto accept_move = [&, this](move_t type, int now) {
                if (lock_time < never && num_moves_left > 0) {
                    num_moves_left--;
                    lock_time = std::max(lock_time, now + lock_period);
                }
                last_move = type;
            };

            // Translation move.
            auto translate = [&, this](int shift, int time) {
                if (block.can_paste(matrix, block.pos + Point{shift, 0})) {
                    block.pos += {shift, 0};
                    accept_move(NORMAL, time);
                }
            };

            while (true) {

                ssize_t input_time = inputs.empty() ? never : inputs.front().frame * frame_period;
                ssize_t current_time =
                    std::min({repeat_translate_time, lock_time, fall_time, input_time});
                if (current_time > game_time) goto done;

                // Repeat translate event
                if (repeat_translate_time <= current_time) {
                    translate(command_state.right - command_state.left, repeat_translate_time);
                    repeat_translate_time += repeat_translate_period;
                }

                // Lockdown event
                else if (lock_time <= current_time) {
                    lock(lock_time);
                    if (game_state == GAME_OVER) goto done;
                }

                // Fall event
                else if (fall_time <= current_time) {
                    block.pos += shift_down;
                    if (scheduled_drop_is_soft) tally++;
                    if (command_state.down) {
                        scheduled_drop_is_soft = true;
                        fall_time += short_fall_period;
                    } else {
                        scheduled_drop_is_soft = false;
                        fall_time += normal_fall_period;
                    }
                }

                // Input event
                else if (input_time <= current_time) {
                    auto input = inputs.front();
                    inputs.pop();
                    switch (input.value) {
                    case Input::quit:
                        if (input.state == IN::released) alive = false;
                        break;

                    case Input::move_left:
                        controller_state.left = command_state.left = (input.state == IN::pressed);
                        command_state.right =
                            (input.state == IN::released) && controller_state.right;
                        goto translate_now;

                    case Input::move_right:
                        controller_state.right = command_state.right = (input.state == IN::pressed);
                        command_state.left = (input.state == IN::released) && controller_state.left;

                    translate_now:
                        if (command_state.left || command_state.right) {
                            translate(command_state.right - command_state.left, input_time);
                            repeat_translate_time = input_time + repeat_translate_grace_period;
                        } else {
                            repeat_translate_time = never;
                        }
                        break;

                    case Input::rotate_left:
                    case Input::rotate_right: {
                        if (input.state == IN::released) break;
                        int dr = (input.value == Input::rotate_left) ? -1 : 1;
                        const auto &kicks =
                            wall_kicks[block.type == Tetrimino::I][dr > 0][block.rot];
                        block.rotate((block.rot + dr) & 3);
                        for (size_t k = 0; k < sizeof(kicks) / sizeof(kicks[0]); ++k) {
                            if (block.can_paste(matrix, block.pos + kicks[k])) {
                                block.pos += kicks[k];
                                // Check for T-Spin and Mini T-Spin.
                                move_t type = NORMAL;
                                if (block.type == Tetrimino::T) {
                                    const auto &pts = tspin_corners[block.rot];
                                    auto A = matrix.occupied(block.pos + pts[0]);
                                    auto B = matrix.occupied(block.pos + pts[1]);
                                    auto C = matrix.occupied(block.pos + pts[2]);
                                    auto D = matrix.occupied(block.pos + pts[3]);
                                    if (k == 4) {
                                        type = TSPIN;
                                    } else if ((A && B) && (C || D)) {
                                        type = TSPIN;
                                    } else if ((A || B) && (C && D)) {
                                        type = MINI_TSPIN;
                                    }
                                }
                                accept_move(type, input_time);
                                goto accepted;
                            }
                        }
                        block.rotate((block.rot - dr) & 3);
                    accepted:
                        break;
                    }

                    case Input::hard_drop: {
                        if (input.state == IN::released) break;
                        int y = drop(block);
                        tally += 2 * (y - block.pos.y);
                        block.pos.y = y;
                        lock(input_time);
                        if (game_state == GAME_OVER) goto done;
                        break;
                    }

                    case Input::soft_drop: {
                        command_state.down = (input.state == IN::pressed);
                        if (command_state.down) {
                            fall_time = input_time;
                            scheduled_drop_is_soft = true;
                        } else {
                            // Cancel a previously-scheduled soft drop.
                            fall_time += normal_fall_period - short_fall_period;
                            scheduled_drop_is_soft = false;
                        }
                        break;
                    }

                    case Input::hold: {
                        if (input.state == IN::released) break;
                        if (can_hold) {
                            can_hold = false;
                            if (held_block.type != Tetrimino::none) {
                                std::swap(held_block, block);
                            } else {
                                held_block = block;
                                block = next_block;
                                sample_next_block();
                            }
                            respawn(input_time, block);
                        }
                        break;
                    }

                    default: break;
                    }
                }

                if (can_fall(block)) {
                    // If the block is not supported by a surface, begin or continue falling and
                    // cancel locking.
                    fall_time = std::min(fall_time, current_time + normal_fall_period);
                    lock_time = never;
                } else {
                    // If the block is supported by a surface, begin or contiue locking and cancel
                    // falling.
                    lock_time = std::min(lock_time, current_time + lock_period);
                    fall_time = never;
                    scheduled_drop_is_soft = false;
                };

                // Extended locking: lowering the block resets the number of moves left.
                if (block.pos.y > lowest_y) {
                    lowest_y = block.pos.y;
                    num_moves_left = max_num_moves;
                }
            }
        }

    done:
        ghost_block = block;
        ghost_block.pos.y = drop(block);
        ghost_block.recolor(Tetrimino::G);
        ghost_block.type = can_fit(ghost_block) ? Tetrimino::G : Tetrimino::none;

        return alive;
    }

  protected:
    Image<matrix_width, matrix_height> matrix;
    Tetrimino block;
    Tetrimino next_block;
    Tetrimino ghost_block;
    Tetrimino held_block;
    std::queue<Tetrimino::type_t> queue;
    bool alive;
    int tally;
    int num_lines_cleared;
    static constexpr int max_level = 15;
    int level;
    bool can_hold;
    int lowest_y;
    int scheduled_drop_is_soft;
    enum move_t { TSPIN, MINI_TSPIN, NORMAL } last_move;
    int back_to_back;
    std::mt19937 rng;
    enum { WELCOME, GAME_OVER, PLAY } game_state;
    struct {
        bool left : 1;
        bool right : 1;
    } controller_state;
    struct {
        bool left : 1;
        bool right : 1;
        bool down : 1;
    } command_state;

    // times in us
    ssize_t game_time;
    ssize_t lock_time;
    ssize_t fall_time;
    ssize_t repeat_translate_time;

    static constexpr int max_num_moves = 15;
    int num_moves_left;

    ssize_t normal_fall_period;
    ssize_t short_fall_period;
    static constexpr ssize_t lock_period = 500'000;
    static constexpr ssize_t frame_period = 16'666;
    static constexpr ssize_t repeat_translate_period = 30'000;
    static constexpr ssize_t repeat_translate_grace_period = 500'000;

    std::vector<std::string> messages;

    void clear_rows() {
        // Find which rows to keep
        int num_kept = 0;
        std::array<int, matrix_height> copy_to;
        std::fill(begin(copy_to), end(copy_to), -1);
        for (int y = matrix_height - 1; y >= 0; --y) {
            const auto &row = matrix[y];
            if (any_of(begin(row), end(row), [](char c) { return c == ' '; })) {
                copy_to[y] = matrix_height - 1 - num_kept++;
            }
        }

        // Update score
        int num_cleared = matrix_height - num_kept;
        num_lines_cleared += num_cleared;
        int score = 0;
        std::string msg;
        int bb = back_to_back;

#undef CASE
#define CASE(N, M, S, B)                                                                           \
    case N:                                                                                        \
        msg = M;                                                                                   \
        score = S;                                                                                 \
        bb B;                                                                                      \
        break;

        switch (last_move) {
        case NORMAL:
            switch (num_cleared) {
                CASE(1, "Single", 100, = 0);
                CASE(2, "Double", 300, = 0);
                CASE(3, "Triple", 500, = 0);
                CASE(4, "Tetris", 800, += 1);
            }
            break;
        case MINI_TSPIN:
            switch (num_cleared) {
                CASE(0, "Mini T-Spin", 100, );
                CASE(1, "Mini T-Spin Single", 200, += 1);
            default: assert(false);
            }
            break;
        case TSPIN:
            switch (num_cleared) {
                CASE(0, "T-Spin", 400, );
                CASE(1, "T-Spin Single", 800, += 1);
                CASE(2, "T-Spin Double", 1200, += 1);
                CASE(3, "T-Spin Triple", 1600, += 1);
            case 4: assert(false);
            }
            break;
        }
        score *= level;
        if (bb > back_to_back && back_to_back >= 1) {
            score += score / 2;
            msg += " B2B";
        }
        back_to_back = bb;
        if (score > 0) {
            messages.push_back(msg + " " + std::to_string(score));
            tally += score;
        }

        set_level(std::min(1 + (num_lines_cleared / 10), max_level));

        // Erase rows
        for (int y = matrix_height - 1; y >= 0; --y) {
            int z = copy_to[y];
            if (z >= 0 && z != y) {
                copy(begin(matrix[y]), end(matrix[y]), begin(matrix[z]));
            }
        }
        for (int z = matrix_height - 1 - num_kept; z >= 0; --z) {
            fill(begin(matrix[z]), end(matrix[z]), ' ');
        }
    }
};

#endif // __tetris_hpp__
