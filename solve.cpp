#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <string>
#include <iomanip>
#include <cassert>
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

pattern_t from_pattern_string(string pattern)
{
    pattern_t ret = 0;
    for(int i = 0; i < 5; i++) {
        ret *= 3;
        if(pattern[i] == 'c') {
            // do nothing
        } else if(pattern[i] == 'p') {
            ret += 1;
        } else if(pattern[i] == 'n') {
            ret += 2;
        } else {
            assert(false);
            return 255;
        }
    }
    return ret;
}

inline float calc_entropy(wdString target, const wdStringSet &word_list)
{
    assert(word_list.size() > 0);
    static std::vector<int16_t> cnt(243, 0);
    fill(cnt.begin(), cnt.end(), 0);
    for(const wdString &w: word_list) {
        pattern_t p = target.compare(w);
        cnt[p]++;
    }
    // pattern 0 is ccccc which is correct
    // we give it a higher expectation
    float exp = 0;
    float total = float(word_list.size());
    for(int i = 0; i < 243; i++) if(cnt[i]) {
        exp += log2f(total / cnt[i]) * cnt[i];
    }
    exp /= total;
    assert(!isnan(exp));
    return exp;
}

wdString calc_entropies(const wdStringArray &candidates, const wdStringSet &valid, bool output = false)
{
    typedef pair<wdString, float> pr;
    static const vector<pr> initial = {
        { wdString("slate"), 6.19592 },
        { wdString("crate"), 6.14959 },
        { wdString("trace"), 6.12446 },
        { wdString("stare"), 6.10236 },
        { wdString("saner"), 6.05645 },
        { wdString("stale"), 6.04945 },
        { wdString("react"), 6.02973 },
        { wdString("least"), 6.00874 },
        { wdString("snare"), 6.00781 },
        { wdString("grate"), 6.00032 },
    };
    assert(valid.size() > 0);
    vector<pr> ans;
    if(valid.size() > 2000) {
        ans = initial;
    } else {
        // first all valid strings
        for(const wdString &w: valid) {
            ans.push_back({w, calc_entropy(w, valid)});
        }
        // then all candidates
        for(const wdString &w: candidates) if(valid.find(w) == valid.cend()) {
            ans.push_back({w, calc_entropy(w, valid)});
        }
    }
    assert(ans.size() > 0);
    // do stable sort so strings in valid set get to be picked first
    stable_sort(ans.begin(), ans.end(), [&](const pr &a, const pr &b) {
        return a.second > b.second;
    });

    if(output) {
        cout << "Top 10 recommended guesses:" << endl;
        for(int i = 0; i < 10; i++) {
            const auto &p = ans[i];
            if(valid.find(p.first) != valid.end()) {
                cout << "✅";
            } else {
                cout << "❌";
            }
            cout << p.first << '\t' << fixed << setprecision(3) << p.second << endl;
        }
        cout << endl;
    }
    return ans[0].first;
}

// read all files
void init_files() {
    ifstream f_answers("possible_answers.txt");
    possible_answers.reserve(2500);
    std::string buf;
    while(f_answers >> buf) {
        possible_answers.push_back(wdString(buf));
    }
    f_answers.close();
    possible_answers_set = wdStringSet(possible_answers.begin(), possible_answers.end());
    ifstream f_words("possible_words.txt");
    possible_words.reserve(13000);
    while(f_words >> buf) {
        possible_words.push_back(wdString(buf));
    }
    f_words.close();

    assert(possible_answers_set.find(wdString("tweak")) != possible_answers_set.end());
}

void benchmark() {
    wdStringSet word_left;
    wdStringSet answers_set(possible_answers.begin(), possible_answers.end());
    vector<int> distribution(7, 0);
    int stat = 0;

    for(int i = 0; i < possible_answers.size(); i++) {
        const wdString &choice = possible_answers[i];
        int j;
        word_left = wdStringSet(possible_answers.begin(), possible_answers.end());
        wdStringArray guesses;
        for(j = 0; j < 10; j++) {
            wdString user_input = calc_entropies(possible_words, word_left);
            if(strcmp(user_input.word, choice.word) == 0) {
                break;
            }
            pattern_t pat = choice.compare(user_input);
            wdStringSet new_list;
            for(const wdString &w: word_left) {
                if(w.compare(user_input) == pat) {
                    new_list.insert(w);
                }
            }
            word_left = new_list;
            guesses.push_back(user_input);
        }
        // too long
        if(j > 6) {
            cout << "Choices:\t";
            for(const wdString &w: guesses) cout << w << '\t';
            cout << endl;
            distribution[6] += 1;
        } else {
            distribution[j] += 1;
        }
        stat += j + 1;
        if(i > 0 && i % 100 == 0) {
            cout << ".";
            cout.flush();
        }
    }
    cout << endl;
    cout << "Average =\t" << (float(stat) / possible_answers.size()) << endl;
    cout << "Distribution:\n";
    for(int i = 0; i < 6; i++) {
        cout << (i + 1) << '\t' << distribution[i] << endl;
    }
    cout << ">6\t" << distribution[6] << endl;
}

void cheat()
{
    wdStringSet left = possible_answers_set;
    while(left.size() > 0) {
        string buf;
        wdString user_input("");
        pattern_t pat;

        cout << left.size() << " possible words left" << endl;
        calc_entropies(possible_answers, left, true);
        
        do {
            cout << "Your input: ";
            cin >> buf;
        } while(buf.size() != 5);
        user_input = wdString(buf);

        do {
            cout << "Returned pattern: ";
            cin >> buf;
            pat = from_pattern_string(buf);
        } while(pat == 255);

        // filter words
        wdStringSet new_left;
        for(wdString w: left) {
            if(w.compare(user_input) == pat) {
                new_left.insert(w);
            }
        }
        left = new_left;
    }
    if(left.size() == 0) {
        cout << "No possibility left." << endl;
    }
}

int main(void) {
    init_files();

    // maybe you want to do benchmark
    // benchmark();

    // or maybe you just want to cheat
    cheat();
    return 0;
}
