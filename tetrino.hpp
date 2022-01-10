#ifndef __tetris_hpp__
#define __tetris_hpp__

#include <algorithm>
#include <array>
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
        return _paste<true, OW, OH>(image, p, xscale, crop_top);
    }

    template <int OW, int OH>
    bool paste(Image<OW, OH> &image, Point p, int xscale = 1, int crop_top = 0) const {
        return _paste<false, OW, OH>(image, p, xscale, crop_top);
    }

    void rotate_clockwise(int _window) {
        assert(0 <= _window && _window <= width && _window <= height);
        for (int y = 0; y < _window; ++y) {
            for (int x = 0; x < y; ++x) {
                std::swap((*this)[{x, y}], (*this)[{y, x}]);
            }
        }
        for (int y = 0; y < _window; ++y) {
            for (int x = 0; x < _window / 2; ++x) {
                std::swap((*this)[{x, y}], (*this)[{_window - 1 - x, y}]);
            }
        }
    }

    bool occupied(Point p) const {
        if (p.x < 0 || p.x >= width || p.y < 0 || p.y >= height) return true;
        return (*this)[p] != ' ';
    }

  private:
    template <bool CM, int OW, int OH, class I>
    bool _paste(I &image, Point p, int xscale, int crop_top = 0) const {
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
        auto index = _index_from_type(type);
        this->data = Tetrimino::defaults.shape[index][rot].data;
    }

    void recolor(type_t value) {
        std::transform(begin(this->data), end(this->data), begin(this->data),
                       [=](int c) { return c ? value : 0; });
    }

    inline static const std::array<type_t, 7> all_types{I, L, O, T, J, Z, S};
    inline static constexpr int num_tetriminoes = all_types.size();

  private:
    inline static const int _index_from_type(type_t type) {
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

        void _make(type_t type, int _window, const char (&support)[size * size + 1]) {
            int index = _index_from_type(type);
            std::transform(
                std::begin(support), std::begin(support) + Tetrimino::size * Tetrimino::size,
                begin(shape[index][0].data), [=](char c) { return (c == ' ') ? 0 : (int)type; });
            for (int r = 1; r < 4; ++r) {
                shape[index][r].data = shape[index][r - 1].data;
                shape[index][r].rotate_clockwise(_window);
            }
        }

        Defaults() {
            _make(I, 4,
                  "    "
                  "####"
                  "    "
                  "    ");
            _make(J, 3,
                  "#   "
                  "### "
                  "    "
                  "    ");
            _make(L, 3,
                  "  # "
                  "### "
                  "    "
                  "    ");
            _make(O, 0,
                  " ## "
                  " ## "
                  "    "
                  "    ");
            _make(S, 3,
                  " ## "
                  "##  "
                  "    "
                  "    ");
            _make(T, 3,
                  " #  "
                  "### "
                  "    "
                  "    ");
            _make(Z, 3,
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
        : _rng{seed}, _alive{true}, _game_state{welcome}, _controller_state{}, _command_state{} {}

    void _set_level(int level) {
        _level = level;
        _normal_fall_period = (ssize_t)(1e6 * pow(0.8 - (_level - 1) * 0.0007, _level - 1));
        _short_fall_period = _normal_fall_period / 20;
    }

    ssize_t current_frame() const { return (_game_time + _frame_period - 1) / _frame_period; }

    void new_game(int level) {
        assert(1 <= level && level <= _max_level);
        _game_time = 0;
        _tally = 0;
        _num_lines_cleared = 0;
        _scheduled_drop_is_soft = false;

        _matrix.clear();

        _sample_next_block();
        _block = _next_block;
        _held_block.type = Tetrimino::none;
        _sample_next_block();

        _set_level(level);

        _can_hold = true;
        _repeat_translate_time = never;

        _respawn(0, _block);

        _game_state = play;
    }

    void _lock(ssize_t time) {
        _block.paste(_matrix, _block.pos);

        if (_block.pos.y < matrix_height - skyline) {
            _game_state = gameover;
        } else {
            _clear_rows();
            _can_hold = true;
            _block = _next_block;
            _sample_next_block();
            _respawn(time, _block);
        }
    }

    void _respawn(ssize_t time, Tetrimino &block) {
        block.pos = {.x = (matrix_width - Tetrimino::size) / 2, .y = matrix_height - skyline - 2};
        _fall_time = never;
        _lock_time = never;
        _scheduled_drop_is_soft = false;
        _lowest_y = block.pos.y;
        _num_moves_left = _max_num_moves;
        _last_move = normal;
        _back_to_back = 0;
    }

    bool _can_fall(const Tetrimino &block) const {
        return _block.can_paste(_matrix, block.pos + shift_down);
    }
    bool _can_fit(const Tetrimino &block) const { return block.can_paste(_matrix, block.pos); }
    int _drop(const Tetrimino &block) const {
        int y = block.pos.y, oky = y;
        for (; block.can_paste(_matrix, {block.pos.x, y}); oky = y++)
            ;
        return oky;
    }

    void _sample_next_block() {
        if (_queue.empty()) {
            auto _bag = Tetrimino::all_types;
            std::shuffle(begin(_bag), end(_bag), _rng);
            for (auto i : _bag) {
                _queue.push(i);
            }
        }
        _next_block = Tetrimino(_queue.front());
        _queue.pop();
    }

    bool tic(ssize_t time, std::queue<Input> &inputs) {
        using IN = Tetris::Input;

        _game_time += time;

        // Other screens.
        if (_game_state == welcome || _game_state == gameover) {
            while (!inputs.empty()) {
                auto key = inputs.front();
                inputs.pop();
                switch (key.value) {
                case Input::hard_drop: {
                    if (key.state == IN::released) break;
                    if (_game_state == welcome) {
                        new_game(1);
                    } else if (_game_state == gameover) {
                        _game_state = welcome;
                    } else {
                        // Ignore must have started already
                    }
                    break;
                }
                case Input::quit: _alive = false; break;
                default: break;
                }
            }
            return _alive;
        }

        // Run all events behind the current frame time.
        while (_game_state != gameover && _alive) {

            // After a successful move, apply extended locking rules.
            auto accept_move = [&, this](move_t type, int now) {
                if (_lock_time < never && _num_moves_left > 0) {
                    _num_moves_left--;
                    _lock_time = std::max(_lock_time, now + _lock_period);
                }
                _last_move = type;
            };

            // Translation move.
            auto translate = [&, this](int shift, int time) {
                if (_block.can_paste(_matrix, _block.pos + Point{shift, 0})) {
                    _block.pos += {shift, 0};
                    accept_move(normal, time);
                }
            };

            while (true) {

                ssize_t _input_time = inputs.empty() ? never : inputs.front().frame * _frame_period;
                ssize_t _current_time =
                    std::min({_repeat_translate_time, _lock_time, _fall_time, _input_time});
                if (_current_time > _game_time) goto done;

                // Repeat translate event
                if (_repeat_translate_time <= _current_time) {
                    translate(_command_state.right - _command_state.left, _repeat_translate_time);
                    _repeat_translate_time += _repeat_translate_period;
                }

                // Lockdown event
                else if (_lock_time <= _current_time) {
                    _lock(_lock_time);
                    if (_game_state == gameover) goto done;
                }

                // Fall event
                else if (_fall_time <= _current_time) {
                    _block.pos += shift_down;
                    if (_scheduled_drop_is_soft) _tally++;
                    if (_command_state.down) {
                        _scheduled_drop_is_soft = true;
                        _fall_time += _short_fall_period;
                    } else {
                        _scheduled_drop_is_soft = false;
                        _fall_time += _normal_fall_period;
                    }
                }

                // Input event
                else if (_input_time <= _current_time) {
                    auto input = inputs.front();
                    inputs.pop();
                    switch (input.value) {
                    case Input::quit:
                        if (input.state == IN::released) _alive = false;
                        break;

                    case Input::move_left:
                        _controller_state.left = _command_state.left = (input.state == IN::pressed);
                        _command_state.right =
                            (input.state == IN::released) && _controller_state.right;
                        goto translate_now;

                    case Input::move_right:
                        _controller_state.right = _command_state.right =
                            (input.state == IN::pressed);
                        _command_state.left =
                            (input.state == IN::released) && _controller_state.left;

                    translate_now:
                        if (_command_state.left || _command_state.right) {
                            translate(_command_state.right - _command_state.left, _input_time);
                            _repeat_translate_time = _input_time + _repeat_translate_grace_period;
                        } else {
                            _repeat_translate_time = never;
                        }
                        break;

                    case Input::rotate_left:
                    case Input::rotate_right: {
                        if (input.state == IN::released) break;
                        int dr = (input.value == Input::rotate_left) ? -1 : 1;
                        const auto &kicks =
                            wall_kicks[_block.type == Tetrimino::I][dr > 0][_block.rot];
                        _block.rotate((_block.rot + dr) & 3);
                        for (size_t k = 0; k < sizeof(kicks) / sizeof(kicks[0]); ++k) {
                            if (_block.can_paste(_matrix, _block.pos + kicks[k])) {
                                _block.pos += kicks[k];
                                // Check for T-Spin and Mini T-Spin.
                                move_t type = normal;
                                if (_block.type == Tetrimino::T) {
                                    const auto &pts = tspin_corners[_block.rot];
                                    auto A = _matrix.occupied(_block.pos + pts[0]);
                                    auto B = _matrix.occupied(_block.pos + pts[1]);
                                    auto C = _matrix.occupied(_block.pos + pts[2]);
                                    auto D = _matrix.occupied(_block.pos + pts[3]);
                                    if (k == 4) {
                                        type = tspin;
                                    } else if ((A && B) && (C || D)) {
                                        type = tspin;
                                    } else if ((A || B) && (C && D)) {
                                        type = mini_tspin;
                                    }
                                }
                                accept_move(type, _input_time);
                                goto accepted;
                            }
                        }
                        _block.rotate((_block.rot - dr) & 3);
                    accepted:
                        break;
                    }

                    case Input::hard_drop: {
                        if (input.state == IN::released) break;
                        int y = _drop(_block);
                        _tally += 2 * (y - _block.pos.y);
                        _block.pos.y = y;
                        _lock(_input_time);
                        if (_game_state == gameover) goto done;
                        break;
                    }

                    case Input::soft_drop: {
                        _command_state.down = (input.state == IN::pressed);
                        if (_command_state.down) {
                            _fall_time = _input_time;
                            _scheduled_drop_is_soft = true;
                        } else {
                            // Cancel a previously-scheduled soft drop.
                            _fall_time += _normal_fall_period - _short_fall_period;
                            _scheduled_drop_is_soft = false;
                        }
                        break;
                    }

                    case Input::hold: {
                        if (input.state == IN::released) break;
                        if (_can_hold) {
                            _can_hold = false;
                            if (_held_block.type != Tetrimino::none) {
                                std::swap(_held_block, _block);
                            } else {
                                _held_block = _block;
                                _block = _next_block;
                                _sample_next_block();
                            }
                            _respawn(_input_time, _block);
                        }
                        break;
                    }

                    default: break;
                    }
                }

                if (_can_fall(_block)) {
                    // If the block is not supported by a surface, begin or continue falling and
                    // cancel locking.
                    _fall_time = std::min(_fall_time, _current_time + _normal_fall_period);
                    _lock_time = never;
                } else {
                    // If the block is supported by a surface, begin or contiue locking and cancel
                    // falling.
                    _lock_time = std::min(_lock_time, _current_time + _lock_period);
                    _fall_time = never;
                    _scheduled_drop_is_soft = false;
                };

                // Extended locking: lowering the block resets the number of moves left.
                if (_block.pos.y > _lowest_y) {
                    _lowest_y = _block.pos.y;
                    _num_moves_left = _max_num_moves;
                }
            }
        }

    done:
        _ghost_block = _block;
        _ghost_block.pos.y = _drop(_block);
        _ghost_block.recolor(Tetrimino::G);
        _ghost_block.type = _can_fit(_ghost_block) ? Tetrimino::G : Tetrimino::none;

        return _alive;
    }

  protected:
    Image<matrix_width, matrix_height> _matrix;
    Tetrimino _block;
    Tetrimino _next_block;
    Tetrimino _ghost_block;
    Tetrimino _held_block;
    std::queue<Tetrimino::type_t> _queue;
    bool _alive;
    int _tally;
    int _num_lines_cleared;
    static constexpr int _max_level = 15;
    int _level;
    bool _can_hold;
    int _lowest_y;
    int _scheduled_drop_is_soft;
    enum move_t { tspin, mini_tspin, normal } _last_move;
    int _back_to_back;
    std::mt19937 _rng;
    enum { welcome, gameover, play } _game_state;
    struct {
        bool left : 1;
        bool right : 1;
    } _controller_state;
    struct {
        bool left : 1;
        bool right : 1;
        bool down : 1;
    } _command_state;

    // times in us
    ssize_t _game_time;
    ssize_t _lock_time;
    ssize_t _fall_time;
    ssize_t _repeat_translate_time;

    static constexpr int _max_num_moves = 15;
    int _num_moves_left;

    ssize_t _normal_fall_period;
    ssize_t _short_fall_period;
    static constexpr ssize_t _lock_period = 500'000;
    static constexpr ssize_t _frame_period = 16'666;
    static constexpr ssize_t _repeat_translate_period = 30'000;
    static constexpr ssize_t _repeat_translate_grace_period = 500'000;

    std::vector<std::string> _messages;

    void _clear_rows() {
        // Find which rows to keep
        int num_kept = 0;
        std::array<int, matrix_height> copy_to;
        std::fill(begin(copy_to), end(copy_to), -1);
        for (int y = matrix_height - 1; y >= 0; --y) {
            const auto &row = _matrix[y];
            if (any_of(begin(row), end(row), [](char c) { return c == ' '; })) {
                copy_to[y] = matrix_height - 1 - num_kept++;
            }
        }

        // Update score
        int num_cleared = matrix_height - num_kept;
        _num_lines_cleared += num_cleared;
        int score = 0;
        std::string msg;
        int bb = _back_to_back;

#undef CASE
#define CASE(N, M, S, B)                                                                           \
    case N:                                                                                        \
        msg = M;                                                                                   \
        score = S;                                                                                 \
        bb B;                                                                                      \
        break;

        switch (_last_move) {
        case normal:
            switch (num_cleared) {
                CASE(1, "Single", 100, = 0);
                CASE(2, "Double", 300, = 0);
                CASE(3, "Triple", 500, = 0);
                CASE(4, "Tetris", 800, += 1);
            }
            break;
        case mini_tspin:
            switch (num_cleared) {
                CASE(0, "Mini T-Spin", 100, );
                CASE(1, "Mini T-Spin Single", 200, += 1);
            default: assert(false);
            }
            break;
        case tspin:
            switch (num_cleared) {
                CASE(0, "T-Spin", 400, );
                CASE(1, "T-Spin Single", 800, += 1);
                CASE(2, "T-Spin Double", 1200, += 1);
                CASE(3, "T-Spin Triple", 1600, += 1);
            case 4: assert(false);
            }
            break;
        }
        score *= _level;
        if (bb > _back_to_back && _back_to_back >= 1) {
            score += score / 2;
            msg += " B2B";
        }
        _back_to_back = bb;
        if (score > 0) {
            _messages.push_back(msg + " " + std::to_string(score));
            _tally += score;
        }

        _set_level(std::min(1 + (_num_lines_cleared / 10), _max_level));

        // Erase rows
        for (int y = matrix_height - 1; y >= 0; --y) {
            int z = copy_to[y];
            if (z >= 0 && z != y) {
                copy(begin(_matrix[y]), end(_matrix[y]), begin(_matrix[z]));
            }
        }
        for (int z = matrix_height - 1 - num_kept; z >= 0; --z) {
            fill(begin(_matrix[z]), end(_matrix[z]), ' ');
        }
    }
};

#endif // __tetris_hpp__
