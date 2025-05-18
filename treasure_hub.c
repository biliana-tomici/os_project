#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>

pid_t monitorID = -1;
int monitorStatus = 0;
int monitor_pipe[2]; // pipe: monitor writes -> main reads

void menu() {
    printf("\nChoose one of the following commands:\n");
    printf("start_monitor\n");
    printf("list_hunts\n");
    printf("list_treasure\n");
    printf("view_treasure\n");
    printf("calculate_score\n");   
    printf("stop_monitor\n");
    printf("exit\n");
}

int count_treasures(const char *hunt) {
    char path[256];
    snprintf(path, sizeof(path), "%s/treasures.dat", hunt);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;

    int count = 0;
    char buffer[sizeof(int) + 32 + sizeof(float)*2 + 256 + sizeof(int)];
    while (read(fd, buffer, sizeof(buffer)) == sizeof(buffer)) {
        count++;
    }
    close(fd);
    return count;
}

// This function lists hunts by scanning current directory and writing output to pipefd[1]
void list_hunts_internal(int pipefd[2]) {
    DIR *dir = opendir(".");
    if (!dir) {
        dprintf(pipefd[1], "opendir failed: %s\n", strerror(errno));
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (entry->d_type == DT_DIR &&
            strcmp(entry->d_name, ".") != 0 &&
            strcmp(entry->d_name, "..") != 0 &&
            strcmp(entry->d_name, ".git") != 0) {

            dprintf(pipefd[1], "Hunt name: %s\n", entry->d_name);
            dprintf(pipefd[1], "Number of treasures: %d\n", count_treasures(entry->d_name));
        }
    }
    closedir(dir);
}

void handle_signal(int sig) {
    if (sig == SIGUSR1) {
        list_hunts_internal(monitor_pipe);
    } else if (sig == SIGUSR2) {
        char hunt[64];
        dprintf(monitor_pipe[1], "Enter hunt name: ");
        dprintf(monitor_pipe[1], "(Use main interface to input hunt name)\n");
    } else if (sig == SIGINT) {
        dprintf(monitor_pipe[1], "(View treasure: Use main interface for input)\n");
    } else if (sig == SIGTERM) {
        dprintf(monitor_pipe[1], "Monitor exiting...\n");
        _exit(0);
    }
}

// Start monitor with pipe communication (monitor writes to pipe[1], main reads pipe[0])
int start_monitor() {
    if (monitorID > 0) {
        printf("Monitor is already running.\n");
        return -1;
    }

    if (pipe(monitor_pipe) == -1) {
        perror("pipe");
        return -1;
    }

    monitorID = fork();

    if (monitorID < 0) {
        perror("fork failed");
        return -1;
    }

    if (monitorID == 0) {
        // child process - monitor
        close(monitor_pipe[0]); // close read end, monitor writes

        struct sigaction sa = {0};
        sa.sa_handler = handle_signal;
        sigemptyset(&sa.sa_mask);

        sigaction(SIGUSR1, &sa, NULL);
        sigaction(SIGUSR2, &sa, NULL);
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);

        // Keep alive and handle signals
        while (1) pause();
        _exit(0);
    }

    // parent process
    close(monitor_pipe[1]); // close write end, main reads monitor_pipe[0]
    monitorStatus = 1;
    return 0;
}

void read_and_print_monitor_output() {
    // Non-blocking or blocking read until no more data
    char buf[512];
    ssize_t n;
    while ((n = read(monitor_pipe[0], buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        printf("%s", buf);
        if (n < sizeof(buf) - 1) break; // assume no more data immediately
    }
}

void list_hunts() {
    if (!monitorStatus) {
        printf("Monitor is not running.\n");
        return;
    }
    kill(monitorID, SIGUSR1);
    sleep(1);
    read_and_print_monitor_output();
}

void list_treasure() {
    if (!monitorStatus) {
        printf("Monitor is not running.\n");
        return;
    }
    kill(monitorID, SIGUSR2);
    sleep(1);
    read_and_print_monitor_output();
}

void view_treasure() {
    if (!monitorStatus) {
        printf("Monitor is not running.\n");
        return;
    }
    kill(monitorID, SIGINT);
    sleep(1);
    read_and_print_monitor_output();
}

void stop_monitor() {
    if (!monitorStatus || monitorID < 0) {
        printf("Monitor is not running.\n");
        return;
    }

    kill(monitorID, SIGTERM);
    waitpid(monitorID, NULL, 0);
    printf("Monitor %d stopped.\n", monitorID);
    monitorID = -1;
    monitorStatus = 0;

    // Close pipe read end
    close(monitor_pipe[0]);
}

void calculate_score() {
    DIR *dir = opendir(".");
    if (!dir) {
        perror("opendir failed");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (entry->d_type == DT_DIR &&
            strcmp(entry->d_name, ".") != 0 &&
            strcmp(entry->d_name, "..") != 0 &&
            strcmp(entry->d_name, ".git") != 0) {

            int pipefd[2];
            if (pipe(pipefd) == -1) {
                perror("pipe");
                closedir(dir);
                return;
            }

            pid_t pid = fork();
            if (pid < 0) {
                perror("fork");
                close(pipefd[0]);
                close(pipefd[1]);
                closedir(dir);
                return;
            }

            if (pid == 0) {
                // child: run score_calculator with hunt name, output to pipe
                close(pipefd[0]); // close read end
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);

                execl("./score_calculator", "score_calculator", entry->d_name, NULL);
                perror("execl failed");
                _exit(1);
            } else {
                // parent: read child's output
                close(pipefd[1]); // close write end
                printf("Scores for hunt: %s\n", entry->d_name);

                char buffer[512];
                ssize_t n;
                while ((n = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
                    buffer[n] = '\0';
                    printf("%s", buffer);
                }
                close(pipefd[0]);
                waitpid(pid, NULL, 0);
                printf("\n");
            }
        }
    }

    closedir(dir);
}

void trim_newline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && str[len-1] == '\n') {
        str[len-1] = '\0';
    }
}

void list_treasure_for_hunt(const char *hunt) {
    char path[256];
    snprintf(path, sizeof(path), "%s/treasures.dat", hunt);
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("Could not open treasures for hunt '%s'\n", hunt);
        return;
    }

    struct {
        int id;
        char username[32];
        float lat;
        float lon;
        char clue[256];
        int value;
    } treasure;

    printf("Treasures in hunt '%s':\n", hunt);
    while (read(fd, &treasure, sizeof(treasure)) == sizeof(treasure)) {
        printf("ID: %d | User: %s | GPS: (%.2f, %.2f) | Clue: %s | Value: %d\n",
               treasure.id, treasure.username, treasure.lat, treasure.lon, treasure.clue, treasure.value);
    }

    close(fd);
}



void view_treasure_for_hunt(const char *hunt) {
    int id;
    printf("Enter treasure ID to view: ");
    if (scanf("%d", &id) != 1) {
        printf("Invalid input.\n");
        while (getchar() != '\n'); 
        return;
    }
    while (getchar() != '\n'); 

    char path[256];
    snprintf(path, sizeof(path), "%s/treasures.dat", hunt);
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("Could not open treasure file for hunt '%s'\n", hunt);
        return;
    }

    struct {
        int id;
        char username[32];
        float lat;
        float lon;
        char clue[256];
        int value;
    } treasure;

    int found = 0;
    while (read(fd, &treasure, sizeof(treasure)) == sizeof(treasure)) {
        if (treasure.id == id) {
            printf("Treasure ID: %d\nUser: %s\nGPS: %.2f, %.2f\nClue: %s\nValue: %d\n",
                   treasure.id, treasure.username, treasure.lat, treasure.lon, treasure.clue, treasure.value);
            found = 1;
            break;
        }
    }

    if (!found) {
        printf("Treasure with ID %d not found in hunt '%s'.\n", id, hunt);
    }

    close(fd);
}


int main() {
    char line[128];
    while (1) {
        menu();
        if (!fgets(line, sizeof(line), stdin)) break;
        trim_newline(line);

        // Parse command and optional argument
        char *cmd = strtok(line, " ");
        char *arg = strtok(NULL, " "); // hunt name if any

        if (!cmd) continue;

        if (strcmp(cmd, "start_monitor") == 0) {
            start_monitor();
        } else if (strcmp(cmd, "list_hunts") == 0) {
            list_hunts();
        } else if (strcmp(cmd, "list_treasure") == 0) {
            if (arg) {
                // Argument given, handle in main
                list_treasure_for_hunt(arg);
            } else {
                
                if (!monitorStatus) {
                    printf("Monitor is not running.\n");
                } else {
                    printf("Please specify hunt name: list_treasure <hunt_name>\n");
                }
            }
        } else if (strcmp(cmd, "view_treasure") == 0) {
            if (arg) {
                view_treasure_for_hunt(arg);
            } else {
                if (!monitorStatus) {
                    printf("Monitor is not running.\n");
                } else {
                    printf("Please specify hunt name: view_treasure <hunt_name>\n");
                }
            }
        } else if (strcmp(cmd, "calculate_score") == 0) {
            calculate_score();
        } else if (strcmp(cmd, "stop_monitor") == 0) {
            stop_monitor();
        } else if (strcmp(cmd, "exit") == 0) {
            if (monitorStatus) {
                printf("Monitor still running. Use stop_monitor first.\n");
            } else {
                break;
            }
        } else {
            printf("Unknown command.\n");
        }
    }

    if (monitorStatus) stop_monitor();

    return 0;
}