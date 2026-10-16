# ติดตั้งลง X4

**ภาษาไทย** · [English](install.en.md)

กลับไปที่ [README](../../README.md)

ดาวน์โหลดไฟล์ `crosspoint-<เวอร์ชัน>-x3-x4.bin` (เช่น `crosspoint-1.6.5-th.4-x3-x4.bin`) จากหน้า
[Releases](https://github.com/AKKatung159/crosspoint-x4/releases) ของ repo นี้ หรือ build เอง
([development.md](development.md)) ไฟล์นี้เป็น app image ธรรมดา แบบเดียวกับ
`crosspoint-1.6.5-x3-x4.bin` ของทางการ

**เก็บไฟล์สำหรับย้อนกลับไว้ก่อนแฟลช** เช่น `firmware.bin` จาก
[Halo 2 v2.2.15](https://github.com/kocha01/crosspoint-halo2-custom/releases/tag/v2.2.15)
(SHA-256 `b0156ec5760dce61dafe24bb76f7de58c25a3f0f8ee7f340d69420b09e151929`)

## วิธี A: web flasher (สาย USB-C, Chrome หรือ Edge)

1. ต่อ X4 ด้วยสาย USB-C แล้วกดปลุกเครื่อง
2. เปิด https://crosspointreader.com/#flash-tools เลือก **X4** แล้ว **Custom .bin** แล้วเลือกไฟล์
3. รอจนเสร็จ แล้วปล่อยให้เครื่องรีบูต ห้ามถอดสายระหว่างแฟลช

## วิธี B: จากการ์ด SD (ไม่ต้องต่อคอม)

1. คัดลอกไฟล์ `.bin` ลงการ์ด SD
2. บนเครื่อง: **Settings → System → SD Card Firmware Update** แล้วเลือกไฟล์
   (ใช้ได้เฉพาะเมื่อเฟิร์มแวร์ที่รันอยู่มีเมนูนี้)

## วิธี C: esptool

```bash
esptool.py --chip esp32c3 --port COM5 --baud 921600 write_flash 0x10000 crosspoint-1.6.5-th.4-x3-x4.bin
```

ใช้ address `0x10000` เท่านั้น เขียนที่อื่นอาจทำ bootloader หรือตาราง partition เสีย

## ย้อนกลับ

แฟลช `firmware.bin` ของ Halo 2 (หรือ `crosspoint-1.6.5-x3-x4.bin` ของทางการ) ด้วยวิธีเดียวกัน
หนังสือ ตำแหน่งที่อ่าน และการตั้งค่าบนการ์ดยังอยู่ครบ แต่ละเฟิร์มแวร์จะจัดหน้าหนังสือใหม่ตอนเปิดครั้งแรก

หลังติดตั้ง: **Settings → System → Language → ไทย** ฟอนต์ในเครื่องมีอักษรไทยอยู่แล้ว
ฟอนต์จากการ์ดที่ไม่มีอักษรไทยจะยืมตัวไทยจากฟอนต์ในเครื่อง
