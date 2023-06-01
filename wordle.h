#pragma once
#include <set>
#include <vector>
#include <string>
#include <string_view>
#include <iostream>

typedef uint8_t pattern_t;
struct wdString;
struct wdStringCmp;
struct wdStringHash;
using wdStringArray = std::vector<wdString>;
// TODO use boost::flat_map instead
using wdStringSet = std::set<wdString, wdStringCmp>;

struct wdString {
public:
    union {
        char str[8];
        uint64_t data64;
    } data;
    wdString(std::string w) {
        assert(w.length() == 5);
        data.data64 = 0;
        strncpy(data.str, w.c_str(), 6);
    }
    bool eql(const wdString &rhs) const { return data.data64 == rhs.data.data64; }
    pattern_t compare(const wdString &real_answer) const;
};

inline std::ostream &operator<<(std::ostream &os, const wdString &wd) {
    return os << wd.data.str;
}

struct wdStringHash {
    bool operator()(const wdString &s) const {
        return std::hash<std::string_view>()(std::string_view(s.data.str, 5));
    }
};

struct wdStringCmp {
    bool operator()(const wdString &lhs, const wdString &rhs) const {
        return lhs.data.data64 < rhs.data.data64;
    }
};

extern wdStringArray possible_answers;
extern wdStringArray possible_words;
extern wdStringSet possible_answers_set;

// pattern string related stuff
std::string get_pattern_string(pattern_t pattern);
pattern_t from_pattern_string(std::string_view pattern);
std::string print_string_with_pattern(wdString w, pattern_t pat);

// the real algorithm
float calc_entropy(wdString guess, const wdStringSet &word_list);
std::vector<std::pair<wdString, float>> get_topn(const wdStringSet &valid, int n);