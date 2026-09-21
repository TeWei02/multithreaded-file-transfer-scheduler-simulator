/* Offline cache for the interactive demo. The demo performs every simulation
 * locally, so once these files are cached the page keeps working with no
 * network connection. Bump CACHE_NAME when the assets change. */
const CACHE_NAME = 'mfts-demo-v2';
const PRECACHE = [
  './',
  'index.html',
  'verify.html',
  'styles.css',
  'engine.js',
  'app.js',
  'oracle_bundle.js',
  'manifest.json',
  'icons/icon-192.png',
  'icons/icon-512.png',
  'data/sample_requests.csv',
  'data/n15_seed99.csv',
  'data/n37_seed2026.csv',
  'data/n50_seed7.csv',
  'data/n120_seed42.csv',
  'data/boundary_workload.csv'
];

self.addEventListener('install', (event) => {
  event.waitUntil(
    caches.open(CACHE_NAME)
      .then((cache) => cache.addAll(PRECACHE))
      .then(() => self.skipWaiting())
  );
});

self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches.keys()
      .then((keys) => Promise.all(
        keys.filter((key) => key !== CACHE_NAME).map((key) => caches.delete(key))
      ))
      .then(() => self.clients.claim())
  );
});

self.addEventListener('fetch', (event) => {
  const request = event.request;
  if (request.method !== 'GET' || new URL(request.url).origin !== self.location.origin) {
    return;
  }
  event.respondWith(
    caches.match(request).then((cached) => {
      const network = fetch(request)
        .then((response) => {
          if (response && response.ok) {
            const copy = response.clone();
            caches.open(CACHE_NAME).then((cache) => cache.put(request, copy));
          }
          return response;
        })
        .catch(() => cached);
      return cached || network;
    })
  );
});
