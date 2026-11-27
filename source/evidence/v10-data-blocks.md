# v10: Datenabrufe in ausgehandelter Blockgröße

## Reale Beobachtung v9

Quelle: v9-hardware.log und v9-image-header.bin. Das Papier wurde laut Nutzer vollständig eingezogen. Nach zwölf bestätigten Warteantworten01 lieferte der Scanner den ersten echten Bildheader: Frontkamera2, gray8,150dpi,9920×14032/1200Zoll,1240×1754Pixel,2174960Bytes, Chunknummer1 und letzterChunk1. Der Header passt zur strikten TIFF-Prüfung.

SCAN BEGIN hatte dmimagechunksize=2097152 bestätigt. v9 sendete dennoch drei separate IMAGE-DATA-Befehle:

| Abruf | Angeforderte Bytes | Status |
|---|---:|---|
| 1 | 1048576 | 00 |
| 2 | 1048576 | 00 |
| 3 | 77808 | 06 |

Anschließend STOP/END und vollständig verifizierter CONFIG-Restore, XML-Unlock erfolgreich. Keine TIFF-Datei: Der vollständige Empfang wurde nicht bestätigt. v9 speicherte die bereits bestätigten2097152Bytes nicht; sie sind nach Prozessende nicht aus dem vorhandenen Ausgabeordner rekonstruierbar. Der bisherige Log unterscheidet nicht, ob Status06 vor dem letzten Payload oder als dessen Abschluss kam.

## Änderung

Die Herstellerimplementierung CDevMgrProcessDI::ReadImage berechnet bei0xe0124..0xe0159 exakt min(verbleibendeBytes,dmimagechunksize). Dieser Wert wird bei0xe0381..0xe03c2 in CMD-Byte8..11 geschrieben. v9 wich davon durch eine zusätzliche1MiB-Kappung ab. v10 verwendet für das beobachtete Bild daher genau2097152 und77808Bytes. Die erste IOKit-Leseanforderung fordert ebenfalls den vollständigen Befehls-Payload an; tatsächliche kurze USB-Lieferungen werden weiterhin bis zur erwarteten Länge gesammelt.

Die Abweichung vom Herstellerablauf ist belegt. Dass sie die konkrete Firmwareablehnung verursacht, bleibt bis zum Gerätetest eine Hypothese. Es gibt weder automatische Variantenversuche noch Wiederholungen fehlgeschlagener IMAGE-DATA-Befehle.

Bei unvollständigem Empfang schreibt v10 nur den durch erfolgreiche Befehlsabschlüsse bestätigten Präfix als image.partial.raw. Diese Datei ist Diagnosematerial und kein vollständiger Scan. Bytes eines abgelehnten oder zeitlich abgebrochenen Befehls werden nicht angehängt. TIFF wird nur nach vollständigem Erfolg erzeugt. Die Protokollausgabe unterscheidet nun Status vor Payload von Status nach vollständig gelesener Nutzlast.

## Tests

22 synthetische Scanszenarien plus bestehende Protokoll-/Sitzungstests mit ASan/UBSan bestanden. Die Header-Fixture stammt jetzt direkt aus dem realen v9-Lauf; Pixel bleiben synthetisch. Tests erzwingen die Befehlsfolge2097152/77808 sowie vollständige initiale IOKit-Lesegrößen. Die64KiB-Variante und kurze USB-Lieferungen bleiben abgedeckt. TIFF-Inhalt wird bytegenau geprüft.

Neue Fälle: letzter Block vor Payload abgelehnt, Payload geliefert aber Abschlussstatus06, Timeout am letzten Block. In allen Fällen entsteht ausschließlich der bestätigte2MiB-Präfix, kein TIFF. Bei Timeout folgen keine weiteren Binärbefehle; XML-Unlock wird versucht. Die Tests scheiterten vor der Korrektur am zusätzlichen1MiB-Befehl. Intel-/ARM-Build erfolgreich. v10 noch nicht am Scanner getestet.
