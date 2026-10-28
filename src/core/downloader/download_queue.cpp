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
#include "downloader/download_queue.hpp"

#include <algorithm>

namespace garlic::core::downloader
{
    DownloadQueue::DownloadQueue(HttpTransport &transport, common::FileSystem &fs, Inventory &inventory,
                                 const common::Clock &clock, const DownloaderConfig &config, std::size_t max_parallel,
                                 Downloader::FinishedCallback callback)
        : on_finished(std::move(callback))
    {
        slots.resize(std::max<std::size_t>(max_parallel, 1));
        for (Slot &slot : slots)
            slot.downloader = std::make_unique<Downloader>(transport, fs, inventory, clock, config);
    }

    void DownloadQueue::add(std::string url, std::string file_name)
    {
        if (contains(url))
            return;

        pending.push_back({std::move(url), std::move(file_name)});
        startPending();
    }

    void DownloadQueue::clear()
    {
        pending.clear();
        for (Slot &slot : slots)
        {
            slot.downloader->cancel();
            slot.url.clear();
        }
    }

    void DownloadQueue::poll()
    {
        for (Slot &slot : slots)
            slot.downloader->poll();
    }

    std::size_t DownloadQueue::runningCount() const noexcept
    {
        return static_cast<std::size_t>(
            std::ranges::count_if(slots, [](const Slot &slot) { return slot.downloader->isBusy(); }));
    }

    void DownloadQueue::startPending()
    {
        for (std::size_t i = 0; i < slots.size() && !pending.empty(); ++i)
        {
            Slot &slot = slots[i];
            if (slot.downloader->isBusy())
                continue;

            Job job = std::move(pending.front());
            pending.pop_front();
            slot.url = job.url;

            // slots never reallocate after construction, so the index stays valid
            slot.downloader->start(std::move(job.url), std::move(job.file_name),
                                   [this, i](const DownloadResult &result)
                                   {
                                       slots[i].url.clear();
                                       if (on_finished)
                                           on_finished(result);
                                       startPending();
                                   });
        }
    }

    bool DownloadQueue::contains(std::string_view url) const
    {
        return std::ranges::any_of(pending, [url](const Job &job) { return job.url == url; }) ||
               std::ranges::any_of(slots, [url](const Slot &slot) { return slot.url == url; });
    }
}
