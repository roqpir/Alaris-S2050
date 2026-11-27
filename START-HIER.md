# Alaris macOS Scan Tool – Version 0.13

1. App beenden, S2050 Scan.app nach Programme ziehen und die bisherige Version ersetzen.
2. App öffnen, Scanner verbinden und Papierstapel einlegen.
3. Auflösung und Farbmodus wählen; „Scan starten“ anklicken und PDF-Speicherort festlegen.

Neu:
- Papierhintergrund aufhellen: neutralen grauen Papierhintergrund auf Weiß korrigieren.
- Schwarzen Scanrand entfernen: erkannte schmale schwarze Randstreifen weiß füllen.
- Button: Scan starten.

Beide Korrekturen sind bereits aktiviert und separat abschaltbar. Für Fotos, sehr helle graue Gestaltung oder randfüllende Vorlagen die entsprechende Option ausschalten. Die Seitengröße bleibt erhalten. Die Original-Rohdaten werden nicht verändert.

Weiterhin:150/300/600 dpi; Farbe/Graustufen/Schwarzweiß; A4-Vorderseite; automatisches Stapelende bei leerem Einzug; ein gemeinsames PDF. Kein Duplex/OCR. Maximal100 Seiten.

Fertige Seiten bleiben bei Fehler oder Abbruch als Teil-PDF erhalten. Protokoll und TIFF-Arbeitsdateien:
~/Library/Application Support/S2050 Scan/Scans/

Korrektur an fünf lokal vorhandenen Rohscans geprüft, zusätzlich Softwaretests für alle Auflösungen, Farben, Schrift und Fehlerfälle. Die interaktive neue Oberfläche ist noch nicht geprüft.

Die App ist ad-hoc signiert, nicht notarisiert. Falls macOS den Start blockiert: Rechtsklick → Öffnen; gegebenenfalls Systemeinstellungen → Datenschutz & Sicherheit → Dennoch öffnen.

Quellcode und vollständige Beschreibung liegen in source/.
