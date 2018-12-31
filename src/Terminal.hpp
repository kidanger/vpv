#pragma once

#include <string>
#include <map>
#include <deque>
#include <mutex>

class LoadingThread;

class Terminal {
    friend class Process;

    std::mutex lock;
    std::map<std::string, std::string> cache;
    char bufcommand[2048];
    std::string command;
    std::string output;
    bool focusInput;
    LoadingThread* runner;
    std::deque<std::string> queuecommands;

    void updateOutput();

public:
    bool shown;

    Terminal();

    void setVisible(bool visible);
    void tick();

};

