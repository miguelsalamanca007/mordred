#include <ncurses.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>

struct dirblock {
    char *path;
    char *selected;
    int column;
};

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

void printbar(char *selected_file, char *selected_dir) {
    int term_heigth = get_term_height();
    attron(COLOR_PAIR(1));
    mvprintw(term_heigth - 1, 0, "D: %s F: %s", selected_dir, selected_file);
    attroff(COLOR_PAIR(1));
}

void printdir(const char *path, int column, char *selected) {
    DIR *dir = opendir(path);
    struct dirent *entry;
    int i = 0;
    int longest = get_size_longest_name(path);
    while((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, selected) == 0) {
            char *padded_name = pad_string(entry->d_name, longest + 2, ' ');
            attron(COLOR_PAIR(1));
            mvprintw(i, column, "%s", padded_name);
            attroff(COLOR_PAIR(1));
            free(padded_name);
        } else {
            char *padded_name = pad_string(entry->d_name, longest + 2, ' ');
            mvprintw(i, column, "%s", padded_name);
            free(padded_name);
        }
        i++;
    }
    closedir(dir);
}

bool equal_strings(const char *a, const char *b) {
    return strcmp(a, b) == 0 ? 1 : 0;
}

void print_blocks(struct dirblock *blocks, int block_q) {
    for (int i = 0; i < block_q; i++) {
        printdir(blocks[i].path, blocks[i].column, blocks[i].selected);
    }
}

int main(int argc, char *argv[]) {
    struct dirblock *blocks = NULL;
    int block_q = 0;
    int x = 10, y = 5; // posición inicial
    int ch;

    initscr();            // Inicia ncurses
    noecho();             // No mostrar teclas pulsadas
    start_color();        // Habilitar colores
    init_pair(1, COLOR_BLACK, COLOR_CYAN);
    cbreak();             // Leer input sin esperar Enter
    keypad(stdscr, TRUE); // Habilitar teclas especiales
    curs_set(0);          // Oculta el cursor
    // Bucle principal

    char *current_selected;
    char *current_path = (argc > 1 ? argv[1] : ".");
    DIR *current_dir = opendir(current_path);
    struct dirent *entry;
    entry = readdir(current_dir);
    current_selected = entry->d_name;
    blocks = realloc(blocks, sizeof(struct dirblock) * (block_q + 1));
    blocks[block_q] = (struct dirblock) {current_path, ".", 0};
    block_q++;
    struct dirblock *current_block = &blocks[block_q - 1];
    int next_column_start = 0;

    while (1) {
        clear(); // Limpiar pantalla
        print_blocks(blocks, block_q);
        printbar(current_block->selected, current_block->path);
        refresh(); // Mostrarlo
    
        ch = getch(); // Esperar tecla
        if (ch == 'q') break; // Salir con 'q'
        
        switch (ch) {
            case KEY_UP:{
            y = (y > 0) ? y - 1 : y;
            break;
            }
            case KEY_DOWN:{
            if ((entry = readdir(current_dir)) == NULL) {
                rewinddir(current_dir);
                entry = readdir(current_dir);
            }
            current_block->selected = strdup(entry->d_name);
            y = (y < LINES - 1) ? y + 1 : y;
            break;
            case KEY_LEFT:
            x = (x > 0) ? x - 1 : x;
            break;}
            case KEY_RIGHT: {
                char newpath[strlen(current_block->path) + strlen(entry->d_name) + 2];
                snprintf(newpath, sizeof(newpath), "%s/%s", (current_block->path), entry->d_name);
                char *prev_path = strdup(current_block->path);
                if (is_directory(newpath) && !equal_strings(current_selected, ".") || is_directory(newpath) && !equal_strings(current_selected, "..")) {
                    blocks = realloc(blocks, sizeof(struct dirblock) * (block_q + 1));
                    next_column_start += get_size_longest_name(prev_path) + 2; 
                    blocks[block_q] = (struct dirblock) {strdup(newpath), strdup("."), next_column_start};
                    current_block = &blocks[block_q];
                    block_q++;
                    closedir(current_dir);
                    current_dir = opendir(newpath);
                    entry = readdir(current_dir);
                } 
                free(prev_path);
                x = (x < COLS - 1) ? x + 1 : x;
                break;
            }          
        }
    }

    endwin(); // Terminar ncurses
    return 0;
}
