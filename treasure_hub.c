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

pid_t monitorID = -1;
int monitorStatus = 0;


void menu() {
    printf("\nChoose one of the following commands:\n");
    printf("start_monitor\n");
    printf("list_hunts\n");
    printf("list_treasure\n");
    printf("view_treasure\n");
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

void list_hunts_internal() {
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

            printf("Hunt name: %s\n", entry->d_name);
            printf("Number of treasures: %d\n", count_treasures(entry->d_name));
        }
    }

    closedir(dir);
}

void handle_signal(int sig) {
    if (sig == SIGUSR1) {
        system("clear");
        list_hunts_internal();
        menu();
    } else if (sig == SIGUSR2) {
        system("clear");
        char hunt[64];
        printf("Enter hunt name: ");
        fgets(hunt, sizeof(hunt), stdin);
        hunt[strcspn(hunt, "\n")] = '\0';

        char cmd[128];
        snprintf(cmd, sizeof(cmd), "./treasure_manager --list %s", hunt);
        system(cmd);
        menu();
    } else if (sig == SIGINT) {
        system("clear");
        char hunt[64], idstr[16];
        printf("Enter hunt name: ");
        fgets(hunt, sizeof(hunt), stdin);
        hunt[strcspn(hunt, "\n")] = '\0';
        printf("Enter treasure ID: ");
        fgets(idstr, sizeof(idstr), stdin);
        idstr[strcspn(idstr, "\n")] = '\0';

        char cmd[128];
        snprintf(cmd, sizeof(cmd), "./treasure_manager --view %s %s", hunt, idstr);
        system(cmd);
        menu();
    } else if (sig == SIGTERM) {
        printf("Monitor exiting...\n");
        _exit(0);
    }
}

int start_monitor() {
    if (monitorID > 0) {
        printf("Monitor is already running.\n");
        return -1;
    }

    monitorID = fork();

    if (monitorID < 0) {
        perror("fork failed");
        return -1;
    }

    if (monitorID == 0) {
        // child process
        struct sigaction sa = {0};
        sa.sa_handler = handle_signal;
        sigemptyset(&sa.sa_mask);

        sigaction(SIGUSR1, &sa, NULL);
        sigaction(SIGUSR2, &sa, NULL);
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);

        while (1) pause();
        _exit(0);
    }

    monitorStatus = 1;
    return 0;
}

void list_hunts() {
    if (!monitorStatus) {
        printf("Monitor is not running.\n");
        return;
    }
    kill(monitorID, SIGUSR1);
    sleep(1);
}

void list_treasure() {
    if (!monitorStatus) {
        printf("Monitor is not running.\n");
        return;
    }
    kill(monitorID, SIGUSR2);
    sleep(1);
}

void view_treasure() {
    if (!monitorStatus) {
        printf("Monitor is not running.\n");
        return;
    }
    kill(monitorID, SIGINT);
    sleep(1);
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
}

int main() {
    char command[64];

    while (1) {
        menu();
        if (!fgets(command, sizeof(command), stdin)) break;

        if (strcmp(command, "start_monitor\n") == 0) {
            start_monitor();
        } else if (strcmp(command, "list_hunts\n") == 0) {
            list_hunts();
        } else if (strcmp(command, "list_treasure\n") == 0) {
            list_treasure();
        } else if (strcmp(command, "view_treasure\n") == 0) {
            view_treasure();
        } else if (strcmp(command, "stop_monitor\n") == 0) {
            stop_monitor();
        } else if (strcmp(command, "exit\n") == 0) {
            if (monitorStatus) {
                printf("Monitor still running. Use stop_monitor first.\n");
            } else {
                break;
            }
        } else {
            printf("Unknown command.\n");
        }
    }

    return 0;
}
