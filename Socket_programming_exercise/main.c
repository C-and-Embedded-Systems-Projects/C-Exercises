// Include libraries
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>

// Definition section
#define BUFFER_SIZE 32
#define BUFFER_MULTIPLIER 1.5
#define CONTENT_TYPE "Content-Type: text/plain"
#define ACCEPT "Accept: */*"
#define CONTENT_LENGTH "Content-Length: "
#define HTTP_END_OF_LINE "\r\n"
#define POST "POST "
#define GET "GET "
#define HTTP_1_1 " HTTP/1.1"
#define PATCH "PATCH "
#define DELETE "DELETE "
#define PUT "PUT "
#define HOST "Host: "
#define TRUE 1
#define FALSE 0
#define TIMEOUT 60

// Function prototypes
// ----------------------------

int setup_socket(struct addrinfo *);

void handle_connection(int, struct addrinfo *, int);

char * build_http_request(const char *);

void send_http_request(int, const char *);

char * recieve_http_response(int);

struct addrinfo * get_domain_ip(const char *);

char * read_string(char *);

int read_int(void);

void clear_buffer(void);

int ask_to_continue(void);

// ----------------------------

// Main function, where the program starts
int main() {
    // Holds the domain name
    char *domain_name;
    // Holds the port number
    int port;
    // Pointer to the address info
    struct addrinfo *server_address;
    // Holds the socket file descriptor
    int sockfd;
    // Pointer to the HTTP request
    char *request;
    // Pointer to the HTTP response
    char *response;
    // Boolean to check if the user wants to send another request
    int continue_program;

    // Read the domain name
    printf("Enter domain name: ");
    domain_name = read_string(" \t\r\n");

    // Read the port number
    printf("Enter port number: ");
    port = read_int();

    // Get the IP address of the domain name
    server_address = get_domain_ip(domain_name);

    // Setup the socket and handle the connection
    sockfd = setup_socket(server_address);

    // Handle the connection
    handle_connection(sockfd, server_address, port);

    // Set the continue_program variable to true
    continue_program = TRUE;

    while (continue_program) {
        // Build the HTTP request
        request = build_http_request(domain_name);

        // Send the HTTP request
        send_http_request(sockfd, request);

        // Print the HTTP request
        printf("Request: %s\n", request);

        // Recieve the HTTP response
        response = recieve_http_response(sockfd);

        // Print the HTTP response
        printf("Response: %s", response);

        // Free the HTTP response and the request
        free(request);
        free(response);

        // Ask the user if they want to send another request
        continue_program = ask_to_continue();
    }

    // Close the socket
    close(sockfd);

    // Free the address info
    freeaddrinfo(server_address);

    // Free the domain name
    free(domain_name);

    // Return 0 to the operating system indicating success execution of the program
    return 0;
}

/**
 * Reads a string from standard input until a newline or EOF is encountered.
 *
 * This function dynamically allocates memory for the string and expands it 
 * as needed. It checks each character against a list of disallowed characters 
 * and skips any that are found, printing an error message for each disallowed 
 * character encountered. The resulting string is null-terminated.
 *
 * @param chars_not_allowed A string containing characters that are not allowed 
 *                          in the input. If any of these characters are 
 *                          encountered, they are skipped and an error message 
 *                          is printed.
 * @return A pointer to the dynamically allocated string containing the user's 
 *         input, excluding any disallowed characters.
 */
char * read_string(char *chars_not_allowed) {
    // Pointer to the string that will be returned
    char *string;
    // Current character read from standard input
    char input;
    // Size of the string
    int size = BUFFER_SIZE;
    // Current string length
    int length = 0;

    // Allocate memory for the string
    string = malloc(size * sizeof(char));

    // Check if memory allocation failed
    if (string == NULL) {
        printf("Error! Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    // Read characters from standard input until a newline or EOF is encountered
    while ((input = getchar()) != '\n' && input != EOF) {
        // Check if the current character is in the list of characters not allowed
        if (strchr(chars_not_allowed, input) != NULL) {
            // If it is, print error message and continue to the next character
            if (input == ' ' || input == '\t') {
                printf("Error! White space is not allowed\n");
            } else if (input == '\r') {
                printf("Error! Carriage return is not allowed\n");
            } else {
                printf("Error! '%c' is not allowed\n", input);
            }
            continue;
        }

        // If buffer is full, reallocate memory
        if (length >= size - 1) {
            size *= BUFFER_MULTIPLIER;
            string = realloc(string, size * sizeof(char));
            if (string == NULL) {
                printf("Error! Memory allocation failed");
                exit(EXIT_FAILURE);
            }
        }

        // Add the current character to the string
        string[length++] = input;
    }

    // Null-terminate the string
    string[length] = '\0';

    return string;
}

/**
 * Reads an integer from standard input.
 *
 * This function repeatedly prompts the user to enter an integer until a valid 
 * input is provided. If a non-integer input is detected, it clears the input 
 * buffer and prompts the user again. Upon successful input of a valid integer, 
 * the function returns the integer.
 *
 * @return The integer entered by the user.
 */
int read_int() {
    // Variable to store the integer read from standard input
    int number;

    // Loop until the user enters a valid integer
    while (scanf("%d", &number) != 1) {
        clear_buffer();
        printf("Invalid input. Please enter an integer: ");
    }

    clear_buffer();

    // Return the integer
    return number;
}

/**
 * Clears the input buffer until a newline character is encountered.
 *
 * This function is useful when a scanf() call fails and does not consume the
 * input, which can prevent subsequent input operations from working correctly.
 * It repeatedly reads a character from standard input until a newline character
 * is encountered, effectively clearing the input buffer.
 */
void clear_buffer() {
    // Clear the input buffer
    while (getchar() != '\n');
}

/**
 * Resolves a domain name to an IP address.
 *
 * @param domain_name The domain name to resolve.
 * @return A pointer to the addrinfo structure containing the resolved IP address.
 *         the program exits with an error if the DNS resolution fails.
 */
struct addrinfo * get_domain_ip(const char *domain_name) {
    // Variables used to turn server domian name into IP
    struct addrinfo hints, *res;

    // Zero out the hints structure so there are no garbage values
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  // We do not know if we are using IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP connection

    printf("Resolving domain: %s\n", domain_name);

    // Resolve domain to IP. If 0 is not returned, something has gone wrong
    // This function resolves the domain name to an IP and saves the result in res
    if (getaddrinfo(domain_name, NULL, &hints, &res) != 0) {
        printf("Error! DNS resolution failed\n");
        freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }

    printf("Domain resolved to: %s\n", res->ai_addr->sa_family == AF_INET ? "IPv4" : "IPv6");

    return res;
}

/**
 * Creates a socket and sets it to non-blocking mode.
 *
 * This function creates a socket and sets it to non-blocking mode. A non-blocking
 * socket is one that does not block the calling process if data is not available
 * on the socket. This is useful for performing I/O operations that do not block,
 * such as reading from a file descriptor.
 *
 * @param domain_info A pointer to the addrinfo structure containing the domain
 *                    information to use for creating the socket.
 * @return The socket file descriptor.
 */
int setup_socket(struct addrinfo *domain_info) {
    // Scocket file descriptor value from socket creation is saved in this variable
    int sockfd;

    printf("Creating socket...\n");  // Debugging line

    // Create socket. If a negative number is returned, something has gone wrong
    // This function creates a socket and returns a file descriptor that can be used to send and receive data
    if ((sockfd = socket(domain_info->ai_family, domain_info->ai_socktype, domain_info->ai_protocol)) < 0) {
        printf("Error! Socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    printf("Socket created: %d\n", sockfd);
    printf("Setting socket to non-blocking mode...\n");

    // Set socket to non-blocking mode
    if (fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0) {
        printf("Error! Failed to set non-blocking mode: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
 
    // printf("Socket set to non-blocking mode\n");

    // Return the socket file descriptor
    return sockfd;
}

/**
 * Connect to a server using the provided socket file descriptor, domain information and port.
 *
 * This function connects to a server using the provided socket file descriptor, domain information and port.
 * It will print a message to the user indicating the server it is trying to connect to and whether or not the
 * connection was successful. If the connection times out, it will print an error message and exit.
 *
 * @param sockfd The file descriptor of the socket to use for the connection.
 * @param domain_info A pointer to the addrinfo structure containing the domain information to use for the connection.
 * @param port The port number to use for the connection.
 */
void handle_connection(int sockfd, struct addrinfo *domain_info, int port) {
    struct sockaddr_in server_address;
    memcpy(&server_address, domain_info->ai_addr, sizeof(struct sockaddr_in));
    server_address.sin_port = htons(port);

    // Convert the IP address to a string
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(server_address.sin_family, &server_address.sin_addr, ip_str, sizeof(ip_str));

    printf("Connecting to %s:%d...\n", ip_str, port);

    // Try to connect (non-blocking), since we are using a non-blocking socket, the connect function
    // will return immediately, even if the connection is not yet established
    if (connect(sockfd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        // We need to check if the connection is in progress
        if (errno == EINPROGRESS) {
            printf("Connection in progress...\n");

            // Set up `select()` for timeout handling
            fd_set write_fds;
            struct timeval tv;
            tv.tv_sec = TIMEOUT;
            tv.tv_usec = 0;

            // Zero the created set
            FD_ZERO(&write_fds);
            FD_SET(sockfd, &write_fds);

            // Set up `select()` for timeout connection
            int sel_res = select(sockfd + 1, NULL, &write_fds, NULL, &tv);
            if (sel_res > 0) {  // Socket is writable, meaning connection is established, however, we do not know if connection succeeded do to asynchronous nature
                // Check if the connection succeeded
                int so_error;
                socklen_t len = sizeof(so_error);
                // This function stores the result of the connection in `so_error`
                getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);

                // If so_error is 0, the connection succeeded, otherwise it failed
                if (so_error == 0) {
                    printf("Connected to server\n");
                    return;
                } else {
                    printf("Connection failed: %s\n", strerror(so_error));
                    exit(EXIT_FAILURE);
                }
            // If the socket is not writable (sselect returns 0), it means the connection timed out,
            } else if (sel_res == 0) {
                printf("Error! Connection timeout\n");
                exit(EXIT_FAILURE);
            // If select returns -1, something has gone wrong
            } else {
                printf("Error! select() failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
        // If we are not in EINPROGRESS, something has gone wrong
        } else {
            printf("Error! Connection failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
}

/**
 * Builds an HTTP request given the host (server) to which the request is to be sent.
 *
 * This function asks the user for the HTTP method they want to use, the endpoint to which the request is to be sent
 * and whether or not they want to include a body in the request. It then builds the request accordingly and returns it.
 *
 * @param host The host (server) to which the request is to be sent.
 *
 * @return The HTTP request as a string.
 */
char * build_http_request(const char * host) {
    // Variabl to save the user's choice of the HTTP method
    int method_choice;
    // Varibale to hold the endpoint to which the request will be sent
    char *endpoint;
    // Variable to save request's body
    char *body;
    // Variable to save the request
    char *request;
    // request size is saved in this variable
    int request_size;
    // Content length is saved in this variable
    int content_length;
    // Variable to save the HTTP method
    char *method;
    // Number of carriage returns and new lines in the request
    int crlf_count;
    // Content length string
    char *content_length_string;
    
    // Loop until the user enters a valid choice for the HTTP method
    do {
        printf("Enter the HTTP method you want to use:\n");
        printf("1. GET\n");
        printf("2. POST\n");
        printf("3. PUT\n");
        printf("4. PATCH\n");
        printf("5. DELETE\n\n");
        printf("Your choice: ");
        method_choice = read_int();
    } while (method_choice < 1 || method_choice > 5);

    // Set the HTTP method
    switch (method_choice) {
        case 1:
            method = GET;
            break;
        case 2:
            method = POST;
            break;
        case 3:
            method = PUT;
            break;
        case 4:
            method = PATCH;
            break;
        case 5:
            method = DELETE;
            break;
    }

    // Ask for the endpoint
    printf("Please Enter the endpoint to which you want to send the request: ");
    endpoint = read_string(" \t\r");

    // If the request is not a GET or DELETE req, ask for body
    if (method_choice != 1 && method_choice != 5) {
        printf("Please Enter the request body. Make sure the body of the request is in JSON format: ");
        body = read_string("\r");
        crlf_count = 12;
    } else {
        body = NULL;
        crlf_count = 8;
    }

    // Calculate content length
    if (body != NULL) {
        content_length = strlen(body);
        // allocate memory to content length string
        content_length_string = malloc(sizeof(char) * ((int) ceil(log10(content_length)) + strlen(CONTENT_LENGTH) + 1));

        // Check if memory allocation failed
        if (content_length_string == NULL) {
            printf("Error! Memory allocation failed");
            exit(EXIT_FAILURE);
        } 

        // Convert content length to string
        sprintf(content_length_string, "%s%d", CONTENT_LENGTH, content_length);
    }

    // Calculate the size of the request
    if (body == NULL) {
        request_size = strlen(method) + strlen(endpoint) + strlen(HTTP_1_1) + strlen(HOST) + strlen(host)
            + strlen(ACCEPT) + crlf_count + 1;
    } else {
        request_size = strlen(method) + strlen(endpoint) + strlen(HTTP_1_1) + strlen(HOST) + strlen(host)
            + strlen(CONTENT_TYPE) + strlen(content_length_string) + strlen(body) + crlf_count + 1;
    }

    // Allocate memory for the request
    request = malloc(sizeof(char) * request_size);

    // Check if memory allocation failed
    if (request == NULL) {
        printf("Error! Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    // Build the request
    if (body == NULL) {
        snprintf(request, request_size, "%s%s%s\r\n%s%s\r\n%s\r\n\r\n", method, endpoint, HTTP_1_1, HOST, host, ACCEPT);
    } else {
        snprintf(request, request_size, "%s%s%s\r\n%s%s\r\n%s\r\n%s\r\n\r\n%s\r\n", method, endpoint, HTTP_1_1, HOST, host, CONTENT_TYPE, content_length_string, body);
    }

    // Free memory alloated to all strings except the request
    if (body != NULL) {
        free(body);
        free(content_length_string);
    }
    free(endpoint);

    // Return the request
    return request;
}

/**
 * Sends the given HTTP request to the server specified by the sockfd.
 *
 * @param sockfd The socket file descriptor of the connection to the server.
 * @param request The HTTP request to be sent.
 *
 * @return void
 *
 * @note If the request sending fails, the program will exit with an error message.
 */
void send_http_request(int sockfd, const char * request) {
    // Send the request using socket file descriptor. If the number of bytes sent is less than 0, something has gone wrong
    // Send returns the number of bytes sent
    if (send(sockfd, request, strlen(request), 0) < 0) {
        printf("Error! Request sending failed\n");
        exit(EXIT_FAILURE);
    }
}

/**
 * Recieves an HTTP response from the server specified by the sockfd.
 *
 * @param sockfd The socket file descriptor of the connection to the server.
 *
 * @return The response from the server. The returned string is null-terminated.
 *
 * @note If the response recieving fails, the program will exit with an error message.
 */
char * recieve_http_response(int sockfd) {
    // Initial buffer allocation
    int response_size = BUFFER_SIZE;
    // Holds the number of bytes read
    int bytes_read;
    // Variable to save the return value of poll
    int ret;
    // Position for writing data
    int total_bytes_read = 0;

    // Allocate memory for the response
    char *response = malloc(response_size);
    
    // Check if memory allocation failed
    if (response == NULL) {
        printf("Error! Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    
    struct pollfd fd;
    fd.fd = sockfd;
    fd.events = POLLIN;  // Wait for data to be available to read

    do {
        ret = poll(&fd, 1, TIMEOUT * 1000); // 1-second timeout

        if (ret == -1) {
            printf("Error! poll() failed: %s\n", strerror(errno));
            free(response);
            exit(EXIT_FAILURE);
        }
        if (ret == 0) {
            printf("No more response received within %d second\n", TIMEOUT);
            break;
        }

        // Data is available, read from socket
        bytes_read = recv(sockfd, response + total_bytes_read, response_size - total_bytes_read - 1, 0);

        // If the number of bytes read is less than 0, something has gone wrong
        if (bytes_read < 0) {
            printf("Error! recv() failed: %s\n", strerror(errno));
            free(response);
            exit(EXIT_FAILURE);
        }

        // Update the total bytes read
        total_bytes_read += bytes_read;
        response[total_bytes_read] = '\0';  // Null-terminate the response

        // If buffer is full, reallocate
        if (total_bytes_read >= response_size - 1) {
            response_size *= BUFFER_MULTIPLIER;
            response = realloc(response, response_size);
            if (response == NULL) {
                printf("Error! Memory allocation failed\n");
                exit(EXIT_FAILURE);
            }
        }
    } while (bytes_read > 0); // If the number of bytes read is 0, the connection has been closed

    return response;
}

/**
 * Asks the user if they want to send another request.
 *
 * This function asks the user if they want to send another request by inputting 'y' or 'n'.
 * It ensures that only valid inputs ('y' or 'n') are accepted, and re-prompts the user in
 * case of invalid input. It returns the user's choice.
 *
 * @return TRUE if the user wants to send another request, or FALSE if not.
 */
int ask_to_continue(void) {
    // Saves the user's input
    char input;

    // Ask the user if they want to send another request
    printf("\nDo you want to send another request? (y/n): ");
    // Loop until the user inputs 'y' or 'n'
    while ((input = getchar()) != 'y' && input != 'n') {
        printf("Invalid input. Please enter 'y' or 'n': ");
        // Clear the input buffer before asking the user again
        clear_buffer();
    }

    // Clear the input buffer
    clear_buffer();

    // Check if the user wants to send another request
    if (input == 'y') {
        return TRUE;
    } else {
        return FALSE;
    }
}