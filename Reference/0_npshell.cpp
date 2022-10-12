#include <bits/stdc++.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#define MAX_INPUT_VALUE 15000
#define MAX_CMD_LEN 256
using namespace std;

void removeVectorSpaces(vector<string> &v, string delimiter)
{
    vector<string>::iterator iter;
    for (iter = v.begin(); iter != v.end();)
    {
        if (*iter == delimiter)
            v.erase(iter);
        else
            iter++;
    }
}

// for string delimiter
vector<string> splitString(string s, string delimiter)
{
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    string token;
    vector<string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != string::npos)
    {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back(token);
        res.push_back(delimiter); // yhsw <++
    }

    res.push_back(s.substr(pos_start));
    return res;
}

int setEnv(vector<string> &cmd)
{
    // printenv
    if (cmd[0] == "printenv")
    {
        if (cmd.size() == 2)
            if (char *p = getenv(cmd[1].c_str()))
                cout << p << endl;
        return EXIT_SUCCESS;
    }

    // setenv
    if (cmd[0] == "setenv")
    {
        if (cmd.size() == 3)
            setenv(cmd[1].c_str(), cmd[2].c_str(), 1);
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}

bool isNumber(const string &str)
{
    for (char const &c : str)
    {
        if (std::isdigit(c) == 0)
            return false;
    }
    return true;
}

static int sig_handler(int sig)
{
    int retval;
    retval = wait(NULL);
    return retval;
}

int same_pipe_ptr = 0;
pid_t pid;
int status = 0;

void kill_child(int sig)
{
    kill(pid, SIGKILL);
}

int main(int argc, char **argv)
{
    string input;
    vector<string> initial_env{"setenv", "PATH", "bin:."};
    vector<int> npCount_v;
    vector<int> npPipeID_in, npPipeID_out;
    setEnv(initial_env);

    cout << "% ";
    while (getline(cin, input))
    {

        if (input == "")
        {
            cout << "% ";
            continue;
        }

        vector<string> temp_split_cmd, space_split_cmd, split_cmd;
        temp_split_cmd = splitString(input, "|");
        for (int i = 0; i < temp_split_cmd.size(); i++)
        {
            space_split_cmd = splitString(temp_split_cmd[i], " ");
            for (int j = 0; j < space_split_cmd.size(); j++)
                split_cmd.push_back(space_split_cmd[j]);
        }
        removeVectorSpaces(split_cmd, "");  // remove multiple spaces between two parameters
        removeVectorSpaces(split_cmd, " "); // remove space in cmd that generate by first split "|"

        // 檢查split_cmd最後一格是不是 "!"
        vector<string> temp_exclamation;
        if (split_cmd[split_cmd.size() - 1].find("!") != string::npos)
        {
            temp_exclamation = splitString(split_cmd[split_cmd.size() - 1], "!");
            removeVectorSpaces(temp_exclamation, ""); // 處理多餘的vector
            split_cmd.erase(split_cmd.end());         // 去除最後一個vector (包含"!")
            split_cmd.push_back(temp_exclamation[0]);
            split_cmd.push_back(temp_exclamation[1]);
        }

        // escape bash
        if (split_cmd[0] == "exit")
            // if (input.find("exit") != string::npos)
            break;

        // if return EXIT_SUCCESS
        if (!setEnv(split_cmd))
        {
            for (int i = 0; i < npCount_v.size(); i++)
            {
                npCount_v[i]--;
            }
            cout << "% ";
            continue;
        }

        // if no pipe
        if (input.find("|") == string::npos && input.find("!") == string::npos)
        {

            // signal(SIGCHLD, sig_handler);
            signal(SIGALRM, (void (*)(int))kill_child);
            // fork cmd
            pid = fork();
            if (pid < 0)
            {
                fprintf(stderr, "fork error.\n");
                exit(EXIT_FAILURE);
            }
            else if (pid == 0)
            { // child

                // file redirection
                int argcount = 0;
                int fd = 0;
                vector<string>::iterator iter = std::find(split_cmd.begin(), split_cmd.end(), ">");
                if (iter != split_cmd.end())
                {
                    // 找出argcount, 避免>之後的參數不只一個
                    for (int i = 0; i < split_cmd.size(); i++)
                    {
                        if (split_cmd[i] == ">")
                            break;
                        argcount++;
                    }
                    iter++; // 指向>符號的下個位置(為輸出檔案名稱)
                    string temp_output = *iter;
                    fd = open(temp_output.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0644);
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }
                else
                    argcount = split_cmd.size();

                // check number pipe
                for (int i = 0; i < npCount_v.size(); i++)
                {
                    if (npCount_v[i] == 0)
                    {
                        if (dup2(npPipeID_in[i], STDIN_FILENO) < 0)
                        {
                            fprintf(stderr, "np dup create error.\n");
                            exit(EXIT_FAILURE);
                        }

                        //  重要!!!!!!
                        close(npPipeID_in[i]);
                        close(npPipeID_in[i] + 1);
                        break;
                    }
                }

                char **args = (char **)malloc((argcount + 1) * sizeof(char *));
                for (int i = 0; i < argcount; i++)
                    args[i] = (char *)split_cmd[i].c_str();
                args[argcount] = NULL;
                // char *cmd_argv[] = {strdup(split_cmd[0].c_str()), strdup(split_cmd[1].c_str()), NULL};  // parameters
                if (execvp(args[0], args) == -1)
                {
                    fprintf(stderr, "Unknown command: [%s].\n", args[0]);
                    exit(EXIT_FAILURE);
                }
                exit(EXIT_FAILURE);
            }

            for (int i = 0; i < npCount_v.size(); i++)
            {
                if (npCount_v[i] == 0)
                {
                    close(npPipeID_in[i]);
                    close(npPipeID_in[i] + 1);
                    break;
                }
            }

            while (true)
            {
                waitpid(pid, &status, 0);
                if (WIFEXITED(status) != 0)
                    break;
            }
        }
        else
        { // pipe area
            int pipe_count = 0;
            int numberPipeCount = 0;
            char buffer[MAX_INPUT_VALUE] = {0};
            bool is_numberPipe = false;
            bool is_exclamation = false; // 驚嘆號

            // whether is numberPipe
            // 如果倒數第二個vector為 | , 且最後一個vector為數字, 表示為numberPipe
            if ((split_cmd[split_cmd.size() - 2] == "|" || split_cmd[split_cmd.size() - 2] == "!") && isNumber(split_cmd[split_cmd.size() - 1]))
            {
                numberPipeCount = stoi(split_cmd[split_cmd.size() - 1]);
                is_numberPipe = true;
                if (split_cmd[split_cmd.size() - 2] == "!")
                    is_exclamation = true;
            }
            for (int i = 0; i < split_cmd.size(); i++)
                if (split_cmd[i].find("|") != string::npos || split_cmd[i].find("!") != string::npos)
                    pipe_count++;

            vector<vector<string>> v_cmd(pipe_count + 1);
            int cmd_count = 0;
            for (int i = 0; i < split_cmd.size(); i++)
            {
                if (split_cmd[i].find(">") != string::npos)
                    break;
                if (split_cmd[i].find("|") == string::npos && split_cmd[i].find("!") == string::npos)
                    v_cmd[cmd_count].push_back(split_cmd[i]);
                else
                    cmd_count++;
            }

            // one dimentional array
            // https://stackoverflow.com/questions/8389033/implementation-of-multiple-pipes-in-c
            int pipefd[2 * pipe_count];
            int fork_count = 0;
            if (!is_numberPipe)
                fork_count = pipe_count + 1;
            else
                fork_count = pipe_count;

            for (int i = 0; i < fork_count; i++)
            {
                if (is_numberPipe)
                {
                    if (pipe(pipefd + i * 2) < 0)
                    { // 一次迴圈加兩個記憶體位置, (0, 1), (2, 3)......
                        fprintf(stderr, "pipe create error.\n");
                        exit(EXIT_FAILURE);
                    }
                }
                else
                {
                    if (i < fork_count - 1)
                    {
                        if (pipe(pipefd + i * 2) < 0)
                        {
                            fprintf(stderr, "pipe create error.\n");
                            exit(EXIT_FAILURE);
                        }
                    }
                }

                // signal(SIGCHLD, sig_handler);
                signal(SIGALRM, (void (*)(int))kill_child);

                pid = fork();
                if (pid < 0)
                { // fork failed
                    fprintf(stderr, "fork create error.\n");
                    exit(EXIT_FAILURE);
                }
                else if (pid == 0)
                { // child
                    // STDIN_FILENO = 0, 接收前一個pipe的value
                    // STDOUT_FILENO = 1, 傳出當前pipe的value
                    if (i != 0)
                    { // if not first command
                        if (dup2(pipefd[(i - 1) * 2], STDIN_FILENO) < 0)
                        { // stdin 0, 2, 4......
                            fprintf(stderr, "in dup error.\n");
                            exit(EXIT_FAILURE);
                        }
                    }
                    if (i != fork_count - 1)
                    { // if not last command
                        if (dup2(pipefd[i * 2 + 1], STDOUT_FILENO) < 0)
                        { // stdout 1, 3, 5......
                            fprintf(stderr, "out dup error.\n");
                            exit(EXIT_FAILURE);
                        }
                        if (is_exclamation)
                        {
                            if (dup2(pipefd[i * 2 + 1], STDERR_FILENO) < 0)
                            {
                                fprintf(stderr, "out dup error.\n");
                                exit(EXIT_FAILURE);
                            }
                        }
                    }

                    // 第一個fork先檢查有沒有number pipe需要stdin
                    if (i == 0)
                    {
                        for (int j = 0; j < npCount_v.size(); j++)
                        {
                            if (npCount_v[j] == 0)
                            {
                                if (dup2(npPipeID_in[j], STDIN_FILENO) < 0)
                                {
                                    fprintf(stderr, "np out dup error.\n");
                                    exit(EXIT_FAILURE);
                                }

                                // 重要!!!!!!
                                close(npPipeID_in[j]);
                                close(npPipeID_in[j] + 1);
                                break;
                            }
                        }
                    }

                    same_pipe_ptr = 0;
                    // store number pipe
                    if (i == fork_count - 1 && is_numberPipe)
                    {
                        // search npCount_v value
                        for (int j = 0; j < npCount_v.size(); j++)
                        {
                            if (same_pipe_ptr != 0)
                            {
                                break;
                            }
                            if (npCount_v[j] == numberPipeCount)
                            {
                                same_pipe_ptr = 1;
                                if (dup2(npPipeID_in[j] + 1, STDOUT_FILENO) < 0)
                                {
                                    fprintf(stderr, "out dup error.\n");
                                    exit(EXIT_FAILURE);
                                }
                                if (is_exclamation)
                                {
                                    if (dup2(npPipeID_in[j] + 1, STDERR_FILENO) < 0)
                                    {
                                        fprintf(stderr, "out dup error.\n");
                                        exit(EXIT_FAILURE);
                                    }
                                }
                            }
                        }
                        if (same_pipe_ptr == 0)
                        {
                            if (dup2(pipefd[i * 2 + 1], STDOUT_FILENO) < 0)
                            {
                                fprintf(stderr, "out dup error.\n");
                                exit(EXIT_FAILURE);
                            }
                            if (is_exclamation)
                            {
                                if (dup2(pipefd[i * 2 + 1], STDERR_FILENO) < 0)
                                {
                                    fprintf(stderr, "out dup error.\n");
                                    exit(EXIT_FAILURE);
                                }
                            }
                        }
                    }

                    for (int j = 0; j < 2 * pipe_count; j++)
                        close(pipefd[j]);

                    // file redirection
                    if (i == fork_count - 1)
                    {
                        int argcount = 0;
                        int fd = 0;
                        vector<string>::iterator iter = std::find(split_cmd.begin(), split_cmd.end(), ">");
                        if (iter != split_cmd.end())
                        {
                            // 找出argcount, 避免>之後的參數不只一個
                            for (int i = 0; i < split_cmd.size(); i++)
                            {
                                if (split_cmd[i] == ">")
                                    break;
                                argcount++;
                            }
                            iter++; // 指向>符號的下個位置(為輸出檔案名稱)
                            string temp_output = *iter;
                            fd = open(temp_output.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0644);
                            dup2(fd, STDOUT_FILENO);
                            close(fd);
                        }
                        else
                            argcount = split_cmd.size();
                    }

                    // exec
                    char **args = (char **)malloc((v_cmd[i].size() + 1) * sizeof(char *));
                    for (int j = 0; j < v_cmd[i].size(); j++)
                    {
                        args[j] = (char *)v_cmd[i][j].c_str();
                    }
                    args[v_cmd[i].size()] = NULL;

                    if (execvp(args[0], args) == -1)
                    {
                        fprintf(stderr, "Unknown command: [%s].\n", args[0]);
                        exit(EXIT_FAILURE);
                    }
                    exit(EXIT_FAILURE);
                }
            }

            for (int i = 0; i < npCount_v.size(); i++)
            {
                if (npCount_v[i] == 0)
                {
                    close(npPipeID_in[i]);
                    close(npPipeID_in[i] + 1);
                    break;
                }
            }

            // store number pipe FD
            if (is_numberPipe && same_pipe_ptr == 0)
            {
                npCount_v.push_back(numberPipeCount);
                npPipeID_in.push_back(pipefd[(fork_count - 1) * 2]); // store STDIN
            }

            // parent finally close all pipe
            for (int i = 0; i < 2 * pipe_count; i++)
            {
                if (is_numberPipe && i >= 2 * pipe_count - 2)
                {
                    continue;
                }
                close(pipefd[i]);
            }

            // wait last child process, if status != 0 is normal exit
            // https://www.twblogs.net/a/5b83317c2b717766a1eb53be
            while (true)
            {
                waitpid(pid, &status, 0);
                if (WIFEXITED(status) != 0) // normal exit
                    break;
            }
        }

        for (int i = 0; i < npCount_v.size(); i++)
        {
            if (npCount_v[i] == 0)
            {
                close(npPipeID_in[i]);
                close(npPipeID_in[i] + 1);
            }
            npCount_v[i]--;
        }
        cout << "% ";
    }

    return EXIT_SUCCESS;
}