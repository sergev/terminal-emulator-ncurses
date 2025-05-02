#ifndef CURSES_INTERFACE_H
#define CURSES_INTERFACE_H

#include <vector>

#include "terminal_logic.h"

class CursesInterface {
public:
    CursesInterface(int cols = 80, int rows = 24);
    ~CursesInterface();
    void process_pty_input();
    void process_keyboard_input();
    void render_frame();
    void resize(int new_cols, int new_rows);
    int get_cols() const;
    int get_rows() const;

private:
    TerminalLogic terminal;
    int pty_fd      = -1;
    pid_t child_pid = -1;
    std::vector<bool> dirty_lines;

    void initialize_ncurses();
    void initialize_pty();
    void initialize_colors();
    int get_color_pair(const CharAttr &attr);
};

#endif // CURSES_INTERFACE_H
