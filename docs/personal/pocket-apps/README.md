# Pocket Hub (แอป)

หน้า Home → **แอป** เปิดรายการแอปที่อยู่ในเฟิร์มแวร์ เนื้อหาของแต่ละแอปอยู่บน SD
ติดตั้งเพิ่มได้ด้วยการคัดลอกไฟล์ ไม่ต้องแฟลชใหม่

| แอป | ไฟล์บน SD | ปุ่ม |
|---|---|---|
| แฟลชการ์ด | `/apps/flashcards/*.tsv` | Confirm พลิก · ลูกศร เลื่อนการ์ด |
| ไพ่ทาโรต์ | `/apps/tarot/deck.tsv`, `/apps/tarot/cards/*.bmp` | Confirm รูป/ความหมาย · ลูกศร ใบถัดไป (3 ใบ) |
| 2048 | – | ลูกศรทั้งสี่ทิศ |
| เกมกู้ระเบิด | – | Confirm เปิด · กดค้าง ปักธง |
| โนโนแกรม | `/apps/nonogram/puzzles.tsv` | Confirm ระบาย · กดค้าง กากบาท |
| สถิติการอ่าน | – | – |

ทุกแอปเซฟเองตอนออก (Back หรือเข้าเครื่องพัก) ไว้ที่ `/.crosspoint/apps/` อย่าลบโฟลเดอร์นี้ถ้าไม่อยากเริ่มใหม่

## รูปแบบไฟล์

ทุกไฟล์เป็นข้อความ UTF-8 คั่นคอลัมน์ด้วย TAB (export จาก Google Sheets / Excel เป็น `.tsv` ได้เลย)
บรรทัดว่างและบรรทัดที่ขึ้นต้นด้วย `#` ถูกข้าม เขียน `\n` เพื่อขึ้นบรรทัดใหม่ในการ์ด
บรรทัดยาวได้ไม่เกิน 1 KB

- **แฟลชการ์ด**: `หน้า<TAB>หลัง` ต่อบรรทัด ไฟล์ละหนึ่งชุด ตัวอย่าง: [flashcards/english-thai.tsv](flashcards/english-thai.tsv)
- **โนโนแกรม**: `ชื่อ<TAB>แถว/แถว/...` ใช้ `#` แทนช่องที่ระบาย ไม่เกิน 15×15 ตัวเลขใบ้คำนวณให้เอง ตัวอย่าง: [nonogram/puzzles.tsv](nonogram/puzzles.tsv)
- **ไพ่ทาโรต์**: [tarot/deck.tsv](tarot/deck.tsv) (78 ใบ ไทย+อังกฤษ) และรูป 1-bit BMP สร้างด้วย

  ```bash
  python3 scripts/pocket-apps/make_tarot_pack.py <โฟลเดอร์รูปสแกน> <โฟลเดอร์ปลายทาง>
  ```

  รูปสแกนชุด Rider-Waite-Smith 1909 จาก Wikimedia Commons (public domain) ตั้งชื่อตามคอลัมน์แรกของ deck.tsv
  (`00_Fool.jpg`, `Cups_03.jpg`, …) แล้วคัดลอกโฟลเดอร์ปลายทางไปที่ `/apps/tarot/`

## ทดสอบ (นักพัฒนา)

```bash
scripts/emu/hub_test.sh <eink-qemu> .pio/build/x4_th_test/firmware.bin [workdir]   # Linux, ทุกแอปบน simulator
cmake -S test -B build/test && cmake --build build/test --target PocketAppsTest      # กติกาเกมและรูปแบบไฟล์
```

<details><summary>English</summary>

Home → **Apps** opens the built-in mini apps. Their content lives on the SD card, so new decks, puzzles
and tarot packs install by copying files. Every app saves on exit (Back or sleep) to `/.crosspoint/apps/`.

Files are UTF-8, TAB-separated (a spreadsheet's `.tsv` export works); blank lines and lines starting with `#`
are skipped, `\n` is a line break, rows are at most 1 KB.

- Flashcards: `front<TAB>back` per line, one deck per file in `/apps/flashcards/`.
- Nonogram: `name<TAB>row/row/...` with `#` for filled cells, up to 15×15; clues are computed.
- Tarot: `deck.tsv` (78 cards, Thai and English) plus 1-bit BMPs built by
  `scripts/pocket-apps/make_tarot_pack.py` from the public-domain Rider-Waite-Smith 1909 scans on Wikimedia Commons.

Controls are in the table above. Tests: `scripts/emu/hub_test.sh` (emulator, all apps) and the `PocketAppsTest`
host target.

</details>
