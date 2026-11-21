#include "TimetableFetcher.h"
#include "Session.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    // ------------------------------------------------------------
    // Data structures
    // ------------------------------------------------------------

    struct ParsedCell
    {
        std::string text;
        int colSpan;
        int rowSpan;
        int width;          // the HTML width="" attribute in pixels (0 = unknown)

        ParsedCell() : colSpan(1), rowSpan(1), width(0) {}
    };

    struct ParsedRow   { std::vector<ParsedCell> cells; };
    struct ParsedTable { std::vector<ParsedRow> rows; };

    struct SubjectReference
    {
        std::string code;
        std::string subjectName;
        std::string faculty;
    };

    // Everything the reference table of one section tells us.
    struct SectionReference
    {
        std::unordered_map<std::string, SubjectReference> byCode;
        std::string iksFaculty;
        // "Career Excellence Classes (CEC) LT-6" rows: room -> faculty.
        std::vector<std::pair<std::string, std::string>> cecByRoom;
    };

    struct BuildStats
    {
        int cells = 0;
        int sessions = 0;
        int unmatched = 0;
        int lookupMissed = 0;
        int unknownRoom = 0;
        int badGeometry = 0;
        std::vector<std::string> notes;

        void note(const std::string& text)
        {
            if (notes.size() < 25)
                notes.push_back(text);
        }
    };

    // Ten teaching slots per day.
    const char* const SLOT_START[10] = {
        "08:00", "09:00", "09:55", "11:10", "12:05",
        "13:00", "13:55", "15:10", "16:05", "17:00"
    };

    const char* const SLOT_END[10] = {
        "08:55", "09:55", "10:50", "12:05", "13:00",
        "13:55", "14:50", "16:05", "17:00", "17:55"
    };

    // ------------------------------------------------------------
    // Small string helpers
    // ------------------------------------------------------------

    std::string readFile(const std::string& filePath)
    {
        std::ifstream file(filePath.c_str(), std::ios::binary);

        if (!file.is_open())
        {
            std::cerr << "Unable to open file: " << filePath << '\n';
            return "";
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::string trim(const std::string& text)
    {
        std::size_t first = text.find_first_not_of(" \t\r\n");

        if (first == std::string::npos)
            return "";

        std::size_t last = text.find_last_not_of(" \t\r\n");
        return text.substr(first, last - first + 1);
    }

    std::string toLower(std::string text)
    {
        for (char& c : text)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return text;
    }

    std::string toUpper(std::string text)
    {
        for (char& c : text)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return text;
    }

    bool containsNoCase(const std::string& text, const std::string& part)
    {
        return toLower(text).find(toLower(part)) != std::string::npos;
    }

    void replaceAll(std::string& text, const std::string& from, const std::string& to)
    {
        if (from.empty())
            return;

        std::size_t position = 0;

        while ((position = text.find(from, position)) != std::string::npos)
        {
            text.replace(position, from.length(), to);
            position += to.length();
        }
    }

    std::string decodeEntities(const std::string& text)
    {
        std::string result = text;

        const std::pair<const char*, const char*> entities[] = {
            {"&nbsp;", " "}, {"&#160;", " "}, {"&amp;", "&"}, {"&lt;", "<"},
            {"&gt;", ">"}, {"&quot;", "\""}, {"&apos;", "'"}, {"&#39;", "'"}
        };

        for (const auto& e : entities)
            replaceAll(result, e.first, e.second);

        return result;
    }

    // Decodes entities, repairs common encoding artefacts and collapses
    // all whitespace to single spaces.
    std::string cleanCellText(const std::string& text)
    {
        std::string result = decodeEntities(text);

        const std::pair<const char*, const char*> replacements[] = {
            {"\xC2\xA0", " "}, {"\xE2\x80\x99", "'"}, {"\xE2\x80\x93", "-"},
            {"\xE2\x80\x94", "-"}
        };

        for (const auto& r : replacements)
            replaceAll(result, r.first, r.second);

        std::string cleaned;
        bool previousSpace = false;

        for (char c : result)
        {
            if (std::isspace(static_cast<unsigned char>(c)))
            {
                if (!previousSpace && !cleaned.empty())
                    cleaned += ' ';
                previousSpace = true;
            }
            else
            {
                cleaned += c;
                previousSpace = false;
            }
        }

        return trim(cleaned);
    }

    // ------------------------------------------------------------
    // HTML tag helpers
    // ------------------------------------------------------------

    std::string getAttributeValue(const std::string& tag, const std::string& attribute)
    {
        std::regex expression(
            "\\b" + attribute + R"(\s*=\s*["']?([^"'\s>]+)["']?)",
            std::regex_constants::icase
        );

        std::smatch match;

        if (std::regex_search(tag, match, expression))
            return match[1].str();

        return "";
    }

    std::string getTagName(const std::string& tag)
    {
        std::size_t position = 0;

        while (position < tag.length() &&
               std::isspace(static_cast<unsigned char>(tag[position])))
            ++position;

        if (position < tag.length() && tag[position] == '/')
            ++position;

        std::size_t start = position;

        while (position < tag.length())
        {
            char ch = tag[position];

            if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '-' && ch != ':')
                break;

            ++position;
        }

        return toLower(tag.substr(start, position - start));
    }

    bool isClosingTag(const std::string& tag)
    {
        std::size_t position = 0;

        while (position < tag.length() &&
               std::isspace(static_cast<unsigned char>(tag[position])))
            ++position;

        return position < tag.length() && tag[position] == '/';
    }

    // ------------------------------------------------------------
    // HTML table parser (top-level tables only)
    // ------------------------------------------------------------

    std::vector<ParsedTable> parseTables(const std::string& html)
    {
        std::vector<ParsedTable> tables;

        int tableDepth = 0;
        bool insideRow = false;
        bool insideCell = false;

        ParsedRow currentRow;
        ParsedCell currentCell;

        std::size_t position = 0;

        while (position < html.length())
        {
            std::size_t tagStart = html.find('<', position);

            if (tagStart == std::string::npos)
                break;

            if (insideCell && tableDepth == 1 && tagStart > position)
                currentCell.text += html.substr(position, tagStart - position);

            if (html.compare(tagStart, 4, "<!--") == 0)
            {
                std::size_t commentEnd = html.find("-->", tagStart + 4);

                if (commentEnd == std::string::npos)
                    break;

                position = commentEnd + 3;
                continue;
            }

            std::size_t tagEnd = html.find('>', tagStart);

            if (tagEnd == std::string::npos)
                break;

            std::string tag = html.substr(tagStart + 1, tagEnd - tagStart - 1);
            std::string tagName = getTagName(tag);
            bool closing = isClosingTag(tag);

            if (tagName == "table")
            {
                if (!closing)
                {
                    ++tableDepth;

                    if (tableDepth == 1)
                        tables.push_back(ParsedTable());
                }
                else if (tableDepth > 0)
                {
                    --tableDepth;
                }
            }
            else if (tableDepth == 1)
            {
                if (tagName == "tr")
                {
                    if (!closing)
                    {
                        currentRow = ParsedRow();
                        insideRow = true;
                    }
                    else
                    {
                        if (insideCell)
                        {
                            currentRow.cells.push_back(currentCell);
                            currentCell = ParsedCell();
                            insideCell = false;
                        }

                        if (insideRow)
                        {
                            tables.back().rows.push_back(currentRow);
                            insideRow = false;
                        }
                    }
                }
                else if (tagName == "td" || tagName == "th")
                {
                    if (!closing)
                    {
                        if (insideCell)
                        {
                            currentRow.cells.push_back(currentCell);
                            currentCell = ParsedCell();
                        }

                        currentCell = ParsedCell();

                        std::string colSpan = getAttributeValue(tag, "colspan");
                        std::string rowSpan = getAttributeValue(tag, "rowspan");
                        std::string width   = getAttributeValue(tag, "width");

                        if (!colSpan.empty()) currentCell.colSpan = std::atoi(colSpan.c_str());
                        if (!rowSpan.empty()) currentCell.rowSpan = std::atoi(rowSpan.c_str());
                        if (!width.empty())   currentCell.width   = std::atoi(width.c_str());

                        if (currentCell.colSpan < 1) currentCell.colSpan = 1;
                        if (currentCell.rowSpan < 1) currentCell.rowSpan = 1;
                        if (currentCell.width < 0)   currentCell.width = 0;

                        insideCell = true;
                    }
                    else if (insideCell)
                    {
                        currentRow.cells.push_back(currentCell);
                        currentCell = ParsedCell();
                        insideCell = false;
                    }
                }
                else if (insideCell && (tagName == "br" || tagName == "p" || tagName == "div"))
                {
                    if (!closing || tagName != "br")
                        currentCell.text += " / ";
                }
            }

            position = tagEnd + 1;
        }

        for (ParsedTable& table : tables)
            for (ParsedRow& row : table.rows)
                for (ParsedCell& cell : row.cells)
                    cell.text = cleanCellText(cell.text);

        return tables;
    }

    // Plain text of the whole page (used only to read the section headings).
    std::string stripTags(const std::string& html)
    {
        std::string text;
        text.reserve(html.size() / 2);
        bool inTag = false;

        for (char c : html)
        {
            if (c == '<')      { inTag = true; text += ' '; }
            else if (c == '>') { inTag = false; }
            else if (!inTag)   { text += c; }
        }

        return cleanCellText(text);
    }

    // "Section-A" -> "A", "Section: AI-DS1" -> "AIDS1", "-Cyber" -> "CYBER".
    std::string canonicalSectionName(const std::string& raw)
    {
        std::string name;

        for (char c : toUpper(raw))
            if (std::isalnum(static_cast<unsigned char>(c)))
                name += c;

        if (name.compare(0, 7, "SECTION") == 0 && name.size() > 7)
            name = name.substr(7);

        return name;
    }

    std::vector<std::string> extractSectionNames(const std::string& html)
    {
        std::vector<std::string> names;
        const std::string text = stripTags(html);
        const std::regex headingPattern(
            R"(^\.?\s*-\s*\(?\s*(?:Section\s*[-:]?\s*)?([A-Za-z0-9][A-Za-z0-9-]*)\s*\)?\s*$)",
            std::regex_constants::icase);

        std::size_t position = 0;

        while ((position = text.find("ENGG", position)) != std::string::npos)
        {
            std::size_t end = text.find("ACADEMIC", position);

            if (end == std::string::npos)
                break;

            // Text between "ENGG" and "ACADEMIC", e.g. ". - (Section-A)".
            std::string between = text.substr(position + 4, end - position - 4);
            std::smatch match;

            if (std::regex_match(between, match, headingPattern))
                names.push_back(canonicalSectionName(match[1].str()));

            position = end;
        }

        return names;
    }

    // ------------------------------------------------------------
    // Cell text splitting and normalisation
    // ------------------------------------------------------------

    std::vector<std::string> splitCellSegments(const std::string& text)
    {
        std::vector<std::string> segments;
        std::string current;

        for (char c : text)
        {
            if (c == '/')
            {
                current = trim(current);
                if (!current.empty())
                    segments.push_back(current);
                current.clear();
            }
            else
            {
                current += c;
            }
        }

        current = trim(current);
        if (!current.empty())
            segments.push_back(current);

        return segments;
    }

    // Joins the non-empty "/" separated pieces with " / ".
    std::string cleanReferenceField(const std::string& text)
    {
        std::string cleaned;

        for (const std::string& segment : splitCellSegments(text))
        {
            if (!cleaned.empty())
                cleaned += " / ";
            cleaned += segment;
        }

        return cleaned;
    }

    // "tcs302", "TCS 302" and "TCS-302" all become "TCS-302";
    // "PDCC-A 301" becomes "PDCC-A-301".
    std::string normalizeCode(const std::string& code)
    {
        std::string result;

        for (char c : code)
            if (!std::isspace(static_cast<unsigned char>(c)))
                result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        static const std::regex pattern(R"(^([A-Z]+(?:-[A-Z]+)?)-?(\d{3})$)");
        std::smatch match;

        if (std::regex_match(result, match, pattern))
            return match[1].str() + "-" + match[2].str();

        return result;
    }

    // All subject codes in a text. Also expands "TCS-561/586" to
    // TCS-561 and TCS-586.
    std::vector<std::string> extractSubjectCodes(const std::string& text)
    {
        std::vector<std::string> codes;

        static const std::regex codePattern(
            R"(([A-Za-z]+(?:-[A-Za-z]+)?[- ]?\s*\d{3})(?:\s*/\s*(\d{3})\b)?)");

        for (std::sregex_iterator it(text.begin(), text.end(), codePattern), end;
             it != end; ++it)
        {
            std::string first = normalizeCode((*it)[1].str());
            std::vector<std::string> found = { first };

            if ((*it)[2].matched && first.size() > 3)
                found.push_back(first.substr(0, first.size() - 3) + (*it)[2].str());

            for (const std::string& code : found)
                if (!code.empty() &&
                    std::find(codes.begin(), codes.end(), code) == codes.end())
                    codes.push_back(code);
        }

        return codes;
    }

    // ------------------------------------------------------------
    // Rooms
    // ------------------------------------------------------------

    // Returns one canonical room name for a cell (or part of a cell).
    //   LT-5, CR-7, Lab-2, New Lab-1, Digital Lab-1, GEHU-Audi (5th Floor),
    //   B.Tech. Block IInd Floor Lab-1, Aryabhatt Ground Floor Lab, Online.
    // "Lab-7 & Lab-6" (one session in two labs) is returned as written.
    std::string extractRoom(const std::string& rawText)
    {
        std::string text = rawText;
        replaceAll(text, "/", " ");

        static const std::regex floorLab(
            R"(B\.?\s*Tech\.?\s*Block\s*IInd\s*Floor\s*Lab\s*-\s*(\d+))",
            std::regex_constants::icase);
        static const std::regex groundLab(
            R"((Aryabhatt|Param)\s*Ground\s*Floor)", std::regex_constants::icase);
        static const std::regex hpcLab(
            R"(CAI-HPC-NVIDIA)", std::regex_constants::icase);
        static const std::regex audi(
            R"(GEHU-Audi)", std::regex_constants::icase);
        static const std::regex lab(
            R"(\b(New|Digital)?\s*Lab\s*-\s*(\d+))", std::regex_constants::icase);
        static const std::regex lecture(
            R"(\b(CR|LT)\s*-?\s*(\d+))", std::regex_constants::icase);

        std::smatch match;

        if (std::regex_search(text, match, floorLab))
            return "B.Tech. Block IInd Floor Lab-" + match[1].str();

        if (std::regex_search(text, match, groundLab))
        {
            std::string name = toLower(match[1].str());
            name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
            return name + " Ground Floor Lab";
        }

        if (std::regex_search(text, match, hpcLab))
            return "CAI-HPC-NVIDIA Lab";

        if (std::regex_search(text, match, audi))
            return "GEHU-Audi (5th Floor)";

        std::vector<std::string> labs;

        for (std::sregex_iterator it(text.begin(), text.end(), lab), end; it != end; ++it)
        {
            std::string prefix;

            if ((*it)[1].matched)
            {
                prefix = toLower((*it)[1].str());
                prefix[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(prefix[0])));
                prefix += " ";
            }

            std::string name = prefix + "Lab-" + (*it)[2].str();

            if (std::find(labs.begin(), labs.end(), name) == labs.end())
                labs.push_back(name);
        }

        if (!labs.empty())
        {
            std::string joined = labs[0];
            for (std::size_t i = 1; i < labs.size(); ++i)
                joined += " & " + labs[i];
            return joined;
        }

        if (std::regex_search(text, match, lecture))
            return toUpper(match[1].str()) + "-" + match[2].str();

        if (containsNoCase(text, "online"))
            return "Online";

        return "Unknown";
    }

    // ------------------------------------------------------------
    // Reference table
    // ------------------------------------------------------------

    SectionReference buildReference(const ParsedTable& table)
    {
        SectionReference ref;

        for (std::size_t i = 1; i < table.rows.size(); ++i)
        {
            const ParsedRow& row = table.rows[i];

            // Two-cell rows: "Career Excellence Classes (CEC) LT-6 | Faculty".
            if (row.cells.size() == 2)
            {
                if (containsNoCase(row.cells[0].text, "Career Excellence"))
                {
                    ref.cecByRoom.push_back({
                        extractRoom(row.cells[0].text),
                        cleanReferenceField(row.cells[1].text)
                    });
                }

                continue;
            }

            if (row.cells.size() < 3)
                continue;

            const std::string name = cleanReferenceField(row.cells[1].text);
            const std::string faculty = cleanReferenceField(row.cells[2].text);

            if (row.cells[0].text.empty())
            {
                if (containsNoCase(name, "IKS"))
                    ref.iksFaculty = faculty;
                continue;
            }

            for (const std::string& code : extractSubjectCodes(row.cells[0].text))
            {
                SubjectReference reference;
                reference.code = code;
                reference.subjectName = name;
                reference.faculty = faculty;
                ref.byCode[code] = reference;
            }
        }

        return ref;
    }

    // ------------------------------------------------------------
    // Cell classification
    // ------------------------------------------------------------

    // Returns a subject name for LUNCH / LIB / SWAYAM-Training cells,
    // or "" for anything else. These are kept in the timetable;
    // ClashDetector decides they never occupy a room or a teacher.
    std::string freeActivityName(const std::string& text)
    {
        const std::string lower = toLower(text);

        if (lower.find("lunch") != std::string::npos)
            return "LUNCH";

        if (trim(lower) == "lib")
            return "LIB";

        if (lower.find("training") != std::string::npos ||
            lower.find("extra-curricular") != std::string::npos ||
            lower.find("extracurricular") != std::string::npos)
        {
            std::string name = text;
            replaceAll(name, " / ", "/");
            replaceAll(name, " /", "/");
            replaceAll(name, "/ ", "/");
            return name;
        }

        return "";
    }

    // A cell holding two parallel lab groups looks like
    //   "PCS-308 Lab (G1) B.Tech. Block IInd Floor Lab-1 / PCS-307 (G2) Lab-5"
    //   "G2-PCS-511(S) Lab New Lab-1 & / G1-PCS-503 Lab New Lab-2"
    // It is cut at every lab code ("PCS-nnn", optionally with "*" or "Gn-" in front).
    std::vector<std::string> splitIntoParts(const std::string& text)
    {
        static const std::regex labCode(
            R"(\*?\s*(?:G\d+\s*-\s*)?P[A-Z]{2}\s*-\s*\d{3})");

        std::vector<std::size_t> starts;

        for (std::sregex_iterator it(text.begin(), text.end(), labCode), end; it != end; ++it)
            starts.push_back(static_cast<std::size_t>(it->position()));

        std::vector<std::string> parts;

        if (starts.size() < 2)
        {
            parts.push_back(text);
            return parts;
        }

        for (std::size_t i = 0; i < starts.size(); ++i)
        {
            std::size_t end = (i + 1 < starts.size()) ? starts[i + 1] : text.size();
            parts.push_back(trim(text.substr(starts[i], end - starts[i])));
        }

        return parts;
    }

    // "(G1)" from "Lab (G1)" or "G1-PCS-503".
    std::string getGroup(const std::string& text)
    {
        static const std::regex groupPattern(R"(\(\s*G(\d+)\s*\)|\bG(\d+)\s*-\s*P)");
        std::smatch match;

        if (std::regex_search(text, match, groupPattern))
            return "(G" + (match[1].matched ? match[1].str() : match[2].str()) + ")";

        return "";
    }

    // "G1 - X & G2 - Y" -> the name that belongs to the given group.
    std::string selectGroupFaculty(const std::string& faculty, const std::string& group)
    {
        std::string key;

        if (group.find("G1") != std::string::npos)      key = "G1";
        else if (group.find("G2") != std::string::npos) key = "G2";
        else return faculty;

        const std::regex pattern(
            key == "G1" ? R"(G1\s*-\s*(.*?)(?:\s*&\s*G2\s*-|$))" : R"(G2\s*-\s*(.*?)$)",
            std::regex_constants::icase);

        std::smatch match;

        if (std::regex_search(faculty, match, pattern))
            return trim(match[1].str());

        return faculty;
    }

    // "QAR-Ms. A / Verbal-Ms. B / Soft Skills-Mr. C" -> the name for the
    // category written in the timetable cell. "" when the cell names none.
    std::string selectCareerSkillsFaculty(const std::string& cellText, const std::string& faculty)
    {
        std::string category;

        if (cellText.find("QAR") != std::string::npos)               category = "QAR";
        else if (containsNoCase(cellText, "Verbal"))                 category = "Verbal";
        else if (containsNoCase(cellText, "Soft Skills"))            category = "Soft Skills";
        else return "";

        const std::regex pattern(category + R"(\s*-\s*([^/]+))", std::regex_constants::icase);
        std::smatch match;

        if (std::regex_search(faculty, match, pattern))
            return trim(match[1].str());

        return "";
    }

    void addSession(std::vector<Session>& sessions, BuildStats& stats,
                    const std::string& subject, const std::string& faculty,
                    const std::string& room, const std::string& section,
                    const std::string& day, const std::string& start,
                    const std::string& end, int studentCount, WeekRule rule)
    {
        sessions.emplace_back(subject, faculty, room, section, day,
                              start, end, studentCount, rule);
        ++stats.sessions;
    }

    // One lecture / lab entry (a single part of a cell).
    void processPart(const std::string& part, const SectionReference& ref,
                     const std::string& section, const std::string& day,
                     const std::string& start, const std::string& end,
                     std::vector<Session>& sessions, BuildStats& stats)
    {
        // The "*" means: this class is held only when Saturday is off.
        const WeekRule rule = (part.find('*') != std::string::npos)
                                  ? WeekRule::SaturdayOffOnly
                                  : WeekRule::Always;

        std::string room = extractRoom(part);
        const std::string group = getGroup(part);

        std::string subject;
        std::string faculty;
        int students = 80;

        static const std::regex careerPattern(R"(Career\s*S?kills)", std::regex_constants::icase);
        static const std::regex iksPattern(R"(^\W*IKS\b)", std::regex_constants::icase);

        if (containsNoCase(part, "Career Excellence"))
        {
            subject = "Career Excellence Classes (CEC)";

            for (const auto& entry : ref.cecByRoom)
                if (entry.first == room)
                    faculty = entry.second;

            if (faculty.empty() && ref.cecByRoom.size() == 1)
                faculty = ref.cecByRoom[0].second;
        }
        else if (std::regex_search(part, careerPattern))
        {
            subject = "Career Skills / PDCC";

            for (const auto& entry : ref.byCode)
            {
                if (entry.first.compare(0, 3, "XCS") == 0)
                {
                    faculty = selectCareerSkillsFaculty(part, entry.second.faculty);
                    break;
                }
            }

            if (faculty.empty())
                stats.note(section + " " + day + ": Career Skills cell without QAR/Verbal/Soft Skills: " + part);
        }
        else if (std::regex_search(part, iksPattern))
        {
            subject = "IKS (Online)";
            faculty = ref.iksFaculty;
            room = "Online";
        }
        else
        {
            const std::vector<std::string> codes = extractSubjectCodes(part);

            for (const std::string& code : codes)
            {
                auto found = ref.byCode.find(code);

                if (found == ref.byCode.end())
                    continue;

                if (!subject.empty())
                    subject += " / ";

                subject += found->second.subjectName;

                if (faculty.empty())
                    faculty = found->second.faculty;
            }

            if (subject.empty())
            {
                ++stats.unmatched;
                stats.note(section + " " + day + " " + start + ": no subject match for \"" + part + "\"");
                subject = codes.empty() ? part : codes[0];
            }
        }

        if (!group.empty())
        {
            faculty = selectGroupFaculty(faculty, group);
            subject += " " + group;
            students = 40;               // G1 / G2 = half of a section
        }

        if (room == "Unknown" && !containsNoCase(part, "Swayam") &&
            !containsNoCase(part, "Audit"))
        {
            ++stats.unknownRoom;
            stats.note(section + " " + day + " " + start + ": no room found in \"" + part + "\"");
        }

        addSession(sessions, stats, subject, faculty, room, section, day,
                   start, end, students, rule);
    }

    void processCell(const std::string& rawText, const SectionReference& ref,
                     const std::string& section, const std::string& day,
                     const std::string& start, const std::string& end,
                     std::vector<Session>& sessions, BuildStats& stats)
    {
        // <p> and <br> tags leave stray " / " separators; keep only real text.
        const std::string text = cleanReferenceField(rawText);
        const std::string freeName = freeActivityName(text);

        if (!freeName.empty())
        {
            addSession(sessions, stats, freeName, "", "", section, day,
                       start, end, 0, WeekRule::Always);
            return;
        }

        if (toLower(trim(text)) == "elective")
        {
            addSession(sessions, stats, "Elective", "", "", section, day,
                       start, end, 0, WeekRule::Always);
            return;
        }

        for (const std::string& part : splitIntoParts(text))
            processPart(part, ref, section, day, start, end, sessions, stats);
    }

    // ------------------------------------------------------------
    // Days and slots
    // ------------------------------------------------------------

    std::string normalizeDay(const std::string& day)
    {
        std::string value = day;
        replaceAll(value, "/", " ");          // cells contain " / " from <p>/<br>
        value = toLower(trim(value));

        const char* const full[6] = {"monday", "tuesday", "wednesday",
                                     "thursday", "friday", "saturday"};

        for (const char* name : full)
        {
            std::string n = name;

            if (value == n || value == n.substr(0, 3))
            {
                n[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(n[0])));
                return n;
            }
        }

        return "";
    }

    // Where each of the ten time slots sits horizontally (in HTML pixels).
    //
    // The Word-generated tables do not have a clean column grid: a header
    // cell may span two columns while a body cell below it spans one, so
    // counting columns places some classes in the wrong slot (it put Section
    // G's Monday TMA-316 on top of a lab). The cell widths are reliable, so a
    // cell belongs to every slot whose centre lies inside the cell.
    struct SlotGrid
    {
        std::vector<std::pair<int, int>> ranges;
        std::vector<int> columnToSlot;      // fallback when widths are missing
        bool valid = false;
    };

    SlotGrid buildSlotGrid(const ParsedRow& header)
    {
        SlotGrid grid;

        if (header.cells.empty())
            return grid;

        int x = header.cells[0].width;
        bool widthsKnown = x > 0;
        int slot = 0;

        for (std::size_t c = 1; c < header.cells.size(); ++c)
        {
            const ParsedCell& cell = header.cells[c];

            if (cell.width <= 0)
                widthsKnown = false;

            grid.ranges.push_back({x, x + cell.width});
            x += cell.width;

            for (int k = 0; k < cell.colSpan; ++k)
                grid.columnToSlot.push_back(slot);

            ++slot;
        }

        grid.valid = widthsKnown && grid.ranges.size() == 10;
        return grid;
    }

    void generateSessionsFromTables(const ParsedTable& timetable,
                                    const ParsedTable& referenceTable,
                                    std::vector<Session>& sessions,
                                    const std::string& section,
                                    BuildStats& stats)
    {
        if (timetable.rows.empty())
            return;

        const SectionReference ref = buildReference(referenceTable);
        const SlotGrid grid = buildSlotGrid(timetable.rows[0]);

        for (std::size_t r = 1; r < timetable.rows.size(); ++r)
        {
            const ParsedRow& row = timetable.rows[r];

            if (row.cells.empty())
                continue;

            const std::string day = normalizeDay(row.cells[0].text);

            if (day.empty())
                continue;                        // blank or footer row

            int x = row.cells[0].width;
            int physicalColumn = 0;

            for (std::size_t c = 1; c < row.cells.size(); ++c)
            {
                const ParsedCell& cell = row.cells[c];

                int firstSlot = -1;
                int lastSlot = -1;

                if (grid.valid && cell.width > 0)
                {
                    for (int s = 0; s < 10; ++s)
                    {
                        const int centre = (grid.ranges[s].first + grid.ranges[s].second) / 2;

                        if (centre >= x && centre < x + cell.width)
                        {
                            if (firstSlot < 0) firstSlot = s;
                            lastSlot = s;
                        }
                    }
                }
                else
                {
                    const int a = physicalColumn;
                    const int b = physicalColumn + cell.colSpan - 1;

                    if (b < static_cast<int>(grid.columnToSlot.size()))
                    {
                        firstSlot = grid.columnToSlot[a];
                        lastSlot = grid.columnToSlot[b];
                    }
                }

                x += cell.width;
                physicalColumn += cell.colSpan;

                if (cell.text.empty())
                    continue;

                ++stats.cells;

                if (firstSlot < 0 || lastSlot < firstSlot)
                {
                    ++stats.badGeometry;
                    stats.note(section + " " + day + ": cell outside the time grid: " + cell.text);
                    continue;
                }

                processCell(cell.text, ref, section, day,
                            SLOT_START[firstSlot], SLOT_END[lastSlot],
                            sessions, stats);
            }
        }
    }

    void printStats(const BuildStats& stats)
    {
        std::cout << "Timetable cells read: " << stats.cells
                  << " | sessions created: " << stats.sessions << '\n';

        if (stats.unmatched || stats.unknownRoom || stats.badGeometry)
        {
            std::cout << "Parser warnings: " << stats.unmatched << " unmatched subject(s), "
                      << stats.unknownRoom << " cell(s) without a room, "
                      << stats.badGeometry << " cell(s) outside the grid\n";

            for (const std::string& note : stats.notes)
                std::cout << "  - " << note << '\n';
        }
    }

    std::string quotePath(const std::string& path)
    {
        std::string result = "\"";

        for (char c : path)
        {
            if (c == '"')
                result += '\\';
            result += c;
        }

        return result + "\"";
    }
}


// ============================================================
// PUBLIC FUNCTIONS
// ============================================================

namespace TimetableFetcher
{
    bool downloadTimetable(const std::string& url, const std::string& outputPath)
    {
        std::string command =
            "curl -L --fail --silent --show-error " + quotePath(url) +
            " -o " + quotePath(outputPath);

        std::cout << "Downloading timetable...\n";

        if (std::system(command.c_str()) != 0)
        {
            std::cerr << "Timetable download failed.\n";
            return false;
        }

        std::ifstream file(outputPath.c_str(), std::ios::binary);

        if (!file.is_open())
        {
            std::cerr << "Downloaded file could not be opened.\n";
            return false;
        }

        std::cout << "Timetable downloaded successfully: " << outputPath << '\n';
        return true;
    }

    bool generateFirstSectionSessions(const std::string& htmlPath,
                                      std::vector<Session>& sessions)
    {
        const std::string html = readFile(htmlPath);

        if (html.empty())
            return false;

        const std::vector<ParsedTable> tables = parseTables(html);

        if (tables.size() < 2)
            return false;

        std::vector<std::string> names = extractSectionNames(html);
        BuildStats stats;

        sessions.clear();
        generateSessionsFromTables(tables[0], tables[1], sessions,
                                   names.empty() ? "A" : names[0], stats);
        printStats(stats);

        return !sessions.empty();
    }

    bool generateAllSections(const std::string& htmlPath,
                             const std::vector<std::string>& sectionNames,
                             std::vector<Session>& sessions)
    {
        const std::string html = readFile(htmlPath);

        if (html.empty())
            return false;

        const std::vector<ParsedTable> tables = parseTables(html);

        if (tables.empty() || tables.size() % 2 != 0)
        {
            std::cout << "Unexpected number of tables: " << tables.size() << '\n';
            return false;
        }

        std::vector<std::string> names = extractSectionNames(html);

        if (names.size() * 2 != tables.size())
        {
            std::cout << "Could not read " << tables.size() / 2
                      << " section headings (found " << names.size()
                      << "); using the supplied section names.\n";
            names = sectionNames;
        }

        if (names.size() * 2 != tables.size())
        {
            std::cout << "Expected " << names.size() * 2 << " tables, found "
                      << tables.size() << '\n';
            return false;
        }

        sessions.clear();
        BuildStats stats;

        for (std::size_t i = 0; i < names.size(); ++i)
            generateSessionsFromTables(tables[i * 2], tables[i * 2 + 1],
                                       sessions, names[i], stats);

        std::cout << names.size() << " sections processed.\n";
        printStats(stats);

        return !sessions.empty();
    }
}
