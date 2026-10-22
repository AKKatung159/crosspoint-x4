# Vendored BleKeyboardHost (PERSONAL)

A copy of `freeink-sdk/libs/network/BleKeyboardHost` at Free-Ink/freeink-sdk `084fdfb`, the SDK pin of
upstream CrossPoint's Bluetooth page turner (crosspoint-reader#3785). `lib/BlePageTurner` needs its
`connectedAddr`, `end(timeoutMs)`, `armSelectedPeerReconnect` and `popRawButton`, which this fork's SDK pin
(`111fdcc`) predates. The rest of that SDK range (130 commits) is not taken.

Delete this folder and restore `BleKeyboardHost=symlink://freeink-sdk/libs/network/BleKeyboardHost` in
`[ble_page_turner]` (`platformio.ini`) once the `freeink-sdk` pin reaches `084fdfb`.
