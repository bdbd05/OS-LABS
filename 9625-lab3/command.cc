#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <glob.h>
#include <wordexp.h>
#include <errno.h>
#include <vector>
#include <string>
#include <cctype>
#include <iostream>
#include <sstream>
#include "command.h"
#include "tokenizer.h"

// benhot el full path 3lshan child.log yefdal fel folder elly bada2na menno el shell 7atta law 3amalna cd le makan tany
std::string logPath;

// bensheel el double quotes men el input 3lshan execvp law shafetha haye3teberha goz2 men esm el file aw el argument
std::string stripQuotes(const std::string &word)
{
    if (word.size() >= 2 && word[0] == '"' && word[word.size() - 1] == '"')
    {
        return word.substr(1, word.size() - 2);
    }
    return word;
}

// bn check el syntax el awal abl parse() 3shan law feeh 7aga na2sa zay "ls >" mn gheer filename mybawazsh el table
bool checkSyntax(std::vector<Token> &tokens)
{
    for (int i = 0; i < (int)tokens.size(); i++)
    {
        TokenType t = tokens[i].type;

        bool isRedirect = (t == TOKEN_REDIRECT || t == TOKEN_APPEND || t == TOKEN_INPUT ||
                           t == TOKEN_ERROR || t == TOKEN_REDIRECT_AND_ERROR);

        if (isRedirect && tokens[i + 1].type != TOKEN_ARGUMENT && tokens[i + 1].type != TOKEN_COMMAND)
        {
            fprintf(stderr, "syntax error: '%s' needs a filename after it\n", tokens[i].value.c_str());
            return false;
        }

        if (t == TOKEN_PIPE)
        {
            if (i == 0 || tokens[i + 1].type == TOKEN_EOF || tokens[i + 1].type == TOKEN_PIPE)
            {
                fprintf(stderr, "syntax error: '|' needs a command on both sides\n");
                return false;
            }
        }
    }
    return true;
}

// glob() betfok el * wel ? le asamy el files ely bet-match, w lw mal2ash 7aga by7ot el kelma zy ma hya
void insertGlobbed(SimpleCommand *cmd, const std::string &word)
{
    if (word.find_first_of("*?[") == std::string::npos)
    {
        cmd->insertArgument(strdup(word.c_str()));
        return;
    }

    glob_t results;
    if (glob(word.c_str(), 0, NULL, &results) == 0)
    {
        for (size_t k = 0; k < results.gl_pathc; k++)
        {
            cmd->insertArgument(strdup(results.gl_pathv[k]));
        }
    }
    else
    {
        cmd->insertArgument(strdup(word.c_str()));
    }
    globfree(&results);
}

void parse(std::vector<Token> &tokens)
{
    if (!checkSyntax(tokens))
        return;

    Command &currentCommand = Command::_currentCommand;
    SimpleCommand *currentSimpleCommand = new SimpleCommand();

    for (int i = 0; i < (int)tokens.size(); i++)
    {
        if (tokens[i].type == TOKEN_COMMAND || tokens[i].type == TOKEN_ARGUMENT)
        {
            std::string word = tokens[i].value;

            // el tokenizer bey2assem bel spaces fa "Hello, World!" 3la kaza token, hena bnerga3 nlz2 l7d ma nla2y el quote ely fel akherr
            if (word[0] == '"' && (word.size() < 2 || word[word.size() - 1] != '"'))
            {
                while (i + 1 < (int)tokens.size() && tokens[i + 1].type != TOKEN_EOF)
                {
                    i++;
                    word += " " + tokens[i].value;
                    if (tokens[i].value[tokens[i].value.size() - 1] == '"')
                        break;
                }
            }

            insertGlobbed(currentSimpleCommand, stripQuotes(word));
        }
        else if (tokens[i].type == TOKEN_REDIRECT)
        {
            i++;
            currentCommand._outFile = strdup(tokens[i].value.c_str());
        }
        else if (tokens[i].type == TOKEN_INPUT)
        {
            i++;
            currentCommand._inputFile = strdup(tokens[i].value.c_str());
        }
        else if (tokens[i].type == TOKEN_APPEND)
        {
            i++;
            currentCommand._outFile = strdup(tokens[i].value.c_str());
            currentCommand._append = 1;
        }
        else if (tokens[i].type == TOKEN_ERROR)
        {
            i++;
            currentCommand._errFile = strdup(tokens[i].value.c_str());
        }
        else if (tokens[i].type == TOKEN_REDIRECT_AND_ERROR)
        {
            i++;
            currentCommand._outFile = strdup(tokens[i].value.c_str());
            currentCommand._errFile = strdup(tokens[i].value.c_str());
            currentCommand._out_error = 1;
            currentCommand._append = 1;
        }
        else if (tokens[i].type == TOKEN_PIPE)
        {
            // save el currentSimpleCommand w nbtdy wa7ed gedeed lel command ely ba3d el pipe
            currentCommand.insertSimpleCommand(currentSimpleCommand);
            currentSimpleCommand = new SimpleCommand();
        }
        else if (tokens[i].type == TOKEN_BACKGROUND)
        {
            currentCommand._background = 1;
        }
    }

    // a5er command no pipe b3do fa nseebo fel table hena ba3d ma el loop te5las
    currentCommand.insertSimpleCommand(currentSimpleCommand);
}

SimpleCommand::SimpleCommand()
{
    _numberOfAvailableArguments = 5;
    _numberOfArguments = 0;
    _arguments = (char **)malloc(_numberOfAvailableArguments * sizeof(char *));
}

void SimpleCommand::insertArgument(char *argument)
{
    if (_numberOfAvailableArguments == _numberOfArguments + 1)
    {
        _numberOfAvailableArguments *= 2;
        _arguments = (char **)realloc(_arguments,
                                      _numberOfAvailableArguments * sizeof(char *));
    }

    _arguments[_numberOfArguments] = argument;
    _arguments[_numberOfArguments + 1] = NULL;

    _numberOfArguments++;
}

Command::Command()
{
    _numberOfAvailableSimpleCommands = 1;
    _simpleCommands = (SimpleCommand **)
        malloc(_numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));

    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _out_error = 0;
    _background = 0;
    _append = 0;
}

void Command::insertSimpleCommand(SimpleCommand *simpleCommand)
{
    if (_numberOfAvailableSimpleCommands == _numberOfSimpleCommands)
    {
        _numberOfAvailableSimpleCommands *= 2;
        _simpleCommands = (SimpleCommand **)realloc(_simpleCommands,
                                                    _numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));
    }

    _simpleCommands[_numberOfSimpleCommands] = simpleCommand;
    _numberOfSimpleCommands++;
}

void Command::clear()
{
    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            free(_simpleCommands[i]->_arguments[j]);
        }

        free(_simpleCommands[i]->_arguments);
        free(_simpleCommands[i]);
    }

    if (_outFile)
    {
        free(_outFile);
    }

    if (_inputFile)
    {
        free(_inputFile);
    }

    if (_errFile)
    {
        free(_errFile);
    }

    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _background = 0;
    _out_error = 0;
    _append = 0;
}

void Command::print()
{
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        printf("  %-3d ", i);
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            printf("\"%s\" \t", _simpleCommands[i]->_arguments[j]);
        }
        printf("\n");
    }

    printf("\n\n");
    printf("  Output       Input        Error        Err&Out       Background\n");
    printf("  ------------ ------------ ------------ ------------ ------------\n");
    printf("  %-12s %-12s %-12s %-12s %-12s\n", _outFile ? _outFile : "default",
           _inputFile ? _inputFile : "default", _errFile ? _errFile : "default", _out_error == 1 ? _errFile : "default", _background ? "YES" : "NO");
    printf("\n\n");
}

void Command::execute()
{
    if (_numberOfSimpleCommands == 0 || _simpleCommands[0]->_numberOfArguments == 0)
    {
        clear();
        return;
    }

    // cd lazem t execute fy el parent nfso mn 8eir fork(), 3lshan lw fy el child el folder bta3 el shell el asly msh hayet8ayar
    if (strcmp(_simpleCommands[0]->_arguments[0], "cd") == 0)
    {
        const char *dir = (_simpleCommands[0]->_numberOfArguments > 1)
                              ? _simpleCommands[0]->_arguments[1]
                              : getenv("HOME");
        if (dir && chdir(dir) != 0)
        {
            perror("cd");
        }
        clear();
        return;
    }

    print();

    int infd = -1;
    if (_inputFile)
    {
        infd = open(_inputFile, O_RDONLY);
        if (infd < 0)
        {
            perror(_inputFile);
            clear();
            return;
        }
    }

    std::vector<int> pids;

    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        bool last = (i == _numberOfSimpleCommands - 1);
        int fdpipe[2] = {-1, -1};

        if (!last)
        {
            if (pipe(fdpipe) == -1)
            {
                perror("pipe");
                break;
            }
        }

        int pid = fork();

        if (pid == 0)
        {
            // dup2 lazem ykon fy el child, lw 3mlnaha fel parent el shell nafso hayet8ayar el output bta3o
            // return SIGINT default hena 3shan law dosna Ctrl+C yewa2af el command el sha8al msh el shell
            signal(SIGINT, SIG_DFL);

            if (infd != -1)
            {
                dup2(infd, 0);
                close(infd);
            }

            if (!last)
            {
                dup2(fdpipe[1], 1);
                close(fdpipe[0]);
                close(fdpipe[1]);
            }
            else if (_outFile)
            {
                int flags = O_WRONLY | O_CREAT | (_append ? O_APPEND : O_TRUNC);
                int fd = open(_outFile, flags, 0644);
                if (fd < 0)
                {
                    perror(_outFile);
                    exit(1);
                }
                dup2(fd, 1);
                if (_out_error)
                {
                    dup2(fd, 2);
                }
                close(fd);
            }

            if (_errFile && !_out_error)
            {
                int fd = open(_errFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0)
                {
                    perror(_errFile);
                    exit(1);
                }
                dup2(fd, 2);
                close(fd);
            }

            execvp(_simpleCommands[i]->_arguments[0], _simpleCommands[i]->_arguments);
            fprintf(stderr, "myshell: %s: command not found\n", _simpleCommands[i]->_arguments[0]);
            exit(1);
        }

        pids.push_back(pid);

        if (infd != -1)
        {
            close(infd);
        }

        // el parent lazem ye2fel el write end bta3 el pipe,lw sabo maftoo7 el command ely ba3do hyfdal mstanny input
        if (!last)
        {
            close(fdpipe[1]);
            infd = fdpipe[0];
        }
    }

    // lw mafeesh & (_background == 0) el shell beyestanna el children t5las, lw background beysebhom sha8aleen w yerga3 lel prompt
    if (!_background)
    {
        for (size_t i = 0; i < pids.size(); i++)
        {
            if (waitpid(pids[i], NULL, 0) > 0)
            {
                FILE *f = fopen(logPath.c_str(), "a");
                if (f)
                {
                    fprintf(f, "A child process terminated\n");
                    fclose(f);
                }
            }
        }
    }

    clear();
}

void showPrompt()
{
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        strcpy(cwd, "?");
    }
    printf("\033[36mmyshell:%s> \033[0m", cwd);
    fflush(stdout);
}

void Command::prompt()
{
    std::string input;
    showPrompt();

    while (std::getline(std::cin, input))
    {
        if (input == "exit")
        {
            printf("Good bye!!\n");
            break;
        }

        if (!input.empty())
        {
            std::vector<Token> tokens = tokenize(input);
            parse(tokens);
            _currentCommand.execute();
        }

        showPrompt();
    }
}

Command Command::_currentCommand;
SimpleCommand *Command::_currentSimpleCommand;

// loop b WNOHANG 3lshan momken kaza child ye5laso m3 b3d, w b-save errno for the handler
void logChild(int sig)
{
    (void)sig;
    int savedErrno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0)
    {
        FILE *f = fopen(logPath.c_str(), "a");
        if (f)
        {
            fprintf(f, "A child process terminated\n");
            fclose(f);
        }
    }

    errno = savedErrno;
}

int main()
{
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
        logPath = std::string(cwd) + "/child.log";
    else
        logPath = "child.log";

    // el shell by ignore Ctrl+C 3shan mye2felsh, bs el children bterga3 SIG_DFL fo2 f execute() 3shan ne2dar nwa2afhom
    signal(SIGINT, SIG_IGN);
    signal(SIGCHLD, logChild);

    Command::_currentCommand.prompt();
    return 0;
}