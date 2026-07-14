#include "capcom/cli/command_parser.hpp"

namespace capcom::cli
{

    Command CommandParser::parse(
        const int argc,
        const char *const argv[]) const
    {

        if (argc < 2)
        {
            return {CommandType::help, {}};
        }

        const std::string_view name{argv[1]};
        Command result{};
        int first_argument = 2;

        if (name == "help" || name == "--help" || name == "-h")
        {
            result.type = CommandType::help;
        }
        else if (name == "version" || name == "--version" || name == "-v")
        {
            result.type = CommandType::version;
        }
        else if (name == "init" || name == "-i")
        {
            result.type = CommandType::init;
        }
        else if (name == "create" || name == "-c")
        {
            result.type = CommandType::create;
        }
        else if (name == "link" || name == "-l")
        {
            result.type = CommandType::link;
        }
        else if (name == "unlink" || name == "-u")
        {
            result.type = CommandType::unlink;
        }
        else if (name == "tree" || name == "-t")
        {
            result.type = CommandType::tree;
        }
        else if (name == "scan" || name == "-s")
        {
            result.type = CommandType::scan;
        }
        else if (name == "status" || name == "-st")
        {
            result.type = CommandType::status;
        }
        else if (name == "validate")
        {
            result.type = CommandType::validate;
        }
        else if (name == "verify")
        {
            result.type = CommandType::verify;
        }
        else if (name == "sign")
        {
            result.type = CommandType::sign;
        }
        else if (name == "diff")
        {
            result.type = CommandType::diff;
        }
        else if (name == "publish")
        {
            result.type = CommandType::publish;
        }
        else if (name == "review")
        {
            result.type = CommandType::review;
        }
        else if (name == "review-check")
        {
            result.type = CommandType::review_check;
        }
        else if (name == "approve")
        {
            result.type = CommandType::approve;
        }
        else if (name == "reject")
        {
            result.type = CommandType::reject;
        }
        else if (
            name == "test" &&
            argc >= 3 &&
            std::string_view{argv[2]} == "import")
        {
            result.type = CommandType::test_import;
            first_argument = 3;
        }

        for (int index = first_argument; index < argc; ++index)
        {
            result.arguments.emplace_back(argv[index]);
        }

        return result;
    }

    std::string_view CommandParser::help_text() noexcept
    {
        return "Captain's Log CLI 0.7.0\n"
               "YAML-First Requirements & Traceability\n"
               "\n"
               "VERWENDUNG\n"
               "  cap <Befehl> [Argumente]\n"
               "\n"
               "BEFEHLSUEBERSICHT\n"
               "  cap init [Ordner]                         Projekt initialisieren\n"
               "  cap create <Typ> [Titel] [-p <Parent>]    Anforderung erstellen\n"
               "  cap link <Child> <Parent>                 Anforderungen verknuepfen\n"
               "  cap unlink <Child> <Parent>               Verknuepfung entfernen\n"
               "  cap tree                                  Anforderungsbaum anzeigen\n"
               "  cap status                                Projektstatus anzeigen\n"
               "  cap scan                                  Implementierungen suchen\n"
               "  cap test import <Datei>                   Testergebnisse importieren\n"
               "  cap validate                              Pflichtfelder und Links pruefen\n"
               "  cap diff <UID>                            YAML-Aenderungen anzeigen\n"
               "  cap sign <UID> -m <Grund>                 YAML-Aenderungen signieren\n"
               "  cap verify                                Audit-Signaturen pruefen\n"
               "  cap help                                  Hilfe anzeigen\n"
               "  cap version                               Version anzeigen\n"
               "  cap publish                               HTML-Report erzeugen\n"
               "  cap review-check <UID>                   Review-Bereitschaft prüfen\n"
               "  cap review <UID> -m <Grund>              Zur Prüfung einreichen\n"
               "  cap approve <UID> -m <Ergebnis>          Anforderung freigeben\n"
               "  cap reject <UID> -m <Grund>              Anforderung ablehnen\n"
               "\n"
               "KURZBEFEHLE\n"
               "  -i init   -c create   -l link   -u unlink   -t tree\n"
               "  -s scan   -st status  -h help   -v version\n"
               "\n"
               "TYPISCHER WORKFLOW\n"
               "  1. cap init\n"
               "  2. cap -c usr \"Nutzeranforderung\"\n"
               "  3. cap -c srs \"Softwareanforderung\"\n"
               "  4. cap -l SRS-001 USR-001\n"
               "  5. YAML bearbeiten und Pflichtfelder ausfuellen\n"
               "  6. cap diff SRS-001\n"
               "  7. cap sign SRS-001 -m \"Anforderung vervollstaendigt\"\n"
               "  8. Code mit @cap-start und @cap-end markieren\n"
               "  9. cap scan\n"
               " 10. cap test import reports/test_results.xml\n"
               " 11. cap validate\n"
               " 12. cap verify\n"
               " 13. cap publish\n"
               "\n"
               "============================================================\n"
               "AUSFUEHRLICHE BEFEHLSBESCHREIBUNG\n"
               "============================================================\n"
               "\n"
               "PROJEKTVERWALTUNG\n"
               "\n"
               "  cap init [Ordner]                         Kurzform: cap -i\n"
               "      Initialisiert ein Projekt und erzeugt Projektordner sowie\n"
               "      die lokale Konfigurationsdatei.\n"
               "\n"
               "  cap create <Typ> [Titel] [-p <Parent>]    Kurzform: cap -c\n"
               "      Erstellt eine YAML-Anforderung in reqs/. Ohne Titel folgt\n"
               "      eine interaktive Abfrage. -p verlinkt direkt einen Parent.\n"
               "      Beispiel: cap -c srs \"Sensor auslesen\" -p SYS-001\n"
               "\n"
               "TRACEABILITY\n"
               "\n"
               "  cap link <Child> <Parent>                 Kurzform: cap -l\n"
               "      Verknuepft zwei Items und signiert beide YAML-Dateien.\n"
               "      Beispiel: cap -l SRS-001 SYS-001\n"
               "\n"
               "  cap unlink <Child> <Parent>               Kurzform: cap -u\n"
               "      Entfernt einen Link und signiert beide YAML-Dateien.\n"
               "\n"
               "  cap tree                                  Kurzform: cap -t\n"
               "      Zeigt den Anforderungsbaum in der Konsole.\n"
               "\n"
               "STATUS UND AUSWERTUNG\n"
               "\n"
               "  cap status                                Kurzform: cap -st\n"
               "      Zeigt Status, Implementierung und Testabdeckung.\n"
               "\n"
               "  cap scan                                  Kurzform: cap -s\n"
               "      Sucht @cap-start/@cap-end in C/C++-Dateien und schreibt\n"
               "      Datei, Zeilen und Git-Hash in die zugehoerigen YAMLs.\n"
               "\n"
               "  cap validate\n"
               "      Prueft Pflichtfelder, Links, Symmetrie und Klassifizierung.\n"
               "\n"
               "MANUELLE YAML-BEARBEITUNG\n"
               "\n"
               "  cap diff <UID>\n"
               "      Zeigt unsignierte YAML-Aenderungen und, falls vorhanden,\n"
               "      den Git-Diff. Beispiel: cap diff SRS-001\n"
               "\n"
               "  cap sign <UID> -m <Begruendung>\n"
               "      Signiert eine bewusst vorgenommene manuelle YAML-Aenderung.\n"
               "      Beispiel: cap sign SRS-001 -m \"Rationale ergaenzt\"\n"
               "\n"
               "TESTINTEGRATION\n"
               "\n"
               "  cap test import <Datei>\n"
               "      Importiert JUnit-XML oder Captain's-Log-JSON.\n"
               "      Beispiel: cap test import reports/test_results.xml\n"
               "\n"
               "SICHERHEIT UND AUDIT\n"
               "\n"
               "  cap verify\n"
               "      Prueft RSA-PSS-Signaturen, Audit-Kette und Content-Hashes.\n"
               "      Normale Editor-Aenderungen werden anschliessend mit cap sign\n"
               "      autorisiert. Eine beschaedigte History erzeugt SECURITY ALERT.\n"
               "\n"
               "  cap publish\n"
               "      Prüft zuerst alle Audit-Signaturen und erzeugt danach\n"
               "      eine interaktive report.html im Projektordner.\n"
               "      Der Report enthält Requirements Tree, Details, Code,\n"
               "      Testergebnisse und verifizierte Audit-Historien.\n"
               "\n"
               "ALLGEMEIN\n"
               "\n"
               "  cap help                                  Kurzformen: -h, --help\n"
               "  cap version                               Kurzformen: -v, --version\n"
               "\n"
               "UNTERSTUETZTE ITEM-TYPEN\n"
               "  USR, SYS, SRS, SEC, ARCH, LLD, RISK, TEST, UT, IT, ST, AT,\n"
               "  BUG, CR und SOUP\n"
               "\n"
               "WICHTIG\n"
               "  YAML-Dateien in reqs/ sind die alleinige Source of Truth.\n"
               "  Fachliche Inhalte duerfen direkt editiert werden. Danach cap sign.\n"
               "  History, Public Keys und Signaturen nie manuell bearbeiten.\n";
    }

} // namespace capcom::cli
