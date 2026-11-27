# S2050 Scan

Native macOS-App für den Kodak Alaris S2050 über USB. Intel und Apple Silicon, macOS 14 oder neuer. Kein Image-Capture- oder TWAIN-Treiber: die App spricht das Gerät direkt an und speichert ein PDF.

**Version 0.13.0.** Die App ist ad-hoc signiert und nicht notarisiert.

## Installieren

1. Das Release-Archiv öffnen und `S2050 Scan.app` nach Programme ziehen.
2. Beim ersten Start: Rechtsklick auf die App, dann **Öffnen**. Falls macOS blockiert: Systemeinstellungen → Datenschutz & Sicherheit → **Dennoch öffnen**.
3. Scanner per USB verbinden, andere Scanprogramme schließen, Papier einlegen.

## Scannen

1. Auflösung wählen: 150, 300 oder 600 dpi. Standard ist 150 dpi.
2. Farbmodus wählen: Graustufen, Farbe oder Schwarzweiß. Standard ist Graustufen.
3. **Scan starten** anklicken und Speicherort für das PDF festlegen.

Der Einzug läuft seitenweise, nur die Vorderseite, A4. Der Stapel endet beim ersten leeren Einzug. Es gibt keine Nachlegepause. Höchstens 100 Seiten, ein gemeinsames PDF. Kein Duplex und kein OCR.

**Papierhintergrund aufhellen** und **Schwarzen Scanrand entfernen** sind eingeschaltet und getrennt abschaltbar. Für Fotos, sehr helles graues Layout oder randfüllende schwarze Flächen die passende Option ausschalten. Die Seitengröße bleibt erhalten.

Bei Abbruch oder Fehler bleiben fertige Seiten als Teil-PDF erhalten. Protokoll und TIFF-Arbeitsdateien:

```text
~/Library/Application Support/S2050 Scan/Scans/
```

## Terminal

Im Ordner `source`:

```sh
./bin/s2050-probe --scan-stack "$HOME/Desktop/S2050-$(date +%Y%m%d-%H%M%S)" --dpi 300 --mode color
```

`--dpi 150|300|600`, `--mode gray|color|bw`. Dokumentkorrektur: `--paper-cleanup on|off` und `--edge-cleanup on|off`.

## Bauen

```sh
cd source
sh test.sh
sh build-app.sh
```

Xcode Command Line Tools genügen. Zur Laufzeit wird keine Kodak-Bibliothek geladen.

## Protokoll

USB-ID `29cc:1019`, drei Bulk-Paare. Ablauf, Versionshistorie und Hardwareläufe stehen in [source/PROTOKOLL.md](source/PROTOKOLL.md). Die Bedienung im Detail steht in [source/README.md](source/README.md).

Disassembly und originale Ressourcenausschnitte aus dem Kodak-Linux-Treiber liegen nur lokal und sind nicht in diesem Repository.
