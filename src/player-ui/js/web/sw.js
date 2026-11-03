// Service worker of the garlic-player: serves the files of the cache under garlic-fs/<path> to web pages and widgets
// (iframes), so their relative links work. The files are in the file system of the core inside the player page; the
// worker asks the page for them (Storage in storage.js).

const PREFIX = new URL('garlic-fs/', self.registration.scope).pathname;
const TIMEOUT = 5000; // ms for the answer of a page

self.addEventListener('install', () => self.skipWaiting());
self.addEventListener('activate', (event) => event.waitUntil(self.clients.claim()));

self.addEventListener('fetch', (event) => {
    const url = new URL(event.request.url);
    if (url.origin !== location.origin || !url.pathname.startsWith(PREFIX))
        return;
    event.respondWith(serve(decodeURI(url.pathname.slice(PREFIX.length - 1)))); // keeps the leading /
});

/// Asks the player pages (not the iframes) one after the other; the first that has the file answers.
async function serve(path) {
    const windows = await self.clients.matchAll({ type: 'window', includeUncontrolled: true });
    for (const client of windows) {
        if (client.frameType !== 'top-level' || new URL(client.url).pathname.startsWith(PREFIX))
            continue;
        const file = await ask(client, path);
        if (file)
            return new Response(file.data, { headers: { 'Content-Type': file.mime } });
    }
    return new Response(`not in the cache: ${path}`, { status: 404, headers: { 'Content-Type': 'text/plain' } });
}

function ask(client, path) {
    return new Promise((resolve) => {
        const channel = new MessageChannel();
        const timer = setTimeout(() => resolve(null), TIMEOUT);
        channel.port1.onmessage = (message) => {
            clearTimeout(timer);
            resolve(message.data);
        };
        client.postMessage({ type: 'garlic-read', path }, [channel.port2]);
    });
}
