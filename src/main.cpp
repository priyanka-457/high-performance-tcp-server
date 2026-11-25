
#include "thread_pool.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <queue>
#include <stdexcept>
#include <string>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

volatile std::sig_atomic_t stop_requested = 0;

void handle_signal(int) {
    stop_requested = 1;
}

constexpr std::uint64_t LISTENER_TOKEN = 1;
constexpr std::uint64_t COMPLETION_TOKEN = 2;
constexpr std::size_t MAX_LINE = 64 * 1024;

struct Connection {
    int fd = -1;
    std::uint64_t id = 0;

    std::string input;
    std::string output;

    bool peer_eof = false;

    std::uint64_t next_request_seq = 0;
    std::uint64_t next_response_seq = 0;

    std::size_t pending_requests = 0;

    std::map<std::uint64_t, std::string> completed_responses;
};

struct Completion {
    std::uint64_t connection_id;
    std::uint64_t sequence;
    std::string response;
};

class Server {
public:
    Server(std::uint16_t port, std::size_t workers)
        : port_(port),
          worker_count_(workers == 0 ? 1 : workers),
          pool_(worker_count_) {}

    ~Server() {
        // Workers must finish before shared completion resources disappear.
        pool_.shutdown();
        cleanup_resources();
    }

    int run() {
        if (!setup()) {
            pool_.shutdown();
            cleanup_resources();
            return 1;
        }

        std::cout << "TCP server listening on 0.0.0.0:"
                  << port_ << " with "
                  << worker_count_ << " worker threads\n";

        std::vector<epoll_event> events(128);

        while (!stop_requested) {
            int count = ::epoll_wait(
                epoll_fd_,
                events.data(),
                static_cast<int>(events.size()),
                500
            );

            if (count < 0) {
                if (errno == EINTR) {
                    continue;
                }

                std::cerr << "epoll_wait: "
                          << std::strerror(errno) << '\n';
                break;
            }

            for (int i = 0; i < count; ++i) {
                const auto token =
                    events[static_cast<std::size_t>(i)].data.u64;

                const auto flags =
                    events[static_cast<std::size_t>(i)].events;

                if (token == LISTENER_TOKEN) {
                    accept_clients();
                    continue;
                }

                if (token == COMPLETION_TOKEN) {
                    drain_completions();
                    continue;
                }

                if (connections_.find(token) == connections_.end()) {
                    continue;
                }

                if (flags & (EPOLLERR | EPOLLHUP)) {
                    close_connection(token);
                    continue;
                }

                if (flags & (EPOLLIN | EPOLLRDHUP)) {
                    read_client(token);
                }

                auto it = connections_.find(token);

                if (it != connections_.end() &&
                    (flags & EPOLLOUT)) {
                    write_client(token);
                }
            }
        }

        std::cout
            << "Shutting down: stopping accepts and finishing worker tasks\n";

        // Keep completion_fd_ and completion_mutex_ alive while workers run.
        pool_.shutdown();

        // No worker can enqueue another completion after this point.
        {
            std::lock_guard<std::mutex> lock(completion_mutex_);
            while (!completions_.empty()) {
                completions_.pop();
            }
        }

        cleanup_resources();

        std::cout << "Shutdown complete\n";

        return 0;
    }

private:
    bool setup() {
        listen_fd_ = ::socket(
            AF_INET,
            SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
            0
        );

        if (listen_fd_ < 0) {
            perror("socket");
            return false;
        }

        int yes = 1;

        if (::setsockopt(
                listen_fd_,
                SOL_SOCKET,
                SO_REUSEADDR,
                &yes,
                sizeof(yes)) < 0) {
            perror("setsockopt");
            return false;
        }

        sockaddr_in address{};

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(port_);

        if (::bind(
                listen_fd_,
                reinterpret_cast<sockaddr*>(&address),
                sizeof(address)) < 0) {
            perror("bind");
            return false;
        }

        if (::listen(listen_fd_, SOMAXCONN) < 0) {
            perror("listen");
            return false;
        }

        epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);

        if (epoll_fd_ < 0) {
            perror("epoll_create1");
            return false;
        }

        completion_fd_ = ::eventfd(
            0,
            EFD_NONBLOCK | EFD_CLOEXEC
        );

        if (completion_fd_ < 0) {
            perror("eventfd");
            return false;
        }

        epoll_event ev{};

        ev.events = EPOLLIN;
        ev.data.u64 = LISTENER_TOKEN;

        if (::epoll_ctl(
                epoll_fd_,
                EPOLL_CTL_ADD,
                listen_fd_,
                &ev) < 0) {
            perror("epoll_ctl listener");
            return false;
        }

        ev = {};
        ev.events = EPOLLIN;
        ev.data.u64 = COMPLETION_TOKEN;

        if (::epoll_ctl(
                epoll_fd_,
                EPOLL_CTL_ADD,
                completion_fd_,
                &ev) < 0) {
            perror("epoll_ctl eventfd");
            return false;
        }

        return true;
    }

    void cleanup_resources() {
        for (auto& entry : connections_) {
            if (entry.second->fd >= 0) {
                ::close(entry.second->fd);
                entry.second->fd = -1;
            }
        }

        connections_.clear();

        if (listen_fd_ >= 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
        }

        if (epoll_fd_ >= 0) {
            ::close(epoll_fd_);
            epoll_fd_ = -1;
        }

        if (completion_fd_ >= 0) {
            ::close(completion_fd_);
            completion_fd_ = -1;
        }
    }

    void accept_clients() {
        for (;;) {
            sockaddr_in peer{};
            socklen_t length = sizeof(peer);

            int fd = ::accept4(
                listen_fd_,
                reinterpret_cast<sockaddr*>(&peer),
                &length,
                SOCK_NONBLOCK | SOCK_CLOEXEC
            );

            if (fd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return;
                }

                if (errno == EINTR) {
                    continue;
                }

                perror("accept4");
                return;
            }

            auto connection = std::make_shared<Connection>();

            connection->fd = fd;
            connection->id = next_id_++;

            epoll_event ev{};

            ev.events = EPOLLIN | EPOLLRDHUP;
            ev.data.u64 = connection->id;

            if (::epoll_ctl(
                    epoll_fd_,
                    EPOLL_CTL_ADD,
                    fd,
                    &ev) < 0) {
                perror("epoll_ctl client");
                ::close(fd);
                continue;
            }

            connections_.emplace(
                connection->id,
                std::move(connection)
            );
        }
    }

    void update_interest(std::uint64_t id) {
        auto it = connections_.find(id);

        if (it == connections_.end()) {
            return;
        }

        auto& conn = *it->second;

        epoll_event ev{};

        if (!conn.peer_eof) {
            ev.events |= EPOLLIN;
        }

        if (!conn.output.empty()) {
            ev.events |= EPOLLOUT;
        }

        ev.events |= EPOLLRDHUP;
        ev.data.u64 = id;

        if (::epoll_ctl(
                epoll_fd_,
                EPOLL_CTL_MOD,
                conn.fd,
                &ev) < 0) {
            close_connection(id);
        }
    }

    void submit_request(
        std::uint64_t connection_id,
        std::string request
    ) {
        auto it = connections_.find(connection_id);

        if (it == connections_.end()) {
            return;
        }

        const auto sequence = it->second->next_request_seq++;

        ++it->second->pending_requests;

        pool_.submit([
            this,
            connection_id,
            sequence,
            request = std::move(request)
        ] {
            std::string response;

            if (request == "PING") {
                response = "PONG\n";
            } else if (request.rfind("ECHO ", 0) == 0) {
                response = "OK " + request.substr(5) + "\n";
            } else if (request.empty()) {
                response = "ERR empty request\n";
            } else {
                response = "ERR unknown command\n";
            }

            {
                std::lock_guard<std::mutex> lock(completion_mutex_);

                completions_.push({
                    connection_id,
                    sequence,
                    std::move(response)
                });
            }

            std::uint64_t one = 1;

            // If eventfd is already readable, a failed EAGAIN write
            // does not lose the existing wake-up notification.
            (void)::write(completion_fd_, &one, sizeof(one));
        });
    }

    void read_client(std::uint64_t id) {
        char buffer[4096];

        for (;;) {
            auto it = connections_.find(id);

            if (it == connections_.end()) {
                return;
            }

            if (it->second->peer_eof) {
                return;
            }

            ssize_t n = ::recv(
                it->second->fd,
                buffer,
                sizeof(buffer),
                0
            );

            if (n > 0) {
                it->second->input.append(
                    buffer,
                    static_cast<std::size_t>(n)
                );

                for (;;) {
                    it = connections_.find(id);

                    if (it == connections_.end()) {
                        return;
                    }

                    auto& input = it->second->input;

                    const auto newline = input.find('\n');

                    if (newline == std::string::npos) {
                        if (input.size() > MAX_LINE) {
                            input.clear();
                            it->second->peer_eof = true;

                            queue_output(
                                id,
                                "ERR request too large\n"
                            );

                            update_interest(id);
                            maybe_close_connection(id);
                            return;
                        }

                        break;
                    }

                    if (newline > MAX_LINE) {
                        input.clear();
                        it->second->peer_eof = true;

                        queue_output(
                            id,
                            "ERR request too large\n"
                        );

                        update_interest(id);
                        maybe_close_connection(id);
                        return;
                    }

                    std::string request =
                        input.substr(0, newline);

                    input.erase(0, newline + 1);

                    submit_request(id, std::move(request));
                }

                continue;
            }

            if (n == 0) {
                auto it = connections_.find(id);

                if (it != connections_.end()) {
                    it->second->peer_eof = true;

                    // An incomplete final line is not a valid request.
                    it->second->input.clear();

                    update_interest(id);
                    maybe_close_connection(id);
                }

                return;
            }

            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }

            close_connection(id);
            return;
        }
    }

    void queue_output(
        std::uint64_t id,
        std::string response
    ) {
        auto it = connections_.find(id);

        if (it == connections_.end()) {
            return;
        }

        it->second->output += std::move(response);

        update_interest(id);
    }

    void drain_completions() {
        std::uint64_t value;

        while (::read(
            completion_fd_,
            &value,
            sizeof(value)) > 0) {
        }

        std::queue<Completion> local;

        {
            std::lock_guard<std::mutex> lock(completion_mutex_);
            std::swap(local, completions_);
        }

        while (!local.empty()) {
            auto item = std::move(local.front());
            local.pop();

            auto it = connections_.find(item.connection_id);

            if (it == connections_.end()) {
                continue;
            }

            auto& conn = *it->second;

            if (conn.pending_requests > 0) {
                --conn.pending_requests;
            }

            conn.completed_responses.emplace(
                item.sequence,
                std::move(item.response)
            );

            while (true) {
                auto response_it =
                    conn.completed_responses.find(
                        conn.next_response_seq
                    );

                if (response_it == conn.completed_responses.end()) {
                    break;
                }

                conn.output += std::move(response_it->second);
                conn.completed_responses.erase(response_it);

                ++conn.next_response_seq;
            }

            update_interest(item.connection_id);
            maybe_close_connection(item.connection_id);
        }
    }

    void write_client(std::uint64_t id) {
        auto it = connections_.find(id);

        if (it == connections_.end()) {
            return;
        }

        auto& conn = *it->second;

        while (!conn.output.empty()) {
            ssize_t n = ::send(
                conn.fd,
                conn.output.data(),
                conn.output.size(),
                MSG_NOSIGNAL
            );

            if (n > 0) {
                conn.output.erase(
                    0,
                    static_cast<std::size_t>(n)
                );
            } else if (n < 0 && errno == EINTR) {
                continue;
            } else if (
                n < 0 &&
                (errno == EAGAIN || errno == EWOULDBLOCK)
            ) {
                return;
            } else {
                close_connection(id);
                return;
            }
        }

        maybe_close_connection(id);

        if (connections_.find(id) != connections_.end()) {
            update_interest(id);
        }
    }

    void maybe_close_connection(std::uint64_t id) {
        auto it = connections_.find(id);

        if (it == connections_.end()) {
            return;
        }

        const auto& conn = *it->second;

        if (conn.peer_eof &&
            conn.pending_requests == 0 &&
            conn.output.empty() &&
            conn.completed_responses.empty()) {
            close_connection(id);
        }
    }

    void close_connection(std::uint64_t id) {
        auto it = connections_.find(id);

        if (it == connections_.end()) {
            return;
        }

        ::epoll_ctl(
            epoll_fd_,
            EPOLL_CTL_DEL,
            it->second->fd,
            nullptr
        );

        ::close(it->second->fd);

        connections_.erase(it);
    }

    std::uint16_t port_;
    std::size_t worker_count_;

    // Keep the pool declared before completion state.
    // Explicit shutdown in run() and the destructor joins workers
    // while the completion state and eventfd are still alive.
    ThreadPool pool_;

    int listen_fd_ = -1;
    int epoll_fd_ = -1;
    int completion_fd_ = -1;

    std::uint64_t next_id_ = 3;

    std::unordered_map<
        std::uint64_t,
        std::shared_ptr<Connection>
    > connections_;

    std::mutex completion_mutex_;
    std::queue<Completion> completions_;
};

} // namespace

int main(int argc, char* argv[]) {
    std::uint16_t port = 9000;
    std::size_t workers = 4;

    try {
        if (argc > 1) {
            int parsed = std::stoi(argv[1]);

            if (parsed < 1 || parsed > 65535) {
                throw std::out_of_range("port");
            }

            port = static_cast<std::uint16_t>(parsed);
        }

        if (argc > 2) {
            int parsed = std::stoi(argv[2]);

            if (parsed < 1 || parsed > 256) {
                throw std::out_of_range("workers");
            }

            workers = static_cast<std::size_t>(parsed);
        }
    } catch (...) {
        std::cerr
            << "Usage: " << argv[0]
            << " [port 1-65535] [workers 1-256]\n";

        return 2;
    }

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    Server server(port, workers);

    return server.run();
}
