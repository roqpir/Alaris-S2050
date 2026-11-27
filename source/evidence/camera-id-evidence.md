# Beleg zur Kamera-ID, v7

Statische Analyse von devicemanager.so aus Kodak Linux08.02.00.00, nicht ausgeführt.

- GetWindowOrder: cameratype==rear → Eintrag-ID3 bei0x97e2a; sonstID2 bei0x97e53. Rückgabe:Objekt+0x218; Eintragsgröße0x808.
- ScanSetup findet den passenden Kamera-Kontext in dieser Tabelle, liest den ersten32Bit-Wert bei0xda60d und setzt dmcameraid bei0xda626.
- v6 sendeteID1; v7 sendetID2 konsistent in CREATE CAMERA und WINDOW ADD und erwartet sie im Bildheader. cameratype/front bleibt0.
- Die mitgelieferten Assemblerausschnitte belegen diese Zuordnung. Die vollständige Protokollgeschichte steht in ../PROTOKOLL.md. ../README.md enthält den exakten Testbefehl.

Dies belegt die Codekorrektur. Ein erfolgreicher Hardware-Scan mitv7 ist noch nicht bestätigt.
