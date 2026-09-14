#pragma once

#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace myplan {

struct LogEntry {
    std::string id;
    std::string type;
    std::string text;
    std::string createdAt;
    std::optional<std::string> sourceSubtargetId;
};

struct Subtarget {
    std::string id;
    std::string title;
    std::string status;
    std::string createdAt;
    std::optional<std::string> deadline;
    std::optional<std::string> promotedAt;
    std::optional<std::string> progressId;
};

struct Target {
    std::string id;
    std::string title;
    std::string createdAt;
    std::optional<std::string> deadline;
    std::string status = "active";
    std::optional<std::string> completedAt;
    std::optional<std::string> archivedAt;
    std::vector<LogEntry> log;
    std::vector<Subtarget> subtargets;
};

// Public JSON conversions let both the CLI and GUI use exactly the same field
// names as the on-disk format.
void to_json(nlohmann::json& value, const LogEntry& entry);
void from_json(const nlohmann::json& value, LogEntry& entry);
void to_json(nlohmann::json& value, const Subtarget& subtarget);
void from_json(const nlohmann::json& value, Subtarget& subtarget);
void to_json(nlohmann::json& value, const Target& target);
void from_json(const nlohmann::json& value, Target& target);

class PlannerError : public std::runtime_error {
public:
    PlannerError(std::string code, const std::string& message);
    const std::string& code() const noexcept;

private:
    std::string code_;
};

class PlannerStore {
public:
    explicit PlannerStore(std::filesystem::path dataFile);

    const std::filesystem::path& dataFile() const noexcept;
    std::vector<Target> listTargets() const;
    Target getTarget(const std::string& targetId) const;

    Target createTarget(const std::string& title,
                        const std::optional<std::string>& deadline = std::nullopt);
    Target editTarget(const std::string& targetId, const std::string& title);
    Target completeTarget(const std::string& targetId);
    Target restoreTarget(const std::string& targetId);
    Target deleteTarget(const std::string& targetId);
    Target setDeadline(const std::string& targetId,
                       const std::optional<std::string>& deadline);
    LogEntry addProgress(const std::string& targetId, const std::string& text);
    LogEntry addNote(const std::string& targetId, const std::string& text);
    LogEntry editLog(const std::string& targetId, const std::string& logId,
                     const std::string& text);
    Target deleteLog(const std::string& targetId, const std::string& logId);
    Subtarget addSubtarget(const std::string& targetId, const std::string& title,
                           const std::optional<std::string>& deadline = std::nullopt);
    Subtarget editSubtarget(const std::string& targetId, const std::string& subtargetId,
                            const std::string& title);
    Subtarget editSubtarget(const std::string& targetId, const std::string& subtargetId,
                            const std::string& title,
                            const std::optional<std::string>& deadline);
    Target deleteSubtarget(const std::string& targetId, const std::string& subtargetId);
    LogEntry promoteSubtarget(const std::string& targetId,
                              const std::string& subtargetId,
                              const std::optional<std::string>& progressText = std::nullopt);

private:
    std::filesystem::path dataFile_;
};

} // namespace myplan
