// What the player shows: the regions of the current layout and the media in them. The timing is the business of the
// core; a medium only shows itself and reports what only the page knows (the end of a video, a file that cannot be
// played, a click) through the callbacks {finished, failed, activated}.

import { Storage } from './storage.js';

const REPEAT = { repeatx: 'repeat-x', repeaty: 'repeat-y', norepeat: 'no-repeat' };

/// fit of SMIL → object-fit; meetBest is meet, but never larger than the original
const IMAGE_FIT = { fill: 'fill', meet: 'contain', meetbest: 'scale-down', slice: 'cover' };

/// One medium in one region, from StartMedia to StopMedia.
class Media {
    id;
    kind;     // image, video, audio, web, brush
    label;    // title or src, for the debug dialog
    primary;  // only the first region of an instance reports end and errors
    paused = false;
    element = null;

    #url = null;
    #done = false; // one end per run, none after the stop
    #reports;

    /// event: the start event of the core; region: its entry for this region (fit, align, volume)
    constructor(event, region, primary, storage, reports) {
        this.id = event.id;
        this.kind = event.kind;
        this.label = event.label;
        this.primary = primary;
        this.#reports = reports;

        if (this.kind === 'web')
            this.#url = storage.webUrl(event.source);
        else if (this.kind !== 'brush')
            this.#url = storage.blobUrl(event.source);
        if (this.#url !== null || this.kind === 'brush')
            this.element = this.#createElement(event, region);
    }

    /// false if the file is missing; then the core got mediaFailed
    get ready() {
        if (this.element !== null)
            return true;
        this.#failed('missing in the cache');
        return false;
    }

    get hasPicture() {
        return this.kind !== 'audio';
    }

    get #playable() {
        return this.kind === 'video' || this.kind === 'audio';
    }

    start() {
        if (this.#playable)
            this.#play();
    }

    stop() {
        if (this.#playable) {
            this.element.pause();
            this.element.removeAttribute('src');
            this.element.load();
        }
        this.element.remove();
        Storage.release(this.#url);
    }

    /// RepeatMedia: again from the beginning; an image just stays
    replay() {
        if (!this.#playable)
            return;
        this.#done = false;
        this.element.currentTime = 0;
        this.#play();
    }

    setPaused(paused) {
        this.paused = paused;
        if (!this.#playable)
            return;
        if (paused)
            this.element.pause();
        else
            this.#play();
    }

    /// Browsers block sound without a user gesture: a video then plays muted (a kiosk browser can allow it, e.g.
    /// Chromium with --autoplay-policy=no-user-gesture-required).
    #play() {
        this.element.play().catch((error) => {
            if (error.name === 'NotAllowedError' && this.kind === 'video' && !this.element.muted) {
                console.warn('sound blocked by the browser, the video plays muted');
                this.element.muted = true;
                this.#play();
            } else if (error.name !== 'AbortError') {
                this.#failed(`${error.name}: ${error.message}`);
            }
        });
    }

    #finished() {
        if (this.primary && !this.#done) {
            this.#done = true;
            this.#reports.finished(this.id);
        }
    }

    #failed(message) {
        console.warn('media failed:', this.label, message);
        if (this.primary && !this.#done) {
            this.#done = true;
            this.#reports.failed(this.id);
        }
    }

    static #horizontal(align) {
        if (align.endsWith('right')) return 'right';
        if (align === 'center' || align.endsWith('mid')) return 'center';
        return 'left';
    }

    static #vertical(align) {
        if (align.startsWith('bottom')) return 'bottom';
        if (align === 'center' || align.startsWith('mid')) return 'center';
        return 'top';
    }

    #createElement(event, region) {
        let element;
        if (this.kind === 'image') {
            element = document.createElement('img');
            element.style.objectFit = IMAGE_FIT[region.fit] ?? 'none';
            element.style.objectPosition = `${Media.#horizontal(region.align)} ${Media.#vertical(region.align)}`;
            element.onerror = () => this.#failed('cannot load image');
            element.src = this.#url;
        } else if (this.#playable) {
            element = document.createElement(this.kind);
            element.volume = region.volume;
            element.style.objectFit = region.fit === 'fill' ? 'fill' : region.fit === 'slice' ? 'cover' : 'contain';
            element.onended = () => this.#finished();
            element.onerror = () => this.#failed(element.error?.message || 'invalid media');
            element.src = this.#url;
        } else if (this.kind === 'web') {
            element = document.createElement('iframe');
            element.onload = () => {
                try { // like garlic-lib: no scroll bars; only possible for pages of this origin (from the cache)
                    element.contentDocument.body.style.overflow = 'hidden';
                } catch (error) { /* another origin */ }
            };
            element.src = this.#url;
        } else { // brush
            element = document.createElement('div');
            element.style.backgroundColor = event.color;
        }
        element.className = 'media';
        if (this.kind !== 'web') // web pages handle their clicks themselves
            element.addEventListener('click', () => {
                if (!element.hidden)
                    this.#reports.activated(this.id);
            });
        return element;
    }
}

/// One <region>: placed relative to the stage, background color and image, its media stacked (the newest on top).
class Region {
    element;
    imageSrc; // the backgroundImage as written in the SMIL
    name;
    media = [];

    constructor(definition) {
        this.imageSrc = definition.imageSrc;
        this.name = definition.name;
        this.element = document.createElement('div');
        this.element.className = 'region';
        const style = this.element.style;
        style.left = `${definition.left * 100}%`;
        style.top = `${definition.top * 100}%`;
        style.width = `${definition.width * 100}%`;
        style.height = `${definition.height * 100}%`;
        style.backgroundColor = definition.color;
        style.backgroundRepeat = REPEAT[definition.repeat] ?? 'repeat';
    }

    add(media) {
        this.media.push(media);
        this.element.append(media.element);
        this.#updateOnTop();
    }

    /// true if the instance was shown here
    remove(id) {
        const index = this.media.findIndex((media) => media.id === id);
        if (index < 0)
            return false;
        this.media[index].stop();
        this.media.splice(index, 1);
        this.#updateOnTop();
        return true;
    }

    find(id) {
        return this.media.find((media) => media.id === id);
    }

    /// a resumed medium comes to the top again
    raise(media) {
        this.media.splice(this.media.indexOf(media), 1);
        this.media.push(media);
        this.element.append(media.element);
        this.#updateOnTop();
    }

    setPaused(media, paused) {
        media.setPaused(paused);
        this.#updateOnTop(); // a paused web page is hidden
    }

    /// Only the topmost medium with a picture is visible; audio has none and covers nothing.
    #updateOnTop() {
        const top = this.media.findLast((media) => media.hasPicture);
        for (const media of this.media)
            media.element.hidden = !media.hasPicture || media !== top || (media.kind === 'web' && media.paused);
    }
}

export class Stage {
    #element;
    #storage;
    #reports;
    #regions = [];
    #rootImageSrc = '';
    #backgroundUrls = [];

    /// reports: {finished(id), failed(id), activated(id)}, called when a medium reports to the core
    constructor(element, storage, reports) {
        this.#element = element;
        this.#storage = storage;
        this.#reports = reports;
    }

    /// The events of the core that change what is shown.
    handle(event) {
        switch (event.type) {
        case 'layout': this.#newLayout(event); break;
        case 'background': this.#backgroundImage(event); break;
        case 'start': this.#start(event); break;
        case 'stop': this.#stop(event.id); break;
        case 'repeat': this.#forEach(event.id, (media) => media.replay()); break;
        case 'pause': this.#forEach(event.id, (media, region) => region.setPaused(media, true)); break;
        case 'resume':
            this.#forEach(event.id, (media, region) => {
                region.setPaused(media, false);
                region.raise(media);
            });
            break;
        }
    }

    /// "region: medium" for every medium shown, for the debug dialog
    playing() {
        return this.#regions.flatMap((region) => region.media.map((media) => `${region.name}: ${media.label}`));
    }

    /// shows a text instead of the media, e.g. when the player ended
    showMessage(text) {
        const message = document.createElement('div');
        message.className = 'message';
        message.textContent = text;
        this.#element.replaceChildren(message);
        this.#regions = [];
    }

    /// A new index: the old media are stopped already (StopMedia come first), so only the regions go.
    #newLayout(layout) {
        this.#backgroundUrls.forEach(Storage.release);
        this.#backgroundUrls = [];
        this.#element.replaceChildren();
        this.#element.style.backgroundColor = layout.rootColor;
        this.#element.style.backgroundRepeat = REPEAT[layout.rootRepeat] ?? 'repeat';
        this.#rootImageSrc = layout.rootImageSrc;
        this.#setBackground(this.#element, layout.rootImage);

        this.#regions = layout.regions.map((definition) => {
            const region = new Region(definition);
            this.#setBackground(region.element, definition.image);
            this.#element.append(region.element); // sorted by z-index: the later lies on top
            return region;
        });
    }

    #backgroundImage(image) {
        if (image.src === this.#rootImageSrc)
            this.#setBackground(this.#element, image.path);
        for (const region of this.#regions)
            if (region.imageSrc === image.src)
                this.#setBackground(region.element, image.path);
    }

    #setBackground(element, path) {
        const url = path ? this.#storage.blobUrl(path) : '';
        if (url)
            this.#backgroundUrls.push(url);
        element.style.backgroundImage = url ? `url("${url}")` : '';
    }

    /// shown in all regions with the name of the instance
    #start(event) {
        this.#stop(event.id); // the same instance again: start anew
        if (event.kind === 'command') { // garlic-lib only knew adapi:blankScreen, which needs the launcher
            if (event.source.toLowerCase() === 'adapi:blankscreen')
                console.info('blankScreen: no launcher, the screen stays on');
            else
                this.#reports.failed(event.id);
            return;
        }

        let primary = true;
        for (const definition of event.regions) {
            const region = this.#regions[definition.index];
            if (!region)
                continue;
            const media = new Media(event, definition, primary, this.#storage, this.#reports);
            primary = false; // one report per instance, also for a missing file
            if (!media.ready)
                continue;
            region.add(media);
            media.start();
        }
    }

    #stop(id) {
        for (const region of this.#regions)
            region.remove(id);
    }

    #forEach(id, action) {
        for (const region of this.#regions) {
            const media = region.find(id);
            if (media)
                action(media, region);
        }
    }
}
