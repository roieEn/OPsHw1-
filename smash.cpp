#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

void handle_INT(int signum){
    const char *in_mssg = "smash: got ctrl-C\n";
    write(1, in_mssg, strlen(in_mssg));
    SmallShell &s = SmallShell::getInstance();
    int pid = s.getPid();
    if(pid > 0){
        if(kill(pid, SIGKILL) != 0)
            perror("smash error: kill failed");
        else{
            waitpid(pid, NULL, 0);
            std::string kill_mssg = "smash: process " + std::to_string(pid) + " was killed\n";
            write(1, kill_mssg.c_str(), kill_mssg.length());
            s.setPid(-1);
        }
    }
    else{
        write(1, s.get_prompt(), strlen(s.get_prompt()));
        write(1, "> ", 2);
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_INT);

    SmallShell &smash = SmallShell::getInstance();
    while (true) {
        write(1, smash.get_prompt(), strlen(smash.get_prompt()));
        write(1, "> ", 2);
        std::string cmd_line;
        std::getline(std::cin, cmd_line);
        smash.executeCommand(cmd_line.c_str());
    }
    return 0;
}