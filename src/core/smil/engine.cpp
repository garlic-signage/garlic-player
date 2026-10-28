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
#include "smil/engine.hpp"

#include <utility>

namespace garlic::core::smil
{
    using std::chrono::steady_clock;

    Engine::Engine(const Document &doc, const common::Clock &c, MediaSource &media, logger::Logger &log,
                   EngineConfig cfg)
        : document(doc), clock(c), media_source(media), logger(log), config(std::move(cfg))
    {
        Runtime body;
        body.container = &document.body;
        nodes.push_back(body);
        for (const Node &child : document.body.children)
            addNode(child, 0);
    }

    /// One Runtime per node of the tree, in the order of the tree; children know their parent.
    void Engine::addNode(const Node &node, std::size_t parent)
    {
        std::size_t index = nodes.size();
        nodes.emplace_back();
        nodes[index].parent = parent;
        nodes[parent].children.push_back(index);

        if (const auto *media = std::get_if<Media>(&node))
            nodes[index].media = media;
        else
        {
            const auto &container     = std::get<Container>(node);
            nodes[index].container = &container;
            for (const Node &child : container.children)
                addNode(child, index);
        }
    }

    // ------------------------------------------------------------------ interface for the player

    void Engine::start()
    {
        beginRun(0);
    }

    void Engine::stop()
    {
        for (std::size_t i = 0; i < nodes.size(); ++i)
        {
            if (nodes[i].media && nodes[i].state == State::Active)
                endMedia(i); // stop command and play log, without going on in the parent
            nodes[i].state = State::Ended;
            ++nodes[i].generation;
        }
        schedule = {};
    }

    std::vector<Command> Engine::poll()
    {
        auto now = clock.monotonic();
        while (!schedule.empty() && schedule.top().time <= now)
        {
            Scheduled event = schedule.top();
            schedule.pop();
            if (event.generation != nodes[event.node].generation)
                continue; // the node has ended or started again since then

            switch (event.type)
            {
                case EventType::SimpleEnd:
                    mediaSimpleEnd(event.node);
                    break;
                case EventType::Run:
                    beginRun(event.node);
                    break;
            }
        }
        return std::exchange(pending, {});
    }

    std::optional<steady_clock::time_point> Engine::nextDeadline() const
    {
        if (!pending.empty())
            return clock.monotonic();
        if (schedule.empty())
            return std::nullopt;
        return schedule.top().time;
    }

    void Engine::mediaFinished(InstanceId id)
    {
        if (auto node = findInstance(id))
            earlyEnd(*node);
    }

    void Engine::mediaFailed(InstanceId id)
    {
        if (auto node = findInstance(id))
        {
            nodes[*node].failed = true;
            earlyEnd(*node);
        }
    }

    // ------------------------------------------------------------------ helpers

    void Engine::at(steady_clock::time_point time, std::size_t node, EventType type)
    {
        schedule.push(Scheduled{time, next_order++, node, nodes[node].generation, type});
    }

    std::optional<std::size_t> Engine::findInstance(InstanceId id) const
    {
        for (std::size_t i = 0; i < nodes.size(); ++i)
            if (nodes[i].media && nodes[i].instance == id && nodes[i].state == State::Active)
                return i;
        return std::nullopt;
    }

    /**
     * All regions whose regionName (or id, if it has no regionName) is the region of the medium, like the
     * RegionsList of the old player. Unknown region: the first region, i.e. the lowest z-index.
     */
    std::vector<std::size_t> Engine::regionsOf(const Media &media) const
    {
        std::vector<std::size_t> found;
        const auto              &regions = document.head.regions;
        for (std::size_t i = 0; i < regions.size(); ++i)
        {
            const std::string &name = regions[i].region_name.empty() ? regions[i].id : regions[i].region_name;
            if (name == media.region)
                found.push_back(i);
        }
        if (found.empty())
            found.push_back(0);
        return found;
    }

    // ------------------------------------------------------------------ containers

    /// Starts a node; false if it was skipped and is already over.
    bool Engine::activate(std::size_t node)
    {
        if (nodes[node].media)
            return startMedia(node);
        return false; // seq, par, excl inside the body: next step
    }

    /// One run of a container: the body plays its children one after another like a seq.
    void Engine::beginRun(std::size_t container)
    {
        Runtime &c = nodes[container];
        if (c.state != State::Active)
        {
            c.state             = State::Active;
            c.remaining_repeats = c.container->timing.repeat_count;
        }
        c.run_start  = clock.monotonic();
        c.next_child = 0;
        playNext(container);
    }

    /// Starts the next child that can be played; skipped children are passed over at once.
    void Engine::playNext(std::size_t container)
    {
        while (nodes[container].next_child < nodes[container].children.size())
        {
            std::size_t child = nodes[container].children[nodes[container].next_child++];
            if (activate(child))
                return; // wait until it ends
        }
        containerSimpleEnd(container);
    }

    void Engine::childEnded(std::size_t container)
    {
        if (nodes[container].state == State::Active)
            playNext(container);
    }

    /// All children are through: repeat (repeatCount) or end, garlic-lib: finishedSimpleDuration.
    void Engine::containerSimpleEnd(std::size_t container)
    {
        Runtime &c      = nodes[container];
        bool     repeat = c.container->timing.repeat_indefinite || c.remaining_repeats > 1;
        if (repeat)
        {
            if (!c.container->timing.repeat_indefinite)
                --c.remaining_repeats;
            if (clock.monotonic() == c.run_start) // nothing was played: do not loop within this poll()
                at(clock.monotonic() + config.empty_run_retry, container, EventType::Run);
            else
                beginRun(container);
            return;
        }

        c.state = State::Ended;
        ++c.generation;
        if (container != 0)
            childEnded(c.parent);
    }

    // ------------------------------------------------------------------ media

    /**
     * Starts a medium, garlic-lib: TImage/TVideo/…::prepareDurationTimers. Skipped (false) if
     *   - it is a prefetch: only downloaded,
     *   - an image, text, widget, brush or command has no dur that ends it (missing, 0, "media"),
     *   - the MediaSource does not have it.
     * Video and audio without dur play until the player reports the end.
     */
    bool Engine::startMedia(std::size_t node)
    {
        Runtime     &m     = nodes[node];
        const Media &media = *m.media;
        if (media.kind == MediaKind::Prefetch)
        {
            media_source.prefetch(media);
            return false;
        }

        const Duration &dur        = media.timing.dur;
        bool            has_clock  = dur.type == Duration::Type::Clock && dur.value.count() > 0;
        bool            intrinsic  = media.kind == MediaKind::Video || media.kind == MediaKind::Audio;
        if (!intrinsic && !has_clock && dur.type != Duration::Type::Indefinite)
            return false;

        std::string source;
        if (media.kind != MediaKind::Brush)
        {
            auto resolved = media_source.resolve(media);
            if (!resolved)
                return false;
            source = *resolved;
            if (media.kind == MediaKind::Widget)
                source += media.query;
        }

        m.state             = State::Active;
        m.instance          = ++next_instance;
        m.failed            = false;
        m.started_at        = clock.now();
        m.remaining_repeats = media.timing.repeat_count;
        pending.emplace_back(StartMedia{m.instance, &media, std::move(source), regionsOf(media)});
        if (has_clock)
            at(clock.monotonic() + dur.value, node, EventType::SimpleEnd);
        return true;
    }

    /// The player reports the end (or a failure): only counts if no dur decides, garlic-lib: finishIntrinsicDuration.
    void Engine::earlyEnd(std::size_t node)
    {
        const Duration &dur = nodes[node].media->timing.dur;
        bool decided_by_dur = (dur.type == Duration::Type::Clock && dur.value.count() > 0) ||
                              dur.type == Duration::Type::Indefinite;
        if (!decided_by_dur)
            mediaSimpleEnd(node);
    }

    /// One pass is over: repeat or end, garlic-lib: finishedSimpleDuration.
    void Engine::mediaSimpleEnd(std::size_t node)
    {
        Runtime     &m      = nodes[node];
        const Timing &timing = m.media->timing;
        if (timing.repeat_indefinite || m.remaining_repeats > 1)
        {
            if (!timing.repeat_indefinite)
                --m.remaining_repeats;
            m.failed = false;
            pending.emplace_back(RepeatMedia{m.instance});
            if (timing.dur.type == Duration::Type::Clock && timing.dur.value.count() > 0)
                at(clock.monotonic() + timing.dur.value, node, EventType::SimpleEnd);
            return;
        }
        endMedia(node);
        childEnded(m.parent);
    }

    /// Stop command and play log; the parent is not told here.
    void Engine::endMedia(std::size_t node)
    {
        Runtime &m = nodes[node];
        pending.emplace_back(StopMedia{m.instance});
        if (!m.failed && !m.media->log_content_id.empty())
            logger.played(m.media->log_content_id, m.started_at);
        m.state = State::Ended;
        ++m.generation;
    }
}
