#include "planner/Planner.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <random>
#include <regex>
#include <sstream>
#include <system_error>

namespace myplan {

using json = nlohmann::json;

struct Database {
    int version = 1;
    std::vector<Target> targets;
};

void to_json(json& value, const LogEntry& entry) {
    value = {
        {"id", entry.id},
        {"type", entry.type},
        {"text", entry.text},
        {"created_at", entry.createdAt},
    };
    value["source_subtarget_id"] = entry.sourceSubtargetId
                                       ? json(*entry.sourceSubtargetId)
                                       : json(nullptr);
}

void from_json(const json& value, LogEntry& entry) {
    value.at("id").get_to(entry.id);
    value.at("type").get_to(entry.type);
    value.at("text").get_to(entry.text);
    value.at("created_at").get_to(entry.createdAt);
    if (value.contains("source_subtarget_id") && !value.at("source_subtarget_id").is_null()) {
        entry.sourceSubtargetId = value.at("source_subtarget_id").get<std::string>();
    } else {
        entry.sourceSubtargetId.reset();
    }
}

void to_json(json& value, const Subtarget& subtarget) {
    value = {
        {"id", subtarget.id},
        {"title", subtarget.title},
        {"status", subtarget.status},
        {"created_at", subtarget.createdAt},
    };
    value["deadline"] = subtarget.deadline ? json(*subtarget.deadline) : json(nullptr);
    value["promoted_at"] = subtarget.promotedAt ? json(*subtarget.promotedAt) : json(nullptr);
    value["progress_id"] = subtarget.progressId ? json(*subtarget.progressId) : json(nullptr);
}

void from_json(const json& value, Subtarget& subtarget) {
    value.at("id").get_to(subtarget.id);
    value.at("title").get_to(subtarget.title);
    value.at("status").get_to(subtarget.status);
    value.at("created_at").get_to(subtarget.createdAt);
    if (value.contains("deadline") && !value.at("deadline").is_null()) {
        subtarget.deadline = value.at("deadline").get<std::string>();
    } else {
        subtarget.deadline.reset();
    }
    if (value.contains("promoted_at") && !value.at("promoted_at").is_null()) {
        subtarget.promotedAt = value.at("promoted_at").get<std::string>();
    } else {
        subtarget.promotedAt.reset();
    }
    if (value.contains("progress_id") && !value.at("progress_id").is_null()) {
        subtarget.progressId = value.at("progress_id").get<std::string>();
    } else {
        subtarget.progressId.reset();
    }
    if (subtarget.status != "active") {
        subtarget.deadline.reset();
    }
}

void to_json(json& value, const Target& target) {
    value = {
        {"id", target.id},
        {"title", target.title},
        {"created_at", target.createdAt},
        {"status", target.status},
        {"log", target.log},
        {"subtargets", target.subtargets},
    };
    value["deadline"] = target.deadline ? json(*target.deadline) : json(nullptr);
    value["completed_at"] = target.completedAt ? json(*target.completedAt) : json(nullptr);
    value["archived_at"] = target.archivedAt ? json(*target.archivedAt) : json(nullptr);
}

void from_json(const json& value, Target& target) {
    value.at("id").get_to(target.id);
    value.at("title").get_to(target.title);
    value.at("created_at").get_to(target.createdAt);
    if (value.contains("deadline") && !value.at("deadline").is_null()) {
        target.deadline = value.at("deadline").get<std::string>();
    } else {
        target.deadline.reset();
    }
    target.status = value.value("status", "active");
    if (target.status != "active" && target.status != "completed" &&
        target.status != "archived") {
        throw PlannerError("invalid_data", "invalid target status: " + target.status);
    }
    if (value.contains("completed_at") && !value.at("completed_at").is_null()) {
        target.completedAt = value.at("completed_at").get<std::string>();
    } else {
        target.completedAt.reset();
    }
    if (value.contains("archived_at") && !value.at("archived_at").is_null()) {
        target.archivedAt = value.at("archived_at").get<std::string>();
    } else {
        target.archivedAt.reset();
    }
    // The short-lived "completed" state predates the combined
    // complete-and-archive operation. Treat it as archived when loading.
    if (target.status == "completed") {
        target.status = "archived";
        if (!target.archivedAt) {
            target.archivedAt = target.completedAt;
        }
    }
    value.at("log").get_to(target.log);
    value.at("subtargets").get_to(target.subtargets);
}

void to_json(json& value, const Database& database) {
    value = {{"version", database.version}, {"targets", database.targets}};
}

void from_json(const json& value, Database& database) {
    value.at("version").get_to(database.version);
    value.at("targets").get_to(database.targets);
}

std::string currentUtcTime() {
    const auto now = std::chrono::system_clock::now();
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  now.time_since_epoch()) %
                              1000;
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S") << '.' << std::setw(3)
           << std::setfill('0') << milliseconds.count() << 'Z';
    return output.str();
}

bool isBlank(const std::string& value) {
    return value.empty() || std::all_of(value.begin(), value.end(), [](unsigned char character) {
               return std::isspace(character) != 0;
           });
}

void requireText(const std::string& value, const char* fieldName) {
    if (isBlank(value)) {
        throw PlannerError("invalid_argument", std::string(fieldName) + " must not be empty");
    }
}

bool isLeapYear(int year) {
    return year % 400 == 0 || (year % 4 == 0 && year % 100 != 0);
}

bool validDate(int year, int month, int day) {
    static constexpr int daysPerMonth[] = {31, 28, 31, 30, 31, 30,
                                            31, 31, 30, 31, 30, 31};
    if (year < 1 || month < 1 || month > 12 || day < 1) {
        return false;
    }
    const int maxDay = month == 2 && isLeapYear(year) ? 29 : daysPerMonth[month - 1];
    return day <= maxDay;
}

void validateDeadline(const std::string& deadline) {
    static const std::regex pattern(
        R"(^(\d{4})-(\d{2})-(\d{2})(?:T(\d{2}):(\d{2})(?::(\d{2}))?(?:\.\d{1,9})?(Z|[+-]\d{2}:\d{2})?)?$)");
    std::smatch match;
    if (!std::regex_match(deadline, match, pattern)) {
        throw PlannerError(
            "invalid_deadline",
            "deadline must be ISO 8601, for example 2026-12-31 or 2026-12-31T18:00:00+08:00");
    }

    const int year = std::stoi(match[1].str());
    const int month = std::stoi(match[2].str());
    const int day = std::stoi(match[3].str());
    if (!validDate(year, month, day)) {
        throw PlannerError("invalid_deadline", "deadline contains an invalid calendar date");
    }

    if (match[4].matched) {
        const int hour = std::stoi(match[4].str());
        const int minute = std::stoi(match[5].str());
        const int second = match[6].matched ? std::stoi(match[6].str()) : 0;
        if (hour > 23 || minute > 59 || second > 59) {
            throw PlannerError("invalid_deadline", "deadline contains an invalid time");
        }
        if (match[7].matched && match[7].str() != "Z") {
            const std::string zone = match[7].str();
            const int zoneHour = std::stoi(zone.substr(1, 2));
            const int zoneMinute = std::stoi(zone.substr(4, 2));
            if (zoneHour > 23 || zoneMinute > 59) {
                throw PlannerError("invalid_deadline", "deadline contains an invalid UTC offset");
            }
        }
    }
}

std::string makeId(const std::string& prefix) {
    static std::mt19937_64 generator([] {
        std::random_device device;
        const auto timeSeed = static_cast<unsigned long long>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
        return device() ^ timeSeed;
    }());
    static constexpr char digits[] = "0123456789abcdef";
    std::uniform_int_distribution<int> distribution(0, 15);
    std::string id = prefix;
    for (int index = 0; index < 12; ++index) {
        id.push_back(digits[distribution(generator)]);
    }
    return id;
}

Database loadDatabase(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return {};
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw PlannerError("io_error", "cannot open data file: " + path.string());
    }

    try {
        json value;
        input >> value;
        Database database = value.get<Database>();
        if (database.version != 1) {
            throw PlannerError("unsupported_version",
                               "unsupported data version: " + std::to_string(database.version));
        }
        return database;
    } catch (const PlannerError&) {
        throw;
    } catch (const std::exception& error) {
        throw PlannerError("invalid_data", "cannot parse data file: " + std::string(error.what()));
    }
}

void saveDatabase(const std::filesystem::path& path, const Database& database) {
    std::error_code error;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            throw PlannerError("io_error", "cannot create data directory: " + error.message());
        }
    }

    std::filesystem::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw PlannerError("io_error", "cannot write temporary data file: " + temporary.string());
        }
        output << std::setw(2) << json(database) << '\n';
        output.flush();
        if (!output) {
            throw PlannerError("io_error", "failed while writing data file: " + temporary.string());
        }
    }

    std::filesystem::path backup = path;
    backup += ".bak";
    const bool hadOriginal = std::filesystem::exists(path);
    if (hadOriginal) {
        std::filesystem::remove(backup, error);
        error.clear();
        std::filesystem::rename(path, backup, error);
        if (error) {
            std::filesystem::remove(temporary);
            throw PlannerError("io_error", "cannot prepare data file update: " + error.message());
        }
    }

    error.clear();
    std::filesystem::rename(temporary, path, error);
    if (error) {
        if (hadOriginal) {
            std::error_code restoreError;
            std::filesystem::rename(backup, path, restoreError);
        }
        std::filesystem::remove(temporary);
        throw PlannerError("io_error", "cannot replace data file: " + error.message());
    }
    if (hadOriginal) {
        std::filesystem::remove(backup, error);
    }
}

Target& findTarget(Database& database, const std::string& targetId) {
    const auto result = std::find_if(database.targets.begin(), database.targets.end(),
                                     [&](const Target& target) { return target.id == targetId; });
    if (result == database.targets.end()) {
        throw PlannerError("target_not_found", "target not found: " + targetId);
    }
    return *result;
}

const Target& findTarget(const Database& database, const std::string& targetId) {
    const auto result = std::find_if(database.targets.begin(), database.targets.end(),
                                     [&](const Target& target) { return target.id == targetId; });
    if (result == database.targets.end()) {
        throw PlannerError("target_not_found", "target not found: " + targetId);
    }
    return *result;
}

LogEntry appendLog(Target& target, const std::string& type, const std::string& text,
                   const std::optional<std::string>& sourceSubtargetId = std::nullopt) {
    requireText(text, type.c_str());
    LogEntry entry{makeId("log_"), type, text, currentUtcTime(), sourceSubtargetId};
    target.log.push_back(entry);
    return entry;
}

PlannerError::PlannerError(std::string code, const std::string& message)
    : std::runtime_error(message), code_(std::move(code)) {}

const std::string& PlannerError::code() const noexcept {
    return code_;
}

PlannerStore::PlannerStore(std::filesystem::path dataFile) : dataFile_(std::move(dataFile)) {
    if (dataFile_.empty()) {
        throw PlannerError("invalid_argument", "data file path must not be empty");
    }
}

const std::filesystem::path& PlannerStore::dataFile() const noexcept {
    return dataFile_;
}

std::vector<Target> PlannerStore::listTargets() const {
    return loadDatabase(dataFile_).targets;
}

Target PlannerStore::getTarget(const std::string& targetId) const {
    const Database database = loadDatabase(dataFile_);
    return findTarget(database, targetId);
}

Target PlannerStore::createTarget(const std::string& title,
                                  const std::optional<std::string>& deadline) {
    requireText(title, "title");
    if (deadline) {
        validateDeadline(*deadline);
    }
    Database database = loadDatabase(dataFile_);
    Target target{makeId("target_"), title, currentUtcTime(), deadline, "active",
                  std::nullopt, std::nullopt, {}, {}};
    database.targets.push_back(target);
    saveDatabase(dataFile_, database);
    return target;
}

Target PlannerStore::editTarget(const std::string& targetId, const std::string& title) {
    requireText(title, "title");
    Database database = loadDatabase(dataFile_);
    Target& target = findTarget(database, targetId);
    target.title = title;
    const Target result = target;
    saveDatabase(dataFile_, database);
    return result;
}

Target PlannerStore::completeTarget(const std::string& targetId) {
    Database database = loadDatabase(dataFile_);
    Target& target = findTarget(database, targetId);
    if (target.status == "archived") {
        throw PlannerError("target_already_archived",
                           "target has already been archived: " + targetId);
    }
    const std::string completedAt = currentUtcTime();
    target.status = "archived";
    target.completedAt = completedAt;
    target.archivedAt = completedAt;
    const Target result = target;
    saveDatabase(dataFile_, database);
    return result;
}

Target PlannerStore::restoreTarget(const std::string& targetId) {
    Database database = loadDatabase(dataFile_);
    Target& target = findTarget(database, targetId);
    if (target.status != "archived") {
        throw PlannerError("target_not_archived",
                           "only an archived target can be restored: " + targetId);
    }
    target.status = "active";
    target.completedAt.reset();
    target.archivedAt.reset();
    const Target result = target;
    saveDatabase(dataFile_, database);
    return result;
}

Target PlannerStore::deleteTarget(const std::string& targetId) {
    Database database = loadDatabase(dataFile_);
    const auto result = std::find_if(database.targets.begin(), database.targets.end(),
                                     [&](const Target& target) { return target.id == targetId; });
    if (result == database.targets.end()) {
        throw PlannerError("target_not_found", "target not found: " + targetId);
    }
    if (result->status != "archived") {
        throw PlannerError("target_not_archived",
                           "only an archived target can be permanently deleted: " + targetId);
    }
    const Target deleted = *result;
    database.targets.erase(result);
    saveDatabase(dataFile_, database);
    return deleted;
}

Target PlannerStore::setDeadline(const std::string& targetId,
                                 const std::optional<std::string>& deadline) {
    if (deadline) {
        validateDeadline(*deadline);
    }
    Database database = loadDatabase(dataFile_);
    Target& target = findTarget(database, targetId);
    target.deadline = deadline;
    const Target result = target;
    saveDatabase(dataFile_, database);
    return result;
}

LogEntry PlannerStore::addProgress(const std::string& targetId, const std::string& text) {
    Database database = loadDatabase(dataFile_);
    LogEntry entry = appendLog(findTarget(database, targetId), "progress", text);
    saveDatabase(dataFile_, database);
    return entry;
}

LogEntry PlannerStore::addNote(const std::string& targetId, const std::string& text) {
    Database database = loadDatabase(dataFile_);
    LogEntry entry = appendLog(findTarget(database, targetId), "note", text);
    saveDatabase(dataFile_, database);
    return entry;
}

LogEntry PlannerStore::editLog(const std::string& targetId, const std::string& logId,
                               const std::string& text) {
    requireText(text, "log text");
    Database database = loadDatabase(dataFile_);
    Target& target = findTarget(database, targetId);
    const auto result = std::find_if(target.log.begin(), target.log.end(),
                                     [&](const LogEntry& entry) { return entry.id == logId; });
    if (result == target.log.end()) {
        throw PlannerError("log_not_found", "log entry not found: " + logId);
    }
    result->text = text;
    const LogEntry edited = *result;
    saveDatabase(dataFile_, database);
    return edited;
}

Target PlannerStore::deleteLog(const std::string& targetId, const std::string& logId) {
    Database database = loadDatabase(dataFile_);
    Target& target = findTarget(database, targetId);
    const auto result = std::find_if(target.log.begin(), target.log.end(),
                                     [&](const LogEntry& entry) { return entry.id == logId; });
    if (result == target.log.end()) {
        throw PlannerError("log_not_found", "log entry not found: " + logId);
    }
    for (auto& subtarget : target.subtargets) {
        if (subtarget.progressId == logId) {
            subtarget.progressId.reset();
        }
    }
    target.log.erase(result);
    const Target edited = target;
    saveDatabase(dataFile_, database);
    return edited;
}

Subtarget PlannerStore::addSubtarget(const std::string& targetId, const std::string& title,
                                     const std::optional<std::string>& deadline) {
    requireText(title, "title");
    if (deadline) {
        validateDeadline(*deadline);
    }
    Database database = loadDatabase(dataFile_);
    Target& target = findTarget(database, targetId);
    Subtarget subtarget{makeId("subtarget_"), title, "active", currentUtcTime(), deadline,
                        std::nullopt, std::nullopt};
    target.subtargets.push_back(subtarget);
    saveDatabase(dataFile_, database);
    return subtarget;
}

Subtarget PlannerStore::editSubtarget(const std::string& targetId,
                                      const std::string& subtargetId,
                                      const std::string& title) {
    requireText(title, "title");
    Database database = loadDatabase(dataFile_);
    Target& target = findTarget(database, targetId);
    const auto result = std::find_if(target.subtargets.begin(), target.subtargets.end(),
                                     [&](const Subtarget& item) { return item.id == subtargetId; });
    if (result == target.subtargets.end()) {
        throw PlannerError("subtarget_not_found", "subtarget not found: " + subtargetId);
    }
    result->title = title;
    const Subtarget edited = *result;
    saveDatabase(dataFile_, database);
    return edited;
}

Subtarget PlannerStore::editSubtarget(const std::string& targetId,
                                      const std::string& subtargetId,
                                      const std::string& title,
                                      const std::optional<std::string>& deadline) {
    requireText(title, "title");
    if (deadline) {
        validateDeadline(*deadline);
    }
    Database database = loadDatabase(dataFile_);
    Target& target = findTarget(database, targetId);
    const auto result = std::find_if(target.subtargets.begin(), target.subtargets.end(),
                                     [&](const Subtarget& item) { return item.id == subtargetId; });
    if (result == target.subtargets.end()) {
        throw PlannerError("subtarget_not_found", "subtarget not found: " + subtargetId);
    }
    if (result->status != "active" && deadline) {
        throw PlannerError("subtarget_already_promoted",
                           "cannot set a deadline on a promoted subtarget: " + subtargetId);
    }
    result->title = title;
    result->deadline = result->status == "active" ? deadline : std::nullopt;
    const Subtarget edited = *result;
    saveDatabase(dataFile_, database);
    return edited;
}

Target PlannerStore::deleteSubtarget(const std::string& targetId,
                                     const std::string& subtargetId) {
    Database database = loadDatabase(dataFile_);
    Target& target = findTarget(database, targetId);
    const auto result = std::find_if(target.subtargets.begin(), target.subtargets.end(),
                                     [&](const Subtarget& item) { return item.id == subtargetId; });
    if (result == target.subtargets.end()) {
        throw PlannerError("subtarget_not_found", "subtarget not found: " + subtargetId);
    }
    for (auto& entry : target.log) {
        if (entry.sourceSubtargetId == subtargetId) {
            entry.sourceSubtargetId.reset();
        }
    }
    target.subtargets.erase(result);
    const Target edited = target;
    saveDatabase(dataFile_, database);
    return edited;
}

LogEntry PlannerStore::promoteSubtarget(
    const std::string& targetId, const std::string& subtargetId,
    const std::optional<std::string>& progressText) {
    Database database = loadDatabase(dataFile_);
    Target& target = findTarget(database, targetId);
    const auto result = std::find_if(target.subtargets.begin(), target.subtargets.end(),
                                     [&](const Subtarget& item) { return item.id == subtargetId; });
    if (result == target.subtargets.end()) {
        throw PlannerError("subtarget_not_found", "subtarget not found: " + subtargetId);
    }
    if (result->status != "active") {
        throw PlannerError("subtarget_already_promoted",
                           "subtarget has already been promoted: " + subtargetId);
    }
    const std::string text = progressText.value_or(result->title);
    LogEntry entry = appendLog(target, "progress", text, subtargetId);
    result->status = "promoted";
    result->promotedAt = entry.createdAt;
    result->progressId = entry.id;
    result->deadline.reset();
    saveDatabase(dataFile_, database);
    return entry;
}

} // namespace myplan
