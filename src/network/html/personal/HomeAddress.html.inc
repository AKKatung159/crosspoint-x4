<!-- PERSONAL: the reader's address and page shortcuts, above upstream's Device Status card (HomePage.html). -->
<style>
  .address {
    margin-top: 0;
  }
  .address .label {
    display: block;
    margin-bottom: 4px;
  }
  .web-ui .address .ip {
    margin: 0;
    font-size: clamp(40px, 9vw, 72px);
    line-height: 1.05;
    color: var(--accent-color);
    overflow-wrap: anywhere;
  }
  .address p {
    margin: 12px 0 0;
    color: var(--muted-color);
  }
  .shortcuts {
    display: grid;
    grid-template-columns: repeat(2, 1fr);
    gap: 16px;
  }
  .shortcut {
    display: grid;
    gap: 12px;
    padding: 20px;
    border-radius: 16px;
    background: var(--card-bg);
    box-shadow: 0 0 0 1px #1c19170d, 0 1px 3px #0000001a, 0 1px 2px -1px #0000001a;
    color: var(--title-color);
    text-decoration: none;
    transition: box-shadow 0.15s ease;
  }
  .shortcut:hover {
    box-shadow: 0 0 0 1px var(--accent-color), 0 1px 3px #0000001a;
  }
  .shortcut .icon {
    width: 28px;
    height: 28px;
    color: var(--accent-color);
  }
  .shortcut b {
    display: block;
    font-weight: 600;
  }
  .shortcut small {
    color: var(--muted-color);
  }
  @media (prefers-color-scheme: dark) {
    .shortcut {
      box-shadow: 0 0 0 1px var(--border-color), 0 1px 3px #0000001a;
    }
  }
  @media (min-width: 720px) {
    .shortcuts {
      grid-template-columns: repeat(4, 1fr);
    }
  }
</style>
<section class="card address" aria-live="polite">
  <span class="label">Address</span>
  <h2 class="ip" id="ip-big">…</h2>
  <p>The reader is on Wi-Fi at this address. It stays awake while File Transfer is open.</p>
</section>

<nav class="shortcuts" aria-label="Pages">
  <a class="shortcut" href="/files">
    <svg class="icon"><use href="/assets/icons.svg#folder"></use></svg>
    <span><b>File Manager</b><small>Send and manage books</small></span>
  </a>
  <a class="shortcut" href="/settings">
    <svg class="icon" viewBox="0 0 24 24"><circle cx="12" cy="12" r="3" /><path d="M12 2v3m0 14v3M2 12h3m14 0h3M4.9 4.9 7 7m10 10 2.1 2.1M4.9 19.1 7 17M17 7l2.1-2.1" /></svg>
    <span><b>Settings</b><small>Reading, display, Wi-Fi</small></span>
  </a>
  <a class="shortcut" href="/fonts">
    <svg class="icon" viewBox="0 0 24 24"><path d="M3 20 9 4l6 16M5.5 14h7M17 20v-6a3 3 0 0 1 6 0v6m-6-3h6" /></svg>
    <span><b>Fonts</b><small>SD card fonts</small></span>
  </a>
  <a class="shortcut" href="http://x4.lan/" rel="noopener">
    <svg class="icon" viewBox="0 0 24 24"><path d="M4 20V14m6 6V9m6 11V4m4 16H2" /></svg>
    <span><b>Reading stats</b><small>On x4.lan</small></span>
  </a>
</nav>

