#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <string>
#include <fstream>
#include <map>
#include <boost/bind/bind.hpp>
#include <boost/asio.hpp>

using boost::asio::io_service;
using boost::asio::ip::tcp;
using namespace std;

typedef struct Query
{
    string host, port, file;
    Query(string host, string port, string file) : host(host), port(port), file(file) {}
} Query;

boost::asio::io_service io_context;
string sockIP, sockPort;

vector<Query> ParseQuery(string queryString);
void SendHTML();
void SendTable(int index, string message);
void DebugMessage(string message);

class client
    : public std::enable_shared_from_this<client>
{
public:
    client(tcp::socket socket_, int ID, string host, string port, string file)
        : clientSocket(std::move(socket_)), serveID(ID), host(host), port(port), file(file)
    {
        if (host.length() == 0)
            return;
        fileStream.open(("./test_case/" + file));
        exitFlag = false;
    }
    void start()
    {
        do_read();
    }

private:
    tcp::socket clientSocket;
    int serveID;
    string host, port, file;
    ifstream fileStream;
    enum
    {
        max_length = 1024
    };
    char data_[max_length];
    bool exitFlag;

    void do_read()
    {
        if (exitFlag)
            return;
        auto self(shared_from_this());
        clientSocket.async_read_some(boost::asio::buffer(data_, max_length),
                                     [this, self](boost::system::error_code ec, std::size_t length)
                                     {
                                         if (!ec)
                                         {
                                             data_[length] = '\0';
                                             string message(data_);
                                             // send to shell
                                             cout << "<script>document.getElementById('s" + to_string(serveID) + "').innerHTML += '" << RefactorString(message) << "';</script>\n";
                                             cout.flush();
                                             if (message.find("% ", 0) != string::npos)
                                                 do_write();
                                             do_read();
                                         }
                                         //  else
                                         //      cerr << "client do_read error: " << ec.message() << endl;
                                     });
    }

    void do_write()
    {
        auto self(shared_from_this());
        string command;
        if (!getline(fileStream, command))
            cerr << "Function 'getline' failed." << endl;
        command += "\n";
        // Send command
        cout << "<script>document.getElementById('s" + to_string(serveID) + "').innerHTML += '<b>" << RefactorString(command) << "</b>';</script>\n";
        cout.flush();
        boost::asio::async_write(clientSocket, boost::asio::buffer(command, command.length()),
                                 [this, self, command](boost::system::error_code ec, std::size_t /*length*/)
                                 {
                                     if (!ec)
                                     {
                                         string temp = command;
                                         size_t pos;
                                         while ((pos = temp.find('\r')) != string::npos)
                                             temp.erase(pos);
                                         while ((pos = temp.find('\n')) != string::npos)
                                             temp.erase(pos);

                                         if (temp == "exit")
                                         {
                                             exitFlag = true;
                                             clientSocket.close();
                                         }
                                     }
                                     else
                                         cerr << "Client do_write error: " << ec.message() << endl;
                                 });
    }

    string RefactorString(string message)
    {
        map<char, string> htmlMap = {{'\n', "<br>"}, {'\r', ""}, {'\'', "\\'"}, {'<', "&lt;"}, {'>', "&gt;"}, {'&', "&amp;"}};
        string result = "";
        for (char c : message)
        {
            map<char, string>::iterator it = htmlMap.find(c);
            if (it != htmlMap.end())
                result += it->second;
            else
                result += c;
        }
        return result;
    }
};

class server
{
public:
    server(vector<Query> queries) : resolver(io_context), queries(queries)
    {
        Process();
    }
    void Process()
    {
        if (sockIP.length() != 0)
        {
            tcp::resolver::query query(sockIP, sockPort);
            resolver.async_resolve(query, boost::bind(&server::ProxyConnection, this, boost::asio::placeholders::error, boost::asio::placeholders::iterator));
            return;
        }
        for (int i = 0; i < (int)queries.size(); i++)
        {
            tcp::resolver::query query(queries[i].host, queries[i].port);
            resolver.async_resolve(query, boost::bind(&server::Connection, this, i, boost::asio::placeholders::error, boost::asio::placeholders::iterator));
        }
    }

    void ProxyConnection(boost::system::error_code ec, tcp::resolver::iterator it)
    {
        if (!ec)
        {
            for (int i = 0; i < (int)queries.size(); i++)
            {
                clientSockets[i] = new tcp::socket(io_context);
                (*clientSockets[i]).async_connect(*it, boost::bind(&server::CreateProxySession, this, i, boost::asio::placeholders::error, it));
            }
        }
    }

    void CreateProxySession(int serveID, boost::system::error_code ec, tcp::resolver::iterator it)
    {
        if (!ec)
        {
            unsigned char socksRequest[200];
            socksRequest[0] = 4, socksRequest[1] = 1;
            socksRequest[2] = stoi(queries[serveID].port) / 256, socksRequest[3] = stoi(queries[serveID].port) % 256;
            socksRequest[4] = 0, socksRequest[5] = 0, socksRequest[6] = 0;
            socksRequest[7] = 1, socksRequest[8] = 0;
            for (int i = 0; i < (int)queries[serveID].host.length(); i++)
                socksRequest[9 + i] = queries[serveID].host[i];
            socksRequest[9 + queries[serveID].host.length()] = 0;

            (*clientSockets[serveID]).async_send(boost::asio::buffer(socksRequest, sizeof(unsigned char) * 200), [this, serveID](boost::system::error_code ec, size_t length)
                                                 {
                                                    if (!ec)
                                                    {
                                                      (*clientSockets[serveID]).async_read_some(boost::asio::buffer(sockReply, 8), [this, serveID](boost::system::error_code ec, size_t length)
                                                        {
                                                            if(!ec)
                                                            {
                                                               if(sockReply[1] == 90) make_shared<client>(move(*clientSockets[serveID]), serveID, queries[serveID].host, queries[serveID].port, queries[serveID].file)->start();
                                                               else return; 
                                                            }
                                                            else cerr << "clientSockets async_read_some error: " << ec.message()<<endl; 
                                                        });

                                                    } });
        }
    }

    void Connection(int serveID, boost::system::error_code ec, tcp::resolver::iterator it)
    {
        if (!ec)
        {
            clientSockets[serveID] = new tcp::socket(io_context);
            (*clientSockets[serveID]).async_connect(*it, boost::bind(&server::CreateSession, this, serveID, boost::asio::placeholders::error, it));
        }
    }

    void CreateSession(int serveID, boost::system::error_code ec, tcp::resolver::iterator it)
    {
        if (!ec)
            make_shared<client>(move(*clientSockets[serveID]), serveID, queries[serveID].host, queries[serveID].port, queries[serveID].file)->start();
    }

private:
    tcp::resolver resolver;
    tcp::socket *clientSockets[5];
    unsigned char sockReply[8];
    vector<Query> queries;
};

int main()
{
    SendHTML();
    vector<Query> queries = ParseQuery((string(getenv("QUERY_STRING")) + "&"));
    // DebugMessage("sockIP: " + sockIP);
    // DebugMessage("sockPort: " + sockPort);

    try
    {
        for (int index = 0; index < (int)queries.size(); index++)
            SendTable(index, queries[index].host + ":" + queries[index].port);

        server s(queries);
        io_context.run();
    }
    catch (exception &e)
    {
        cerr << "Exception: " << e.what() << "\n";
    }
}

vector<Query> ParseQuery(string queryString)
{
    size_t start = 0, end;
    int index = 0;
    vector<Query> result;
    vector<string> queryData;
    while ((end = queryString.find('&', start)) != string::npos)
    {
        string queryBlock = queryString.substr(start, end - start);
        queryBlock = queryBlock.substr(queryBlock.find('=') + 1);
        if (!queryBlock.empty())
        {
            queryData.push_back(queryBlock);
            if (queryData.size() == 3)
            {
                // queries[index].host = queryData[0];
                // queries[index].port = queryData[1];
                // queries[index].file = queryData[2];
                result.push_back(Query(queryData[0], queryData[1], queryData[2]));
                queryData.clear();
                index++;
            }
        }
        start = end + 1;
    }
    if (queryData.size())
        sockIP = queryData[0], sockPort = queryData[1];
    return result;
}

void SendHTML()
{
    cout << "Content-type: text/html\r\n\r\n";
    cout << "<!DOCTYPE html>\n"
         << "<html lang =\"en\">\n"
         << "  <head>\n"
         << "    <meta charset=\"UTF-8\" />\n"
         << "    <title>NP Project 3 Console</title>\n"
         << "    <link\n"
         << "      rel=\"stylesheet\"\n"
         << "      href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\n"
         << "      integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\n"
         << "      crossorigin=\"anonymous\"\n"
         << "    />\n"
         << "    <link\n"
         << "      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n"
         << "      rel=\"stylesheet\"\n"
         << "    />\n"
         << "    <link\n"
         << "      rel=\"icon\"\n"
         << "      type=\"image/png\"\n"
         << "      href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\n"
         << "    />\n"
         << "    <style>\n"
         << "    * {\n"
         << "      font-family: 'Source Code Pro', monospace;\n"
         << "      font-size: 1rem !important;\n"
         << "    }\n"
         << "    body {\n"
         << "      background-color: #212529;\n"
         << "    }\n"
         << "    pre {\n"
         << "      color: #cccccc;\n"
         << "    }\n"
         << "    b {\n"
         << "      color: #01b468;\n"
         << "    }\n"
         << "    </style>\n"
         << "  </head>\n"
         << "  <body>\n"
         << "    <table class=\"table table-dark table-bordered\">\n"
         << "      <thead>\n"
         << "        <tr id=\"tableHead\">\n"
         << "        </tr>\n"
         << "      </thead>\n"
         << "      <tbody>\n"
         << "        <tr id=\"tableBody\">\n"
         << "        </tr>\n"
         << "      </tbody>\n"
         << "    </table>\n"
         << "  </body>\n"
         << "</html>\n";
    cout.flush();
}

void SendTable(int index, string address)
{
    string message = "<th scope=\\\"col\\\">" + address + "</th>";
    cout << "<script>document.getElementById('tableHead').innerHTML += '" << message << "';</script>\n";
    cout.flush();
    message = "<td><pre id=\\\"s" + to_string(index) + "\\\" class=\\\"mb-0\\\"></pre></td>";
    cout << "<script>document.getElementById('tableBody').innerHTML += '" << message << "';</script>\n";
    cout.flush();
}

void DebugMessage(string message)
{
    cout << "<!-- " << message << " -->\n";
    // cerr << message << endl;
}