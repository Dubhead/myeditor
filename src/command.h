#pragma once

#include <memory>
#include <string>

enum class CommandStatusCode: unsigned int {
    Success,
    Error,
    EmptyCommand,
    CommandNotFound,
    CommandNotImplemented,
};
typedef std::tuple<CommandStatusCode, std::string> CommandStatus;

class Command
{
public:
    Command();
    virtual ~Command();
    CommandStatus execute(const std::string& command);
    void log(const std::string& msg);

private:
    Command(const Command&) = delete;
    Command(Command&&) = delete;
    Command& operator=(const Command&) = delete;
    Command& operator=(Command&&) = delete;

    class Impl;
    const std::unique_ptr<Impl> pimpl;
};

// eof
