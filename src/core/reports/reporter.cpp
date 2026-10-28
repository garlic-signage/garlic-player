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
#include "reports/reporter.hpp"

#include "reports/report_xml.hpp"

namespace garlic::core::reports
{
    /// One subscription: its schedule and its running upload.
    struct Reporter::Job final : downloader::HttpResponseHandler
    {
        Reporter          &owner;
        Kind               kind;
        smil::Subscription subscription;
        std::optional<std::chrono::steady_clock::time_point> due;
        std::optional<downloader::RequestId>                 request;
        int                                                  status = 0;
        std::string                                          sending; // log file of the running upload

        Job(Reporter &r, Kind k, smil::Subscription s) : owner(r), kind(k), subscription(std::move(s)) {}

        void onHead(const downloader::HttpResponseHead &head) override { status = head.status; }
        bool onData(std::span<const std::byte>) override { return true; }
        void onComplete(std::expected<void, downloader::TransportError> result) override
        {
            request.reset();
            owner.finished(*this, result.has_value() && status >= 200 && status < 300);
        }
    };

    namespace
    {
        std::optional<int> kindOf(std::string_view type)
        {
            if (type == "PlaylogCollection")
                return 0;
            if (type == "EventlogCollection")
                return 1;
            if (type == "InventoryReport")
                return 2;
            if (type == "SystemReport")
                return 3;
            return std::nullopt; // e.g. TaskSchedule: not a report
        }

        bool same(const smil::Subscription &a, const smil::Subscription &b)
        {
            return a.type == b.type && a.action == b.action && a.method == b.method &&
                   a.refresh_interval == b.refresh_interval && a.retry_interval == b.retry_interval;
        }

        /// garlic-lib: CreateInventoryReport::getObjectId - the file name up to the first dot.
        std::string objectId(std::string_view file_name)
        {
            return std::string(file_name.substr(0, file_name.find('.')));
        }
    }

    Reporter::Reporter(downloader::HttpTransport &t, common::FileSystem &f, const common::Clock &c, logger::Logger &log,
                       configuration::Configuration &cfg, const database::InventoryTable &inv, std::string cache,
                       SystemInfoSource info)
        : transport(t), fs(f), clock(c), logger(log), config(cfg), inventory(inv), cache_dir(std::move(cache)),
          system_info(std::move(info))
    {
    }

    Reporter::~Reporter()
    {
        for (auto &job : jobs)
            if (job->request)
                transport.cancel(*job->request);
    }

    void Reporter::subscribe(const std::vector<smil::Subscription> &subscriptions)
    {
        std::vector<std::unique_ptr<Job>> next;
        for (const smil::Subscription &subscription : subscriptions)
        {
            auto kind = kindOf(subscription.type);
            if (!kind)
                continue;
            std::erase_if(next, [&](const auto &job) { return job->subscription.type == subscription.type; });

            auto existing = std::ranges::find_if(jobs, [&](const auto &job) { return job && same(job->subscription, subscription); });
            if (existing != jobs.end())
            {
                next.push_back(std::move(*existing)); // unchanged: keeps its schedule
                continue;
            }
            auto job = std::make_unique<Job>(*this, static_cast<Kind>(*kind), subscription);
            job->due = clock.monotonic(); // new or changed: at once
            next.push_back(std::move(job));
        }
        for (auto &job : jobs)
            if (job && job->request)
                transport.cancel(*job->request);
        jobs = std::move(next);
    }

    void Reporter::poll()
    {
        auto now = clock.monotonic();
        for (auto &job : jobs)
            if (!job->request && job->due && *job->due <= now)
                send(*job);
    }

    std::optional<std::chrono::steady_clock::time_point> Reporter::nextDeadline() const
    {
        std::optional<std::chrono::steady_clock::time_point> next;
        for (const auto &job : jobs)
            if (!job->request && job->due && (!next || *job->due < *next))
                next = job->due;
        return next;
    }

    // ------------------------------------------------------------------ uploads

    void Reporter::send(Job &job)
    {
        job.due.reset();
        if (job.kind == Kind::PlayLog || job.kind == Kind::EventLog)
        {
            job.sending = nextLogFile(job.kind);
            if (job.sending.empty())
            {
                schedule(job, job.subscription.refresh_interval); // nothing to send
                return;
            }
        }

        downloader::HttpRequest request{downloader::HttpMethod::Put, url(job), {}, body(job)};
        request.headers.add("Content-Type", "application/xml");
        request.headers.add("User-Agent", config.userAgent());

        job.status = 0;
        auto started = transport.start(request, job);
        if (!started)
        {
            logger.warning("Reporting", "upload not started: " + started.error().message);
            finished(job, false);
            return;
        }
        job.request = *started;
    }

    /// garlic-lib: doSucceed / doFailed
    void Reporter::finished(Job &job, bool ok)
    {
        bool is_log = job.kind == Kind::PlayLog || job.kind == Kind::EventLog;
        if (!ok)
        {
            logger.warning("Reporting", "upload failed: " + job.subscription.action);
            int retry = job.subscription.retry_interval > 0 ? job.subscription.retry_interval
                                                            : job.subscription.refresh_interval;
            schedule(job, retry);
            return;
        }
        if (is_log)
        {
            (void)fs.remove(job.sending);
            job.sending.clear();
            job.due = clock.monotonic(); // the next file at once
            return;
        }
        schedule(job, job.subscription.refresh_interval);
    }

    void Reporter::schedule(Job &job, int seconds)
    {
        if (seconds > 0)
            job.due = clock.monotonic() + std::chrono::seconds(seconds);
        else
            job.due.reset(); // without an interval only once (garlic-lib: a timer of 0 fired all the time)
    }

    /**
     * The next log file to upload, renamed to "<log>.sending": first one left from a failed try, otherwise the oldest
     * rotated one. The current log is rotated first, so it is sent too (garlic-lib: checkForRotate).
     */
    std::string Reporter::nextLogFile(Kind kind)
    {
        auto        log     = kind == Kind::PlayLog ? logger::LogFile::Play : logger::LogFile::Event;
        std::string base    = logger.path(log);
        std::string sending = base + ".sending";
        if (fs.exists(sending))
            return sending;

        if (fs.exists(base))
            logger.rotate(log);
        int oldest = 0;
        while (fs.exists(base + "." + std::to_string(oldest + 1)))
            ++oldest;
        if (oldest == 0)
            return {};
        if (!fs.rename(base + "." + std::to_string(oldest), sending))
            return {};
        return sending;
    }

    std::string Reporter::body(Job &job)
    {
        auto now = clock.now();
        switch (job.kind)
        {
            case Kind::PlayLog:
                return playLogReport(config.uuid(), now, fs.readFile(job.sending).value_or(""));
            case Kind::EventLog:
                return eventLogReport(config.uuid(), now, fs.readFile(job.sending).value_or(""));
            case Kind::Inventory:
            {
                std::vector<InventoryObject> objects;
                for (const database::InventoryRow &row : inventory.findAll())
                {
                    bool        complete = row.state == downloader::InventoryState::Complete;
                    std::string file     = cache_dir + "/" + row.file_name + (complete ? "" : ".part");
                    objects.push_back({objectId(row.file_name), row.url, complete ? "COMPLETE" : "TRANSFER",
                                       row.content_type, row.content_length, row.last_update,
                                       fs.fileSize(file).value_or(0)});
                }
                return inventoryReport(config.uuid(), now, system_info ? system_info() : SystemInfo{}, objects);
            }
            case Kind::System:
                return systemReport(now, system_info ? system_info() : SystemInfo{},
                                    {config.uuid(), "garlic-player", std::string(configuration::Configuration::os()),
                                     config.playerName(), config.indexUri()});
        }
        return {};
    }

    /// Logs get a new name per upload (garlic-lib: random UUID start + player UUID end), reports overwrite.
    std::string Reporter::url(const Job &job) const
    {
        if (job.kind != Kind::PlayLog && job.kind != Kind::EventLog)
            return job.subscription.action;
        std::string uuid = config.uuid();
        std::string name = configuration::Configuration::createUuid().substr(0, 23) + (uuid.size() > 23 ? uuid.substr(23) : "");
        return job.subscription.action + (job.kind == Kind::PlayLog ? "/playlog-" : "/event-") + name + ".xml";
    }
}
