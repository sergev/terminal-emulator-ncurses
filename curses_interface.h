//
// Terminal emulator: interface to Unix and Curses library.
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
