#include <string>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <cerrno>
#include <limits>

#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>

struct Options
{
    std::string socket_path;
    std::string command;
    std::string file_path;
};

std::string socket_path_default()
{
    const char *user = std::getenv("USER");

    if (user != nullptr && *user != '\0')
        return "/tmp/aiws-" + std::string(user) + ".sock";

    return "/tmp/aiws-" + std::to_string(static_cast<unsigned long>(::getuid())) + ".sock";
}

Options parse_arguments(int argc, char *argv[])
{
    Options options;
    options.socket_path = socket_path_default();

    if (argc < 2)
        throw std::invalid_argument("Usage: aiws_send [--socket PATH] add FILE");

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "--socket")
        {
            if (++i >= argc)
                throw std::invalid_argument("--socket requires a path");

            options.socket_path = argv[i];
        }
        else if (arg == "add")
        {
            if (!options.command.empty())
                throw std::invalid_argument("Only one command is allowed");

            options.command = arg;

            if (++i >= argc)
                throw std::invalid_argument("add requires a file path");

            options.file_path = argv[i];
        }
        else
        {
            throw std::invalid_argument("Unknown argument: " + arg);
        }
    }

    if (options.command.empty())
        throw std::invalid_argument("Missing command: add");

    return options;
}

int connect_to_server(const std::string &socket_path)
{
    // Create a Unix domain stream socket.
    int socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (socket_fd == -1)
        throw std::runtime_error("Failed to create socket");

    // Configure socket address.
    sockaddr_un address{};
    address.sun_family = AF_UNIX;

    if (socket_path.size() >= sizeof(address.sun_path))
    {
        close(socket_fd);
        throw std::runtime_error("Socket path too long");
    }

    std::memcpy(address.sun_path,
                socket_path.c_str(),
                socket_path.size() + 1);

    // Make socket nonblocking so connect() cannot hang.
    int flags = fcntl(socket_fd, F_GETFL, 0);

    if (flags == -1 ||
        fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        close(socket_fd);
        throw std::runtime_error("Failed to configure socket");
    }

    int result = connect(
        socket_fd,
        reinterpret_cast<sockaddr *>(&address),
        sizeof(address));

    if (result == -1 && errno != EINPROGRESS)
    {
        close(socket_fd);
        throw std::runtime_error("Failed to connect to AI Workspace");
    }

    if (result == -1)
    {
        // Wait up to 2 seconds for the connection.
        pollfd pfd{};
        pfd.fd = socket_fd;
        pfd.events = POLLOUT;

        do
        {
            result = poll(&pfd, 1, 2000);
        } while (result == -1 && errno == EINTR);

        if (result <= 0)
        {
            close(socket_fd);
            throw std::runtime_error("Connection timed out");
        }

        // Check whether the connection actually succeeded.
        int error = 0;
        socklen_t error_size = sizeof(error);

        if (getsockopt(socket_fd, SOL_SOCKET, SO_ERROR,
                       &error, &error_size) == -1 ||
            error != 0)
        {
            close(socket_fd);
            throw std::runtime_error("Failed to connect to AI Workspace");
        }
    }

    // Restore blocking mode. Communication functions below
    // will use poll() before reading and writing.
    if (fcntl(socket_fd, F_SETFL, flags) == -1)
    {
        close(socket_fd);
        throw std::runtime_error("Failed to configure socket");
    }

    return socket_fd;
}

bool write_all(int socket_fd, const void *data, size_t size)
{
    const char *buffer = static_cast<const char *>(data);
    size_t total_sent = 0;

    while (total_sent < size)
    {
        pollfd pfd{};
        pfd.fd = socket_fd;
        pfd.events = POLLOUT;

        int ready = poll(&pfd, 1, 5000);

        if (ready <= 0)
            return false;

        ssize_t sent = send(
            socket_fd,
            buffer + total_sent,
            size - total_sent,
            MSG_NOSIGNAL);

        if (sent == -1)
        {
            if (errno == EINTR || errno == EAGAIN)
                continue;

            return false;
        }

        if (sent == 0)
            return false;

        total_sent += static_cast<size_t>(sent);
    }

    return true;
}

void send_add_request(int socket_fd,
                      const std::string &file_path)
{
    // The protocol uses a 4-byte unsigned length.
    if (file_path.size() > UINT32_MAX)
        throw std::runtime_error("File path is too long");

    uint32_t length = htonl(
        static_cast<uint32_t>(file_path.size()));

    // Send the length prefix.
    if (!write_all(socket_fd, &length, sizeof(length)))
        throw std::runtime_error("Failed to send request length");

    // Send the actual path bytes.
    if (!write_all(socket_fd, file_path.data(), file_path.size()))
        throw std::runtime_error("Failed to send file path");
}

bool read_exact(int socket_fd, void *data, size_t size)
{
    char *buffer = static_cast<char *>(data);
    size_t total_received = 0;

    while (total_received < size)
    {
        pollfd pfd{};
        pfd.fd = socket_fd;
        pfd.events = POLLIN;

        int ready = poll(&pfd, 1, 5000);

        if (ready <= 0)
            return false;

        ssize_t received = recv(
            socket_fd,
            buffer + total_received,
            size - total_received,
            0);

        if (received == -1)
        {
            if (errno == EINTR || errno == EAGAIN)
                continue;

            return false;
        }

        if (received == 0)
            return false; // Connection closed prematurely.

        total_received += static_cast<size_t>(received);
    }

    return true;
}

int receive_response(int socket_fd)
{
    uint8_t status;

    // Receive the status byte.
    if (!read_exact(socket_fd, &status, sizeof(status)))
    {
        std::cerr << "Communication with AI Workspace failed\n";
        return 2;
    }

    // Application accepted the request.
    if (status == 0)
        return 0;

    // Application rejected the request.
    if (status == 1)
    {
        uint32_t network_length;

        if (!read_exact(socket_fd,
                        &network_length,
                        sizeof(network_length)))
        {
            std::cerr << "Failed to receive error message\n";
            return 2;
        }

        uint32_t length = ntohl(network_length);

        // Prevent unreasonable allocations from malformed responses.
        if (length > 65536)
        {
            std::cerr << "Invalid server response\n";
            return 2;
        }

        std::string error_message(length, '\0');

        if (!read_exact(socket_fd, error_message.data(), length))
        {
            std::cerr << "Failed to receive error message\n";
            return 2;
        }

        std::cerr << error_message << '\n';
        return 1;
    }

    std::cerr << "Invalid server response\n";
    return 2;
}

int main(int argc, char *argv[])
{
    try
    {
        Options options = parse_arguments(argc, argv);

        int socket_fd = connect_to_server(options.socket_path);

        try
        {
            std::filesystem::path path(options.file_path);
            path = std::filesystem::absolute(path);

            send_add_request(socket_fd, path.string());

            int result = receive_response(socket_fd);

            close(socket_fd);
            return result;
        }
        catch (...)
        {
            close(socket_fd);
            throw;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 2;
    }
}