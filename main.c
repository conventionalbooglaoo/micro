/*
 * Micro - An extremely lightweight text editor
 * Features: Basic cursor movement, insertion, deletion, file load/save
 * Optimized for minimum code size with maximum utility
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>

#define CTRL_KEY(k) ((k) & 0x1f)
#define MAX_LINE_LEN 1024
#define TAB_STOP 8

typedef struct line {
    char *chars;
    int len;
    struct line *next;
    struct line *prev;
} line;

typedef struct {
    int cx, cy;    // Cursor position
    int rowoff;    // Row offset for scrolling
    int coloff;    // Column offset for scrolling
    int rows;      // Terminal rows
    int cols;      // Terminal cols
    line *first;   // First line
    line *current; // Current line
    char *filename;
    int dirty;     // File modified flag
    struct termios orig_termios;
} editor;

editor E;

void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7);
    perror(s);
    exit(1);
}

void disable_raw_mode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

void enable_raw_mode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
    atexit(disable_raw_mode);
    
    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

int read_key() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1) die("read");
    }
    
    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
        
        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return 'k'; // Up
                case 'B': return 'j'; // Down
                case 'C': return 'l'; // Right
                case 'D': return 'h'; // Left
                case '5': return read(STDIN_FILENO, &seq[2], 1) == 1 && seq[2] == '~' ? 'K' : '\x1b'; // Page Up
                case '6': return read(STDIN_FILENO, &seq[2], 1) == 1 && seq[2] == '~' ? 'J' : '\x1b'; // Page Down
                case 'H': return '0'; // Home
                case 'F': return '$'; // End
            }
        }
        return '\x1b';
    }
    return c;
}

void clear_screen() {
    write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7);
}

void get_window_size() {
    // Simple but less portable method
    // For production, consider using ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)
    write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12);
    write(STDOUT_FILENO, "\x1b[6n", 4);
    
    char buf[32];
    unsigned int i = 0;
    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1 || buf[i] == 'R')
            break;
        i++;
    }
    buf[i] = '\0';
    
    if (buf[0] != '\x1b' || buf[1] != '[') return;
    if (sscanf(&buf[2], "%d;%d", &E.rows, &E.cols) != 2) return;
    
    // Adjust for status line
    E.rows -= 1;
}

void insert_line(char *s, int len) {
    line *new = malloc(sizeof(line));
    if (!new) die("malloc");
    
    new->chars = malloc(len + 1);
    if (!new->chars) die("malloc");
    
    memcpy(new->chars, s, len);
    new->chars[len] = '\0';
    new->len = len;
    
    if (!E.first) {
        new->prev = new->next = NULL;
        E.first = E.current = new;
    } else {
        new->prev = E.current;
        new->next = E.current->next;
        if (E.current->next)
            E.current->next->prev = new;
        E.current->next = new;
        E.current = new;
    }
    E.cy++;
    E.dirty = 1;
}

void insert_char(int c) {
    if (!E.current) {
        line *new = malloc(sizeof(line));
        if (!new) die("malloc");
        new->chars = malloc(2);
        if (!new->chars) die("malloc");
        new->chars[0] = c;
        new->chars[1] = '\0';
        new->len = 1;
        new->prev = new->next = NULL;
        E.first = E.current = new;
    } else {
        char *new = realloc(E.current->chars, E.current->len + 2);
        if (!new) die("realloc");
        
        memmove(&new[E.cx + 1], &new[E.cx], E.current->len - E.cx + 1);
        new[E.cx] = c;
        E.current->chars = new;
        E.current->len++;
    }
    E.cx++;
    E.dirty = 1;
}

void delete_char() {
    if (!E.current || E.cx == 0 && !E.current->prev) return;
    
    if (E.cx > 0) {
        memmove(&E.current->chars[E.cx - 1], &E.current->chars[E.cx], 
                E.current->len - E.cx + 1);
        E.current->len--;
        E.cx--;
        E.current->chars = realloc(E.current->chars, E.current->len + 1);
    } else {
        // Join with previous line
        line *prev = E.current->prev;
        int oldlen = prev->len;
        
        prev->chars = realloc(prev->chars, prev->len + E.current->len + 1);
        memcpy(&prev->chars[prev->len], E.current->chars, E.current->len + 1);
        prev->len += E.current->len;
        
        line *temp = E.current;
        prev->next = E.current->next;
        if (E.current->next)
            E.current->next->prev = prev;
        E.current = prev;
        E.cx = oldlen;
        E.cy--;
        
        free(temp->chars);
        free(temp);
    }
    E.dirty = 1;
}

void split_line() {
    char *right = malloc(E.current->len - E.cx + 1);
    if (!right) die("malloc");
    
    strcpy(right, &E.current->chars[E.cx]);
    E.current->chars[E.cx] = '\0';
    E.current->len = E.cx;
    
    line *new = malloc(sizeof(line));
    if (!new) die("malloc");
    
    new->chars = right;
    new->len = strlen(right);
    new->next = E.current->next;
    new->prev = E.current;
    
    if (E.current->next)
        E.current->next->prev = new;
    E.current->next = new;
    
    E.current = new;
    E.cx = 0;
    E.cy++;
    E.dirty = 1;
}

void move_cursor(int key) {
    switch (key) {
        case 'h':
            if (E.cx > 0) E.cx--;
            break;
        case 'l':
            if (E.current && E.cx < E.current->len) E.cx++;
            break;
        case 'k':
            if (E.current && E.current->prev) {
                E.current = E.current->prev;
                E.cy--;
                if (E.cx > E.current->len)
                    E.cx = E.current->len;
            }
            break;
        case 'j':
            if (E.current && E.current->next) {
                E.current = E.current->next;
                E.cy++;
                if (E.cx > E.current->len)
                    E.cx = E.current->len;
            }
            break;
        case '0':
            E.cx = 0;
            break;
        case '$':
            if (E.current) E.cx = E.current->len;
            break;
        case 'J': // Page down
            for (int i = 0; i < E.rows / 2; i++) {
                if (E.current && E.current->next) {
                    E.current = E.current->next;
                    E.cy++;
                }
            }
            if (E.cx > E.current->len) E.cx = E.current->len;
            break;
        case 'K': // Page up
            for (int i = 0; i < E.rows / 2; i++) {
                if (E.current && E.current->prev) {
                    E.current = E.current->prev;
                    E.cy--;
                }
            }
            if (E.cx > E.current->len) E.cx = E.current->len;
            break;
    }
}

int row_cx_to_rx(char *row, int cx) {
    int rx = 0;
    for (int j = 0; j < cx; j++) {
        if (row[j] == '\t')
            rx += (TAB_STOP - 1) - (rx % TAB_STOP);
        rx++;
    }
    return rx;
}

void scroll() {
    // Vertical scrolling
    if (E.cy < E.rowoff)
        E.rowoff = E.cy;
    if (E.cy >= E.rowoff + E.rows)
        E.rowoff = E.cy - E.rows + 1;
        
    // Horizontal scrolling
    if (E.current) {
        int rx = row_cx_to_rx(E.current->chars, E.cx);
        
        if (rx < E.coloff)
            E.coloff = rx;
        if (rx >= E.coloff + E.cols)
            E.coloff = rx - E.cols + 1;
    }
}

void draw_rows() {
    write(STDOUT_FILENO, "\x1b[?25l", 6); // Hide cursor
    write(STDOUT_FILENO, "\x1b[H", 3);    // Reset cursor position
    
    line *l = E.first;
    int filerow = 0;
    
    // Skip to the visible start
    while (l && filerow < E.rowoff) {
        l = l->next;
        filerow++;
    }
    
    for (int y = 0; y < E.rows; y++) {
        if (filerow >= E.rowoff + y && l) {
            int len = l->len - E.coloff;
            if (len < 0) len = 0;
            if (len > E.cols) len = E.cols;
            
            if (len > 0)
                write(STDOUT_FILENO, &l->chars[E.coloff], len);
                
            l = l->next;
        } else {
            write(STDOUT_FILENO, "~", 1);
        }
        
        write(STDOUT_FILENO, "\x1b[K", 3); // Clear line to right
        write(STDOUT_FILENO, "\r\n", 2);
    }
    
    // Status line
    char status[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
                      E.filename ? E.filename : "[New File]",
                      filerow, E.dirty ? "(modified)" : "");
    if (len > E.cols) len = E.cols;
    
    write(STDOUT_FILENO, "\x1b[7m", 4); // Inverted colors
    write(STDOUT_FILENO, status, len);
    write(STDOUT_FILENO, "\x1b[m", 3);  // Normal colors
    write(STDOUT_FILENO, "\x1b[K", 3);  // Clear to right
    
    // Position cursor
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", 
            (E.cy - E.rowoff) + 1, 
            row_cx_to_rx(E.current ? E.current->chars : "", E.cx) - E.coloff + 1);
    write(STDOUT_FILENO, buf, strlen(buf));
    
    write(STDOUT_FILENO, "\x1b[?25h", 6); // Show cursor
}

void open_file(char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return;
    
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    
    E.first = E.current = NULL;
    E.cy = 0;
    
    while ((linelen = getline(&line, &linecap, fp)) > 0) {
        // Strip trailing newline
        while (linelen > 0 && (line[linelen-1] == '\n' || line[linelen-1] == '\r'))
            linelen--;
            
        insert_line(line, linelen);
    }
    
    free(line);
    fclose(fp);
    
    E.filename = strdup(filename);
    E.dirty = 0;
    E.cy = 0;
    
    // Reset to first line
    E.current = E.first;
}

void save_file() {
    if (!E.filename) {
        // Simple inline prompt for filename
        char buf[128];
        write(STDOUT_FILENO, "\x1b[999D\x1b[KSave as: ", 17);
        
        int i = 0;
        while (i < sizeof(buf) - 1) {
            char c;
            if (read(STDIN_FILENO, &c, 1) != 1) break;
            if (c == '\r' || c == '\n') break;
            if (c == CTRL_KEY('c')) return;
            
            buf[i++] = c;
            write(STDOUT_FILENO, &c, 1);
        }
        buf[i] = '\0';
        
        if (i == 0) return;
        E.filename = strdup(buf);
    }
    
    FILE *fp = fopen(E.filename, "w");
    if (!fp) die("fopen");
    
    line *l = E.first;
    while (l) {
        fwrite(l->chars, 1, l->len, fp);
        fwrite("\n", 1, 1, fp);
        l = l->next;
    }
    
    fclose(fp);
    E.dirty = 0;
}

void init_editor() {
    E.cx = E.cy = 0;
    E.rowoff = E.coloff = 0;
    E.first = E.current = NULL;
    E.filename = NULL;
    E.dirty = 0;
    
    get_window_size();
}

void process_keypress() {
    int c = read_key();
    
    switch (c) {
        case CTRL_KEY('q'):
            if (E.dirty) {
                char buf[32];
                int len = snprintf(buf, sizeof(buf), 
                                  "\r\nUnsaved changes. Press Ctrl-Q again to quit.");
                write(STDOUT_FILENO, buf, len);
                if (read_key() != CTRL_KEY('q')) {
                    draw_rows();
                    return;
                }
            }
            clear_screen();
            exit(0);
            break;
            
        case CTRL_KEY('s'):
            save_file();
            break;
            
        case '\r':
            split_line();
            break;
            
        case CTRL_KEY('h'):
        case 127:
            delete_char();
            break;
            
        case 'h':
        case 'j':
        case 'k':
        case 'l':
        case '0':
        case '$':
        case 'J':
        case 'K':
            move_cursor(c);
            break;
            
        default:
            if (!iscntrl(c))
                insert_char(c);
            break;
    }
}

int main(int argc, char *argv[]) {
    enable_raw_mode();
    init_editor();
    
    if (argc >= 2)
        open_file(argv[1]);
        
    while (1) {
        scroll();
        draw_rows();
        process_keypress();
    }
    
    return 0;
}
