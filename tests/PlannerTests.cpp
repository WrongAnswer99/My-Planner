#include "planner/Planner.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Function>
void requireErrorCode(Function&& function, const std::string& expectedCode) {
    try {
        function();
    } catch (const myplan::PlannerError& error) {
        require(error.code() == expectedCode,
                "expected error " + expectedCode + ", got " + error.code());
        return;
    }
    throw std::runtime_error("expected PlannerError: " + expectedCode);
}

} // namespace

int main() {
    const auto unique = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / ("myplan-test-" + std::to_string(unique));
    const std::filesystem::path dataFile = directory / "nested" / "data.json";

    try {
        myplan::PlannerStore store(dataFile);
        const myplan::Target created = store.createTarget("Release version 1");
        require(created.id.starts_with("target_"), "target ID prefix");
        require(!created.deadline, "new target deadline should be empty");
        require(created.status == "active", "new target should be active");
        require(!created.completedAt && !created.archivedAt,
                "new target should not have completion or archive times");

        const myplan::Target renamed = store.editTarget(created.id, "Release version 2");
        require(renamed.title == "Release version 2", "target title should be editable");
        const myplan::Target archived = store.completeTarget(created.id);
        require(archived.status == "archived",
                "completing a target should archive it immediately");
        require(archived.completedAt && archived.completedAt->ends_with('Z'),
                "target completion should record UTC time");
        require(archived.archivedAt == archived.completedAt,
                "completion and archive should use the same operation time");
        requireErrorCode([&] { store.completeTarget(created.id); },
                         "target_already_archived");

        const myplan::Target withDeadline = store.setDeadline(created.id, "2026-12-31");
        require(withDeadline.deadline == "2026-12-31", "deadline should be stored");

        const myplan::LogEntry progress = store.addProgress(created.id, "CLI is working");
        const myplan::LogEntry note = store.addNote(created.id, "Keep the core GUI-independent");
        require(progress.type == "progress", "progress log type");
        require(note.type == "note", "note log type");
        require(progress.createdAt.ends_with('Z'), "progress timestamp should be UTC");
        require(note.createdAt.ends_with('Z'), "note timestamp should be UTC");

        const myplan::Subtarget subtarget =
            store.addSubtarget(created.id, "Write tests", "2026-11-30");
        require(subtarget.status == "active", "new subtarget should be active");
        require(subtarget.deadline == "2026-11-30",
                "new subtarget deadline should be stored");
        const myplan::Subtarget rescheduledSubtarget =
            store.editSubtarget(created.id, subtarget.id, subtarget.title, "2026-12-15");
        require(rescheduledSubtarget.deadline == "2026-12-15",
                "subtarget deadline should be editable");
        const myplan::LogEntry promoted = store.promoteSubtarget(created.id, subtarget.id);
        require(promoted.text == "Write tests", "promotion should reuse subtarget title");
        require(promoted.sourceSubtargetId == subtarget.id, "promotion source should be linked");

        myplan::PlannerStore reloaded(dataFile);
        const myplan::Target saved = reloaded.getTarget(created.id);
        require(saved.title == "Release version 2", "edited target title should persist");
        require(saved.status == "archived", "archived target status should persist");
        require(saved.completedAt && saved.archivedAt,
                "target completion and archive times should persist");
        require(saved.log.size() == 3, "all log entries should persist");
        require(saved.subtargets.size() == 1, "subtarget should persist");
        require(saved.subtargets[0].status == "promoted", "promoted state should persist");
        require(saved.subtargets[0].progressId == promoted.id, "progress link should persist");
        require(!saved.subtargets[0].deadline,
                "promoting a subtarget should discard its deadline");

        const myplan::LogEntry editedNote = store.editLog(created.id, note.id, "Edited note");
        require(editedNote.text == "Edited note", "log text should be editable");
        const myplan::Subtarget editedSubtarget =
            store.editSubtarget(created.id, subtarget.id, "Tests completed");
        require(editedSubtarget.title == "Tests completed", "subtarget title should be editable");

        const myplan::Target withoutPromotedLog = store.deleteLog(created.id, promoted.id);
        require(withoutPromotedLog.log.size() == 2, "log entry should be deleted");
        require(!withoutPromotedLog.subtargets[0].progressId,
                "deleting a linked progress should clear the subtarget progress link");
        require(withoutPromotedLog.subtargets[0].promotedAt.has_value(),
                "deleting a linked progress should preserve completion time");

        const myplan::LogEntry linkedAgain =
            store.addProgress(created.id, "Independent progress after completion");
        require(!linkedAgain.sourceSubtargetId, "ordinary progress should not have a source");
        const myplan::Target withoutSubtarget = store.deleteSubtarget(created.id, subtarget.id);
        require(withoutSubtarget.subtargets.empty(), "subtarget should be deleted");
        require(withoutSubtarget.log.size() == 3, "deleting a subtarget should preserve logs");

        const myplan::Subtarget secondSubtarget =
            store.addSubtarget(created.id, "Verify relationship cleanup");
        const myplan::LogEntry secondPromoted =
            store.promoteSubtarget(created.id, secondSubtarget.id);
        const myplan::Target withoutSecondSubtarget =
            store.deleteSubtarget(created.id, secondSubtarget.id);
        const auto preservedProgress = std::find_if(
            withoutSecondSubtarget.log.begin(), withoutSecondSubtarget.log.end(),
            [&](const myplan::LogEntry& entry) { return entry.id == secondPromoted.id; });
        require(preservedProgress != withoutSecondSubtarget.log.end(),
                "deleting a subtarget should preserve its generated progress");
        require(!preservedProgress->sourceSubtargetId,
                "deleting a subtarget should clear the generated progress source link");

        requireErrorCode([&] { store.promoteSubtarget(created.id, subtarget.id); },
                         "subtarget_not_found");
        requireErrorCode([&] { store.editLog(created.id, "log_missing", "text"); },
                         "log_not_found");
        requireErrorCode([&] { store.deleteSubtarget(created.id, "subtarget_missing"); },
                         "subtarget_not_found");
        requireErrorCode([&] { store.setDeadline(created.id, "2026-02-30"); },
                         "invalid_deadline");
        requireErrorCode(
            [&] { store.addSubtarget(created.id, "Invalid deadline", "2026-02-30"); },
            "invalid_deadline");
        requireErrorCode([&] { store.getTarget("target_missing"); }, "target_not_found");

        const myplan::Target recoverable = store.createTarget("Recoverable target");
        requireErrorCode([&] { store.deleteTarget(recoverable.id); },
                         "target_not_archived");
        store.completeTarget(recoverable.id);
        const myplan::Target restored = store.restoreTarget(recoverable.id);
        require(restored.status == "active", "restored target should become active");
        require(!restored.completedAt && !restored.archivedAt,
                "restoring should clear completion and archive times");
        requireErrorCode([&] { store.restoreTarget(recoverable.id); },
                         "target_not_archived");
        store.completeTarget(recoverable.id);
        const myplan::Target permanentlyDeleted = store.deleteTarget(recoverable.id);
        require(permanentlyDeleted.id == recoverable.id,
                "permanent delete should return the deleted target");
        requireErrorCode([&] { store.getTarget(recoverable.id); }, "target_not_found");

        std::filesystem::remove_all(directory);
        std::cout << "Planner tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::filesystem::remove_all(directory);
        std::cerr << "Planner tests failed: " << error.what() << '\n';
        return 1;
    }
}
