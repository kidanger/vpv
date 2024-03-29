#pragma once

#include <array>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "LoadingThread.hpp"

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
    std::string bufcommand;
    std::string command;
    CommandResult currentResult;
    enum State { NO_COMMAND,
        RUNNING,
        FINISHED } state
        = NO_COMMAND;
    bool shown;
    bool focusInput;
    std::unique_ptr<SleepyLoadingThread<class Process>> runner;
    std::deque<std::string> queuecommands;

    void updateOutput();

    Terminal();
    ~Terminal();

    void setVisible(bool visible);
    void tick();
    void stopAllAndJoin();
};
