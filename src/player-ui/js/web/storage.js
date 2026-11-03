// The file system of the core (configuration, cache, logs) as seen by the page: kept in IndexedDB, files of the cache
// as URLs for the media, and served to the service worker (sw.js) for web pages and widgets.

const SYNC_INTERVAL = 10000; // ms: the file system is written to IndexedDB that often

const MIME = {
    jpg: 'image/jpeg', jpeg: 'image/jpeg', png: 'image/png', gif: 'image/gif', webp: 'image/webp', svg: 'image/svg+xml',
    ico: 'image/x-icon', bmp: 'image/bmp',
    mp4: 'video/mp4', m4v: 'video/mp4', webm: 'video/webm', ogv: 'video/ogg', mov: 'video/quicktime',
    mp3: 'audio/mpeg', m4a: 'audio/mp4', ogg: 'audio/ogg', oga: 'audio/ogg', wav: 'audio/wav', aac: 'audio/aac',
    html: 'text/html', htm: 'text/html', txt: 'text/plain', css: 'text/css', js: 'text/javascript',
    mjs: 'text/javascript', json: 'application/json', xml: 'application/xml', wasm: 'application/wasm',
    woff: 'font/woff', woff2: 'font/woff2', ttf: 'font/ttf', otf: 'font/otf',
};

export class Storage {
    #core;
    #root;
    #persistent;
    #syncing = false;

    /// root: the directory of the core; persistent: kept in IndexedDB, else only in memory
    constructor(core, root, persistent) {
        this.#core = core;
        this.#root = root;
        this.#persistent = persistent;
    }

    /// Mounts the root (from IndexedDB if persistent) and registers the service worker for web pages and widgets.
    async open() {
        this.#core.FS.mkdir(this.#root);
        if (this.#persistent) {
            this.#core.FS.mount(this.#core.IDBFS, {}, this.#root);
            await this.sync(true);
            setInterval(() => this.sync(), SYNC_INTERVAL);
            addEventListener('pagehide', () => this.sync());
        }
        await this.#startServiceWorker();
    }

    /// populate: read from IndexedDB, else write to it
    sync(populate = false) {
        if (!this.#persistent || this.#syncing)
            return Promise.resolve();
        this.#syncing = true;
        return new Promise((resolve) => this.#core.FS.syncfs(populate, (error) => {
            this.#syncing = false;
            if (error)
                console.warn('IndexedDB sync failed', error);
            resolve();
        }));
    }

    /// A local path of the core as URL for an image, video or audio: the file is read into a blob. A URL (an
    /// uncachable medium) stays as it is. null if the file is missing.
    blobUrl(path) {
        if (Storage.#isUrl(path))
            return path;
        const file = this.#read(path.split('?')[0]);
        return file === null ? null : URL.createObjectURL(new Blob([file.data], { type: file.mime }));
    }

    /// A local path of the core as URL for a web page or widget: served by the service worker, so relative links
    /// (scripts, images of a widget) work. The query of a widget is kept.
    webUrl(path) {
        if (Storage.#isUrl(path))
            return path;
        return new URL(`garlic-fs${encodeURI(path)}`, location.href).href;
    }

    static release(url) {
        if (url?.startsWith('blob:'))
            URL.revokeObjectURL(url);
    }

    static #isUrl(path) {
        return path === '' || /^[a-z][a-z0-9+.-]*:/i.test(path);
    }

    /// {data, mime} or null
    #read(path) {
        try {
            const data = this.#core.FS.readFile(path);
            const extension = path.split('.').pop().toLowerCase();
            return { data, mime: MIME[extension] ?? 'application/octet-stream' };
        } catch (error) {
            console.warn('cannot read', path, error);
            return null;
        }
    }

    /// sw.js asks for files below <root>/cache/ with a message and gets them through a MessageChannel.
    async #startServiceWorker() {
        if (!('serviceWorker' in navigator)) {
            console.warn('no service worker (needs https or localhost): web pages and widgets from the cache cannot be shown');
            return;
        }
        navigator.serviceWorker.addEventListener('message', (event) => {
            if (event.data?.type !== 'garlic-read')
                return;
            const path = event.data.path;
            const file = path.startsWith(`${this.#root}/cache/`) ? this.#read(path) : null;
            event.ports[0].postMessage(file, file ? [file.data.buffer] : []);
        });
        navigator.serviceWorker.startMessages();
        try {
            await navigator.serviceWorker.register('sw.js');
            await navigator.serviceWorker.ready;
        } catch (error) {
            console.warn('service worker not registered:', error);
        }
    }
}
