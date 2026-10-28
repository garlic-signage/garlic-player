/*
 garlic-player: SMIL Player for Digital Signage
 Copyright (C) 2026 Nikolaos Sagiadinos <garlic@saghiadinos.de>
 This file is part of the GarlicSignage source code

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License, version 3,
 as published by the Free Software Foundation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef GARLIC_CORE_TASKS_TASK_SCHEDULER_HPP
#define GARLIC_CORE_TASKS_TASK_SCHEDULER_HPP

#include "common/file_system.hpp"
#include "configuration/configuration.hpp"
#include "downloader/download_queue.hpp"
#include "logger/logger.hpp"
#include "smil/document.hpp"
#include "tasks/task_list.hpp"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace garlic::core::tasks
{
    // What the host has to do: the core restarts no device and installs nothing.
    struct ApplyConfiguration // settings changed: load the index again
    {
    };
    struct InstallSoftware // the firmware is in the cache
    {
        std::string task_id;
        std::string path;
    };
    struct Reboot
    {
        std::string task_id;
    };
    struct ClearCaches // then restart, like garlic-lib
    {
        std::string task_id;
        bool        player = false;
        bool        web    = false;
    };
    using TaskAction = std::variant<ApplyConfiguration, InstallSoftware, Reboot, ClearCaches>;

    /**
     * The tasks of the subscription TaskSchedule, garlic-lib: TaskScheduler, XMLConfiguration, FirmwareDownloader.
     * The task list is loaded with every new index. A task runs once: its id is stored as done after success.
     *
     *   updateSettings  configuration.xml loaded, checked, applied           → ApplyConfiguration, COMPLETED
     *   firmwareUpdate  software loaded and checked                          → InstallSoftware, PENDING
     *   shutdownPlayer                                                       → Reboot, PENDING
     *   applyCommand    clear_all_caches, clear_playercache/clear_cache, clear_webcache → ClearCaches, PENDING
     *
     * PENDING becomes COMPLETED at the next start (checkPending()): the player has restarted, and after a firmware
     * update it runs the version of the task (otherwise FAILED). The states go to task_execution_log.xml.
     */
    class TaskScheduler
    {
        public:
            using OnAction = std::function<void(TaskAction)>;

            TaskScheduler(downloader::DownloadQueue &queue, common::FileSystem &fs, logger::Logger &logger,
                          configuration::Configuration &config, std::string cache_dir, OnAction on_action);

            /// At start: the result of a task that waited for a restart.
            void checkPending();

            /// The subscriptions of a new index; the one with type TaskSchedule is used.
            void subscribe(const std::vector<smil::Subscription> &subscriptions);

            /// Result from the DownloadQueue; false if it was not one of the files of the scheduler.
            bool downloaded(const downloader::DownloadResult &result);

            /// A local configuration.xml, e.g. from the launcher (garlic-lib: setConfigFromExternal).
            bool applyConfigurationFile(std::string_view path);

        private:
            downloader::DownloadQueue    &queue;
            common::FileSystem           &fs;
            logger::Logger               &logger;
            configuration::Configuration &config;
            std::string                   cache_dir;
            OnAction                      on_action;

            std::string                        list_url;  // of the task list
            std::map<std::string, Task, std::less<>> loading; // URL → task whose file is downloaded

            void        run(const std::vector<Task> &tasks);
            void        finish(const Task &task, const downloader::DownloadResult &result);
            bool        applySettings(std::string_view xml, const std::string &path);
            bool        isDone(const Task &task) const;
            void        markDone(const Task &task);
            void        waitForRestart(const Task &task);
            std::optional<std::string> readChecked(const Task &task, const std::string &path);
            std::string takeFile(const downloader::DownloadResult &result);
            std::string firmwareName() const;
            logger::Metadata metadata(const std::string &path) const;
    };
}

#endif // GARLIC_CORE_TASKS_TASK_SCHEDULER_HPP
