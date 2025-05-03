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
#include "curses_terminal.h"

#include <fcntl.h>
#include <ncurses.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <iostream>

CursesTerminal::CursesTerminal(int cols, int rows) : display(cols, rows), dirty_lines(rows, true)
{
    initialize_ncurses();
    initialize_pty();
    initialize_colors();
}

CursesTerminal::~CursesTerminal()
{
    if (child_pid > 0) {
        kill(child_pid, SIGTERM);
        waitpid(child_pid, nullptr, 0);
    }
    if (pty_fd != -1) {
        close(pty_fd);
    }
    endwin();
}

void CursesTerminal::initialize_ncurses()
{
    initscr();
    raw(); // Use raw mode to disable signal generation for Ctrl+C
    noecho();
    nonl();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE); // Non-blocking input
    start_color();
    use_default_colors();
}

void CursesTerminal::initialize_pty()
{
    pty_fd = posix_openpt(O_RDWR | O_NOCTTY);
    if (pty_fd < 0) {
        std::cerr << "posix_openpt failed: " << strerror(errno) << std::endl;
        exit(1);
    }
    if (grantpt(pty_fd) < 0 || unlockpt(pty_fd) < 0) {
        std::cerr << "PTY setup failed: " << strerror(errno) << std::endl;
        exit(1);
    }

    child_pid = fork();
    if (child_pid < 0) {
        std::cerr << "fork failed: " << strerror(errno) << std::endl;
        exit(1);
    } else if (child_pid == 0) {
        if (setsid() < 0) {
            std::cerr << "setsid failed: " << strerror(errno) << std::endl;
            exit(1);
        }

        const char *pty_name = ptsname(pty_fd);
        int slave_fd         = open(pty_name, O_RDWR);
        if (slave_fd < 0) {
            std::cerr << "open slave PTY failed: " << strerror(errno) << std::endl;
            exit(1);
        }

        if (ioctl(slave_fd, TIOCSCTTY, 0) < 0) {
            std::cerr << "ioctl TIOCSCTTY failed: " << strerror(errno) << std::endl;
            exit(1);
        }

        dup2(slave_fd, STDIN_FILENO);
        dup2(slave_fd, STDOUT_FILENO);
        dup2(slave_fd, STDERR_FILENO);
        close(slave_fd);
        close(pty_fd);

        // Set initial geometry of child tty.
        struct winsize ws = {};
        ws.ws_col         = display.get_cols();
        ws.ws_row         = display.get_rows();
        if (ioctl(STDIN_FILENO, TIOCSWINSZ, &ws) < 0) {
            std::cerr << "ioctl TIOCSWINSZ failed: " << strerror(errno) << std::endl;
            exit(1);
        }

        setenv("TERM", "xterm-256color", 1);
        execlp("/bin/sh", "/bin/sh", nullptr);
        std::cerr << "execlp failed: " << strerror(errno) << std::endl;
        exit(1);
    }
}

void CursesTerminal::initialize_colors()
{
    for (int fg = 0; fg < 8; ++fg) {
        for (int bg = 0; bg < 8; ++bg) {
            init_pair(fg * 8 + bg + 1, fg, bg);
        }
    }
}

int CursesTerminal::get_color_pair(const CharAttr &attr)
{
    int fg = 7; // Default to white
    int bg = 0; // Default to black
    if (attr.fg == RgbColor{ 0, 0, 0 }) // Black
        fg = 0;
    else if (attr.fg == RgbColor{ 255, 0, 0 }) // Red
        fg = 1;
    else if (attr.fg == RgbColor{ 0, 255, 0 }) // Green
        fg = 2;
    else if (attr.fg == RgbColor{ 255, 255, 0 }) // Yellow
        fg = 3;
    else if (attr.fg == RgbColor{ 0, 0, 255 }) // Blue
        fg = 4;
    else if (attr.fg == RgbColor{ 255, 0, 255 }) // Magenta
        fg = 5;
    else if (attr.fg == RgbColor{ 0, 255, 255 }) // Cyan
        fg = 6;
    else if (attr.fg == RgbColor{ 255, 255, 255 }) // White
        fg = 7;

    if (attr.bg == RgbColor{ 0, 0, 0 }) // Black
        bg = 0;
    else if (attr.bg == RgbColor{ 255, 0, 0 }) // Red
        bg = 1;
    else if (attr.bg == RgbColor{ 0, 255, 0 }) // Green
        bg = 2;
    else if (attr.bg == RgbColor{ 255, 255, 0 }) // Yellow
        bg = 3;
    else if (attr.bg == RgbColor{ 0, 0, 255 }) // Blue
        bg = 4;
    else if (attr.bg == RgbColor{ 255, 0, 255 }) // Magenta
        bg = 5;
    else if (attr.bg == RgbColor{ 0, 255, 255 }) // Cyan
        bg = 6;
    else if (attr.bg == RgbColor{ 255, 255, 255 }) // White
        bg = 7;

    return COLOR_PAIR(fg * 8 + bg + 1);
}

void CursesTerminal::process_pty_input()
{
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(pty_fd, &read_fds);

    struct timeval timeout = { 0, 0 };
    if (select(pty_fd + 1, &read_fds, nullptr, nullptr, &timeout) > 0) {
        char buffer[1024];
        ssize_t bytes_read = read(pty_fd, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            std::vector<int> dirty_rows = display.process_input(buffer, bytes_read);
            for (int row : dirty_rows) {
                if (row >= 0 && static_cast<size_t>(row) < dirty_lines.size()) {
                    dirty_lines[row] = true;
                }
            }
        } else {
            throw std::runtime_error("PTY closed: child process terminated");
        }
    }
}

void CursesTerminal::process_keyboard_input()
{
    wint_t ch;
    int status = get_wch(&ch);
    if (status == ERR)
        return;

    KeyInput key;
    key.character = ch;

    switch (ch) {
    case 127:
        key.code = KeyCode::BACKSPACE;
        break;
    case KEY_UP:
        key.code = KeyCode::UP;
        break;
    case KEY_DOWN:
        key.code = KeyCode::DOWN;
        break;
    case KEY_RIGHT:
        key.code = KeyCode::RIGHT;
        break;
    case KEY_LEFT:
        key.code = KeyCode::LEFT;
        break;
    case KEY_HOME:
        key.code = KeyCode::HOME;
        break;
    case KEY_END:
        key.code = KeyCode::END;
        break;
    case KEY_IC:
        key.code = KeyCode::INSERT;
        break;
    case KEY_DC:
        key.code = KeyCode::DELETE;
        break;
    case KEY_PPAGE:
        key.code = KeyCode::PAGEUP;
        break;
    case KEY_NPAGE:
        key.code = KeyCode::PAGEDOWN;
        break;
    case KEY_F(1):
        key.code = KeyCode::F1;
        break;
    case KEY_F(2):
        key.code = KeyCode::F2;
        break;
    case KEY_F(3):
        key.code = KeyCode::F3;
        break;
    case KEY_F(4):
        key.code = KeyCode::F4;
        break;
    case KEY_F(5):
        key.code = KeyCode::F5;
        break;
    case KEY_F(6):
        key.code = KeyCode::F6;
        break;
    case KEY_F(7):
        key.code = KeyCode::F7;
        break;
    case KEY_F(8):
        key.code = KeyCode::F8;
        break;
    case KEY_F(9):
        key.code = KeyCode::F9;
        break;
    case KEY_F(10):
        key.code = KeyCode::F10;
        break;
    case KEY_F(11):
        key.code = KeyCode::F11;
        break;
    case KEY_F(12):
        key.code = KeyCode::F12;
        break;
    default:
        key.code = KeyCode::CHARACTER;
        if (ch < ' ') {
            // Handle control characters (ASCII 0x00–0x1F).
            key.character = ch + '@';
            key.mod_ctrl = true;
        }
        break;
    }

    std::string input = display.process_key(key);
    if (!input.empty()) {
        if (write(pty_fd, input.c_str(), input.size()) < 0) {
            throw std::runtime_error("PTY closed: child process terminated");
        }
    }
}

void CursesTerminal::render_frame()
{
    const auto &text_buffer = display.get_text_buffer();
    for (size_t row = 0; row < text_buffer.size(); ++row) {
        if (!dirty_lines[row])
            continue;
        dirty_lines[row] = false;

        move(row, 0);
        clrtoeol();

        std::wstring current_text;
        CharAttr current_attr = text_buffer[row][0].attr;
        int start_col         = 0;

        for (int col = 0; col < display.get_cols(); ++col) {
            const Char &ch = text_buffer[row][col];
            if (ch.attr == current_attr && col < display.get_cols() - 1) {
                current_text += ch.ch;
            } else {
                if (!current_text.empty()) {
                    attron(get_color_pair(current_attr));
                    mvaddwstr(row, start_col, current_text.c_str());
                    attroff(get_color_pair(current_attr));
                }
                current_text = ch.ch;
                current_attr = ch.attr;
                start_col    = col;
            }
        }
        if (!current_text.empty()) {
            attron(get_color_pair(current_attr));
            mvaddwstr(row, start_col, current_text.c_str());
            attroff(get_color_pair(current_attr));
        }
    }

    const Cursor &cursor = display.get_cursor();
    if (cursor.row >= 0 && cursor.row < static_cast<int>(text_buffer.size()) && cursor.col >= 0 &&
        cursor.col < display.get_cols()) {
        move(cursor.row, cursor.col);
        curs_set(1);
    } else {
        curs_set(0);
    }

    refresh();
}

void CursesTerminal::resize(int new_cols, int new_rows)
{
    display.resize(new_cols, new_rows);
    dirty_lines.assign(new_rows, true);

    struct winsize ws = {};
    ws.ws_col         = new_cols;
    ws.ws_row         = new_rows;
    if (ioctl(pty_fd, TIOCSWINSZ, &ws) < 0) {
        std::cerr << "ioctl TIOCSWINSZ failed: " << strerror(errno) << std::endl;
    }

    wresize(stdscr, new_rows, new_cols);
}
