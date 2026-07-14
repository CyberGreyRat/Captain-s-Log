#include "capcom/yaml/yaml_store.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace capcom::yaml {
namespace {

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string unquote(std::string value) {
    value = trim(std::move(value));
    if (value.size() > 1 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

std::vector<std::string> read_lines(const std::filesystem::path& file) {
    std::ifstream input(file, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot read " + file.string());
    }
    std::vector<std::string> result;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        result.push_back(line);
    }
    return result;
}

void save(const std::filesystem::path& file,const std::vector<std::string>& lines) {
    auto temporary = file;
    temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("Cannot write " + temporary.string());
        }
        for (const auto& line : lines) {
            output << line << '\n';
        }
    }
    std::error_code error;
    std::filesystem::remove(file, error);
    error.clear();
    std::filesystem::rename(temporary, file, error);
    if (error) {
        throw std::runtime_error("Cannot replace " + file.string() + ": " + error.message());
    }
}

std::vector<std::string> inline_list(const std::string& value) {
    const auto first = value.find('[');
    const auto last = value.rfind(']');
    std::vector<std::string> result;
    if (first == std::string::npos || last <= first) {
        return result;
    }
    std::stringstream input{value.substr(first + 1, last - first - 1)};
    std::string entry;
    while (std::getline(input, entry, ',')) {
        entry = normalize_uid(unquote(entry));
        if (!entry.empty()) {
            result.push_back(entry);
        }
    }
    return result;
}

std::vector<std::string> attribute_list(
    const Item& item,
    const std::string& key) {
    const auto iterator = item.attributes.find(key);
    return iterator == item.attributes.end()
        ? std::vector<std::string>{}
        : inline_list(iterator->second);
}

std::string attribute(const Item& item, const std::string& key) {
    const auto iterator = item.attributes.find(key);
    return iterator == item.attributes.end() ? std::string{} : iterator->second;
}

bool test_type(const std::string& type) {
    return type == "TEST" || type == "UT" || type == "IT" ||
           type == "ST" || type == "AT";
}

bool safety_class(const std::string& value) {
    return value == "A" || value == "B" || value == "C";
}

std::size_t section_end(
    const std::vector<std::string>& values,
    const std::size_t start) {
    auto index = start + 1;
    while (index < values.size() &&
           (values[index].empty() || values[index][0] == ' ' ||
            values[index][0] == '\t')) {
        ++index;
    }
    return index;
}

void replace_section(
    const std::filesystem::path& file,
    const std::string& name,
    const std::vector<std::string>& block) {
    auto values = read_lines(file);
    const auto iterator = std::find_if(
        values.begin(), values.end(),
        [&](const auto& line) { return line.rfind(name + ":", 0) == 0; });

    if (iterator != values.end()) {
        const auto position = static_cast<std::size_t>(iterator - values.begin());
        values.erase(
            values.begin() + static_cast<std::ptrdiff_t>(position),
            values.begin() + static_cast<std::ptrdiff_t>(section_end(values, position)));
        values.insert(
            values.begin() + static_cast<std::ptrdiff_t>(position),
            block.begin(), block.end());
    } else {
        const auto position = std::find_if(
            values.begin(), values.end(),
            [](const auto& line) { return line.rfind("integration_template:", 0) == 0; });
        values.insert(position, block.begin(), block.end());
    }
    save(file, values);
}

std::string escape(std::string value) {
    std::string output;
    for (const char character : value) {
        if (character == '"' || character == '\\') {
            output.push_back('\\');
        }
        if (character == '\n') {
            output += "\\n";
        } else if (character != '\r') {
            output.push_back(character);
        }
    }
    return output;
}

Item parse(const std::filesystem::path& file) {
    Item item;
    item.file = file;
    const auto values = read_lines(file);

    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto& raw = values[index];
        const auto colon = raw.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        const auto key = trim(raw.substr(0, colon));
        const auto value = unquote(raw.substr(colon + 1));
        const bool top_level = !raw.empty() && raw.front() != ' ' && raw.front() != '\t';

        if (top_level && key == "uid") item.uid = normalize_uid(value);
        else if (top_level && key == "type") item.type = normalize_uid(value);
        else if (top_level && key == "status") item.status = value;
        else if (top_level && key == "title") item.title = value;
        else if (top_level && key == "text") item.text = value;
        else if (top_level && key == "rationale") item.rationale = value;
        else if (top_level && key == "version") item.version = value;
        else if (top_level && key == "git_hash") item.git_hash = value;
        else if (top_level && key == "parents") item.parents = inline_list(raw);
        else if (top_level && key == "children") item.children = inline_list(raw);
        else if (top_level && key == "attributes") {
            for (auto child = index + 1;
                 child < values.size() && values[child].rfind("  ", 0) == 0;
                 ++child) {
                const auto child_colon = values[child].find(':');
                if (child_colon == std::string::npos) continue;
                item.attributes[trim(values[child].substr(0, child_colon))] =
                    unquote(values[child].substr(child_colon + 1));
            }
        } else if (top_level && key == "implementation") {
            Implementation implementation;
            for (auto child = index + 1;
                 child < values.size() && values[child].rfind("  ", 0) == 0;
                 ++child) {
                const auto child_colon = values[child].find(':');
                if (child_colon == std::string::npos) continue;
                auto child_key = trim(values[child].substr(0, child_colon));
                if (child_key.rfind("- ", 0) == 0) {
                    child_key = trim(child_key.substr(2));
                }
                const auto child_value = unquote(values[child].substr(child_colon + 1));
                if (child_key == "file") {
                    if (!implementation.file.empty()) {
                        item.implementations.push_back(implementation);
                        implementation = {};
                    }
                    implementation.file = child_value;
                } else if (child_key == "start_line") {
                    implementation.start_line = std::stoi(child_value);
                } else if (child_key == "end_line") {
                    implementation.end_line = std::stoi(child_value);
                } else if (child_key == "git_hash") {
                    implementation.git_hash = child_value;
                }
            }
            if (!implementation.file.empty()) {
                item.implementations.push_back(implementation);
            }
        } else if (top_level && key == "test_result") {
            for (auto child = index + 1;
                 child < values.size() && values[child].rfind("  ", 0) == 0;
                 ++child) {
                const auto child_colon = values[child].find(':');
                if (child_colon == std::string::npos) continue;
                const auto child_key = trim(values[child].substr(0, child_colon));
                const auto child_value = unquote(values[child].substr(child_colon + 1));
                if (child_key == "status") item.test.status = child_value;
                else if (child_key == "source") item.test.source = child_value;
                else if (child_key == "timestamp") item.test.timestamp = child_value;
                else if (child_key == "message") item.test.message = child_value;
            }
        }
    }
    return item;
}

void require_attribute(
    std::vector<std::string>& errors,
    const Item& item,
    const std::string& key) {
    if (attribute(item, key).empty()) {
        errors.push_back(item.uid + ": missing attributes." + key);
    }
}

} // namespace

std::string normalize_uid(std::string value) {
    value = trim(std::move(value));
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](const unsigned char character) {
            return static_cast<char>(std::toupper(character));
        });
    std::replace(value.begin(), value.end(), '_', '-');
    if (value.find('-') == std::string::npos) {
        const auto number = std::find_if(
            value.begin(), value.end(),
            [](const unsigned char character) { return std::isdigit(character) != 0; });
        if (number != value.end()) value.insert(number, '-');
    }
    return value;
}

YamlStore::YamlStore(std::filesystem::path project)
    : project_{std::move(project)} {}

std::map<std::string, Item> YamlStore::load_all() const {
    std::map<std::string, Item> result;
    const auto directory = project_ / "reqs";
    if (!std::filesystem::is_directory(directory)) {
        throw std::runtime_error("reqs directory missing");
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        const auto extension = entry.path().extension().string();
        if (!entry.is_regular_file() ||
            (extension != ".yaml" && extension != ".yml") ||
            entry.path().filename() == "captainslog.yml") {
            continue;
        }
        auto item = parse(entry.path());
        if (item.uid.empty()) {
            throw std::runtime_error("Missing uid in " + entry.path().string());
        }
        if (!result.emplace(item.uid, std::move(item)).second) {
            throw std::runtime_error("Duplicate UID: " + item.uid);
        }
    }
    return result;
}

void YamlStore::replace_implementations(
    const std::map<std::string, std::vector<Implementation>>& values) const {
    const auto items = load_all();
    for (const auto& [uid, item] : items) {
        std::vector<std::string> block{"implementation:"};
        const auto iterator = values.find(uid);
        if (iterator == values.end() || iterator->second.empty()) {
            block = {"implementation: []"};
        } else {
            for (const auto& implementation : iterator->second) {
                block.push_back("  - file: \"" + escape(implementation.file) + "\"");
                block.push_back("    start_line: " + std::to_string(implementation.start_line));
                block.push_back("    end_line: " + std::to_string(implementation.end_line));
                block.push_back("    git_hash: \"" + escape(implementation.git_hash) + "\"");
            }
        }
        replace_section(item.file, "implementation", block);
    }
}

void YamlStore::update_test(const std::string& raw_uid,const TestResult& result) const {
    const auto items = load_all();
    const auto uid = normalize_uid(raw_uid);
    const auto iterator = items.find(uid);
    if (iterator == items.end()) {
        throw std::runtime_error("Test UID not found: " + uid);
    }
    replace_section(iterator->second.file, "test_result", {
        "test_result:",
        "  status: " + result.status,
        "  source: \"" + escape(result.source) + "\"",
        "  timestamp: \"" + escape(result.timestamp) + "\"",
        "  message: \"" + escape(result.message) + "\""
    });
}

std::vector<std::string> YamlStore::validate(
    const std::map<std::string, Item>& items) const {
    std::vector<std::string> errors;

    for (const auto& [uid, item] : items) {
        const auto require = [&](const std::string& value, const char* field) {
            if (value.empty()) errors.push_back(uid + ": missing " + field);
        };
        require(item.uid, "uid");
        require(item.type, "type");
        require(item.status, "status");
        require(item.title, "title");
        require(item.text, "text");
        require(item.rationale, "rationale");
        require(item.version, "version");

        for (const auto& parent : item.parents) {
            const auto found = items.find(parent);
            if (found == items.end()) {
                errors.push_back(uid + ": missing parent " + parent);
            } else if (std::find(
                           found->second.children.begin(),
                           found->second.children.end(), uid) ==
                       found->second.children.end()) {
                errors.push_back(uid + ": asymmetric parent link " + parent);
            }
        }
        for (const auto& child : item.children) {
            const auto found = items.find(child);
            if (found == items.end()) {
                errors.push_back(uid + ": missing child " + child);
            } else if (std::find(
                           found->second.parents.begin(),
                           found->second.parents.end(), uid) ==
                       found->second.parents.end()) {
                errors.push_back(uid + ": asymmetric child link " + child);
            }
        }

        if (item.type == "CLASS") {
            require_attribute(errors, item, "software_system");
            require_attribute(errors, item, "software_safety_class");
            require_attribute(errors, item, "classification_responsible");
            require_attribute(errors, item, "classification_rationale");
            require_attribute(errors, item, "risk_management_document");
            if (!safety_class(attribute(item, "software_safety_class"))) {
                errors.push_back(uid + ": attributes.software_safety_class must be A, B or C");
            }
            if (attribute_list(item, "risk_management_record_ids").empty()) {
                errors.push_back(uid + ": missing attributes.risk_management_record_ids");
            }
        }

        if (item.type == "SRS" || item.type == "SEC") {
            require_attribute(errors, item, "classification_ref");
            require_attribute(errors, item, "software_item_safety_class");
            require_attribute(errors, item, "classification_responsible");
            require_attribute(errors, item, "classification_rationale");

            const auto item_class = attribute(item, "software_item_safety_class");
            if (!safety_class(item_class)) {
                errors.push_back(uid + ": attributes.software_item_safety_class must be A, B or C");
            }

            const auto class_uid = normalize_uid(attribute(item, "classification_ref"));
            const auto class_item = items.find(class_uid);
            if (class_uid.empty() || class_item == items.end() ||
                class_item->second.type != "CLASS") {
                errors.push_back(uid + ": classification_ref must reference a CLASS item");
            }

            const auto risks = attribute_list(item, "risk_references");
            if (risks.empty()) {
                errors.push_back(uid + ": missing attributes.risk_references");
            }
            for (const auto& risk_uid : risks) {
                const auto risk = items.find(risk_uid);
                if (risk == items.end() || risk->second.type != "RISK") {
                    errors.push_back(uid + ": risk reference is not a RISK item: " + risk_uid);
                } else {
                    const auto controls = attribute_list(risk->second, "control_requirements");
                    if (std::find(controls.begin(), controls.end(), uid) == controls.end()) {
                        errors.push_back(uid + ": risk " + risk_uid + " does not reference this control requirement");
                    }
                }
            }

            bool has_test = false;
            for (const auto& child_uid : item.children) {
                const auto child = items.find(child_uid);
                if (child == items.end() || !test_type(child->second.type)) continue;
                has_test = true;
                const auto verifies = attribute_list(child->second, "verifies");
                if (std::find(verifies.begin(), verifies.end(), uid) == verifies.end()) {
                    errors.push_back(child_uid + ": attributes.verifies must contain " + uid);
                }
            }
            if (!has_test) {
                errors.push_back(uid + ": no linked verification test child");
            }
        }

        if (item.type == "RISK") {
            require_attribute(errors, item, "hazard");
            require_attribute(errors, item, "hazardous_situation");
            require_attribute(errors, item, "harm");
            require_attribute(errors, item, "risk_responsible");
            require_attribute(errors, item, "initial_severity");
            require_attribute(errors, item, "initial_probability");
            const auto controls = attribute_list(item, "control_requirements");
            const auto tests = attribute_list(item, "verification_tests");
            if (controls.empty()) errors.push_back(uid + ": missing attributes.control_requirements");
            if (tests.empty()) errors.push_back(uid + ": missing attributes.verification_tests");
            for (const auto& control_uid : controls) {
                if (!items.contains(control_uid)) {
                    errors.push_back(uid + ": unknown control requirement " + control_uid);
                }
            }
            for (const auto& test_uid : tests) {
                const auto test = items.find(test_uid);
                if (test == items.end() || !test_type(test->second.type)) {
                    errors.push_back(uid + ": invalid verification test " + test_uid);
                }
            }
        }

        if (test_type(item.type)) {
            require_attribute(errors, item, "test_method");
            require_attribute(errors, item, "expected_result");
            require_attribute(errors, item, "test_responsible");
            const auto verifies = attribute_list(item, "verifies");
            if (verifies.empty()) {
                errors.push_back(uid + ": missing attributes.verifies");
            }
            for (const auto& requirement_uid : verifies) {
                const auto requirement = items.find(requirement_uid);
                if (requirement == items.end()) {
                    errors.push_back(uid + ": verifies unknown item " + requirement_uid);
                } else if (std::find(
                               requirement->second.children.begin(),
                               requirement->second.children.end(), uid) ==
                           requirement->second.children.end()) {
                    errors.push_back(uid + ": verified requirement does not link this test as child: " + requirement_uid);
                }
            }
        }
    }
    return errors;
}

} // namespace capcom::yaml
