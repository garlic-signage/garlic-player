// The menu of all players (Settings, Debug infos, Exit) with its gesture, and the two dialogs. Reading, checking and
// storing the settings is done by the core; the dialogs only show.

const MENU_CLICKS = 10;
const MENU_CLICK_GAP = 500; // ms between the fast clicks
const MENU_HOLD = 10000;    // ms

/**
 * Ten fast clicks or touches (less than 500 ms apart) or one held for 10 s anywhere open the menu, like in the other
 * players. Clicks into a web page (iframe) do not reach the page of the player.
 */
export class Menu {
    #element;
    #actions;
    #fastClicks = 0;
    #releasedAt = 0;
    #holdTimer = null;

    /// actions: {settings, debug, exit, changed}; changed() after the menu opened or closed
    constructor(element, actions, blocked) {
        this.#element = element;
        this.#actions = actions;
        element.addEventListener('click', (event) => this.#choose(event.target.closest('button')));
        document.addEventListener('pointerdown', (event) => this.#pressed(event, blocked), true);
        document.addEventListener('pointerup', () => this.#released(), true);
        document.addEventListener('keydown', (event) => {
            if (event.key === 'Escape' && this.isOpen)
                this.close();
        });
    }

    get isOpen() {
        return !this.#element.hidden;
    }

    open(x, y) {
        this.#element.hidden = false;
        this.#element.style.left = `${Math.min(x, innerWidth - this.#element.offsetWidth)}px`;
        this.#element.style.top = `${Math.min(y, innerHeight - this.#element.offsetHeight)}px`;
        this.#element.querySelector('button').focus();
        this.#actions.changed();
    }

    close() {
        this.#element.hidden = true;
        this.#actions.changed();
    }

    #choose(button) {
        if (!button)
            return;
        this.close();
        this.#actions[button.dataset.action]();
    }

    /// blocked(): a dialog is open, the gesture does not count
    #pressed(event, blocked) {
        if (this.isOpen) {
            if (!this.#element.contains(event.target))
                this.close();
            return;
        }
        if (blocked())
            return;
        const now = performance.now();
        this.#fastClicks = now - this.#releasedAt < MENU_CLICK_GAP ? this.#fastClicks + 1 : 1;
        clearTimeout(this.#holdTimer);
        if (this.#fastClicks >= MENU_CLICKS) {
            this.#fastClicks = 0;
            this.open(event.clientX, event.clientY);
        } else {
            this.#holdTimer = setTimeout(() => this.open(event.clientX, event.clientY), MENU_HOLD);
        }
    }

    #released() {
        this.#releasedAt = performance.now();
        clearTimeout(this.#holdTimer);
    }
}

/**
 * Player name, content URL, HTTP server on/off and port. The dialog closes only when the core has stored them,
 * otherwise it shows the error text of the core. Opened at the start without index, cancelling ends the player.
 */
export class SettingsDialog {
    #dialog;
    #form;
    #player;
    #actions;
    #atStart = false;

    /// actions: {stored, cancelledAtStart, changed}
    constructor(dialog, player, actions) {
        this.#dialog = dialog;
        this.#form = dialog.querySelector('form');
        this.#player = player;
        this.#actions = actions;
        this.#form.addEventListener('submit', (event) => this.#submit(event));
        dialog.addEventListener('close', () => this.#closed());
    }

    get isOpen() {
        return this.#dialog.open;
    }

    open(atStart = false) {
        this.#atStart = atStart;
        const current = this.#player.settings();
        this.#form.playerName.value = current.playerName;
        this.#form.indexUri.value = current.indexUri;
        this.#form.httpdEnabled.checked = current.httpdEnabled;
        this.#form.httpdPort.value = current.httpdPort;
        this.#showError('');
        this.#dialog.showModal();
        this.#actions.changed();
    }

    #submit(event) {
        if (event.submitter?.value !== 'ok')
            return;
        const message = this.#player.changeSettings({
            playerName: this.#form.playerName.value.trim(),
            indexUri: this.#form.indexUri.value.trim(),
            httpdEnabled: this.#form.httpdEnabled.checked,
            httpdPort: Number(this.#form.httpdPort.value) || 0,
        });
        if (message !== '') {
            event.preventDefault();
            this.#showError(message);
            return;
        }
        this.#actions.stored();
    }

    #closed() {
        this.#actions.changed();
        const cancelled = this.#atStart && this.#dialog.returnValue !== 'ok';
        this.#atStart = false;
        if (cancelled)
            this.#actions.cancelledAtStart();
    }

    #showError(message) {
        const error = this.#dialog.querySelector('.error');
        error.textContent = message;
        error.hidden = message === '';
    }
}

/// Every second the resources from the core and the media that play now, one line per region and medium.
export class DebugDialog {
    #dialog;
    #player;
    #actions;
    #timer = null;

    /// actions: {playing, quit, changed}; playing() gives the lines of the media shown
    constructor(dialog, player, actions) {
        this.#dialog = dialog;
        this.#player = player;
        this.#actions = actions;
        dialog.addEventListener('close', () => this.#closed());
    }

    get isOpen() {
        return this.#dialog.open;
    }

    open() {
        this.#refresh();
        this.#dialog.showModal();
        this.#timer = setInterval(() => this.#refresh(), 1000);
        this.#actions.changed();
    }

    #closed() {
        clearInterval(this.#timer);
        this.#actions.changed();
        if (this.#dialog.returnValue === 'quit')
            this.#actions.quit();
    }

    #refresh() {
        const info = this.#player.debugInfo();
        const playing = this.#actions.playing();
        const values = {
            title: info.title || '-',
            userAgent: info.userAgent,
            disk: DebugDialog.#freeOf(info.diskFree, info.diskTotal),
            memory: DebugDialog.#freeOf(info.memoryFree, info.memoryTotal),
            playerMemory: info.memoryUsed === 0 ? '-'
                : `${DebugDialog.#bytes(info.memoryUsed)}, max ${DebugDialog.#bytes(info.memoryMax)} at ${info.memoryMaxTime}`,
            threads: info.threads === 0 ? '-' : `${info.threads}, max ${info.threadsMax}`,
            playing: playing.length > 0 ? playing.join('\n') : '-',
        };
        for (const [key, value] of Object.entries(values))
            this.#dialog.querySelector(`[data-info="${key}"]`).textContent = value;
    }

    static #bytes(value) {
        const units = ['B', 'KiB', 'MiB', 'GiB', 'TiB'];
        let unit = 0;
        while (value >= 1024 && unit < units.length - 1) {
            value /= 1024;
            ++unit;
        }
        return `${value.toFixed(unit === 0 ? 0 : 2)} ${units[unit]}`;
    }

    static #freeOf(free, total) {
        return free === null || total === null ? '-' : `${DebugDialog.#bytes(free)} free of ${DebugDialog.#bytes(total)}`;
    }
}
