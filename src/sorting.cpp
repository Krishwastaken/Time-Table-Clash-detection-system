#include "sorting.h"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// Monday = 0 ... Sunday = 6, unknown = 7. Case-insensitive.
int dayRank(std::string day) {
    std::transform(day.begin(), day.end(), day.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    static const std::vector<std::string> days = {
        "monday", "tuesday", "wednesday", "thursday",
        "friday", "saturday", "sunday"
    };

    auto it = std::find(days.begin(), days.end(), day);
    return it == days.end() ? 7 : static_cast<int>(it - days.begin());
}

// Strict "a comes before b": weekday order, then start time (HH:MM is zero-padded).
bool before(const Session& a, const Session& b) {
    int da = dayRank(a.getDay());
    int db = dayRank(b.getDay());

    if (da != db)
        return da < db;

    return a.getStartTime() < b.getStartTime();
}

// Merges the sorted halves [left, mid) and [mid, right).
// STABLE: the right element is taken only if it is strictly before the left one,
// so equal elements keep their original relative order.
void merge(std::vector<Session>& sessions, std::vector<Session>& temp,
           size_t left, size_t mid, size_t right) {
    size_t i = left, j = mid, k = left;

    while (i < mid && j < right) {
        if (before(sessions[j], sessions[i]))
            temp[k++] = sessions[j++];
        else
            temp[k++] = sessions[i++];
    }

    while (i < mid)   temp[k++] = sessions[i++];
    while (j < right) temp[k++] = sessions[j++];

    for (size_t x = left; x < right; ++x)
        sessions[x] = temp[x];
}

void mergeSortRecursive(std::vector<Session>& sessions, std::vector<Session>& temp,
                        size_t left, size_t right) {
    if (right - left < 2)
        return;

    size_t mid = left + (right - left) / 2;
    mergeSortRecursive(sessions, temp, left, mid);
    mergeSortRecursive(sessions, temp, mid, right);
    merge(sessions, temp, left, mid, right);
}

}  // namespace

// Overload 1: stable two-way merge sort by weekday, then start time. O(n log n).
void sortSessions(std::vector<Session>& sessions) {
    if (sessions.empty())
        throw std::invalid_argument("Cannot sort an empty session list.");

    std::vector<Session> temp(sessions.size());  // one shared buffer
    mergeSortRecursive(sessions, temp, 0, sessions.size());
}

// Overload 2: stable insertion sort by "faculty" or "room". O(n^2).
// Elements are shifted only when strictly greater, which keeps it stable.
void sortSessions(std::vector<Session>& sessions, const std::string& key) {
    if (sessions.empty())
        throw std::invalid_argument("Cannot sort an empty session list.");

    if (key != "faculty" && key != "room")
        throw std::invalid_argument("Unknown sort key: " + key);

    auto keyOf = [&key](const Session& s) {
        return key == "faculty" ? s.getFaculty() : s.getRoom();
    };

    for (size_t i = 1; i < sessions.size(); ++i) {
        Session current = sessions[i];
        const std::string currentKey = keyOf(current);
        size_t j = i;

        while (j > 0 && keyOf(sessions[j - 1]) > currentKey) {
            sessions[j] = sessions[j - 1];
            --j;
        }

        sessions[j] = current;
    }
}
