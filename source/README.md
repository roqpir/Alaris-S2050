# Alaris macOS Scan Tool 0.13

Kleine native macOS-App für den Alaris S2050 per USB. Intel und Apple Silicon, macOS 14 oder neuer. Einzelseitenscan (v0.10), Stapelbetrieb und PDF-Erstellung (v0.11) wurden vom Nutzer am Gerät bestätigt. Version 0.12 ergänzt DPI-Auswahl und Farbmodi. Version 0.13 ergänzt lokale Papieraufhellung und Randerkennung sowie den Button „Scan starten“. Noch kein ICA-/TWAIN-Treiber.

## App verwenden

1. S2050 Scan.app aus dem Paket nach Programme ziehen und öffnen.
2. Einen Stapel mit gewöhnlichen A4-Blättern einlegen. Andere Scannerprogramme schließen.
3. Auflösung (150, 300 oder 600 dpi) und Farbmodus (Graustufen, Farbe oder Schwarzweiß) wählen. Danach „Scan starten“ anklicken, Dateinamen und Speicherort festlegen.
4. Der Scanner verarbeitet die Seiten nacheinander. Beim ersten leeren Einzug endet der Vorgang automatisch. Es gibt keine Nachlegepause.
5. Das gemeinsame PDF liegt am gewählten Speicherort. „PDF öffnen“ und „Ordner anzeigen“ führen direkt zum Ergebnis.

Aktuell: Vorderseite, feste A4-Näherung. Standard bleibt Graustufen mit 150 dpi. 300 und 600 dpi sind auswählbar. Farbe wird als 24-Bit-RGB angefordert; Schwarzweiß wird aus einem Graustufenscan mit festem Schwellwert 128 in echte 1-Bit-TIFF-Bilddaten umgewandelt. 600 dpi erzeugt deutlich größere Arbeitsdateien, insbesondere bei Farbe. Kein Duplex oder OCR. Höchstens 100 Seiten pro Vorgang; bei weiter gefülltem Einzug wird die Grenze ausdrücklich als unvollständiger Auftrag gemeldet. Eine aktive Sitzung verhindert weitere Starts in derselben App. „Abbrechen“ und Beenden der App fordern Aufräumen an; ein laufender USB-Aufruf kann noch bis zu zehn Sekunden dauern.

Die App ist lokal ad-hoc signiert, nicht mit einer Developer-ID signiert oder notarisiert. macOS kann beim ersten Start eine Freigabe verlangen: Rechtsklick auf die App → Öffnen; gegebenenfalls Systemeinstellungen → Datenschutz & Sicherheit → Dennoch öffnen.

## Papierhintergrund und Scanrand

„Papierhintergrund aufhellen“ und „Schwarzen Scanrand entfernen“ sind standardmäßig aktiviert und separat abschaltbar. Die Korrektur wird vor der TIFF-/PDF-Erstellung lokal ausgeführt und gilt für alle drei Auflösungen und Farbmodi. Die Blattgröße bleibt erhalten; es wird nicht skaliert oder pauschal beschnitten. Schwarzweiß wird erst nach der Korrektur erzeugt.

Die Aufhellung erkennt einen überwiegend hellen, neutralen Papierhintergrund und setzt dessen Spitzlichter auf Weiß. Dunkle Werte bis180/255 sowie deutlich gesättigte Farben bleiben unverändert. Sehr helle graue Inhalte können wie Papier aufgehellt werden. Die Randerkennung ersetzt schmale, zusammenhängende dunkle Randbereiche mit anschließendem hellem Papier durch Weiß (maximal etwa6mm). Sie erkennt keine Dokumentsemantik: echte randfüllende schwarze Gestaltung kann ähnlich aussehen. Für Fotos, bewusst graues Papier oder randfüllende Vorlagen die entsprechenden Optionen ausschalten.

Die Originaldaten image.raw bleiben unverändert im Arbeitsordner. Ein Reprocessing von fünf vorhandenen lokalen Graustufen-Rohscans bestätigt: Papiermedian255 und keine dunklen Pixel unter100 mehr in den geprüften linken/oberen Streifen. Die beigefügte PDF konnte in dieser Umgebung nicht heruntergeladen werden; der praktische Vergleich verwendet die lokal vorhandenen Rohscans. Keine persönlichen Bildinhalte liegen im Downloadpaket. Synthetische Tests prüfen zusätzlich Schrift/Farben, schräge Kanten, alle Auflösungen, getrennte Schalter, schwarze Flächen, Verläufe und verteilte Randtexturen. USB-/Geräteeinstellungen wurden für diese Korrektur nicht erweitert.

## PDFs und erhaltene Seiten

Alle Seiten werden in Einzugsreihenfolge zu genau einem PDF zusammengefasst. TIFF-Dateien werden lokal in PDF eingebettet; keine Cloud, kein externer Konverter und keine zusätzliche Installation.

Bei Abbruch, Scanfehler oder fehlgeschlagener USB-Bereinigung werden bereits vollständige Seiten als scan-partial.pdf erhalten. Die App zeigt das ausdrücklich als Teil-PDF an und öffnet es über „PDF öffnen“. Das gewählte Zieldokument wird in diesem Fall nicht ersetzt. Scheitert das Speichern am gewählten Ort, bleibt die fertige Arbeitskopie erhalten.

Arbeitsdateien, TIFF-Seiten und Protokoll bleiben hier:

```
~/Library/Application Support/S2050 Scan/Scans/<Vorgangs-ID>/
```

„Protokoll öffnen“ öffnet das Protokoll des letzten Vorgangs. Bei fehlgeschlagenem Scan öffnet „Ordner anzeigen“ die erhaltene Arbeitskopie bzw. den Vorgangsordner. Arbeitsdaten werden nicht automatisch gelöscht; sie können Dokumentinhalte enthalten.

## Ablauf und Nachweisgrenzen

Stapelbetrieb wiederholt den funktionierenden Einzelseitenablauf innerhalb einer USB-/Ressourcensitzung. Nach jeder Seite folgen STOP/END sowie die vollständige Wiederherstellung der ursprünglichen Konfiguration. Danach wird der Papiersensor geprüft: leer beendet den Auftrag sofort. Es wird nicht auf neue Blätter gewartet. Diese Variante kann langsamer sein als ein durchgehender Hersteller-Stapelscan. Während des Vorgangs keine Blätter nachlegen.

Der Nutzer hat v0.11 mit Stapel/PDF als funktionierend bestätigt. Farbe sowie 300/600 dpi sind in v0.12 implementiert, aber noch nicht am echten Gerät bestätigt. Softwaretests bestehen: die bisherigen 22 Scanprotokollfälle plus acht weitere vollständige simulierte A4-Transfers für alle neun Auflösungs-/Farbmodus-Kombinationen; 11 Stapelfälle; TIFF/PDF-Dekodierung mit Kanalreihenfolge, Schwarzweiß-Polarität, Schwellwertgrenze, nicht durch acht teilbaren Zeilenbreiten und identischer physischer PDF-Seitengröße. Ungültige Optionen und nicht unterstützte Bildlayouts werden abgewiesen. App-Selbsttest, Universal-Build (x86_64/arm64) und Codesign-Prüfung erfolgreich. Eine interaktive Prüfung der neuen Oberfläche war in der Entwicklungsumgebung nicht möglich.

Die USB-Blockaushandlung bleibt unverändert. Das vollständige Bild darf jetzt bis zu 128 MiB umfassen; damit passen auch unkomprimierte A4-RGB-Bilder bei 600 dpi. Nach Erhalt des Headers ist die Datenphase auf 120 Sekunden begrenzt. Kompression, mehrteilige Bildheader, unbekannte Zeilenabstände und abweichende DPI werden nicht erraten; Diagnose-Rohdaten bleiben erhalten.

## Terminal

```sh
./bin/s2050-probe --scan-stack "$HOME/Desktop/S2050-$(date +%Y%m%d-%H%M%S)" --dpi 300 --mode color
```

Optionen: `--dpi 150|300|600` und `--mode gray|color|bw`; ohne Optionen gilt 150 dpi/Graustufen. Dieselben Optionen gelten für `--scan-one`. Dokumentkorrektur: `--paper-cleanup on|off` und `--edge-cleanup on|off` (beide standardmäßig on).

Ergebnis: scan.pdf im neuen Ausgabeordner; einzelne TIFF-Dateien in page-001, page-002 usw. Kein manuelles Zusammenführen erforderlich. Auch `--scan-one NEUER_ORDNER` erzeugt jetzt automatisch scan.pdf zusätzlich zu scan.tiff. Bei unvollständigem Auftrag heißt ein aus fertigen Seiten erzeugtes PDF scan-partial.pdf. Ausgabeordner müssen neu sein; vorhandene Dateien werden vom Kommandozeilenprogramm nicht überschrieben.

## Bauen und testen

```sh
sh test.sh
sh build-app.sh
"dist/S2050 Scan.app/Contents/MacOS/S2050Scan" --self-test
```

Xcode Command Line Tools/macOS-SDK erforderlich. Keine Herstellerbibliothek zur Laufzeit. Die App enthält das Hilfsprogramm selbst, benötigt weder Homebrew noch Python. PDF-Erstellung nutzt CoreGraphics/ImageIO, Oberfläche AppKit. Scaninterface 14, USB-ID 29cc:1019. Keine Firmware-/Kalibrierungsbefehle, keine automatischen Resets. Weitere Protokollbelege und Entwicklungshistorie: PROTOKOLL.md und evidence/.
