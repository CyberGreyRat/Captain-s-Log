# Captain's Log 📖🚀

ist ein leistungsstarkes Werkzeug für lückenlose **Traceability**, strukturiertes **Requirements Engineering** und integriertes **Risk Management** in Software- und Systementwicklungsprojekten.

Egal ob über das schnelle **CLI** oder die intuitive **GUI** gesteuert – Captain's Log speichert alle Projektinformationen, Risikobewertungen und Evidenzen transparent in leicht lesbaren und versionierbaren **YAML-Dateien**. 

Das System ist hochgradig skalierbar: Es kann lokal auf dem Rechner einzelner Entwickler genutzt oder zentral auf einem **Unternehmensserver** betrieben werden. Dadurch wird es zum idealen Hub, um tagesaktuelle, manipulationssichere Berichte und Audit-Trails nahtlos mit **Auditoren**, dem Qualitätsmanagement oder dem gesamten Team zu teilen. 

<img width="1150" height="134" alt="image" src="https://github.com/user-attachments/assets/6e750fae-382e-47e2-98c9-d102d8c338f4" />

*Captain's Log im Einsatz: Volle Kontrolle über Anforderungen und Risiken.*

---

## ✨ Features

* **End-to-End Traceability:** Verknüpfe Code-Änderungen, Architekturentscheidungen und Tests lückenlos miteinander.
* **Requirements & Risk Management:** Erfasse, verwalte und verfolge Anforderungen (Requirements) und bewerte Projektrisiken direkt aus dem Terminal.
* **Audit-Ready:** Alle Einträge werden manipulationssicher mit Zeitstempeln und Evidenzen (Evidence-Logs) versehen.
* **Automatisierte Word-Berichte:** Generiere mit einem einzigen Befehl professionelle, formatierte Word-Dokumente für Meilenstein-Reviews, Projektübergaben oder externe Audits.
* **Lokales Backend:** Arbeitet komplett offline mit einem integrierten C++ API-Server, SQLite- und YAML-Storage für maximale Datenkontrolle.

---

## 📸 Screenshots

### Das CLI-Interface
<img width="693" height="171" alt="image" src="https://github.com/user-attachments/assets/4e5c67e2-b588-42cd-a049-7449077d1735" />

*Beschreibung: Effiziente Steuerung von Requirements und Logs über das Terminal.*

### Der generierte Word-Bericht
<img width="935" height="1315" alt="image" src="https://github.com/user-attachments/assets/b6c91c6e-e38d-4d7d-8c59-8327c09f6a29" />



*Beschreibung: Exportierte Traceability-Matrix und Risikobewertung im finalen Word-Bericht.*

---

## 🚀 Lokale Testnutzung (Quickstart)

Du möchtest Captain's Log auf deinem eigenen Windows-Rechner ausprobieren? So startest du in wenigen Sekunden:

### 1. Projekt herunterladen

Klone das Repository in ein beliebiges Verzeichnis auf deinem Rechner:

`git clone https://github.com/CyberGreyRat/Captain-s-Log.git`

`cd captainslog`



2. Direkt starten (Portable Modus)
 
Für einen ersten Testlauf musst du nichts tiefgreifend installieren. Starte das Tool einfach über die beiliegende Batch-Datei im Hauptverzeichnis:


`.\cap.bat`

3. Systemweit installieren (Optional)

Damit du den Befehl cap von jedem beliebigen Projektordner aus aufrufen kannst, füge den Pfad zu deinen Windows-Umgebungsvariablen (PATH) hinzu.

Führe dazu das beiliegende PowerShell-Skript als Administrator aus:

`.\build_and_install.ps1`


Wichtig: Schließe danach dein Terminal und öffne es neu, damit die Änderungen wirksam werden.

💻 Nutzung & Befehle
Sobald das Tool eingerichtet ist, erfolgt die gesamte Steuerung über den Befehl cap. Hier sind die wichtigsten Basis-Befehle:

Bash
### Zeigt die Hilfe und alle verfügbaren Befehle an
`cap --help `

### Initialisiert ein neues Projekt-Log im aktuellen Verzeichnis
`cap init`

### Erfasst ein neues Requirement (Anforderung)
`cap req add "Das System muss Offline-Verfügbarkeit bieten"`

### Erfasst ein neues Risiko
`cap risk add "Datenverlust bei Stromausfall" --severity high`

# Starte die GUI
`cap gui`

<img width="1695" height="1133" alt="Screenshot 2026-07-23 110731" src="https://github.com/user-attachments/assets/887eb018-e661-48d8-b81e-de36dca98c2e" />


### Generiert den finalen Word-Bericht inkl. Traceability-Matrix
`cap publish`
(Hinweis: Schau dir die --help an, um alle Möglichkeiten für Verknüpfungen und Evidenz-Erfassung zu sehen!)


🛠️ Tech Stack & Architektur
Das Projekt kombiniert schnelle Low-Level-Verarbeitung mit flexiblen Skript-Automatisierungen für die Berichterstellung:

Core/Backend: C++ (API Server & CMake Build-System)

Datenspeicherung: SQLite & YAML

Report-Generierung: PowerShell (Word COM-Objekte / OpenXML)

🤝 Mitwirken (Contributing)
Pull Requests sind willkommen! Für größere Architektur-Änderungen öffne bitte zuerst ein Issue, um das Vorgehen zu besprechen.

📄 Lizenz
Dieses Projekt ist lizenziert unter der MIT License - siehe die entsprechende Datei für Details.
