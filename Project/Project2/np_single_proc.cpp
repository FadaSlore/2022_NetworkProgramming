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
#include <sys/time.h>

// TODO: write/read buffer

#define NOPIPE 0
#define NORMALPIPE 1
#define ERRORPIPE 2

#define BUFFERSIZE 15000
#define ONLINEMAX 30

using namespace std;

typedef struct NumberPipe
{
    int count, fd[2];
    NumberPipe(int number) : count(number) {}
} NumberPipe;

typedef struct CommandBlock
{
    vector<string> argv;
    bool FDIN, FDOUT, userPipeSend, userPipeReceive;
    int pipeStatus, pipeNumber, fd_in, fd_out, sendToID, receiveFromID;
    CommandBlock() : argv(vector<string>{}), FDIN(false), FDOUT(false), userPipeSend(false), userPipeReceive(false), pipeStatus(0), pipeNumber(0), fd_in(-1), fd_out(-1), sendToID(-1), receiveFromID(-1) {}
    CommandBlock(int status) : argv(vector<string>{}), FDIN(false), FDOUT(false), userPipeSend(false), userPipeReceive(false), pipeStatus(status), pipeNumber(0), fd_in(-1), fd_out(-1), sendToID(-1), receiveFromID(-1) {}
    CommandBlock(int status, vector<string> arguments) : argv(arguments), FDIN(false), FDOUT(false), userPipeSend(false), userPipeReceive(false), pipeStatus(status), pipeNumber(0), fd_in(-1), fd_out(-1), sendToID(-1), receiveFromID(-1) {}
    CommandBlock(int status, vector<string> arguments, bool send, bool recv, int sendID, int recvID) : argv(arguments), FDIN(false), FDOUT(false), userPipeSend(send), userPipeReceive(recv), pipeStatus(status), pipeNumber(0), fd_in(-1), fd_out(-1), sendToID(sendID), receiveFromID(recvID) {}
    CommandBlock(int status, vector<string> arguments, int count) : argv(arguments), FDIN(false), FDOUT(false), userPipeSend(false), userPipeReceive(false), pipeStatus(status), pipeNumber(count), fd_in(-1), fd_out(-1), sendToID(-1), receiveFromID(-1) {}
    CommandBlock(int status, vector<string> arguments, bool send, bool recv, int sendID, int recvID, int count) : argv(arguments), FDIN(false), FDOUT(false), userPipeSend(send), userPipeReceive(recv), pipeStatus(status), pipeNumber(count), fd_in(-1), fd_out(-1), sendToID(sendID), receiveFromID(recvID) {}
} CommandBlock;

typedef struct UserPipe
{
    int fd[2], sendID, receiveID;
    UserPipe(int send, int receive) : sendID(send), receiveID(receive) {}
} UserPipe;

typedef struct Client
{
    bool active;
    int fd;
    string ip, name; // initialize name is empty string
    vector<NumberPipe> numberPipes;
    map<string, string> environment;
} Client;

/* demo */
// typedef struct Group
// {
//     string groupName;
//     vector<int> userIDs;
// } Group;

// global variable
string commandLine;
vector<string> commands;
vector<CommandBlock> commandBlocks;
/* demo */
// vector<Group> groups;

vector<UserPipe> userPipes;
Client clientInfo[ONLINEMAX + 1];

int serverSocket;
fd_set readFDs, activeFDs;
const int fdNull = open("/dev/null", O_RDWR);

// function
void initVariables();
int CreateSocket(int port);
int GetClientID();
void BroadcastMessage(string message, int targetID);
void NewClient(int ID, int FD, string IP);
void DeleteClient(int clientID);
// Shell Functions
void Shell(int clientID);
void SplitCommand(string command);
void TransferToCommandBlock(int clientID);
void CreatePipe(CommandBlock &block, int clientID);
void CheckFDIN(CommandBlock &block, int cnt, int clientID);
void minusPipeCount(int clientID);
bool ExecuteCommand(CommandBlock &block, int clientID);
void SetEnv(string name, string value, int clientID);
string PrintEnv(string name, int clientID);
void Login(int clientID);
void Logout(int clientID);
void Who(int clientFD);
void ChangeName(string newName, int clientID);
void Yell(string msg, int clientID);
void Tell(string msg, int clientID, int targetID);
int MaxFD();
string GetUserName(int clientID);
bool CheckUserExist(int targetID, int clientID);
int CheckUserPipeExist(int sourceID, int targetID);

/* demo */
// void MakeGroup(string groupName, vector<int> users);
// void GroupTell(int clientID, string groupName, string msg);

void initVariables()
{
    clearenv();
    commandLine.clear();
    commands.clear(), commandBlocks.clear();
    userPipes.clear();
    for (int id = 0; id <= ONLINEMAX; id++)
        clientInfo[id].active = false, clientInfo[id].numberPipes.clear();
}

int main(int argc, char *argv[])
{
    if (argc != 2)
        return 0;
    initVariables();
    int port = atoi(argv[1]);
    serverSocket = CreateSocket(port);
    if (listen(serverSocket, 5) < 0)
        cerr << "Listen fail.\n", exit(EXIT_FAILURE);

    FD_ZERO(&activeFDs);
    FD_ZERO(&readFDs);
    FD_SET(serverSocket, &activeFDs);
    string welcomeMessage = "****************************************\n** Welcome to the information server. **\n****************************************\n";
    while (true)
    {
        int maximumFDs = MaxFD();
        memcpy(&readFDs, &activeFDs, sizeof(readFDs));
        int errorNumber, selectStatus;
        while ((selectStatus = select(maximumFDs, &readFDs, NULL, NULL, (struct timeval *)0)) < 0)
            if (errno != EINTR)
                break;
        if (selectStatus < 0)
            cerr << "select fail.\n";

        if (FD_ISSET(serverSocket, &readFDs))
        {
            struct sockaddr_in clientAddress;
            int length = sizeof(clientAddress), clientSocket, clientID;
            if ((clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddress, (socklen_t *)&length)) < 0)
                cerr << "accept fail.\n", exit(EXIT_FAILURE);
            if ((clientID = GetClientID()) == -1)
            {
                cerr << "Client max.\n";
                continue;
            }
            FD_SET(clientSocket, &activeFDs);
            // write(clientSocket, welcomeMessage.c_str(), welcomeMessage.length());
            string clientIP = string(inet_ntoa(clientAddress.sin_addr)) + ":" + to_string(ntohs(clientAddress.sin_port));
            NewClient(clientID, clientSocket, clientIP);
            BroadcastMessage(welcomeMessage, clientID);
            Login(clientID);
            BroadcastMessage("% ", clientID);
        }

        for (int id = 1; id <= ONLINEMAX; id++)
            if (clientInfo[id].active)
                Shell(id);
    }
    close(fdNull);
    exit(0);
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
        cerr << "Socket create fail.\n", exit(EXIT_FAILURE);
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) == -1)
        cerr << "Set socket fail.\n", exit(EXIT_FAILURE);

    if (bind(serverSocket, (struct sockaddr *)&socketAddress, sizeof(socketAddress)) < 0)
        cerr << "Socket bind fail.\n", exit(EXIT_FAILURE);
    return serverSocket;
}

int GetClientID()
{
    for (int id = 1; id <= ONLINEMAX; id++)
        if (!clientInfo[id].active)
            return id;
    return -1;
}

void BroadcastMessage(string message, int targetID)
{
    int maximumFDs = MaxFD();
    if (targetID == -1)
    {
        for (int fd = 0; fd < maximumFDs; fd++)
            if (fd != serverSocket && FD_ISSET(fd, &activeFDs))
                write(fd, message.c_str(), message.length());
    }
    else if (FD_ISSET(clientInfo[targetID].fd, &activeFDs))
        write(clientInfo[targetID].fd, message.c_str(), message.length());
}

void NewClient(int ID, int FD, string IP)
{
    clientInfo[ID].active = true;
    clientInfo[ID].fd = FD, clientInfo[ID].ip = IP;
    clientInfo[ID].numberPipes.clear(), clientInfo[ID].environment.clear();
    clientInfo[ID].name = "", clientInfo[ID].environment["PATH"] = "bin:.";
}

void DeleteClient(int clientID)
{
    clientInfo[clientID].active = false;
    clientInfo[clientID].name.clear(), clientInfo[clientID].ip.clear();
    clientInfo[clientID].environment.clear();
    for (NumberPipe pipe : clientInfo[clientID].numberPipes)
        close(pipe.fd[0]), close(pipe.fd[1]);
    clientInfo[clientID].numberPipes.clear();
    for (int index = userPipes.size() - 1; index >= 0; index--)
    {
        if (userPipes[index].sendID == clientID || userPipes[index].receiveID == clientID)
        {
            close(userPipes[index].fd[0]), close(userPipes[index].fd[1]);
            userPipes.erase(userPipes.begin() + index);
        }
    }
    /* demo */
    // for(int index = 0;index<groups.size();index++){
    //     for(int i=0;i<groups[index].userIDs.size();i++){
    //         cout<<"groupUserID: "<<groups[index].userIDs[i]<<"\tclientID: "<<clientID<<endl;
    //         if(groups[index].userIDs[i] == clientID){
    //             cout<<"delete ID: "<<groups[index].userIDs[i]<<endl;
    //             groups[index].userIDs.erase(groups[index].userIDs.begin() + i);
    //             break;
    //         }
    //     }
    // }
}

// Shell Functions
void Shell(int clientID)
{
    // initialize: set path
    clearenv();
    for (map<string, string>::iterator it = clientInfo[clientID].environment.begin(); it != clientInfo[clientID].environment.end(); it++)
        setenv(it->first.c_str(), it->second.c_str(), 1);

    commandLine.clear();
    char readBuffer[BUFFERSIZE];
    if (FD_ISSET(clientInfo[clientID].fd, &readFDs))
    {
        bzero(readBuffer, sizeof(readBuffer));
        int readBytes = read(clientInfo[clientID].fd, readBuffer, sizeof(readBuffer));
        if (readBytes < 0)
        {
            cerr << "[ERROR]: #" << clientID << " read error.\n";
            return;
        }
        commandLine = readBuffer;
        commandLine.erase(remove(commandLine.begin(), commandLine.end(), '\n'), commandLine.end());
        commandLine.erase(remove(commandLine.begin(), commandLine.end(), '\r'), commandLine.end());
    }
    else
        return;
    if (commandLine.length() == 0)
        return;

    bool firstCommand = true, lastNumbered = false;
    commands.clear();
    commandBlocks.clear();
    SplitCommand(commandLine + " ");
    TransferToCommandBlock(clientID);

    while (!commandBlocks.empty())
    {
        CreatePipe(commandBlocks[0], clientID);
        CheckFDIN(commandBlocks[0], (firstCommand) ? 0 : -1, clientID);

        if (!ExecuteCommand(commandBlocks[0], clientID))
            return;

        if (firstCommand)
            firstCommand = false;

        if (commandBlocks[0].pipeNumber > 0 && (commandBlocks[0].pipeStatus <= ERRORPIPE))
        {
            minusPipeCount(clientID);
            firstCommand = true;
            if (commandBlocks.size() == 1)
                lastNumbered = true;
        }
        commandBlocks.erase(commandBlocks.begin());
    }
    if (!lastNumbered)
        minusPipeCount(clientID);

    fflush(stdout);
    BroadcastMessage("% ", clientID);
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

void TransferToCommandBlock(int clientID)
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

void CreatePipe(CommandBlock &block, int clientID)
{
    if (block.pipeStatus == NORMALPIPE || block.pipeStatus == ERRORPIPE)
    {
        int cnt = block.pipeNumber;
        bool merge = false;
        NumberPipe newPipe(cnt);
        if (cnt > 0)
            for (int i = 0; i < clientInfo[clientID].numberPipes.size(); i++)
                if (clientInfo[clientID].numberPipes[i].count == cnt)
                {
                    block.fd_out = clientInfo[clientID].numberPipes[i].fd[1], merge = true;
                    break;
                }

        if (!merge)
        {
            pipe(newPipe.fd);
            block.fd_out = newPipe.fd[1];
            clientInfo[clientID].numberPipes.push_back(newPipe);
        }
        block.FDOUT = true;
    }

    if (block.userPipeReceive)
    {
        if (CheckUserExist(block.receiveFromID, clientID))
        {
            int pipeIndex = CheckUserPipeExist(block.receiveFromID, clientID);
            if (pipeIndex == -1)
            {
                string message = "*** Error: the pipe #" + to_string(block.receiveFromID) + "->#" + to_string(clientID) + " does not exist yet. ***\n";
                BroadcastMessage(message, clientID);
                block.FDIN = true, block.fd_in = fdNull;
            }
            else
            {
                string message = "*** " + GetUserName(clientID) + " (#" + to_string(clientID) + ") just received from " + GetUserName(block.receiveFromID) + " (#" + to_string(block.receiveFromID) + ") by '" + commandLine + "' ***\n";
                BroadcastMessage(message, -1);
                block.FDIN = true, block.fd_in = userPipes[pipeIndex].fd[0];
            }
        }
        else
            block.FDIN = true, block.fd_in = fdNull;
    }

    if (block.userPipeSend)
    {
        if (CheckUserExist(block.sendToID, clientID))
        {

            int pipeIndex = CheckUserPipeExist(clientID, block.sendToID);
            if (pipeIndex == -1)
            {
                string message = "*** " + GetUserName(clientID) + " (#" + to_string(clientID) + ") just piped '" + commandLine + "' to " + GetUserName(block.sendToID) + " (#" + to_string(block.sendToID) + ") ***\n";
                BroadcastMessage(message, -1);
                UserPipe newUserPipe(clientID, block.sendToID);
                pipe(newUserPipe.fd);
                userPipes.push_back(newUserPipe);
                block.FDOUT = true, block.fd_out = newUserPipe.fd[1];
            }
            else
            {
                string message = "*** Error: the pipe #" + to_string(clientID) + "->#" + to_string(block.sendToID) + " already exists. ***\n";
                BroadcastMessage(message, clientID);
                block.FDOUT = true, block.FDOUT = fdNull;
            }
        }
        else
            block.FDOUT = true, block.FDOUT = fdNull;
    }
}

void CheckFDIN(CommandBlock &block, int cnt, int clientID)
{
    for (NumberPipe pipe : clientInfo[clientID].numberPipes)
        if (pipe.count == cnt)
        {
            block.fd_in = pipe.fd[0], block.FDIN = true;
            break;
        }
    return;
}

void minusPipeCount(int clientID)
{
    for (int i = 0; i < clientInfo[clientID].numberPipes.size(); i++)
        clientInfo[clientID].numberPipes[i].count--;
    return;
}

bool ExecuteCommand(CommandBlock &block, int clientID)
{
    char *arguments[4000];
    string command = block.argv[0];
    int status;
    if (command == "exit" || command == "EOF")
    {
        FD_CLR(clientInfo[clientID].fd, &activeFDs);
        close(clientInfo[clientID].fd);
        Logout(clientID);
        while (waitpid(-1, &status, WNOHANG) > 0)
            ;
        return false;
    }
    else if (command == "printenv")
        BroadcastMessage(PrintEnv((block.argv)[1], clientID), clientID);
    else if (command == "setenv")
        SetEnv((block.argv)[1], (block.argv)[2], clientID);
    else if (command == "who")
        Who(clientID);
    else if (command == "tell")
    {
        string message = commandLine.substr(commandLine.find(block.argv[2]));
        Tell(message, clientID, stoi(block.argv[1]));
    }
    else if (command == "yell")
    {
        string message = commandLine.substr(commandLine.find(block.argv[1]));
        Yell(message, clientID);
    }
    else if (command == "name")
    {
        ChangeName(block.argv[1], clientID);
    }
    /* demo */
    // else if(command == "group"){
    //     vector<int> userIDs;
    //     for(int i=2;i<block.argv.size();i++) {
    //         if(clientInfo[clientID].active){
    //             userIDs.push_back(stoi(block.argv[i]));
    //         }
    //     }
    //     MakeGroup(block.argv[1], userIDs);
    // }
    // else if(command == "grouptell"){
    //     string message = commandLine.substr(commandLine.find(block.argv[2]));
    //     GroupTell(clientID, block.argv[1], message);
    // }
    else
    {
        pid_t childPid;
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
                else
                    dup2(clientInfo[clientID].fd, STDERR_FILENO);
            }
            else
                dup2(clientInfo[clientID].fd, STDOUT_FILENO), dup2(clientInfo[clientID].fd, STDERR_FILENO);

            // child closes all pipe fds
            for (NumberPipe pipe : clientInfo[clientID].numberPipes)
                close(pipe.fd[0]), close(pipe.fd[1]);
            for (UserPipe pipe : userPipes)
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

            exit(EXIT_SUCCESS);
        }
        else // parent process
        {
            if (block.FDIN) // close useless pipe
            {
                for (int i = 0; i < clientInfo[clientID].numberPipes.size(); i++)
                    if (clientInfo[clientID].numberPipes[i].fd[0] == block.fd_in)
                    {
                        close(clientInfo[clientID].numberPipes[i].fd[0]), close(clientInfo[clientID].numberPipes[i].fd[1]);
                        clientInfo[clientID].numberPipes.erase(clientInfo[clientID].numberPipes.begin() + i);
                        break;
                    }
                for (int i = 0; i < userPipes.size(); i++)
                {
                    if (userPipes[i].fd[0] == block.fd_in)
                    {
                        close(userPipes[i].fd[0]), close(userPipes[i].fd[1]);
                        userPipes.erase(userPipes.begin() + i);
                        break;
                    }
                }
            }

            if (block.FDOUT)
            {
                waitpid(-1, &status, WNOHANG);
            }
            else
            {
                waitpid(childPid, &status, 0);
            }
        }
    }
    return true;
}

void SetEnv(string name, string value, int clientID)
{
    clientInfo[clientID].environment[name] = value;
    setenv(name.c_str(), value.c_str(), 1);
}

string PrintEnv(string name, int clientID)
{
    if (getenv(name.c_str()) != NULL)
        return string(getenv(name.c_str())) + "\n";
    return "Environment " + name + " has no value.\n";
}

void Login(int clientID)
{
    string message = "*** User '" + GetUserName(clientID) + "' entered from " + clientInfo[clientID].ip + ". ***\n";
    BroadcastMessage(message, -1);
}

void Logout(int clientID)
{
    string message = "*** User '" + GetUserName(clientID) + "' left. ***\n";
    BroadcastMessage(message, -1);
    DeleteClient(clientID);
}

void Who(int clientID)
{
    string message = "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
    for (int id = 1; id <= ONLINEMAX; id++)
        if (clientInfo[id].active)
            message += to_string(id) + "\t" + GetUserName(id) + "\t" + clientInfo[id].ip + ((id == clientID) ? "\t<-me\n" : "\n");
    BroadcastMessage(message, clientID);
}

void ChangeName(string newName, int clientID)
{
    string message = "";
    for (int id = 1; id <= ONLINEMAX; id++)
    {
        if (clientInfo[id].active && clientInfo[id].name == newName && id != clientID)
        {
            message = "*** User '" + newName + "' already exists. ***\n";
            BroadcastMessage(message, clientID);
            return;
        }
    }
    clientInfo[clientID].name = newName;
    message = "*** User from " + clientInfo[clientID].ip + " is named '" + newName + "'. ***\n";
    BroadcastMessage(message, -1);
}

void Yell(string msg, int clientID)
{
    string message = "*** " + GetUserName(clientID) + " yelled ***: " + msg + "\n";
    BroadcastMessage(message, -1);
}

void Tell(string msg, int clientID, int targetID)
{
    string message;
    if (CheckUserExist(targetID, clientID))
    {
        message = "*** " + GetUserName(clientID) + " told you ***: " + msg + "\n";
        BroadcastMessage(message, targetID);
    }
}

/* demo */
// void MakeGroup(string groupName, vector<int> users){
//     cout<<"groupName: "<<groupName<<endl;
//     Group newGroup;
//     newGroup.groupName = groupName;
//     newGroup.userIDs.clear();
//     for(int i=0;i<users.size();i++){
//         if(clientInfo[users[i]].active)
//             newGroup.userIDs.push_back(users[i]);
//     }
//     groups.push_back(newGroup);
// }

// void GroupTell(int clientID, string groupName, string msg){
//     for(Group group: groups){
//         if(group.groupName == groupName){
//             int index;
//             for(index = 0;index<group.userIDs.size();index++){
//                 if(group.userIDs[index] == clientID) break;
//             }
//             if(index == group.userIDs.size()){
//                 string message = "Error: You are not in " + groupName+"\n";
//                 BroadcastMessage(message, clientID);
//                 return;
//             }
//             for(index = 0;index<group.userIDs.size();index++){
//                 string message = "*** " + GetUserName(clientID) + " tell " + groupName + "***: " + msg + "\n";
//                 BroadcastMessage(message, group.userIDs[index]);
//             }
//         }
//     }
// }

int MaxFD()
{
    int maxValue = serverSocket;
    for (int id = 1; id <= ONLINEMAX; id++)
        if (clientInfo[id].fd > maxValue)
            maxValue = clientInfo[id].fd;
    return maxValue + 1;
}

string GetUserName(int clientID)
{
    return (clientInfo[clientID].name.empty() ? "(no name)" : clientInfo[clientID].name);
}

bool CheckUserExist(int targetID, int clientID)
{
    if (targetID > ONLINEMAX || targetID < 1 || (!clientInfo[targetID].active))
    {
        string message = "*** Error: user #" + to_string(targetID) + " does not exist yet. ***\n";
        BroadcastMessage(message, clientID);
        return false;
    }
    return true;
}

int CheckUserPipeExist(int sourceID, int targetID)
{
    for (int i = 0; i < userPipes.size(); i++)
        if (userPipes[i].sendID == sourceID && userPipes[i].receiveID == targetID)
            return i;
    return -1;
}