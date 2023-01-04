#include <iostream>
#include <stdio.h>
#include <cstdlib>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <map>
#include <cstring>
#include <sstream>
#include <vector>
#include <fcntl.h>
#include <sys/stat.h>
#include <algorithm>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <dirent.h>

// TODO: add 'fflush(stdout)' to cout

#define PIPEPATH "user_pipe/"

#define NOPIPE 0
#define NORMALPIPE 1
#define ERRORPIPE 2

#define BUFFERSIZE 15000
#define ONLINEMAX 31
#define MAXLENGTH 21
#define MESSAGELENGTH 0x400000

using namespace std;

typedef struct NumberPipe
{
    int count, fd[2];
    NumberPipe(int number) : count(number) {}
} NumberPipe;

typedef struct CommandBlock
{
    vector<string> argv;
    bool FDIN, FDOUT, userPipeSend, userPipeReceive, sendError, recvError;
    int pipeStatus, pipeNumber, fd_in, fd_out, sendToID, receiveFromID;
    CommandBlock() : argv(vector<string>{}), FDIN(false), FDOUT(false), userPipeSend(false), userPipeReceive(false), sendError(false), recvError(false), pipeStatus(0), pipeNumber(0), fd_in(-1), fd_out(-1), sendToID(-1), receiveFromID(-1) {}
    CommandBlock(int status) : argv(vector<string>{}), FDIN(false), FDOUT(false), userPipeSend(false), userPipeReceive(false), sendError(false), recvError(false), pipeStatus(status), pipeNumber(0), fd_in(-1), fd_out(-1), sendToID(-1), receiveFromID(-1) {}
    CommandBlock(int status, vector<string> arguments) : argv(arguments), FDIN(false), FDOUT(false), userPipeSend(false), userPipeReceive(false), sendError(false), recvError(false), pipeStatus(status), pipeNumber(0), fd_in(-1), fd_out(-1), sendToID(-1), receiveFromID(-1) {}
    CommandBlock(int status, vector<string> arguments, bool send, bool recv, int sendID, int recvID) : argv(arguments), FDIN(false), FDOUT(false), userPipeSend(send), sendError(false), recvError(false), userPipeReceive(recv), pipeStatus(status), pipeNumber(0), fd_in(-1), fd_out(-1), sendToID(sendID), receiveFromID(recvID) {}
    CommandBlock(int status, vector<string> arguments, int count) : argv(arguments), FDIN(false), FDOUT(false), userPipeSend(false), userPipeReceive(false), sendError(false), recvError(false), pipeStatus(status), pipeNumber(count), fd_in(-1), fd_out(-1), sendToID(-1), receiveFromID(-1) {}
    CommandBlock(int status, vector<string> arguments, bool send, bool recv, int sendID, int recvID, int count) : argv(arguments), FDIN(false), FDOUT(false), userPipeSend(send), sendError(false), recvError(false), userPipeReceive(recv), pipeStatus(status), pipeNumber(count), fd_in(-1), fd_out(-1), sendToID(sendID), receiveFromID(recvID) {}
} CommandBlock;

typedef struct Client
{
    bool active;
    int id, fd;
    pid_t childPid;
    char ip[INET6_ADDRSTRLEN];
    char name[MAXLENGTH];
    // map<string, string> environment;
} Client;

typedef struct FIFOUnit
{
    bool used;
    int fd_in, fd_out;
    char path[MAXLENGTH];
} FIFOUnit;

typedef struct FIFOInfo
{
    FIFOUnit list[ONLINEMAX][ONLINEMAX];
} FIFOInfo;

// global variable
int sharedMemoryFD, infoSharedFD, pipeSharedFD;
// int serverSocket;
int clientID;
// fd_set readFDs, activeFDs;
const size_t clientInfoSize = sizeof(Client) * ONLINEMAX;
const size_t fifoInfoSize = sizeof(FIFOInfo);
const size_t pathSize = sizeof(char) * MAXLENGTH;

string commandLine;
vector<string> commands;
vector<CommandBlock> commandBlocks;
vector<NumberPipe> numberPipes;
// function
void InitVariables();
int CreateSocket(int port);
int GetClientID();
void BroadcastMessage(string message, int targetID);
void SIGHANDLE(int sig);
// Shell Functions
bool Shell();
void SIGHANDLECHILD(int sig);
void SplitCommand(string command);
void TransferToCommandBlock();
void CreatePipe(CommandBlock &block);
void CheckFDIN(CommandBlock &block, int cnt);
void minusPipeCount();
bool ExecuteCommand(CommandBlock &block);
void SetEnv(string name, string value);
void PrintEnv(string name);
void Login(string IP);
void Logout();
void Who();
void ChangeName(string newName);
void Yell(string msg);
void Tell(string msg, int targetID);
string GetUserName(int targetID);
bool CheckUserExist(int targetID);
bool CheckUserPipeExist(int sourceID, int targetID);
void ClearUserPipes();

void InitVariables()
{
    commandLine.clear();
    commands.clear(), commandBlocks.clear();
    numberPipes.clear();

    if (NULL == opendir(PIPEPATH))
        mkdir(PIPEPATH, 0777);

    sharedMemoryFD = shm_open("Broadcast", O_CREAT | O_RDWR, 0666);
    ftruncate(sharedMemoryFD, MESSAGELENGTH);

    infoSharedFD = shm_open("ClientInfo", O_CREAT | O_RDWR, 0666);
    ftruncate(infoSharedFD, clientInfoSize);
    Client *clientInfo = (Client *)mmap(NULL, clientInfoSize, PROT_READ | PROT_WRITE, MAP_SHARED, infoSharedFD, 0);
    for (int id = 1; id < ONLINEMAX; id++)
        clientInfo[id].active = false;
    munmap(clientInfo, clientInfoSize);

    pipeSharedFD = shm_open("UserPipe", O_CREAT | O_RDWR, 0666);
    ftruncate(pipeSharedFD, fifoInfoSize);
    FIFOInfo *fifoInfo = (FIFOInfo *)mmap(NULL, fifoInfoSize, PROT_READ | PROT_WRITE, MAP_SHARED, pipeSharedFD, 0);
    for (int id1 = 1; id1 < ONLINEMAX; id1++)
        for (int id2 = 1; id2 < ONLINEMAX; id2++)
        {
            fifoInfo->list[id1][id2].used = false, fifoInfo->list[id1][id2].fd_in = -1, fifoInfo->list[id1][id2].fd_out = -1;
            memset(&fifoInfo->list[id1][id2].path, 0, pathSize);
        }
    munmap(fifoInfo, fifoInfoSize);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
        return 0;
    InitVariables();
    int port = atoi(argv[1]);
    int serverSocket = CreateSocket(port);
    if (listen(serverSocket, 0) < 0)
    {
        cerr << "Listen fail." << endl;
        exit(EXIT_FAILURE);
    }
    while (true)
    {
        struct sockaddr_in clientAddress;
        int length = sizeof(clientAddress), clientSocket;
        if ((clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddress, (socklen_t *)&length)) < 0)
        {
            cerr << "accept fail." << endl;
            exit(EXIT_FAILURE);
        }
        if ((clientID = GetClientID()) == -1)
        {
            cerr << "Client max." << endl;
            continue;
        }
        int status;
        pid_t childPid;
        while ((childPid = fork()) < 0)
            while (waitpid(-1, &status, WNOHANG) > 0)
                ;
        if (childPid == 0) // Child
        {
            dup2(clientSocket, STDIN_FILENO);
            dup2(clientSocket, STDOUT_FILENO);
            dup2(clientSocket, STDERR_FILENO);
            close(clientSocket), close(serverSocket);
            char clientIP[INET6_ADDRSTRLEN];
            sprintf(clientIP, "%s:%d", inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port));
            // string clientIP = string(inet_ntoa(clientAddress.sin_addr)).+ ":" + to_string(ntohs(clientAddress.sin_port));
            Client *clientInfo = (Client *)mmap(NULL, clientInfoSize, PROT_READ | PROT_WRITE, MAP_SHARED, infoSharedFD, 0);
            strncpy(clientInfo[clientID].ip, clientIP, INET6_ADDRSTRLEN);
            strcpy(clientInfo[clientID].name, "(no name)");
            clientInfo[clientID].childPid = getpid(), clientInfo[clientID].active = true;
            munmap(clientInfo, clientInfoSize);
            signal(SIGUSR1, SIGHANDLE);
            signal(SIGUSR2, SIGHANDLE);
            signal(SIGINT, SIGHANDLE);
            signal(SIGQUIT, SIGHANDLE);
            signal(SIGTERM, SIGHANDLE);
            string welcomeMessage = "****************************************\n** Welcome to the information server. **\n****************************************";
            cout << welcomeMessage << endl;
            // cout << "clienIP: " << string(clientIP) << endl;
            Login(string(clientIP));

            if (!Shell())
            {
                Logout();
                // close(STDIN_FILENO), close(STDOUT_FILENO), close(STDERR_FILENO);
                // Client *clientInfo = (Client *)mmap(NULL, clientInfoSize, PROT_READ | PROT_WRITE, MAP_SHARED, infoSharedFD, 0);
                // clientInfo[clientID].active = false;
                // munmap(clientInfo, clientInfoSize);
                exit(EXIT_SUCCESS);
            }
        }
        else // Parent
        {
            signal(SIGCHLD, SIGHANDLE);
            signal(SIGINT, SIGHANDLE);
            signal(SIGQUIT, SIGHANDLE);
            signal(SIGTERM, SIGHANDLE);
            close(clientSocket);
        }
    }
    exit(EXIT_SUCCESS);
}

int CreateSocket(int port)
{
    int serverSocket, optval = 1;
    struct sockaddr_in socketAddress;
    bzero((char *)&socketAddress, sizeof(socketAddress));
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_addr.s_addr = INADDR_ANY;
    socketAddress.sin_port = htons(port);

    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        cerr << "Socket create fail." << endl;
        exit(EXIT_FAILURE);
    }
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) == -1)
    {
        cerr << "Set socket fail." << endl;
        exit(EXIT_FAILURE);
    }
    if (bind(serverSocket, (struct sockaddr *)&socketAddress, sizeof(socketAddress)) < 0)
    {
        cerr << "Socket bind fail." << endl;
        exit(EXIT_FAILURE);
    }
    return serverSocket;
}

int GetClientID()
{
    int resultID = -1;
    Client *clientInfo = (Client *)mmap(NULL, clientInfoSize, PROT_READ, MAP_SHARED, infoSharedFD, 0);
    for (int id = 1; id < ONLINEMAX; id++)
        if (!clientInfo[id].active)
        {
            resultID = id;
            break;
        }
    munmap(clientInfo, clientInfoSize);
    return resultID;
}

void BroadcastMessage(string message, int targetID)
{
    Client *clientInfo = (Client *)mmap(NULL, clientInfoSize, PROT_READ, MAP_SHARED, infoSharedFD, 0);
    char *messagePointer = (char *)mmap(NULL, MESSAGELENGTH, PROT_READ | PROT_WRITE, MAP_SHARED, sharedMemoryFD, 0);
    strncpy(messagePointer, message.c_str() + '\0', message.length() + 1);
    munmap(messagePointer, MESSAGELENGTH);
    if (targetID == -1)
    {
        for (int id = 1; id < ONLINEMAX; id++)
            if (clientInfo[id].active)
                kill(clientInfo[id].childPid, SIGUSR1);
    }
    else
        kill(clientInfo[targetID].childPid, SIGUSR1);
    munmap(clientInfo, clientInfoSize);
    usleep(50);
}

void SIGHANDLE(int sig)
{
    if (sig == SIGUSR1) // broadcast
    {
        char *messagePointer = (char *)mmap(NULL, MESSAGELENGTH, PROT_READ, MAP_SHARED, sharedMemoryFD, 0);
        cout << string(messagePointer) << endl;
        munmap(messagePointer, MESSAGELENGTH);
    }
    else if (sig == SIGUSR2) // user pipe
    {
        FIFOInfo *fifoInfo = (FIFOInfo *)mmap(NULL, fifoInfoSize, PROT_READ | PROT_WRITE, MAP_SHARED, pipeSharedFD, 0);
        for (int id = 1; id < ONLINEMAX; id++)
        {
            if (fifoInfo->list[clientID][id].used)
            {
                close(fifoInfo->list[clientID][id].fd_out);
                memset(&fifoInfo->list[clientID][id].path, 0, pathSize);
                fifoInfo->list[clientID][id].fd_out = -1, fifoInfo->list[clientID][id].used = false;
            }
            if (fifoInfo->list[id][clientID].used)
            {
                close(fifoInfo->list[id][clientID].fd_in);
                memset(&fifoInfo->list[id][clientID].path, 0, pathSize);
                fifoInfo->list[id][clientID].fd_in = -1, fifoInfo->list[id][clientID].used = false;
            }
            if (fifoInfo->list[id][clientID].fd_in == -1 && fifoInfo->list[id][clientID].path[0] != 0)
                fifoInfo->list[id][clientID].fd_in = open(fifoInfo->list[id][clientID].path, O_RDONLY);
        }
        munmap(fifoInfo, fifoInfoSize);
    }
    else if (sig == SIGINT || sig == SIGQUIT || sig == SIGTERM)
    {
        // cout << "sig: " << ((sig == SIGINT) ? "SIGINT" : (sig == SIGQUIT ? "SIGQUIT" : "SIGTERM")) << endl;
        exit(EXIT_SUCCESS);
    }
    signal(sig, SIGHANDLE);
}

// Shell Functions
bool Shell()
{
    // initialize: set path
    clearenv();
    SetEnv("PATH", "bin:.");
    bool firstCommand = true, lastNumbered = false;
    while (true)
    {
        cout << "% ";
        getline(cin, commandLine);
        commandLine.erase(remove(commandLine.begin(), commandLine.end(), '\n'), commandLine.end());
        commandLine.erase(remove(commandLine.begin(), commandLine.end(), '\r'), commandLine.end());
        // cout << "commandLine: " << commandLine << endl;
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

            if (!ExecuteCommand(commandBlocks[0]))
                return false;

            if (firstCommand)
                firstCommand = false;

            if (commandBlocks[0].pipeNumber > 0 && (commandBlocks[0].pipeStatus <= ERRORPIPE))
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
    return true;
}

void SIGHANDLECHILD(int sig)
{
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0)
        ;
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
    bool userPipeSend = false, userPipeRecv = false;
    int sendToID = -1, recvFromID = -1;
    while (!commands.empty())
    {
        if (commands[0][0] == '|' || commands[0][0] == '!')
        {
            pipeStatus = (commands[0][0] == '|') ? NORMALPIPE : ERRORPIPE;
            commandBlocks.push_back(CommandBlock(pipeStatus, arguments, userPipeSend, userPipeRecv, sendToID, recvFromID, (commands[0].size() == 1) ? -1 : stoi(commands[0].substr(1))));
            arguments.clear(), userPipeSend = false, userPipeRecv = false, sendToID = -1, recvFromID = -1;
        }
        else if ((commands[0][0] == '>' || commands[0][0] == '<') && (commands[0].size() > 1))
        {
            bool send = (commands[0][0] == '>');
            int ID = stoi(commands[0].substr(1));
            if (send)
                userPipeSend = true, sendToID = ID;
            else
                userPipeRecv = true, recvFromID = ID;
        }
        else
            arguments.push_back(commands[0]);
        commands.erase(commands.begin());
    }
    if (arguments.size())
        commandBlocks.push_back(CommandBlock(0, arguments, userPipeSend, userPipeRecv, sendToID, recvFromID)), arguments.clear();

    return;
}

void CreatePipe(CommandBlock &block)
{
    if (block.pipeStatus == NORMALPIPE || block.pipeStatus == ERRORPIPE)
    {
        int cnt = block.pipeNumber;
        bool merge = false;
        NumberPipe newPipe(cnt);
        if (cnt > 0)
            for (int i = 0; i < numberPipes.size(); i++)
                if (numberPipes[i].count == cnt)
                {
                    block.fd_out = numberPipes[i].fd[1], merge = true;
                    break;
                }

        if (!merge)
        {
            pipe(newPipe.fd);
            block.fd_out = newPipe.fd[1];
            numberPipes.push_back(newPipe);
        }
        block.FDOUT = true;
    }

    if (block.userPipeReceive)
    {
        if (CheckUserExist(block.receiveFromID))
        {
            if (CheckUserPipeExist(block.receiveFromID, clientID))
            {
                string message = "*** " + GetUserName(clientID) + " (#" + to_string(clientID) + ") just received from " + GetUserName(block.receiveFromID) + " (#" + to_string(block.receiveFromID) + ") by '" + commandLine + "' ***";
                BroadcastMessage(message, -1);
            }
            else
            {
                block.recvError = true;
                string message = "*** Error: the pipe #" + to_string(block.receiveFromID) + "->#" + to_string(clientID) + " does not exist yet. ***";
                cout << message << endl;
            }
        }
        else
            block.recvError = true;
    }
    if (block.userPipeSend)
    {
        if (CheckUserExist(block.sendToID))
        {
            if (CheckUserPipeExist(clientID, block.sendToID))
            {
                block.sendError = true;
                string message = "*** Error: the pipe #" + to_string(clientID) + "->#" + to_string(block.sendToID) + " already exists. ***";
                cout << message << endl;
            }
            else
            {
                string message = "*** " + GetUserName(clientID) + " (#" + to_string(clientID) + ") just piped '" + commandLine + "' to " + GetUserName(block.sendToID) + " (#" + to_string(block.sendToID) + ") ***";
                BroadcastMessage(message, -1);
                string sendPath = PIPEPATH + to_string(clientID) + to_string(block.sendToID);
                mkfifo(sendPath.c_str(), S_IFIFO | 0666);
                FIFOInfo *fifoInfo = (FIFOInfo *)mmap(NULL, fifoInfoSize, PROT_READ | PROT_WRITE, MAP_SHARED, pipeSharedFD, 0);
                strncpy(fifoInfo->list[clientID][block.sendToID].path, sendPath.c_str(), MAXLENGTH);
                Client *clientInfo = (Client *)mmap(NULL, clientInfoSize, PROT_READ, MAP_SHARED, infoSharedFD, 0);
                kill(clientInfo[block.sendToID].childPid, SIGUSR2);
                munmap(clientInfo, clientInfoSize);
                fifoInfo->list[clientID][block.sendToID].fd_out = open(sendPath.c_str(), O_WRONLY);
                munmap(fifoInfo, fifoInfoSize);
            }
        }
        else
            block.sendError = true;
    }
}

void CheckFDIN(CommandBlock &block, int cnt)
{
    for (NumberPipe pipe : numberPipes)
        if (pipe.count == cnt)
        {
            block.fd_in = pipe.fd[0], block.FDIN = true;
            break;
        }
    return;
}

void minusPipeCount()
{
    for (int i = 0; i < numberPipes.size(); i++)
        numberPipes[i].count--;
    return;
}

bool ExecuteCommand(CommandBlock &block)
{
    char *arguments[4000];
    string command = block.argv[0];
    int status;
    if (command == "exit" || command == "EOF")
    {
        while (waitpid(-1, &status, WNOHANG) > 0)
            ;
        return false;
    }
    else if (command == "printenv")
        PrintEnv((block.argv)[1]);
    else if (command == "setenv")
        SetEnv((block.argv)[1], (block.argv)[2]);
    else if (command == "who")
        Who();
    else if (command == "tell")
    {
        string message = commandLine.substr(commandLine.find(block.argv[2]));
        Tell(message, stoi(block.argv[1]));
    }
    else if (command == "yell")
    {
        string message = commandLine.substr(commandLine.find(block.argv[1]));
        Yell(message);
    }
    else if (command == "name")
    {
        ChangeName(block.argv[1]);
    }
    else
    {
        signal(SIGCHLD, SIGHANDLECHILD);
        pid_t childPid;
        while ((childPid = fork()) < 0)
            while (waitpid(-1, &status, WNOHANG) > 0)
                ;
        if (childPid == 0) // child process
        {
            // cout << "command: " << command << endl
            //      << "block.FDIN: " << (block.FDIN ? "True" : "False") << "\tblock.FDOUT: " << (block.FDOUT ? "True" : "False") << endl
            //      << "block.userPipeReceive: " << (block.userPipeReceive ? "True" : "False") << "\tblock.userPipeSend: " << (block.userPipeSend ? "True" : "False") << endl
            //      << "block.recvError: " << (block.recvError ? "True" : "False") << "\tblock.sendError: " << (block.sendError ? "True" : "False") << endl;
            if (block.FDIN)
                dup2(block.fd_in, STDIN_FILENO);
            if (block.FDOUT)
            {
                dup2(block.fd_out, STDOUT_FILENO);
                if (block.pipeStatus == 2)
                    dup2(block.fd_out, STDERR_FILENO);
            }
            if (block.userPipeReceive)
            {
                if (block.recvError)
                {
                    int fdNull = open("/dev/null", O_RDWR);
                    dup2(fdNull, STDIN_FILENO);
                    close(fdNull);
                }
                else
                {
                    FIFOInfo *fifoInfo = (FIFOInfo *)mmap(NULL, fifoInfoSize, PROT_READ | PROT_WRITE, MAP_SHARED, pipeSharedFD, 0);
                    dup2(fifoInfo->list[block.receiveFromID][clientID].fd_in, STDIN_FILENO);
                    fifoInfo->list[block.receiveFromID][clientID].used = true;
                    close(fifoInfo->list[block.receiveFromID][clientID].fd_in);
                    munmap(fifoInfo, fifoInfoSize);

                    Client *clientInfo = (Client *)mmap(NULL, clientInfoSize, PROT_READ, MAP_SHARED, infoSharedFD, 0);
                    kill(clientInfo[block.receiveFromID].childPid, SIGUSR2);
                    munmap(clientInfo, clientInfoSize);
                }
            }
            if (block.userPipeSend)
            {
                if (block.sendError)
                {
                    int fdNull = open("/dev/null", O_RDWR);
                    dup2(fdNull, STDOUT_FILENO);
                    close(fdNull);
                }
                else
                {
                    FIFOInfo *fifoInfo = (FIFOInfo *)mmap(NULL, fifoInfoSize, PROT_READ, MAP_SHARED, pipeSharedFD, 0);
                    dup2(fifoInfo->list[clientID][block.sendToID].fd_out, STDOUT_FILENO);
                    close(fifoInfo->list[clientID][block.sendToID].fd_out);
                    munmap(fifoInfo, fifoInfoSize);
                }
            }

            // child closes all pipe fds
            for (NumberPipe pipe : numberPipes)
                close(pipe.fd[0]), close(pipe.fd[1]);

            // file redirection
            int index;
            for (index = 0; index < (block.argv).size(); index++)
                if ((block.argv)[index] == ">")
                    break;

            if (index != (block.argv).size())
            {
                int fd = open((block.argv).back().c_str(), O_CREAT | O_TRUNC | O_RDWR, 0644);
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            // execution
            for (int i = 0; i < index; i++)
                arguments[i] = (char *)(block.argv)[i].c_str();
            arguments[index] = NULL;

            if (execvp(command.c_str(), arguments) == -1)
                cerr << "Unknown command: [" << command << "]." << endl;
            ClearUserPipes();
            exit(EXIT_SUCCESS);
        }
        else // parent process
        {
            if (block.FDIN) // close useless pipe
            {
                for (int i = 0; i < numberPipes.size(); i++)
                    if (numberPipes[i].fd[0] == block.fd_in)
                    {
                        close(numberPipes[i].fd[0]), close(numberPipes[i].fd[1]);
                        numberPipes.erase(numberPipes.begin() + i);
                        break;
                    }
            }

            if (block.FDOUT)
                waitpid(-1, &status, WNOHANG);
            else
                waitpid(childPid, &status, 0);
            ClearUserPipes();
        }
    }
    return true;
}

void SetEnv(string name, string value)
{
    setenv(name.c_str(), value.c_str(), 1);
}

void PrintEnv(string name)
{
    if (getenv(name.c_str()) != NULL)
        cout << getenv(name.c_str()) << endl;
    else
        cout << "Environment " << name << " has no value." << endl;
}

void Login(string IP)
{
    string message = "*** User '" + GetUserName(clientID) + "' entered from " + IP + ". ***";
    BroadcastMessage(message, -1);
}

void Logout()
{
    close(STDIN_FILENO), close(STDOUT_FILENO), close(STDERR_FILENO);
    Client *clientInfo = (Client *)mmap(NULL, clientInfoSize, PROT_READ | PROT_WRITE, MAP_SHARED, infoSharedFD, 0);
    clientInfo[clientID].active = false;
    munmap(clientInfo, clientInfoSize);

    // erase user pipes
    FIFOInfo *fifoInfo = (FIFOInfo *)mmap(NULL, fifoInfoSize, PROT_READ | PROT_WRITE, MAP_SHARED, pipeSharedFD, 0);
    for (int id = 1; id < ONLINEMAX; id++)
    {
        if (fifoInfo->list[clientID][id].fd_in != -1)
            close(fifoInfo->list[clientID][id].fd_in), unlink(fifoInfo->list[clientID][id].path);
        if (fifoInfo->list[clientID][id].fd_out != -1)
            close(fifoInfo->list[clientID][id].fd_out), unlink(fifoInfo->list[clientID][id].path);
        fifoInfo->list[clientID][id].fd_in = -1, fifoInfo->list[clientID][id].fd_out = -1;
        fifoInfo->list[clientID][id].used = false;
        memset(&fifoInfo->list[clientID][id].path, 0, pathSize);

        if (fifoInfo->list[id][clientID].fd_in != -1)
            close(fifoInfo->list[id][clientID].fd_in), unlink(fifoInfo->list[id][clientID].path);
        if (fifoInfo->list[id][clientID].fd_out != -1)
            close(fifoInfo->list[id][clientID].fd_in), unlink(fifoInfo->list[id][clientID].path);
        fifoInfo->list[id][clientID].fd_in = -1, fifoInfo->list[id][clientID].fd_out = -1;
        fifoInfo->list[id][clientID].used = false;
        memset(&fifoInfo->list[id][clientID].path, 0, pathSize);
    }
    munmap(fifoInfo, fifoInfoSize);

    string message = "*** User '" + GetUserName(clientID) + "' left. ***";
    BroadcastMessage(message, -1);
}

void Who()
{
    Client *clientInfo = (Client *)mmap(NULL, clientInfoSize, PROT_READ, MAP_SHARED, infoSharedFD, 0);
    cout << "<ID>\t<nickname>\t<IP:port>\t<indicate me>" << endl;
    for (int id = 1; id < ONLINEMAX; id++)
        if (clientInfo[id].active)
            cout << id << "\t" << GetUserName(id) << "\t" << clientInfo[id].ip << ((id == clientID) ? "\t<-me" : "") << endl;
    munmap(clientInfo, clientInfoSize);
}

void ChangeName(string newName)
{
    Client *clientInfo = (Client *)mmap(NULL, clientInfoSize, PROT_READ | PROT_WRITE, MAP_SHARED, infoSharedFD, 0);
    for (int id = 1; id < ONLINEMAX; id++)
    {
        if (clientInfo[id].active && string(clientInfo[id].name) == newName && id != clientID)
        {
            cout << "*** User '" << newName << "' already exists. ***" << endl;
            munmap(clientInfo, clientInfoSize);
            return;
        }
    }
    strncpy(clientInfo[clientID].name, newName.c_str() + '\0', newName.length() + 1);
    string message = "*** User from " + string(clientInfo[clientID].ip) + " is named '" + newName + "'. ***";
    BroadcastMessage(message, -1);
    munmap(clientInfo, clientInfoSize);
}

void Yell(string msg)
{
    string message = "*** " + GetUserName(clientID) + " yelled ***: " + msg;
    BroadcastMessage(message, -1);
}

void Tell(string msg, int targetID)
{
    string message;
    if (CheckUserExist(targetID))
    {
        message = "*** " + GetUserName(clientID) + " told you ***: " + msg;
        BroadcastMessage(message, targetID);
    }
}

string GetUserName(int targetID)
{
    Client *clientInfo = (Client *)mmap(NULL, clientInfoSize, PROT_READ, MAP_SHARED, infoSharedFD, 0);
    string userName = string(clientInfo[targetID].name);
    munmap(clientInfo, clientInfoSize);
    return userName;
}

bool CheckUserExist(int targetID)
{
    Client *clientInfo = (Client *)mmap(NULL, clientInfoSize, PROT_READ, MAP_SHARED, infoSharedFD, 0);
    bool exist = true;
    if (targetID >= ONLINEMAX || targetID < 1 || (!clientInfo[targetID].active))
    {
        string message = "*** Error: user #" + to_string(targetID) + " does not exist yet. ***";
        cout << message << endl;
        exist = false;
    }
    munmap(clientInfo, clientInfoSize);
    return exist;
}

bool CheckUserPipeExist(int sourceID, int targetID)
{
    FIFOInfo *fifoInfo = (FIFOInfo *)mmap(NULL, fifoInfoSize, PROT_READ | PROT_WRITE, MAP_SHARED, pipeSharedFD, 0);
    bool result = false;
    if (fifoInfo->list[sourceID][targetID].path[0] != 0)
        result = true;
    munmap(fifoInfo, fifoInfoSize);
    return result;
}

void ClearUserPipes()
{
    FIFOInfo *fifoInfo = (FIFOInfo *)mmap(NULL, fifoInfoSize, PROT_READ | PROT_WRITE, MAP_SHARED, pipeSharedFD, 0);
    for (int id = 1; id < ONLINEMAX; id++)
    {
        if (fifoInfo->list[id][clientID].used)
        {
            close(fifoInfo->list[id][clientID].fd_in);
            fifoInfo->list[id][clientID].fd_in = -1, fifoInfo->list[id][clientID].fd_out = -1;
            fifoInfo->list[id][clientID].used = false;
            unlink(fifoInfo->list[id][clientID].path);
            memset(&fifoInfo->list[id][clientID].path, 0, pathSize);
        }
        if (fifoInfo->list[clientID][id].used)
        {
            close(fifoInfo->list[clientID][id].fd_out);
            fifoInfo->list[clientID][id].fd_in = -1, fifoInfo->list[clientID][id].fd_out = -1;
            fifoInfo->list[clientID][id].used = false;
            unlink(fifoInfo->list[clientID][id].path);
            memset(&fifoInfo->list[clientID][id].path, 0, pathSize);
        }
    }
    munmap(fifoInfo, fifoInfoSize);
}