#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include "sys/syscall.h"

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;
    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

bool isComplex(char** args) {
    if(args == nullptr)
        return false;
    for(int i = 0; args[i] != nullptr; i++) {
        if(strchr(args[i],'*') != nullptr || strchr(args[i],'?') != nullptr) {
            return true;
        }
    }
    return false;
}

// TODO: Add your implementation for classes in Commands.h 

SmallShell::SmallShell() {
    og_name = (char*) malloc(string("smash").length() + 1);
    strcpy(og_name, string("smash").c_str());
    curr_name = (char*) malloc(string(og_name).length() + 1);
    strcpy(curr_name, og_name);
}

SmallShell::~SmallShell() {
    free(og_name);
    free(curr_name);
}

void SmallShell::ch_prompt(const char *name){
    free(curr_name);
    if(name == NULL){
        curr_name = (char*) malloc(string(og_name).length() + 1);
        strcpy(curr_name, og_name);
    }
    else {
        curr_name = (char*) malloc(string(name).length() + 1);
        strcpy(curr_name, name);
    }
}


const char* SmallShell::get_prompt(){
    return curr_name;
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    // For example:
    
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    bool bg = false;
    std::string cmd = cmd_line;
    if(_isBackgroundComamnd(cmd_line)) {
        bg = true;
    }

    if(firstWord.compare("chprompt") == 0){
        return new ChPrompt(cmd_line);
    }
    else if (firstWord.compare("pwd") == 0) {
      return new GetCurrDirCommand(cmd_line);
    }
    /*
    else if (firstWord.compare("showpid") == 0) {
      return new ShowPidCommand(cmd_line);
    }
    else if ...
    .....
    */
    else {
        if(bg) {
            return new ExternalCommand(cmd_line, true);
        }
        else {
            return new ExternalCommand(cmd_line, false);
        }
    }
    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    // TODO: Add your implementation here
    // for example:
    Command* cmd = CreateCommand(cmd_line);
    if(ExternalCommand* extCmd = dynamic_cast<ExternalCommand*>(cmd)) {//if succeeds then cmd is external
        const pid_t p = fork();
        if(p > 0) {
            if(!extCmd->is_bg) {
                wait(NULL);
            }
            else {

            }
        }
        else {
            cmd->execute();
        }
    }
    else
        cmd->execute();
    // Please note that you must fork smash process for some commands (e.g., external commands....)
}

Command::Command(const char* cmd_line){
    this->cmd_line = (char*) malloc(sizeof(char)*(string(cmd_line).length()+1));
    strcpy(this->cmd_line, cmd_line);
}

Command::~Command(){free(this->cmd_line);}

char** Command::make_args(const char* cmd_line){
    char** args = (char**) malloc(sizeof(char*) * 20);
    char* copy_cmd_line = strdup(cmd_line);
    _removeBackgroundSign(copy_cmd_line);
    _parseCommandLine(copy_cmd_line, args);
    return args;
}

void Command::free_args(char** args){
    for(int i = 0; args[i] != NULL; i++)
        free(args[i]);
    free(args);
}

void ChPrompt::execute(){
    SmallShell& s = SmallShell::getInstance();
    char** args = this->make_args(this->get_cmd_line());
    s.ch_prompt(args[1]);
    this->free_args(args);
}

void GetCurrDirCommand::execute(){
    char* path = getcwd(NULL, 0);
    std::cout << path << std::endl;
    free(path);
}

void ExternalCommand::execute() { //will always be the son
    char** args = this->make_args(this->get_cmd_line());
    if(!isComplex(args)) { //should not have *,? and & because of make_args
        if(args[0] != nullptr) {
            execvp(args[0], args); //should leave automatically
            perror("smash error: execvp failed"); // there is no command like that/no fitting flags
        }
    }
}
