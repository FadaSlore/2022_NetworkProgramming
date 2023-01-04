#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <sstream>
#include <string>
#include <map>
#include <boost/asio.hpp>
#include <sys/wait.h>

using boost::asio::ip::tcp;
using namespace std;

class session : public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket, char **arguments) : socket_(std::move(socket)), arguments(arguments) {}

    void start() { do_read(); }

private:
    tcp::socket socket_;
    enum
    {
        max_length = 1024
    };
    char data_[max_length];
    char **arguments;
    map<string, string> connectionEnv;

    void do_read()
    {
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(data_, max_length),
                                [this, self](boost::system::error_code ec, std::size_t length)
                                {
                                    if (!ec)
                                    {
                                        data_[length] = '\0';
                                        ParseRequest();
                                        pid_t childPid;
                                        int status;
                                        while ((childPid = fork()) < 0)
                                            while (waitpid(-1, &status, WNOHANG) < 0)
                                                ;
                                        if (childPid == 0) // child
                                        {
                                            // SERVER ADDR
                                            connectionEnv["SERVER_ADDR"] = socket_.local_endpoint().address().to_string();
                                            // SERVER PORT
                                            connectionEnv["SERVER_PORT"] = to_string(socket_.local_endpoint().port());
                                            // REMOTE ADDR
                                            connectionEnv["REMOTE_ADDR"] = socket_.remote_endpoint().address().to_string();
                                            // REMOTE PORT
                                            connectionEnv["REMOTE_PORT"] = to_string(socket_.remote_endpoint().port());
                                            SetEnvironment();
                                            dup2(socket_.native_handle(), STDIN_FILENO);
                                            dup2(socket_.native_handle(), STDOUT_FILENO);
                                            string tempUri = connectionEnv["REQUEST_URI"] + "?";
                                            string path = "." + tempUri.substr(0, tempUri.find('?', 0));

                                            /* demo */
                                            // if (path == "./panel.cgi")
                                            // {
                                            //     cout << "HTTP/1.1 200 OK\r\n"
                                            //          << flush;
                                            //     socket_.close();

                                            //     if (execvp(path.data(), arguments) == -1)
                                            //     {
                                            //         // TODO: handle error
                                            //     }
                                            // }
                                            // else
                                            // {
                                            //     cout << "HTTP/1.1 403 Forbidden\r\n"
                                            //          << flush;
                                            //     socket_.close();
                                            // }

                                            cout << "HTTP/1.1 200 OK\r\n"
                                                 << flush;
                                            socket_.close();

                                            if (execvp(path.data(), arguments) == -1)
                                            {
                                                // TODO: handle error
                                            }

                                            exit(0);
                                        }
                                        else // parent
                                        {
                                            socket_.close();
                                            waitpid(childPid, &status, WNOHANG);
                                        }

                                        do_write(length);
                                    }
                                });
    }

    void do_write(std::size_t length)
    {
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(data_, length),
                                 [this, self](boost::system::error_code ec, std::size_t /*length*/)
                                 {
                                     if (!ec)
                                     {
                                         do_read();
                                     }
                                 });
    }

    void ParseRequest()
    {
        string request(data_);
        memset(data_, '\0', sizeof(data_));

        size_t start = 0, end;
        while ((end = request.find('\r', start)) != string::npos)
            request.erase(request.begin() + end), start = end;

        int firstLineEnd = request.find('\n', 0);
        int secondLineEnd = request.find('\n', firstLineEnd + 1);
        string firstLine = request.substr(0, firstLineEnd), secondLine = request.substr(firstLineEnd + 1, secondLineEnd - firstLineEnd - 1);
        // REQUEST METHOD
        start = 0, end = firstLine.find(' ', 0);
        connectionEnv["REQUEST_METHOD"] = firstLine.substr(start, end - start);
        // REQUEST URI
        start = end + 1, end = firstLine.find(' ', start);
        string checkQuery = firstLine.substr(start, end - start);
        connectionEnv["REQUEST_URI"] = checkQuery;
        // QUERY STRING
        size_t queryIndex;
        string queryString = "";
        if ((queryIndex = checkQuery.find('?', 0)) != string::npos)
            queryString = checkQuery.substr(queryIndex + 1);
        connectionEnv["QUERY_STRING"] = queryString;
        // SERVER PROTOCOL
        start = end + 1, end = firstLine.find(' ', start);
        connectionEnv["SERVER_PROTOCOL"] = firstLine.substr(start, end - start);
        // HTTP HOST
        string host = secondLine.substr(secondLine.find(' ', 0) + 1);
        size_t portIndex = host.find(':', 0);
        connectionEnv["HTTP_HOST"] = secondLine.substr(secondLine.find(' ', 0) + 1);
        // SERVER ADDR
        connectionEnv["SERVER_ADDR"] = host.substr(0, portIndex);
        // SERVER PORT
        connectionEnv["SERVER_PORT"] = host.substr(portIndex + 1);
        // REMOTE ADDR
        // REMOTE PORT
    }

    void SetEnvironment()
    {
        for (map<string, string>::iterator it = connectionEnv.begin(); it != connectionEnv.end(); it++)
            setenv((it->first).data(), (it->second).data(), 1);
    }
};

class server
{
public:
    server(boost::asio::io_context &io_context, short port, char **argv)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        arguments = argv;
        do_accept();
    }

private:
    tcp::acceptor acceptor_;
    char **arguments;

    void do_accept()
    {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket)
            {
                if (!ec)
                {
                    std::make_shared<session>(std::move(socket), arguments)->start();
                }

                do_accept();
            });
    }
};

int main(int argc, char *argv[])
{
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: async_tcp_echo_server <port>\n";
            return 1;
        }
        cerr << "port: " << atoi(argv[1]) << endl;

        boost::asio::io_context io_context;
        server s(io_context, std::atoi(argv[1]), argv);
        io_context.run();
    }
    catch (std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}