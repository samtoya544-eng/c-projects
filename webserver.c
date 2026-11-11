#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netdb.h>

#define PORT "8080"
#define BACKLOG 5
#define MAX_FILE (10 * 1024 * 1024)   // refuse files bigger than 10 MB

// Keeps calling send() until every byte has gone out (section 7.4)
int sendall(int s, char *buf, int *len) {
    int total = 0;
    int bytesleft = *len;
    int n = 0;

    while (total < *len) {
        n = send(s, buf + total, bytesleft, 0);
        if (n == -1) break;
        total += n;
        bytesleft -= n;
    }

    *len = total;
    return n == -1 ? -1 : 0;
}

// Picks the Content-Type from the file extension, so the browser
// knows whether it is getting a page, a stylesheet, or an image.
const char *content_type(const char *name) {
    const char *dot = strrchr(name, '.');
    if (dot == NULL) return "application/octet-stream";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0) return "text/html";
    if (strcmp(dot, ".css") == 0)  return "text/css";
    if (strcmp(dot, ".js") == 0)   return "text/javascript";
    if (strcmp(dot, ".txt") == 0)  return "text/plain";
    if (strcmp(dot, ".png") == 0)  return "image/png";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)  return "image/gif";
    if (strcmp(dot, ".svg") == 0)  return "image/svg+xml";
    if (strcmp(dot, ".ico") == 0)  return "image/x-icon";
    return "application/octet-stream";
}

// Sends a short error page, e.g. send_message(fd, "404 Not Found", "...")
void send_message(int fd, const char *status, const char *msg) {
    char response[1024];
    int len = snprintf(response, sizeof response,
                       "HTTP/1.1 %s\r\n"
                       "Content-Type: text/html\r\n"
                       "Content-Length: %zu\r\n"
                       "Connection: close\r\n"
                       "\r\n"
                       "%s",
                       status, strlen(msg), msg);
    if (sendall(fd, response, &len) == -1) {
        perror("sendall");
    }
}

// Adds one line to server.log, e.g.:
// 2026-11-11 23:45:10  GET  /about.html  200
void log_request(const char *method, const char *path, int status) {
    FILE *log = fopen("server.log", "a");   // "a" = append to the end
    if (log == NULL) return;

    char stamp[32];
    time_t now = time(NULL);
    strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", localtime(&now));

    fprintf(log, "%s  %s  %s  %d\n", stamp, method, path, status);
    fclose(log);
}

// ---- Form helpers ----

// Value of one hex digit: '0'-'9', 'a'-'f', 'A'-'F'
int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return c - 'A' + 10;
}

// Undoes URL encoding: "Mary+Anne%21" -> "Mary Anne!"
// Browsers turn spaces into '+' and special characters into %XX.
void url_decode(char *dst, const char *src, size_t size) {
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j + 1 < size; i++) {
        if (src[i] == '+') {
            dst[j++] = ' ';
        } else if (src[i] == '%' && isxdigit((unsigned char)src[i + 1])
                                 && isxdigit((unsigned char)src[i + 2])) {
            int value = hexval(src[i + 1]) * 16 + hexval(src[i + 2]);
            i += 2;
            if (value != 0) dst[j++] = (char)value;   // never allow a hidden end-of-string
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

// Finds key in a query like "name=Toya&age=20" and copies its decoded value
// into out. Returns 1 if the key was found, 0 if not.
int get_param(const char *query, const char *key, char *out, size_t size) {
    if (query == NULL) return 0;
    size_t klen = strlen(key);
    const char *p = query;

    while (p != NULL && *p != '\0') {
        if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
            const char *value = p + klen + 1;
            size_t vlen = strcspn(value, "&");      // value ends at the next '&'
            char raw[256];
            if (vlen >= sizeof raw) vlen = sizeof raw - 1;
            memcpy(raw, value, vlen);
            raw[vlen] = '\0';
            url_decode(out, raw, size);
            return 1;
        }
        p = strchr(p, '&');                          // jump to the next pair
        if (p != NULL) p++;
    }
    return 0;
}

// Makes text safe to put inside a web page. Without this, someone could type
// <script>...</script> into the form and run code in the next visitor's browser.
void html_escape(char *dst, const char *src, size_t size) {
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0'; i++) {
        const char *rep = NULL;
        switch (src[i]) {
            case '&':  rep = "&amp;";  break;
            case '<':  rep = "&lt;";   break;
            case '>':  rep = "&gt;";   break;
            case '"':  rep = "&quot;"; break;
            case '\'': rep = "&#39;";  break;
        }
        if (rep != NULL) {
            size_t rl = strlen(rep);
            if (j + rl + 1 >= size) break;
            memcpy(dst + j, rep, rl);
            j += rl;
        } else {
            if (j + 2 >= size) break;
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

// Serves ONE browser. The caller closes fd afterwards.
// Every early exit is a plain "return" now (it used to be "continue").
void handle_client(int fd) {
    char buf[4096];
    ssize_t numbytes;

    // ---- See what the browser sends ----
    numbytes = recv(fd, buf, sizeof(buf) - 1, 0);
    if (numbytes <= 0) {
        return;
    }
    buf[numbytes] = '\0';

    // ---- Read the method and path from the first line ----
    char method[8];
    char path[256];
    if (sscanf(buf, "%7s %255s", method, path) != 2) {
        return;
    }
    printf("browser wants: %s %s\n", method, path);

    // Split off a query string: "/about.html?x=1" -> path "/about.html", query "x=1"
    char *query = strchr(path, '?');
    if (query != NULL) {
        *query = '\0';          // cut the path here
        query++;                // query now points at "x=1"
        printf("query string: %s\n", query);
    }

    // Only GET and HEAD are supported
    int is_head = (strcmp(method, "HEAD") == 0);
    if (strcmp(method, "GET") != 0 && !is_head) {
        printf("405: method %s not allowed\n", method);
        log_request(method, path, 405);
        send_message(fd, "405 Method Not Allowed",
                     "<html><body><h1>405 Method Not Allowed</h1></body></html>");
        return;
    }

    // Safety: never allow ".." in a path (checked BEFORE we touch the disk)
    if (strstr(path, "..") != NULL) {
        printf("blocked: %s\n", path);
        log_request(method, path, 403);
        send_message(fd, "403 Forbidden",
                     "<html><body><h1>403 Forbidden</h1></body></html>");
        return;
    }

    // ---- A page made by the server: /hello?name=Toya ----
    if (strcmp(path, "/hello") == 0) {
        char name[128] = "";
        char safe[800];
        if (!get_param(query, "name", name, sizeof name) || name[0] == '\0') {
            snprintf(name, sizeof name, "stranger");
        }
        html_escape(safe, name, sizeof safe);

        char page[1500];
        int plen = snprintf(page, sizeof page,
                            "<html><head><link rel=\"stylesheet\" href=\"/style.css\"></head>"
                            "<body><h1>Hello, %s!</h1>"
                            "<p>This page was built by my C server just now.</p>"
                            "<a href=\"/form.html\">Back to the form</a>"
                            "</body></html>", safe);

        char header[256];
        int hlen = snprintf(header, sizeof header,
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: %d\r\n"
                            "Connection: close\r\n"
                            "\r\n", plen);
        int blen = plen;
        if (sendall(fd, header, &hlen) == -1 ||
            (!is_head && sendall(fd, page, &blen) == -1)) {
            perror("sendall");
        }
        printf("built /hello page for '%s'\n", safe);
        log_request(method, path, 200);
        return;
    }

    // ---- Choose the file name ----
    char file[300];
    if (strcmp(path, "/") == 0) {
        snprintf(file, sizeof file, "index.html");
    } else {
        snprintf(file, sizeof file, "%s", path + 1);   // skip the leading '/'
    }

    // A path ending in '/' means a folder: serve index.html inside it
    size_t flen = strlen(file);
    if (flen > 0 && file[flen - 1] == '/') {
        snprintf(file + flen, sizeof file - flen, "index.html");
    } else {
        // "/docs" where docs is a folder: send the browser to "/docs/"
        struct stat st;
        if (stat(file, &st) == 0 && S_ISDIR(st.st_mode)) {
            char redirect[512];
            int rlen = snprintf(redirect, sizeof redirect,
                                "HTTP/1.1 301 Moved Permanently\r\n"
                                "Location: %s/\r\n"
                                "Content-Length: 0\r\n"
                                "Connection: close\r\n"
                                "\r\n", path);
            printf("301: %s -> %s/\n", path, path);
            log_request(method, path, 301);
            if (sendall(fd, redirect, &rlen) == -1) perror("sendall");
            return;
        }
    }
    const char *filename = file;

    // ---- Open the file ----
    FILE *f = fopen(filename, "rb");     // "rb" = read raw bytes (needed for images)
    if (f == NULL) {
        printf("404: could not open '%s'\n", filename);
        log_request(method, path, 404);
        send_message(fd, "404 Not Found",
                     "<html><body><h1>404 Not Found</h1></body></html>");
        return;
    }

    // How big is the file?
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    if (size < 0 || size > MAX_FILE) {
        fclose(f);
        printf("500: '%s' is not a normal file or is too big\n", filename);
        log_request(method, path, 500);
        send_message(fd, "500 Internal Server Error",
                     "<html><body><h1>500 Server Error</h1></body></html>");
        return;
    }

    // Read the whole file into memory
    char *body = malloc(size > 0 ? size : 1);
    if (body == NULL) {
        fclose(f);
        log_request(method, path, 500);
        send_message(fd, "500 Internal Server Error",
                     "<html><body><h1>500 Server Error</h1></body></html>");
        return;
    }
    size_t n = fread(body, 1, size, f);
    fclose(f);

    // ---- Send the header, then the file ----
    char header[512];
    int hlen = snprintf(header, sizeof header,
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: %s\r\n"
                        "Content-Length: %zu\r\n"
                        "Connection: close\r\n"
                        "\r\n",
                        content_type(filename), n);

    int blen = (int)n;
    if (sendall(fd, header, &hlen) == -1) {
        perror("sendall");
    } else if (!is_head && sendall(fd, body, &blen) == -1) {
        perror("sendall");   // HEAD gets the header only, no body
    }
    printf("sent %s (%zu bytes, %s)\n", filename, n, content_type(filename));
    log_request(method, path, 200);

    free(body);
}

int main(void) {
    struct addrinfo hints, *res;
    int sockfd, new_fd, status;
    struct sockaddr_storage their_addr;
    socklen_t addr_size;

    // ---- Same server setup you already know ----
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    status = getaddrinfo(NULL, PORT, &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) {
        perror("server:socket");
        freeaddrinfo(res);
        return 1;
    }

    int yes = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("server:bind");
        close(sockfd);
        freeaddrinfo(res);
        return 1;
    }

    freeaddrinfo(res);

    if (listen(sockfd, BACKLOG) == -1) {
        perror("server:listen");
        close(sockfd);
        return 1;
    }

    // Finished child processes are cleaned up automatically (no zombies)
    signal(SIGCHLD, SIG_IGN);

    printf("Web server running. Open http://localhost:%s in your browser.\n", PORT);
    char cwd[512];
    if (getcwd(cwd, sizeof cwd) != NULL) printf("Serving files from: %s\n", cwd);

    // ---- Keep serving ----
    while (1) {
        addr_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if (new_fd == -1) {
            perror("server:accept");
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) {              // child: serves this one browser
            close(sockfd);           // child doesn't need the listening socket
            handle_client(new_fd);
            close(new_fd);
            exit(0);
        }
        if (pid == -1) {
            perror("server:fork");
        }
        close(new_fd);               // parent doesn't need this client's socket
    }

    close(sockfd);
    return 0;
}
