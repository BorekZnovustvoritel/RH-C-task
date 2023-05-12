#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>

#define MESSAGE_LENGTH 1024
#define SPLIT_SPACE_COUNT 3


volatile sig_atomic_t run = 1;

char** array;
int last_line = 0;
int lines = 2;

void clear_buffer(char* buffer, int size) {
    for (int i=0; i<size; i++) {
        buffer[i] = '\0';
    }
}

void init_array() {
    array = malloc(sizeof(char*) * lines);
    for (int i=0;i<lines;i++){
        array[i] = calloc(MESSAGE_LENGTH, sizeof(char));
    }
}

void add_to_array(const char* line) {
    if (last_line + 2 >= lines) {
        lines *= 2;
        char** tmp = (char**)realloc(array, lines * sizeof(char*));
        array = tmp;
        for (int i=((lines/2)-1); i<lines; i++){
            array[i] = calloc(MESSAGE_LENGTH, sizeof(char));
        }
    }
    strcpy(array[last_line], line);
    last_line++;
}

void to_file(char* line, char* file) {
    FILE* out_file = NULL;
    out_file = fopen(file, "a");
    if (out_file == NULL) {
        printf("Oh no, cringe!\n");
        return;
    }
    fprintf(out_file, "%s", line);
    fclose(out_file);
}

static void handleExit(int signum) {
    puts("This is the end.\n");
    system("logger 'Ending the logging process.\n'");
    run = 0;
}

void strip_message(char* in_message, char* out_buffer) {
    char temp;
    short no_of_blanks_seen = 0;
    int idx = 0;
    while (no_of_blanks_seen < SPLIT_SPACE_COUNT) {
        temp = in_message[idx];
        if (temp == '\0') {
            return;
        }
        else if (temp == ' ') {
            no_of_blanks_seen++;
        }
        idx++;
    }
    int out_idx = 0;
    while (temp != '\0') {
        temp = in_message[idx];
        out_buffer[out_idx] = temp;
        idx++;
        out_idx++;
    }
    printf("Ret value: %s", out_buffer);
}

void print_max() {
    char** array_of_used = malloc((last_line+1)*sizeof(char*));
    int* array_of_usages = calloc((last_line+1), sizeof(int));
    for (int i=0; i<=last_line; i++) {
        array_of_usages[i] = 1;
    }
    int no_of_used = 0;
    for (int i = 0; i<last_line; i++) {
        short found_in_used = 0;
        for (int j=0; j<no_of_used;j) {
            if (!strcmp(array_of_used[j], array[i])) {
                found_in_used = 1;
                break;
            }
        }
        if (!found_in_used) {
            array_of_used[no_of_used] = calloc(MESSAGE_LENGTH, sizeof(char));
            strcpy(array_of_used[no_of_used], array[i]);
            for (int j=(i+1); j<=last_line; j++) {
                if (!strcmp(array[i], array[j])) {
                    array_of_usages[i] += 1;
                }
            }
        }
    }
    int max_usages = 0;
    int max_usage_idx;
    for (int i=0; i<=last_line; i++) {
        if (array_of_usages[i] > max_usages) {
            max_usages = array_of_usages[i];
            max_usage_idx = i;
        }
    }
    printf("\n\nMost common message: '%s' with usage count: %d.\nTotal messages=%d\n",array[max_usage_idx], max_usages, (last_line + 1));
}

int main(int argc, char *argv[]) {
    init_array();
    char buffer[MESSAGE_LENGTH];
    char message[MESSAGE_LENGTH];
    char* destination = "/dev/log";
    unlink(destination);
    int sock_out = socket(AF_LOCAL, SOCK_DGRAM, 0);
    if (sock_out < 0) return 1;
    struct sockaddr_un namesock;
    memset(&namesock, 0, sizeof(namesock));
    namesock.sun_family = AF_UNIX;
    strncpy(namesock.sun_path, destination, sizeof(namesock.sun_path));
    namesock.sun_path[sizeof(namesock.sun_path)-1] = '\0';
    int bind_out = bind(sock_out, (struct sockaddr*)&namesock, SUN_LEN(&namesock));
    int chmod_out = chmod(destination, 0666);
    int listen_out = listen(sock_out, 3);
    int accept_out = accept(sock_out, (struct sockaddr*)&namesock, sizeof(namesock));
    printf("Sock_out: %d, Bind_out: %d, chmod_out: %d\n", sock_out, bind_out, chmod_out);

    if (sock_out < 0 || bind_out) {
        printf("Some error has occured.");
        return 1;
    }
    signal(SIGINT, handleExit);

    while (run) {
        clear_buffer(buffer, MESSAGE_LENGTH);
        clear_buffer(message, MESSAGE_LENGTH);
        read(sock_out, buffer, MESSAGE_LENGTH);
        strip_message(buffer, message);
        printf("%s", buffer);
        add_to_array(message);
        for (int i = 1; i< argc; i++){
            to_file(buffer, argv[i]);
        }
    }
    close(sock_out);
    printf("Exiting now\n");
    for (int i=0; i<lines; i++){
        if (array[i][0] != '\0') {
            //printf("%s", array[i]);
        }
        else {
            continue;
        }
    }
    print_max();
    return 0;
}