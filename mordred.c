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
    char **files;
    int column;
    int n_files;
};

int get_number_of_files(const char *path) {
    DIR *dir = opendir(path);
    struct dirent *entry;
    int quantity = 0;
    while((entry = readdir(dir)) != NULL) {
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

int main(int argc, char *argv[]) {
    struct dirblock *blocks = NULL;
    int block_q = 1;
    int ch;
    char *path;

    if (argc > 1) {
        path = argv[argc-1];
    } else {
        path = ".";
    }

    initscr();            // Inicia ncurses
    noecho();             // No mostrar teclas pulsadas
    start_color();        // Habilitar colores
    init_pair(1, COLOR_BLACK, COLOR_CYAN);
    cbreak();             // Leer input sin esperar Enter
    keypad(stdscr, TRUE); // Habilitar teclas especiales
    curs_set(0);          // Oculta el cursor

    blocks = realloc(blocks, sizeof(struct dirblock) * block_q);
    blocks[block_q - 1] = get_dirblock(path, 0);
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
            case KEY_UP:
            break;
            case KEY_DOWN:{
                current_block->selected = get_next_file(current_block, current_block->n_files);
                
            break;
            case KEY_LEFT:
                if (block_q > 1) {
                    next_column_start -= get_size_longest_name(current_block->path) + 3;
                    blocks = realloc(blocks, sizeof(struct dirblock) * (block_q));
                    current_block = &blocks[block_q - 2];
                    block_q--;
                }
            break;}
            case KEY_RIGHT: {
                char newpath[strlen(current_block->path) + strlen(current_block->selected) + 2];
                snprintf(newpath, sizeof(newpath), "%s/%s", current_block->path, current_block->selected);
                if (is_directory(newpath) && !(equal_strings(current_block->selected ,".") || equal_strings(current_block->selected ,".."))) {
                    next_column_start += get_size_longest_name(current_block->path) + 2; 
                    blocks = realloc(blocks, sizeof(struct dirblock) * (block_q + 1));
                    blocks[block_q] = get_dirblock(strdup(newpath), next_column_start);
                    current_block = &blocks[block_q];
                    block_q++;
                } 
                break;
            }          
        }
    }

    endwin(); // Terminar ncurses
    return 0;
}
