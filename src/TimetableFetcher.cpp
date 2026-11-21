
#include "TimetableFetcher.h"
#include "Session.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <regex>
#include <cstdlib>
#include <cctype>
#include <unordered_map>
#include <algorithm>

namespace
{
    struct ParsedCell
    {
        std::string text;
        int colSpan;
        int rowSpan;

        ParsedCell() : colSpan(1), rowSpan(1) {}
    };

    struct ParsedRow
    {
        std::vector<ParsedCell> cells;
    };

    struct ParsedTable
    {
        std::vector<ParsedRow> rows;
    };

    struct SubjectReference
    {
        std::string code;
        std::string subjectName;
        std::string faculty;
    };

    struct ExtractedSession
    {
        Session session;
        bool conditionalSaturday;
    };

    std::string normalizeDay(const std::string& day);
    std::vector<std::string> splitCellSegments(const std::string& text);
    bool isNonAcademic(const std::string& text);
    std::string getSlotTime(int slot, bool isEnd);
    std::vector<std::vector<std::string>> splitParallelLabs(
        const std::vector<std::string>& segments
    );

    // --------------------------------------------------------
    // FILE READING
    // --------------------------------------------------------

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


    // --------------------------------------------------------
    // HTML TEXT CLEANING
    // --------------------------------------------------------

    std::string decodeEntities(const std::string& text)
    {
        std::string result = text;

        const std::vector<std::pair<std::string, std::string>> entities =
        {
            {"&nbsp;", " "},
            {"&#160;", " "},
            {"&amp;", "&"},
            {"&lt;", "<"},
            {"&gt;", ">"},
            {"&quot;", "\""},
            {"&apos;", "'"},
            {"&#39;", "'"}
        };

        for (std::size_t i = 0; i < entities.size(); ++i)
        {
            std::size_t position = 0;

            while ((position = result.find(
                entities[i].first, position)) != std::string::npos)
            {
                result.replace(
                    position,
                    entities[i].first.length(),
                    entities[i].second
                );

                position += entities[i].second.length();
            }
        }

        return result;
    }


    std::string trim(const std::string& text)
    {
        std::size_t first = text.find_first_not_of(" \t\r\n");

        if (first == std::string::npos)
            return "";

        std::size_t last = text.find_last_not_of(" \t\r\n");

        return text.substr(first, last - first + 1);
    }


    std::string cleanCellText(const std::string& text)
    {
        std::string result = decodeEntities(text);

        const std::vector<std::pair<std::string, std::string>> replacements =
        {
            {"\xC2\xA0", " "},
            {"\xE2\x80\x99", "'"},
            {"\xE2\x80\x93", "-"},
            {"\xE2\x80\x94", "-"},
            {"┬á", " "},
            {"ΓÇÖ", "'"},
            {"ΓÇô", "-"},
            {"ΓÇö", "-"}
        };

        for (std::size_t i = 0; i < replacements.size(); ++i)
        {
            std::size_t position = 0;

            while ((position = result.find(
                replacements[i].first, position)) != std::string::npos)
            {
                result.replace(
                    position,
                    replacements[i].first.length(),
                    replacements[i].second
                );

                position += replacements[i].second.length();
            }
        }

        std::string cleaned;
        bool previousSpace = false;

        for (std::size_t i = 0; i < result.length(); ++i)
        {
            unsigned char ch =
                static_cast<unsigned char>(result[i]);

            if (std::isspace(ch))
            {
                if (!previousSpace && !cleaned.empty())
                    cleaned += ' ';

                previousSpace = true;
            }
            else
            {
                cleaned += static_cast<char>(ch);
                previousSpace = false;
            }
        }

        return trim(cleaned);
    }


    // --------------------------------------------------------
    // HTML TAG HELPERS
    // --------------------------------------------------------

    std::string getAttributeValue(
        const std::string& tag,
        const std::string& attribute)
    {
        std::regex expression(
            attribute + R"(\s*=\s*["']?([^"'\s>]+)["']?)",
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
        {
            ++position;
        }

        if (position < tag.length() && tag[position] == '/')
            ++position;

        std::size_t start = position;

        while (position < tag.length())
        {
            char ch = tag[position];

            if (!std::isalnum(static_cast<unsigned char>(ch)) &&
                ch != '-' && ch != ':')
            {
                break;
            }

            ++position;
        }

        std::string name = tag.substr(start, position - start);

        for (std::size_t i = 0; i < name.length(); ++i)
        {
            name[i] = static_cast<char>(
                std::tolower(static_cast<unsigned char>(name[i]))
            );
        }

        return name;
    }


    bool isClosingTag(const std::string& tag)
    {
        std::size_t position = 0;

        while (position < tag.length() &&
               std::isspace(static_cast<unsigned char>(tag[position])))
        {
            ++position;
        }

        return position < tag.length() && tag[position] == '/';
    }


    std::string cleanReferenceField(const std::string& text)
    {
        std::string result = trim(text);

        // Remove leading and trailing slashes.
        while (!result.empty() && result.front() == '/')
        {
            result.erase(0, 1);
            result = trim(result);
        }

        while (!result.empty() && result.back() == '/')
        {
            result.pop_back();
            result = trim(result);
        }

        // Remove empty segments and join valid segments.
        std::vector<std::string> segments =
            splitCellSegments(result);

        std::string cleaned;

        for (std::size_t i = 0; i < segments.size(); ++i)
        {
            if (!cleaned.empty())
                cleaned += " / ";

            cleaned += segments[i];
        }

        return cleaned;
    }


    // Normalize a subject code for consistent lookup.
    std::string normalizeCode(const std::string& code)
    {
        std::string result = trim(code);

        result.erase(
            std::remove_if(
                result.begin(),
                result.end(),
                [](unsigned char ch)
                {
                    return std::isspace(ch);
                }
            ),
            result.end()
        );

        for (std::size_t i = 0; i < result.length(); ++i)
        {
            result[i] = static_cast<char>(
                std::toupper(static_cast<unsigned char>(result[i]))
            );
        }

        return result;
    }


    // Extract all subject codes from a reference-table code cell.
    // Handles codes such as TCS-302, toc-301 and PDCC-A 301.
    std::vector<std::string> extractSubjectCodes(
        const std::string& text)
    {
        std::vector<std::string> codes;

        const std::regex codePattern(
            R"(([A-Za-z]+(?:-[A-Za-z]+)?[- ]\s*\d{3}))"
        );

        std::sregex_iterator it(
            text.begin(), text.end(), codePattern
        );

        std::sregex_iterator end;

        for (; it != end; ++it)
        {
            std::string code = normalizeCode((*it)[1].str());

            if (!code.empty() &&
                std::find(codes.begin(), codes.end(), code) == codes.end())
            {
                codes.push_back(code);
            }
        }

        return codes;
    }


    // Build the subject lookup from a section's reference table.
    std::unordered_map<std::string, SubjectReference>
    buildSubjectLookup(const ParsedTable& subjectTable)
    {
        std::unordered_map<std::string, SubjectReference> lookup;

        // Row 0 contains the column headings.
        for (std::size_t i = 1; i < subjectTable.rows.size(); ++i)
        {
            const ParsedRow& row = subjectTable.rows[i];

            if (row.cells.size() < 3)
                continue;

            std::string rawCode = row.cells[0].text;
            std::string subjectName =
                cleanReferenceField(row.cells[1].text);
            std::string faculty =
                cleanReferenceField(row.cells[2].text);

            std::vector<std::string> codes =
                extractSubjectCodes(rawCode);

            if (codes.empty())
                continue;

            for (std::size_t j = 0; j < codes.size(); ++j)
            {
                SubjectReference reference;

                reference.code = codes[j];
                reference.subjectName = subjectName;
                reference.faculty = faculty;

                lookup[codes[j]] = reference;
            }
        }

        return lookup;
    }



    std::string extractRoom(const std::vector<std::string>& segments)
    {
        for (const std::string& segment : segments)
        {
            std::string room = trim(segment);
            if (room.find("B.Tech.") != std::string::npos)
                return room;

            if (room.find("GEHU-Audi") != std::string::npos)
                return room;

            std::regex roomPattern(
                R"((CR|LT)\s*-\s*\d+|Lab\s*-\s*\d+|Digital Lab\s*-\s*\d+)",
                std::regex_constants::icase
            );

            std::smatch match;

            if (std::regex_search(room, match, roomPattern))
            {
                room = match.str();

                // Remove unnecessary spaces around hyphens.
                room = std::regex_replace(
                    room,
                    std::regex(R"(\s*-\s*)"),
                    "-"
                );

                // Normalize room prefixes.
                std::transform(
                    room.begin(),
                    room.end(),
                    room.begin(),
                    [](unsigned char c)
                    {
                        return static_cast<char>(std::toupper(c));
                    }
                );

                if (room.find("DIGITAL") != std::string::npos)
                {
                    return "Digital " + room.substr(8);
                }

                return room;
            }

            if (room.find("B.Tech.") != std::string::npos)
                return room;

            if (room.find("GEHU-Audi") != std::string::npos)
                return room;

            if (room.find("Online") != std::string::npos)
                return "Online";
        }

        return "Unknown";
    }


    void inspectCellRooms(const ParsedTable& timetable)
    {
        std::cout << "\n========== CSE A ROOM EXTRACTION ==========\n";

        for (std::size_t r = 1; r < timetable.rows.size(); ++r)
        {
            const ParsedRow& row = timetable.rows[r];

            if (row.cells.empty())
                continue;

            std::string day = normalizeDay(row.cells[0].text);

            int slotIndex = 0;

            const std::vector<std::pair<std::string, std::string>> timeSlots =
            {
                {"08:00", "08:55"},
                {"09:00", "09:55"},
                {"09:55", "10:50"},
                {"11:10", "12:05"},
                {"12:05", "13:00"},
                {"13:00", "13:55"},
                {"13:55", "14:50"},
                {"15:10", "16:05"},
                {"16:05", "17:00"},
                {"17:00", "17:55"}
            };

            for (std::size_t c = 1; c < row.cells.size(); ++c)
            {
                const ParsedCell& cell = row.cells[c];

                int span = cell.colSpan;

                if (span < 1)
                    span = 1;

                if (slotIndex >= static_cast<int>(timeSlots.size()))
                    break;

                int endSlot = slotIndex + span;

                if (endSlot > static_cast<int>(timeSlots.size()))
                    endSlot = static_cast<int>(timeSlots.size());

                if (!cell.text.empty() && !isNonAcademic(cell.text))
                {
                    std::vector<std::string> segments =
                        splitCellSegments(cell.text);

                    std::string room = extractRoom(segments);

                    std::cout << "\nDay: " << day
                            << "\nTime: "
                            << timeSlots[slotIndex].first
                            << " - "
                            << timeSlots[endSlot - 1].second
                            << "\nContent: " << cell.text
                            << "\nRoom: " << room
                            << "\n------------------------------\n";
                }

                slotIndex += span;
            }
        }
    }


    std::string getGroup(const std::string& text)
    {
        std::regex groupPattern(R"(\(G\d+\))");
        std::smatch match;

        if (std::regex_search(text, match, groupPattern))
            return match.str();

        return "";
    }

    std::string getCleanSubjectCode(const std::string& text)
    {
        std::vector<std::string> codes = extractSubjectCodes(text);

        if (!codes.empty())
            return normalizeCode(codes[0]);

        return "";
    }

    void addSession(
        std::vector<Session>& sessions,
        const std::string& subject,
        const std::string& faculty,
        const std::string& room,
        const std::string& section,
        const std::string& day,
        const std::string& startTime,
        const std::string& endTime,
        int studentCount,
        const std::string& group = "",
        bool conditional = false)
    {
        std::string finalSubject = subject;

        if (!group.empty())
            finalSubject += " " + group;

        // Preserve the Saturday-off marker.
        if (conditional)
            finalSubject += " *";

        sessions.emplace_back(
            finalSubject,
            faculty,
            room,
            section,
            day,
            startTime,
            endTime,
            studentCount
        );
    }

    std::string selectGroupFaculty(const std::string& faculty,const std::string& group)
    {
        std::string groupKey;

        if (group.find("G1") != std::string::npos)
            groupKey = "G1";
        else if (group.find("G2") != std::string::npos)
            groupKey = "G2";
        else
            return faculty;

        std::string pattern;

        if (groupKey == "G1")
            pattern = R"(G1\s*-\s*(.*?)(?:\s*&\s*G2\s*-|$))";
        else
            pattern = R"(G2\s*-\s*(.*?)(?:$))";

        std::regex facultyRegex(
            pattern,
            std::regex_constants::icase
        );

        std::smatch match;

        if (std::regex_search(faculty, match, facultyRegex))
            return trim(match[1].str());

        return faculty;
    }

    std::string selectCareerSkillsFaculty(const std::string& cellText,const std::string& faculty)
    {
        std::string category;

        if (cellText.find("QAR") != std::string::npos)
            category = "QAR";
        else if (cellText.find("Verbal") != std::string::npos)
            category = "Verbal";
        else if (cellText.find("Soft Skills") != std::string::npos)
            category = "Soft Skills";
        else
            return faculty;

        std::regex pattern(
            category + R"(\s*-\s*([^/]+))",
            std::regex_constants::icase
        );

        std::smatch match;

        if (std::regex_search(faculty, match, pattern))
            return trim(match[1].str());

        return faculty;
    }
 
    void generateSessionsFromTables(
        const ParsedTable& timetable,
        const ParsedTable& subjectTable,
        std::vector<Session>& sessions,
        const std::string& section)
    {
        if (timetable.rows.empty())
            return;

        std::unordered_map<std::string, SubjectReference> lookup =
            buildSubjectLookup(subjectTable);

        // Map physical HTML columns to the 10 actual timetable slots.
        std::vector<int> columnToSlot;
        int currentSlot = 0;

        for (std::size_t c = 1; c < timetable.rows[0].cells.size(); c++)
        {
            const ParsedCell& headerCell = timetable.rows[0].cells[c];

            for (int k = 0; k < headerCell.colSpan; k++)
            {
                columnToSlot.push_back(currentSlot);
            }

            currentSlot++;
        }

        // Process each day.
        for (std::size_t r = 1; r < timetable.rows.size(); r++)
        {
            const ParsedRow& row = timetable.rows[r];

            if (row.cells.empty())
                continue;

            std::string day = normalizeDay(row.cells[0].text);
            int physicalColumn = 0;

            // Process each timetable cell only once.
            for (std::size_t c = 1; c < row.cells.size(); c++)
            {
                const ParsedCell& cell = row.cells[c];

                // Skip duplicate final cell caused by the merged last timetable column
                if (c > 1 &&
                    cell.text == row.cells[c - 1].text &&
                    row.cells[c - 1].colSpan > 1)
                {
                    continue;
                }

                int startPhysical = physicalColumn;
                int endPhysical = physicalColumn + cell.colSpan - 1;

                physicalColumn += cell.colSpan;

                if (startPhysical < 0 ||
                    endPhysical >= static_cast<int>(columnToSlot.size()))
                {
                    continue;
                }

                int startSlot = columnToSlot[startPhysical];
                int endSlot = columnToSlot[endPhysical];

                if (cell.text.empty() || isNonAcademic(cell.text))
                    continue;

                // Validate the mapped timetable slots.
                if (startSlot < 0 || endSlot >= 10 || startSlot > endSlot)
                {
                    std::cout << "WARNING: Invalid time slot skipped\n";
                    std::cout << "Section: " << section << '\n';
                    std::cout << "Day: " << day << '\n';
                    std::cout << "Content: " << cell.text << '\n';
                    std::cout << "Slot range: " << startSlot
                            << " - " << endSlot << "\n\n";

                    continue;
                }

                std::string startTime = getSlotTime(startSlot, false);
                std::string endTime = getSlotTime(endSlot, true);

                std::vector<std::string> segments =
                    splitCellSegments(cell.text);

                // Handle parallel laboratory sessions.
                std::vector<std::vector<std::string>> labs =
                    splitParallelLabs(segments);

                if (!labs.empty())
                {
                    for (const auto& lab : labs)
                    {
                        std::string code = getCleanSubjectCode(lab[0]);

                        if (code.empty() || lookup.find(code) == lookup.end())
                            continue;

                        const SubjectReference& ref = lookup.at(code);

                        std::string group = getGroup(lab[0]);

                        std::string faculty = selectGroupFaculty(ref.faculty, group);

                        bool conditional =
                            !lab[0].empty() && lab[0][0] == '*';

                        int count = 40;

                        addSession(
                            sessions,
                            ref.subjectName,
                            faculty,
                            lab[1],
                            section,
                            day,
                            startTime,
                            endTime,
                            count,
                            group,
                            conditional
                        );
                    }

                    continue;
                }

                // Handle ordinary timetable entries.
                std::string code = getCleanSubjectCode(cell.text);
                std::string subject;
                std::string faculty = "Not specified";
                std::string room = extractRoom(segments);

                bool conditional =
                    cell.text.find('*') != std::string::npos;

                int studentCount = 80;

                if (!code.empty() && lookup.find(code) != lookup.end())
                {
                    const SubjectReference& ref = lookup.at(code);
                    subject = ref.subjectName;
                    faculty = ref.faculty;
                }
                else if (cell.text.find("Career Skills") != std::string::npos)
                {
                    subject = "Career Skills / PDCC";

                    auto it = lookup.find("XCS-301");

                    if (it != lookup.end())
                        faculty = selectCareerSkillsFaculty(cell.text,it->second.faculty);
                }
                else if (cell.text.find("Elective") != std::string::npos)
                {
                    subject = "Elective";
                    faculty = "Not specified";
                    studentCount = 0;
                }
                else
                {
                    continue;
                }

                // Explicit G1/G2 lab sessions have 40 students.
                if (cell.text.find("PCS-308") != std::string::npos ||
                    cell.text.find("PCS-307") != std::string::npos)
                {
                    studentCount = 40;
                }

                std::string group = getGroup(cell.text);
                faculty = selectGroupFaculty(faculty, group);

                addSession(
                    sessions,
                    subject,
                    faculty,
                    room,
                    section,
                    day,
                    startTime,
                    endTime,
                    studentCount,
                    group,
                    conditional
                );
            }
        }
    }


    // --------------------------------------------------------
    // HTML TABLE PARSER
    // --------------------------------------------------------

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

            // Collect text inside a table cell.
            if (insideCell && tableDepth == 1 && tagStart > position)
            {
                currentCell.text += html.substr(
                    position,
                    tagStart - position
                );
            }

            // Skip HTML comments.
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

            std::string tag = html.substr(
                tagStart + 1,
                tagEnd - tagStart - 1
            );

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
                else
                {
                    if (tableDepth > 0)
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

                        std::string colSpan =
                            getAttributeValue(tag, "colspan");

                        std::string rowSpan =
                            getAttributeValue(tag, "rowspan");

                        if (!colSpan.empty())
                            currentCell.colSpan = std::atoi(colSpan.c_str());

                        if (!rowSpan.empty())
                            currentCell.rowSpan = std::atoi(rowSpan.c_str());

                        if (currentCell.colSpan < 1)
                            currentCell.colSpan = 1;

                        if (currentCell.rowSpan < 1)
                            currentCell.rowSpan = 1;

                        insideCell = true;
                    }
                    else
                    {
                        if (insideCell)
                        {
                            currentRow.cells.push_back(currentCell);
                            currentCell = ParsedCell();
                            insideCell = false;
                        }
                    }
                }
                else if (insideCell && !closing &&
                         (tagName == "br" ||
                          tagName == "p" ||
                          tagName == "div"))
                {
                    currentCell.text += " / ";
                }
                else if (insideCell && closing &&
                         (tagName == "p" || tagName == "div"))
                {
                    currentCell.text += " / ";
                }
            }

            position = tagEnd + 1;
        }

        // Clean the text in all parsed cells.
        for (std::size_t t = 0; t < tables.size(); ++t)
        {
            for (std::size_t r = 0; r < tables[t].rows.size(); ++r)
            {
                for (std::size_t c = 0;
                     c < tables[t].rows[r].cells.size(); ++c)
                {
                    tables[t].rows[r].cells[c].text =
                        cleanCellText(
                            tables[t].rows[r].cells[c].text
                        );
                }
            }
        }

        return tables;
    }


    // --------------------------------------------------------
    // DAY AND CELL PROCESSING
    // --------------------------------------------------------

    std::string normalizeDay(const std::string& day)
    {
        std::string value = day;

        // Remove slashes and surrounding spaces.
        for (std::size_t i = 0; i < value.length(); ++i)
        {
            if (value[i] == '/')
                value[i] = ' ';
        }

        value = trim(value);

        if (value == "Mon" || value == "Monday")
            return "Monday";
        if (value == "Tue" || value == "Tuesday")
            return "Tuesday";
        if (value == "Wed" || value == "Wednesday")
            return "Wednesday";
        if (value == "Thu" || value == "Thursday")
            return "Thursday";
        if (value == "Fri" || value == "Friday")
            return "Friday";
        if (value == "Sat" || value == "Saturday")
            return "Saturday";

        return value;
    }


    std::vector<std::string> splitCellSegments(const std::string& text)
    {
        std::vector<std::string> segments;

        std::string current;

        for (std::size_t i = 0; i < text.length(); ++i)
        {
            if (text[i] == '/')
            {
                current = trim(current);

                if (!current.empty())
                    segments.push_back(current);

                current.clear();
            }
            else
            {
                current += text[i];
            }
        }

        current = trim(current);

        if (!current.empty())
            segments.push_back(current);

        return segments;
    }


    std::vector<std::vector<std::string>> splitParallelLabs(
        const std::vector<std::string>& segments)
    {
        std::vector<std::vector<std::string>> labs;

        // A parallel lab cell follows this structure:
        // Subject 1 / Room 1 / Subject 2 with Room 2

        if (segments.size() < 3)
            return labs;

        std::string firstSubject = trim(segments[0]);
        std::string firstRoom = trim(segments[1]);
        std::string secondPart = trim(segments[2]);

        // Check whether this is actually a parallel lab cell.
        if (firstSubject.find("PCS-") == std::string::npos ||
            firstSubject.find("Lab") == std::string::npos ||
            secondPart.find("PCS-") == std::string::npos)
        {
            return labs;
        }

        // Extract the second lab's room from its subject segment.
        std::regex secondLabPattern(R"(Lab\s*-\s*\d+)",
                                    std::regex_constants::icase);

        std::smatch match;

        if (!std::regex_search(secondPart, match, secondLabPattern))
            return labs;

        std::string secondRoom = match.str();

        secondRoom = std::regex_replace(
            secondRoom,
            std::regex(R"(\s*-\s*)"),
            "-"
        );

        std::string secondSubject =
            trim(secondPart.substr(0, match.position()));

        labs.push_back({firstSubject, firstRoom});
        labs.push_back({secondSubject, secondRoom});

        return labs;
    }


    std::string getSlotTime(int slot, bool isEnd)
    {
        const std::string startTimes[] = {
            "08:00", "09:00", "09:55", "11:10", "12:05",
            "13:00", "13:55", "15:10", "16:05", "17:00"
        };

        const std::string endTimes[] = {
            "08:55", "09:55", "10:50", "12:05", "13:00",
            "13:55", "14:50", "16:05", "17:00", "17:55"
        };

        if (slot < 0 || slot >= 10)
            return "Invalid";

        return isEnd ? endTimes[slot] : startTimes[slot];
    }


    void inspectParallelLabs(const ParsedTable& table)
    {
        std::cout << "\n========== PARALLEL LAB TEST ==========\n";

        for (std::size_t r = 1; r < table.rows.size(); r++)
        {
            const ParsedRow& row = table.rows[r];

            if (row.cells.empty())
                continue;

            std::string day = normalizeDay(row.cells[0].text);

            int slot = 0;

            for (std::size_t c = 1; c < row.cells.size(); c++)
            {
                const ParsedCell& cell = row.cells[c];

                int startSlot = slot;
                int endSlot = slot + cell.colSpan - 1;
                slot += cell.colSpan;

                if (cell.text.empty() || isNonAcademic(cell.text))
                    continue;

                std::vector<std::string> segments =
                    splitCellSegments(cell.text);

                std::vector<std::vector<std::string>> labs =
                    splitParallelLabs(segments);

                if (labs.empty())
                    continue;

                std::cout << "\nDay: " << day << '\n';
                std::cout << "Time: "
                        << getSlotTime(startSlot, false) << " - "
                        << getSlotTime(endSlot, true) << '\n';

                for (const auto& lab : labs)
                {
                    std::cout << "Subject: " << lab[0] << '\n';
                    std::cout << "Room: " << lab[1] << "\n";
                    std::cout << "-------------------------\n";
                }
            }
        }
    }

    bool isNonAcademic(const std::string& text)
    {
        std::string value = text;

        for (std::size_t i = 0; i < value.length(); ++i)
        {
            value[i] = static_cast<char>(
                std::tolower(static_cast<unsigned char>(value[i]))
            );
        }

        return value.find("lunch") != std::string::npos ||
               value.find("lib") != std::string::npos ||
               value.find("swayam/training") != std::string::npos ||
               value.find("extra-curricular") != std::string::npos ||
               value.find("csp-301") != std::string::npos;
    }


    bool isLabSubject(const std::string& text)
    {
        return text.find("PCS-") != std::string::npos &&
               text.find("Lab") != std::string::npos;
    }


    bool extractEmbeddedLab(
        const std::string& text,
        std::string& subject,
        std::string& room)
    {
        std::regex roomPattern(
            R"(\bLab\s*-\s*\d+\s*$)",
            std::regex_constants::icase
        );

        std::smatch match;

        if (!std::regex_search(text, match, roomPattern))
            return false;

        room = trim(match.str());

        subject = trim(text.substr(
            0,
            static_cast<std::size_t>(match.position())
        ));

        subject += " Lab";

        return true;
    }


    // Print a cleaned cell, splitting parallel lab sessions.
    void printCleanedCell(
        const std::string& day,
        const std::string& startTime,
        const std::string& endTime,
        const std::string& rawText)
    {
        std::vector<std::string> segments =
            splitCellSegments(rawText);

        if (segments.empty() || isNonAcademic(rawText))
            return;

        // Identify two parallel lab sessions.
        if (segments.size() >= 3 && isLabSubject(segments[0]))
        {
            std::string firstSubject = segments[0];
            std::string firstRoom = segments[1];

            std::string secondSubject;
            std::string secondRoom;

            if (extractEmbeddedLab(
                segments[2], secondSubject, secondRoom))
            {
                std::cout << "\n[LAB SESSION 1]"
                        << "\nDay: " << day
                        << "\nSubject: " << firstSubject
                        << "\nRoom: " << firstRoom
                        << "\nTime: " << startTime
                        << " - " << endTime
                        << "\n";

                std::cout << "\n[LAB SESSION 2]"
                        << "\nDay: " << day
                        << "\nSubject: " << secondSubject
                        << "\nRoom: " << secondRoom
                        << "\nTime: " << startTime
                        << " - " << endTime
                        << "\n";

                return;
            }
        }

        std::cout << "\nDay: " << day
                << "\nTime: " << startTime
                << " - " << endTime
                << "\nContent: ";

        for (std::size_t i = 0; i < segments.size(); ++i)
        {
            if (i > 0)
                std::cout << " | ";

            std::cout << segments[i];
        }

        std::cout << '\n';
    }


    // Print every row and cell in a table.
    void printTable(const ParsedTable& table)
    {
        for (std::size_t r = 0; r < table.rows.size(); ++r)
        {
            std::cout << "\nRow " << r + 1 << ":\n";

            for (std::size_t c = 0;
                 c < table.rows[r].cells.size(); ++c)
            {
                const ParsedCell& cell = table.rows[r].cells[c];

                std::cout
                    << "  Cell " << c + 1
                    << " [colspan=" << cell.colSpan
                    << ", rowspan=" << cell.rowSpan << "]: "
                    << cell.text << '\n';
            }
        }
    }


    std::string quotePath(const std::string& path)
    {
        std::string result = "\"";

        for (std::size_t i = 0; i < path.length(); ++i)
        {
            if (path[i] == '"')
                result += '\\';

            result += path[i];
        }

        result += "\"";

        return result;
    }
}


// ============================================================
// PUBLIC TIMETABLE FETCHER FUNCTIONS
// ============================================================

namespace TimetableFetcher
{
    // Download the timetable HTML.
    bool downloadTimetable(
        const std::string& url,
        const std::string& outputPath)
    {
        std::string command =
            "curl -L --fail --silent --show-error " +
            quotePath(url) +
            " -o " +
            quotePath(outputPath);

        std::cout << "Downloading timetable...\n";

        int result = std::system(command.c_str());

        if (result != 0)
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

        std::cout << "Timetable downloaded successfully: "
                  << outputPath << '\n';

        return true;
    }


    // Inspect the first section's two tables.
    bool inspectFirstSectionTables(const std::string& htmlPath)
    {
        std::string html = readFile(htmlPath);

        if (html.empty())
            return false;

        std::vector<ParsedTable> tables = parseTables(html);

        std::cout << "\nTotal tables found: "
                  << tables.size() << '\n';

        if (tables.size() < 2)
        {
            std::cerr << "First section tables not found.\n";
            return false;
        }

        std::cout << "\n========== FIRST TABLE: TIMETABLE ==========\n";
        printTable(tables[0]);

        std::cout << "\n========== SECOND TABLE: SUBJECT DETAILS ==========\n";
        printTable(tables[1]);

        std::cout << "\nInspection successful.\n";

        return true;
    }


    // Extract and print cleaned timetable entries for CSE A.
    bool inspectFirstSectionSlots(const std::string& htmlPath)
    {
        std::string html = readFile(htmlPath);

        if (html.empty())
            return false;

        std::vector<ParsedTable> tables = parseTables(html);

        if (tables.empty() || tables[0].rows.size() < 2)
        {
            std::cerr << "First timetable table not found.\n";
            return false;
        }

        const std::vector<std::pair<std::string, std::string>> timeSlots =
        {
            {"08:00", "08:55"},
            {"09:00", "09:55"},
            {"09:55", "10:50"},
            {"11:10", "12:05"},
            {"12:05", "13:00"},
            {"13:00", "13:55"},
            {"13:55", "14:50"},
            {"15:10", "16:05"},
            {"16:05", "17:00"},
            {"17:00", "17:55"}
        };

        const ParsedTable& timetable = tables[0];

        std::cout << "\n--- CSE A: Cleaned Timetable ---\n";

        for (std::size_t rowIndex = 1;
             rowIndex < timetable.rows.size();
             ++rowIndex)
        {
            const ParsedRow& row = timetable.rows[rowIndex];

            if (row.cells.empty())
                continue;

            std::string day = normalizeDay(row.cells[0].text);

            int slotIndex = 0;

            for (std::size_t cellIndex = 1;
                 cellIndex < row.cells.size();
                 ++cellIndex)
            {
                const ParsedCell& cell = row.cells[cellIndex];

                int span = cell.colSpan;

                if (span < 1)
                    span = 1;

                if (slotIndex >= static_cast<int>(timeSlots.size()))
                    break;

                int endSlot = slotIndex + span;

                if (endSlot > static_cast<int>(timeSlots.size()))
                    endSlot = static_cast<int>(timeSlots.size());

                if (!cell.text.empty())
                {
                    printCleanedCell(
                        day,
                        timeSlots[slotIndex].first,
                        timeSlots[endSlot - 1].second,
                        cell.text
                    );
                }

                slotIndex += span;
            }
        }

        std::cout << "\nCleaning completed.\n";

        return true;
    }
    bool inspectFirstSectionSubjects(const std::string& htmlPath)
    {
        std::string html = readFile(htmlPath);

        if (html.empty())
            return false;

        std::vector<ParsedTable> tables = parseTables(html);

        if (tables.size() < 2)
        {
            std::cerr << "Subject reference table not found.\n";
            return false;
        }

        const ParsedTable& subjectTable = tables[1];

        std::cout << "\n========== CSE A SUBJECT REFERENCE ==========\n";

        for (std::size_t i = 1; i < subjectTable.rows.size(); ++i)
        {
            const ParsedRow& row = subjectTable.rows[i];

            // Skip headings such as Elective and Lab.
            if (row.cells.size() < 3)
                continue;

            std::string code = cleanReferenceField(row.cells[0].text);
            std::string subject = cleanReferenceField(row.cells[1].text);
            std::string faculty = cleanReferenceField(row.cells[2].text);

            if (code.empty())
                continue;

            std::cout << "\nSubject Code: " << code
                    << "\nSubject Name: " << subject
                    << "\nFaculty: ";

            if (faculty.empty())
                std::cout << "Not specified";
            else
                std::cout << faculty;

            std::cout << "\n----------------------------------------\n";
        }

        std::cout << "\nSubject reference extraction completed.\n";

        return true;
    }

    bool inspectFirstSectionLookup(const std::string& htmlPath)
    {
        std::string html = readFile(htmlPath);

        if (html.empty())
            return false;

        std::vector<ParsedTable> tables = parseTables(html);

        if (tables.size() < 2)
        {
            std::cerr << "Timetable or subject reference table missing.\n";
            return false;
        }

        const ParsedTable& timetable = tables[0];
        const ParsedTable& subjectTable = tables[1];

        std::unordered_map<std::string, SubjectReference> lookup =
            buildSubjectLookup(subjectTable);

        std::cout << "\nTotal subject references loaded: "
                << lookup.size() << '\n';

        const std::vector<std::pair<std::string, std::string>> timeSlots =
        {
            {"08:00", "08:55"},
            {"09:00", "09:55"},
            {"09:55", "10:50"},
            {"11:10", "12:05"},
            {"12:05", "13:00"},
            {"13:00", "13:55"},
            {"13:55", "14:50"},
            {"15:10", "16:05"},
            {"16:05", "17:00"},
            {"17:00", "17:55"}
        };

        std::cout << "\n========== CSE A: TIMETABLE LOOKUP ==========\n";

        for (std::size_t r = 1; r < timetable.rows.size(); ++r)
        {
            const ParsedRow& row = timetable.rows[r];

            if (row.cells.empty())
                continue;

            std::string day = normalizeDay(row.cells[0].text);
            int slotIndex = 0;

            for (std::size_t c = 1; c < row.cells.size(); ++c)
            {
                const ParsedCell& cell = row.cells[c];

                int span = cell.colSpan;

                if (span < 1)
                    span = 1;

                if (slotIndex >= static_cast<int>(timeSlots.size()))
                    break;

                int endSlot = slotIndex + span;

                if (endSlot > static_cast<int>(timeSlots.size()))
                    endSlot = static_cast<int>(timeSlots.size());

                if (!cell.text.empty() && !isNonAcademic(cell.text))
                {
                    std::vector<std::string> codes =
                        extractSubjectCodes(cell.text);

                    if (!codes.empty())
                    {
                        for (std::size_t k = 0; k < codes.size(); ++k)
                        {
                            std::cout << "\nDay: " << day
                                    << "\nTime: "
                                    << timeSlots[slotIndex].first
                                    << " - "
                                    << timeSlots[endSlot - 1].second
                                    << "\nCode: " << codes[k];

                            std::unordered_map<std::string,
                                SubjectReference>::const_iterator found =
                                lookup.find(codes[k]);

                            if (found != lookup.end())
                            {
                                std::cout
                                    << "\nSubject: "
                                    << found->second.subjectName
                                    << "\nFaculty: ";

                                if (found->second.faculty.empty())
                                    std::cout << "Not specified";
                                else
                                    std::cout << found->second.faculty;
                            }
                            else
                            {
                                std::cout
                                    << "\nSubject: Reference not found"
                                    << "\nFaculty: Not available";
                            }

                            std::cout << "\n------------------------------\n";
                        }
                    }
                    else
                    {
                        std::cout << "\nDay: " << day
                                << "\nTime: "
                                << timeSlots[slotIndex].first
                                << " - "
                                << timeSlots[endSlot - 1].second
                                << "\nUnmatched entry: "
                                << cleanReferenceField(cell.text)
                                << "\n------------------------------\n";
                    }
                }

                slotIndex += span;
            }
        }

        std::cout << "\nTimetable lookup completed.\n";

        return true;
    }

    bool inspectFirstSectionRooms(const std::string& htmlPath)
    {
        std::string html = readFile(htmlPath);

        if (html.empty())
            return false;

        std::vector<ParsedTable> tables = parseTables(html);

        if (tables.empty())
            return false;

        inspectCellRooms(tables[0]);

        return true;
    }

    bool inspectFirstSectionParallelLabs(const std::string& htmlPath)
    {
        std::string html = readFile(htmlPath);

        if (html.empty())
        {
            std::cout << "Could not read HTML file.\n";
            return false;
        }

        std::vector<ParsedTable> tables = parseTables(html);

        if (tables.empty())
        {
            std::cout << "No tables found.\n";
            return false;
        }

        inspectParallelLabs(tables[0]);

        return true;
    }

        bool generateFirstSectionSessions(
        const std::string& htmlPath,
        std::vector<Session>& sessions)
    {
        std::string html = readFile(htmlPath);

        if (html.empty())
            return false;

        std::vector<ParsedTable> tables = parseTables(html);

        if (tables.size() < 2)
            return false;

        sessions.clear();

        generateSessionsFromTables(
            tables[0],
            tables[1],
            sessions,
            "CSE A"
        );

        return !sessions.empty();
    }


    bool generateAllSections(
        const std::string& htmlPath,
        const std::vector<std::string>& sectionNames,
        std::vector<Session>& sessions)
    {
        std::string html = readFile(htmlPath);

        if (html.empty())
            return false;

        std::vector<ParsedTable> tables = parseTables(html);

        if (tables.size() != sectionNames.size() * 2)
        {
            std::cout << "Expected "
                    << sectionNames.size() * 2
                    << " tables, found "
                    << tables.size() << '\n';

            return false;
        }

        sessions.clear();

        for (std::size_t i = 0; i < sectionNames.size(); i++)
        {
            std::size_t tableIndex = i * 2;

            generateSessionsFromTables(
                tables[tableIndex],
                tables[tableIndex + 1],
                sessions,
                sectionNames[i]
            );

            std::cout << sectionNames[i] << " processed.\n";
        }

        return !sessions.empty();
    }

}
