#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BLOCK_SIZE 1024
#define MAX_BLOCKS 1000
#define MAX_FILES 100
#define MAX_FILENAME 32
#define MAX_PATH 256

typedef struct {
    char filename[MAX_FILENAME];
    size_t size;
    time_t created;
    time_t modified;
    int start_block;
    int num_blocks;
    int is_directory;
    int parent_dir;
} FileMetadata;

typedef struct {
    FileMetadata files[MAX_FILES];
    char blocks[MAX_BLOCKS][BLOCK_SIZE];
    int free_blocks[MAX_BLOCKS];
    int num_files;
    int current_dir;  // Index of the current directory
} FileSystem;

FileSystem fs;

// Initialize the file system
void init_filesystem() {
    memset(&fs, 0, sizeof(FileSystem));
    for (int i = 0; i < MAX_BLOCKS; i++) {
        fs.free_blocks[i] = 1;
    }

    // Create the root directory
    strcpy(fs.files[0].filename, "/");
    fs.files[0].is_directory = 1;
    fs.files[0].created = time(NULL);
    fs.files[0].modified = time(NULL);
    fs.files[0].parent_dir = 0;
    fs.num_files = 1;
    fs.current_dir = 0;  // Start in the root directory
}

// Get the full path of a file
void get_full_path(int file_index, char* path) {
    if (file_index == 0) {
        strcpy(path, "/");
        return;
    }

    char temp_path[MAX_PATH] = "";
    int current = file_index;

    while (current != 0) {
        char temp[MAX_PATH];
        snprintf(temp, sizeof(temp), "/%s%s",
                fs.files[current].filename,
                temp_path);
        strcpy(temp_path, temp);
        current = fs.files[current].parent_dir;
    }

    strcpy(path, temp_path);
}

// Find a file by its name in the current directory
int find_file_in_dir(const char* filename, int dir_index) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs.files[i].filename[0] != '\0' &&
            fs.files[i].parent_dir == dir_index &&
            strcmp(fs.files[i].filename, filename) == 0) {
            return i;
        }
    }
    return -1;
}

// Check if a directory is empty
int is_directory_empty(int dir_index) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs.files[i].filename[0] != '\0' &&
            fs.files[i].parent_dir == dir_index) {
            return 0; // Directory is not empty
        }
    }
    return 1; // Directory is empty
}

// Delete a directory and its contents recursively
int delete_directory_recursive(int dir_index) {
    if (!fs.files[dir_index].is_directory) {
        printf("Error: This is not a directory\n");
        return -1;
    }

    // Delete all files and subdirectories first
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs.files[i].filename[0] != '\0' &&
            fs.files[i].parent_dir == dir_index) {
            if (fs.files[i].is_directory) {
                delete_directory_recursive(i);
            } else {
                // Free the blocks of the file
                for (int j = 0; j < fs.files[i].num_blocks; j++) {
                    fs.free_blocks[fs.files[i].start_block + j] = 1;
                }
                memset(&fs.files[i], 0, sizeof(FileMetadata));
                fs.num_files--;
            }
        }
    }

    // Delete the directory itself
    memset(&fs.files[dir_index], 0, sizeof(FileMetadata));
    fs.num_files--;
    return 0;
}

// Create a new file or directory
int create_file(const char* filename, int is_directory) {
    if (fs.num_files >= MAX_FILES) {
        printf("Error: Maximum number of files reached\n");
        return -1;
    }

    // Check if the file already exists in the current directory
    if (find_file_in_dir(filename, fs.current_dir) != -1) {
        printf("Error: A file or directory with this name already exists\n");
        return -1;
    }

    // Find a free slot
    int file_slot = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs.files[i].filename[0] == '\0') {
            file_slot = i;
            break;
        }
    }

    if (file_slot == -1) {
        printf("Error: No free slot available\n");
        return -1;
    }

    FileMetadata* file = &fs.files[file_slot];
    strncpy(file->filename, filename, MAX_FILENAME - 1);
    file->size = 0;
    file->created = time(NULL);
    file->modified = time(NULL);
    file->start_block = -1;
    file->num_blocks = 0;
    file->is_directory = is_directory;
    file->parent_dir = fs.current_dir;
    fs.num_files++;

    return file_slot;
}

// Write to a file
int write_file(const char* filename, const char* content) {
    int file_index = find_file_in_dir(filename, fs.current_dir);
    if (file_index == -1) {
        printf("Error: File not found\n");
        return -1;
    }

    if (fs.files[file_index].is_directory) {
        printf("Error: Cannot write to a directory\n");
        return -1;
    }

    size_t content_length = strlen(content) + 1;
    int blocks_needed = (content_length + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // Free old blocks if the file existed already
    if (fs.files[file_index].start_block != -1) {
        for (int i = 0; i < fs.files[file_index].num_blocks; i++) {
            fs.free_blocks[fs.files[file_index].start_block + i] = 1;
        }
    }

    // Find contiguous blocks
    int start_block = -1;
    int consecutive_blocks = 0;
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (fs.free_blocks[i]) {
            if (consecutive_blocks == 0) start_block = i;
            consecutive_blocks++;
            if (consecutive_blocks == blocks_needed) break;
        } else {
            consecutive_blocks = 0;
        }
    }

    if (consecutive_blocks < blocks_needed) {
        printf("Error: Insufficient space\n");
        return -1;
    }

    // Allocate blocks and write the content
    for (int i = 0; i < blocks_needed; i++) {
        fs.free_blocks[start_block + i] = 0;
        size_t to_write = (i == blocks_needed - 1) ?
            content_length - (i * BLOCK_SIZE) : BLOCK_SIZE;
        memcpy(fs.blocks[start_block + i], content + (i * BLOCK_SIZE), to_write);
    }

    fs.files[file_index].start_block = start_block;
    fs.files[file_index].num_blocks = blocks_needed;
    fs.files[file_index].size = content_length;
    fs.files[file_index].modified = time(NULL);

    return 0;
}

// Read a file
char* read_file(const char* filename) {
    int file_index = find_file_in_dir(filename, fs.current_dir);
    if (file_index == -1) {
        printf("Error: File not found\n");
        return NULL;
    }

    if (fs.files[file_index].is_directory) {
        printf("Error: Cannot read a directory\n");
        return NULL;
    }

    if (fs.files[file_index].start_block == -1) {
        printf("Error: Empty file\n");
        return NULL;
    }

    char* content = malloc(fs.files[file_index].size);
    if (!content) {
        printf("Error: Memory allocation failed\n");
        return NULL;
    }

    size_t total_read = 0;
    for (int i = 0; i < fs.files[file_index].num_blocks; i++) {
        size_t to_read = (i == fs.files[file_index].num_blocks - 1) ?
            fs.files[file_index].size - (i * BLOCK_SIZE) : BLOCK_SIZE;
        memcpy(content + total_read,
               fs.blocks[fs.files[file_index].start_block + i],
               to_read);
        total_read += to_read;
    }

    return content;
}

// Delete a file
int delete_file(const char* filename) {
    int file_index = find_file_in_dir(filename, fs.current_dir);
    if (file_index == -1) {
        printf("Error: File not found\n");
        return -1;
    }

    if (fs.files[file_index].is_directory) {
        printf("Error: Use delete_directory_recursive for directories\n");
        return -1;
    }

    // Free blocks
    for (int i = 0; i < fs.files[file_index].num_blocks; i++) {
        fs.free_blocks[fs.files[file_index].start_block + i] = 1;
    }

    // Clear metadata
    memset(&fs.files[file_index], 0, sizeof(FileMetadata));
    fs.num_files--;

    return 0;
}

// List directory contents
void list_directory(int dir_index) {
    char path[MAX_PATH];
    get_full_path(dir_index, path);
    printf("\nContents of directory %s:\n", path);
    printf("Name | Size | Type | Last Modified\n");
    printf("----------------------------------------\n");

    for (int i = 0; i < MAX_FILES; i++) {
        if (fs.files[i].filename[0] != '\0' &&
            fs.files[i].parent_dir == dir_index) {
            char date_str[26];
            strcpy(date_str, ctime(&fs.files[i].modified));
            date_str[24] = '\0';  // Remove newline

            printf("%s | %llu | %s | %s\n",
                   fs.files[i].filename,
                   (unsigned long long)fs.files[i].size,
                   fs.files[i].is_directory ? "DIR" : "FILE",
                   date_str);
        }
    }
}

// Improved user interface
void print_prompt() {
    char current_path[MAX_PATH];
    get_full_path(fs.current_dir, current_path);
    printf("\n%s $ ", current_path);
}

void print_help() {
    printf("\nAvailable commands:\n");
    printf("mkdir <name> : Create a directory\n");
    printf("cd <name> : Change directory\n");
    printf("cd .. : Go up one level\n");
    printf("create <name> : Create a file\n");
    printf("write <name> <content> : Write to a file\n");
    printf("read <name> : Read a file\n");
    printf("delete <name> : Delete a file or directory\n");
    printf("ls : List directory contents\n");
    printf("pwd : Display current path\n");
    printf("help : Display help\n");
    printf("exit : Quit\n");
}

int main() {
    init_filesystem();
    char command[MAX_PATH];
    char arg1[MAX_PATH];
    char arg2[MAX_PATH];

    printf("File system initialized. Type 'help' for the list of commands.\n");

    while (1) {
        print_prompt();

        char line[1024];
        if (fgets(line, sizeof(line), stdin) == NULL) break;

        command[0] = arg1[0] = arg2[0] = '\0';
        sscanf(line, "%s %s %[^\n]", command, arg1, arg2);

        if (strcmp(command, "exit") == 0) {
            break;
        }
        else if (strcmp(command, "help") == 0) {
            print_help();
        }
        else if (strcmp(command, "pwd") == 0) {
            char path[MAX_PATH];
            get_full_path(fs.current_dir, path);
            printf("%s\n", path);
        }
        else if (strcmp(command, "ls") == 0) {
            list_directory(fs.current_dir);
        }
        else if (strcmp(command, "mkdir") == 0) {
            if (arg1[0] == '\0') {
                printf("Usage: mkdir <name>\n");
                continue;
            }
            int result = create_file(arg1, 1);
            if (result >= 0) {
                printf("Directory created successfully\n");
            }
        }
        else if (strcmp(command, "cd") == 0) {
            if (arg1[0] == '\0') {
                printf("Usage: cd <name> or cd ..\n");
                continue;
            }

            if (strcmp(arg1, "..") == 0) {
                if (fs.current_dir != 0) {  // If not already at root
                    fs.current_dir = fs.files[fs.current_dir].parent_dir;
                }
            } else {
                int dir_index = find_file_in_dir(arg1, fs.current_dir);
                if (dir_index == -1) {
                    printf("Error: Directory not found\n");
                } else if (!fs.files[dir_index].is_directory) {
                    printf("Error: This is not a directory\n");
                } else {
                    fs.current_dir = dir_index;
                }
            }
        }
        else if (strcmp(command, "create") == 0) {
            if (arg1[0] == '\0') {
                printf("Usage: create <name>\n");
                continue;
            }
            int result = create_file(arg1, 0);
            if (result >= 0) {
                printf("File created successfully\n");
            }
        }
        else if (strcmp(command, "write") == 0) {
            if (arg1[0] == '\0' || arg2[0] == '\0') {
                printf("Usage: write <name> <content>\n");
                continue;
            }
            if (write_file(arg1, arg2) == 0) {
                printf("Content written successfully\n");
            }
        }
        else if (strcmp(command, "read") == 0) {
            if (arg1[0] == '\0') {
                printf("Usage: read <name>\n");
                continue;
            }
            char* content = read_file(arg1);
            if (content) {
                printf("Content: %s\n", content);
                free(content);
            }
        }
        else if (strcmp(command, "delete") == 0) {
            if (arg1[0] == '\0') {
                printf("Usage: delete <name>\n");
                continue;
            }
            if (strcmp(arg1, "/") == 0) {
                printf("Error: Cannot delete the root directory\n");
                continue;
            }

            int index = find_file_in_dir(arg1, fs.current_dir);
            if (index == -1) {
                printf("Error: File or directory not found\n");
                continue;
            }

            if (fs.files[index].is_directory) {
                if (delete_directory_recursive(index) == 0) {
                    printf("Directory and its contents deleted successfully\n");
                }
            } else {
                if (delete_file(arg1) == 0) {
                    printf("File deleted successfully\n");
                }
            }
        }
        else {
            printf("Unrecognized command. Type 'help' for the list of commands.\n");
        }
    }

    return 0;
}