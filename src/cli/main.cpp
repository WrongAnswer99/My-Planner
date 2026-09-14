#include "planner/Planner.hpp"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

using json = nlohmann::json;

[[noreturn]] void usageError(const std::string& message) {
    throw myplan::PlannerError("usage_error", message);
}

void printHelp() {
    std::cout
        << "My Plan CLI\n\n"
        << "Usage:\n"
        << "  myplan-cli [--data FILE] [--pretty] target create TITLE [--deadline TIME]\n"
        << "  myplan-cli [--data FILE] [--pretty] target edit TARGET_ID TITLE\n"
        << "  myplan-cli [--data FILE] [--pretty] target complete TARGET_ID\n"
        << "  myplan-cli [--data FILE] [--pretty] target restore TARGET_ID\n"
        << "  myplan-cli [--data FILE] [--pretty] target delete TARGET_ID\n"
        << "  myplan-cli [--data FILE] [--pretty] target deadline TARGET_ID TIME|none\n"
        << "  myplan-cli [--data FILE] [--pretty] target list\n"
        << "  myplan-cli [--data FILE] [--pretty] target show TARGET_ID\n"
        << "  myplan-cli [--data FILE] [--pretty] progress add TARGET_ID TEXT\n"
        << "  myplan-cli [--data FILE] [--pretty] note add TARGET_ID TEXT\n"
        << "  myplan-cli [--data FILE] [--pretty] log edit TARGET_ID LOG_ID TEXT\n"
        << "  myplan-cli [--data FILE] [--pretty] log delete TARGET_ID LOG_ID\n"
        << "  myplan-cli [--data FILE] [--pretty] subtarget add TARGET_ID TITLE [--deadline TIME]\n"
        << "  myplan-cli [--data FILE] [--pretty] subtarget edit TARGET_ID SUBTARGET_ID TITLE [--deadline TIME|none]\n"
        << "  myplan-cli [--data FILE] [--pretty] subtarget delete TARGET_ID SUBTARGET_ID\n"
        << "  myplan-cli [--data FILE] [--pretty] subtarget promote TARGET_ID SUBTARGET_ID [TEXT]\n\n"
        << "  subtarget complete is an alias for subtarget promote.\n"
        << "TIME uses ISO 8601, such as 2026-12-31 or 2026-12-31T18:00:00+08:00.\n"
        << "The default data file is myplan-data.json beside myplan-cli.\n";
}

struct Options {
    std::filesystem::path dataFile;
    bool pretty = false;
    std::vector<std::string> command;
};

std::filesystem::path pathFromUtf8(const std::string& value) {
    const auto* begin = reinterpret_cast<const char8_t*>(value.data());
    return std::filesystem::path(std::u8string(begin, begin + value.size()));
}

#ifdef _WIN32
std::string utf8FromWide(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        throw myplan::PlannerError("argument_encoding_error",
                                   "cannot convert a command-line argument to UTF-8");
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                        static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::vector<std::string> commandLineArguments() {
    int count = 0;
    wchar_t** wideArguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!wideArguments) {
        throw myplan::PlannerError("argument_encoding_error",
                                   "cannot read the Windows command line");
    }
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(count));
    try {
        for (int index = 0; index < count; ++index) {
            arguments.push_back(utf8FromWide(wideArguments[index]));
        }
    } catch (...) {
        LocalFree(wideArguments);
        throw;
    }
    LocalFree(wideArguments);
    return arguments;
}
#else
std::vector<std::string> commandLineArguments(int argc, char* argv[]) {
    return {argv, argv + argc};
}
#endif

Options parseOptions(const std::vector<std::string>& arguments) {
    Options options;
    bool hasExplicitDataFile = false;
#ifdef _WIN32
    if (const wchar_t* environmentPath = _wgetenv(L"MYPLAN_DATA")) {
        options.dataFile = environmentPath;
        hasExplicitDataFile = true;
    }
#else
    if (const char* environmentPath = std::getenv("MYPLAN_DATA")) {
        options.dataFile = environmentPath;
        hasExplicitDataFile = true;
    }
#endif

    for (std::size_t index = 1; index < arguments.size(); ++index) {
        const std::string& argument = arguments[index];
        if (argument == "--data") {
            if (++index >= arguments.size()) {
                usageError("--data requires a file path");
            }
            options.dataFile = pathFromUtf8(arguments[index]);
            hasExplicitDataFile = true;
        } else if (argument == "--pretty") {
            options.pretty = true;
        } else if (argument == "--help" || argument == "-h") {
            options.command = {"help"};
        } else {
            options.command.push_back(argument);
        }
    }
    if (!hasExplicitDataFile) {
        std::error_code error;
        const std::filesystem::path executable = arguments.empty()
                                                     ? std::filesystem::path{}
                                                     : std::filesystem::absolute(
                                                           pathFromUtf8(arguments[0]), error);
        options.dataFile = !error && executable.has_parent_path()
                               ? executable.parent_path() / "myplan-data.json"
                               : std::filesystem::path("myplan-data.json");
    }
    return options;
}

json success(const std::string& action, json result) {
    return {{"ok", true}, {"action", action}, {"result", std::move(result)}};
}

json execute(const Options& options) {
    if (options.command.empty()) {
        usageError("a command is required; use --help for usage");
    }
    myplan::PlannerStore store(options.dataFile);
    const auto& command = options.command;

    if (command[0] == "target") {
        if (command.size() >= 2 && command[1] == "list" && command.size() == 2) {
            return success("target.list", store.listTargets());
        }
        if (command.size() >= 2 && command[1] == "show" && command.size() == 3) {
            return success("target.show", store.getTarget(command[2]));
        }
        if (command.size() == 4 && command[1] == "edit") {
            return success("target.edit", store.editTarget(command[2], command[3]));
        }
        if (command.size() == 3 && command[1] == "complete") {
            return success("target.complete", store.completeTarget(command[2]));
        }
        if (command.size() == 3 && command[1] == "restore") {
            return success("target.restore", store.restoreTarget(command[2]));
        }
        if (command.size() == 3 && command[1] == "delete") {
            return success("target.delete", store.deleteTarget(command[2]));
        }
        if (command.size() >= 2 && command[1] == "deadline" && command.size() == 4) {
            const std::optional<std::string> deadline =
                command[3] == "none" ? std::nullopt : std::optional<std::string>(command[3]);
            return success("target.deadline", store.setDeadline(command[2], deadline));
        }
        if (command.size() >= 2 && command[1] == "create") {
            if (command.size() != 3 && command.size() != 5) {
                usageError("target create requires TITLE and optional --deadline TIME");
            }
            std::optional<std::string> deadline;
            if (command.size() == 5) {
                if (command[3] != "--deadline") {
                    usageError("expected --deadline TIME after TITLE");
                }
                deadline = command[4];
            }
            return success("target.create", store.createTarget(command[2], deadline));
        }
        usageError("unknown target command or wrong number of arguments");
    }

    if (command[0] == "progress" && command.size() == 4 && command[1] == "add") {
        return success("progress.add", store.addProgress(command[2], command[3]));
    }
    if (command[0] == "note" && command.size() == 4 && command[1] == "add") {
        return success("note.add", store.addNote(command[2], command[3]));
    }
    if (command[0] == "log" && command.size() == 5 && command[1] == "edit") {
        return success("log.edit", store.editLog(command[2], command[3], command[4]));
    }
    if (command[0] == "log" && command.size() == 4 && command[1] == "delete") {
        return success("log.delete", store.deleteLog(command[2], command[3]));
    }
    if (command[0] == "subtarget" && (command.size() == 4 || command.size() == 6) &&
        command[1] == "add") {
        std::optional<std::string> deadline;
        if (command.size() == 6) {
            if (command[4] != "--deadline") {
                usageError("expected --deadline TIME after TITLE");
            }
            deadline = command[5];
        }
        return success("subtarget.add", store.addSubtarget(command[2], command[3], deadline));
    }
    if (command[0] == "subtarget" && (command.size() == 5 || command.size() == 7) &&
        command[1] == "edit") {
        if (command.size() == 5) {
            return success("subtarget.edit",
                           store.editSubtarget(command[2], command[3], command[4]));
        }
        if (command[5] != "--deadline") {
            usageError("expected --deadline TIME|none after TITLE");
        }
        const std::optional<std::string> deadline =
            command[6] == "none" ? std::nullopt : std::optional<std::string>(command[6]);
        return success("subtarget.edit",
                       store.editSubtarget(command[2], command[3], command[4], deadline));
    }
    if (command[0] == "subtarget" && command.size() == 4 && command[1] == "delete") {
        return success("subtarget.delete", store.deleteSubtarget(command[2], command[3]));
    }
    if (command[0] == "subtarget" && (command.size() == 4 || command.size() == 5) &&
        (command[1] == "promote" || command[1] == "complete")) {
        const std::optional<std::string> text =
            command.size() == 5 ? std::optional<std::string>(command[4]) : std::nullopt;
        return success("subtarget.promote", store.promoteSubtarget(command[2], command[3], text));
    }

    usageError("unknown command or wrong number of arguments; use --help for usage");
}

} // namespace

int main(int argc, char* argv[]) {
    try {
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        const Options options = parseOptions(commandLineArguments());
#else
        const Options options = parseOptions(commandLineArguments(argc, argv));
#endif
        if (options.command == std::vector<std::string>{"help"}) {
            printHelp();
            return 0;
        }
        const json output = execute(options);
        std::cout << output.dump(options.pretty ? 2 : -1) << '\n';
        return 0;
    } catch (const myplan::PlannerError& error) {
        std::cerr << json{{"ok", false},
                          {"error", {{"code", error.code()}, {"message", error.what()}}}}
                         .dump()
                  << '\n';
        return error.code() == "usage_error" ? 2 : 1;
    } catch (const std::exception& error) {
        std::cerr << json{{"ok", false},
                          {"error", {{"code", "internal_error"}, {"message", error.what()}}}}
                         .dump()
                  << '\n';
        return 1;
    }
}
