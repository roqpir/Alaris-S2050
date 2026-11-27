# v8: Kamerawerte und Stapelaktion

## Reale Beobachtung

Quelle: v7-hardware.log und v7-config-{before,test,after}.bin vom 26.11.2026.

- SET CONFIG: Status 00.
- CREATE CAMERA: 36 Bytes, ID 2, alle übrigen Bytes 0; Status 06.
- WINDOW ADD, WINDOW ORDER und SCAN BEGIN nicht erreicht.
- RESTORE CONFIG/VERIFY: Transport und Status erfolgreich, aber ausschließlich Byte 13 verändert: 00 → 01. Feld laut Interface-14-Layout: batchendfunction.
- XML-Unlock erfolgreich.

Das widerlegt die Annahme, die ID-Korrektur allein würde die Einrichtung vollständig reparieren. Status 06 enthält keine Diagnose eines einzelnen Parameters.

## Herstellerquelle

Statische Untersuchung von driver.so aus Kodak Linux driver 08.02.00.00. Eingebettetes Default-Profil ab Dateioffset 0x49a7ec; Lookup ab 0x4a123b. Die relevanten Original-XML-Einträge und der SHA-256 der untersuchten Datei stehen in vendor-camera-defaults.xml. Keine Herstellerbibliothek wird ausgeführt oder mit dem Prototyp ausgeliefert.

Lookup-ID 8 bezeichnet autocoloramount, ID b autocolorthreshold. Zahlen im kompilierten Profil stehen hexadezimal. Für BF und CF sind identische Range-Einträge enthalten: e = Vorgabe, f = Minimum, g = Maximum, h = Schritt.

| Feld | Standard | Minimum | Maximum | CREATE-CAMERA-Offset |
|---|---:|---:|---:|---:|
| autocoloramount | 1 | 1 | 200 (c8) | 3 |
| autocolorthreshold | 20 (14) | 0 | 100 (64) | 4 |

Die Byteoffsets stammen aus der createcamera-Ressource in devicemanager.so; beide Felder verwenden convert=4 und je ein Byte. Das mitgelieferte Auto-Color-Profil 20060526175022901.profile enthält als separate benutzerseitige Vorgabe amount=9, threshold=20. v8 verwendet die oben angegebenen Werte des Default-Profils, nicht die Auto-Color-Einstellung 9.

Kamerablock v8: 02 00 00 01 14, gefolgt von 31 Nullbytes. Automatische Farberkennung bleibt aus. Keine spekulative Änderung der Barcodefelder: CoerceCreateCameraSettings kann bei hostseitiger Barcodeverarbeitung explizit barcodeenable=false, alle Barcodetypen und barcodecount=0 setzen.

## Rücksetzung

v8 setzt batchcountenabled (Offset 8) auf 0 und erhält die Originalbytes 9–13. Statt zusätzlich aktivierter Stapelverarbeitung begrenzen imagecount und sheetcount den Test auf eins. Ursache der Firmware-Rückleseabweichung ist nicht abschließend geklärt; das beobachtet problematische Schreiben der Endaktion wird vermieden. Restore bleibt ein vollständiger Vergleich aller 757 Bytes. Eine erneute Abweichung führt weiterhin zu Fehlerstatus und wird gespeichert und ausgegeben.

## Nachweisgrenzen

Die Verletzung des Hersteller-Wertebereichs bei amount=0 ist belegt. Dass genau sie die Ablehnung durch diese Firmware verursacht, bleibt eine zu prüfende Hypothese. Der neue Schwellwert 20 ist ebenfalls ein belegter Standardwert, während der bisherige Schwellwert 0 innerhalb des Wertebereichs liegt. Synthetische Tests ersetzen keinen Hardwarelauf.
