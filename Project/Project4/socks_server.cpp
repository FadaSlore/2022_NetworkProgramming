#include <cstdlib>
#include <stdio.h>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include <utility>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <sys/wait.h>
#include <map>

using boost::asio::io_service;
using boost::asio::ip::tcp;
using namespace std;

boost::asio::io_service io_context;

typedef struct Socket4Request
{
    unsigned char VN, CD;
    unsigned int dstPort;
    string dstIP, userID, domainName;
} Socket4Request;

/*demo*/
// map<string, int> ipConnect;

class session
    : public std::enable_shared_from_this<session>
{
public:
    // session(tcp::socket socket, bool acceptIP) : clientSocket(std::move(socket)), accept(acceptIP), serverSocket(io_context), resolver(io_context), bindAcceptor(io_context, tcp::endpoint(tcp::v4(), 0))
    session(tcp::socket socket) : clientSocket(std::move(socket)), serverSocket(io_context), resolver(io_context), bindAcceptor(io_context, tcp::endpoint(tcp::v4(), 0))
    {
        memset(clientData, 0, max_length);
        memset(serverData, 0, max_length);
        printConsole = false;
        accept = false;
    }
    void start()
    {
        do_read();
    }

private:
    tcp::socket clientSocket;
    tcp::socket serverSocket;
    tcp::resolver resolver;
    enum
    {
        max_length = 1024
    };
    unsigned char data_[max_length];

    Socket4Request request;

    // TODO: check variables
    unsigned char clientData[max_length];
    unsigned char serverData[max_length];
    unsigned short bindPort;
    tcp::acceptor bindAcceptor;

    unsigned char replyPacket[8];
    bool accept;
    bool printConsole;

    void do_read()
    {
        auto self(shared_from_this());
        clientSocket.async_read_some(boost::asio::buffer(data_, max_length),
                                     [this, self](boost::system::error_code ec, std::size_t length)
                                     {
                                         if (!ec)
                                         {
                                             request.VN = data_[0], request.CD = data_[1];
                                             request.dstPort = (data_[2] << 8) | data_[3];
                                             request.dstIP = to_string((int)data_[4]) + "." + to_string((int)data_[5]) + "." + to_string((int)data_[6]) + "." + to_string((int)data_[7]);

                                             string temp = "";
                                             int index = 8;
                                             while (data_[index] != 0x00)
                                                 temp += to_string(data_[index++]);
                                             request.userID = temp;
                                             if (data_[4] == 0x00 && data_[5] == 0 && data_[6] == 0)
                                             {
                                                 index += 1, temp = "";
                                                 while (data_[index] != 0x00)
                                                     temp += data_[index++];
                                                 request.domainName = temp;
                                             }
                                             else
                                                 request.domainName = "";

                                             memset(replyPacket, '\0', sizeof(unsigned char) * 8);
                                             replyPacket[0] = 0;
                                             if (request.CD == 2) // BIND
                                             {
                                                 /* demo */
                                                 //  accept &= CheckFirewall();
                                                 accept = CheckFirewall();
                                                 if (accept)
                                                     replyPacket[1] = 90;
                                                 else
                                                     replyPacket[1] = 91;
                                                 BindReply();
                                                 return;
                                             }
                                             bindAcceptor.close();
                                             do_write(length);
                                         }
                                     });
    }

    void BindReply()
    {
        bindAcceptor.set_option(tcp::acceptor::reuse_address(true));
        bindAcceptor.listen();
        bindPort = bindAcceptor.local_endpoint().port();
        replyPacket[2] = (unsigned char)(bindPort / 256), replyPacket[3] = (unsigned char)(bindPort % 256);

        auto self(shared_from_this());
        clientSocket.async_send(boost::asio::buffer(replyPacket, 8),
                                [this, self](boost::system::error_code ec, std::size_t length)
                                {
                                    if (replyPacket[1] == 91)
                                        PrintConsole();
                                    else if (!ec)
                                        SendReply();
                                });
    }

    void SendReply()
    {
        auto self(shared_from_this());
        bindAcceptor.async_accept(serverSocket,
                                  [this, self](boost::system::error_code ec)
                                  {
                                      BindAccept((ec ? true : false));
                                  });
    }

    void BindAccept(bool errorExist)
    {
        if (errorExist)
            replyPacket[1] = 91, PrintConsole();

        auto self(shared_from_this());
        clientSocket.async_send(boost::asio::buffer(replyPacket, 8),
                                [this, self, errorExist](boost::system::error_code ec, std::size_t len)
                                {
                                    if (!ec)
                                    {
                                        if (errorExist)
                                        {
                                            clientSocket.close(), serverSocket.close();
                                            exit(0);
                                        }
                                        else
                                            ReadData(3);
                                    }
                                });
    }

    void do_write(size_t length)
    {
        auto self(shared_from_this());
        if (request.domainName.empty())
        {
            boost::asio::ip::tcp::endpoint dstEndpoint(boost::asio::ip::address::from_string(request.dstIP), request.dstPort);
            /* demo */
            //  accept &= CheckFirewall();
            accept = CheckFirewall();
            if (accept)
                replyPacket[1] = 90, serverSocket.async_connect(dstEndpoint, boost::bind(&session::Redirector, self, 3, boost::asio::placeholders::error));
            else
                replyPacket[1] = 91, PrintConsole();
        }
        else
        {
            tcp::resolver::query query(request.domainName, to_string(request.dstPort));
            resolver.async_resolve(query, boost::bind(&session::Connect, self, boost::asio::placeholders::error, boost::asio::placeholders::iterator));
        }
    }

    void Redirector(int caseNum, boost::system::error_code ec)
    {
        auto self(shared_from_this());
        clientSocket.async_send(boost::asio::buffer(replyPacket, 8),
                                [this, self, caseNum](boost::system::error_code ec, size_t length)
                                {
                                    if (!ec)
                                        ReadData(caseNum);
                                });
    }

    void Connect(boost::system::error_code ec, tcp::resolver::iterator it)
    {
        auto self(shared_from_this());
        if (!ec)
        {
            boost::asio::ip::tcp::endpoint endPoint = (*it);
            request.dstIP = endPoint.address().to_string();
            /* demo */
            //  accept &= CheckFirewall();
            accept = CheckFirewall();
            if (accept)
                replyPacket[1] = 90, serverSocket.async_connect(endPoint, boost::bind(&session::Redirector, self, 3, boost::asio::placeholders::error));
            else
                replyPacket[1] = 91, PrintConsole();
        }
    }

    void PrintConsole()
    {
        printConsole = true;

        boost::asio::ip::tcp::endpoint remote_ep = clientSocket.remote_endpoint();
        boost::asio::ip::address remote_ad = remote_ep.address();
        string srcIP = remote_ad.to_string(), srcPort = to_string(remote_ep.port());
        string command = (request.CD == 1) ? "CONNECT" : (request.CD == 2 ? "BIND" : ("UNKNOWN -- CD = " + to_string(request.CD)));

        cout << "<S_IP>: " << srcIP << endl
             << "<S_PORT>: " << srcPort << endl
             << "<D_IP>: " << request.dstIP << endl
             << "<D_PORT>: " << request.dstPort << endl
             << "<Command>: " << command << endl
             << "<Reply>: " << (accept ? "Accept" : "Reject") << endl
             << "-------------------------------------" << endl
             << flush;

        if (!accept)
        {
            clientSocket.close();
            exit(0);
        }
    }

    void ReadData(int caseNum)
    {
        if (!printConsole)
            PrintConsole();

        auto self(shared_from_this());
        if (caseNum & 1)
        {
            clientSocket.async_read_some(boost::asio::buffer(clientData, max_length),
                                         [this, self](boost::system::error_code ec, size_t length)
                                         {
                                             if (!ec)
                                                 WriteData(1, length);
                                             else if (ec == boost::asio::error::eof)
                                             {
                                                 serverSocket.async_send(boost::asio::buffer(clientData, length),
                                                                         [this, self, ec](boost::system::error_code error, size_t length)
                                                                         {
                                                                             if (error)
                                                                                 cerr << "ReadData serverSocket error" << error.message() << endl;
                                                                         });
                                                 serverSocket.shutdown(boost::asio::ip::tcp::socket::shutdown_send);
                                             }
                                         });
        }
        if (caseNum & 2)
        {
            serverSocket.async_read_some(boost::asio::buffer(serverData, max_length),
                                         [this, self](boost::system::error_code ec, size_t length)
                                         {
                                             if (!ec)
                                                 WriteData(2, length);
                                             else if (ec == boost::asio::error::eof)
                                             {
                                                 clientSocket.async_send(boost::asio::buffer(serverData, length),
                                                                         [this, self, ec](boost::system::error_code error, size_t length)
                                                                         {
                                                                             if (error)
                                                                                 cerr << "ReadData clientSocket error" << error.message() << endl;
                                                                         });
                                                 clientSocket.shutdown(boost::asio::ip::tcp::socket::shutdown_send);
                                             }
                                         });
        }
    }

    void WriteData(int who, size_t length)
    {
        auto self(shared_from_this());
        if (who == 1)
        {
            async_write(serverSocket, boost::asio::buffer(clientData, length),
                        [this, self, length](boost::system::error_code ec, size_t len)
                        {
                            if (!ec)
                                memset(clientData, 0, max_length), ReadData(1);
                            else
                                cerr << "WriteData error: " << ec.message() << endl;
                        });
        }
        else if (who == 2)
        {
            async_write(clientSocket, boost::asio::buffer(serverData, length),
                        [this, self, length](boost::system::error_code ec, size_t len)
                        {
                            if (!ec)
                                memset(serverData, 0, max_length), ReadData(2);
                            else
                                cerr << "WriteData error: " << ec.message() << endl;
                        });
        }
    }

    bool CheckFirewall()
    {
        vector<string> IP;
        string dstIPFind = request.dstIP + '.';
        size_t start = 0, end;
        while ((end = dstIPFind.find('.', start)) != string::npos)
        {
            IP.push_back(dstIPFind.substr(start, end - start));
            start = end + 1;
        }

        ifstream inputFile;
        inputFile.open("./socks.conf");
        string input;
        while (getline(inputFile, input))
        {
            while ((end = input.find('\r')) != string::npos)
                input.erase(end);
            while ((end = input.find('\n')) != string::npos)
                input.erase(end);
            string permit = input.substr(9) + ".";
            start = 0;
            vector<string> permitIP;
            while ((end = permit.find('.', start)) != string::npos)
            {
                permitIP.push_back(permit.substr(start, end - start));
                start = end + 1;
            }
            if ((input[7] == 'c' && request.CD == 1) || (input[7] == 'b' && request.CD == 2))
            {
                int times = 0;
                for (int i = 0; i < (int)permitIP.size(); i++)
                    if (permitIP[i] == "*" || (permitIP[i] == IP[i]))
                        times++;
                if (times == 4)
                    return true;
            }
        }
        return false;
    }
};

class server
{
public:
    server(boost::asio::io_context &io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        do_accept();
    }

private:
    tcp::acceptor acceptor_;

    void do_accept()
    {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket)
            {
                if (!ec)
                {
                    io_context.notify_fork(io_service::fork_prepare);
                    pid_t childPid;
                    int status;
                    while ((childPid = fork()) < 0)
                        while (waitpid(-1, &status, WNOHANG) < 0)
                            ;

                    /* demo */
                    // bool accept = true;
                    // boost::asio::ip::tcp::endpoint remote_ep = socket.remote_endpoint();
                    // boost::asio::ip::address remote_ad = remote_ep.address();
                    // string srcIP = remote_ad.to_string();
                    // ipConnect[srcIP]++;

                    if (childPid != 0) // parent
                    {
                        io_context.notify_fork(io_service::fork_parent);
                        socket.close();
                    }
                    else // child
                    {
                        /* demo */
                        // if (ipConnect.find(srcIP) != ipConnect.end())
                        // {
                        //     if (ipConnect[srcIP] > 3)
                        //     {
                        //         accept = false;
                        //     }
                        // }

                        io_context.notify_fork(io_service::fork_child);
                        acceptor_.close();
                        /* demo */
                        // std::make_shared<session>(std::move(socket), accept)->start();
                        std::make_shared<session>(std::move(socket))->start();
                    }
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
            cerr << "Usage: async_tcp_echo_server <port>\n";
            return 1;
        }
        /* demo */
        // ipConnect.clear();
        server s(io_context, std::atoi(argv[1]));
        io_context.run();
    }
    catch (std::exception &e)
    {
        // cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}