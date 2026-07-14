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
        else if (
            name == "test" &&
            argc >= 3 &&
            std::string_view{argv[2]} == "import")
        {
            result.type = CommandType::test_import;
            first_argument = 3;
        }
        else if (name == "verify")
        {
            result.type = CommandType::verify;
        }

        for (int index = first_argument; index < argc; ++index)
        {
            result.arguments.emplace_back(argv[index]);
        }

        return result;
    }

    std::string_view CommandParser::help_text() noexcept
    {
        return "Captain's Log CLI 0.5.0 - YAML-First Traceability\n"
               "\n"
               "Verwendung:\n"
               "  cap <Befehl> [Argumente]\n"
               "\n"
               "Projektbefehle:\n"
               "  cap init [Ordner]                         (-i)\n"
               "      Initialisiert ein neues CapCom-Projekt und erzeugt die\n"
               "      Projektordner sowie .capcom_config.json.\n"
               "\n"
               "  cap create <Typ> [Titel] [-p <Parent>]    (-c)\n"
               "      Erstellt eine neue YAML-Anforderung im Ordner reqs/.\n"
               "      Ohne Titel fragt CapCom interaktiv danach. Mit -p wird\n"
               "      das neue Item direkt mit einem vorhandenen Parent verlinkt.\n"
               "      Beispiel: cap -c srs \"Sensor auslesen\" -p SYS-001\n"
               "\n"
               "  cap link <Child-UID> <Parent-UID>         (-l)\n"
               "      Verknuepft zwei vorhandene Items und aktualisiert die\n"
               "      parents- und children-Felder in beiden YAML-Dateien.\n"
               "      Beispiel: cap -l SRS-001 SYS-001\n"
               "\n"
               "  cap unlink <Child-UID> <Parent-UID>       (-u)\n"
               "      Entfernt eine vorhandene Verknuepfung aus beiden YAMLs.\n"
               "\n"
               "Auswertung und Traceability:\n"
               "  cap tree                                  (-t)\n"
               "      Zeigt den Anforderungsbaum mit Parent-/Child-Beziehungen.\n"
               "\n"
               "  cap scan                                  (-s)\n"
               "      Durchsucht C/C++-Quelldateien nach @cap-start/@cap-end\n"
               "      und schreibt Datei, Zeilen und Git-Hash in die YAMLs.\n"
               "\n"
               "  cap status                                (-st)\n"
               "      Zeigt alle Anforderungen mit Status, Implementierungs-\n"
               "      und Testabdeckung in einer tabellarischen Uebersicht.\n"
               "\n"
               "  cap validate\n"
               "      Prueft Pflichtfelder, Links und klassenspezifische Angaben.\n"
               "      Bei Fehlern wird ein Exit-Code ungleich 0 zurueckgegeben.\n"
               "\n"
               "  cap verify\n"
               "      Prueft die kryptografischen Signaturen und die Audit-Kette.\n"
               "\n"
               "Testintegration:\n"
               "  cap test import <Datei>\n"
               "      Importiert Testergebnisse aus JUnit-XML oder CapCom-JSON\n"
               "      und aktualisiert das zugehoerige YAML-Item.\n"
               "      Beispiel: cap test import reports/test_results.xml\n"
               "\n"
               "Allgemein:\n"
               "  cap help                                  (-h, --help)\n"
               "      Zeigt diese Hilfe an.\n"
               "\n"
               "  cap version                               (-v, --version)\n"
               "      Zeigt die installierte CapCom-Version an.\n"
               "\n"
               "Unterstuetzte Typen:\n"
               "  USR, SYS, SRS, SEC, ARCH, LLD, RISK, TEST, UT, IT, ST, AT,\n"
               "  BUG, CR und SOUP\n"
               "\n"
               "Hinweis: YAML-Dateien in reqs/ sind die alleinige Source of Truth.\n";
    }

} // namespace capcom::cli
