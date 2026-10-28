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
        schedule.clear();
        frozen.clear();
    }

    std::vector<Command> Engine::poll()
    {
        auto now = clock.monotonic();
        while (!schedule.empty() && schedule.front().time <= now)
        {
            Scheduled event = pop();
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
                    settle(event.node);
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
        return schedule.front().time;
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
        push(Scheduled{time, next_order++, node, per_activation ? nodes[node].activation : nodes[node].generation,
                       type});
    }

    void Engine::push(Scheduled event)
    {
        schedule.push_back(event);
        std::push_heap(schedule.begin(), schedule.end(), std::greater<>{});
    }

    Engine::Scheduled Engine::pop()
    {
        std::pop_heap(schedule.begin(), schedule.end(), std::greater<>{});
        Scheduled event = schedule.back();
        schedule.pop_back();
        return event;
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
            settle(node);
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
                settle(node);
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
     * expr is checked at every start; false skips the element. In an excl the priority rules may defer or ignore it.
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

        if (n.media)
            startMedia(node, elapsed);
        else
            startContainer(node, elapsed);
        if (n.state == State::Active && !n.skipping)
            n.started_once = true;
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
        n.state    = State::Ended;
        n.deferred = false; // a queued entry is void
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

    /// After an ignored begin (restart, excl never): if nothing will come any more, the element has ended.
    void Engine::settle(std::size_t node)
    {
        Runtime &n = nodes[node];
        if (n.state != State::Waiting || n.deferred || n.pending_begins > 0 || n.open_begin)
            return;
        n.state = State::Ended;
        ++n.activation;
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
        if (n.paused) // its frozen events are void
        {
            std::erase_if(frozen, [&](const auto &entry) { return entry.first == node; });
            n.paused = false;
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
     * no children and no dur - unless it has an end. An empty container with dur or end is a pause of that length.
     */
    void Engine::startContainer(std::size_t container, milliseconds elapsed)
    {
        Runtime        &c       = nodes[container];
        const Duration &dur     = c.container->timing.dur;
        bool            has_end = !c.container->timing.end.empty();
        bool            no_time = dur.type == Duration::Type::Media ||
                       (dur.type == Duration::Type::Clock && dur.value.count() <= 0);
        bool            empty   = c.children.empty();
        if (c.container->kind == ContainerKind::Excl) // its children are priorityClasses: empty means no elements
            empty = std::ranges::all_of(c.children, [&](std::size_t pc) { return nodes[pc].children.empty(); });
        if ((no_time && !has_end) || (empty && !hasDur(c.container->timing) && !has_end))
        {
            skip(container);
            return;
        }
        if (!admit(container))
            return;

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
                c.playing.reset();
                c.queue.clear();
                for (std::size_t priority_class : c.children)
                    startPriorityClass(priority_class);
                if (std::ranges::all_of(c.children, [&](std::size_t pc) { return nodes[pc].state == State::Ended; }))
                    childrenDone(container); // no elements: a pause with dur or end
                break;
            case ContainerKind::PriorityClass:
                break; // started by its excl
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
            case ContainerKind::PriorityClass:
            {
                std::size_t excl = c.parent;
                Runtime    &e    = nodes[excl];
                if (e.state != State::Active)
                    return;

                // endsync of the excl reacts to the elements, garlic-lib: TExcl::next
                const Container &x = *e.container;
                bool by_endsync    = x.endsync == EndSync::First ||
                                  (x.endsync == EndSync::Element && idOf(child) == x.endsync_id);
                if (by_endsync && !hasDur(x.timing) && !interrupting)
                {
                    stopChildren(excl);
                    containerSimpleEnd(excl);
                    return;
                }

                if (e.playing == child)
                {
                    e.playing.reset();
                    playFromQueue(excl);
                }

                for (std::size_t other : c.children)
                    if (nodes[other].state != State::Ended)
                        return;
                c.state = State::Ended; // all elements of the class are through
                ++c.generation;
                childEnded(excl, container);
                break;
            }
            case ContainerKind::Excl:
                for (std::size_t other : c.children)
                    if (nodes[other].state != State::Ended)
                        return;
                childrenDone(container); // all classes are through, the queue is empty
                break;
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
            n.state    = State::Ended;
            n.deferred = false;
            ++n.activation; // begin and end times of the child are void
            ++n.generation;
        }
        nodes[container].playing.reset();
        nodes[container].queue.clear();
    }

    // ------------------------------------------------------------------ excl

    /// A priorityClass has no timing: it activates all its elements at once and ends when all have ended.
    void Engine::startPriorityClass(std::size_t priority_class)
    {
        Runtime &pc = nodes[priority_class];
        ++pc.activation;
        ++pc.generation;
        pc.skipping = false;
        if (pc.children.empty())
        {
            pc.state = State::Ended;
            return;
        }
        pc.state = State::Active;
        for (std::size_t child : pc.children)
            activate(child);
    }

    std::size_t Engine::classIndex(std::size_t priority_class) const
    {
        const auto &classes = nodes[nodes[priority_class].parent].children;
        return static_cast<std::size_t>(std::ranges::find(classes, priority_class) - classes.begin());
    }

    /**
     * An element of an excl wants to start, garlic-lib: TExcl::determineContinue. Only one plays; the priorityClass of
     * the playing one decides with peers (same class), higher (new one from a higher class) or lower:
     *   stop   the playing one is stopped, the new one starts
     *   pause  the playing one is frozen and queued, the new one starts
     *   defer  the new one is queued and starts when it is its turn
     *   never  the new one is ignored like a begin ignored by restart
     * Returns whether the element may start now.
     */
    bool Engine::admit(std::size_t node)
    {
        std::size_t pc = nodes[node].parent;
        if (node == 0 || nodes[pc].media || nodes[pc].container->kind != ContainerKind::PriorityClass)
            return true;
        std::size_t excl = nodes[pc].parent;
        Runtime    &e    = nodes[excl];
        if (!e.playing || *e.playing == node)
        {
            e.playing = node;
            return true;
        }

        std::size_t      current     = *e.playing;
        std::size_t      current_pc  = nodes[current].parent;
        const Container &rules       = *nodes[current_pc].container;
        std::size_t      new_index   = classIndex(pc);
        std::size_t      cur_index   = classIndex(current_pc);
        Interrupt        rule        = new_index == cur_index ? rules.peers
                                     : new_index < cur_index ? rules.higher
                                                             : rules.lower;
        switch (rule)
        {
            case Interrupt::Stop:
                // no normal end: it must not start the queue or trigger endsync (garlic-lib: "not to jump to next")
                e.playing    = node;
                interrupting = true;
                stopNode(current);
                intervalEnded(current);
                interrupting = false;
                return true;
            case Interrupt::Pause:
                e.playing = node;
                pauseNode(current);
                e.queue.push_back({current, cur_index, true});
                return true;
            case Interrupt::Defer:
                if (!nodes[node].deferred)
                {
                    nodes[node].deferred = true;
                    e.queue.push_back({node, new_index, false});
                }
                return false;
            case Interrupt::Never:
                return false;
        }
        return false;
    }

    /// The playing element has ended: the next from the queue, highest class first, in a class the last queued.
    void Engine::playFromQueue(std::size_t excl)
    {
        Runtime &e = nodes[excl];
        while (!e.queue.empty() && !e.playing)
        {
            auto best = e.queue.end() - 1;
            for (auto it = e.queue.end() - 1; it != e.queue.begin();)
            {
                --it;
                if (it->priority_class < best->priority_class)
                    best = it;
            }
            Queued next = *best;
            e.queue.erase(best);

            Runtime &n = nodes[next.node];
            if (next.paused && n.paused)
            {
                e.playing = next.node;
                resumeNode(next.node);
            }
            else if (!next.paused && n.deferred)
            {
                n.deferred = false;
                tryStart(next.node, milliseconds(0)); // starts anew, expr is checked again
                settle(next.node);
            }
            // otherwise the entry is stale: the element was stopped or restarted meanwhile
        }
    }

    void Engine::subtree(std::size_t node, std::vector<std::size_t> &out) const
    {
        out.push_back(node);
        for (std::size_t child : nodes[node].children)
            subtree(child, out);
    }

    /**
     * Freezes an element with everything in it, garlic-lib: pauseAllTimers. Its media are paused, its dur times stop
     * and keep their rest. begin and end times go on counting ("pause in enhancedTimer do not stop counting"), but
     * fire only when the element is resumed, at once if they are due by then.
     */
    void Engine::pauseNode(std::size_t node)
    {
        std::vector<std::size_t> members;
        subtree(node, members);
        auto now = clock.monotonic();

        std::vector<Frozen> events;
        std::erase_if(schedule,
                      [&](const Scheduled &event)
                      {
                          if (std::ranges::find(members, event.node) == members.end())
                              return false;
                          events.push_back({event.time - now, event});
                          return true;
                      });
        std::make_heap(schedule.begin(), schedule.end(), std::greater<>{});
        frozen.emplace_back(node, std::move(events));

        for (std::size_t member : members)
            if (nodes[member].media && nodes[member].state == State::Active && !nodes[member].skipping)
                pending.emplace_back(PauseMedia{nodes[member].instance});
        nodes[node].paused = true;
    }

    /// Thaws a paused element: its media resume, dur goes on with its rest, due begin and end times fire now.
    void Engine::resumeNode(std::size_t node)
    {
        auto entry = std::ranges::find_if(frozen, [&](const auto &f) { return f.first == node; });
        if (entry != frozen.end())
        {
            auto now = clock.monotonic();
            for (Frozen &f : entry->second)
            {
                bool fixed   = f.event.type == EventType::Begin || f.event.type == EventType::End;
                f.event.time = fixed ? std::max(f.event.time, now) : now + f.remaining;
                push(f.event);
            }
            frozen.erase(entry);
        }

        std::vector<std::size_t> members;
        subtree(node, members);
        for (std::size_t member : members)
            if (nodes[member].media && nodes[member].state == State::Active && !nodes[member].skipping)
                pending.emplace_back(ResumeMedia{nodes[member].instance});
        nodes[node].paused = false;
    }

    // ------------------------------------------------------------------ media

    /**
     * Starts a medium, garlic-lib: TImage/TVideo/…::prepareDurationTimers. Skipped (skip_delay) if
     *   - it is a prefetch: only downloaded,
     *   - an image, text, widget, brush or command has neither a dur that ends it (missing, 0, "media") nor an end,
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
        if (!intrinsic && !has_clock && dur.type != Duration::Type::Indefinite && media.timing.end.empty())
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
        if (!admit(node))
            return;

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

    /**
     * Stop command and play log of the current instance. If its start has not been handed to the player yet (it ends in
     * the same poll() it began, e.g. displaced at once in an excl), both are dropped: it was never visible.
     */
    void Engine::endMedia(std::size_t node)
    {
        Runtime &m      = nodes[node];
        auto     unseen = std::ranges::find_if(pending,
                                               [&](const Command &command)
                                               {
                                                   const auto *start = std::get_if<StartMedia>(&command);
                                                   return start && start->id == m.instance;
                                               });
        if (unseen != pending.end())
        {
            pending.erase(unseen);
            std::erase_if(pending,
                          [&](const Command &command)
                          {
                              return std::visit([&](const auto &c) { return c.id == m.instance; }, command);
                          });
            return;
        }
        std::erase_if(pending,
                      [&](const Command &command)
                      {
                          // paused or resumed in this poll() and stopped at once: only the stop counts
                          const auto *resume = std::get_if<ResumeMedia>(&command);
                          const auto *pause  = std::get_if<PauseMedia>(&command);
                          return (resume && resume->id == m.instance) || (pause && pause->id == m.instance);
                      });
        pending.emplace_back(StopMedia{m.instance});
        if (!m.failed && !m.media->log_content_id.empty())
            logger.played(m.media->log_content_id, m.started_at);
    }
}
