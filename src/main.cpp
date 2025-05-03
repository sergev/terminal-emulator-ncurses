//
// Main routine of the terminal emulator.
//
// Copyright (c) 2025 Serge Vakulenko
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
#include <ncurses.h>
#include <unistd.h>
#include <locale.h>

#include <iostream>

#include "curses_interface.h"

int main(int argc, char *argv[])
{
    try {
        // Get terminal size
        int rows, cols;
        setlocale(LC_ALL, "en_US.UTF-8");
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
