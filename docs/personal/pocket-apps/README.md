# Pocket Hub (แอป)

<p align="center">
  <img src="../../showcase/apps-hub.png" alt="หน้าแอป ตารางไอคอน 3 ช่องต่อแถว" width="30%">
  <img src="../../showcase/apps-tarot.png" alt="ไพ่ทาโรต์" width="30%">
  <img src="../../showcase/apps-minesweeper.png" alt="เกมกู้ระเบิด" width="30%">
</p>

หน้าหลัก → **แอป** เปิดตารางแอป 3 ช่องต่อแถว ซ้าย/ขวาเลื่อนทีละช่อง ขึ้น/ลงทีละแถว Confirm เปิด Back กลับหน้าหลัก
แอปมีสองแบบ: แอปในเฟิร์มแวร์ และแอป Pocket SDK บนการ์ด แอป SDK ที่ id ตรงกับแอปในเฟิร์มแวร์จะแสดงแทนตัวในเฟิร์มแวร์

| แอป | id | ไฟล์บน SD | ปุ่ม |
|---|---|---|---|
| แฟลชการ์ด | `flashcards` | `/apps/flashcards/*.tsv` | Confirm พลิก · ลูกศร เลื่อนการ์ด |
| ไพ่ทาโรต์ | `tarot` | `/apps/tarot/deck.tsv`, `/apps/tarot/cards/*.bmp` | Confirm รูป/ความหมาย · ลูกศร ใบถัดไป (3 ใบ) |
| 2048 | `2048` | – | ลูกศรทั้งสี่ทิศ |
| เกมกู้ระเบิด | `minesweeper` | – | Confirm เปิด · กดค้าง ปักธง |
| โนโนแกรม | `nonogram` | `/apps/nonogram/puzzles.tsv` | Confirm ระบาย · กดค้าง กากบาท |
| สถิติการอ่าน | `stats` | – | – |
| แดชบอร์ด (SDK เท่านั้น) | `dashboard` | – | Confirm ต่อ Wi-Fi แล้วโหลดแดชบอร์ดใหม่ |

ทุกแอปเซฟเองตอนออก (Back หรือพักเครื่อง) ไว้ที่ `/.crosspoint/apps/` แบบสองช่องสลับกันพร้อม CRC
ไฟดับกลางคันก็ยังได้เซฟก่อนหน้า แอป SDK เซฟแยกจากแอปในเฟิร์มแวร์ (`sdk-<id>.sav`)

## ติดตั้งแอป SDK

ดาวน์โหลดไฟล์ `.xpa` จาก [x4-pocket-apps releases](https://github.com/AKKatung159/x4-pocket-apps/releases)
แล้วแตกไฟล์ (เป็น zip) ลง `/apps/` บนการ์ด จะได้ `/apps/<id>/` ที่มี `manifest.json`, `app.wasm`, `icon.bmp`
และข้อมูลของแอป (เช่นรูปไพ่ทาโรต์ 78 ใบ) X4 ไม่มีโหมด USB drive ถ้าไม่ถอดการ์ดมาเสียบคอม ให้ใช้ถ่ายโอนไฟล์ผ่าน Wi-Fi
(เว็บของเครื่องไม่เขียนทับไฟล์เดิม ต้องลบก่อน)

## เขียนแอปเอง

แอปเขียนด้วย C คอมไพล์เป็น WebAssembly ด้วย zig แล้วรันบน wasm3 ในเครื่อง ตัวอย่างที่สั้นที่สุดคือ
[sdk/examples/2048](../../../sdk/examples/2048) วิธี build และแพ็กอยู่ใน x4-pocket-apps (`scripts/build.sh`)

- **API:** [sdk/include/x4.h](../../../sdk/include/x4.h) แอป export `init`, `event`, `render`, `exit`
  เฟิร์มแวร์วาดหัวจอกับป้ายปุ่มให้ และ Back ออกจากแอปเสมอ
- **SDK v2** (`"sdk": 2` ใน manifest): `x4_list` (รายชื่อไฟล์ในโฟลเดอร์แอป), `x4_rows` (ดัชนีแถว TSV ที่เฟิร์มแวร์สแกนให้),
  `x4_image2` (รูปหมุน 180°), `x4_stats` (สถิติการอ่าน), `x4_dashboard` และ `x4_sync` (ภาพแดชบอร์ด และโหลดใหม่ผ่าน Wi-Fi)
- **manifest.json:** `sdk`, `id`, `version`, `entry`, `name` (`en`, `th`), `memory` (KB, ไม่เกิน 32), `save`
  (ไบต์ ไม่เกิน 1024 ต้องเท่ากับขนาดที่แอปเซฟ), `icon` (ค่าเริ่มต้น `icon.bmp`: 32×32 1 บิต)
- **ข้อจำกัด:** แต่ละการเรียกมีงบ gas ราว 0.4 วินาที ไฟล์อ่านได้เฉพาะในโฟลเดอร์แอป ชื่อไฟล์ใช้ได้แค่ `A-Z a-z 0-9 . _ -`

## รูปแบบไฟล์

ทุกไฟล์เป็นข้อความ UTF-8 คั่นคอลัมน์ด้วย TAB (export จาก Google Sheets / Excel เป็น `.tsv` ได้เลย)
บรรทัดว่างและบรรทัดที่ขึ้นต้นด้วย `#` ถูกข้าม เขียน `\n` เพื่อขึ้นบรรทัดใหม่ในการ์ด บรรทัดยาวได้ไม่เกิน 1 KB

- **แฟลชการ์ด**: `หน้า<TAB>หลัง` ต่อบรรทัด ไฟล์ละหนึ่งชุด
- **โนโนแกรม**: `ชื่อ<TAB>แถว/แถว/...` ใช้ `#` แทนช่องที่ระบาย ไม่เกิน 15×15 ตัวเลขใบ้คำนวณให้เอง
- **ไพ่ทาโรต์**: `deck.tsv` (78 ใบ ไทย+อังกฤษ) และรูป 1 บิต `cards/<ชื่อ>.bmp` (มากับแพ็กแล้ว)
  รูปเองสร้างจากภาพสแกนด้วย `python3 scripts/pocket-apps/make_tarot_pack.py <โฟลเดอร์สแกน> <ปลายทาง>`

ตัวอย่างเนื้อหาทั้งหมดอยู่ใน `apps/<id>/` ของ x4-pocket-apps

## ทดสอบ (นักพัฒนา)

```bash
scripts/emu/hub_test.sh <eink-qemu> .pio/build/x4_th_test/firmware.bin [workdir]   # แอปในเฟิร์มแวร์บน emulator
scripts/emu/sdk_test.sh <eink-qemu> .pio/build/x4_th_test/firmware.bin [workdir]   # SDK: 2048 เซฟ/เปิดซ้ำ, งบ gas
cmake -S test -B build/test && cmake --build build/test --target PocketAppsTest      # กติกาเกมและรูปแบบไฟล์
```

แอป SDK ทั้งชุดเทสใน x4-pocket-apps: `tests/run.sh` (บนคอม) และ `tests/emu.sh` (eink-qemu)

<details><summary>English</summary>

Home → **Apps** opens a grid of apps, three to a row: Left/Right step a cell, Up/Down a row, Confirm opens,
Back goes home. Apps are built into the firmware or are Pocket SDK apps on the card; an SDK app whose id
matches a built-in one (`flashcards`, `tarot`, `2048`, `minesweeper`, `nonogram`, `stats`) replaces it, with
its own save (`sdk-<id>.sav`). Every app saves on exit (Back or sleep) to `/.crosspoint/apps/`, in two
alternating CRC-checked slots.

**Installing SDK apps.** Unzip a `.xpa` from the
[x4-pocket-apps releases](https://github.com/AKKatung159/x4-pocket-apps/releases) into `/apps/` on the card.
The X4 has no USB drive mode; without a card reader use the Wi-Fi file transfer, which does not overwrite
existing files.

**Writing one.** C compiled to WebAssembly with zig, run by wasm3. The API is `sdk/include/x4.h`; the
smallest example is `sdk/examples/2048`. SDK v2 (`"sdk": 2`) adds `x4_list`, `x4_rows`, `x4_image2`,
`x4_stats`, `x4_dashboard` and `x4_sync`. The manifest has `sdk`, `id`, `version`, `entry`, `name`
(`en`, `th`), `memory` (KB, at most 32), `save` (bytes, at most 1024, equal to what the app saves) and `icon`
(default `icon.bmp`, 32x32 1-bit). Each call has a gas budget of about 0.4 s; files are read only from the
app's folder, with names of `A-Z a-z 0-9 . _ -`.

**Files** are UTF-8, TAB-separated (a spreadsheet's `.tsv` export works); blank lines and lines starting with
`#` are skipped, `\n` is a line break, rows are at most 1 KB. Flashcards: `front<TAB>back` per line, one deck
per file. Nonogram: `name<TAB>row/row/...` with `#` for filled cells, up to 15x15. Tarot: `deck.tsv` plus 1-bit
`cards/<name>.bmp` (shipped in the pack; `scripts/pocket-apps/make_tarot_pack.py` builds them from scans).

**Tests:** `scripts/emu/hub_test.sh` (built-in apps), `scripts/emu/sdk_test.sh` (SDK runtime) and the
`PocketAppsTest` host target here; the SDK apps themselves are tested in x4-pocket-apps (`tests/run.sh`,
`tests/emu.sh`).

</details>
