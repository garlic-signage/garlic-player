// garlic-player in the browser. The core runs as WebAssembly (garlic-core.js, bindings.cpp); the page only shows:
// it polls the core, hands the events to the stage and reports what only it knows (end of a video, a file that cannot
// be played, a click, a key). The same behaviour as the qwidget and qml players: menu with ten fast clicks or holding
// 10 s, settings, debug infos, shortcuts.
//
// URL parameters: index=<SMIL index> (like SMIL_INDEX on the command line), storage=memory (no IndexedDB: the cache
// is gone with the page), uuid=<uuid>&name=<player name> (preview of a player in garlic-hub: the player plays as
// that one and gets its playlist).

import createGarlicCore from './garlic-core.js';
import { DebugDialog, Menu, SettingsDialog } from './dialogs.js';
import { Stage } from './stage.js';
import { Storage } from './storage.js';

const ROOT = '/garlic'; // configuration, cache and logs in the file system of the core

class GarlicPlayer {
    #params = new URLSearchParams(location.search);
    #storage = null;
    #player = null; // Player of bindings.cpp
    #stage = null;
    #menu = null;
    #settings = null;
    #debug = null;
    #timer = null;
    #stopped = false;

    async run() {
        const core = await createGarlicCore();
        this.#storage = new Storage(core, ROOT, this.#params.get('storage') !== 'memory');
        await this.#storage.open();
        const uuid = this.#params.get('uuid');
        if (uuid) {
            const name = this.#params.get('name') ?? '';
            this.#storage.presetConfig(uuid, name);
            document.title = `garlic-player preview: ${name || uuid}`;
        }

        this.#player = new core.Player({
            root: ROOT,
            index: this.#params.get('index') ?? '',
            seed: crypto.getRandomValues(new Uint32Array(1))[0],
        });
        const error = this.#player.start();
        this.#stage = new Stage(document.getElementById('stage'), this.#storage, {
            finished: (id) => this.#call(() => this.#player.mediaFinished(id)),
            failed: (id) => this.#call(() => this.#player.mediaFailed(id)),
            activated: (id) => this.#call(() => this.#player.activated(id)),
        });
        if (error !== '') {
            this.#stopped = true;
            this.#stage.showMessage(`garlic-player cannot start: ${error}`);
            return;
        }

        this.#storage.sync(); // the configuration with an index from the URL, at once and not only after 10 s
        this.#createDialogs();
        document.addEventListener('keydown', (event) => this.#keyPressed(event));
        document.addEventListener('fullscreenchange', () => this.#updateCursor());

        // without an index nothing can be played: the settings first, cancelled ends the player (garlic-lib)
        if (this.#player.indexUri() === '')
            this.#settings.open(true);
        this.#tick();
    }

    #createDialogs() {
        const changed = () => this.#updateCursor();
        this.#settings = new SettingsDialog(document.getElementById('settings'), this.#player, {
            stored: () => {
                this.#poke();
                this.#storage.sync();
            },
            cancelledAtStart: () => this.#quit(),
            changed,
        });
        this.#debug = new DebugDialog(document.getElementById('debug'), this.#player, {
            playing: () => this.#stage.playing(),
            quit: () => this.#quit(),
            changed,
        });
        this.#menu = new Menu(document.getElementById('menu'), {
            settings: () => this.#settings.open(),
            debug: () => this.#debug.open(),
            exit: () => this.#quit(),
            changed,
        }, () => this.#settings.isOpen || this.#debug.isOpen);
    }

    // ------------------------------------------------------------------ loop

    /// poll() of the core, then the next one when the core needs it (at once after input)
    #tick() {
        this.#timer = null;
        if (this.#stopped)
            return;
        for (const event of this.#player.poll())
            this.#handle(event);
        const next = this.#player.msUntilNext();
        if (next >= 0)
            this.#schedule(next);
    }

    #schedule(ms) {
        if (this.#stopped)
            return;
        clearTimeout(this.#timer);
        this.#timer = setTimeout(() => this.#tick(), ms);
    }

    /// after a call into the core: its answer comes with the next poll()
    #poke() {
        this.#schedule(0);
    }

    #call(action) {
        action();
        this.#poke();
    }

    #handle(event) {
        switch (event.type) {
        // no cache API for the page; the core restarts the player through the supervisor (none in the browser)
        case 'clearCaches': this.#call(() => this.#player.cachesCleared()); break;
        // the browser cannot take a picture of the screen: the core answers without one
        case 'screenshot': this.#call(() => this.#player.screenshotTaken(event.path)); break;
        case 'layout':
            // as preview in an overlay of garlic-hub: the page around gets the size of the layout for its proportions
            if (window.parent !== window)
                window.parent.postMessage({ type: 'garlic-preview-layout', width: event.rootWidth, height: event.rootHeight }, location.origin);
            this.#stage.handle(event);
            break;
        default: this.#stage.handle(event);
        }
    }

    // ------------------------------------------------------------------ keys, cursor, end

    #keyPressed(event) {
        if (event.ctrlKey && !event.altKey && !event.metaKey) {
            const actions = {
                f: () => this.#toggleFullScreen(),
                b: () => this.#toggleFullScreen(), // over all screens is not possible in the browser
                s: () => this.#settings.open(),
                d: () => this.#debug.open(),
                q: () => this.#quit(),
            };
            const action = actions[event.key.toLowerCase()];
            if (action) {
                event.preventDefault();
                if (!this.#settings.isOpen && !this.#debug.isOpen)
                    action();
            }
            return;
        }
        // every other key is an accesskey for the SMIL
        if (!this.#dialogOpen() && !event.altKey && !event.metaKey && event.key.length === 1)
            this.#call(() => this.#player.accessKey(event.key));
    }

    #dialogOpen() {
        return this.#settings.isOpen || this.#debug.isOpen || this.#menu.isOpen;
    }

    #toggleFullScreen() {
        if (document.fullscreenElement)
            document.exitFullscreen();
        else
            document.documentElement.requestFullscreen().catch((error) => console.warn('no full screen:', error.message));
    }

    /// hidden in full screen, shown for the menu and the dialogs
    #updateCursor() {
        document.body.classList.toggle('hide-cursor', document.fullscreenElement !== null && !this.#dialogOpen());
    }

    #quit() {
        if (this.#stopped)
            return;
        this.#player.stop();
        this.#tick(); // the StopMedia of the stop
        this.#stopped = true;
        clearTimeout(this.#timer);
        this.#storage.sync().then(() => {
            this.#stage.showMessage('garlic-player stopped');
            window.close(); // only works for a window opened by a script
        });
    }
}

new GarlicPlayer().run();
