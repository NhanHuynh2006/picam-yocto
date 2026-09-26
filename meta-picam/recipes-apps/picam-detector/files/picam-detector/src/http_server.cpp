#include "http_server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "index_html.hpp"
#include "util.hpp"

namespace {

constexpr int kMaxStreamClients = 4;

bool sendAll(int fd, const void* data, size_t len) {
    const char* p = static_cast<const char*>(data);
    while (len > 0) {
        ssize_t n = ::send(fd, p, len, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        p += n;
        len -= static_cast<size_t>(n);
    }
    return true;
}

bool sendStr(int fd, const std::string& s) { return sendAll(fd, s.data(), s.size()); }

void setTimeouts(int fd, int seconds) {
    timeval tv{};
    tv.tv_sec = seconds;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

// Returns the request path of a GET request, "" if the request is invalid.
std::string readRequestPath(int fd) {
    std::string req;
    char buf[1024];
    while (req.find("\r\n\r\n") == std::string::npos && req.size() < 8192) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        req.append(buf, static_cast<size_t>(n));
    }
    if (req.compare(0, 4, "GET ") != 0) return "";
    const size_t end = req.find(' ', 4);
    if (end == std::string::npos) return "";
    std::string path = req.substr(4, end - 4);
    const size_t q = path.find('?');
    if (q != std::string::npos) path.resize(q);
    return path;
}

void sendResponse(int fd, const char* status, const char* type, const void* body, size_t len) {
    char hdr[256];
    std::snprintf(hdr, sizeof(hdr),
                  "HTTP/1.0 %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
                  "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
                  status, type, len);
    if (sendStr(fd, hdr)) sendAll(fd, body, len);
}

void sendText(int fd, const char* status, const std::string& text) {
    sendResponse(fd, status, "text/plain; charset=utf-8", text.data(), text.size());
}

void streamMjpeg(int fd, const Config& cfg, SharedState& st) {
    if (st.stream_clients.load() >= kMaxStreamClients) {
        sendText(fd, "503 Service Unavailable", "too many viewers\n");
        return;
    }
    st.stream_clients++;
    const std::string hdr =
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
        "Cache-Control: no-store\r\nConnection: close\r\n\r\n";
    if (sendStr(fd, hdr)) {
        const auto period = std::chrono::milliseconds(1000 / cfg.stream_fps);
        std::vector<uchar> jpg;
        while (st.running.load()) {
            const auto t0 = std::chrono::steady_clock::now();
            if (!st.annotatedJpeg(jpg, cfg.jpeg_quality)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }
            char part[128];
            std::snprintf(part, sizeof(part),
                          "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %zu\r\n\r\n",
                          jpg.size());
            if (!sendStr(fd, part) || !sendAll(fd, jpg.data(), jpg.size()) || !sendStr(fd, "\r\n"))
                break;  // viewer closed the page
            std::this_thread::sleep_until(t0 + period);
        }
    }
    st.stream_clients--;
}

void handleClient(int fd, const Config& cfg, SharedState& st) {
    setTimeouts(fd, 5);
    const std::string path = readRequestPath(fd);

    if (path.empty()) {
        sendText(fd, "400 Bad Request", "bad request\n");
    } else if (path == "/" || path == "/index.html") {
        sendResponse(fd, "200 OK", "text/html; charset=utf-8", kIndexHtml, sizeof(kIndexHtml) - 1);
    } else if (path == "/stats") {
        const std::string json = st.statsJson();
        sendResponse(fd, "200 OK", "application/json", json.data(), json.size());
    } else if (path == "/snapshot.jpg") {
        std::vector<uchar> jpg;
        if (st.annotatedJpeg(jpg, cfg.jpeg_quality))
            sendResponse(fd, "200 OK", "image/jpeg", jpg.data(), jpg.size());
        else
            sendText(fd, "503 Service Unavailable", "no camera frame yet\n");
    } else if (path == "/stream") {
        streamMjpeg(fd, cfg, st);
    } else {
        sendText(fd, "404 Not Found", "not found\n");
    }
    ::close(fd);
}

}  // namespace

void httpServerLoop(const Config& cfg, SharedState& st) {
    const int srv = ::socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        PLOG("ERROR: socket(): %s — web page disabled", std::strerror(errno));
        return;
    }
    const int one = 1;
    ::setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(cfg.http_port));
    if (::bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 || ::listen(srv, 8) < 0) {
        PLOG("ERROR: cannot listen on port %d: %s — web page disabled", cfg.http_port,
             std::strerror(errno));
        ::close(srv);
        return;
    }
    PLOG("HTTP server listening on port %d", cfg.http_port);

    while (st.running.load()) {
        pollfd p{srv, POLLIN, 0};
        if (::poll(&p, 1, 500) <= 0) continue;
        const int fd = ::accept(srv, nullptr, nullptr);
        if (fd < 0) continue;
        // Detached: objects it uses (cfg, st) live for the whole process lifetime.
        std::thread(handleClient, fd, std::cref(cfg), std::ref(st)).detach();
    }
    ::close(srv);
}
