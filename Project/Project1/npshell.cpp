#include <iostream>
#include <string>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/wait.h>
#include <errno.h>
#include <pwd.h>

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
pid_t childPid;

// function
void SplitCommand(string command);
void TransferToCommandBlock();
void CreatePipe(CommandBlock &block);
void CheckFDIN(CommandBlock &block, int cnt);
int ExecuteCommand(CommandBlock &block);
void minusPipeCount();

/* demo */
// string home_dir()
// {
//     return getpwuid(getuid())->pw_dir;
// };
// string historyPath = home_dir() + "/.npshell_history";
// int historyFD;

int main()
{
    // initialize: set path
    setenv("PATH", "bin:.", 1);

    /* demo */
    // historyFD = open(historyPath.data(), O_CREAT | O_RDWR | O_APPEND);

    bool firstCommand = true, lastNumbered = false;
    CommandBlock block;
    string commandLine;
    while (true)
    {
        cout << "% ";
        getline(cin, commandLine);
        if (commandLine.length() == 0)
            continue;

        // 2022/10/22 demo
        int writeHistory = write(historyFD, (commandLine + "\n").data(), commandLine.length());

        firstCommand = true, lastNumbered = false;

        commands.clear();
        SplitCommand(commandLine + " ");
        TransferToCommandBlock();

        while (!commandBlocks.empty())
        {
            CreatePipe(commandBlocks[0]);
            CheckFDIN(commandBlocks[0], (firstCommand) ? 0 : -1);
            if (firstCommand)
                firstCommand = false;

            ExecuteCommand(commandBlocks[0]);

            if (commandBlocks[0].pipeNumber > 0)
            {
                minusPipeCount();
                firstCommand = true;
                if (commandBlocks.size() == 1)
                    lastNumbered = true;
            }
            /* demo */
            // if (commandBlocks[0].argv[0] == "exit")
            //     close(historyFD);
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
        exit(0);
    else if (command == "printenv")
    {
        if (getenv((block.argv)[1].data()) != NULL)
            cout << getenv((block.argv)[1].data()) << endl;
    }
    else if (command == "setenv")
        setenv((block.argv)[1].data(), (block.argv)[2].data(), 1);
    else
    {
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
            exit(0);
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