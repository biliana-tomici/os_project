#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>

#define USERNAME_MAX 32
#define CLUE_MAX 256
#define PATH_MAX_LEN 512

typedef struct {
    int id;
    char username[USERNAME_MAX];
    float latitude;
    float longitude;
    char clue[CLUE_MAX];
    int value;
} Treasure;

// Logs an action with timestamp to a log file
void log_action(int log_fd, const char* action) {
    char buf[512];
    time_t now = time(NULL);
    int len = snprintf(buf, sizeof(buf), "[%ld] %s\n", now, action);
    write(log_fd, buf, len);
}

// Generates path to treasure data file
void get_treasure_file_path(char *dest, const char *hunt_id) {
    snprintf(dest, PATH_MAX_LEN, "%s/treasures.dat", hunt_id);
}

// Generates path to log file
void get_log_file_path(char *dest, const char *hunt_id) {
    snprintf(dest, PATH_MAX_LEN, "%s/logged_hunt", hunt_id);
}

// Creates a symbolic link to the log file for the hunt
void create_symlink(const char *hunt_id) {
    char target[PATH_MAX_LEN];
    char link_name[PATH_MAX_LEN];

    // Get absolute path to the log file
    snprintf(target, sizeof(target), "%s/logged_hunt", hunt_id);
    char abs_target[PATH_MAX_LEN];
    realpath(target, abs_target);  // resolve to absolute path

    snprintf(link_name, sizeof(link_name), "logged_hunt-%s", hunt_id);

    // Remove existing symlink if it exists
    unlink(link_name);

    // Create the symbolic link
    if (symlink(abs_target, link_name) < 0) {
        perror("symlink creation failed");
    }
}

// Helper function to write string to stdout
void write_str(const char *str) {
    write(STDOUT_FILENO, str, strlen(str));
}

// Adds a treasure to the hunt
void add_treasure(const char *hunt_id) {
    mkdir(hunt_id, 0755);  // create hunt directory if not exists

    char file_path[PATH_MAX_LEN];
    get_treasure_file_path(file_path, hunt_id);

    int fd = open(file_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        perror("Failed to open treasure file");
        return;
    }

    Treasure t;
    char buf[256];

    write_str("Enter ID: ");
    read(STDIN_FILENO, buf, sizeof(buf));
    sscanf(buf, "%d", &t.id);

    write_str("Enter username: ");
    read(STDIN_FILENO, buf, sizeof(buf));
    sscanf(buf, "%s", t.username);

    write_str("Enter latitude: ");
    read(STDIN_FILENO, buf, sizeof(buf));
    sscanf(buf, "%f", &t.latitude);

    write_str("Enter longitude: ");
    read(STDIN_FILENO, buf, sizeof(buf));
    sscanf(buf, "%f", &t.longitude);

    write_str("Enter clue: ");
    read(STDIN_FILENO, t.clue, CLUE_MAX);
    t.clue[strcspn(t.clue, "\n")] = 0;

    write_str("Enter value: ");
    read(STDIN_FILENO, buf, sizeof(buf));
    sscanf(buf, "%d", &t.value);

    write(fd, &t, sizeof(Treasure));
    close(fd);

    // Write log entry
    char log_path[PATH_MAX_LEN];
    get_log_file_path(log_path, hunt_id);
    int log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    log_action(log_fd, "Added a treasure");
    close(log_fd);

    // Create symlink after log file exists
    create_symlink(hunt_id);
}

// Lists all treasures in the hunt
void list_treasures(const char *hunt_id) {
    char file_path[PATH_MAX_LEN];
    get_treasure_file_path(file_path, hunt_id);

    struct stat st;
    if (stat(file_path, &st) < 0) {
        perror("Cannot stat file");
        return;
    }

    char output[512];
    int len = snprintf(output, sizeof(output), "Hunt: %s\nSize: %ld bytes\nLast Modified: %s", 
                       hunt_id, st.st_size, ctime(&st.st_mtime));
    write(STDOUT_FILENO, output, len);

    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        perror("Cannot open file");
        return;
    }

    Treasure t;
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        len = snprintf(output, sizeof(output), 
            "ID: %d | User: %s | Location: %.4f, %.4f | Value: %d\nClue: %s\n\n",
            t.id, t.username, t.latitude, t.longitude, t.value, t.clue);
        write(STDOUT_FILENO, output, len);
    }
    close(fd);

    // Write log entry
    char log_path[PATH_MAX_LEN];
    get_log_file_path(log_path, hunt_id);
    int log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    log_action(log_fd, "Listed all the treasures");
    close(log_fd);
}

// Views a specific treasure by ID
void view_treasure(const char *hunt_id, int id) {
    char file_path[PATH_MAX_LEN];
    get_treasure_file_path(file_path, hunt_id);

    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        perror("Can't open file");
        return;
    }

    Treasure t;
    int found = 0;
    char output[512];
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        if (t.id == id) {
            int len = snprintf(output, sizeof(output), 
                "Found Treasure ID %d:\nUser: %s\nLocation: %.4f, %.4f\nValue: %d\nClue: %s\n",
                t.id, t.username, t.latitude, t.longitude, t.value, t.clue);
            write(STDOUT_FILENO, output, len);
            found = 1;
            break;
        }
    }
    if (!found) {
        int len = snprintf(output, sizeof(output), "Treasure ID %d not found.\n", id);
        write(STDOUT_FILENO, output, len);
    }
    close(fd);

    // Write log entry
    char log_path[PATH_MAX_LEN];
    get_log_file_path(log_path, hunt_id);
    int log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    log_action(log_fd, "Viewed a treasure");
    close(log_fd);
}

// Removes a specific treasure by ID
void remove_treasure(const char *hunt_id, int id) {
    char file_path[PATH_MAX_LEN];
    get_treasure_file_path(file_path, hunt_id);

    char tmp_path[PATH_MAX_LEN];
    snprintf(tmp_path, PATH_MAX_LEN, "%s/tmp.dat", hunt_id);

    int fd = open(file_path, O_RDONLY);
    int tmp_fd = open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0 || tmp_fd < 0) {
        perror("Error opening files");
        return;
    }

    Treasure t;
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        if (t.id != id) {
            write(tmp_fd, &t, sizeof(Treasure));
        }
    }
    close(fd);
    close(tmp_fd);

    rename(tmp_path, file_path);

    // Write log entry
    char log_path[PATH_MAX_LEN];
    get_log_file_path(log_path, hunt_id);
    int log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    log_action(log_fd, "Removed a treasure");
    close(log_fd);
}

// Deletes a hunt and all associated files
void remove_hunt(const char *hunt_id) {
    char file_path[PATH_MAX_LEN];
    get_treasure_file_path(file_path, hunt_id);
    char log_path[PATH_MAX_LEN];
    get_log_file_path(log_path, hunt_id);

    unlink(file_path);
    unlink(log_path);

    char symlink_path[PATH_MAX_LEN];
    snprintf(symlink_path, PATH_MAX_LEN, "logged_hunt-%s", hunt_id);
    unlink(symlink_path);

    rmdir(hunt_id);

    char output[256];
    int len = snprintf(output, sizeof(output), "Removed hunt %s\n", hunt_id);
    write(STDOUT_FILENO, output, len);
}

// Main program entry
int main(int argc, char *argv[]) {
    if (argc < 3) {
        char usage[] = "Usage: ./treasure_manager --<operation> <hunt_id> [<id>]\n";
        write(STDOUT_FILENO, usage, strlen(usage));
        return 1;
    }

    const char *cmd = argv[1];
    const char *hunt_id = argv[2];

    if (strcmp(cmd, "--add") == 0) {
        add_treasure(hunt_id);
    } else if (strcmp(cmd, "--list") == 0) {
        list_treasures(hunt_id);
    } else if (strcmp(cmd, "--view") == 0 && argc >= 4) {
        int id = atoi(argv[3]);
        view_treasure(hunt_id, id);
    } else if (strcmp(cmd, "--remove_treasure") == 0 && argc >= 4) {
        int id = atoi(argv[3]);
        remove_treasure(hunt_id, id);
    } else if (strcmp(cmd, "--remove_hunt") == 0) {
        remove_hunt(hunt_id);
    } else {
        char error[] = "Invalid command or missing arguments.\n";
        write(STDOUT_FILENO, error, strlen(error));
    }

    return 0;
}
