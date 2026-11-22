// Unit tests for Session ordering, merge sort and ClashDetector.
// Build (from the repository root):
//   g++ -std=c++17 -Wall -Wextra -Isrc tests/test_detector.cpp src/Session.cpp src/sorting.cpp src/ClashDetector.cpp -o test_detector.exe
#include "ClashDetector.h"
#include "Session.h"
#include "sorting.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

static int failures = 0;
static int checks = 0;

#define CHECK(cond, msg) do { ++checks; if (!(cond)) { ++failures; \
    cout << "  FAIL: " << msg << "  (line " << __LINE__ << ")\n"; } } while (0)

static Session S(const string& subj, const string& fac, const string& room,
                 const string& sec, const string& day, const string& st,
                 const string& en, int n = 40,
                 WeekRule r = WeekRule::Always) {
    return Session(subj, fac, room, sec, day, st, en, n, r);
}

static int count(const vector<Clash>& c, ClashType t) {
    int n = 0;
    for (const Clash& x : c) if (x.type == t) ++n;
    return n;
}

static void test(const string& name, void (*fn)()) {
    cout << name << '\n';
    fn();
}

static void roomClash() {
    vector<Session> v = { S("A","F1","CR-1","X","Monday","09:00","10:00"),
                          S("B","F2","CR-1","Y","Monday","09:30","10:30") };
    auto c = ClashDetector::detectClashes(v);
    CHECK(c.size() == 1 && c[0].type == ClashType::Room, "one room clash");
}
static void facultyClash() {
    vector<Session> v = { S("A","Dr. Rao","CR-1","X","Monday","09:00","10:00"),
                          S("B","Mr. Rao","CR-2","Y","Monday","09:30","10:30") };
    auto c = ClashDetector::detectClashes(v);   // "Dr. Rao" == "Mr. Rao" after honorific strip
    CHECK(count(c, ClashType::Faculty) == 1, "faculty clash across honorifics");
    CHECK(count(c, ClashType::Room) == 0, "no room clash");
}
static void sectionClash() {
    vector<Session> v = { S("A","F1","CR-1","X","Monday","09:00","10:00"),
                          S("B","F2","CR-2","X","Monday","09:30","10:30") };
    auto c = ClashDetector::detectClashes(v);
    CHECK(c.size() == 1 && c[0].type == ClashType::Section, "one section clash");
}
static void adjacent() {
    vector<Session> v = { S("A","F1","CR-1","X","Monday","09:00","09:55"),
                          S("B","F1","CR-1","X","Monday","09:55","10:50") };
    CHECK(ClashDetector::detectClashes(v).empty(), "touching intervals do not clash");
}
static void duplicates() {
    vector<Session> v = { S("A","F1","CR-1","X","Monday","09:00","10:00"),
                          S("A","F1","CR-1","X","Monday","09:00","10:00") };
    auto c = ClashDetector::detectClashes(v);
    CHECK(c.size() == 1 && c[0].type == ClashType::DuplicateSession,
          "duplicate reported once, no extra faculty/room/section clash");
}
static void capacity() {
    vector<Session> v = { S("A","F1","CR-1","X","Monday","09:00","10:00", 81),
                          S("B","F2","LT-1","Y","Monday","09:00","10:00", 160),
                          S("C","F3","LT-2","Z","Monday","09:00","10:00", 161),
                          S("D","F4","Digital Lab-1","W","Monday","09:00","10:00", 81) };
    auto c = ClashDetector::detectClashes(v);
    CHECK(count(c, ClashType::RoomCapacity) == 3, "CR 81>80, LT 161>160, Digital Lab 81>80 (LT 160 ok)");
}
static void parallelLabs() {
    vector<Session> v = { S("PCS-308 Lab (G1)","F1","Lab-1","X","Monday","09:55","12:05"),
                          S("PCS-307 Lab (G2)","F2","Lab-5","X","Monday","09:55","12:05") };
    CHECK(ClashDetector::detectClashes(v).empty(), "G1/G2 parallel labs are legal");
    v.push_back(S("OOP","F3","CR-2","X","Monday","10:00","11:00"));
    auto c = ClashDetector::detectClashes(v);
    CHECK(count(c, ClashType::Section) == 2, "whole-class lecture clashes with each lab group");
}
static void sharedFacilitiesAndCase() {
    vector<Session> v = { S("A","F1","Digital Lab-1","X","Tuesday","09:00","10:00"),
                          S("B","F2","DIGITAL LAB - 1","Y","tuesday","09:00","10:00"),
                          S("C","F3","lt-13","Z","Tuesday","09:00","10:00"),
                          S("D","F4","LT-13","W","Tuesday","09:00","10:00") };
    auto c = ClashDetector::detectClashes(v);
    CHECK(count(c, ClashType::Room) == 2, "case/spacing-insensitive room match (labs and LT-13)");
}
static void freeSessions() {
    vector<Session> v = { S("LUNCH","","","X","Monday","13:00","13:55"),
                          S("LIB","","","X","Monday","13:00","13:55"),
                          S("SWAYAM/Training/Extra-Curricular Activities/LIB/ CSP-301","","","X","Monday","13:00","13:55"),
                          S("A","F1","CR-1","X","Monday","13:00","13:55") };
    CHECK(ClashDetector::detectClashes(v).empty(), "free periods never clash");
}
static void invalidRows() {
    vector<Session> v = { S("A","F1","CR-1","X","Monday","10:00","09:00"),
                          S("B","F1","CR-1","X","Monday","25:00","26:00"),
                          S("C","F1","CR-1","X","Someday","09:00","10:00"),
                          S("D","F1","CR-1","X","Monday","","") };
    CHECK(ClashDetector::detectClashes(v).empty(), "invalid rows are ignored without crashing");
}
static void combinedLecture() {
    vector<Session> v = { S("Data Structures","Mr. U","GEHU-Audi (5th Floor)","A","Monday","12:05","13:00", 80),
                          S("Data Structures","Mr. U","GEHU-Audi (5th Floor)","B","Monday","12:05","13:00", 80) };
    CHECK(ClashDetector::detectClashes(v).empty(), "shared lecture is not a room/faculty clash");
}
static void president() {
    vector<Session> v = { S("DS","Hon'ble President Sir / Mr. A","CR-1","X","Monday","09:00","10:00"),
                          S("DS","Hon'ble President Sir / Mr. B","CR-2","Y","Monday","09:00","10:00"),
                          S("OOP","Mr. A","CR-3","Z","Monday","09:00","10:00"),
                          S("OOP2","F9","CR-1","Q","Monday","09:00","10:00") };
    auto off = ClashDetector::detectClashes(v, false);
    auto on  = ClashDetector::detectClashes(v, true);
    CHECK(count(off, ClashType::Faculty) == 2, "flag off: president counted as shared teacher");
    CHECK(count(on, ClashType::Faculty) == 1, "flag on: only Mr. A is double booked");
    CHECK(count(on, ClashType::Room) == 1, "flag on: president session still in room checks");
}
static void unknownFaculty() {
    vector<Session> v = { S("Elective","Not specified","","A","Tuesday","16:05","17:00",0),
                          S("Elective","Not specified","","B","Tuesday","16:05","17:00",0) };
    CHECK(ClashDetector::detectClashes(v).empty(), "unspecified faculty is not one shared person");
}
static void saturdayRule() {
    vector<Session> v = {
        S("TCS-307 *","F1","LT-2","A","Thursday","17:00","17:55",80, WeekRule::SaturdayOffOnly),
        S("TCS-999","F2","LT-2","B","Thursday","17:00","17:55",80),          // always active
        S("PCS Lab","F3","Lab-1","A","Saturday","08:00","09:55",40),
        S("PCS Lab","F4","Lab-1","B","Saturday","09:00","09:55",40) };
    auto c = ClashDetector::detectClashes(v);
    CHECK(count(c, ClashType::Room) == 2, "two room clashes");
    bool offSeen = false, workSeen = false;
    for (auto& x : c) {
        if (x.scenario == WeekRule::SaturdayOffOnly) offSeen = true;
        if (x.scenario == WeekRule::SaturdayWorkingOnly) workSeen = true;
    }
    CHECK(offSeen && workSeen, "starred clash tagged Saturday-off, Saturday clash tagged Saturday-working");
    // starred class vs a Saturday class can never be active together
    vector<Session> w = {
        S("X *","F1","LT-2","A","Saturday","09:00","10:00",80, WeekRule::SaturdayOffOnly),
        S("Y","F2","LT-2","B","Saturday","09:00","10:00",80) };
    CHECK(ClashDetector::detectClashes(w).empty(), "Saturday-off-only vs Saturday-working-only never co-occur");
}

static void labBlockPool() {
    const string blk = "B.Tech. Block IInd Floor Lab-1";
    vector<Session> v;
    for (int i = 0; i < 4; ++i)
        v.push_back(S("LDCO Lab (G1)", "F" + to_string(i), blk, "S" + to_string(i), "Tuesday", "08:00", "09:55", 40));
    CHECK(ClashDetector::detectClashes(v).empty(), "4 parallel sessions fit the 4-lab block");
    v.push_back(S("LDCO Lab (G1)", "F9", blk, "S9", "Tuesday", "08:00", "09:55", 40));
    auto c = ClashDetector::detectClashes(v);
    CHECK(count(c, ClashType::Room) == 1, "5th parallel session overflows the block");
    // starred sessions only count in Saturday-off weeks
    vector<Session> w;
    for (int i = 0; i < 4; ++i)
        w.push_back(S("LDCO Lab (G2)", "F" + to_string(i), blk, "S" + to_string(i), "Friday", "13:55", "16:05", 40));
    w.push_back(S("LDCO Lab (G2)", "F8", blk, "S8", "Friday", "13:55", "16:05", 40, WeekRule::SaturdayOffOnly));
    auto d = ClashDetector::detectClashes(w);
    CHECK(d.size() == 1 && d[0].scenario == WeekRule::SaturdayOffOnly, "overflow tagged Saturday-off only");
    // a plain Lab-1 is a different room from the block
    vector<Session> x = { S("A","F1",blk,"X","Monday","08:00","09:55"), S("B","F2","Lab-1","Y","Monday","08:00","09:55") };
    CHECK(ClashDetector::detectClashes(x).empty(), "Lab-1 and the block are different rooms");
}
static void auditAndMultiRoom() {
    vector<Session> v = { S("Probability and Statistics (Audit Course)","Dr. T","","A","Monday","15:10","16:05"),
                          S("Probability and Statistics (Audit Course)","Dr. T","","B","Monday","15:10","16:05"),
                          S("Advance Probability for AI","Dr. T","LT-5","C","Monday","15:10","16:05") };
    CHECK(ClashDetector::detectClashes(v).empty(), "audit course is not a real class");
    vector<Session> w = { S("DS Lab","F1","Lab-7 & Lab-6","A","Monday","13:55","16:05",80),
                          S("DS Lab","F2","Lab-6","B","Monday","13:55","16:05",40) };
    auto c = ClashDetector::detectClashes(w);
    CHECK(count(c, ClashType::Room) == 1, "a session in two labs blocks both");
    CHECK(count(c, ClashType::RoomCapacity) == 0, "capacity of two labs adds up (40 + 80)");
}
static void combinedNeedsSameWeeks() {
    vector<Session> v = { S("DSC","Dr. A","CR-7","I","Friday","16:05","17:00",80),
                          S("DSC","Dr. B","CR-7","E","Friday","16:05","17:00",80, WeekRule::SaturdayOffOnly) };
    CHECK(count(ClashDetector::detectClashes(v), ClashType::Room) == 1, "same subject/room but different weeks and teachers is a clash");
}
static void sorting() {
    vector<Session> v = { S("a","f","r","s","Friday","08:00","09:00"),
                          S("b","f","r","s","Monday","10:00","11:00"),
                          S("c","f","r","s","Monday","09:00","10:00"),
                          S("d","f","r","s","Saturday","08:00","09:00"),
                          S("e","f","r","s","Monday","09:00","09:30") };
    sortSessions(v);
    string order;
    for (auto& s : v) order += s.getSubject();
    CHECK(order == "cebad", "weekday order, start time, stable on ties (got " + order + ")");
}
static void randomisedAgainstBruteForce() {
    srand(12345);
    const char* days[] = {"Monday","Tuesday","Wednesday"};
    const char* rooms[] = {"CR-1","CR-2","LT-1"};
    vector<Session> v;
    for (int i = 0; i < 400; ++i) {
        int st = 8*60 + 5*(rand()%90), len = 5*(1 + rand()%20);
        auto hm = [](int m){ char b[16]; snprintf(b,16,"%02d:%02d",m/60,m%60); return string(b); };
        v.push_back(S("sub"+to_string(i), "fac"+to_string(i), rooms[rand()%3],
                      "sec"+to_string(i), days[rand()%3], hm(st), hm(st+len)));
    }
    long expected = 0;
    for (size_t i = 0; i < v.size(); ++i)
        for (size_t j = i+1; j < v.size(); ++j) {
            if (v[i].getDay()!=v[j].getDay() || v[i].getRoom()!=v[j].getRoom()) continue;
            if (v[i].getStartTime() < v[j].getEndTime() && v[j].getStartTime() < v[i].getEndTime()) ++expected;
        }
    auto c = ClashDetector::detectClashes(v);
    CHECK((long)c.size() == expected && count(c, ClashType::Room) == (int)expected,
          "sweep line found " + to_string(c.size()) + ", brute force " + to_string(expected));
}

int main() {
    test("room clash", roomClash);
    test("faculty clash", facultyClash);
    test("section clash", sectionClash);
    test("adjacent intervals", adjacent);
    test("duplicates", duplicates);
    test("capacity", capacity);
    test("parallel labs", parallelLabs);
    test("shared facilities + case", sharedFacilitiesAndCase);
    test("free sessions", freeSessions);
    test("invalid rows", invalidRows);
    test("combined lecture", combinedLecture);
    test("president", president);
    test("unspecified faculty", unknownFaculty);
    test("saturday rule", saturdayRule);
    test("lab block pool", labBlockPool);
    test("audit + multi-room", auditAndMultiRoom);
    test("combined needs same weeks", combinedNeedsSameWeeks);
    test("sorting", sorting);
    test("sweep vs brute force", randomisedAgainstBruteForce);
    cout << "\n" << (checks - failures) << "/" << checks << " checks passed\n";
    return failures ? 1 : 0;
}
