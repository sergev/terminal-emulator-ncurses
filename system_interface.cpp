#include "system_interface.h"

#include <fcntl.h>
#include <ncurses.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>

SystemInterface::SystemInterface(int cols, int rows) : terminal(cols, rows), dirty_lines(rows, true)
{
    initialize_ncurses();
    initialize_pty();
    initialize_colors();
}

SystemInterface::~SystemInterface()
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

void SystemInterface::initialize_ncurses()
{
    initscr();
    raw(); // Use raw mode to disable signal generation for Ctrl+C
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE); // Non-blocking input
    start_color();
    use_default_colors();
}

void SystemInterface::initialize_pty()
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

        setenv("TERM", "xterm-256color", 1);
        execlp("/bin/sh", "/bin/sh", nullptr);
        std::cerr << "execlp failed: " << strerror(errno) << std::endl;
        exit(1);
    }
}

void SystemInterface::initialize_colors()
{
    for (int fg = 0; fg < 8; ++fg) {
        for (int bg = 0; bg < 8; ++bg) {
            init_pair(fg * 8 + bg + 1, fg, bg);
        }
    }
}

int SystemInterface::get_color_pair(const CharAttr &attr)
{
    int fg = 7; // Default to white
    int bg = 0; // Default to black
    if (attr.fg_r == 0 && attr.fg_g == 0 && attr.fg_b == 0)
        fg = 0; // Black
    else if (attr.fg_r == 255 && attr.fg_g == 0 && attr.fg_b == 0)
        fg = 1; // Red
    else if (attr.fg_r == 0 && attr.fg_g == 255 && attr.fg_b == 0)
        fg = 2; // Green
    else if (attr.fg_r == 255 && attr.fg_g == 255 && attr.fg_b == 0)
        fg = 3; // Yellow
    else if (attr.fg_r == 0 && attr.fg_g == 0 && attr.fg_b == 255)
        fg = 4; // Blue
    else if (attr.fg_r == 255 && attr.fg_g == 0 && attr.fg_b == 255)
        fg = 5; // Magenta
    else if (attr.fg_r == 0 && attr.fg_g == 255 && attr.fg_b == 255)
        fg = 6; // Cyan
    else if (attr.fg_r == 255 && attr.fg_g == 255 && attr.fg_b == 255)
        fg = 7; // White

    if (attr.bg_r == 0 && attr.bg_g == 0 && attr.bg_b == 0)
        bg = 0; // Black
    else if (attr.bg_r == 255 && attr.bg_g == 0 && attr.bg_b == 0)
        bg = 1; // Red
    else if (attr.bg_r == 0 && attr.bg_g == 255 && attr.bg_b == 0)
        bg = 2; // Green
    else if (attr.bg_r == 255 && attr.bg_g == 255 && attr.bg_b == 0)
        bg = 3; // Yellow
    else if (attr.bg_r == 0 && attr.bg_g == 0 && attr.bg_b == 255)
        bg = 4; // Blue
    else if (attr.bg_r == 255 && attr.bg_g == 0 && attr.bg_b == 255)
        bg = 5; // Magenta
    else if (attr.bg_r == 0 && attr.bg_g == 255 && attr.bg_b == 255)
        bg = 6; // Cyan
    else if (attr.bg_r == 255 && attr.bg_g == 255 && attr.bg_b == 255)
        bg = 7; // White

    return COLOR_PAIR(fg * 8 + bg + 1);
}

void SystemInterface::process_pty_input()
{
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(pty_fd, &read_fds);

    struct timeval timeout = { 0, 0 };
    if (select(pty_fd + 1, &read_fds, nullptr, nullptr, &timeout) > 0) {
        char buffer[1024];
        ssize_t bytes_read = read(pty_fd, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            std::vector<int> dirty_rows = terminal.process_input(buffer, bytes_read);
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

void SystemInterface::process_keyboard_input()
{
    wint_t ch;
    int status = get_wch(&ch);
    if (status == ERR)
        return;

    // Handle control characters (ASCII 0x00–0x1F) directly
    if (ch <= 0x1F) {
        char ctrl_char = static_cast<char>(ch);
        write(pty_fd, &ctrl_char, 1);
        return;
    }

    KeyInput key;
    key.character = ch;

    switch (ch) {
    case '\n':
    case '\r':
        key.code = KeyCode::ENTER;
        break;
    case 127:
    case KEY_BACKSPACE:
        key.code = KeyCode::BACKSPACE;
        break;
    case '\t':
        key.code = KeyCode::TAB;
        break;
    case 27:
        key.code = KeyCode::ESCAPE;
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
        if (ch >= 32) {
            key.code = KeyCode::CHARACTER;
            if (ch >= 'A' && ch <= 'Z')
                key.mod_shift = true;
        }
        break;
    }

    std::string input = terminal.process_key(key);
    if (!input.empty()) {
        write(pty_fd, input.c_str(), input.size());
    }
}

void SystemInterface::render_frame()
{
    const auto &text_buffer = terminal.get_text_buffer();
    for (size_t row = 0; row < text_buffer.size(); ++row) {
        if (!dirty_lines[row])
            continue;
        dirty_lines[row] = false;

        move(row, 0);
        clrtoeol();

        std::wstring current_text;
        CharAttr current_attr = text_buffer[row][0].attr;
        int start_col         = 0;

        for (int col = 0; col < terminal.get_cols(); ++col) {
            const Char &ch = text_buffer[row][col];
            if (ch.attr == current_attr && col < terminal.get_cols() - 1) {
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

    const Cursor &cursor = terminal.get_cursor();
    if (cursor.row >= 0 && cursor.row < static_cast<int>(text_buffer.size()) && cursor.col >= 0 &&
        cursor.col < terminal.get_cols()) {
        move(cursor.row, cursor.col);
        curs_set(1);
    } else {
        curs_set(0);
    }

    refresh();
}

void SystemInterface::resize(int new_cols, int new_rows)
{
    terminal.resize(new_cols, new_rows);
    dirty_lines.assign(new_rows, true);

    struct winsize ws = {};
    ws.ws_col         = new_cols;
    ws.ws_row         = new_rows;
    if (ioctl(pty_fd, TIOCSWINSZ, &ws) < 0) {
        std::cerr << "ioctl TIOCSWINSZ failed: " << strerror(errno) << std::endl;
    }

    wresize(stdscr, new_rows, new_cols);
}

int SystemInterface::get_cols() const
{
    return terminal.get_cols();
}

int SystemInterface::get_rows() const
{
    return terminal.get_rows();
}