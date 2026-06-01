#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"
#include <sys/wait.h>

using namespace std;

void ctrlCHandler(int sig_num) {
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
