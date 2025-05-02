#include <ncurses.h>
#include <unistd.h>

#include <iostream>

#include "system_interface.h"

int main(int argc, char *argv[])
{
    try {
        // Get terminal size
        int rows, cols;
        initscr();
        getmaxyx(stdscr, rows, cols);
        endwin();

        SystemInterface sys_interface(cols, rows);

        // Main loop
        while (true) {
            sys_interface.process_keyboard_input();
            sys_interface.process_pty_input();
            sys_interface.render_frame();

            // Check for terminal resize
            int new_rows, new_cols;
            getmaxyx(stdscr, new_rows, new_cols);
            if (new_rows != sys_interface.get_rows() || new_cols != sys_interface.get_cols()) {
                sys_interface.resize(new_cols, new_rows);
            }

            usleep(10000); // 10ms delay
        }
    } catch (const std::exception &e) {
        endwin();
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}