#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct {
    int id;
    char username[32];
    float latitude;
    float longitude;
    char clue[256];
    int value;
} Treasure;

typedef struct UserScore {
    char username[32];
    int total;
    struct UserScore *next;
} UserScore;

UserScore* find_or_create(UserScore **head, const char *username) {
    UserScore *cur = *head;
    while (cur) {
        if (strcmp(cur->username, username) == 0)
            return cur;
        cur = cur->next;
    }
    UserScore *new_node = malloc(sizeof(UserScore));
    if (!new_node) {
        perror("malloc failed");
        exit(1);
    }
    strncpy(new_node->username, username, 31);
    new_node->username[31] = '\0';
    new_node->total = 0;
    new_node->next = *head;
    *head = new_node;
    return new_node;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <hunt>\n", argv[0]);
        return 1;
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/treasures.dat", argv[1]);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open treasure file");
        return 1;
    }

    Treasure t;
    UserScore *head = NULL;

    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        UserScore *user = find_or_create(&head, t.username);
        user->total += t.value;
    }
    close(fd);

    UserScore *cur = head;
    while (cur) {
        printf("%s: %d\n", cur->username, cur->total);
        UserScore *tmp = cur;
        cur = cur->next;
        free(tmp);
    }

    return 0;
}
