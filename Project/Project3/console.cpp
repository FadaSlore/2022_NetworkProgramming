#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <string>
#include <fstream>
#include <map>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;
using namespace std;

typedef struct Query
{
    string host, port, file;
    Query(string host, string port, string file) : host(host), port(port), file(file) {}
} Query;

vector<Query> ParseQuery(string queryString);
void SendHTML();
void SendTable(int index, string message);
void DebugMessage(string message);

class client
    : public std::enable_shared_from_this<client>
{
public:
    client(boost::asio::io_context &io_context, int ID, string host, string port, string file)
        : resolver_(io_context), socket_(io_context), io_context_(io_context), serveID(ID), host(host), port(port), file(file) {}
    void start()
    {
        do_resolve();
    }

private:
    tcp::resolver resolver_;
    tcp::socket socket_;
    boost::asio::io_context &io_context_;
    boost::asio::ip::tcp::resolver::results_type endpoints_;
    int serveID;
    string host, port, file;
    ifstream fileStream;
    enum
    {
        max_length = 1024
    };
    char data_[max_length];

    void do_resolve()
    {
        auto self(shared_from_this());
        resolver_.async_resolve(host, port,
                                [this, self](const boost::system::error_code &ec,
                                             const boost::asio::ip::tcp::resolver::results_type endpoints)
                                {
                                    if (!ec)
                                    {
                                        endpoints_ = endpoints;
                                        do_connect();
                                    }
                                    else
                                        socket_.close();
                                });
    }

    void do_connect()
    {
        auto self(shared_from_this());
        boost::asio::async_connect(socket_, endpoints_,
                                   [this, self](const boost::system::error_code &ec, tcp::endpoint ed)
                                   {
                                       if (!ec)
                                       {
                                           memset(data_, '\0', sizeof(data_));
                                           string path = "./test_case/" + file;
                                           fileStream.open(path.data());
                                           do_read();
                                       }
                                       else
                                           socket_.close();
                                   });
    }

    void do_read()
    {
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(data_, max_length),
                                [this, self](boost::system::error_code ec, std::size_t length)
                                {
                                    if (!ec)
                                    {
                                        data_[length] = '\0';
                                        string message(data_);
                                        memset(data_, '\0', sizeof(data_));
                                        // send to shell
                                        cout << "<script>document.getElementById('s" + to_string(serveID) + "').innerHTML += '" << RefactorString(message) << "';</script>\n";
                                        cout.flush();
                                        if (length != 0)
                                        {
                                            if (message.find("% ", 0) != string::npos)
                                                do_write();
                                            else
                                                do_read();
                                        }
                                    }
                                    else
                                        socket_.close();
                                });
    }

    void do_write()
    {
        auto self(shared_from_this());
        string command;
        getline(fileStream, command);
        command += "\n";
        // Send command
        cout << "<script>document.getElementById('s" + to_string(serveID) + "').innerHTML += '<b>" << RefactorString(command) << "</b>';</script>\n";
        cout.flush();
        boost::asio::async_write(socket_, boost::asio::buffer(command, command.length()),
                                 [this, self, command](boost::system::error_code ec, std::size_t /*length*/)
                                 {
                                     if (!ec)
                                     {
                                         if (command.compare("exit\n"))
                                             do_read();
                                         else
                                             socket_.close();
                                     }
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

int main()
{
    vector<Query> queries = ParseQuery((string(getenv("QUERY_STRING")) + "&"));
    SendHTML();

    try
    {
        boost::asio::io_context io_context;
        for (int index = 0; index < (int)queries.size(); index++)
        {
            SendTable(index, queries[index].host + ":" + queries[index].port);
            make_shared<client>(io_context, index, queries[index].host, queries[index].port, queries[index].file)->start();
        }
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
        if (queryBlock.empty())
            break;
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
        start = end + 1;
    }
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
}