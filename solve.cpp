#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <string>
#include <cassert>
#include <ctime>
#include <functional>
#include <unordered_set>
using namespace std;

// this is the wordle game solver inspired by 3b1b
// watch this: https://www.youtube.com/watch?v=v68zYyaEmEA
typedef uint8_t pattern_t;

struct wdString {
    char word[8];   // we'll have padding anyway
    unsigned mask;
    wdString(const string &s)
    {
        strncpy(word, s.c_str(), 6);
        mask = 0;
        for(char c: s) {
            mask |= 1 << (c - 'a');
        }
    }

    bool operator==(const wdString &rhs) const {
        return strcmp(word, rhs.word) == 0;
    }

    pattern_t compare(const wdString &target) const
    {
        pattern_t ret = 0;
        for(int i = 0; i < 5; i++) {
            ret *= 3;
            if(word[i] == target.word[i]) {
                // ret += 0;
            } else if(mask & (1 << (target.word[i] - 'a'))) {
                ret += 1;
            } else {
                ret += 2;
            }
        }
        return ret;
    }
};

ostream &operator<<(ostream &os, const wdString &wds) {
    return os << wds.word;
}

struct wdStringHash {
    size_t operator()(const wdString &s) const {
        return std::hash<std::string_view>{}(s.word);
    }
};

using wdStringArray = vector<wdString>;
using wdStringSet = unordered_set<wdString, wdStringHash>;

wdStringArray possible_answers;
wdStringSet possible_answers_set;
wdStringArray possible_words;

// deserialize pattern
std::string get_pattern_string(pattern_t pattern)
{
    static char pat[6] = {0};
    pat[5] = '\0';
    for(int i = 0; i < 5; i++) {
        int cur = pattern % 3;
        if(cur == 0) pat[4 - i] = 'c';
        else if(cur == 1) pat[4 - i] = 'p';
        else pat[4 - i] = 'n';
        pattern /= 3;
    }
    return string(pat);
}

inline float calc_entropy(const wdString &target, const wdStringSet &word_list)
{
    static std::vector<int16_t> cnt(243, 0);
    fill(cnt.begin(), cnt.end(), 0);
    int total = 0;
    for(const wdString &w: word_list) {
        if(possible_answers_set.find(w) != possible_answers_set.end()) {
            pattern_t p = target.compare(w);
            cnt[p]++;
            total++;
        }
    }
    // pattern 0 is ccccc which is correct
    // we give it a higher expectation
    float exp = 0;
    if(cnt[0]) exp = log2f(float(total) / cnt[0]) * cnt[0] * 2;
    for(int i = 1; i < 243; i++) if(cnt[i]) {
        exp += log2f(float(total) / cnt[i]) * cnt[i];
    }
    exp /= total;
    return exp;
}

wdString calc_entropies(const wdStringArray &candidates, const wdStringSet &valid)
{
    typedef pair<wdString, float> pr;
    // static const vector<pr> initial = {
    //     { wdString("tares"), 6.60114 },
    //     { wdString("lares"), 6.55608 },
    //     { wdString("rales"), 6.51277 },
    //     { wdString("pares"), 6.49504 },
    //     { wdString("cares"), 6.49041 },
    //     { wdString("rates"), 6.47933 },
    //     { wdString("teras"), 6.47299 },
    //     { wdString("nares"), 6.46856 },
    //     { wdString("tores"), 6.45933 },
    //     { wdString("tales"), 6.45901 },
    // };
    vector<pr> ans;
    if(candidates.size() == valid.size()) {
        return wdString("tares");
    } else {
        for(const wdString &w: candidates) {
            ans.push_back({w, calc_entropy(w, valid)});
        }
    }
    assert(ans.size() > 0);
    // sort & get top
    sort(ans.begin(), ans.end(), [&](const pr &a, const pr &b) {
        if(a.second != b.second) {
            return a.second > b.second;
        } else {
            return valid.count(a.first) > 0;
        }
    });
    return ans[0].first;
}

// read all files
void init_files() {
    ifstream f_answers("possible_answers.txt");
    possible_answers.reserve(2309);
    std::string buf;
    while(f_answers >> buf) {
        possible_answers.push_back(wdString(buf));
    }
    f_answers.close();
    possible_answers_set = wdStringSet(possible_answers.begin(), possible_answers.end());
    ifstream f_words("possible_words.txt");
    possible_words.reserve(10665);
    while(f_words >> buf) {
        possible_words.push_back(wdString(buf));
    }
    f_words.close();

    assert(possible_answers_set.find(wdString("tweak")) != possible_answers_set.end());
}

int main(void) {
    srand(time(NULL));
    init_files();

    wdStringSet word_list;
    wdStringSet word_set(possible_answers.begin(), possible_answers.end());
    int stat = 0;
    for(const wdString &choice: possible_answers) {
        int i;
        word_list = wdStringSet(possible_words.begin(), possible_words.end());
        wdStringArray guesses;
        for(i = 0; i < 10; i++) {
            if(word_list.size() == 1) {
                break;
            }
            wdString user_input = calc_entropies(possible_words, word_list);
            if(strcmp(user_input.word, choice.word) == 0) {
                break;
            }
            pattern_t pat = choice.compare(user_input);
            wdStringSet new_list;
            for(const wdString &w: word_list) {
                if(w.compare(user_input) == pat) {
                    new_list.insert(w);
                }
            }
            word_list = new_list;
            guesses.push_back(user_input);
        }
        // cout << "Got " << choice << " in " << (i + 1) << " guesses" << endl;

        if(i > 5) {
            cout << "Choices:\t";
            for(const wdString &w: guesses) cout << w << '\t';
            cout << endl;
        }

        stat += i + 1;
    }
    cout << "Average =\t" << (float(stat) / word_set.size()) << endl;
}