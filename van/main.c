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
    int current_dir;  // Index du répertoire courant
} FileSystem;

FileSystem fs;

// Initialisation du système de fichiers
void init_filesystem() {
    memset(&fs, 0, sizeof(FileSystem));
    for (int i = 0; i < MAX_BLOCKS; i++) {
        fs.free_blocks[i] = 1;
    }

    // Création du répertoire racine
    strcpy(fs.files[0].filename, "/");
    fs.files[0].is_directory = 1;
    fs.files[0].created = time(NULL);
    fs.files[0].modified = time(NULL);
    fs.files[0].parent_dir = 0;
    fs.num_files = 1;
    fs.current_dir = 0;  // On commence dans le répertoire racine
}

// Trouve le chemin complet d'un fichier
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

// Trouve un fichier par son nom dans le répertoire courant
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

// Vérifie si un répertoire est vide
int is_directory_empty(int dir_index) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs.files[i].filename[0] != '\0' &&
            fs.files[i].parent_dir == dir_index) {
            return 0;
        }
    }
    return 1;
}

// Supprime récursivement un répertoire et son contenu
int delete_directory_recursive(int dir_index) {
    if (!fs.files[dir_index].is_directory) {
        printf("Erreur: Ce n'est pas un répertoire\n");
        return -1;
    }

    // Supprime d'abord tous les fichiers et sous-répertoires
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs.files[i].filename[0] != '\0' &&
            fs.files[i].parent_dir == dir_index) {
            if (fs.files[i].is_directory) {
                delete_directory_recursive(i);
            } else {
                // Libère les blocs du fichier
                for (int j = 0; j < fs.files[i].num_blocks; j++) {
                    fs.free_blocks[fs.files[i].start_block + j] = 1;
                }
                memset(&fs.files[i], 0, sizeof(FileMetadata));
                fs.num_files--;
            }
        }
    }

    // Supprime le répertoire lui-même
    memset(&fs.files[dir_index], 0, sizeof(FileMetadata));
    fs.num_files--;
    return 0;
}

// Crée un nouveau fichier ou répertoire
int create_file(const char* filename, int is_directory) {
    if (fs.num_files >= MAX_FILES) {
        printf("Erreur: Nombre maximum de fichiers atteint\n");
        return -1;
    }

    // Vérifie si le fichier existe déjà dans le répertoire courant
    if (find_file_in_dir(filename, fs.current_dir) != -1) {
        printf("Erreur: Un fichier ou répertoire avec ce nom existe déjà\n");
        return -1;
    }

    // Trouve un emplacement libre
    int file_slot = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs.files[i].filename[0] == '\0') {
            file_slot = i;
            break;
        }
    }

    if (file_slot == -1) {
        printf("Erreur: Pas d'emplacement libre\n");
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

// Les fonctions write_file et read_file restent les mêmes...
// Fonction pour écrire dans un fichier
int write_file(int file_index, const char* content) {
    if (file_index < 0 || file_index >= MAX_FILES || fs.files[file_index].filename[0] == '\0') {
        printf("Erreur: Index de fichier invalide\n");
        return -1;
    }

    if (fs.files[file_index].is_directory) {
        printf("Erreur: Impossible d'écrire dans un répertoire\n");
        return -1;
    }

    size_t content_length = strlen(content) + 1;
    int blocks_needed = (content_length + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // Libérer les anciens blocs si le fichier existait déjà
    if (fs.files[file_index].start_block != -1) {
        for (int i = 0; i < fs.files[file_index].num_blocks; i++) {
            fs.free_blocks[fs.files[file_index].start_block + i] = 1;
        }
    }

    // Trouver des blocs contigus
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
        printf("Erreur: Espace insuffisant\n");
        return -1;
    }

    // Allouer les blocs et écrire le contenu
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

// Fonction pour lire un fichier
char* read_file(int file_index) {
    if (file_index < 0 || file_index >= MAX_FILES || fs.files[file_index].filename[0] == '\0') {
        printf("Erreur: Index de fichier invalide\n");
        return NULL;
    }

    if (fs.files[file_index].is_directory) {
        printf("Erreur: Impossible de lire un répertoire\n");
        return NULL;
    }

    if (fs.files[file_index].start_block == -1) {
        printf("Erreur: Fichier vide\n");
        return NULL;
    }

    char* content = malloc(fs.files[file_index].size);
    if (!content) {
        printf("Erreur: Allocation mémoire échouée\n");
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

// Fonction pour supprimer un fichier
int delete_file(int file_index) {
    if (file_index < 0 || file_index >= MAX_FILES || fs.files[file_index].filename[0] == '\0') {
        printf("Erreur: Index de fichier invalide\n");
        return -1;
    }

    if (fs.files[file_index].is_directory) {
        printf("Erreur: Utilisez delete_directory_recursive pour les répertoires\n");
        return -1;
    }

    // Libérer les blocs
    for (int i = 0; i < fs.files[file_index].num_blocks; i++) {
        fs.free_blocks[fs.files[file_index].start_block + i] = 1;
    }

    // Effacer les métadonnées
    memset(&fs.files[file_index], 0, sizeof(FileMetadata));
    fs.num_files--;

    return 0;
}

// Modifiez la fonction list_directory comme suit:
void list_directory(int dir_index) {
    char path[MAX_PATH];
    get_full_path(dir_index, path);
    printf("\nContenu du répertoire %s:\n", path);
    printf("Index | Nom | Taille | Type | Date modification\n");
    printf("----------------------------------------\n");

    for (int i = 0; i < MAX_FILES; i++) {
        if (fs.files[i].filename[0] != '\0' &&
            fs.files[i].parent_dir == dir_index) {
            char date_str[26];
            // Utiliser ctime au lieu de ctime_r
            strcpy(date_str, ctime(&fs.files[i].modified));
            date_str[24] = '\0';  // Supprime le \n

            printf("%d | %s | %llu | %s | %s\n",
                   i,
                   fs.files[i].filename,
                   (unsigned long long)fs.files[i].size,
                   fs.files[i].is_directory ? "DIR" : "FILE",
                   date_str);
        }
    }
}
// Interface utilisateur améliorée
void print_prompt() {
    char current_path[MAX_PATH];
    get_full_path(fs.current_dir, current_path);
    printf("\n%s $ ", current_path);
}


void print_help() {
    printf("\nCommandes disponibles:\n");
    printf("mkdir <nom> : Créer un répertoire\n");
    printf("cd <nom> : Changer de répertoire\n");
    printf("cd .. : Remonter d'un niveau\n");
    printf("create <nom> : Créer un fichier\n");
    printf("write <index> <contenu> : Écrire dans un fichier\n");
    printf("read <index> : Lire un fichier\n");
    printf("delete <index> : Supprimer un fichier ou répertoire\n");
    printf("ls : Lister le contenu du répertoire\n");
    printf("pwd : Afficher le chemin actuel\n");
    printf("help : Afficher l'aide\n");
    printf("exit : Quitter\n");
}

int main() {
    init_filesystem();
    char command[MAX_PATH];
    char arg1[MAX_PATH];
    char arg2[MAX_PATH];

    printf("Système de fichiers initialisé. Tapez 'help' pour la liste des commandes.\n");

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
                printf("Usage: mkdir <nom>\n");
                continue;
            }
            int result = create_file(arg1, 1);
            if (result >= 0) {
                printf("Répertoire créé avec succès\n");
            }
        }
        else if (strcmp(command, "cd") == 0) {
            if (arg1[0] == '\0') {
                printf("Usage: cd <nom> ou cd ..\n");
                continue;
            }

            if (strcmp(arg1, "..") == 0) {
                if (fs.current_dir != 0) {  // Si on n'est pas déjà à la racine
                    fs.current_dir = fs.files[fs.current_dir].parent_dir;
                }
            } else {
                int dir_index = find_file_in_dir(arg1, fs.current_dir);
                if (dir_index == -1) {
                    printf("Erreur: Répertoire non trouvé\n");
                } else if (!fs.files[dir_index].is_directory) {
                    printf("Erreur: Ce n'est pas un répertoire\n");
                } else {
                    fs.current_dir = dir_index;
                }
            }
        }
        else if (strcmp(command, "create") == 0) {
            if (arg1[0] == '\0') {
                printf("Usage: create <nom>\n");
                continue;
            }
            int result = create_file(arg1, 0);
            if (result >= 0) {
                printf("Fichier créé avec succès\n");
            }
        }
        else if (strcmp(command, "write") == 0) {
            if (arg1[0] == '\0' || arg2[0] == '\0') {
                printf("Usage: write <index> <contenu>\n");
                continue;
            }
            int index = atoi(arg1);
            if (write_file(index, arg2) == 0) {
                printf("Contenu écrit avec succès\n");
            }
        }
        else if (strcmp(command, "read") == 0) {
            if (arg1[0] == '\0') {
                printf("Usage: read <index>\n");
                continue;
            }
            int index = atoi(arg1);
            char* content = read_file(index);
            if (content) {
                printf("Contenu: %s\n", content);
                free(content);
            }
        }
        else if (strcmp(command, "delete") == 0) {
            if (arg1[0] == '\0') {
                printf("Usage: delete <index>\n");
                continue;
            }
            int index = atoi(arg1);
            if (index == 0) {
                printf("Erreur: Impossible de supprimer le répertoire racine\n");
                continue;
            }

            if (fs.files[index].is_directory) {
                if (delete_directory_recursive(index) == 0) {
                    printf("Répertoire et son contenu supprimés avec succès\n");
                }
            } else {
                if (delete_file(index) == 0) {
                    printf("Fichier supprimé avec succès\n");
                }
            }
        }
        else {
            printf("Commande non reconnue. Tapez 'help' pour la liste des commandes.\n");
        }
    }

    return 0;
}
