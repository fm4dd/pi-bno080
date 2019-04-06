Current problem:

Enabling sensor function. Setting "acc" on enables the sensory output reports. The periodic output reports currently break any subsequent "inf" data collection. Now, the second product-ID packet does not come as expected after 1st product-ID, even when waiting for a few output reports to come in. For "inf" mode, it may need disabling all output first, or check if the I2C "flush" function helps.

The issue of i2c-read when there is no data, crashing the I2C communication: this makes it difficult to just wait and "poll" until expected data arrives. If polling would work, this would greatly simplify the handling of the SH-2 asynchronous communication (responses coming out-of-order, and with varied delays).

Christmas wish: Salae Logic-8 logic analyzer, to check I2C bus actvities against code, identifying lost/missing I2C packets.

```
pi@nanopi-neo2:~/pi-bno080 $ ./getbno080 -t acc -v
Debug: ts=[1550280829] date=Sat Feb 16 10:33:49 2019
Debug: Sensor I2C address: [0x4B]
Debug: TX   5 bytes HEAD 05 00 00 01 CARGO 01
Debug: RX 276 bytes HEAD 14 81 00 01 CARGO 00 01 04 00 00 00 00 80 06 31 2E 30 2E 30 00 02 +260 more bytes ST [0]
Debug: RX   5 bytes HEAD 05 80 01 01 CARGO 01 ST [0]
Debug: RX  20 bytes HEAD 14 80 02 01 CARGO F1 00 84 00 00 00 01 00 00 00 00 00 00 00 00 00 ST [0]
Debug: CMD reportID [F1] REPseq [00] CMD [84] CMDseq [00] RESPseq [00] R0 [00]
Debug: RX   6 bytes HEAD 06 80 00 03 CARGO 01 0B ST [0]
Debug: OK  Error list 1 entries
Debug: TX   5 bytes HEAD 05 00 01 02 CARGO 01
Debug: RX 276 bytes HEAD 14 81 00 01 CARGO 00 01 04 00 00 00 00 80 06 31 2E 30 2E 30 00 02 +260 more bytes ST [0]
Debug: RX   5 bytes HEAD 05 80 01 01 CARGO 01 ST [0]
Debug: RX  20 bytes HEAD 14 80 02 01 CARGO F1 00 84 00 00 00 01 00 00 00 00 00 00 00 00 00 ST [0]
Debug: CMD reportID [F1] REPseq [00] CMD [84] CMDseq [00] RESPseq [00] R0 [00]
Debug: OK  Reset complete
Debug: OK  Initialization complete

Set Feature Command 0xFD
Debug: TX  21 bytes HEAD 15 00 02 02 CARGO FD 01 84 00 00 60 EA 00 00 00 00 00 00 00 00 00 00

Get Feature Response 0xFC
Debug: RX  21 bytes HEAD 15 80 02 03 CARGO FC 01 84 00 00 00 7D 00 00 00 00 00 00 00 00 00 +5 more bytes ST [0]

Get Feature Request 0xFE
Debug: TX   6 bytes HEAD 06 00 02 04 CARGO FE 01

WakeUp/Normal Base Timestamp 0xFB
Debug: RX  39 bytes HEAD 27 80 04 01 CARGO FB 0C 03 00 00 01 00 02 00 8E FB A5 02 89 F7 01 +23 more bytes ST [0]

Get Feature Response 0xFC
Debug: RX  21 bytes HEAD 15 80 02 05 CARGO FC 01 84 00 00 00 7D 00 00 00 00 00 00 00 00 00 +5 more bytes ST [0]
Error: Not getting SHTP feature report
```

```
pi@nanopi-neo2:~/pi-bno080 $ ./getbno080 -t acc -v
Debug: ts=[1550281013] date=Sat Feb 16 10:36:53 2019
Debug: Sensor I2C address: [0x4B]
Debug: TX   5 bytes HEAD 05 00 00 01 CARGO 01
Debug: RX  19 bytes HEAD 13 80 04 03 CARGO FB 05 00 00 00 01 03 02 00 8E FB A5 02 89 F7 ST [0]
Debug: RX   5 bytes HEAD 05 80 00 03 CARGO 01 ST [0]
Debug: OK  Error list 0 entries
Debug: OK  Initialization complete

Set Feature Command 0xFD
Debug: TX  21 bytes HEAD 15 00 02 01 CARGO FD 01 00 00 00 60 EA 00 00 00 00 00 00 00 00 00 00

WAKE REPORT CHANNEL activity:
Debug: RX  29 bytes HEAD 1D 80 04 05 CARGO FB B9 FC 1B 00 01 04 02 00 8E FB A5 02 89 F7 01 +13 more bytes ST [0]

Get Feature Request 0xFE
Debug: TX   6 bytes HEAD 06 00 02 02 CARGO FE 01

REPORT CHANNEL activity:
Debug: RX  69 bytes HEAD 45 80 03 01 CARGO FB 31 07 00 00 01 E0 02 00 85 FB 56 02 7F F7 01 +53 more bytes ST [0]

Get Feature Response 0xFC
Debug: RX  21 bytes HEAD 15 80 02 07 CARGO FC 01 00 00 00 00 7D 00 00 00 00 00 00 00 00 00 +5 more bytes ST [0]
Error: Not getting SHTP feature report
```

```
pi@nanopi-neo2:~/pi-bno080 $ ./getbno080 -t acc -v
Debug: ts=[1550282125] date=Sat Feb 16 10:55:25 2019
Debug: Sensor I2C address: [0x4B]
Debug: TX   5 bytes HEAD 05 00 00 01 CARGO 01
Debug: RX  19 bytes HEAD 13 80 03 03 CARGO FB 8E 00 00 00 01 E6 02 00 7B FB 56 02 7F F7 ST [0]
Debug: RX   5 bytes HEAD 05 80 00 05 CARGO 01 ST [0]
Debug: OK  Error list 0 entries
Debug: OK  Initialization complete
Debug: TX  21 bytes HEAD 15 00 02 01 CARGO FD 01 00 00 00 60 EA 00 00 00 00 00 00 00 00 00 00
Debug: RX 199 bytes HEAD C7 80 03 05 CARGO FB 80 8D A9 00 01 E7 02 00 7B FB 56 02 7F F7 01 +183 more bytes ST [0]
Debug: TX   6 bytes HEAD 06 00 02 02 CARGO FE 01
Debug: RX  19 bytes HEAD 13 80 03 07 CARGO FB CC 08 00 00 01 69 02 00 33 FE 32 FF 28 F6 ST [0]
Debug: RX  21 bytes HEAD 15 80 02 09 CARGO FC 01 00 00 00 00 7D 00 00 00 00 00 00 00 00 00 +5 more bytes ST [0]
Debug: FRS feature report received, [17 bytes]
```
