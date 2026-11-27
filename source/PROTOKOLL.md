# S2050: belegter Stand vom 26.11.2026

Quelle: offizielles Linux-Paket LinuxSoftware_s2000_v8.2.x86_64.deb.tar.gz, darin kodak_s2000-08.02.00.00-1.amd64.deb. Vom Anwender heruntergeladen; nur entpackt und statisch analysiert, nicht installiert oder ausgeführt. Herstellerbibliotheken werden nicht mit diesem Prototyp weitergegeben.

## Endpunktzuordnung

In device.so enthält die eingebettete Ressource deviceconfig/common/reportinquiry/devosusbopen die Zuordnung für die S2000-Modelle, einschließlich S2050 mit VID/PID 29cc:1019:

| Endpunkte OUT / IN | Herstellerbezeichnung | Rolle laut Treiber |
|---|---|---|
| 01 / 81 | LOCKOUT / LOCKIN | Zugriffssperre |
| 02 / 82 | BULKOUT / BULKIN | Befehls-/Datentransfer |
| 03 / 83 | BULKOUTINTERRUPT / BULKINTERRUPT | Ereigniskanal, MonitorButtonEvents |

Konfiguration 1, Schnittstelle 0, Alternate Setting 0. „Interrupt“ ist hier die interne Kanalbezeichnung; USB-Deskriptoren weisen alle sechs als Bulk aus.

## INQUIRY

In devicemanager.so enthält die Ressource devicemanager mit device=sage und commandprotocol=2 unter create[name=inquiry] folgenden 20-Byte-commandreceive:

```text
43 4d 44 00 00 00 00 02 00 00 00 ed 80 10 00 00 00 00 00 00
```

Die Vorlage sieht 237 Byte vor; v4 passt die Länge an die zuvor ausgehandelte Schnittstellenversion an. Die Ressource ordnet die Felder zu:

| Byteoffset | Länge | Feld |
|---|---|---|
| 0 | 2 | inquiryversioninterface |
| 2 | 10 | versionfirmware |
| 12 | 16 | serialnumber |

Der Probe-Code gibt Version vorläufig als Big-Endian aus. Diese Interpretation muss zusätzlich an der realen Antwort plausibilisiert werden.

## Übertragung und Abschluss

Statische Analyse von CDevMgrProcessDI::SendCommandtoScanner, Adresse 0x0e3f24, in devicemanager.so: Befehlsblock senden, bei Empfangsrichtung Daten lesen, anschließend Abschluss lesen. Eine vorzeitige Statusantwort anstelle der Nutzdaten wird ebenfalls berücksichtigt.

Der Konstruktor CDevMgrProcessDI setzt die Statuslänge bei 0x0eab2a auf 12 Byte. SendCommandtoScanner prüft die Kennung STS gefolgt von NUL. Der Statuscode wird bei 0x0e4ab6 aus Offset 8 gelesen. Die Ressource statusdevice bildet Gerätecode 0 auf Erfolg ab. Der Prototyp verlangt eine vollständige Nutzdatenantwort und einen formal gültigen Abschluss mit Code 0, bevor er Geräteinformationen als bestätigt ausgibt.

## Offene Punkte

- INQUIRY ohne Anmeldung am realen Gerät: USB-Schreiben erfolgreich, Antwort-Timeout 0xe0004051.
- Ressourcenanmeldung und Freigabe am realen Gerät bestätigt. INQUIRY ohne Versionsaushandlung wird mit STS 0x06 abgelehnt; Scanner zeigt Hostkommunikationskonflikt. v4 mit Aushandlung wurde anschließend erfolgreich am Gerät getestet.
- Keine Aussage, dass weitere Befehle ohne vorherige Initialisierung funktionieren.
- HTTP-bezogene Funktionen sind ebenfalls in der Bibliothek vorhanden. Einige USB-Implementierungen davon sind in diesem Linux-Build jedoch Stubs; daraus wird keine HTTP-over-USB-Unterstützung des S2050 abgeleitet.
- Keine Scan-, Kalibrierungs-, Profiländerungs- oder Firmwarebefehle im Prototyp. Ressourcenanmeldung und Freigabe ändern nur den Sitzungszustand.

## Ressourcenanmeldung in v0.3

Statische Primärquelle: dieselbe Herstellerbibliothek devicemanager.so.

- COsUsbImpl::GetTaskLock (0x1b10f0): StartTask, StartCommand resourcelock, Argumente userid und pcid, FinalizeCommand/FinalizeTask. Die alternative Variante mit getinquiry=true wird in v3 nicht verwendet.
- COsXmlTaskImpl::StartTask (0x2181c0): XML beginnt ohne XML-Deklaration mit task und Attributen bytes, id, reply, jeweils 20 Dezimalstellen. Die Länge umfasst das gesamte XML ohne NUL. Die Feldpositionen sind 13, 39 und 68.
- COsXmlTaskImpl::FinalizeTask (0x216aa0): Abschluss </task> plus LF; anschließend Eintragen der Gesamtlänge.
- COsUsbImpl::DeviceLock (0x1dab00): Schreiben über Ioctl 0x2d, Lesen über 0x2c. Erfolg anhand status=success; lockid wird übernommen (0x1dd11e). Die Endpunkte sind laut device.so LOCKOUT=01 und LOCKIN=81.
- COsUsbImpl::DeviceUnlock (0x1e5670, XML-Aufbau ab 0x1e5a7e): resourceunlock mit lockid als Dezimalzahl.

v3 verwendet einen eigenen temporären pcid-Bezeichner. Anfrage-ID 1 für Sperren, 2 für Entsperren. Ab v3.1 verlangt der XML-Parser entweder reply gleich Anfrage-ID oder reply=0 zusammen mit id gleich Anfrage-ID. Beide Attribute müssen gültige Dezimalzahlen sein. Eine fremde nichtnull-reply-ID wird auch bei passender id abgewiesen. Zusätzlich verlangt er genau einen Erfolgsstatus und beim Sperren genau eine positive 32-Bit-lockid. Eine gelieferte lockmode muss normal sein. Unbekannte Antworten werden ausgegeben und nicht als Erfolg interpretiert. Diese bewusste Einschränkung kann eine bislang unbekannte gültige Firmwareantwort ablehnen.

XML-Empfang ist auf 4096 Byte und acht Lesevorgänge mit jeweils maximal fünf Sekunden begrenzt. Teilantworten werden anhand bytes zusammengesetzt. Kein Lesen fremder Kanäle, keine Wiederholung fehlgeschlagener Transaktionen, keine erzwungene Freigabe fremder Sperren. Die Transporttests sind simuliert. test-session.c enthält zusätzlich die vom Anwender gepostete 285-Byte-Antwort mit wiederhergestellten Zeilenumbrüchen; ein roher USB-Mitschnitt liegt nicht vor.

## Gerätebeleg und Korrektur v0.3.1

Der Anwenderlauf mit v3 lieferte eine vollständige XML-Antwort auf Endpunkt 81: task bytes=285, id=1, reply=0; reportlock/lockid=174647827; reportstatus/status=success; vid=0x29CC; pid=0x1019. Damit sind Ressourcenanmeldung und Kanalzuordnung am Gerät bestätigt. Die bisherige Forderung reply=1 war eine unzutreffende Annahme des Prototyps.

v3.1 akzeptiert die beobachtete Zuordnung id=Anfrage-ID bei reply=0. Die Regression prüft sowohl diese Antwort als auch den simulierten Gesamtablauf einschließlich Freigabe derselben lockid. Die simulierte Freigabeantwort mit id=2, reply=0 wurde noch nicht am Gerät beobachtet. Tests für fremde, fehlende, ungültige und widersprüchliche IDs bleiben aktiv. Vor dem nächsten Hardwaretest muss die durch v3 erworbene, aber nicht freigegebene Sitzung durch Aus-/Einschalten beendet werden.

## Fehlende Versionsaushandlung und v0.4

Realer Lauf v3.1: lockid=1925002407; binäre Antwort `53 54 53 00 00 00 00 02 06 00 00 00`; danach erfolgreich bestätigtes resourceunlock. Die XML-Tabelle statusdevice bildet Gerätecode 000006 auf internen Enum 1 ab. Daraus allein lässt sich keine spezifische Fehlerbezeichnung ableiten. Die Scanneranzeige wurde vom Anwender als Hostkommunikationskonflikt gemeldet.

Beim Auflösen von sessionbegin/callfunction 1 wurde ein ausgelassener Schritt gefunden:
- ProcessFunction bei 0xe20c0: Sprungtabelle 0x3be4c0, Eintrag 1 führt nach 0xe22a5.
- Dort HandleSpecialApplicationRules, danach bei 0xe2321 CheckInterfaceVersion (0xe1238), anschließend CheckMgmtInterfaceVersion. Erst nach Rückkehr erfolgt die XML-Anweisung inquiry.
- CheckInterfaceVersion baut folgenden 20-Byte-Befehl auf (Hostversion 15 stammt aus hostinterfaceversion der Herstellerressource):

```text
43 4d 44 00 00 00 00 01 00 00 00 02 80 01 00 0f 00 00 00 00
```

Tag 1, erwartete Antwort 2 Byte, Opcode 0x01. Das Ergebnis wird bei 0xe1642 als Big-Endian gelesen und als versioninterface gespeichert. Danach ruft der Treiber UseInterface auf. CDevMgrCreate::UseInterface (0x9142a) setzt die Nutzdatenlänge auf das Ende des letzten für die Version unterstützten Antwortfeldes und aktualisiert die CMD-Bytes 8–11. Alle untenstehenden Längen wurden zusätzlich über die kumulierten Feldgrößen nach diesem Algorithmus geprüft:

| Version | INQUIRY-Bytes |
|---|---|
| 1–2 | 108 |
| 3 | 110 |
| 4 | 123 |
| 5–6 | 131 |
| 7 | 132 |
| 8 | 148 |
| 9 | 190 |
| 10–11 | 198 |
| 12 | 223 |
| 13 | 235 |
| 14 | 236 |
| 15 | 237 |

CheckMgmtInterfaceVersion überspringt seine Abfrage bei Managementversion 0 (0xcb31d). Die sage-Ressource enthält keine mgmtinterfaceversion; v4 implementiert keine separate Managementabfrage. Sollte sie für das reale Gerät erforderlich sein, bleibt dies zu prüfen.

v4: XML-Lock → VERSION → INQUIRY → XML-Unlock. VERSION muss zwei Datenbytes sowie einen passenden STS-Abschluss (Tag 1, Status 0) liefern. Werte außerhalb 1–15 werden abgewiesen. Für frühe STS-Fehler steht beim ersten VERSION-Lesen ein 12-Byte-Puffer bereit. Kein automatisches Wiederholen bei Fehlern. Nach jedem bestätigten Lock wird die Freigabe auch bei Versionsfehlern versucht. --inquiry ist ab v4 ebenfalls an diesen Ablauf gebunden. Der fehlende Versionsschritt ist im Prototyp belegt; die anschließende Geräteabfrage wurde im Anwenderlauf v4 erfolgreich bestätigt.

## Gerätebeleg v4 und Konfigurationsabfrage v0.5

Anwenderlauf v4: VERSION liefert 00 0e und erfolgreichen STS mit Tag 1. INQUIRY mit Länge 236 liefert Firmware 180503, Seriennummer 61144406 und erfolgreichen STS mit Tag 2. Ressourcenfreigabe ebenfalls success. Die 236 Nutzdatenbytes sind in test-inquiry-v14.h exakt aus dem geposteten Terminal-Hexdump übernommen.

v5 ergänzt --session-config: Nach dem bewährten Ablauf CONFIG lesen (Tag 0x0e, Opcode 0x20, Richtung 0x80), danach getpaperdetected (Tag 0x0d, Opcode 0x17, Richtung 0x80, ein Byte, 0=false/1=true). Beide stammen aus der sage-Ressource. CONFIG-Länge für Version 14 ist 757 Byte. Versionsabhängige Längen für 1–15: 545,545,545,552,564,564,697,698,716,738,739,744,757,757,757. Berechnung analog UseInterface anhand kumulierter Feldgrößen bis zum letzten unterstützten Feld.

Der Prototyp gibt CONFIG vollständig als Hexdump aus. Ausgewählte Feldpositionen: adfautostart=0, adfhandling=1, adftimeoutenabled=2, adftimeout=3/2 Byte, adftimeoutfirstpage=5/2, batchcount=9/2, imagecount=86/2, papersource=104, printerenabled=118, adfseparation=739, adfactivefeed=740, sleevedocument=742. Mehrbyte-Zahlen Big-Endian. Es wird nur commandreceive verwendet, niemals commandsend.

Noch offen: reale CONFIG- und Sensorantworten, Fensterdefinitionen (windowadd), setwindoworder, Scanstart und Bildübertragung. Der Herstellerablauf schreibt vor scanbegin die Konfiguration und definiert Bildbereiche. Diese Schritte sind noch nicht als Hardwaretest freigegeben; v5 enthält ausschließlich die beschriebenen Leseabfragen mit Sitzungsverwaltung. Tests prüfen zusätzlich Freigabe nach CONFIG-/Sensorfehlern und dass nach fehlgeschlagenem CONFIG keine Sensorabfrage folgt.


## Gerätebeleg v5 und experimenteller Scan v0.6

Der Anwenderlauf v5 bestätigt CONFIG mit genau 757 Byte und STS Tag0e/Status0. PAPER DETECTED liefert 00 und STS Tag0d/Status0, anschließend erfolgreiche XML-Freigabe. Die zuvor offenen Leseabfragen sind damit am Gerät bestätigt. Der Einzug war leer.

v6 verwendet ausschließlich den inzwischen identifizierten Ablauf des sage-Protokolls, zunächst nur für Interface14. Es handelt sich um einen ersten Hardwaretest, nicht um einen bereits verifizierten Scannertreiber.

| Schritt | Tag | Opcode | Richtung | Bytes |
|---|---|---|---|---|
| CONFIG lesen | 0e | 20 | IN | 757 |
| Papier erkannt | 0d | 17 | IN | 1 |
| CONFIG schreiben | 0f | 20 | OUT | 757 |
| CREATE CAMERA | 10 | 21 | OUT | 36 (Interface14) |
| WINDOW ADD | 11 | 22 | OUT | 87 |
| WINDOW ORDER | 12 | 23 | OUT | 4 |
| SCAN BEGIN | 13 | 30 | IN | 8 |
| REQUEST HEADER | 14 | 31 | IN | 213 |
| IMAGE DATA | ff | 32 | IN | gewünschte Teilmenge |
| SCAN REQUEST STOP | 15 | 33 | keine | 0 |
| SCAN END | 16 | 34 | IN | 12 |

Quelle: create-Felder und scanbegin/scanend-Skripte der sage-Ressource. COPY/Transport separat neu implementiert; keine Herstellerbibliothek wird geladen oder mitgeliefert. SendCommandtoScanner bei 0xe3f24 sendet CMD, gegebenenfalls OUT-Daten, dann liest es den STS-Abschluss. Bei IN kommen Daten und anschließend STS. Fehler-STS kann bereits anstelle der Daten zurückkommen.

ReadImage bei 0xdf1f6 konstruiert REQUEST HEADER mit Tag14/Opcode31. Bei abgeschlossenem Status16 wird dort erneut abgefragt. Für den eigentlichen Bildabruf konstruiert es bei 0xe030f ff/Opcode32, setzt die angeforderte Länge und wiederholt für verbleibende Bytes. v6 begrenzt die Einzelbefehle auf 1 MiB. Header-Bildgröße steht bei Offset4/4 Byte. Breite und Höhe stehen bei21/4 und25/4 in Einheiten 1/1200 Zoll: die JPEG-Konstruktion bei0xdffdb dividiert durch1200/dpi. v6 erstellt TIFF nur bei passender unkomprimierter gray8-Länge ohne unklare Zeilenauffüllung, mit einem vollständigen Chunk und ohne Mehrfacheinzugsmeldung.

SCAN BEGIN enthält dmmaxchunksize bei CMD14..17, dmdeliverrawimages bei18. v6 fordert höchstens16MiB an und keine zusätzlichen Raw-Kamerabilder. Antwort: dmimagechunksize bei0/4, dmimageheightmax bei4/4. Der vollständige Header wird gesichert, bevor Bilddaten angefordert werden.

CREATE CAMERA ist für Interface14 36 statt37 Byte, weil autocolorignorebackground beiOffset36 erst abVersion15 existiert. Kamera-ID1 und Fenster-ID0 werden konsistent verwendet. WINDOW ORDER setzt unbenutzte IDs auf255; ScanSetup initialisiert diese bei0xdb164 aufff. Der Standardbildbereich beträgt9920×14032/1200Zoll, 150dpi. Physische Graustufen sind imageformat1, windowtype color1. Keine Kompression, kein Dropout (ecdo255), keine Schrift (fontid255).

Besonders relevant: convert13 ist KEIN Zweierkomplement. ConvertDatabaseToDevice Sprungtabelle0x3c6c0c, Eintrag13→0x116c17: bei0x116c6b wird100 addiert. Deshalb werden neutrale Helligkeits-/Kontrast-/Aggressivitätsfelder mit100 statt0 geschrieben.

Die Originalkonfiguration wird aus dem Gerät übernommen, vor jeder Änderung lokal gesichert und nach bestätigtem SCAN END (bzw. Fehler vor erfolgreichem SCAN BEGIN) zurückgeschrieben und vollständig verifiziert. Bei unklarem Transportzustand keine weiteren Binärbefehle. XML-Unlock läuft auf dem getrennten Ressourcenkanal. Persistente Firmware-, Kalibrierungs- oder Resetbefehle werden nicht implementiert. Die Kamera-/Fensterdefinitionen haben in diesem Prototyp keinen Readback und werden nicht wiederhergestellt.

Validierung: Bestehende Tests plus zwölf synthetische Scan-/Fehlerfälle, ASan/UBSan erfolgreich. Universal Build x86_64+arm64 mit Mindestversion14.0. Synthetische TIFF-Ausgabe durch macOS sips geprüft:1240×1754,150dpi. Hardware-Scan und tatsächliche Bildanordnung bleiben bis zum nächsten Anwenderlauf offen.


## Reale CREATE-CAMERA-Ablehnung in v6 und Korrektur v0.7

Im v6-Anwenderlauf wurde SET CONFIG757 mit Status00 bestätigt. CREATE CAMERA (Tag10/Opcode21,36Byte, erste Byte01, Rest00) lieferte Status06. Die Anzeige laut Nutzer: „Unzulässige Auftragseinrichtung“. WINDOW ADD und SCAN BEGIN wurden nicht gesendet. Restore-Schreiben und Verify-Lesen lieferten00, aber der Bytevergleich scheiterte. Der Readback wurde noch nicht gespeichert. XML-Unlock war erfolgreich. Vollständiger Lauf: evidence/v6-hardware.log.

Die in v6 gewählte Kamera-ID1 war nicht aus dem Hersteller-Mapping abgeleitet. Die jetzt untersuchte Funktion CDevMgrDatabase::GetWindowOrder (0x97cac..0x97e90) stellt das Mapping klar: Nach cameratype-Vergleich mit „rear“ bei0x97dfa wird bei0x97e2a die3 geschrieben; andernfalls bei0x97e53 die2. Der Tabelleneintrag beginnt beiObjekt+0x218, Stride0x808. Der Rückgabepointer wird bei0x97e75 aufObjekt+0x218 gesetzt. ScanSetup liest den ersten32Bit-Wert des passenden Eintrags bei0xda60d und setzt damit dmcameraid bei0xda626. Derselbe Wert wird für WINDOW ADD benutzt. Somit ist für unsere Vorderseite ID2 zu verwenden, während cameratype weiterhin0 ist. Das sind unterschiedliche Felder.

v7 korrigiert Kamera-Payloadbyte0, WINDOW-ADD-Byte2 und die Erwartung für ImageHeader-Byte0 auf2. Kein Durchprobieren alternativer IDs. Die Länge36 wurde erneut gegen die CREATE-CAMERA-Felder und UseInterface überprüft: cumulierte Feldgrößen bis zum letzten beiVersion14 unterstützten Feld ergeben36; autocolorignorebackground amOffset36 ist erst fürVersion15 vorgesehen. Keine Änderung der übrigen Kamera- oder Konfigurationswerte in dieser Version.

Restore-Diagnose: config-after.bin wird ausschließlich nach vollständig erfolgreichem Readback gespeichert. Ein generierter Interface14-Feldindex (config-fields.h) benennt alle136 unterstützten CONFIG-Felder; er dient nur der Diagnose, niemals zum Maskieren von Abweichungen. Alle abweichenden Bytepositionen erscheinen als CONFIG DIFFERENCE mit Feldname, Original und Readback. Differenzen/Dateifehler führen weiterhin zu einem Fehlerresultat und anschließendem XML-Unlock. Vorhandene Diagnose-Dateien werden nicht überschrieben.

Validierung: Bestehende Tests und15 Scanszenarien unterASan/UBSan erfolgreich, Universal Build x86_64/arm64. Drei neue Szenarien führen den vollständigen Sessionablauf mit Kameraablehnung, verändertem Readback und Dateikollision durch; alle bestätigen Ressourcenfreigabe. Der nächste Hardwarelauf muss noch zeigen, ob damit weitere Auftragsschritte und schließlich der Bildabruf funktionieren. Status06 identifiziert allein nicht das genaue fehlerhafte Feld; ID2 ist eine belegte Korrektur, kein Beweis für vollständige Gerätefunktion.

## v0.8: CREATE CAMERA weiterhin abgelehnt; ungültige Farbmenge korrigiert

Der reale v7-Lauf liegt in evidence/v7-hardware.log. SET CONFIG wurde akzeptiert, CREATE CAMERA (ID2) erneut mit06 abgelehnt. Kein SCAN BEGIN. Die gespeicherten Original-/Test-/Readback-Konfigurationen zeigen genau eine Restore-Abweichung: batchendfunction anOffset13 blieb1 statt0.

Die eingebettete Default-Datenbank in driver.so belegt für autocoloramount Standard/Minimum1, Maximum200. Die bisher gesendete0 liegt außerhalb dieses Bereichs. v8 sendet camera[3]=1 und camera[4]=20 (Standard-Schwellwert); Detektion bleibt aus. Byteoffsets und Länge bleiben gemäß Interface14. v8 deaktiviert außerdem batchcountenabled und erhält die Originalbytes9–13; imagecount/sheetcount bleiben1. Keine Restore-Abweichung wird unterdrückt.

15 synthetische Szenarien und bestehende Protokoll-/Sitzungstests bestehen mit ASan/UBSan; zusätzliche Assertions prüfen beide ursprünglichen Stapelzustände und Endaktionen. Der simulierte Restore-Unterschied reproduziert jetzt exakt den realen Offset13. Universal-Build erfolgreich. Kein Hardwaretest vonv8; ob die Wertebereichskorrektur Status06 beseitigt, bleibt offen. Quellen und Belege: evidence/camera-defaults.md, evidence/vendor-camera-defaults.xml.

## v0.9: Einrichtung bestätigt; vorzeitiges STOP nach USB-Status01 korrigiert

Realer v8-Lauf: alle Setup-Befehle sowie SCAN BEGIN erfolgreich. SCAN BEGIN liefert0020000000085a58. Erster IMAGE HEADER: Status01 ohne Header. v8 sendet unmittelbar STOP/END. Restore bytegleich, XML-Unlock erfolgreich. Nutzer: Blatt bewegt, aber nicht eingezogen; keine Fehlermeldung am Display.

Korrektur: Der Hersteller übersetzt USB01 in den internen Enum16 (=hex10). ReadImage wartet auf diesen internen Wert; unser direkter USB-Client muss daher auf01 warten, nicht auf10. v9 wiederholt nur diesen vollständig empfangenen Wartestatus,200ms Abstand,45s-Frist. Der bisherige NOT_READY-Test hatte dieselbe Verwechslung und ist korrigiert.

Zweiter belegter Fehler: dmimagechunksize begrenzt einzelne Datenabrufe, nicht imagesize. v9 akzeptiert Gesamtbilder bis16MiB und sendet IMAGE DATA in min(Rest,dmimagechunksize,1MiB)-Abrufen. Reale2MiB-Antwort und kleinere64KiB-Grenze werden getestet; TIFF-Pixelinhalt wird über die Abrufgrenzen geprüft.

19 synthetische Szenarien plus Protokoll-/Sitzungstests bestehen mit ASan/UBSan. Neue Tests scheiterten mitv8 an beiden Fehlern; nach den Korrekturen erfolgreich. Universal-Build erfolgreich. Hardwaretest vonv9 und erster realer Bildheader/Bildempfang stehen aus. Details/Originalbelege: evidence/v9-status-and-transfer.md.

## v0.10: Erstes Papier und Bildheader, Datenblockgrenzen angepasst

Realer v9-Lauf: Papier durchgezogen, zwölf Wartestatus01, dann gültiger213-Byte-Bildheader für2174960Bytes/1240×1754gray150. Chunknummer1, letzterChunk1. Zwei separate1MiB-Datenbefehle erfolgreich, dritter Befehl77808Bytes mit06 abgelehnt. STOP/END, CONFIG-Restore und XML-Unlock erfolgreich. Kein TIFF und keine erhaltenen Rohpixel, da v9 nur vollständige Bilder speicherte.

v10 entfernt die zusätzliche1MiB-Befehlsgrenze und fordert gemäß ReadImage min(Rest,dmimagechunksize) an:2097152/77808Bytes. Auch die initiale IOKit-Lesegröße entspricht dem vollen Befehls-Payload; kurze Lieferungen werden weiterhin gesammelt. Bei Fehler werden ausschließlich vorher erfolgreich bestätigte Blöcke als image.partial.raw gesichert. Fehler vor Payload und Abschlussfehler nach Payload werden unterschieden.

22 synthetische Szenarien mit realer Header-Fixture und bytegenauer TIFF-Prüfung bestehen mit ASan/UBSan; neue letzte-Block-Ablehnungs-/Timeoutfälle bestätigen die Sicherung des2MiB-Präfixes ohne unbestätigte Bytes. Universal-Build erfolgreich. Ob die an den Hersteller angepasste Blockfolge die konkrete Ablehnung behebt, ist noch am Gerät zu prüfen. Details: evidence/v10-data-blocks.md.

## v0.11: Bestätigter Einzelseitenscan, Stapel/PDF/App

Der Nutzer bestätigt Version 0.10 als erfolgreich. Das reale Protokoll zeigt 2097152 + 77808 erfolgreich bestätigte Bildbytes, gespeichertes TIFF und vollständig verifizierten CONFIG-Restore. Das bisherige Firmware-/Transportproblem ist für diesen Einzelseitenablauf damit am Gerät bestätigt behoben.

Neue Funktion: --scan-stack führt in einer Ressourcensitzung wiederholte scan_one-Aufträge aus. Ein Papiersensortest vor jeder Seite beendet den Auftrag beim ersten leeren Einzug. Kein Warten auf Nachlegen. Neue Unterordner page-001 usw., maximal 100 Seiten; eine Überschreitung ist kein stiller Erfolg. Jeder Einzelauftrag behält den bestätigten Setup-/Daten-/STOP-/END-/Restore-Ablauf. Bei einem Fehler keine Fortsetzung mit weiteren Blättern. Vollständige TIFFs werden auch bei anschließendem Cleanup-Fehler gezählt und als Teil-PDF erhalten.

Nach Schließen der USB-Verbindung werden die Seiten mit ImageIO/CoreGraphics lokal in PDF eingebettet. Seitengröße aus TIFF-Pixelmaßen und DPI; explizite Seitenreihenfolge. Temporäre Datei plus exklusives Publish verhindert Überschreiben bestehender Backend-PDFs. Normaler Abschluss: scan.pdf. Fehler/Abbruch: scan-partial.pdf aus vollständigen Seiten. Kein PDF aus unvollständigen Rohdaten. --scan-one erhält ebenfalls automatische PDF-Ausgabe.

AppKit-App mit Speicherdialog, Start, Abbruch, Seitenstatus, PDF-/Ordner-/Protokoll-Tasten. Backend per NSTask, kein Shell-Befehl. Arbeitskopien unter Application Support/S2050 Scan/Scans. Gewählte Zieldatei erst nach vollständigem Erfolg ersetzen; fehlgeschlagenes Speichern erhält die Arbeitskopie. Beenden während des Scans wartet auf den signalisierten Cleanup. Bundle universal/ad-hoc signiert, kein Developer-ID-Zertifikat und keine Notarisierung.

Tests: 22 bestehende Scanszenarien weiterhin erfolgreich, 11 neue Stapel-Orchestrierungsfälle. PDF mit zehn Seiten auf Anzahl, Reihenfolge und physische Maße geprüft; Einzel-/Teil-PDF, vorhandenes Ziel und ungültige Eingabe geprüft. App-Selbsttest: gebündeltes Backend, neues Ziel, Ersetzen, Erhalt bei Kopierfehler. ASan/UBSan für C-Tests; Universal-Build und Signaturprüfung erfolgreich. App-Preview konnte ohne nutzbaren GUI-Zugang nicht ausgeführt werden. Echter Stapeltest und interaktiver App-Test stehen aus.


## v0.12: Auflösung und Farbmodus

Nutzer bestätigt v0.11 mit Stapel/PDF als funktionierend. Die App trägt nun die Überschrift Alaris macOS Scan Tool und bietet150/300/600 dpi sowie Graustufen/Farbe/Schwarzweiß. Standard bleibt150/Graustufen. Icon bleibt enthalten.

Primärquelle: devicemanager.so, eingebettete sage-create-Ressourcen windowadd und requestheader. WINDOW Byte3 imageformat: gray_8=1, rgb_24=2. Byte4/6 enthalten X/Y-DPI als Big-Endian16; alle drei Werte sind in der Enumtabelle enthalten. Windowtype bleibt color=1 für beide Formate. Im Bildheader sind imageformat bei8, DPI bei9/11, physische Maße bei21/25. RGB wird unkomprimiert als drei8-Bit-Kanäle in Pixelreihenfolge abgelegt. Die Farbkanalreihenfolge ist durch synthetische rote/blaue Testbilder geprüft; die tatsächliche Firmwareausgabe muss der nächste Farbscan bestätigen.

Schwarzweiß nutzt weiterhin gray_8 am Gerät. Lokal werden Pixel<128 schwarz und>=128 weiß, zeilenweise MSB-first,1Bit, TIFF BlackIsZero. Damit wird kein neuer unbekannter Geräte-Binarisierungsmodus benötigt. Bildabmessungen ergeben sich aus Headermaß*DPI/1200; nichtganzzahlige Maße und abweichende Nutzdatenlängen werden abgewiesen. TIFF schreibt die tatsächlichen DPI, PDF erhält dieselbe physische Größe.

dmmaxchunksize/SCAN BEGIN bleibt16MiB wie bisher; verhandelte Gerätegrenze begrenzt jeden einzelnen READ. Gesamtes Bildlimit separat128MiB (A4 RGB600 etwa100MiB); Datenfrist nach Header120s. Einstellungen und einzelne Seiten bleiben pro Auftrag konstant. CLI validiert Optionen vor Anlegen eines Ausgabeordners oder USB-Zugriff.

Tests:30 simulierte Scanprotokollfälle einschließlich aller neun Kombinationen bei echter A4-Payloadgröße;11 Stapelfälle;9 zusätzliche TIFF/PDF-Formatfälle einschließlich Kanalreihenfolge,1-Bit-Schwellwertgrenze und Zeilenpadding, DPI/Seitengröße sowie Fehlerablehnung. Bestehende zehnseitige PDF-Reihenfolgeprüfung und App-Selbsttest erfolgreich. ASan/UBSan und Universal-Build erfolgreich. Neue Hardwaremodi noch unbestätigt.


## v0.13: lokaler Papierhintergrund und Randkorrektur

Nutzer meldet v0.12 als offenbar funktionierend, jedoch grauen Papierhintergrund und schmalen schwarzen Rand. Lokale Rohscans des vorigen bestätigten Stapels zeigen neutralen Papiermodus234–236 und einen schwarzen linken Streifen bis34Pixel bei150dpi, dazu einen dünnen schrägen oberen Rand. Die separat beigefügte PDF war über den Downloadweg nicht erreichbar; kein behaupteter Pixelvergleich dieser Datei.

Keine neuen Gerätebefehle. cleanup.h arbeitet auf einer Kopie der bestätigten Rohdaten vor TIFF-Erstellung; image.raw bleibt unverändert. Weißpunkt: Histogramm heller neutraler Innenpixel, konzentrierte helle Population erforderlich, Peak200–250 minus8; nur Werte>180 werden angehoben, deutlich gesättigte RGB-Farben bleiben unangetastet. Ränder: zusammenhängende dunkle Läufe von der Außenkante, begrenzte Tiefe etwa6mm und nachfolgend stabiles helles Papier. Ausreichende räumliche Kontinuität und Kantenabdeckung erforderlich, um verteilte Textur nicht als Rand zu entfernen. Weißfüllung statt Beschnitt; Maße/DPI bleiben unverändert.

Beide Optionen in App/CLI standardmäßig aktiv, separat abschaltbar; während Scan gesperrt. Button heißt Scan starten. Fotos und randfüllende/hellgraue Gestaltung können die Annahmen verletzen: Optionen dafür abschalten. Native Bilddaten bleiben für Diagnose erhalten.

Validierung: fünf vorhandene rohe Graustufenscans erneut durch denselben TIFF/PDF-Code verarbeitet. Papiermedian255; in linkem40-Pixel-/oberem8-Pixel-Prüfstreifen keine verbleibenden Pixel<100. Dazu bisherige Protokoll-, Stapel-, PDF- und Optionssuiten, neue Regression für Graustufen/RGB bei150/300/600dpi, Text20/170, Farbflächen, abgeschaltete/individuelle Optionen, dicke schwarze Flächen, kleine Randmarkierungen, Gradienten, periodische Randtextur und kleinste Bilder. ASan/UBSan erfolgreich; Universal-Build und App-Selbsttest erfolgreich. Neue Oberfläche nicht interaktiv geprüft.
