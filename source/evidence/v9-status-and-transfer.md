# v9: USB-Status und Transfergrenze

## Beobachteter v8-Lauf

v8-hardware.log: SET CONFIG, CREATE CAMERA, ADD WINDOW, SET WINDOW ORDER und SCAN BEGIN wurden mit Status00 bestätigt. SCAN BEGIN antwortete mit `00 20 00 00 00 08 5a 58` (dmimagechunksize=2097152). Der erste IMAGE-HEADER-Abruf lieferte USB-Status01 ohne Bildheader. v8 behandelte ihn als Fehler und sendete unmittelbar STOP FEEDER und SCAN END. Beide erfolgreich. Restore und Readback waren bytegleich; XML-Unlock erfolgreich. Keine Bilddaten empfangen.

Der Nutzer beobachtete Papierbewegung, aber keinen Einzug und keine Displayfehlermeldung. Das frühe STOP ist im Log belegt; nach seiner Korrektur ist der reale Einzug noch zu testen.

## Falsche Statusinterpretation

`vendor-status-device.xml` ist der originale statusdevice-Abschnitt aus der eingebetteten manager-Ressource im Herstellerpaket Kodak Linux driver08.02.00.00.

| USB-Status | Interner Hersteller-Enum | Verhalten in ReadImage |
|---|---:|---|
| 00 | 0 | Daten auswerten |
| 01 | 16 (hex10) | Warten und Header erneut anfordern |
| 10 | 23 | Fehler zurückgeben |

Der bisherige Code verglich den rohen USB-Status mit hex10. Er hatte den internen Enum16 mit dem USB-Byte verwechselt. Der alte synthetische NOT_READY-Test enthielt denselben Fehler; v9 korrigiert ihn anhand des realen Logs und der Lookup-Tabelle.

Im Disassembly ReadImage ab0xdf59c wird der bereits übersetzte Rückgabewert mit0x10 verglichen. Dieser Pfad protokolliert „Sleep, no image ready“, wartet und springt bei0xdf681 zurück zum Headerabruf. Andere Fehlerwerte werden zurückgegeben. Auszug: read-image-pending-transfer.asm.

v9 wiederholt nur vollständig bestätigte Headerantworten mit rohem Status01, mit200ms Abstand und einer weiterhin begrenzten45s-Frist. Abbruchsignal, Transportfehler, falscher Tag und andere Statuswerte werden nicht als Wartestatus behandelt. USB10 wird ausdrücklich nicht wiederholt.

## Transfergrenze ist keine Gesamtbildgrenze

ReadImage liest imagesize aus dem Header beiOffset4 und berechnet bei0xe0124..0xe0159 die nächste Übertragung als min(verbleibende Bildbytes, dmimagechunksize). Anschließend wird opcode32 wiederholt, bis alle imagesize-Bytes empfangen sind. Die Gesamtbildgröße kann somit über dmimagechunksize liegen.

v8 verwarf fälschlich jedes Bild über der ausgehandelten2MiB-Grenze. Ein synthetisches A4-gray150-Bild mit1240×1754Pixeln enthält2174960Bytes und ist größer. v9 behält16MiB als maximale Gesamtgröße im Speicher und begrenzt jeden IMAGE-DATA-Befehl auf min(Rest, ausgehandelte Grenze,1MiB).

Dies implementiert mehrere USB-Datenabrufe für einen Bildheader. Mehrere Bildheader mit imagechunklast=0 werden weiterhin nicht zu einem TIFF zusammengesetzt; die bisherige Formatprüfung bleibt bestehen.

## Regressionstests

19 synthetische Szenarien mit ASan/UBSan erfolgreich. Alle erfolgreichen SCAN-BEGIN-Fixtures verwenden jetzt die real beobachtete8-Byte-Antwort. NOT_READY sendet drei rohe01-Antworten, danach den Bildheader; vor dem vollständigen Empfang darf kein STOP erscheinen. Weitere Fälle: roher Status10 stoppt ohne Wiederholung, Abbruch während des Wartens, simuliertes Überschreiten der45s-Frist, kleinere64KiB-Transfergrenze. TIFF-Pixel werden bytegenau über Transfergrenzen hinweg geprüft. Alle bisherigen Fehler-/Restore-/Sitzungstests bleiben erhalten.

Die neuen Tests scheiterten mit dem v8-Code zunächst an der Gesamtbildgrenze und nach deren Korrektur noch am Wartestatus. Nach beiden Korrekturen bestehen sie. Universal-Build x86_64/arm64 erfolgreich. v9 noch nicht am Scanner getestet.
