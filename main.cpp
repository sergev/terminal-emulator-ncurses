#include <ncurses.h>
#include <unistd.h>

#include <iostream>

#include "curses_interface.h"

int main(int argc, char *argv[])
{
    try {
        // Get terminal size
        int rows, cols;
        initscr();
        getmaxyx(stdscr, rows, cols);
        endwin();

        CursesInterface curses_interface(cols, rows);

        // Main loop
        while (true) {
            try {
                curses_interface.process_keyboard_input();
                curses_interface.process_pty_input();
                curses_interface.render_frame();

                // Check for terminal resize
                int new_rows, new_cols;
                getmaxyx(stdscr, new_rows, new_cols);
                if (new_rows != curses_interface.get_rows() ||
                    new_cols != curses_interface.get_cols()) {
                    curses_interface.resize(new_cols, new_rows);
                }

                usleep(10000); // 10ms delay
            } catch (const std::runtime_error &e) {
                if (std::string(e.what()) == "PTY closed: child process terminated") {
                    break; // Exit loop when child process terminates
                }
                throw; // Re-throw other exceptions
            }
        }
    } catch (const std::exception &e) {
        endwin();
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}