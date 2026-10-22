# ติดตั้งลง X4 / X4 Pro

**ภาษาไทย** · [English](install.en.md)

กลับไปที่ [README](../../README.md)

ดาวน์โหลดไฟล์ของรุ่นตัวเองจากหน้า [Releases](https://github.com/AKKatung159/crosspoint-x4/releases)
ของ repo นี้ หรือ build เอง ([development.md](development.md)):

| เครื่อง | ไฟล์ | ชิป |
| --- | --- | --- |
| X4, X3 | `crosspoint-<เวอร์ชัน>-x3-x4.bin` | ESP32-C3 |
| X4 Pro | `crosspoint-<เวอร์ชัน>-x4pro.bin` | ESP32-S3 |

ไฟล์เป็น app image ธรรมดา แบบเดียวกับ `crosspoint-1.6.5-x3-x4.bin` / `-x4pro.bin` ของทางการ
ห้ามแฟลชไฟล์ข้ามรุ่น (OTA และอัปเดตจากการ์ดจะปฏิเสธเอง) X4 Pro ยังเทสบน emulator เท่านั้น

**เก็บไฟล์สำหรับย้อนกลับไว้ก่อนแฟลช** สำหรับ X4 เช่น `firmware.bin` จาก
[Halo 2 v2.2.15](https://github.com/kocha01/crosspoint-halo2-custom/releases/tag/v2.2.15)
(SHA-256 `b0156ec5760dce61dafe24bb76f7de58c25a3f0f8ee7f340d69420b09e151929`)
สำหรับ X4 Pro ใช้ `crosspoint-1.6.5-x4pro.bin` ของ [CrossPoint ทางการ](https://github.com/crosspoint-reader/crosspoint-reader/releases)

## วิธี A: web flasher (สาย USB-C, Chrome หรือ Edge)

1. ต่อเครื่องด้วยสาย USB-C แล้วกดปลุกเครื่อง
2. เปิด https://crosspointreader.com/#flash-tools เลือกรุ่น (**X4** หรือ **Xteink X4Pro**) แล้ว **Custom .bin** แล้วเลือกไฟล์
3. รอจนเสร็จ แล้วปล่อยให้เครื่องรีบูต ห้ามถอดสายระหว่างแฟลช

## วิธี B: จากการ์ด SD (ไม่ต้องต่อคอม)

1. คัดลอกไฟล์ `.bin` ลงการ์ด SD
2. บนเครื่อง: **Settings → System → SD Card Firmware Update** แล้วเลือกไฟล์
   (ใช้ได้เฉพาะเมื่อเฟิร์มแวร์ที่รันอยู่มีเมนูนี้)

## วิธี C: esptool

```bash
esptool.py --chip esp32c3 --port COM5 --baud 921600 write_flash 0x10000 crosspoint-1.6.5-th.4-x3-x4.bin
```

X4 Pro ใช้ `--chip esp32s3` และไฟล์ `-x4pro.bin`:

```bash
esptool.py --chip esp32s3 --port COM5 --baud 921600 write_flash 0x10000 crosspoint-1.6.5-th.7-x4pro.bin
```

ใช้ address `0x10000` เท่านั้น เขียนที่อื่นอาจทำ bootloader หรือตาราง partition เสีย

## ย้อนกลับ

แฟลช `firmware.bin` ของ Halo 2 (หรือ `crosspoint-1.6.5-x3-x4.bin` ของทางการ) ด้วยวิธีเดียวกัน
X4 Pro ย้อนกลับด้วย `crosspoint-1.6.5-x4pro.bin` ของทางการ
หนังสือ ตำแหน่งที่อ่าน และการตั้งค่าบนการ์ดยังอยู่ครบ แต่ละเฟิร์มแวร์จะจัดหน้าหนังสือใหม่ตอนเปิดครั้งแรก

หลังติดตั้ง: **Settings → System → Language → ไทย** ฟอนต์ในเครื่องมีอักษรไทยอยู่แล้ว
ฟอนต์จากการ์ดที่ไม่มีอักษรไทยจะยืมตัวไทยจากฟอนต์ในเครื่อง
