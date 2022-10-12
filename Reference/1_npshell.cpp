#include "1_npshell.h"

static regex reg("[|!][0-9]+");
vector<Pipe> pipes;

int main()
{
    init_shell();

    string cmdLine;
    vector<cmdBlock> cmdBlocks;
    bool is_first_cmd = true;

    while (true)
    {
        cout << "% ";
        getline(cin, cmdLine);
        if (cmdLine.length() == 0)
            continue;
        parsePipe(cmdLine, cmdBlocks);
        is_first_cmd = true;

        while (!cmdBlocks.empty())
        {
            parseCmd(cmdBlocks[0]);

            checkPipeType(cmdBlocks[0]);

            if (!is_first_cmd)
            {
                for (int i = 0; i < pipes.size(); i++)
                {
                    if (pipes[i].count == -1)
                    {
                        cmdBlocks[0].fd_in = pipes[i].fd[0];
                        break;
                    }
                }
                cmdBlocks[0].has_fd_in = true;
            }
            else
            {
                for (int i = 0; i < pipes.size(); i++)
                {
                    if (pipes[i].count == 0)
                    {
                        cmdBlocks[0].fd_in = pipes[i].fd[0];
                        cmdBlocks[0].has_fd_in = true;
                        break;
                    }
                }
            }

            exec_cmd(cmdBlocks[0]);

            cmdBlocks.erase(cmdBlocks.begin());
            is_first_cmd = false;
        }

        for (int i = 0; i < pipes.size(); i++)
        {
            pipes[i].count--;
        }
    }

    return 0;
}

// Getting start for a shell
void init_shell()
{
    setenv("PATH", "bin:.", 1);
}

// parse input commandLine into commandBlocks
void parsePipe(string cmdLine, vector<cmdBlock> &cmdBlocks)
{
    int front = 0;
    int end;
    cmdBlock cmdBlock;
    cmdLine += "| ";

    while ((end = cmdLine.find(' ', cmdLine.find_first_of("|!", front))) != -1)
    {
        // initial cmdBlock
        cmdBlock.has_fd_in = false;
        cmdBlock.has_fd_out = false;
        cmdBlock.fd_in = 0;
        cmdBlock.fd_out = 0;
        cmdBlock.pipeType = 0;

        // parse cmdLine
        cmdBlock.cmd = cmdLine.substr(front, end - front);
        while (cmdBlock.cmd[0] == ' ')
            cmdBlock.cmd = (cmdBlock.cmd).substr(1);
        if (end == cmdLine.length() - 1)
            cmdBlock.cmd = (cmdBlock.cmd).substr(0, (cmdBlock.cmd).length() - 1);
        while ((cmdBlock.cmd)[(cmdBlock.cmd).length() - 1] == ' ')
            cmdBlock.cmd = (cmdBlock.cmd).substr(0, (cmdBlock.cmd).length() - 1);
        cmdBlocks.push_back(cmdBlock);

        front = end + 1;
    }
}

// parse inpur commandBlock into command and arguments
void parseCmd(cmdBlock &cmdBlock)
{
    int front = 0;
    int end;
    cmdBlock.cmd += " ";

    // read arguments
    while ((end = (cmdBlock.cmd).find(" ", front)) != -1)
    {
        if (end == front)
        {
            front = end + 1;
            continue;
        }
        (cmdBlock.argv).push_back((cmdBlock.cmd).substr(front, end - front));
        front = end + 1;
    }
}

// check the pipeType of the cmdBlock, and generate a new pipe. !! The fd_in in cmdBlock is not recorded.
void checkPipeType(cmdBlock &cmdBlock)
{ //, vector<Pipe> &pipes){
    string last_argv = cmdBlock.argv.back();
    Pipe newPipe;
    newPipe.count = 0;
    if (regex_match(last_argv, reg))
    { // number pipe case
        if (last_argv[0] == '|')
        {
            cmdBlock.pipeType = 1;
        }
        else if (last_argv[0] == '!')
        {
            cmdBlock.pipeType = 2;
        }
        newPipe.count = stoi(last_argv.substr(1));
        bool merge_numberpipe = false;
        for (int i = 0; i < pipes.size(); i++)
        {
            if (newPipe.count == pipes[i].count)
            {
                cmdBlock.fd_out = pipes[i].fd[1];
                merge_numberpipe = true;
                break;
            }
        }
        if (!merge_numberpipe)
        {
            pipe(newPipe.fd);
            cmdBlock.fd_out = newPipe.fd[1];
            pipes.push_back(newPipe);
        }
    }
    else
    {
        if (last_argv == "|")
        { // normal pipe case
            cmdBlock.pipeType = 1;
        }
        else if (last_argv == "!")
        { // error pipe case
            cmdBlock.pipeType = 2;
        }
        else
        { // no pipe case
            cmdBlock.pipeType = 0;
        }
        if (cmdBlock.pipeType != 0)
        {
            newPipe.count = -1;
            pipe(newPipe.fd);
            cmdBlock.fd_out = newPipe.fd[1];
            pipes.push_back(newPipe);
        }
    }
    if (cmdBlock.pipeType != 0)
    {
        cmdBlock.has_fd_out = true;
    }
}

void exec_cmd(cmdBlock &cmdBlock)
{
    char *argv[1000];
    string cmd = cmdBlock.argv[0];
    int child_pid;

    if (cmd == "printenv")
    {
        if (getenv(cmdBlock.argv[1].data()) != NULL)
            cout << getenv(cmdBlock.argv[1].data()) << endl;
    }
    else if (cmd == "setenv")
    {
        setenv(cmdBlock.argv[1].data(), cmdBlock.argv[2].data(), 1);
    }
    else if (cmd == "exit" || cmd == "EOF")
    {
        exit(0);
    }
    else
    {
        int status;
        while ((child_pid = fork()) < 0)
        {
            while (waitpid(-1, &status, WNOHANG) > 0)
                ;
        }
        switch (child_pid)
        {
        case 0:
            if (cmdBlock.has_fd_in)
            {
                dup2(cmdBlock.fd_in, STDIN_FILENO);
            }
            if (cmdBlock.has_fd_out)
            {
                if (cmdBlock.pipeType == 1)
                {
                    dup2(cmdBlock.fd_out, STDOUT_FILENO);
                }
                else if (cmdBlock.pipeType == 2)
                {
                    dup2(cmdBlock.fd_out, STDOUT_FILENO);
                    dup2(cmdBlock.fd_out, STDERR_FILENO);
                }
            }
            // child closes all pipe fds
            for (int i = 0; i < pipes.size(); i++)
            {
                close(pipes[i].fd[0]);
                close(pipes[i].fd[1]);
            }

            // file redirection
            int fd_redirection, index;
            if ((index = findIndex(cmdBlock.argv, ">")) == -1)
            {
                if (cmdBlock.pipeType == 0)
                    vec2arr(cmdBlock.argv, argv, (cmdBlock.argv).size());
                else
                    vec2arr(cmdBlock.argv, argv, (cmdBlock.argv).size() - 1);
            }
            else
            {
                fd_redirection = open((cmdBlock.argv).back().data(), O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
                dup2(fd_redirection, STDOUT_FILENO);
                dup2(fd_redirection, STDERR_FILENO);
                close(fd_redirection);
                vec2arr(cmdBlock.argv, argv, index);
            }
            if (execvp(cmd.data(), argv) == -1)
                cerr << "Unknown command: [" << cmd << "]." << endl;
            exit(0);
        default:
            // parent close useless pipe fd
            if (cmdBlock.has_fd_in)
            {
                for (int i = 0; i < pipes.size(); i++)
                {
                    if (pipes[i].fd[0] == cmdBlock.fd_in)
                    {
                        close(pipes[i].fd[0]);
                        close(pipes[i].fd[1]);
                        pipes.erase(pipes.begin() + i);
                        break;
                    }
                }
            }
            if (cmdBlock.pipeType == 0)
            {
                waitpid(child_pid, &status, 0);
            }
            else
            {
                waitpid(-1, &status, WNOHANG);
            }
        }
    }
}

void vec2arr(vector<string> &vec, char *arr[], int index)
{
    for (int i = 0; i < index; i++)
    {
        arr[i] = (char *)vec[i].data();
    }
    arr[vec.size()] = NULL;
}

int findIndex(vector<string> &vec, string str)
{
    int index;
    for (index = 0; index < vec.size(); index++)
    {
        if (vec[index] == str)
        {
            break;
        }
    }
    if (index == vec.size())
    {
        index = -1;
    }
    return index;
}