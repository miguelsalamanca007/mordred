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
#include <errno.h>

#define UPPER_RIGHT_CORNER ACS_URCORNER
#define LOWER_RIGHT_CORNER ACS_LRCORNER
#define UPPER_LEFT_CORNER ACS_ULCORNER
#define LOWER_LEFT_CORNER ACS_LLCORNER
#define HORIZONTAL_LINE ACS_HLINE
#define VERTICAL_LINE ACS_VLINE
#define TEE_DOWN ACS_TTEE
#define TEE_UP ACS_BTEE
#define SPACES_AFTER_LEFT_BORDER 1
#define SPACES_BEFORE_RIGHT_BORDER 5

struct dirblock
{
    char *path;
    char *selected;
    char **files;
    int column;
    int n_files;
    int selected_index;
    int column_size;
};

struct window
{
    int term_height;
    int term_width;
    int top_bar_row;
    int box_row;
    int bottom_bar_row;
    struct dirblock *blocks;
    struct dirblock *current_block;
    int block_quantity;
    char *path;
};

struct window wd;

void get_type_and_permissions(char *buffer, const char *path) {

    struct stat st;
    if (buffer && stat(path, &st) == 0) {
        mode_t perm = st.st_mode;

        if (S_ISREG(st.st_mode)) {
            buffer[0] = 'r';
        } else if (S_ISDIR(st.st_mode)) {
            buffer[0] = 'd';
        } else if (S_ISLNK(st.st_mode)) {
            buffer[0] = 'l';
        } else if (S_ISCHR(st.st_mode)) {
            buffer[0] = 'c';
        } else if (S_ISBLK(st.st_mode)) {
            buffer[0] = 'b';
        } else if (S_ISFIFO(st.st_mode)) {
            buffer[0] = 'f';
        } else if (S_ISSOCK(st.st_mode)) {
            buffer[0] = 's';
        } else {
            buffer[0] = '#'; //unkown type
        }

        buffer[1] = (perm & S_IRUSR) ? 'r' : '-',
        buffer[2] = (perm & S_IWUSR) ? 'w' : '-',
        buffer[3] = (perm & S_IXUSR) ? 'x' : '-';

        buffer[4] = (perm & S_IRGRP) ? 'r' : '-',
        buffer[5] = (perm & S_IWGRP) ? 'w' : '-',
        buffer[6] = (perm & S_IXGRP) ? 'x' : '-';

        buffer[7] = (perm & S_IROTH) ? 'r' : '-',
        buffer[8] = (perm & S_IWOTH) ? 'w' : '-',
        buffer[9] = (perm & S_IXOTH) ? 'x' : '-';
        buffer[10] = '\0';
    } else {
        strncpy(buffer, "unkown", 10); // 9 because there are 9 possible permission bits
        buffer[10] = '\0';
    }
}

void get_username(char *buffer, size_t size)
{
    struct passwd *pw = getpwuid(getuid());
    if (pw && buffer && size > 0)
    {
        strncpy(buffer, pw->pw_name, size - 1);
        buffer[size - 1] = '\0';
    }
    else if (buffer && size > 0)
    {
        strncpy(buffer, "unkown", size - 1);
        buffer[size - 1] = '\0';
    }
}

void get_hostname(char *buffer, size_t size)
{
    if (gethostname(buffer, size) != 0)
    {
        strncpy(buffer, "unknown", size);
        buffer[size - 1] = '\0'; // ensure null-termination
    }
}

bool equal_strings(const char *a, const char *b)
{
    if (a == NULL || b == NULL)
    {
        return NULL;
    }
    return strcmp(a, b) == 0 ? 1 : 0;
}

int get_number_of_files(const char *path)
{
    DIR *dir = opendir(path);
    struct dirent *entry;
    int quantity = 0;
    while ((entry = readdir(dir)) != NULL)
    {
        if (equal_strings(entry->d_name, ".") || equal_strings(entry->d_name, ".."))
        {
            continue;
        }
        quantity++;
    }

    closedir(dir);
    return quantity;
}

void get_files(const char *path, char **files)
{
    int fileslen = get_number_of_files(path);
    DIR *dir = opendir(path);
    struct dirent *entry;
    int i = 0;

    while ((entry = readdir(dir)) != NULL)
    {
        if (equal_strings(entry->d_name, ".") || equal_strings(entry->d_name, ".."))
        {
            continue;
        }
        files[i++] = strdup(entry->d_name);
    }

    closedir(dir);
}

int get_size_longest_name(const char *path)
{
    DIR *dir = opendir(path);
    struct dirent *entry;
    int longest = 0;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strlen(entry->d_name) > longest)
        {
            longest = strlen(entry->d_name);
        }
    }

    closedir(dir);
    return longest;
}

int get_column_size(const char *path)
{
    return SPACES_AFTER_LEFT_BORDER + get_size_longest_name(path) + SPACES_BEFORE_RIGHT_BORDER;
}

struct dirblock get_dirblock(char *path, int column)
{
    int fileslen = get_number_of_files(path);
    char **files = malloc(sizeof(char *) * fileslen);
    get_files(path, files);
    char *selected = files[0];
    int selected_index = 0;
    int column_size = get_column_size(path);

    return (struct dirblock){path, selected, files, column, fileslen, selected_index, column_size};
}

int get_term_height()
{
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
    {
        perror("ioctl");
        return 80; // Default terminal width instead of returning 1
    }
    int term_height = w.ws_row;
    #ifdef DEBUG
        term_height = 48;
    #endif
    return term_height;
}

int get_term_width()
{
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
    {
        perror("ioctl");
        return 80; // Default terminal width instead of returning 1
    }
    int term_width = w.ws_col;
    #ifdef DEBUG
        term_width = 116;
    #endif
    return term_width;
}

char *pad_string(const char *input, size_t q, char fill_char)
{
    size_t len = strlen(input);
    if (len >= q)
    {
        char *result = malloc(len + 1);
        if (!result)
            return NULL;
        strcpy(result, input);
        return result;
    }
    char *result = malloc(q + 1);
    if (!result)
        return NULL;
    strcpy(result, input);
    for (size_t i = len; i < q; i++)
    {
        result[i] = fill_char;
    }
    result[q] = '\0';
    return result;
}

bool is_directory(const char *path)
{
    struct stat statbuf;
    if (stat(path, &statbuf) != 0)
    {
        return false; // Path doesn't exist or can't be accessed
    }
    return S_ISDIR(statbuf.st_mode);
}

void print_top_bar(const char *path, char *selected)
{
    int term_width = wd.term_width;
    char username[256];
    get_username(username, sizeof(username));
    char hostname[256];
    get_hostname(hostname, sizeof(hostname));

    size_t leftlen = strlen(username) + strlen(hostname) + 2;
    char names[leftlen];

    snprintf(names, leftlen, "%s@%s", username, hostname);

    attron(COLOR_PAIR(2));
    mvprintw(0, 0, "%s", names);
    attroff(COLOR_PAIR(2));

    attron(COLOR_PAIR(3));
    mvprintw(0, strlen(names) + 1, "%s/", path);
    attroff(COLOR_PAIR(3));

    mvprintw(0, strlen(names) + strlen(path) + 2, "%s", selected);
}

char *get_new_path(char *path, char *filename)
{
    size_t np_size = sizeof(char) * (strlen(path) + strlen(filename) + strlen("/") + 1);
    char *newpath = malloc(np_size);
    snprintf(newpath, sizeof(char) * np_size, "%s/%s", path, filename);

    return newpath;
}

void print_bottom_bar(char *selected_file, char *selected_dir)
{
    char permissions[11];
    get_type_and_permissions(permissions, get_new_path(wd.current_block->path, wd.current_block->selected));

    attron(COLOR_PAIR(3));
    mvprintw(wd.bottom_bar_row, 0, "%s", permissions);
    attroff(COLOR_PAIR(3));
}

void filename_formatted(char *src, char *dst, int column_size)
{
    for (int i = 0; i < column_size; i++)
    {
        dst[i] = ' ';
    }
    for (int i = 0; i < strlen(src); i++)
    {
        dst[i + SPACES_AFTER_LEFT_BORDER] = src[i];
    }
    dst[column_size] = '\0';
}

void print_block(struct dirblock block)
{
    int starting_row = wd.box_row + 1;
    int box_height = (wd.bottom_bar_row - 1) - starting_row;
    int loop_limit = block.n_files < box_height ? block.n_files : box_height;
    int offset = 0;
    if (block.selected_index > box_height - 1) {
        offset = block.selected_index - (box_height - 1);
    }

    char fted_string[block.column_size];
    for (int i = 0; i < loop_limit; i++) {
        filename_formatted(block.files[i + offset], fted_string, block.column_size);
        if (equal_strings(block.selected, block.files[i + offset])) {
            attron(COLOR_PAIR(1));
            mvprintw(starting_row, block.column, "%s", fted_string);
            attroff(COLOR_PAIR(1));
        } else {
            mvprintw(starting_row, block.column, "%s", fted_string);
        }
        starting_row++;
    }
}

void print_blocks(struct dirblock *blocks, int block_q)
{
    for (int i = 0; i < block_q; i++)
    {
        print_block(blocks[i]);
    }
}

char *get_next_file(struct dirblock *block, int blocklen)
{
    char *next_file;
    for (int i = 0; i < blocklen; i++)
    {
        if (equal_strings(block->files[i], block->selected))
        {
            if (i == blocklen - 1)
            {
                next_file = strdup(block->files[0]);
            }
            else
            {
                next_file = strdup(block->files[i + 1]);
            }
        }
    }

    return next_file;
}

char *get_previous_file(struct dirblock *block, int blocklen)
{
    char *prev_file;
    for (int i = 0; i < blocklen; i++)
    {
        if (equal_strings(block->files[i], block->selected))
        {
            if (i == 0)
            {
                prev_file = strdup(block->files[blocklen - 1]);
            }
            else
            {
                prev_file = strdup(block->files[i - 1]);
            }
        }
    }

    return prev_file;
}

int get_next_column(struct dirblock *blocks, int block_q)
{
    int nc = 1;
    if (block_q < 1)
    {
        return nc;
    }

    for (int i = 0; i < block_q; i++)
    {
        nc += blocks[i].column_size + 1;
    }

    return nc;
}

bool can_draw_next_block(char *path, struct dirblock *blocks, int block_q)
{
    int nc = get_next_column(blocks, block_q);
    int longest_name = get_size_longest_name(path);
    int right_limit = nc + longest_name;

    if (right_limit < wd.term_width)
    {
        return true;
    }

    return false;
}

void print_borders(struct dirblock *blocks, int block_q, int starting_row)
{
    int box_height = wd.term_height - 1;
    int box_width = wd.term_width;

    mvaddch(starting_row, 0, UPPER_LEFT_CORNER);
    mvaddch(starting_row, box_width - 1, UPPER_RIGHT_CORNER);
    mvaddch(box_height - 1, 0, LOWER_LEFT_CORNER);
    mvaddch(box_height - 1, box_width - 1, LOWER_RIGHT_CORNER);

    for (int j = starting_row + 1; j < box_height - 1; j++)
    {
        mvaddch(j, 0, VERTICAL_LINE);
        mvaddch(j, box_width - 1, VERTICAL_LINE);
    }

    for (int i = 0 + 1; i < box_width - 1; i++)
    {
        mvaddch(starting_row, i, HORIZONTAL_LINE);
        mvaddch(box_height - 1, i, HORIZONTAL_LINE);
    }

    for (int i = 1; i < block_q; i++)
    {
        int column = blocks[i].column - 1;
        for (int j = starting_row; j < box_height; j++)
        {
            if (j == starting_row)
            {
                mvaddch(j, column, TEE_DOWN);
            }
            else if (j == box_height - 1)
            {
                mvaddch(j, column, TEE_UP);
            }
            else
            {
                mvaddch(j, column, VERTICAL_LINE);
            }
        }
    }
}

void start_ncurses(void)
{
    initscr();     // Inicia ncurses
    noecho();      // No mostrar teclas pulsadas
    start_color(); // Habilitar colores
    init_pair(1, COLOR_BLACK, COLOR_CYAN);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_RED, COLOR_BLACK);
    cbreak();             // Leer input sin esperar Enter
    keypad(stdscr, TRUE); // Habilitar teclas especiales
    curs_set(0);          // Oculta el cursor
}

void add_block()
{
    if (wd.block_quantity == 0)
    {
        wd.blocks = malloc(sizeof(struct dirblock));
        wd.blocks[0] = get_dirblock(wd.path, 1);
        wd.block_quantity = 1;
        wd.current_block = &wd.blocks[0];
    }
    else
    {
        int next_column = get_next_column(wd.blocks, wd.block_quantity);
        char *newpath = get_new_path(wd.current_block->path, wd.current_block->selected);
        wd.blocks = realloc(wd.blocks, sizeof(struct dirblock) * (wd.block_quantity + 1));
        wd.blocks[wd.block_quantity] = get_dirblock(newpath, next_column);
        wd.current_block = &wd.blocks[wd.block_quantity];
        wd.block_quantity++;
    }
}

void delete_block() {
    if (wd.block_quantity > 1) {
        wd.blocks = realloc(wd.blocks, sizeof(struct dirblock) * (wd.block_quantity - 1));
        wd.current_block = &wd.blocks[wd.block_quantity - 2];
        wd.block_quantity--;
    }
}

void start_window(char *path)
{
    wd.term_height = get_term_height();
    wd.term_width = get_term_width();
    wd.top_bar_row = 0;
    wd.box_row = 1;
    wd.bottom_bar_row = wd.term_height - 1;
    wd.block_quantity = 0;
    wd.path = path;
    add_block();
}

void show_message_bottom_bar(const char *message) {
    attron(COLOR_PAIR(2));
    mvprintw(wd.bottom_bar_row, 0, "%s, press any key to continue", message);
    attroff(COLOR_PAIR(2));
    getch();
}

bool delete_file(const char *path) {
    struct stat st;
    
    if (stat(path, &st) != 0) {
        if (errno == ENOENT) {
            show_message_bottom_bar("Could not delete: file does not exist!");
        } else {
            show_message_bottom_bar("Could not delete: error verifying file");
        }
        return false;
    }
    
    if (!S_ISREG(st.st_mode)) {
        show_message_bottom_bar("Could not delete: file is not a regular file");
        return false;
    }
    
    if (remove(path) == 0) {
        return true;
    } else {
        show_message_bottom_bar("Could not delete: error deleting file");
        return false;
    }
}

void delete_bar() {
    int ch;
    bool deleted = false;
    attron(COLOR_PAIR(2));
    mvprintw(wd.bottom_bar_row, 0, "are you sure you want to delete the file %s? [y/n]", wd.current_block->selected);
    attroff(COLOR_PAIR(2));

    while (1) {
        ch = getch();
        if (ch == 'y') {
            char *path = get_new_path(wd.current_block->path, wd.current_block->selected);
            deleted = delete_file(path);
            break;
        } else if (ch == 'n') {
            return;
        } else {
            break;
        }
    }

    move(wd.bottom_bar_row, 0);
    clrtoeol();

    if (deleted) {
        *wd.current_block = get_dirblock(wd.current_block->path, wd.current_block->column);
        attron(COLOR_PAIR(2));
        mvprintw(wd.bottom_bar_row, 0, "file deleted successfully! press any key to continue", wd.current_block->selected);
        attroff(COLOR_PAIR(2));
    }

    getch();
}

void start_loop()
{
    int ch;   
    while (1)
    {
        clear(); // Limpiar pantalla
        print_blocks(wd.blocks, wd.block_quantity);
        print_bottom_bar(wd.current_block->selected, wd.current_block->path);
        print_top_bar(wd.current_block->path, wd.current_block->selected);
        print_borders(wd.blocks, wd.block_quantity, wd.box_row);
        refresh(); // Mostrarlo

        ch = getch(); // Esperar tecla
        if (ch == 'q')
            break; // Salir con 'q'

        switch (ch)
        {
        case KEY_UP:
            wd.current_block->selected = get_previous_file(wd.current_block, wd.current_block->n_files);
            wd.current_block->selected_index = wd.current_block->selected_index == 0 ? (wd.current_block->n_files) - 1 : wd.current_block->selected_index - 1;
            break;
        case KEY_DOWN:
            wd.current_block->selected = get_next_file(wd.current_block, wd.current_block->n_files);
            wd.current_block->selected_index = wd.current_block->selected_index == (wd.current_block->n_files) - 1 ? 0 : wd.current_block->selected_index + 1;
            break;
        case KEY_LEFT:
            delete_block();
            break;
        case KEY_RIGHT:
        {
            if (is_directory(get_new_path(wd.current_block->path, wd.current_block->selected))) {
                add_block();
            }
            break;
        }
        case 'd': 
        {
            delete_bar();
        }
        }
    }

    endwin();
}

int main(int argc, char *argv[])
{
    char *path;

    if (argc > 1)
    {
        path = argv[argc - 1];
    }
    else
    {
        path = ".";
    }

    start_ncurses();
    start_window(path);
    start_loop();

    // Terminar ncurses
    return 0;
}
