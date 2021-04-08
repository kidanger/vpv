#pragma once

#include <string>
#include <map>
#include <deque>
#include <mutex>

class SleepyLoadingThread;

struct CommandResult {
    std::string out;
    std::string err;
    int status;
};

class Terminal {
    friend class Process;

    std::mutex lock;
    std::map<std::string, CommandResult> cache;

public:
    char bufcommand[2048];
    std::string command;
    CommandResult currentResult;
    enum State { NO_COMMAND, RUNNING, FINISHED } state = NO_COMMAND;
    bool shown;
    bool focusInput;
    SleepyLoadingThread* runner;
    std::deque<std::string> queuecommands;

    void updateOutput();

    Terminal();
    ~Terminal();

    void setVisible(bool visible);
    void tick();

};

