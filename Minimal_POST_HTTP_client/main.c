/**
 * A simple, minimal, HTTP1.1 client, written in C.
 * Supports most Unix-like systems (e.g., OSX, Linux) and Windows.
 * @author: Michal Spano
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h" /* custom config */

// Detect most common Unix-like system
#if (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
  #include <sys/socket.h>
  #include <netdb.h>   /* socket, inet */
  #include <unistd.h>  /* close()      */
#elif defined(_WIN32) || defined(WIN32) // Windows
  #include <winsock2.h>
  #pragma comment(lib,"ws2_32.lib") // needed for linking
  #define WINDOWS_PLATFORM          // my custom macro (preserves logic directives)
#else
  #error "Unknown platform: This cody only suport Unix-like or Windows systems."
#endif
// Further reading:
// https://stackoverflow.com/a/26225829
// https://handsonnetworkprogramming.com/articles/differences-windows-winsock-linux-unix-bsd-sockets-compatibility

#define BUFF_MAX 10240 // = 10KiB (~10kB)

/**
 * A helper function that formats a POST HTTP request (version 1.1).
 *
 * @param host:         the desired host address
 * @param path:         the desired path at the host address
 * @param data:         the contents of the request body
 * @param content_type: the content type (e.g. json, txt, html)
 *
 * @returns: formatted string of the request
 */
char* create_post_req(const char* host,
                      const char* path,
                      const char* data,
                      const char* content_type) {

    char* request = (char*)malloc(BUFF_MAX);
    snprintf(request, BUFF_MAX,
        "POST %s HTTP/1.1\r\n"    // add the path on the host
        "Host: %s\r\n"            // add the host string
        "Content-Type: %s\r\n"    // type of content
        "Content-Length: %ld\r\n" // length of the buffer
        "Connection: close\r\n"
        "\r\n"                    // separates the body
        "%s",                     // the body of the request
        path, host, content_type, strlen(data), data);
    return request;
}

int main(void) {
  /** Example 'raw' JSON request body (for the sake of demonstration, to an
   * internal server):
   * ```
   * {
   *  "model": "llama3.2",
   *  "prompt: "Write a program to compute Fibonacci numbers in Python.",
   *  "stream": false
   * }
   * ```
   **/
  const char* req_body = "{"
    "\"model\": \"llama3.2\","
    "\"prompt\": \"Write a program to compute Fibonacci numbers in Python.\","
    "\"stream\": false"
  "}";
  
  const char* content_type = "application/json"; // want to send the above as JSON

  // Required for Windows (Winsock needs to be initialized)
#ifdef WINDOWS_PLATFORM
    WSADATA d;
    if (WSAStartup(MAKEWORD(2, 2), &d)) {
        fprintf(stderr, "Winsock intialization failed: %d", WSAGetLastError());
        return 1;
    }
#endif

  // Initialize socket with AF_INET which specifies a type (i.e. family)
  // of the address (i.e. IPv4.)
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("Failed to create socket");
    return 1;
  }

  // Initialize server, get by host, check for erors.
  struct hostent *server = gethostbyname(HOST);
  if (server == NULL) {
    fprintf(stderr, "Failed to resolve host: %s\n", HOST);
    return 2;
  }
  
  // Open an (internet) socket address
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr)); // populate with zeros
  server_addr.sin_family = AF_INET;             // retain IP family (type)
  server_addr.sin_port = htons(PORT);           // same with port

  // memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
  // Potentially redundant call (preserved for documentation)

  // Try to open a socket connection, handle the case if the connection is refused.
  if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    perror("Connection failed");
#ifdef WINDOWS_PLATFORM
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif
    return -1;
  }
  
  // Format the request and send it via the socket. Handle the case when
  // sending is refused.
  char* request = create_post_req(HOST, PATH, req_body, content_type);
  if (send(sock, request, strlen(request), 0) < 0) {
    perror("Failed to send request");
    free(request);
#ifdef WINDOWS_PLATFORM
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif
    return -2;
  }
  
  free(request); // The POST request buffer can now be safely freed.
  
  char res_buffer[BUFF_MAX]; // Response buffer for reading from socket
  char* res_body = NULL;     // Cumulative buffer to store response body
  int total_size = 0;        // incremented per iteration
  int headers_found = 0;     // the stream is continuous, headers passed once!
  long bytes_received;       // a long should do for the current BUFF_MAX

  // Continue receiving bytes from the socket (response)
  while ((bytes_received = recv(sock, res_buffer, BUFF_MAX - 1, 0)) > 0) {
    res_buffer[bytes_received] = '\0'; // Terminate stream

    // Start of response's body (after headers), remove escapes (i.e.
    // separator).
    if (!headers_found) {
      char* body = strstr(res_buffer, "\r\n\r\n");
      if (body != NULL) {  // ignore if pattern not found
        headers_found = 1; // update flag
        body += 4;         // skip "\r\n\r\n" (first 4 chars)

        int body_len = strlen(body);     // Size of current byte junk
        res_body = malloc(body_len + 1); // Allocate memory; +1 for \0
        if (res_body == NULL) {          // Ensure that there's enough space
          perror("Initial memory allocation failed");
          close(sock);
          return 3;
        }
      
        memcpy(res_body, body, body_len); // copy context to buffer
        total_size = body_len;            // Initial size = length of the first junk
        res_body[total_size] = '\0';      // Terminate current substring in buffer
      }

    } else {
      // Further junk(s) of bytes, with no headers now
      size_t new_size = total_size + bytes_received; // # of newly received bytes
      char *temp = realloc(res_body, new_size + 1);  // Attempt to realloc extended buffer
      if (temp == NULL) {                            // Not enough space, free, close socket
          free(res_body);
          close(sock);
          perror("Memory reallocation failed");
          return 3;
      }

      res_body = temp; // ok
      // Copy new bytes to the buffer, shift the pointer
      memcpy(res_body + total_size, res_buffer, bytes_received);
      total_size = new_size;       // Udpate size
      res_body[total_size] = '\0'; // Terminate
    }
  }
  
  // Only print the response if sufficient data was collected
  if (res_body != NULL) {
    printf("Response body:\n%s\n", res_body);
    free(res_body);
  }

  // Close socket connection, return 0 (success)
#ifdef WINDOWS_PLATFORM
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif
  return 0;
}
