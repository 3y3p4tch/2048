#include <array>
#include <cstdint>
#include <random>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <limits>
#include <locale>

extern "C" {
#include <ncursesw/curses.h>
}

namespace {
    /**
     * @brief 32-bit Xorshift PRNG for faster execution
     */
    struct Xorshift {
        uint32_t state;
        Xorshift(uint32_t seed) : state(seed == 0 ? 1 : seed) {}
        uint32_t operator()()
        {
            state ^= (state << 13);
            state ^= (state >> 17);
            return (state ^= (state << 15));
        }
    };

    enum class direction { up = 0b00, down = 0b01, left = 0b10, right = 0b11 };

    struct Dimensions {
        uint16_t height;
        uint16_t width;
        constexpr Dimensions(uint16_t height, uint16_t width) : height{height}, width{width} {}
    };

    /**
     * @brief Game Grid
     *
     * @tparam size Size of the square grid, <= 40
     */
    template <unsigned short size>
    struct Grid {
        enum class State { running, ended };

        Grid(uint32_t seed) : prng_(seed), empty_cells_count_(size * size), score_{0}, state_{State::running}, grid_{}
        {
            this->spawn_new_number();
        }
        void move(direction dir)
        {
            if (__builtin_expect(this->empty_cells_count_ == 0, 0)) {
                state_ = State::ended;
                return;
            }
            if (move_helper(dir)) spawn_new_number();
        }
        unsigned int score() const { return this->score_; }
        State state() const { return this->state_; }
        void reset()
        {
            std::fill(grid_.begin(), grid_.end(), 0);
            empty_cells_count_ = size * size;
            this->spawn_new_number();
        }
        const std::array<uint8_t, size * size> &data() const { return this->grid_; }

    private:
        void spawn_new_number()
        {
            unsigned short spawn_loc = prng_() % this->empty_cells_count_;

            for (uint8_t &x : grid_) {
                if (spawn_loc == 0 && x == 0) {
                    // 25% probability of 2, 75% probability of 4
                    x = ((prng_() / 4) == 0) + 1;
                    this->empty_cells_count_--;
                    return;
                }
                if (x == 0) spawn_loc--;
            }
            __builtin_unreachable();
        }

        bool move_helper(direction dir)
        {
            bool modified = false;
            const auto dir_integral = static_cast<std::underlying_type_t<direction>>(dir);
            const auto top_start = grid_.begin() + ((size * size - 1) * (dir_integral & 1));
            auto top = top_start;
            short top_stride, it_stride;
            switch (dir) {
            case direction::up:
                top_stride = 1;
                it_stride = size;
                break;
            case direction::down:
                top_stride = -1;
                it_stride = -size;
                break;
            case direction::left:
                top_stride = size;
                it_stride = 1;
                break;
            case direction::right:
                top_stride = -size;
                it_stride = -1;
                break;
            }

            for (unsigned short i = 0; i < size; i++, top = top_start + i * top_stride) {
                auto it = top + it_stride;
                for (unsigned short j = 1; j < size; j++, it += it_stride) {
                    if (*it == 0)
                        continue;
                    else if (*top == 0) {
                        std::iter_swap(it, top);
                        modified = true;
                    }
                    else if (*it == *top) {
                        *it = 0;
                        this->empty_cells_count_++;
                        this->score_ += 1 << *top;
                        (*top)++;
                        top += it_stride;
                        modified = true;
                    }
                    else {
                        top += it_stride;
                        if (top != it) {
                            std::iter_swap(it, top);
                            modified = true;
                        }
                    }
                }
            }
            return modified;
        }

    private:
        Xorshift prng_;
        unsigned short empty_cells_count_;
        unsigned int score_;
        State state_;
        std::array<uint8_t, size * size> grid_;
    };

    /**
     * @brief Terminal User Interface for the Game (uses ncurses)
     *
     */
    class TUI {
    public:
        TUI() : grid_(std::random_device{}())
        {
            std::locale::global(std::locale("en_IN.UTF-8")); // set locale to UTF8, so that unicode works
            initscr();
            noecho();             // don't echo during getch
            cbreak();             // disable bufferring
            keypad(stdscr, true); // enable arrow keys for movement
            curs_set(0);          // hide the cursor from screen
            colors_ = false;
            if (has_colors()) {
                colors_ = true;
                start_color();
                init_pair(1, COLOR_WHITE, COLOR_RED);
                init_pair(2, COLOR_WHITE, COLOR_BLACK);
            }
            refresh();

            getmaxyx(stdscr, this->y_max_, this->x_max_);
            gameboard_ = newwin(gameboard_dims.height, gameboard_dims.width, y_max_ / 2 - gameboard_dims.height / 2,
                                x_max_ / 2 - gameboard_dims.width / 2);
            this->draw_skeleton();
            this->draw_grid();
            wrefresh(gameboard_);
        }
        void mainloop()
        {
            while (true) {
                switch (getch()) {
                case KEY_UP:
                    grid_.move(direction::up);
                    break;
                case KEY_DOWN:
                    grid_.move(direction::down);
                    break;
                case KEY_LEFT:
                    grid_.move(direction::left);
                    break;
                case KEY_RIGHT:
                    grid_.move(direction::right);
                    break;
                default:
                    break;
                }
                this->draw_grid();
                wrefresh(gameboard_);
                refresh();
            }
        }
        void benchmark() {
            using std::chrono::duration_cast;
            using std::chrono::seconds;
            using std::chrono::steady_clock;
            using namespace std::chrono_literals;

            std::ofstream fh("temp.txt", std::ofstream::out);

            const auto start = steady_clock::now();
            unsigned long long count = 0;
            Xorshift rand(1);
            while (duration_cast<seconds>(steady_clock::now() - start) < 1s) {
                grid_.move(static_cast<direction>(rand() % 4));
                this->draw_grid();
                wrefresh(gameboard_);
                count++;
                if (grid_.state() == Grid<grid_complexity>::State::ended)
                    grid_.reset();
            }
            fh << count << std::endl;
        }
        ~TUI() { endwin(); }

    private:
        void draw_grid()
        {
            const auto grid_data = grid_.data();

            for (auto r = 0; r < grid_complexity; r++) {
                for (auto c = 0; c < grid_complexity; c++) {
                    const auto val = grid_data[r * grid_complexity + c];
                    if (colors_) {
                        wattron(gameboard_, val ? COLOR_PAIR(1) : COLOR_PAIR(2));
                        for (auto i = 0; i < cell_size.height; i++) {
                            wmove(gameboard_, r * (cell_size.height + 1) + 1 + i, c * (cell_size.width + 1) + 1);
                            for (auto j = 0; j < cell_size.width; j++) {
                                waddch(gameboard_, ' ');
                            }
                        }
                        wattroff(gameboard_, val ? COLOR_PAIR(1) : COLOR_PAIR(2));
                    }
                    if (val) {
                        if (colors_) wattron(gameboard_, val ? COLOR_PAIR(1) : COLOR_PAIR(2));
                        const uint8_t val_width = snprintf(nullptr, 0, "%d", 1 << val);
                        mvwprintw(gameboard_, r * (cell_size.height + 1) + (cell_size.height + 1) / 2,
                                  c * (cell_size.width + 1) + (cell_size.width - val_width) / 2 + 1, "%d", 1 << val);
                        if (colors_) wattroff(gameboard_, val ? COLOR_PAIR(1) : COLOR_PAIR(2));
                    }
                }
            }
        }
        void draw_skeleton()
        {
            auto top = []() {
                std::array<cchar_t, gameboard_dims.width> array = {};
                auto it = array.begin();
                *it = *WACS_T_ULCORNER;
                it++;
                for (auto i = 0; i < grid_complexity; i++) {
                    for (auto j = 0; j < cell_size.width; j++) {
                        *it = *WACS_T_HLINE;
                        it++;
                    }
                    *it = *WACS_T_TTEE;
                    it++;
                }
                array.back() = *WACS_T_URCORNER;
                return array;
            }();
            auto middle = []() {
                std::array<cchar_t, gameboard_dims.width> array = {};
                auto it = array.begin();
                *it = *WACS_T_LTEE;
                it++;
                for (auto i = 0; i < grid_complexity; i++) {
                    for (auto j = 0; j < cell_size.width; j++) {
                        *it = *WACS_T_HLINE;
                        it++;
                    }
                    *it = *WACS_T_PLUS;
                    it++;
                }
                array.back() = *WACS_T_RTEE;
                return array;
            }();
            auto bottom = []() {
                std::array<cchar_t, gameboard_dims.width> array = {};
                auto it = array.begin();
                *it = *WACS_T_LLCORNER;
                it++;
                for (auto i = 0; i < grid_complexity; i++) {
                    for (auto j = 0; j < cell_size.width; j++) {
                        *it = *WACS_T_HLINE;
                        it++;
                    }
                    *it = *WACS_T_BTEE;
                    it++;
                }
                array.back() = *WACS_T_LRCORNER;
                return array;
            }();
            // top
            wadd_wchstr(gameboard_, top.data());
            // vertical bars
            for (auto i = 0; i < grid_complexity; i++) {
                for (auto j = 0; j < grid_complexity + 1; j++) {
                    for (auto k = 0; k < cell_size.height; k++) {
                        mvwadd_wch(gameboard_, i * (cell_size.height + 1) + 1 + k, j * (cell_size.width + 1),
                                   WACS_T_VLINE);
                    }
                }
            }
            // middles
            for (auto i = 1; i < grid_complexity; i++) {
                mvwadd_wchstr(gameboard_, i * (cell_size.height + 1), 0, middle.data());
            }
            // bottom
            mvwadd_wchstr(gameboard_, gameboard_dims.height - 1, 0, bottom.data());
        }

        static constexpr uint8_t grid_complexity = 5;
        static constexpr Dimensions cell_size = {3, 9};
        static constexpr Dimensions gameboard_dims = {(cell_size.height + 1) * grid_complexity + 1,
                                                      (cell_size.width + 1) * grid_complexity + 1};
        // constexpr 
        Grid<grid_complexity> grid_;
        WINDOW *gameboard_;
        int x_max_;
        int y_max_;
        bool colors_;
    };
} // namespace

int main()
{
    // using std::chrono::duration_cast;
    // using std::chrono::seconds;
    // using std::chrono::steady_clock;
    // using namespace std::chrono_literals;

    // Grid<5> grid(1);
    // auto start = steady_clock::now();
    // unsigned long long count = 0;
    // Xorshift rand(1);
    // while (duration_cast<seconds>(steady_clock::now() - start) < 1s) {
    //     grid.move(static_cast<direction>(rand() % 4));
    //     count++;
    //     if (grid.state() == Grid<5>::State::ended) grid.reset();
    // }
    // std::cout << count << std::endl;

    auto ui = TUI();
    ui.benchmark();
}