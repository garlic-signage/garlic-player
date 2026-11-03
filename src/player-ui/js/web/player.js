// garlic-player in the browser. The core runs as WebAssembly (garlic-core.js, bindings.cpp); this file only shows:
// it polls the core, turns its events into regions and media of the page and reports what only it knows (end of a
// video, a file that cannot be played, a click, a key). The same behaviour as the qwidget and qml players: menu with
// ten fast clicks or holding 10 s, settings, debug infos, shortcuts.
//
// URL parameters: index=<SMIL index> (like SMIL_INDEX on the command line), storage=memory (no IndexedDB: the cache
// is gone with the page, e.g. for a preview).

import createGarlicCore from './garlic-core.js';

const ROOT = '/garlic';          // configuration, cache and logs in the file system of the core
const SYNC_INTERVAL = 10000;     // ms: the file system is written to IndexedDB that often
const MENU_CLICKS = 10;
const MENU_CLICK_GAP = 500;      // ms between the fast clicks
const MENU_HOLD = 10000;         // ms

const params = new URLSearchParams(location.search);
const persistent = params.get('storage') !== 'memory';

const stage = document.getElementById('stage');
const menu = document.getElementById('menu');
const settingsDialog = document.getElementById('settings');
const debugDialog = document.getElementById('debug');

let core = null;
let player = null;
let timer = null;
let syncing = false;
let stopped = false;

let regions = [];                // {element, imageSrc, media: [item]}; item: {id, kind, element, primary, paused, url}
const instances = new Map();     // instance → items, one per region of the instance
let rootImageSrc = '';
let backgroundUrls = [];

// ------------------------------------------------------------------ file system

function sync(populate) {
    if (!persistent || syncing)
        return Promise.resolve();
    syncing = true;
    return new Promise((resolve) => core.FS.syncfs(populate, (error) => {
        syncing = false;
        if (error)
            console.warn('IndexedDB sync failed', error);
        resolve();
    }));
}

const MIME = {
    jpg: 'image/jpeg', jpeg: 'image/jpeg', png: 'image/png', gif: 'image/gif', webp: 'image/webp', svg: 'image/svg+xml',
    mp4: 'video/mp4', m4v: 'video/mp4', webm: 'video/webm', ogv: 'video/ogg', mov: 'video/quicktime',
    mp3: 'audio/mpeg', m4a: 'audio/mp4', ogg: 'audio/ogg', oga: 'audio/ogg', wav: 'audio/wav', aac: 'audio/aac',
    html: 'text/html', htm: 'text/html', txt: 'text/plain',
};

/// A local path of the core as URL for the page: the file is read from the file system of the core into a blob. A URL
/// (an uncachable medium) stays as it is. null if the file is missing.
function urlOf(path) {
    if (path === '' || /^[a-z][a-z0-9+.-]*:/i.test(path))
        return path;
    const file = path.split('?')[0];
    try {
        const data = core.FS.readFile(file);
        const extension = file.split('.').pop().toLowerCase();
        return URL.createObjectURL(new Blob([data], { type: MIME[extension] ?? '' }));
    } catch (error) {
        console.warn('cannot read', file, error);
        return null;
    }
}

function release(url) {
    if (url && url.startsWith('blob:'))
        URL.revokeObjectURL(url);
}

// ------------------------------------------------------------------ loop

/// poll() of the core, then the next one when the core needs it (or after input)
function tick() {
    timer = null;
    if (stopped)
        return;
    for (const event of player.poll())
        handle(event);
    const next = player.msUntilNext();
    if (next >= 0)
        schedule(next);
}

function schedule(ms) {
    if (stopped)
        return;
    clearTimeout(timer);
    timer = setTimeout(tick, ms);
}

/// after a call into the core: its answer comes with the next poll()
function poke() {
    schedule(0);
}

function handle(event) {
    switch (event.type) {
    case 'layout': newLayout(event); break;
    case 'background': backgroundImage(event); break;
    case 'start': start(event); break;
    case 'stop': stop(event.id); break;
    case 'repeat': forEach(event.id, (item) => replay(item)); break;
    case 'pause': forEach(event.id, (item) => setPaused(item, true)); break;
    case 'resume':
        forEach(event.id, (item, region) => {
            setPaused(item, false);
            raise(region, item);
        });
        break;
    // no cache API for the page; the core restarts the player through the supervisor (none in the browser)
    case 'clearCaches': player.cachesCleared(); poke(); break;
    // the browser cannot take a picture of the screen: the core answers without one
    case 'screenshot': player.screenshotTaken(event.path); poke(); break;
    }
}

// ------------------------------------------------------------------ layout

const REPEAT = { repeatx: 'repeat-x', repeaty: 'repeat-y', norepeat: 'no-repeat' };

function setBackground(element, path) {
    const url = path ? urlOf(path) : '';
    if (url)
        backgroundUrls.push(url);
    element.style.backgroundImage = url ? `url("${url}")` : '';
}

/// A new index: the old media are stopped already (StopMedia come first), so only the regions go.
function newLayout(layout) {
    instances.clear();
    backgroundUrls.forEach(release);
    backgroundUrls = [];
    stage.replaceChildren();
    stage.style.backgroundColor = layout.rootColor;
    stage.style.backgroundRepeat = REPEAT[layout.rootRepeat] ?? 'repeat';
    rootImageSrc = layout.rootImageSrc;
    setBackground(stage, layout.rootImage);

    regions = layout.regions.map((definition) => {
        const element = document.createElement('div');
        element.className = 'region';
        element.style.left = `${definition.left * 100}%`;
        element.style.top = `${definition.top * 100}%`;
        element.style.width = `${definition.width * 100}%`;
        element.style.height = `${definition.height * 100}%`;
        element.style.backgroundColor = definition.color;
        element.style.backgroundRepeat = REPEAT[definition.repeat] ?? 'repeat';
        setBackground(element, definition.image);
        stage.append(element); // sorted by z-index: the later lies on top
        return { element, imageSrc: definition.imageSrc, media: [] };
    });
}

function backgroundImage(image) {
    if (image.src === rootImageSrc)
        setBackground(stage, image.path);
    for (const region of regions)
        if (region.imageSrc === image.src)
            setBackground(region.element, image.path);
}

// ------------------------------------------------------------------ media

function horizontal(align) {
    if (align.endsWith('right')) return 'right';
    if (align === 'center' || align.endsWith('mid')) return 'center';
    return 'left';
}

function vertical(align) {
    if (align.startsWith('bottom')) return 'bottom';
    if (align === 'center' || align.startsWith('mid')) return 'center';
    return 'top';
}

/// fit of SMIL → object-fit; meetBest is meet, but never larger than the original
const IMAGE_FIT = { fill: 'fill', meet: 'contain', meetbest: 'scale-down', slice: 'cover' };

function finished(item) {
    if (item.primary && !item.done) {
        item.done = true;
        player.mediaFinished(item.id);
        poke();
    }
}

function failed(item, message) {
    console.warn('media failed:', message);
    if (item.primary && !item.done) {
        item.done = true;
        player.mediaFailed(item.id);
        poke();
    }
}

/// Plays video and audio. Browsers block sound without a user gesture: a video then plays muted (a kiosk browser can
/// allow it, e.g. Chromium with --autoplay-policy=no-user-gesture-required).
function play(item) {
    item.element.play().catch((error) => {
        if (error.name === 'NotAllowedError' && item.kind === 'video' && !item.element.muted) {
            console.warn('sound blocked by the browser, the video plays muted');
            item.element.muted = true;
            play(item);
        } else if (error.name !== 'AbortError') {
            failed(item, `${error.name}: ${error.message}`);
        }
    });
}

function createElement(item, event, region) {
    const kind = event.kind;
    let element;
    if (kind === 'image') {
        element = document.createElement('img');
        element.style.objectFit = IMAGE_FIT[region.fit] ?? 'none';
        element.style.objectPosition = `${horizontal(region.align)} ${vertical(region.align)}`;
        element.onerror = () => failed(item, `cannot load image ${event.source}`);
        element.src = item.url;
    } else if (kind === 'video' || kind === 'audio') {
        element = document.createElement(kind);
        element.volume = region.volume;
        element.style.objectFit = region.fit === 'fill' ? 'fill' : region.fit === 'slice' ? 'cover' : 'contain';
        element.onended = () => finished(item);
        element.onerror = () => failed(item, element.error ? element.error.message || `media error ${element.error.code}` : 'invalid media');
        element.src = item.url;
    } else if (kind === 'web') {
        element = document.createElement('iframe');
        element.onload = () => {
            try { // only possible for pages of this origin (from the cache)
                element.contentDocument.body.style.overflow = 'hidden';
            } catch (error) { /* another origin */ }
        };
        element.src = item.url;
    } else { // brush
        element = document.createElement('div');
        element.style.backgroundColor = event.color;
    }
    element.className = 'media';
    if (kind !== 'web') // web pages handle their clicks themselves
        element.addEventListener('click', () => {
            if (!element.hidden) {
                player.activated(item.id);
                poke();
            }
        });
    return element;
}

function start(event) {
    stop(event.id); // the same instance again: start anew
    if (event.kind === 'command') { // garlic-lib only knew adapi:blankScreen, which needs the launcher
        if (event.source.toLowerCase() === 'adapi:blankscreen')
            console.info('blankScreen: no launcher, the screen stays on');
        else {
            player.mediaFailed(event.id);
            poke();
        }
        return;
    }

    const items = [];
    instances.set(event.id, items);
    for (const definition of event.regions) {
        const region = regions[definition.index];
        if (!region)
            continue;
        const item = { id: event.id, kind: event.kind, label: event.label, primary: items.length === 0, paused: false, done: false };
        item.url = event.kind === 'brush' ? '' : urlOf(event.source);
        if (item.url === null) {
            failed(item, `missing in the cache: ${event.source}`);
            continue;
        }
        item.element = createElement(item, event, definition);
        region.element.append(item.element);
        region.media.push(item);
        items.push(item);
        updateOnTop(region);
        if (item.kind === 'video' || item.kind === 'audio')
            play(item);
    }
}

function stop(id) {
    const items = instances.get(id);
    if (!items)
        return;
    instances.delete(id);
    for (const region of regions) {
        const before = region.media.length;
        region.media = region.media.filter((item) => {
            if (item.id !== id)
                return true;
            if (item.kind === 'video' || item.kind === 'audio') {
                item.element.pause();
                item.element.removeAttribute('src');
                item.element.load();
            }
            item.element.remove();
            release(item.url);
            return false;
        });
        if (region.media.length !== before)
            updateOnTop(region);
    }
}

function forEach(id, action) {
    for (const region of regions)
        for (const item of region.media)
            if (item.id === id)
                action(item, region);
}

function replay(item) {
    if (item.kind !== 'video' && item.kind !== 'audio')
        return;
    item.done = false;
    item.element.currentTime = 0;
    play(item);
}

function setPaused(item, paused) {
    item.paused = paused;
    if (item.kind === 'video' || item.kind === 'audio') {
        if (paused)
            item.element.pause();
        else
            play(item);
    }
    if (item.kind === 'web')
        updateOnTop(regions.find((region) => region.media.includes(item)));
}

/// a resumed medium comes to the top again
function raise(region, item) {
    region.media = region.media.filter((each) => each !== item);
    region.media.push(item);
    region.element.append(item.element);
    updateOnTop(region);
}

/// Only the topmost medium with a picture is visible; audio has none and covers nothing.
function updateOnTop(region) {
    if (!region)
        return;
    const top = region.media.findLast((item) => item.kind !== 'audio');
    for (const item of region.media)
        item.element.hidden = item.kind === 'audio' || item !== top || (item.kind === 'web' && item.paused);
}

// ------------------------------------------------------------------ menu, dialogs, keys

function dialogOpen() {
    return settingsDialog.open || debugDialog.open || !menu.hidden;
}

function updateCursor() {
    document.body.classList.toggle('hide-cursor', document.fullscreenElement !== null && !dialogOpen());
}

function toggleFullScreen() {
    if (document.fullscreenElement)
        document.exitFullscreen();
    else
        document.documentElement.requestFullscreen().catch((error) => console.warn('no full screen:', error.message));
}

function openMenu(x, y) {
    menu.hidden = false;
    menu.style.left = `${Math.min(x, window.innerWidth - menu.offsetWidth)}px`;
    menu.style.top = `${Math.min(y, window.innerHeight - menu.offsetHeight)}px`;
    menu.querySelector('button').focus();
    updateCursor();
}

function closeMenu() {
    menu.hidden = true;
    updateCursor();
}

menu.addEventListener('click', (event) => {
    const button = event.target.closest('button');
    if (!button)
        return;
    closeMenu();
    if (button.dataset.action === 'settings') openSettings(false);
    else if (button.dataset.action === 'debug') openDebug();
    else quit();
});

// Ten fast clicks or touches (less than 500 ms apart) or one held for 10 s anywhere open the menu, like in the other
// players. Clicks into a web page (iframe) do not reach the page of the player.
let fastClicks = 0;
let releasedAt = 0;
let holdTimer = null;

document.addEventListener('pointerdown', (event) => {
    if (dialogOpen()) {
        if (!menu.hidden && !menu.contains(event.target))
            closeMenu();
        return;
    }
    const now = performance.now();
    fastClicks = now - releasedAt < MENU_CLICK_GAP ? fastClicks + 1 : 1;
    clearTimeout(holdTimer);
    if (fastClicks >= MENU_CLICKS) {
        fastClicks = 0;
        openMenu(event.clientX, event.clientY);
    } else {
        holdTimer = setTimeout(() => openMenu(event.clientX, event.clientY), MENU_HOLD);
    }
}, true);

document.addEventListener('pointerup', () => {
    releasedAt = performance.now();
    clearTimeout(holdTimer);
}, true);

document.addEventListener('keydown', (event) => {
    if (event.key === 'Escape' && !menu.hidden) {
        closeMenu();
        return;
    }
    if (event.ctrlKey && !event.altKey && !event.metaKey) {
        const actions = {
            f: toggleFullScreen,
            b: toggleFullScreen, // over all screens is not possible in the browser
            s: () => openSettings(false),
            d: openDebug,
            q: quit,
        };
        const action = actions[event.key.toLowerCase()];
        if (action) {
            event.preventDefault();
            if (!settingsDialog.open && !debugDialog.open)
                action();
        }
        return;
    }
    // every other key is an accesskey for the SMIL
    if (!dialogOpen() && !event.altKey && !event.metaKey && event.key.length === 1) {
        player.accessKey(event.key);
        poke();
    }
});

document.addEventListener('fullscreenchange', updateCursor);

// --- settings: reading, checking and storing is done by the core; the dialog closes only when the core has stored
// them, otherwise it shows the error text of the core

let settingsAtStart = false; // at the start without index: cancelled ends the player

function openSettings(atStart) {
    settingsAtStart = atStart;
    const current = player.settings();
    const form = settingsDialog.querySelector('form');
    form.playerName.value = current.playerName;
    form.indexUri.value = current.indexUri;
    form.httpdEnabled.checked = current.httpdEnabled;
    form.httpdPort.value = current.httpdPort;
    showError('');
    settingsDialog.showModal();
    updateCursor();
}

function showError(message) {
    const error = settingsDialog.querySelector('.error');
    error.textContent = message;
    error.hidden = message === '';
}

settingsDialog.querySelector('form').addEventListener('submit', (event) => {
    if (event.submitter?.value !== 'ok')
        return;
    const form = event.target;
    const message = player.changeSettings({
        playerName: form.playerName.value.trim(),
        indexUri: form.indexUri.value.trim(),
        httpdEnabled: form.httpdEnabled.checked,
        httpdPort: Number(form.httpdPort.value) || 0,
    });
    poke();
    if (message !== '') {
        event.preventDefault();
        showError(message);
        return;
    }
    settingsDialog.returnValue = 'ok';
    sync(false);
});

settingsDialog.addEventListener('close', () => {
    updateCursor();
    if (settingsAtStart && settingsDialog.returnValue !== 'ok')
        quit();
    settingsAtStart = false;
});

// --- debug infos: every second the resources from the core and the media that play now, one line per region

let debugTimer = null;

function formatBytes(bytes) {
    const units = ['B', 'KiB', 'MiB', 'GiB', 'TiB'];
    let value = bytes;
    let unit = 0;
    while (value >= 1024 && unit < units.length - 1) {
        value /= 1024;
        ++unit;
    }
    return `${value.toFixed(unit === 0 ? 0 : 2)} ${units[unit]}`;
}

function freeOf(free, total) {
    return free === null || total === null ? '-' : `${formatBytes(free)} free of ${formatBytes(total)}`;
}

function refreshDebug() {
    const info = player.debugInfo();
    const playing = [];
    regions.forEach((region, index) => {
        for (const item of region.media)
            playing.push(`${index}: ${item.label}`);
    });
    const values = {
        title: info.title || '-',
        userAgent: info.userAgent,
        disk: freeOf(info.diskFree, info.diskTotal),
        memory: freeOf(info.memoryFree, info.memoryTotal),
        playerMemory: info.memoryUsed === 0 ? '-'
            : `${formatBytes(info.memoryUsed)}, max ${formatBytes(info.memoryMax)} at ${info.memoryMaxTime}`,
        threads: info.threads === 0 ? '-' : `${info.threads}, max ${info.threadsMax}`,
        playing: playing.length > 0 ? playing.join('\n') : '-',
    };
    for (const [key, value] of Object.entries(values))
        debugDialog.querySelector(`[data-info="${key}"]`).textContent = value;
}

function openDebug() {
    refreshDebug();
    debugDialog.showModal();
    debugTimer = setInterval(refreshDebug, 1000);
    updateCursor();
}

debugDialog.addEventListener('close', () => {
    clearInterval(debugTimer);
    updateCursor();
    if (debugDialog.returnValue === 'quit')
        quit();
});

// ------------------------------------------------------------------ start and end

function quit() {
    if (stopped)
        return;
    player.stop();
    tick(); // the StopMedia of the stop
    stopped = true;
    clearTimeout(timer);
    sync(false).then(() => {
        stage.replaceChildren();
        stage.innerHTML = '<div class="stopped">garlic-player stopped</div>';
        window.close(); // only works for a window opened by a script
    });
}

async function main() {
    core = await createGarlicCore();
    core.FS.mkdir(ROOT);
    if (persistent) {
        core.FS.mount(core.IDBFS, {}, ROOT);
        await sync(true);
        setInterval(() => sync(false), SYNC_INTERVAL);
        addEventListener('pagehide', () => sync(false));
    }

    player = new core.Player({
        root: ROOT,
        index: params.get('index') ?? '',
        seed: crypto.getRandomValues(new Uint32Array(1))[0],
    });
    const error = player.start();
    if (error !== '') {
        stage.innerHTML = '';
        const message = document.createElement('div');
        message.className = 'stopped';
        message.textContent = `garlic-player cannot start: ${error}`;
        stage.append(message);
        stopped = true;
        return;
    }

    // without an index nothing can be played: the settings first, cancelled ends the player (garlic-lib)
    if (player.indexUri() === '')
        openSettings(true);
    tick();
}

main();
