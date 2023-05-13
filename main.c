#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> // socket-related
#include <unistd.h>     // socket-related
#include <sys/un.h>     // socket-related
#include <sys/stat.h>   // chmod requires this
#include <string.h>     // string manipulation
#include <signal.h>     // SIGINT handling

// Allowed msg length.
#define MESSAGE_LENGTH 1024
// After SPLIT_SPACE_COUNT spaces, the actual message starts.
#define SPLIT_SPACE_COUNT 3

// Controls the exit process.
volatile sig_atomic_t run = 1;

// Used for storing the messages.
char** array;
int next_line = 0;
int lines = 2;

// Helper function to flush a string buffer in the stack. Param size is the buffer size.
void clear_buffer(char* buffer, int size) {
    for (int i=0; i<size; i++) {
        buffer[i] = '\0';
    }
}

// Initiate the array of strings that will hold the messages.
void init_array() {
    array = malloc(sizeof(char*) * lines);
    for (int i=0;i<lines;i++){
        array[i] = calloc(MESSAGE_LENGTH, sizeof(char));
    }
}

// Copy the contents of the buffer into the heap-located array defined in the beginning of this file.
void add_to_array(const char* line) {
    if (next_line + 2 >= lines) {
        lines *= 2;
        char** tmp = (char**)realloc(array, lines * sizeof(char*));
        array = tmp;
        for (int i=(lines/2); i<lines; i++){
            array[i] = calloc(MESSAGE_LENGTH, sizeof(char));
        }
    }
    strcpy(array[next_line], line);
    next_line++;
}

// Copy line into the specified file. Appends lines.
void to_file(char* line, char* file) {
    FILE* out_file = NULL;
    out_file = fopen(file, "a");
    if (out_file == NULL) {
        printf("I cannot write to %s!\n", file);
        return;
    }
    fprintf(out_file, "%s\n", line);
    fclose(out_file);
}

// Handles the SIGINT. logger call is needed not to wait for more data from the socket before closing.
static void handleExit(int signum) {
    run = 0;
    system("logger 'Ending the logging process.'");
}

// Clears the beginning (icluding time) from the log messages. Only the athor and message are kept.
// The output is written into the out_buffer string.
void strip_message(char* in_message, char* out_buffer) {
    char temp;
    short no_of_blanks_seen = 0;
    int idx = 0;
    // Iterate over the skipped part.
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
    // Iterate over the message part.
    while (temp != '\0') {
        temp = in_message[idx];
        out_buffer[out_idx] = temp;
        idx++;
        out_idx++;
    }
}

// Prints the summary after SIGINT.
void print_max() {
    char** array_of_used = malloc((next_line+1)*sizeof(char*)); // Counted messages. Same messages are not repeated.
    int* array_of_usages = calloc((next_line+1), sizeof(int)); // Count of messages.
    // Every message is used at least once.
    for (int i=0; i<=next_line; i++) {
        array_of_usages[i] = 1;
    }
    int no_of_used = 0;
    // Iterate over every message.
    for (int i = 0; i<next_line; i++) {
        // Has this message been counted already?
        short found_in_used = 0;
        for (int j=0; j<no_of_used;j) {
            if (!strcmp(array_of_used[j], array[i])) {
                found_in_used = 1;
                break;
            }
        }
        // If not, count it.
        if (!found_in_used) {
            array_of_used[no_of_used] = calloc(MESSAGE_LENGTH, sizeof(char));
            strcpy(array_of_used[no_of_used], array[i]);
            // And look for the same messages in the remaining ones.
            for (int j=(i+1); j<=next_line; j++) {
                if (!strcmp(array[i], array[j])) {
                    array_of_usages[i] += 1;
                }
            }
        }
        // Else: just continue to the next  message.
    }
    int max_usages = 0;
    int max_usage_idx;
    // Find which message was counted the most (in case of a tie, the first one is shown).
    for (int i=0; i<=next_line; i++) {
        if (array_of_usages[i] > max_usages) {
            max_usages = array_of_usages[i];
            max_usage_idx = i;
        }
    }
    printf("\n\nTotal messages: %d\nMost common message:'%s' with usage count: %d.\n", next_line, array[max_usage_idx], max_usages);
}

int main(int argc, char *argv[]) {
    init_array();
    char out_of_socket_buffer[MESSAGE_LENGTH];
    char message[MESSAGE_LENGTH];
    // And now all the socket-related business.
    char* destination = "/dev/log";
    unlink(destination);
    int sock_descriptor = socket(AF_LOCAL, SOCK_DGRAM, 0);
    if (sock_descriptor < 0) {
        printf("Cannot create a local socket. Please run using sudo.");
        return 1;
    }
    struct sockaddr_un namesock;
    memset(&namesock, 0, sizeof(namesock));
    namesock.sun_family = AF_UNIX;
    strncpy(namesock.sun_path, destination, sizeof(namesock.sun_path));
    namesock.sun_path[sizeof(namesock.sun_path)-1] = '\0';
    int bind_out = bind(sock_descriptor, (struct sockaddr*)&namesock, SUN_LEN(&namesock));
    if (bind_out) {
        printf("Binding error occured. Please run using sudo.");
        return 1;
    }
    int chmod_out = chmod(destination, 0666);
    socklen_t addr_len = sizeof(struct sockaddr_un);
    int accept_out = accept(sock_descriptor, (struct sockaddr*)&namesock, &addr_len);

    // Handle SIGINT.
    signal(SIGINT, handleExit);
    while (run) {
        clear_buffer(out_of_socket_buffer, MESSAGE_LENGTH);
        clear_buffer(message, MESSAGE_LENGTH);
        read(sock_descriptor, out_of_socket_buffer, MESSAGE_LENGTH);
        strip_message(out_of_socket_buffer, message);
        if(out_of_socket_buffer[0] != '\0') {
            printf("%s\n", out_of_socket_buffer);   
        }
        add_to_array(message);
        for (int i = 1; i< argc; i++){
            to_file(out_of_socket_buffer, argv[i]);
        }
    }
    close(sock_descriptor);
    printf("Exiting now\n");
    print_max();
    return 0;
}