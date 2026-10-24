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

// Full path to child.log, set once in main() to the folder where the shell was started
std::string logPath;

// Removes the quote marks from a word: "hello" -> hello
std::string stripQuotes(const std::string &word)
{
    if (word.size() >= 2 && word[0] == '"' && word[word.size() - 1] == '"')
    {
        return word.substr(1, word.size() - 2);
    }
    return word;
}

// Catches bad input before parse() builds anything.
// Every token list ends with TOKEN_EOF, so tokens[i + 1] is always safe.
bool checkSyntax(std::vector<Token> &tokens)
{
    for (int i = 0; i < (int)tokens.size(); i++)
    {
        TokenType t = tokens[i].type;

        bool isRedirect = (t == TOKEN_REDIRECT || t == TOKEN_APPEND || t == TOKEN_INPUT ||
                           t == TOKEN_ERROR || t == TOKEN_REDIRECT_AND_ERROR);

        // a redirect needs a filename after it
        if (isRedirect && tokens[i + 1].type != TOKEN_ARGUMENT && tokens[i + 1].type != TOKEN_COMMAND)
        {
            fprintf(stderr, "syntax error: '%s' needs a filename after it\n", tokens[i].value.c_str());
            return false;
        }

        // a pipe needs a command before it and after it
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

// Adds a word as an argument. If it has * or ?, glob() replaces it with the matching files.
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
        // no match: keep the word exactly as typed
        cmd->insertArgument(strdup(word.c_str()));
    }
    globfree(&results);
}

// Turns the token list into the shared command table. It does not run anything.
void parse(std::vector<Token> &tokens)
{
    if (!checkSyntax(tokens))
        return;

    // reference, not a copy: we fill the real table that prompt() will run
    Command &currentCommand = Command::_currentCommand;
    SimpleCommand *currentSimpleCommand = new SimpleCommand();

    for (int i = 0; i < (int)tokens.size(); i++)
    {
        if (tokens[i].type == TOKEN_COMMAND || tokens[i].type == TOKEN_ARGUMENT)
        {
            std::string word = tokens[i].value;

            // "hello world" arrives as two tokens, so join them until one ends with "
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
            i++; // the next token is the filename
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
            currentCommand._append = 1; // append, not overwrite
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
            currentCommand._out_error = 1; // both go to the same file
            currentCommand._append = 1;
        }
        else if (tokens[i].type == TOKEN_PIPE)
        {
            // finish this command, start a new empty one for after the |
            currentCommand.insertSimpleCommand(currentSimpleCommand);
            currentSimpleCommand = new SimpleCommand();
        }
        else if (tokens[i].type == TOKEN_BACKGROUND)
        {
            currentCommand._background = 1;
        }
    }

    // the last command has no | after it, so add it here
    currentCommand.insertSimpleCommand(currentSimpleCommand);
}

SimpleCommand::SimpleCommand()
{
    _numberOfAvailableArguments = 5;
    _numberOfArguments = 0;
    _arguments = (char **)malloc(_numberOfAvailableArguments * sizeof(char *));
}

// Adds one word. The array must always end with NULL, because execvp needs that.
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

// Frees everything and resets the table so the next command starts empty
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

// Shows the table. Off by default. Turn on by removing the // in execute().
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

// Runs the table. Each command gets its own child. Pipes connect them in a chain.
void Command::execute()
{
    // print();

    if (_numberOfSimpleCommands == 0 || _simpleCommands[0]->_numberOfArguments == 0)
    {
        clear();
        return;
    }

    // the input file is opened once, before the loop, for the first command
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

        // a pipe is needed between this command and the next one
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
            // CHILD: set up 0, 1, 2 here, then become the command.
            // Never dup2 in the parent, or the shell's own screen changes.
            signal(SIGINT, SIG_DFL); // child reacts to Ctrl+C like a normal program

            // input: from the previous pipe, or from the input file
            if (infd != -1)
            {
                dup2(infd, 0);
                close(infd);
            }

            // output: into the next pipe, or the output file, or stays on screen
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

            // errors: into the error file, unless >>& already sent them to the output
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

            // only reached if execvp failed
            execvp(_simpleCommands[i]->_arguments[0], _simpleCommands[i]->_arguments);
            fprintf(stderr, "myshell: %s: command not found\n", _simpleCommands[i]->_arguments[0]);
            exit(1);
        }

        // PARENT: remember the child, then close the ends it no longer needs.
        // If the parent keeps a write end open, the next command waits forever.
        pids.push_back(pid);

        if (infd != -1)
        {
            close(infd);
        }

        if (!last)
        {
            close(fdpipe[1]);
            infd = fdpipe[0];
        }
    }

    // wait for every child, unless the command ends with &
    if (!_background)
    {
        for (size_t i = 0; i < pids.size(); i++)
        {
            waitpid(pids[i], NULL, 0);
        }
    }

    clear();
}

// Prints the prompt with the current folder.
// Only when typing at a keyboard: piped input gets no prompt, like real bash.
void showPrompt()
{
    if (!isatty(0))
        return;

    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        strcpy(cwd, "?");
    }
    printf("\033[36mmyshell:%s> \033[0m", cwd);
    fflush(stdout);
}

// Main loop: read a line, handle exit and cd here (no fork), otherwise parse and run
void Command::prompt()
{
    std::string input;
    showPrompt();

    while (std::getline(std::cin, input))
    {
        if (input == "exit")
        {
            printf("\033[31mGood bye!\033[0m\n");
            break;
        }

        // cd must change the shell's own folder, so it can't run in a child
        if (input.compare(0, 2, "cd") == 0 && (input.size() == 2 || input[2] == ' '))
        {
            std::string dir = input.size() > 3 ? input.substr(3) : "";
            if (dir.empty())
            {
                const char *home = getenv("HOME");
                if (home)
                    chdir(home);
            }
            else if (chdir(dir.c_str()) != 0)
            {
                perror("cd");
            }
        }
        else if (!input.empty())
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

// Runs when a child ends. Reaps every finished child and logs each one.
// Signals can merge, so one signal may mean several children: loop until none are left.
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
    // remember the folder we started in, so the log always goes there
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
        logPath = std::string(cwd) + "/child.log";
    else
        logPath = "child.log";

    signal(SIGINT, SIG_IGN); // the shell ignores Ctrl+C; children don't (see execute)
    signal(SIGCHLD, logChild);

    Command::_currentCommand.prompt();
    return 0;
}