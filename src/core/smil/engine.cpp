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

#include <algorithm>
#include <utility>

namespace garlic::core::smil
{
    using std::chrono::milliseconds;
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
            const auto &container  = std::get<Container>(node);
            nodes[index].container = &container;
            for (const Node &child : container.children)
                addNode(child, index);
        }
    }

    // ------------------------------------------------------------------ interface for the player

    void Engine::start()
    {
        activate(0);
    }

    void Engine::stop()
    {
        stopChildren(0);
        Runtime &body = nodes[0];
        body.state    = State::Ended;
        ++body.activation;
        ++body.generation;
        schedule = {};
    }

    std::vector<Command> Engine::poll()
    {
        auto now = clock.monotonic();
        while (!schedule.empty() && schedule.top().time <= now)
        {
            Scheduled event = schedule.top();
            schedule.pop();
            Runtime &n         = nodes[event.node];
            bool     per_activation = event.type == EventType::Begin || event.type == EventType::End;
            if (event.generation != (per_activation ? n.activation : n.generation))
                continue; // belongs to an interval or activation that is over

            switch (event.type)
            {
                case EventType::SimpleEnd:
                    if (n.media)
                        mediaSimpleEnd(event.node);
                    else
                    {
                        stopChildren(event.node); // the dur of the container cuts its children
                        containerSimpleEnd(event.node);
                    }
                    break;
                case EventType::Skipped:
                    intervalEnded(event.node);
                    break;
                case EventType::End:
                    endEvent(event.node);
                    break;
                case EventType::Begin:
                    --n.pending_begins;
                    tryStart(event.node, milliseconds(0));
                    if (n.state == State::Waiting && n.pending_begins == 0 && !n.open_begin)
                    {
                        // the last begin was ignored (restart): nothing will come any more
                        n.state = State::Ended;
                        ++n.activation;
                        if (event.node != 0)
                            childEnded(n.parent, event.node);
                    }
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
        bool per_activation = type == EventType::Begin || type == EventType::End;
        schedule.push(Scheduled{time, next_order++, node,
                                per_activation ? nodes[node].activation : nodes[node].generation, type});
    }

    const std::string &Engine::idOf(std::size_t node) const
    {
        return nodes[node].media ? nodes[node].media->id : nodes[node].container->id;
    }

    const Timing &Engine::timingOf(std::size_t node) const
    {
        return nodes[node].media ? nodes[node].media->timing : nodes[node].container->timing;
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

    bool Engine::hasDur(const Timing &timing) const
    {
        return (timing.dur.type == Duration::Type::Clock && timing.dur.value.count() > 0) ||
               timing.dur.type == Duration::Type::Indefinite;
    }

    // ------------------------------------------------------------------ lifecycle of every element

    /**
     * The parent activates the element: a par at the start of its run, a seq when the previous child has ended.
     * begin and end count from now, garlic-lib: activateTriggerTimers, BeginTimer, EndTimer.
     *
     *   begin 0s        start now
     *   begin 5s        start in 5 s
     *   begin -2s       has begun 2 s ago: start now for the rest of its dur, if there is a rest
     *   begin indefinite  wait for a trigger
     *   end 10s         ends in 10 s, whether it plays or still waits
     *   end 0s, -2s     has ended already: does not start
     */
    void Engine::activate(std::size_t node)
    {
        Runtime      &n      = nodes[node];
        const Timing &timing = timingOf(node);
        ++n.activation;
        ++n.generation;
        n.state          = State::Waiting;
        n.pending_begins = 0;
        n.open_begin     = false;
        n.started_once   = false;
        auto now         = clock.monotonic();

        bool has_end = false, ended_already = false;
        for (const TimeCondition &condition : timing.end)
        {
            if (const auto *offset = std::get_if<Offset>(&condition))
            {
                if (offset->value > milliseconds(0))
                {
                    at(now + offset->value, node, EventType::End);
                    has_end = true;
                }
                else
                    ended_already = true;
            }
        }

        bool                        begin_now = false;
        std::optional<milliseconds> begun_ago; // the latest begin in the past
        for (const TimeCondition &condition : timing.begin)
        {
            if (const auto *offset = std::get_if<Offset>(&condition))
            {
                if (offset->value > milliseconds(0))
                {
                    at(now + offset->value, node, EventType::Begin);
                    ++n.pending_begins;
                }
                else if (offset->value == milliseconds(0))
                    begin_now = true;
                else if (!begun_ago || -offset->value < *begun_ago)
                    begun_ago = -offset->value;
            }
            else
                n.open_begin = true; // indefinite; wallclock and triggers follow
        }

        if (ended_already)
        {
            at(now, node, EventType::End); // ends without being shown
            return;
        }
        if (begin_now)
        {
            tryStart(node, milliseconds(0));
            return;
        }
        if (begun_ago)
        {
            // still within its active duration? garlic-lib: BeginTimer::allowByDurTimer
            const Duration &dur = timing.dur;
            bool rest = dur.type == Duration::Type::Indefinite || dur.type == Duration::Type::Media ||
                        (dur.type == Duration::Type::Clock && dur.value > *begun_ago);
            if (rest)
            {
                tryStart(node, *begun_ago);
                return;
            }
        }
        if (n.pending_begins == 0 && !n.open_begin && !has_end)
            skip(node); // can never start: skipped instead of blocking the parent
    }

    /**
     * A begin time has come, garlic-lib: prepareDurationTimers with isRestartable and Expr.
     *   restart="always"         a playing element starts a new interval (new instance)
     *   restart="whenNotActive"  a playing element ignores it
     *   restart="never"          only the first start of this activation
     * expr is checked at every start; false skips the element.
     */
    void Engine::tryStart(std::size_t node, milliseconds elapsed)
    {
        Runtime      &n      = nodes[node];
        const Timing &timing = timingOf(node);
        if (n.state == State::Active)
        {
            if (timing.restart != Restart::Always)
                return;
            stopNode(node); // the current interval ends, a new one begins
        }
        else if (n.started_once && timing.restart == Restart::Never)
            return;

        const std::string &expr = n.media ? n.media->expr : n.container->expr;
        if (!evaluateExpr(expr, ExprContext{clock.now(), config.player_id, config.player_name}))
        {
            skip(node);
            return;
        }

        n.started_once = true;
        if (n.media)
            startMedia(node, elapsed);
        else
            startContainer(node, elapsed);
    }

    /**
     * A skipped element is not shown, but its interval ends after skip_delay like a normal one, garlic-lib:
     * skipElement. So it counts for endsync and a seq of skipped elements does not loop without time.
     */
    void Engine::skip(std::size_t node)
    {
        Runtime &n = nodes[node];
        n.state    = State::Active;
        n.skipping = true;
        n.instance = 0;
        ++n.generation;
        at(clock.monotonic() + config.skip_delay, node, EventType::Skipped);
    }

    /// A time of the end attribute: the element ends for this activation, playing or not; later begins are void.
    void Engine::endEvent(std::size_t node)
    {
        Runtime &n = nodes[node];
        if (n.state == State::Active)
            stopNode(node);
        n.state = State::Ended;
        ++n.activation;
        ++n.generation;
        if (node != 0)
            childEnded(n.parent, node);
    }

    /// An interval is over (dur, end of the medium, skipped). With begin times to come the element waits for them.
    void Engine::intervalEnded(std::size_t node)
    {
        Runtime &n = nodes[node];
        ++n.generation;
        if (n.pending_begins > 0 || n.open_begin)
            n.state = State::Waiting;
        else
        {
            n.state = State::Ended;
            ++n.activation;
        }
        if (node != 0)
            childEnded(n.parent, node);
    }

    /// Stops the current interval without telling anybody: stop command for media, children of containers.
    void Engine::stopNode(std::size_t node)
    {
        Runtime &n = nodes[node];
        if (n.state != State::Active)
            return;
        if (!n.skipping)
        {
            if (n.media)
                endMedia(node);
            else
                stopChildren(node);
        }
        ++n.generation;
    }

    /// repeatCount is ignored while begin times are to come, garlic-lib: !isBeginTimerActive().
    bool Engine::mayRepeat(std::size_t node) const
    {
        const Runtime &n = nodes[node];
        if (n.pending_begins > 0 && timingOf(node).restart != Restart::Never)
            return false;
        const Timing &timing = timingOf(node);
        return timing.repeat_indefinite || n.remaining_repeats > 1;
    }

    // ------------------------------------------------------------------ containers

    /**
     * Starts a container, garlic-lib: TSeq/TPar::prepareDurationTimers. Skipped if dur is 0 or "media", or if it has
     * no children and no dur. An empty container with dur is a pause of that length.
     */
    void Engine::startContainer(std::size_t container, milliseconds elapsed)
    {
        Runtime        &c       = nodes[container];
        const Duration &dur     = c.container->timing.dur;
        bool            no_time = dur.type == Duration::Type::Media ||
                       (dur.type == Duration::Type::Clock && dur.value.count() <= 0);
        if (no_time || (c.children.empty() && !hasDur(c.container->timing)))
        {
            skip(container);
            return;
        }

        c.state             = State::Active;
        c.skipping          = false;
        c.remaining_repeats = c.container->timing.repeat_count;
        beginRun(container, elapsed);
    }

    /// One run of a container: seq and body activate their children one after another, par all at once.
    void Engine::beginRun(std::size_t container, milliseconds elapsed)
    {
        Runtime &c = nodes[container];
        ++c.generation; // events of the previous run do not count any more
        const Duration &dur = c.container->timing.dur;
        if (dur.type == Duration::Type::Clock)
            at(clock.monotonic() + dur.value - elapsed, container, EventType::SimpleEnd);

        switch (c.container->kind)
        {
            case ContainerKind::Body:
            case ContainerKind::Seq:
                c.run_order  = pickChildren(container);
                c.next_child = 0;
                playNext(container);
                break;
            case ContainerKind::Par:
                for (std::size_t child : c.children)
                    activate(child);
                if (c.children.empty()) // a pause with dur
                    childrenDone(container);
                break;
            case ContainerKind::Excl:
            case ContainerKind::PriorityClass:
                childrenDone(container); // next step
                break;
        }
    }

    /**
     * The children of one seq run: all in order, or with pickNumber N random ones, garlic-lib: TShuffle. The children
     * are shuffled once per engine (garlic-lib: once when parsing), then every run takes the next N of this fixed
     * order and starts again from the front at the end - deliberately the same order every round.
     * Own Fisher-Yates instead of std::shuffle, which shuffles differently on every standard library.
     */
    std::vector<std::size_t> Engine::pickChildren(std::size_t container)
    {
        Runtime &c = nodes[container];
        if (!c.container->pick_number || c.children.empty())
            return c.children;

        if (c.pick_order.empty())
        {
            c.pick_order = c.children;
            for (std::size_t n = c.pick_order.size() - 1; n > 0; --n)
                std::swap(c.pick_order[n], c.pick_order[random() % (n + 1)]);
        }

        std::vector<std::size_t> picked;
        for (int i = 0; i < *c.container->pick_number; ++i)
        {
            if (c.pick_next >= c.pick_order.size())
                c.pick_next = 0;
            picked.push_back(c.pick_order[c.pick_next++]);
        }
        return picked;
    }

    /// seq: activates the next child and waits until it has ended; after the last one the children are done.
    void Engine::playNext(std::size_t container)
    {
        Runtime &c = nodes[container];
        if (c.next_child < c.run_order.size())
            activate(c.run_order[c.next_child++]);
        else
            childrenDone(container);
    }

    /**
     * An interval of a child is over (it waits for a later begin) or the child has ended, garlic-lib: TSeq/TPar::next.
     * The seq goes on only when the child has ended; endsync first/id react to every interval, last waits for all.
     */
    void Engine::childEnded(std::size_t container, std::size_t child)
    {
        Runtime &c = nodes[container];
        if (c.state != State::Active)
            return;

        switch (c.container->kind)
        {
            case ContainerKind::Body:
            case ContainerKind::Seq:
                if (nodes[child].state == State::Ended)
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
                    if (nodes[other].state != State::Ended)
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

    /// Repeat (repeatCount) or end of the interval, garlic-lib: finishedSimpleDuration.
    void Engine::containerSimpleEnd(std::size_t container)
    {
        Runtime &c = nodes[container];
        if (mayRepeat(container))
        {
            if (!c.container->timing.repeat_indefinite)
                --c.remaining_repeats;
            beginRun(container, milliseconds(0));
            return;
        }
        intervalEnded(container);
    }

    /// Ends everything in the container, without telling anybody: when dur, endsync, end or a restart cuts it.
    void Engine::stopChildren(std::size_t container)
    {
        for (std::size_t child : nodes[container].children)
        {
            Runtime &n = nodes[child];
            if (n.state != State::Active && n.state != State::Waiting)
                continue;
            stopNode(child);
            n.state = State::Ended;
            ++n.activation; // begin and end times of the child are void
            ++n.generation;
        }
    }

    // ------------------------------------------------------------------ media

    /**
     * Starts a medium, garlic-lib: TImage/TVideo/…::prepareDurationTimers. Skipped (skip_delay) if
     *   - it is a prefetch: only downloaded,
     *   - an image, text, widget, brush or command has no dur that ends it (missing, 0, "media"),
     *   - the MediaSource does not have it.
     * Video and audio without dur play until the player reports the end. elapsed shortens the first dur (begin -2s).
     */
    void Engine::startMedia(std::size_t node, milliseconds elapsed)
    {
        Runtime     &m     = nodes[node];
        const Media &media = *m.media;
        if (media.kind == MediaKind::Prefetch)
        {
            media_source.prefetch(media);
            skip(node);
            return;
        }

        const Duration &dur       = media.timing.dur;
        bool            has_clock = dur.type == Duration::Type::Clock && dur.value.count() > 0;
        bool            intrinsic = media.kind == MediaKind::Video || media.kind == MediaKind::Audio;
        if (!intrinsic && !has_clock && dur.type != Duration::Type::Indefinite)
        {
            skip(node);
            return;
        }

        std::string source;
        if (media.kind != MediaKind::Brush)
        {
            auto resolved = media_source.resolve(media);
            if (!resolved)
            {
                skip(node);
                return;
            }
            source = *resolved;
            if (media.kind == MediaKind::Widget)
                source += media.query;
        }

        m.state             = State::Active;
        m.skipping          = false;
        m.instance          = ++next_instance;
        m.failed            = false;
        m.started_at        = clock.now();
        m.remaining_repeats = media.timing.repeat_count;
        ++m.generation;
        pending.emplace_back(StartMedia{m.instance, &media, std::move(source), regionsOf(media)});
        if (has_clock)
            at(clock.monotonic() + dur.value - elapsed, node, EventType::SimpleEnd);
    }

    /// The player reports the end (or a failure): only counts if no dur decides, garlic-lib: finishIntrinsicDuration.
    void Engine::earlyEnd(std::size_t node)
    {
        if (!hasDur(nodes[node].media->timing))
            mediaSimpleEnd(node);
    }

    /// One pass is over: repeat within the interval (same instance) or end of the interval.
    void Engine::mediaSimpleEnd(std::size_t node)
    {
        Runtime      &m      = nodes[node];
        const Timing &timing = m.media->timing;
        if (mayRepeat(node))
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
        intervalEnded(node);
    }

    /// Stop command and play log of the current instance.
    void Engine::endMedia(std::size_t node)
    {
        Runtime &m = nodes[node];
        pending.emplace_back(StopMedia{m.instance});
        if (!m.failed && !m.media->log_content_id.empty())
            logger.played(m.media->log_content_id, m.started_at);
    }
}
