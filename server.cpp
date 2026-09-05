/*
This file sets up a basic multithreaded HTTP/1.1 server in C++ that connects to HTTPclient.py. 
*/

// server.cpp
//
// A multithreaded C++17 HTTP/1.1 server for macOS, translated from
// server.py's LabHttpTCPHandler. Each accepted client connection is
// handled on its own std::thread (thread-per-connection model), so
// multiple clients can be served concurrently.
//
// Build:  clang++ -std=c++17 -O2 -pthread server.cpp -o server
// Run:    ./server            (serves files from ./www)

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

// ---------------- Configuration (mirrors server.py) ----------------
static const char*       HOST        = "0.0.0.0"; // Host is 0.0.0.0
static const int         PORT        = 8000; // Port 8000
static const int         BACKLOG     = 64;
static const std::string LINE_ENDING = "\r\n";
static const std::string HTTP_1_1    = "HTTP/1.1"; // Format using HTTP/1.1

// Resolved, absolute path to the directory we serve files from ("www")
static fs::path SERVE_PATH;

// ---------------- percent_decode ----------------
// Decode %XX escape sequences in a URL path (mirrors percent_decode in server.py).
std::string percent_decode(const std::string& s) {
    std::string decoded;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '%' && i + 2 < s.size() &&
            std::isxdigit(static_cast<unsigned char>(s[i + 1])) &&
            std::isxdigit(static_cast<unsigned char>(s[i + 2]))) {
            std::string hex = s.substr(i + 1, 2);
            char c = static_cast<char>(std::stoi(hex, nullptr, 16));
            decoded += c;
            i += 3;
        } else {
            decoded += s[i];
            i += 1;
        }
    }
    return decoded;
}

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t");
    size_t end = s.find_last_not_of(" \t");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}

static std::vector<std::string> split_ws(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

/*
Define the LabHTTPConection class
    Purpose: Handles exactly one client connection. 
    An instance of this class runs entirely on its own thread, 
    so there is no shared mutable state between connections 
*/
class LabHttpConnection {
public:
    explicit LabHttpConnection(int client_fd) : fd_(client_fd) {}

    ~LabHttpConnection() {
        if (fd_ >= 0) close(fd_);
    }

    // Handle function attempts to read the content the client sent back 
    void handle() {
        try {
            std::string req_line;
            if (!read_line(req_line) || req_line.empty()) {
                return; 
            }

            // Attempt to parse request line
            std::vector<std::string> parsed = split_ws(req_line);
            if (parsed.size() < 2) {
                send_error(400);
                return;
            }

            // Seperate reuqest line into method and path
            std::string method = parsed[0];
            std::string path   = parsed[1];

            // Read headers until a blank line.
            std::map<std::string, std::string> headers;
            std::string line;
            while (read_line(line) && !line.empty()) {
                auto pos = line.find(':');
                if (pos != std::string::npos) {
                    headers[trim(line.substr(0, pos))] = trim(line.substr(pos + 1));
                }
            }

            if (method == "GET") {
                serve_file(path);
            } else {
                send_error(405);
            }
        } catch (...) {
            std::cerr << "An error occurred while handling a connection\n";
            send_error(500);
        }
    }

private:
    int fd_;

    // ---- low-level socket line I/O ----

    // Reads a single line terminated by '\n' (tolerates a preceding '\r'),
    // strips the line ending, and returns false only if nothing was read
    // before EOF/error.
    bool read_line(std::string& out) {
        out.clear();
        char c;
        ssize_t n;
        bool got_any = false;
        while ((n = recv(fd_, &c, 1, 0)) > 0) {
            got_any = true;
            if (c == '\n') break;
            out += c;
        }
        if (!out.empty() && out.back() == '\r') out.pop_back();
        return got_any;
    }

    void send_raw(const std::string& data) {
        size_t sent = 0;
        while (sent < data.size()) {
            ssize_t n = send(fd_, data.data() + sent, data.size() - sent, 0);
            if (n <= 0) break;
            sent += static_cast<size_t>(n);
        }
    }

    // ---- request handling ----

    // Serve a file under SERVE_PATH corresponding to the request path.
    void serve_file(const std::string& raw_path) {
        std::string decoded_path = percent_decode(raw_path);

        fs::path file_path;
        if (decoded_path == "/") {
            file_path = SERVE_PATH / "index.html";
        } else {
            std::string rel = decoded_path;
            while (!rel.empty() && rel.front() == '/') rel.erase(rel.begin());
            file_path = SERVE_PATH / rel;

            std::error_code ec;
            if (fs::is_directory(file_path, ec)) {
                // Directory requested without a trailing slash -> redirect.
                if (raw_path.empty() || raw_path.back() != '/') {
                    send_error(301, raw_path);
                    return;
                }
                file_path = file_path / "index.html";
            }
        }

        // 403 check: the resolved path must stay within SERVE_PATH.
        std::error_code ec;
        fs::path resolved = fs::weakly_canonical(file_path, ec);
        fs::path served_resolved = fs::weakly_canonical(SERVE_PATH, ec);
        if (ec) {
            send_error(403);
            return;
        }
        fs::path rel_check = resolved.lexically_relative(served_resolved);
        std::string rel_str = rel_check.string();
        if (rel_check.empty() || rel_str == ".." ||
            rel_str.rfind("..", 0) == 0 /* starts with ".." */) {
            send_error(403);
            return;
        }

        if (fs::exists(resolved, ec) && fs::is_regular_file(resolved, ec)) {
            std::ifstream f(resolved, std::ios::binary);
            std::ostringstream ss;
            ss << f.rdbuf();
            std::string body = ss.str();

            // Normalize CRLF -> LF, mirroring server.py's body.replace(b"\r\n", b"\n")
            std::string normalized;
            normalized.reserve(body.size());
            for (size_t i = 0; i < body.size(); ++i) {
                if (body[i] == '\r' && i + 1 < body.size() && body[i + 1] == '\n') {
                    continue; // drop the \r, the \n gets appended on the next loop iteration
                }
                normalized += body[i];
            }
            body.swap(normalized);

            std::string content_type = "text/html";
            if (resolved.extension() == ".css") {
                content_type = "text/css; charset=utf-8";
            }

            std::ostringstream resp;
            resp << HTTP_1_1 << " 200 OK" << LINE_ENDING;
            resp << "Content-Length: " << body.size() << LINE_ENDING;
            resp << "Content-Type: " << content_type << "; charset=utf-8" << LINE_ENDING;
            resp << "Connection: Close" << LINE_ENDING;
            resp << LINE_ENDING;
            send_raw(resp.str());
            send_raw(body);
        } else {
            send_error(404);
        }
    }

    // Send an HTTP error response.
    // NOTE: server.py's error() concatenates str(err_code) into `reason` and
    // then writes "{err_code} {reason}" on the status line, producing a
    // duplicated code (e.g. "400 400 Bad request"). That's fixed here.
    void send_error(int err_code, const std::string& path = "/") {
        if (err_code == 301) {
            std::string new_location = path + "/";
            std::ostringstream resp;
            resp << HTTP_1_1 << " 301 Moved Permanently" << LINE_ENDING;
            resp << "Location: " << new_location << LINE_ENDING;
            resp << "Content-Length: 0" << LINE_ENDING;
            resp << "Connection: Close" << LINE_ENDING << LINE_ENDING;
            send_raw(resp.str());
            return;
        }

        std::string reason;
        switch (err_code) {
            case 400: reason = "Bad Request"; break;
            case 403: reason = "Forbidden"; break;
            case 404: reason = "Not Found"; break;
            case 500: reason = "Internal Server Error"; break;
            case 405: reason = "Method Not Allowed"; break;
            default:  reason = "Error"; break;
        }

        std::string body = "<html><body><h1>" + std::to_string(err_code) + " " +
                            reason + "</h1></body></html>";

        std::ostringstream resp;
        resp << HTTP_1_1 << " " << err_code << " " << reason << LINE_ENDING;
        resp << "Content-Length: " << body.size() << LINE_ENDING;
        resp << "Content-Type: text/html; charset=utf-8" << LINE_ENDING;
        resp << "Connection: Close" << LINE_ENDING;
        resp << LINE_ENDING;
        send_raw(resp.str());
        send_raw(body);
    }
};

// ---------------- main / accept loop ----------------

int main() {
    SERVE_PATH = fs::absolute("www").lexically_normal();

    // Use the socket POSIX command to create a socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;

    // IUse the setsocketopt to set an option on the socket
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, HOST, &addr.sin_addr);

    // Attach socket to a specific local address and port
    if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    // Make the socket a passive listening socket
    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    std::cout << "Starting server on " << HOST << ":" << PORT
              << ", serving " << SERVE_PATH << "\n";

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        // One detached thread per connection -> multithreaded server.
        std::thread([client_fd]() {
            LabHttpConnection conn(client_fd);
            conn.handle();
        }).detach();
    }

    close(server_fd);
    return 0;
}