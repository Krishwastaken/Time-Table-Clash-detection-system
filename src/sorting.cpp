#include "sorting.h"
#include <stdexcept>

// Uses Session::operator< (day order, then start time) as the comparison

static void merge(std::vector<Session>& sessions, int left, int mid, int right) 
{
    std::vector<Session> temp;
    int i = left, j = mid + 1;

    while (i <= mid && j <= right) {
        if (sessions[i] < sessions[j]) temp.push_back(sessions[i++]);
        else temp.push_back(sessions[j++]);
    }
    while (i <= mid) temp.push_back(sessions[i++]);
    while (j <= right) temp.push_back(sessions[j++]);

    for (int k = 0; k < (int)temp.size(); k++)
        sessions[left + k] = temp[k];
}

static void mergeSortRecursive(std::vector<Session>& sessions, int left, int right) 
{
    if (left >= right) return;
    int mid = left + (right - left) / 2;
    mergeSortRecursive(sessions, left, mid);
    mergeSortRecursive(sessions, mid + 1, right);
    merge(sessions, left, mid, right);
}

// Overload 1: default sort — day + start time
void sortSessions(std::vector<Session>& sessions) {
    if (sessions.empty())                                   // Exception Handling
        throw std::invalid_argument("Cannot sort an empty session list.");
    mergeSortRecursive(sessions, 0, sessions.size() - 1);
}

// Overload 2: DSA "Sorting on Different Keys" — sort by faculty or room instead
void sortSessions(std::vector<Session>& sessions, const std::string& key) {
    if (sessions.empty())
        throw std::invalid_argument("Cannot sort an empty session list.");

    // Insertion-sort style key sort, kept separate from Merge Sort so both algorithms are clearly visible as distinct pieces of work.
    for (size_t i = 1; i < sessions.size(); i++) {
        Session current = sessions[i];
        int j = (int)i - 1;
        while (j >= 0) {
            bool shouldShift = (key == "faculty")
                            ? (sessions[j].getFaculty() > current.getFaculty())
                            : (key == "room")
                            ? (sessions[j].getRoom() > current.getRoom())
                            : false;

            if (!shouldShift) break;

            sessions[j + 1] = sessions[j];
            j--;
        }
        sessions[j + 1] = current;
    }
}