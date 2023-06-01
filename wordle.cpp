#include "wordle.h"
#include <algorithm>
#include <cassert>
#include <cmath>

using namespace std;
// this is the wordle game solver inspired by 3b1b
// watch this: https://www.youtube.com/watch?v=v68zYyaEmEA

wdStringArray possible_answers;
wdStringArray possible_words;
wdStringSet possible_answers_set;

pattern_t wdString::compare(const wdString &real_answer) const {
    pattern_t ret = 0;
    static const uint8_t weights[] = {1, 3, 9, 27, 81};
    // make a copy of data
    char buf1[8], buf2[8];
    *((uint64_t *)buf1) = data.data64;
    *((uint64_t *)buf2) = real_answer.data.data64;
    // pass 1(green)
    for (int i = 0; i < 5; i++) {
        if (buf1[i] == buf2[i]) {
            ret += weights[i] * 2;
            buf1[i] = buf2[i] = '\0';
        }
    }
    // pass 2(yellow)
    for (int i = 0; i < 5; i++)
        if (buf1[i]) {
            char *found = (char *)memchr(buf2, buf1[i], 5);
            if (found) {
                ret += weights[i];
                *found = '\0';
            }
        }
    // remaining is zero
    return ret;
}

// deserialize pattern
std::string get_pattern_string(pattern_t pattern) {
    thread_local char pat[6] = {0};
    pat[5] = '\0';
    for (int i = 0; i < 5; i++) {
        int cur = pattern % 3;
        if (cur == 0)
            pat[i] = 'n';
        else if (cur == 1)
            pat[i] = 'p';
        else
            pat[i] = 'c';
        pattern /= 3;
    }
    return string(pat);
}

pattern_t from_pattern_string(std::string_view pattern) {
    pattern_t ret = 0;
    pattern_t weights[] = {1, 3, 9, 27, 81};
    for (int i = 0; i < 5; i++) {
        if (pattern[i] == 'n') {
            // do nothing
        } else if (pattern[i] == 'p') {
            ret += weights[i];
        } else if (pattern[i] == 'c') {
            ret += 2 * weights[i];
        } else {
            assert(false);
            return 255;
        }
    }
    return ret;
}

std::string print_string_with_pattern(wdString w, pattern_t pat) {
    std::string ret;
    ret.reserve(48);
    for (int i = 0; i < 5; i++) {
        uint8_t cp = pat % 3;
        if (cp == 0) {
            ret += "\e[1;40m";
        } else if (cp == 1) {
            ret += "\e[1;43m";
        } else {
            ret += "\e[1;42m";
        }
        ret += w.data.str[i];
        ret += "\e[0m";
        pat /= 3;
    }
    return ret;
}

float calc_entropy(wdString guess, const wdStringSet &word_list) {
    assert(word_list.size() > 0);
    int16_t cnt[243] = {0};
    memset(cnt, 0, sizeof(cnt));
    for (const wdString &w : word_list) {
        pattern_t p = guess.compare(w);
        cnt[p]++;
    }
    float exp = 0;
    float total = float(word_list.size());
    for (int i = 0; i < 243; i++) {
        if (cnt[i]) {
            exp += log2f(total / cnt[i]) * cnt[i];
        }
    }
    exp /= total;
    assert(!isnan(exp));
    return exp;
}

vector<pair<wdString, float>> get_topn(const wdStringSet &valid, int n) {
    typedef pair<wdString, float> pr;
    static const vector<pr> initial = {
        {wdString("soare"), 5.885}, {wdString("roate"), 5.885},
        {wdString("raise"), 5.878}, {wdString("reast"), 5.868},
        {wdString("raile"), 5.865}, {wdString("slate"), 5.856},
        {wdString("salet"), 5.836}, {wdString("crate"), 5.835},
        {wdString("irate"), 5.833}, {wdString("trace"), 5.830},
    };
    assert(valid.size() > 0);
    vector<pr> ans;
    if (valid.size() > 2000) {
        ans = initial;
    } else {
        ans.reserve(valid.size() + possible_words.size() - possible_answers.size());
        // first all valid strings
        for (const wdString &w : valid) {
            ans.push_back({w, calc_entropy(w, valid)});
        }
        // all words, except for those answers!
        for (const wdString &w : possible_words)
            if (valid.find(w) == valid.cend()) {
                ans.push_back({w, calc_entropy(w, valid)});
            }
        assert(ans.size() > 0);
        // do stable sort so strings in valid set get to be picked first
        stable_sort(ans.begin(), ans.end(),
                                [&](const pr &a, const pr &b) { return a.second > b.second; });
    }

    return vector<pr>(ans.begin(), ans.begin() + n);
}