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

boost::asio::io_context io_context;
int max;

typedef struct Query
{
    string host, port, file;
    Query(string host, string port, string file) : host(host), port(port), file(file) {}
} Query;

class session : public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket, char **arguments) : socket_(std::move(socket)), arguments(arguments) {}

    void start();

    void do_write(string message);

private:
    tcp::socket socket_;
    enum
    {
        max_length = 1024
    };
    char data_[max_length];
    char **arguments;
    map<string, string> connectionEnv;
    shared_ptr<session> selfPointer;

    void do_read();
    void ParseRequest();
    void PanelCGI();
    void ConsoleCGI();
    vector<Query> ParseQuery(string queryString);
    void SendHTML();
    void SendTable(int index, string address);
};
class client
    : public std::enable_shared_from_this<client>
{
public:
    client(boost::asio::io_context &io_context, int ID, string host, string port, string file, shared_ptr<session> sessionPointer)
        : resolver_(io_context), socket_(io_context), io_context_(io_context), serveID(ID), host(host), port(port), file(file), session_(sessionPointer) {}
    void start();

private:
    tcp::resolver resolver_;
    tcp::socket socket_;
    boost::asio::io_context &io_context_;
    boost::asio::ip::tcp::resolver::results_type endpoints_;
    shared_ptr<session> session_;
    int serveID;
    string host, port, file;
    ifstream fileStream;
    enum
    {
        max_length = 1024
    };
    char data_[max_length];

    void do_resolve();
    void do_connect();
    void do_read();
    void do_write(string message);
    string RefactorString(string message);
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
                    std::make_shared<session>(std::move(socket), arguments)->start();
                else
                    cerr << "[ERROR] Accept error: " << ec.message() << endl;

                do_accept();
            });
    }
};

void session::start()
{
    do_read();
}

void session::do_write(string message)
{
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(message, message.length()),
                             [this, self](boost::system::error_code ec, std::size_t /*length*/)
                             {
                                 if (!ec)
                                 {
                                     //  do_read();
                                 }
                                 else
                                     cerr << "[ERROR] Session write error: " << ec.message() << endl;
                             });
}

void session::do_read()
{
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
                            [this, self](boost::system::error_code ec, std::size_t length)
                            {
                                if (!ec)
                                {
                                    selfPointer = self;
                                    // data_[length] = '\0';
                                    ParseRequest();

                                    string tempUri = connectionEnv["REQUEST_URI"] + "?";
                                    string path = tempUri.substr(0, tempUri.find('?', 0));
                                    if (path == "/panel.cgi")
                                        PanelCGI();
                                    else if (path == "/console.cgi")
                                        ConsoleCGI();

                                    socket_.close();
                                }
                            });
}

void session::ParseRequest()
{
    string request(data_);
    // memset(data_, '\0', sizeof(data_));

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

void session::PanelCGI()
{
    string host_menu = "";
    for (int index = 1; index < 13; index++)
        host_menu += "<option value=\"nplinux" + to_string(index) + ".cs.nctu.edu.tw\">nplinux" + to_string(index) + "</option>";
    string test_case_menu = "";
    for (int index = 1; index < 6; index++)
        test_case_menu += "<option value=\"t" + to_string(index) + ".txt\">t" + to_string(index) + ".txt</option>";

    string message = "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";
    message += "<!DOCTYPE html>\n";
    message += "<html lang=\"en\">\n";
    message += "  <head>\n";
    message += "    <title>NP Project 3 Panel</title>\n";
    message += "    <link\n";
    message += "      rel=\"stylesheet\"\n";
    message += "      href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\n";
    message += "      integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\n";
    message += "      crossorigin=\"anonymous\"\n";
    message += "    />\n";
    message += "    <link\n";
    message += "      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n";
    message += "      rel=\"stylesheet\"\n";
    message += "    />\n";
    message += "    <link\n";
    message += "      rel=\"icon\"\n";
    message += "      type=\"image/png\"\n";
    message += "      href=\"https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png\"\n";
    message += "    />\n";
    message += "    <style>\n";
    message += "      * {\n";
    message += "        font-family: 'Source Code Pro', monospace;\n";
    message += "      }\n";
    message += "    </style>\n";
    message += "  </head>\n";
    message += "  <body class=\"bg-secondary pt-5\">\n";
    message += "    <form action=\"console.cgi\" method=\"GET\">\n";
    message += "      <table class=\"table mx-auto bg-light\" style=\"width: inherit\">\n";
    message += "        <thead class=\"thead-dark\">\n";
    message += "          <tr>\n";
    message += "            <th scope=\"col\">#</th>\n";
    message += "            <th scope=\"col\">Host</th>\n";
    message += "            <th scope=\"col\">Port</th>\n";
    message += "            <th scope=\"col\">Input File</th>\n";
    message += "          </tr>\n";
    message += "        </thead>\n";
    message += "        <tbody>\n";

    for (int i = 0; i < 5; i++)
    {
        message += "          <tr>\n";
        message += "            <th scope=\"row\" class=\"align-middle\">Session" + to_string(i + 1) + "</th>\n";
        message += "            <td>\n";
        message += "              <div class=\"input-group\">\n";
        message += "                <select name=\"h" + to_string(i) + "\" class=\"custom-select\">\n";
        message += "                  <option></option>" + host_menu + "\n";
        message += "                </select>\n";
        message += "                <div class=\"input-group-append\">\n";
        message += "                  <span class=\"input-group-text\">.cs.nctu.edu.tw</span>\n";
        message += "                </div>\n";
        message += "              </div>\n";
        message += "            </td>\n";
        message += "            <td>\n";
        message += "              <input name=\"p" + to_string(i) + "\" type=\"text\" class=\"form-control\" size=\"5\" />\n";
        message += "            </td>\n";
        message += "            <td>\n";
        message += "              <select name=\"f" + to_string(i) + "\" class=\"custom-select\">\n";
        message += "                <option></option>\n";
        message += "                " + test_case_menu + "\n";
        message += "              </select>\n";
        message += "            </td>\n";
        message += "          </tr>\n";
    }
    message += "          \n";
    message += "          <tr>\n";
    message += "            <td colspan=\"3\"></td>\n";
    message += "            <td>\n";
    message += "              <button type=\"submit\" class=\"btn btn-info btn-block\">Run</button>\n";
    message += "            </td>\n";
    message += "          </tr>\n";
    message += "        </tbody>\n";
    message += "      </table>\n";
    message += "    </form>\n";
    message += "  </body>\n";
    message += "</html>\n";
    do_write(message);
}

void session::ConsoleCGI()
{
    vector<Query> queries = ParseQuery(connectionEnv["QUERY_STRING"] + "&");
    SendHTML();
    try
    {
        for (int index = 0; index < (int)queries.size(); index++)
        {
            ;
            SendTable(index, queries[index].host + ":" + queries[index].port);
            shared_ptr<client> client_ = make_shared<client>(io_context, index, queries[index].host, queries[index].port, queries[index].file, selfPointer);
            client_->start();
        }
        io_context.run();
        socket_.close();
    }
    catch (exception &e)
    {
        cerr << "Exception: " << e.what() << "\n";
    }
}

vector<Query> session::ParseQuery(string queryString)
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
            result.push_back(Query(queryData[0], queryData[1], queryData[2]));
            queryData.clear();
            index++;
        }
        start = end + 1;
    }
    return result;
}

void session::SendHTML()
{
    string message = "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";
    message += "<!DOCTYPE html>\n";
    message += "<html lang =\"en\">\n";
    message += "  <head>\n";
    message += "    <meta charset=\"UTF-8\" />\n";
    message += "    <title>NP Project 3 Console</title>\n";
    message += "    <link\n";
    message += "      rel=\"stylesheet\"\n";
    message += "      href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\n";
    message += "      integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\n";
    message += "      crossorigin=\"anonymous\"\n";
    message += "    />\n";
    message += "    <link\n";
    message += "      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n";
    message += "      rel=\"stylesheet\"\n";
    message += "    />\n";
    message += "    <link\n";
    message += "      rel=\"icon\"\n";
    message += "      type=\"image/png\"\n";
    message += "      href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\n";
    message += "    />\n";
    message += "    <style>\n";
    message += "    * {\n";
    message += "      font-family: 'Source Code Pro', monospace;\n";
    message += "      font-size: 1rem !important;\n";
    message += "    }\n";
    message += "    body {\n";
    message += "      background-color: #212529;\n";
    message += "    }\n";
    message += "    pre {\n";
    message += "      color: #cccccc;\n";
    message += "    }\n";
    message += "    b {\n";
    message += "      color: #01b468;\n";
    message += "    }\n";
    message += "    </style>\n";
    message += "  </head>\n";
    message += "  <body>\n";
    message += "    <table class=\"table table-dark table-bordered\">\n";
    message += "      <thead>\n";
    message += "        <tr id=\"tableHead\">\n";
    message += "        </tr>\n";
    message += "      </thead>\n";
    message += "      <tbody>\n";
    message += "        <tr id=\"tableBody\">\n";
    message += "        </tr>\n";
    message += "      </tbody>\n";
    message += "    </table>\n";
    message += "  </body>\n";
    message += "</html>\n";
    do_write(message);
}

void session::SendTable(int index, string address)
{
    string message = "<script>document.getElementById('tableHead').innerHTML += '<th scope=\\\"col\\\">" + address + "</th>';</script>\n";
    do_write(message);
    message = "<script>document.getElementById('tableBody').innerHTML += '<td><pre id=\\\"s" + to_string(index) + "\\\" class=\\\"mb-0\\\"></pre></td>';</script>\n";
    do_write(message);
}

void client::start()
{
    do_resolve();
}

void client::do_resolve()
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

void client::do_connect()
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
                                       if (fileStream.fail())
                                           cerr << "[ERROR] Open file error." << endl;
                                       else
                                           do_read();
                                   }
                                   else
                                       socket_.close();
                               });
}

void client::do_read()
{
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
                            [this, self](boost::system::error_code ec, std::size_t length)
                            {
                                if (!ec)
                                {
                                    data_[length] = '\0';
                                    string commandLine(data_);
                                    memset(data_, '\0', sizeof(data_));
                                    // send to shell
                                    string message = "<script>document.getElementById('s" + to_string(serveID) + "').innerHTML += '" + RefactorString(commandLine) + "';</script>\n";
                                    session_->do_write(message);
                                    if (length != 0)
                                    {
                                        if (commandLine.find('%', 0) != string::npos)
                                        {
                                            string command;
                                            getline(fileStream, command);
                                            command += "\n";
                                            // Send command
                                            message = "<script>document.getElementById('s" + to_string(serveID) + "').innerHTML += '<b>" + RefactorString(command) + "</b>';</script>\n";
                                            session_->do_write(message);

                                            do_write(command);
                                        }
                                        else
                                            do_read();
                                    }
                                }
                                else
                                    socket_.close();
                            });
}

void client::do_write(string message)
{
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(message, message.length()),
                             [this, self, message](boost::system::error_code ec, std::size_t /*length*/)
                             {
                                 if (!ec)
                                 {
                                     if (message.compare("exit\n"))
                                         do_read();
                                     else
                                         socket_.close();
                                 }
                             });
}

string client::RefactorString(string message)
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

int main(int argc, char *argv[])
{
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: async_tcp_echo_server <port>\n";
            exit(EXIT_FAILURE);
        }
        server s(io_context, std::atoi(argv[1]), argv);

        io_context.run();
    }
    catch (std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}

void DebugMessage(string message)
{
    cout << "<!-- " << message << " -->\n";
}