/* PERSONAL: swap upstream's colour emoji for one solid icon family (the same
   filled-shape style as the device's home icons). Upstream's pages and scripts
   stay untouched: text nodes are rewritten as they appear, including rows the
   file manager builds later. Typographic marks (arrows, check and play marks) are left alone. */
(function () {
  var P = {
    folder: 'M3 6.5A2.5 2.5 0 0 1 5.5 4H10l2 2.5h6.5A2.5 2.5 0 0 1 21 9v8.5a2.5 2.5 0 0 1-2.5 2.5h-13A2.5 2.5 0 0 1 3 17.5z',
    folderOpen: 'M3 6.5A2.5 2.5 0 0 1 5.5 4H10l2 2.5h6.5A2.5 2.5 0 0 1 21 9v1H7.2a2 2 0 0 0-1.9 1.4L3 18.5zM6.8 12h15.4l-2.4 6.6a2 2 0 0 1-1.9 1.4H4.2z',
    file: 'M6.5 2H14l6 6v11.5A2.5 2.5 0 0 1 17.5 22h-11A2.5 2.5 0 0 1 4 19.5v-15A2.5 2.5 0 0 1 6.5 2zM13 3.5V9h5.5z',
    trash: 'M9 2.5h6l1 2h4V7H4V4.5h4zM5.5 8.5h13l-1 11A2.5 2.5 0 0 1 15 22H9a2.5 2.5 0 0 1-2.5-2.5z',
    warning: 'M12 2 23 21.5H1zM10.8 9v6.2h2.4V9zm0 7.8v2.4h2.4v-2.4z',
    home: 'M12 2.5 2 11h2.8v10.5h5.7v-6.5h3v6.5h5.7V11H22z',
    box: 'M12 2 21 6.5v11L12 22l-9-4.5v-11zM12 4.3 6.2 7.2 12 10l5.8-2.8z',
    image: 'M4.5 3h15A2.5 2.5 0 0 1 22 5.5v13a2.5 2.5 0 0 1-2.5 2.5h-15A2.5 2.5 0 0 1 2 18.5v-13A2.5 2.5 0 0 1 4.5 3zM5 18h14l-4.5-6-3.5 4.5-2.5-3zM8 6.5a2 2 0 1 0 0 4 2 2 0 0 0 0-4z',
    upload: 'M12 1.5 19 9h-5v7h-4V9H5zM4 18h16v4H4z',
    download: 'M10 1.5h4v7h5l-7 7.5-7-7.5h5zM4 18h16v4H4z',
    arrowDown: 'M10 2.5h4v10h5.5L12 21l-7.5-8.5H10z',
    grey: 'M12 2a10 10 0 1 0 0 20 10 10 0 0 0 0-20zm0 3v14a7 7 0 0 0 0-14z',
    ruler: 'M2 7h20v10H2zM5.5 7v4.5H7V7zm4 0v6H11V7zm4 0v4.5H15V7zm4 0v6H19V7z',
    scissors: 'M6 2.5a3.6 3.6 0 1 1 0 7.2 3.6 3.6 0 0 1 0-7.2zm0 2.1a1.5 1.5 0 1 0 0 3 1.5 1.5 0 0 0 0-3zM6 14.3a3.6 3.6 0 1 1 0 7.2 3.6 3.6 0 0 1 0-7.2zm0 2.1a1.5 1.5 0 1 0 0 3 1.5 1.5 0 0 0 0-3zM9 8.8 21.5 2.5v2.8L12.3 12l9.2 6.7v2.8L9 15.2 7.5 12z',
    lock: 'M6.8 10V7.2a5.2 5.2 0 0 1 10.4 0V10h1.3A1.5 1.5 0 0 1 20 11.5v9a1.5 1.5 0 0 1-1.5 1.5h-13A1.5 1.5 0 0 1 4 20.5v-9A1.5 1.5 0 0 1 5.5 10zm2.6 0h5.2V7.2a2.6 2.6 0 0 0-5.2 0z',
    spark: 'M12 1.5c1 5.6 3.3 8.5 10 10.5-6.7 2-9 4.9-10 10.5-1-5.6-3.3-8.5-10-10.5 6.7-2 9-4.9 10-10.5z',
    pencil: 'M16.2 2.3 21.7 7.8 9.2 20.3H3.7v-5.5z',
    chart: 'M3 20h18v2.5H3zM4 12h4.2v6.5H4zm5.9-5.5h4.2v12H9.9zM15.8 2.5H20v16h-4.2z',
    wrench: 'M21.5 6.8a5.7 5.7 0 0 1-7.7 5.3L6.4 19.5a2.4 2.4 0 1 1-3.4-3.4l7.4-7.4A5.7 5.7 0 0 1 17.2 1l-3.5 3.5.8 3.3 3.3.8z',
    device: 'M7 1.5h10A2.5 2.5 0 0 1 19.5 4v16a2.5 2.5 0 0 1-2.5 2.5H7A2.5 2.5 0 0 1 4.5 20V4A2.5 2.5 0 0 1 7 1.5zM7.5 4.5v12h9v-12zM10 18.6v1.6h4v-1.6z',
    setSquare: 'M3 2.5 21.5 21H3zM6 9.6V18h8.4z',
    clipboard: 'M9 1.5h6a1 1 0 0 1 1 1V4h2.5A2.5 2.5 0 0 1 21 6.5v13a2.5 2.5 0 0 1-2.5 2.5h-13A2.5 2.5 0 0 1 3 19.5v-13A2.5 2.5 0 0 1 5.5 4H8V2.5a1 1 0 0 1 1-1zM7 10v2h10v-2zm0 4v2h7v-2z',
    book: 'M5.5 2h13A1.5 1.5 0 0 1 20 3.5V17H6.8a1.6 1.6 0 0 0 0 3.2H20V22H6.8A3.8 3.8 0 0 1 3 18.2V4.5A2.5 2.5 0 0 1 5.5 2z'
  };
  // taste-skill-ignore: the keys below are the emoji being replaced, written as escapes
  var MAP = {
    '\u{1F4C1}': 'folder', '\u{1F4C2}': 'folderOpen', '\u{1F4C4}': 'file', '\u{1F5D1}': 'trash', '⚠': 'warning',  // taste-skill-ignore
    '\u{1F3E0}': 'home', '\u{1F4E6}': 'box', '\u{1F5BC}': 'image', '\u{1F4E4}': 'upload', '\u{1F4E5}': 'download',
    '⬇': 'arrowDown', '⚫': 'grey', '\u{1F4CF}': 'ruler', '✂': 'scissors', '\u{1F512}': 'lock',  // taste-skill-ignore
    '✦': 'spark', '✏': 'pencil', '\u{1F4CA}': 'chart', '\u{1F527}': 'wrench', '\u{1F4F1}': 'device',  // taste-skill-ignore
    '\u{1F4D0}': 'setSquare', '\u{1F4CB}': 'clipboard', '\u{1F4D7}': 'book', '\u{1F4DA}': 'book'
  };
  var RE = new RegExp('(' + Object.keys(MAP).join('|') + ')\\uFE0F?\\s?', 'gu');
  var SKIP = { SCRIPT: 1, STYLE: 1, TEXTAREA: 1, OPTION: 1, TITLE: 1 };
  var NS = 'http://www.w3.org/2000/svg';

  function icon(name) {
    var svg = document.createElementNS(NS, 'svg');
    svg.setAttribute('viewBox', '0 0 24 24');
    svg.setAttribute('class', 'x4i');
    svg.setAttribute('aria-hidden', 'true');
    var path = document.createElementNS(NS, 'path');
    path.setAttribute('d', P[name]);
    path.setAttribute('fill-rule', 'evenodd');
    svg.appendChild(path);
    return svg;
  }

  function swapText(node) {
    var text = node.nodeValue;
    RE.lastIndex = 0;
    if (!RE.test(text)) return;
    RE.lastIndex = 0;
    var frag = document.createDocumentFragment(), last = 0, m;
    while ((m = RE.exec(text))) {
      if (m.index > last) frag.appendChild(document.createTextNode(text.slice(last, m.index)));
      frag.appendChild(icon(MAP[m[1]]));
      last = RE.lastIndex;
    }
    if (last < text.length) frag.appendChild(document.createTextNode(text.slice(last)));
    node.parentNode.replaceChild(frag, node);
  }

  function walk(root) {
    if (root.nodeType === 3) {
      if (root.parentNode && !SKIP[root.parentNode.nodeName]) swapText(root);
      return;
    }
    if (root.nodeType !== 1 || SKIP[root.nodeName]) return;
    var w = document.createTreeWalker(root, NodeFilter.SHOW_TEXT), list = [], n;
    while ((n = w.nextNode())) if (!SKIP[n.parentNode.nodeName]) list.push(n);
    list.forEach(swapText);
  }

  function start() {
    walk(document.body);
    new MutationObserver(function (records) {
      records.forEach(function (r) {
        if (r.type === 'characterData') walk(r.target);
        else r.addedNodes.forEach(walk);
      });
    }).observe(document.body, { childList: true, subtree: true, characterData: true });
  }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', start);
  else start();
})();
