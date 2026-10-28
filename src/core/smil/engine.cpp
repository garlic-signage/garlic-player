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
        : document(doc), clock(c), media_source(media), logger(log), config(std::move(cfg)), random(config.random_seed)
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
        startContainer(0);
    }

    void Engine::stop()
    {
        stopChildren(0);
        nodes[0].state = State::Ended;
        ++nodes[0].generation;
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
                    if (nodes[event.node].media)
                        mediaSimpleEnd(event.node);
                    else
                    {
                        stopChildren(event.node); // the dur of the container cuts its children
                        containerSimpleEnd(event.node);
                    }
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

    const std::string &Engine::idOf(std::size_t node) const
    {
        return nodes[node].media ? nodes[node].media->id : nodes[node].container->id;
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

    bool Engine::hasDur(const Timing &timing) const
    {
        return (timing.dur.type == Duration::Type::Clock && timing.dur.value.count() > 0) ||
               timing.dur.type == Duration::Type::Indefinite;
    }

    /// Starts a node; false if it was skipped or is already over.
    bool Engine::activate(std::size_t node)
    {
        if (nodes[node].media)
            return startMedia(node);
        return startContainer(node);
    }

    /**
     * Starts a container, garlic-lib: TSeq/TPar::prepareDurationTimers. Skipped (false) if dur is 0 or "media", or if
     * it has no children and no dur. An empty container with dur is a pause of that length.
     */
    bool Engine::startContainer(std::size_t container)
    {
        Runtime        &c   = nodes[container];
        const Duration &dur = c.container->timing.dur;
        if (dur.type == Duration::Type::Media || (dur.type == Duration::Type::Clock && dur.value.count() <= 0))
            return false;
        if (c.children.empty() && !hasDur(c.container->timing))
            return false;

        c.state             = State::Active;
        c.remaining_repeats = c.container->timing.repeat_count;
        c.starting          = true;
        beginRun(container);
        c.starting = false;
        return c.state == State::Active;
    }

    /// One run of a container: seq and body play their children one after another, par all at once.
    void Engine::beginRun(std::size_t container)
    {
        Runtime &c = nodes[container];
        ++c.generation; // events of the previous run do not count any more
        c.run_start = clock.monotonic();

        const Duration &dur = c.container->timing.dur;
        if (dur.type == Duration::Type::Clock)
            at(c.run_start + dur.value, container, EventType::SimpleEnd);

        switch (c.container->kind)
        {
            case ContainerKind::Body:
            case ContainerKind::Seq:
                c.run_order  = pickChildren(container);
                c.next_child = 0;
                playNext(container);
                break;
            case ContainerKind::Par:
            {
                bool started = false;
                for (std::size_t child : c.children)
                    started = activate(child) || started;
                if (!started)
                    childrenDone(container);
                break;
            }
            case ContainerKind::Excl:
            case ContainerKind::PriorityClass:
                childrenDone(container); // next step
                break;
        }
    }

    /**
     * The children of one seq run: all in order, or with pickNumber N random ones, garlic-lib: TShuffle. They come
     * from a shuffled stack, so every child plays once before one repeats; an empty stack is shuffled anew.
     * Own Fisher-Yates instead of std::shuffle, which shuffles differently on every standard library.
     */
    std::vector<std::size_t> Engine::pickChildren(std::size_t container)
    {
        Runtime &c = nodes[container];
        if (!c.container->pick_number || c.children.empty())
            return c.children;

        std::vector<std::size_t> picked;
        for (int i = 0; i < *c.container->pick_number; ++i)
        {
            if (c.pick_stack.empty())
            {
                c.pick_stack = c.children;
                for (std::size_t n = c.pick_stack.size() - 1; n > 0; --n)
                    std::swap(c.pick_stack[n], c.pick_stack[random() % (n + 1)]);
            }
            picked.push_back(c.pick_stack.back());
            c.pick_stack.pop_back();
        }
        return picked;
    }

    /// seq: starts the next child that can be played; skipped children are passed over at once.
    void Engine::playNext(std::size_t container)
    {
        Runtime &c = nodes[container];
        while (c.next_child < c.run_order.size())
        {
            if (activate(c.run_order[c.next_child++]))
                return; // wait until it ends
        }
        childrenDone(container);
    }

    /// A child has ended by itself, garlic-lib: TSeq/TPar::next.
    void Engine::childEnded(std::size_t container, std::size_t child)
    {
        Runtime &c = nodes[container];
        if (c.state != State::Active)
            return;

        switch (c.container->kind)
        {
            case ContainerKind::Body:
            case ContainerKind::Seq:
                playNext(container);
                break;
            case ContainerKind::Par:
            {
                // endsync, only if no dur decides, garlic-lib: TPar::interruptByEndSync
                const Container &par = *c.container;
                bool by_endsync      = par.endsync == EndSync::First ||
                                  (par.endsync == EndSync::Element && idOf(child) == par.endsync_id);
                if (by_endsync && !hasDur(par.timing))
                {
                    stopChildren(container);
                    containerSimpleEnd(container);
                    return;
                }
                for (std::size_t other : c.children)
                    if (nodes[other].state == State::Active)
                        return; // endsync last: wait for all
                childrenDone(container);
                break;
            }
            case ContainerKind::Excl:
            case ContainerKind::PriorityClass:
                break; // next step
        }
    }

    /// All children are through: with a dur the container waits for it, otherwise its simple duration is over.
    void Engine::childrenDone(std::size_t container)
    {
        if (!hasDur(nodes[container].container->timing))
            containerSimpleEnd(container);
    }

    /// Repeat (repeatCount) or end, garlic-lib: finishedSimpleDuration.
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
        if (container != 0 && !c.starting) // ended while starting: activate() reports it as skipped
            childEnded(c.parent, container);
    }

    /// Stops everything that plays in the container, without telling anybody: when dur or endsync cuts it.
    void Engine::stopChildren(std::size_t container)
    {
        for (std::size_t child : nodes[container].children)
        {
            Runtime &n = nodes[child];
            if (n.state != State::Active)
                continue;
            if (n.media)
                endMedia(child);
            else
            {
                stopChildren(child);
                n.state = State::Ended;
                ++n.generation;
            }
        }
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
        if (!hasDur(nodes[node].media->timing))
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
        childEnded(m.parent, node);
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
