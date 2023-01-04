#include <iostream>
#include <string>
#include <vector>
#include <fcntl.h>
#include <cstdlib>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using namespace std;

// structure
typedef struct numberPipe
{
    int count, fd[2];
    numberPipe(int number) : count(number){};
} numberPipe;

typedef struct CommandBlock
{
    vector<string> argv;
    bool FDIN, FDOUT;
    int pipeStatus, pipeNumber, fd_in, fd_out;
    CommandBlock() : argv(vector<string>{}), FDIN(false), FDOUT(false), pipeStatus(0), pipeNumber(0), fd_in(-1), fd_out(-1) {}
    CommandBlock(int status) : argv(vector<string>{}), FDIN(false), FDOUT(false), pipeStatus(status), pipeNumber(0), fd_in(-1), fd_out(-1) {}
    CommandBlock(int status, vector<string> arguments) : argv(arguments), FDIN(false), FDOUT(false), pipeStatus(status), pipeNumber(0), fd_in(-1), fd_out(-1) {}
    CommandBlock(int status, vector<string> arguments, int count) : argv(arguments), FDIN(false), FDOUT(false), pipeStatus(status), pipeNumber(count), fd_in(-1), fd_out(-1) {}
} CommandBlock;

// global variable
vector<string> commands;
vector<CommandBlock> commandBlocks;
vector<numberPipe> numberedPipes;

// function
int CreateSocket(int port);
void Shell();
void SplitCommand(string command);
void TransferToCommandBlock();
void CreatePipe(CommandBlock &block);
void CheckFDIN(CommandBlock &block, int cnt);
int ExecuteCommand(CommandBlock &block);
void minusPipeCount();

int main(int argc, char *argv[])
{
    if (argc != 2)
        return 0;
    int port = atoi(argv[1]);
    int serverFD = CreateSocket(port);
    if (listen(serverFD, 0) < 0)
        cerr << "Listen fail.\n", exit(EXIT_FAILURE);

    struct sockaddr_in childAddress;
    int childSocket, addressSize = sizeof(childAddress);
    setenv("PATH", "bin:.", 1);
    while (true)
    {
        if ((childSocket = accept(serverFD, (struct sockaddr *)&childAddress, (socklen_t *)&addressSize)) < 0)
            cerr << "Accept fail.\n", exit(EXIT_FAILURE);
        int childPid, status;
        while ((childPid = fork()) < 0)
            while (waitpid(-1, &status, WNOHANG) > 0)
                ;
        if (childPid > 0)
        {
            close(childSocket);
            waitpid(childPid, NULL, 0);
            while (waitpid(-1, NULL, WNOHANG) > 0)
                ;
        }
        else
        {
            dup2(childSocket, STDIN_FILENO);
            dup2(childSocket, STDOUT_FILENO);
            dup2(childSocket, STDERR_FILENO);
            close(childSocket), close(serverFD);
            Shell();
            exit(EXIT_SUCCESS);
        }
    }
    return 0;
}

int CreateSocket(int port)
{
    int serverFD, optval = 1;
    if ((serverFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        cerr << "Socket create fail.\n", exit(EXIT_FAILURE);
    if (setsockopt(serverFD, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) == -1)
        cerr << "Set socket fail.\n", exit(EXIT_FAILURE);

    struct sockaddr_in socketAddress;
    bzero((char *)&socketAddress, sizeof(socketAddress));
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_addr.s_addr = INADDR_ANY;
    socketAddress.sin_port = htons(port);
    if (bind(serverFD, (struct sockaddr *)&socketAddress, sizeof(socketAddress)) < 0)
        cerr << "Socket bind fail.\n", exit(EXIT_FAILURE);
    return serverFD;
}

void Shell()
{
    // initialize: set path
    setenv("PATH", "bin:.", 1);

    string commandLine;
    bool firstCommand = true, lastNumbered = false;
    CommandBlock block;
    while (true)
    {
        cout << "% ";
        getline(cin, commandLine);
        if (commandLine.length() == 0)
            continue;

        firstCommand = true, lastNumbered = false;
        commands.clear();
        SplitCommand(commandLine + " ");
        TransferToCommandBlock();

        while (!commandBlocks.empty())
        {
            CreatePipe(commandBlocks[0]);
            CheckFDIN(commandBlocks[0], (firstCommand) ? 0 : -1);

            ExecuteCommand(commandBlocks[0]);

            if (firstCommand)
                firstCommand = false;

            if (commandBlocks[0].pipeNumber > 0)
            {
                minusPipeCount();
                firstCommand = true;
                if (commandBlocks.size() == 1)
                    lastNumbered = true;
            }
            commandBlocks.erase(commandBlocks.begin());
        }
        if (!lastNumbered)
            minusPipeCount();
    }
}

void SplitCommand(string command)
{
    size_t start = 0, end = 0;
    while ((end = command.find(' ', start)) != string::npos)
    {
        if (end != start)
            commands.push_back(command.substr(start, end - start));
        start = end + 1;
    }
    return;
}

void TransferToCommandBlock()
{
    vector<string> arguments;
    int pipeStatus = 0;
    while (!commands.empty())
    {
        if (commands[0][0] == '|' || commands[0][0] == '!')
        {
            pipeStatus = (commands[0][0] == '|') ? 1 : 2;
            commandBlocks.push_back(CommandBlock(pipeStatus, arguments, (commands[0].size() == 1) ? -1 : stoi(commands[0].substr(1))));
            arguments.clear();
        }
        else
            arguments.push_back(commands[0]);
        commands.erase(commands.begin());
    }
    if (arguments.size())
        commandBlocks.push_back(CommandBlock(0, arguments)), arguments.clear();

    return;
}

void CreatePipe(CommandBlock &block)
{
    if (block.pipeStatus)
    {
        int cnt = block.pipeNumber;
        bool merge = false;
        numberPipe newPipe(cnt);
        if (cnt > 0)
            for (int i = 0; i < numberedPipes.size(); i++)
                if (numberedPipes[i].count == cnt)
                {
                    block.fd_out = numberedPipes[i].fd[1], merge = true;
                    break;
                }

        if (!merge)
        {
            pipe(newPipe.fd);
            block.fd_out = newPipe.fd[1];
            numberedPipes.push_back(newPipe);
        }
        block.FDOUT = true;
    }
}

void CheckFDIN(CommandBlock &block, int cnt)
{
    for (numberPipe pipe : numberedPipes)
        if (pipe.count == cnt)
        {
            block.fd_in = pipe.fd[0], block.FDIN = true;
            break;
        }
    return;
}

void minusPipeCount()
{
    for (int i = 0; i < numberedPipes.size(); i++)
        numberedPipes[i].count--;
    return;
}

int ExecuteCommand(CommandBlock &block)
{
    char *arguments[4000];
    string command = block.argv[0];
    if (command == "exit" || command == "EOF")
        exit(EXIT_SUCCESS);
    else if (command == "printenv")
    {
        if (getenv((block.argv)[1].data()) != NULL)
            cout << getenv((block.argv)[1].data()) << endl;
    }
    else if (command == "setenv")
        setenv((block.argv)[1].data(), (block.argv)[2].data(), 1);
    else
    {
        pid_t childPid;
        int status;
        while ((childPid = fork()) < 0)
            while (waitpid(-1, &status, WNOHANG) > 0)
                ;
        if (childPid == 0) // child process
        {
            if (block.FDIN)
                dup2(block.fd_in, STDIN_FILENO);
            if (block.FDOUT)
            {
                dup2(block.fd_out, STDOUT_FILENO);
                if (block.pipeStatus == 2)
                    dup2(block.fd_out, STDERR_FILENO);
            }

            // close child pipe fds
            for (numberPipe pipe : numberedPipes)
                close(pipe.fd[0]), close(pipe.fd[1]);

            // file redirection
            int index;
            for (index = 0; index < (block.argv).size(); index++)
                if ((block.argv)[index] == ">")
                    break;
            if (index != (block.argv).size())
            {
                int fd = open((block.argv).back().data(), O_CREAT | O_TRUNC | O_RDWR, 0644);
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            // execution
            for (int i = 0; i < index; i++)
                arguments[i] = (block.argv)[i].data();
            arguments[index] = NULL;

            if (execvp(command.data(), arguments) == -1)
                cerr << "Unknown command: [" << command << "]." << endl;

            exit(EXIT_SUCCESS);
        }
        else // parent process
        {
            if (block.FDIN) // close useless parent pipe
                for (int i = 0; i < numberedPipes.size(); i++)
                    if (numberedPipes[i].fd[0] == block.fd_in)
                    {
                        close(numberedPipes[i].fd[0]), close(numberedPipes[i].fd[1]);
                        numberedPipes.erase(numberedPipes.begin() + i);
                        break;
                    }

            if (block.pipeStatus)
                waitpid(-1, &status, WNOHANG);
            else
                waitpid(childPid, &status, 0);
        }
    }
    return EXIT_SUCCESS;
}