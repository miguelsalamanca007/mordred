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

#define UPPER_RIGHT_CORNER         ACS_URCORNER
#define LOWER_RIGHT_CORNER         ACS_LRCORNER
#define UPPER_LEFT_CORNER          ACS_ULCORNER
#define LOWER_LEFT_CORNER          ACS_LLCORNER
#define HORIZONTAL_LINE            ACS_HLINE
#define VERTICAL_LINE              ACS_VLINE
#define TEE_DOWN                   ACS_TTEE
#define TEE_UP                     ACS_BTEE
#define SPACES_AFTER_LEFT_BORDER   1
#define SPACES_BEFORE_RIGHT_BORDER 5

#define K_ENTER     10
#define K_BACKSPACE 127

char *TEXT_FILE_EXTENSIONS[] = {".txt", ".py", ".c", ".java"};

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
    bool moving_file;
};

struct window wd;

static int compare_strings(const void *a, const void *b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

static void sort_files(char **files, int n_files) {
    if (!files) return;
    
    if (n_files > 0) {
        qsort(files, (size_t) n_files, sizeof(char*), compare_strings);
    }
}

// returns the column to start writing text
int get_column_by_index(int i) {
    if (i != 0 && i != 1) {
        return 0;
    }

    if (i == 0) {
        return wd.term_width / 8 + 2;
    } else {
        return wd.term_width / 2;
    }

}

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

void get_username(char *buffer, size_t size) {
    struct passwd *pw = getpwuid(getuid());
    if (pw && buffer && size > 0) {
        strncpy(buffer, pw->pw_name, size - 1);
        buffer[size - 1] = '\0';
    } else if (buffer && size > 0) {
        strncpy(buffer, "unknown", size - 1);
        buffer[size - 1] = '\0';
    }
}

void get_hostname(char *buffer, size_t size) {
    if (gethostname(buffer, size) != 0) {
        strncpy(buffer, "unknown", size);
        buffer[size - 1] = '\0'; // ensure null-termination
    }
}

bool equal_strings(const char *a, const char *b) {
    if (!a || !b) {
        return false;
    }
    return strcmp(a, b) == 0 ? 1 : 0;
}

int get_number_of_files(const char *path) {
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

void get_files(const char *path, char **files) {
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

int get_size_longest_name(const char *path) {
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

int get_column_size(const char *path) {
    return SPACES_AFTER_LEFT_BORDER + get_size_longest_name(path) + SPACES_BEFORE_RIGHT_BORDER;
}

struct dirblock get_dirblock(char *path, int column) {
    int fileslen = get_number_of_files(path);
    char **files = malloc(sizeof(char *) * fileslen);
    get_files(path, files);
    sort_files(files, fileslen);
    char *selected = files[0];
    int selected_index = 0;
    int column_size = get_column_size(path);

    return (struct dirblock){path, selected, files, column, fileslen, selected_index, column_size};
}

int get_term_height() {
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

int get_term_width() {
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

bool is_directory(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0) {
        return false; // Path doesn't exist or can't be accessed
    }
    return S_ISDIR(statbuf.st_mode);
}

void print_top_bar(const char *path, char *selected) {
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

char *get_new_path(char *path, char *filename) {
    if (path == NULL || filename == NULL) {
        return NULL;
    }

    size_t np_size = sizeof(char) * (strlen(path) + strlen(filename) + strlen("/") + 1);
    char *newpath = malloc(np_size);
    snprintf(newpath, sizeof(char) * np_size, "%s/%s", path, filename);

    return newpath;
}

void print_normal_bottom_bar(char *selected_file, char *selected_dir) {
    char permissions[11];
    char opt_message[] = "Press o for options";
    int mes_col = wd.term_width - strlen(opt_message);
    char *path = get_new_path(wd.current_block->path, wd.current_block->selected);

    if (path != NULL) {
        get_type_and_permissions(permissions, path);
        free(path);
        attron(COLOR_PAIR(3));
        mvprintw(wd.bottom_bar_row, 0, "%s", permissions);
        mvprintw(wd.bottom_bar_row, mes_col, opt_message);
        attroff(COLOR_PAIR(3));
    } else {
        mvprintw(wd.bottom_bar_row, 0, "Error getting type and permissions"); 
    }

}

void print_moving_file_bar() {
    attron(COLOR_PAIR(2));
    mvprintw(wd.bottom_bar_row, 0, "Directory to move: %s", wd.current_block->path);
    attroff(COLOR_PAIR(3));
}

void print_bottom_bar(char *selected_file, char *selected_dir)
{
    if (wd.moving_file) {
        print_moving_file_bar();
    } else {
        print_normal_bottom_bar(selected_file, selected_dir);
    }
}

void filename_formatted(char *src, char *dst, int column_size) {
    memset(dst, ' ', column_size);
    if (column_size < strlen(src)) {
        for (int i = 0; i < column_size - 2; i++) {
            dst[i + SPACES_AFTER_LEFT_BORDER] = src[i];
        }
        dst[column_size - 2] = '~';
    } else {
        for (int i = 0; i < strlen(src); i++) {
            dst[i + SPACES_AFTER_LEFT_BORDER] = src[i];
        }
    }
    dst[column_size] = '\0';
}

void print_block(struct dirblock block, int index)
{

    if (index != 1 && index != 0) {
        return;
    }

    int column_size;
    int column;

    if (index == 0) {
        column_size = (get_column_by_index(0) - 2) - 1;
        column = 1;
    } else if (index == 1) {
        column_size = ((get_column_by_index(1) - 2) - 1) - ((get_column_by_index(0) - 2) - 1);
        column = get_column_by_index(0) - 1;
    }

    int starting_row = wd.box_row + 1;
    int box_height = (wd.bottom_bar_row - 1) - starting_row;
    int loop_limit = block.n_files < box_height ? block.n_files : box_height;
    int offset = 0;
    if (block.selected_index > box_height - 1) {
        offset = block.selected_index - (box_height - 1);
    }

    char fted_string[column_size];
    for (int i = 0; i < loop_limit; i++) {
        filename_formatted(block.files[i + offset], fted_string, column_size);
        if (equal_strings(block.selected, block.files[i + offset])) {
            attron(COLOR_PAIR(1));
            mvprintw(starting_row, column, "%s", fted_string);
            attroff(COLOR_PAIR(1));
        } else {
            mvprintw(starting_row, column, "%s", fted_string);
        }
        starting_row++;
    }
}

void print_blocks(struct dirblock *blocks, int block_q)
{
    if (block_q >= 2) {
        print_block(blocks[block_q - 1], 1);
        print_block(blocks[block_q - 2], 0);
    } else if (block_q == 1) {
        print_block(blocks[0], 0);
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

    // Printing corners
    mvaddch(starting_row, 0, UPPER_LEFT_CORNER);
    mvaddch(starting_row, box_width - 1, UPPER_RIGHT_CORNER);
    mvaddch(box_height - 1, 0, LOWER_LEFT_CORNER);
    mvaddch(box_height - 1, box_width - 1, LOWER_RIGHT_CORNER);

    // Printing side vertical lines
    for (int j = starting_row + 1; j < box_height - 1; j++)
    {
        mvaddch(j, 0, VERTICAL_LINE);
        mvaddch(j, box_width - 1, VERTICAL_LINE);
    }

    // Printing side horizontal lines
    for (int i = 0 + 1; i < box_width - 1; i++)
    {
        mvaddch(starting_row, i, HORIZONTAL_LINE);
        mvaddch(box_height - 1, i, HORIZONTAL_LINE);
    }

    int loop_limit = block_q >= 2 ? 2 : block_q;

    // Printing block lines
    for (int i = 0; i < loop_limit; i++)
    {
        int column = get_column_by_index(i) - 2;
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
        if (newpath != NULL) {
            wd.blocks = realloc(wd.blocks, sizeof(struct dirblock) * (wd.block_quantity + 1));
            wd.blocks[wd.block_quantity] = get_dirblock(newpath, next_column);
            wd.current_block = &wd.blocks[wd.block_quantity];
            wd.block_quantity++;
        }
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
    wd.moving_file = false;
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
    move(wd.bottom_bar_row, 0);
    clrtoeol();

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

bool create_file(char *filename) {
    FILE *file;
    char *path = get_new_path(wd.current_block->path, filename);

    if (path == NULL) {
        show_message_bottom_bar("Error, could not create new file");
        return false;
    }

    file = fopen(path, "w");
    if (file == NULL) {
        show_message_bottom_bar("Error, could not create new file");
        return false;
    }

    fclose(file);
    return true;
}

void new_file_bar() {
    int ch;
    char *new_name = malloc(wd.term_width * sizeof(char));
    char message[] = "New file name: ";
    int message_len = strlen(message);
    int column = message_len;
    int i = 0;

    while (1) {   
        move(wd.bottom_bar_row, 0);
        clrtoeol();
        attron(COLOR_PAIR(2));
        mvprintw(wd.bottom_bar_row, 0, "%s%s", message, new_name);
        attroff(COLOR_PAIR(2));

        ch = getch();

        if (ch == KEY_BACKSPACE || ch == 127) {
            if (column > message_len) {
                i--;
                new_name[i] = '\0';
                column--;
                mvaddch(wd.bottom_bar_row, column, ' ');
                move(wd.bottom_bar_row, column);
            } 
            continue;
        } else if (ch == KEY_ENTER || ch == K_ENTER) {
            if (column > message_len) {
                break;
            } else {
                show_message_bottom_bar("Filename cannot be empty");
                continue;
            }
        }

        attron(COLOR_PAIR(2));
        mvaddch(wd.bottom_bar_row, column++, ch);
        attroff(COLOR_PAIR(2));
        new_name[i++] = ch;  
        if (column >= wd.term_width - 1) {
            break;
        }
    }

    new_name[i] = '\0';

    attron(COLOR_PAIR(2));
    mvprintw(wd.bottom_bar_row, 0, "Create file %s? [y/n]", new_name);
    attroff(COLOR_PAIR(2));
    ch = getch();
    if (ch == 'y' || ch == 'Y') {
        if (create_file(new_name)) {
            *wd.current_block = get_dirblock(wd.current_block->path, wd.current_block->column);
            show_message_bottom_bar("File created successfully");
        } else {
            show_message_bottom_bar("There was an error creating the file");
        }
        free(new_name);
    } else {
        show_message_bottom_bar("File will not be created");
    }
}

void delete_bar() {
    int ch;
    bool deleted = false;
    attron(COLOR_PAIR(2));
    mvprintw(wd.bottom_bar_row, 0, "Are you sure you want to delete the file %s? [y/n]", wd.current_block->selected);
    attroff(COLOR_PAIR(2));

    ch = getch();
    if (ch == 'y' || ch == 'Y') {
        char *path = get_new_path(wd.current_block->path, wd.current_block->selected);
        if (path == NULL) {
            show_message_bottom_bar("Could not delete: error deleting file");
            return;
        }
        deleted = delete_file(path);
        free(path);
    } else if (ch == 'n' || ch == 'N') {
        return;
    } else {
        show_message_bottom_bar("Wrong option selected: file will not be deleted");
        return;
    }

    move(wd.bottom_bar_row, 0);
    clrtoeol();

    if (deleted) {
        attron(COLOR_PAIR(2));
        mvprintw(wd.bottom_bar_row, 0, "File deleted successfully, press any key to continue");
        attroff(COLOR_PAIR(2));
        *wd.current_block = get_dirblock(wd.current_block->path, wd.current_block->column);
    }
    getch();
}

bool copy_file(const char *src, const char *dest) {
    FILE *f_src = fopen(src, "rb");
    if (!f_src) {
        show_message_bottom_bar("Error opening source file");
        return false;
    }

    FILE *f_dest = fopen(dest, "wb");
    if (!f_dest) {
        show_message_bottom_bar("Error opening destiny file");
        fclose(f_src);
        return false;
    }

    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), f_src)) > 0) {
        fwrite(buffer, 1, bytes, f_dest);
    }

    fclose(f_src);
    fclose(f_dest);
    return true;
}

void move_bar(char *src) {
    int ch;
    ch = getch();
    mvprintw(wd.bottom_bar_row, 0, "Are you sure you want to move the file [y/n]");

    if (ch == 'y' || ch == 'Y') {
        copy_file(src, wd.current_block->path);
    } else if (ch == 'n' || ch == 'N') {
        show_message_bottom_bar("File will not be moved");
    } else {
        show_message_bottom_bar("Error moving file");
    }

}

bool rename_file(char *current_name, char *new_name) {
    struct stat st;

    char *current_path = get_new_path(wd.current_block->path, current_name);
    char *new_path = get_new_path(wd.current_block->path, new_name);

    if (current_path == NULL) {
        show_message_bottom_bar("Could not rename file: error finding current path");
        return false;
    }

    if (new_path == NULL) {
        show_message_bottom_bar("Could not rename file: error finding new path");
        return false;
    }

    if (stat(current_path, &st) != 0) {
        if (errno == ENOENT) {
            show_message_bottom_bar("Could not rename file: file does not exist!");
        } else {
            show_message_bottom_bar("Could not rename file: error verifying file");
        }
        return false;
    }

    if (stat(new_path, &st) == 0) {
        show_message_bottom_bar("Could not rename file: file already exist!");
        return false;
    }

    if (rename(current_path, new_path) == 0) {
        show_message_bottom_bar("File renamed!");
        return true;
    } else {
        show_message_bottom_bar("Error renaming file");
        return false;
    }

}

void rename_bar() {
    int ch;
    char *new_name = malloc(wd.term_width * sizeof(char));
    char message[] = "New name: ";
    int message_len = strlen(message);
    int column = message_len;
    int i = 0;

    while (1) {   
        move(wd.bottom_bar_row, 0);
        clrtoeol();
        attron(COLOR_PAIR(2));
        mvprintw(wd.bottom_bar_row, 0, "%s%s", message, new_name);
        attroff(COLOR_PAIR(2));

        ch = getch();

        if (ch == KEY_BACKSPACE || ch == 127) {
            if (column > message_len) {
                i--;
                new_name[i] = '\0';
                column--;
                mvaddch(wd.bottom_bar_row, column, ' ');
                move(wd.bottom_bar_row, column);
            } 
            continue;
        } else if (ch == KEY_ENTER || ch == K_ENTER) {
            if (column > message_len) {
                break;
            } else {
                show_message_bottom_bar("Filename cannot be empty");
                continue;
            }
        }

        attron(COLOR_PAIR(2));
        mvaddch(wd.bottom_bar_row, column++, ch);
        attroff(COLOR_PAIR(2));
        new_name[i++] = ch;  
        if (column >= wd.term_width - 1) {
            break;
        }
    }

    new_name[i] = '\0';

    attron(COLOR_PAIR(2));
    mvprintw(wd.bottom_bar_row, 0, "Rename file as %s? [y/n]", new_name);
    attroff(COLOR_PAIR(2));
    ch = getch();
    if (ch == 'y' || ch == 'Y') {
        if (rename_file(wd.current_block->selected, new_name)) {
            *wd.current_block = get_dirblock(wd.current_block->path, wd.current_block->column);
        } else {
            show_message_bottom_bar("There was an error renaming the file");
        }
        free(new_name);
    } else {
        show_message_bottom_bar("File will not be renamed");
    }
}

bool is_regular(char *path) {
    struct stat path_stat;

    if (stat(path, &path_stat) != 0) {
        // Error: file does not exist or cannot be accessed
        return false;
    }

    return S_ISREG(path_stat.st_mode);
}

bool is_printable(char *path) {
    if (!is_regular(path)) {
        return false;
    }

    int n = sizeof(TEXT_FILE_EXTENSIONS) / sizeof(TEXT_FILE_EXTENSIONS[0]);

    for (int i = 0; i < n; i++) {
        if(strstr(path, TEXT_FILE_EXTENSIONS[i]) != NULL) {
            return true;
        }
    }

    return false;
}

void print_overview(char *path) {

    if (!is_printable(path)) {
        return;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        return;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    int column = (wd.block_quantity >= 2) ? get_column_by_index(1) : get_column_by_index(0);
    int quantity = 0;
    int max_width = (wd.term_width / 2) - 10;

    do
    {
        if ((read = getline(&line, &len, fp)) == -1) {
            break;
        }
        mvaddnstr(wd.box_row + 1 + quantity, column, line, max_width);
        quantity++;
    } while (quantity < wd.term_height - 4);
    
    free(line);
    fclose(fp);
}

void start_loop()
{
    int ch;   
    char *file_to_copy;
    char *path_to_copy;
    while (1)
    {
        clear(); // Limpiar pantalla
        print_blocks(wd.blocks, wd.block_quantity);
        print_bottom_bar(wd.current_block->selected, wd.current_block->path);
        print_top_bar(wd.current_block->path, wd.current_block->selected);
        print_borders(wd.blocks, wd.block_quantity, wd.box_row);
        print_overview(get_new_path(wd.current_block->path, wd.current_block->selected));
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
                char *newpath = get_new_path(wd.current_block->path, wd.current_block->selected);
                if (newpath != NULL) {
                    if (is_directory(newpath)) {
                        add_block();
                    }
                }
                break;
            }
            case 'd': 
            {
                delete_bar();
                break;
            }
            case 'r':
            {
                rename_bar();
                break;
            }
            case 'n':
            {
                new_file_bar();
                break;
            }
            case 'm':
            {
                if (!wd.moving_file) {
                    path_to_copy = strdup(wd.current_block->path);
                    file_to_copy = strdup(wd.current_block->selected);
                    char *path = get_new_path(wd.current_block->path, wd.current_block->selected);
                    wd.moving_file = true;
                } else {
                    wd.moving_file = false;
                }
                break;
            }
            case K_ENTER:
            {
                if (wd.moving_file) {
                    char *src = get_new_path(path_to_copy, file_to_copy);
                    char *dst = get_new_path(wd.current_block->path, file_to_copy);
                    if (src == NULL || dst == NULL) {
                        break;
                    }
                    if (copy_file(src, dst)) {
                        wd.moving_file = false;
                        *wd.current_block = get_dirblock(wd.current_block->path, wd.current_block->column);
                        show_message_bottom_bar("File moved successfully");
                    }
                }
                break;
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
    if (wd.term_width < 32) {
        printf("Terminal should should have a width greater of equal than 32\n");
        return EXIT_FAILURE;
    }
    start_loop();

    // Terminar ncurses
    return 0;
}
