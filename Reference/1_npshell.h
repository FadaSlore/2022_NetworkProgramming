#include <iostream>
#include <string.h>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <regex>

using namespace std;

typedef struct cmdBlock
{
    string cmd;
    vector<string> argv;
    bool has_fd_in, has_fd_out;
    int fd_in, fd_out;
    int pipeType; // 0: no pipe, 1: pipe, 2: error pipe
} cmdBlock;

typedef struct Pipe
{
    int count;
    int fd[2];
} Pipe;

void init_shell();

void parsePipe(string cmdLine, vector<cmdBlock> &cmdBlocks);

void parseCmd(cmdBlock &cmdBlock);

void checkPipeType(cmdBlock &cmdBlock);

void exec_cmd(cmdBlock &cmdBlock);

void vec2arr(vector<string> &vec, char *arr[], int index);

int findIndex(vector<string> &vec, string str);