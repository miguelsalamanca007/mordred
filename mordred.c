#include <ncurses.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <pwd.h>
#include <stdio.h>
#include <locale.h>

#define UPPER_RIGHT_CORNER ACS_URCORNER
#define LOWER_RIGHT_CORNER ACS_LRCORNER  
#define UPPER_LEFT_CORNER ACS_ULCORNER
#define LOWER_LEFT_CORNER ACS_LLCORNER
#define HORIZONTAL_LINE ACS_HLINE
#define VERTICAL_LINE ACS_VLINE

struct dirblock {
    char *path;
    char *selected;
    char **files;
    int column;
    int n_files;
};

const char *get_username() {
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        return pw->pw_name;
    } else {
        return "unknown";
    }
}

void get_hostname(char *buffer, size_t size) {
    if (gethostname(buffer, size) != 0) {
        strncpy(buffer, "unknown", size);
        buffer[size - 1] = '\0'; // ensure null-termination
    }
}

bool equal_strings(const char *a, const char *b) {
    return strcmp(a, b) == 0 ? 1 : 0;
}

int get_number_of_files(const char *path) {
    DIR *dir = opendir(path);
    struct dirent *entry;
    int quantity = 0;
    while((entry = readdir(dir)) != NULL) {
        if (equal_strings(entry->d_name, ".") || equal_strings(entry->d_name, "..")) {
            continue;
        }
        quantity++;
    }

    closedir(dir);
    return quantity;
}

void get_files(const char *path, char **files) {
    int fileslen = get_number_of_files(path);
    DIR *dir = opendir(path);
    struct dirent *entry;
    int i = 0;
    
    while((entry = readdir(dir)) != NULL) {
        if (equal_strings(entry->d_name, ".") || equal_strings(entry->d_name, "..")) {
            continue;
        }
        files[i++] = strdup(entry->d_name); 
    }
    
    closedir(dir);
}

struct dirblock get_dirblock(char *path, int column) {
    int fileslen = get_number_of_files(path);
    char **files = malloc(sizeof(char *) * fileslen);
    get_files(path, files);
    char *selected = files[0];

    return (struct dirblock) {path, selected, files, column, fileslen};
}

int get_term_height() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
    {
        perror("ioctl");
        return 80; // Default terminal width instead of returning 1
    }
    int term_height = w.ws_row;
    return term_height;
}

int get_term_width() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
    {
        perror("ioctl");
        return 80; // Default terminal width instead of returning 1
    }
    int term_width = w.ws_col;
    return term_width;
}

char *pad_string(const char *input, size_t q, char fill_char) {
    size_t len = strlen(input);
    if (len >= q) {
        char *result = malloc(len + 1);
        if (!result)
            return NULL;
        strcpy(result, input);
        return result;
    }
    char *result = malloc(q + 1);
    if (!result) return NULL;
    strcpy(result, input);
    for (size_t i = len; i < q; i++) {
        result[i] = fill_char;
    }
    result[q] = '\0';
    return result;
}

int get_size_longest_name(const char *path) {
    DIR *dir = opendir(path);
    struct dirent *entry;
    int longest = 0;
    while((entry = readdir(dir)) != NULL) {
        if (strlen(entry->d_name) > longest) {
            longest = strlen(entry->d_name);
        }
    }

    closedir(dir);
    return longest;
}

bool is_directory(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0) {
        return false;  // Path doesn't exist or can't be accessed
    }
    return S_ISDIR(statbuf.st_mode);
}

void print_top_bar(const char *path, char *selected) {
    int term_width = get_term_width();
    char *bar = pad_string("", term_width, ' ');
    const char *username = get_username();
    char hostname[256];
    get_hostname(hostname, sizeof(hostname));

    int needed = snprintf(NULL, 0, " %s@%s", username, hostname);
    char *content = malloc(needed + 1);
    snprintf(content, needed + 1, " %s@%s", username, hostname);

    strncpy(bar, content, needed);

    attron(COLOR_PAIR(2));
    mvprintw(0, 0, content);
    attroff(COLOR_PAIR(2));

    attron(COLOR_PAIR(3));
    mvprintw(0, needed + 1, "%s/", path);
    attroff(COLOR_PAIR(3));

    mvprintw(0, needed + strlen(path) +  2, "%s", selected);

}

void print_bottom_bar(char *selected_file, char *selected_dir) {
    int term_height = get_term_height();
    int term_width = get_term_width();
    char *bar = pad_string("", term_width, ' ');

    int needed = snprintf(NULL, 0, " directory: %s   selected: %s",
                          selected_dir, selected_file);
    char *content = malloc(needed + 1);
    snprintf(content, needed + 1, " directory: %s   selected: %s",
             selected_dir, selected_file);

    strncpy(bar, content, needed);

    attron(COLOR_PAIR(1));
    mvprintw(term_height - 1, 0, bar);
    attroff(COLOR_PAIR(1));

    free(bar);
    free(content);
}

char *get_filename_formatted(char *filename, int column_size) {
    char *f_string = pad_string("", column_size, ' ');
    int needed = snprintf(NULL, 0, " %s", filename);
    char *content = malloc(needed + 1);
    snprintf(content, needed + 1, "  %s", filename);
    strncpy(f_string, content, needed);

    return f_string;
}

void print_block(struct dirblock block, int starting_row) {
    int column_size = get_size_longest_name(block.path) + 5;
    int sr = starting_row;
    for (int i = 0; i < block.n_files; i++) {
        if (equal_strings(block.selected, block.files[i])) {
            attron(COLOR_PAIR(1));
            char *fted_string = get_filename_formatted(block.files[i], column_size);
            mvprintw(sr, block.column, "%s", fted_string);
            attroff(COLOR_PAIR(1));
            free(fted_string);
            sr++;
        } else {
            char *fted_string = get_filename_formatted(block.files[i], column_size);
            mvprintw(sr, block.column, "%s", fted_string);
            free(fted_string);
            sr++;
        }
    }
}

void print_blocks(struct dirblock *blocks, int block_q, int starting_row) {
    for (int i = 0; i < block_q; i++) {
        print_block(blocks[i], starting_row);
    }
}

char *get_next_file(struct dirblock *block, int blocklen) {
    char *next_file;
    for (int i = 0; i < blocklen; i++) {
        if (equal_strings(block->files[i], block->selected)) {
            if (i == blocklen - 1) {
                next_file = strdup(block->files[0]);
            } else {
                next_file = strdup(block->files[i+1]);
            }
        }
    }

    return next_file;
}

char *get_previous_file(struct dirblock *block, int blocklen) {
    char *prev_file;
    for (int i = 0; i < blocklen; i++) {
        if (equal_strings(block->files[i], block->selected)) {
            if (i == 0) {
                prev_file = strdup(block->files[blocklen - 1]);
            } else {
                prev_file = strdup(block->files[i-1]);
            }
        }
    }

    return prev_file;
}

int get_next_column(struct dirblock *blocks, int block_q) {
    int nc = 0;
    for (int i = 0; i < block_q; i++) {
        nc += get_size_longest_name(blocks[i].path) + 5;
    }

    return nc;
}

bool can_draw_next_block(char *path, struct dirblock *blocks, int block_q) {
    int nc = get_next_column(blocks, block_q);
    int longest_name = get_size_longest_name(path);
    int right_limit = nc + longest_name;

    if (right_limit < get_term_width()) {
        return true;
    }

    return false;
}

void print_borders(struct dirblock *blocks, int block_q, int starting_row) {
    int box_height = get_term_height() - 1;
    int box_width = get_term_width();
    
    for (int i = 0; i < box_width; i++) {
        for (int j = starting_row; j < box_height; j++) {
            if(j == starting_row && i == 0) {
                mvaddch(j, i, UPPER_LEFT_CORNER);
            } else if (j == starting_row && i == box_width - 1) {
                mvaddch(j, i, UPPER_RIGHT_CORNER);
            } else if (i == 0 && j == box_height - 1) {
                mvaddch(j, i, LOWER_LEFT_CORNER);
            } else if (i == box_width - 1 && j == box_height - 1) {
                mvaddch(j, i, LOWER_RIGHT_CORNER);
            } else if (j == starting_row || j == box_height - 1) {
                mvaddch(j, i, HORIZONTAL_LINE);
            } else if (i == 0 || i == box_width - 1) {
                mvaddch(j, i, VERTICAL_LINE);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    struct dirblock *blocks = NULL;
    int block_q = 1;
    int ch;
    char *path;
    int starting_row = 1;

    if (argc > 1) {
        path = argv[argc-1];
    } else {
        path = ".";
    }

    initscr();            // Inicia ncurses
    noecho();             // No mostrar teclas pulsadas
    start_color();        // Habilitar colores
    init_pair(1, COLOR_BLACK, COLOR_CYAN);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_RED, COLOR_BLACK);

    cbreak();             // Leer input sin esperar Enter
    keypad(stdscr, TRUE); // Habilitar teclas especiales
    curs_set(0);          // Oculta el cursor
    setlocale(LC_ALL, "");

    blocks = realloc(blocks, sizeof(struct dirblock) * block_q);
    blocks[block_q - 1] = get_dirblock(path, 0);
    struct dirblock *current_block = &blocks[block_q - 1];
    
    

    while (1) {
        clear(); // Limpiar pantalla
        print_blocks(blocks, block_q, starting_row + 1);
        print_bottom_bar(current_block->selected, current_block->path);
        print_top_bar(current_block->path, current_block->selected);
        print_borders(blocks, block_q, starting_row);
        
        refresh(); // Mostrarlo
    
        ch = getch(); // Esperar tecla
        if (ch == 'q') break; // Salir con 'q'
        
        switch (ch) {
            case KEY_UP:
                current_block->selected = get_previous_file(current_block, current_block->n_files);

            break;
            case KEY_DOWN:
                current_block->selected = get_next_file(current_block, current_block->n_files);
                
            break;
            case KEY_LEFT:
                if (block_q > 1) {
                    blocks = realloc(blocks, sizeof(struct dirblock) * (block_q));
                    current_block = &blocks[block_q - 2];
                    block_q--;
                }
            break;
            case KEY_RIGHT: {

                char newpath[strlen(current_block->path) + strlen(current_block->selected) + 2];
                snprintf(newpath, sizeof(newpath), "%s/%s", current_block->path, current_block->selected);
                if (is_directory(newpath) && can_draw_next_block(newpath, blocks, block_q) && !(equal_strings(current_block->selected ,".") || equal_strings(current_block->selected ,".."))) {
                    int next_column = get_next_column(blocks, block_q);
                    blocks = realloc(blocks, sizeof(struct dirblock) * (block_q + 1));
                    blocks[block_q++] = get_dirblock(strdup(newpath), next_column);
                    current_block = &blocks[block_q - 1];
                } 

                break;
            }          
        }
    }

    endwin(); // Terminar ncurses
    return 0;
}

