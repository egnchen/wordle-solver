#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <unistd.h>
#include <string>
#include <iomanip>
#include <cassert>
#include <functional>
#include <cstring>
#include <algorithm>
#include <mutex>
#include <thread>
#include <set>
#include <unordered_set>

using namespace std;

const char *usage_prompt = "Options:\n\
\t-b: Run benchmark\n\
\t-c: Run interactive cheating\n\
\t-h: Show this message\n\
\n\
Help on interactive cheating:\n\
c represent correct\n\
n represents null/non-existent\n\
p represents exist but position not right\n\
For example, \"ppcnn\" means the first two have wrong positions, the middle one is correct and the rest aren't find anywhere in this word.\
";

// this is the wordle game solver inspired by 3b1b
// watch this: https://www.youtube.com/watch?v=v68zYyaEmEA
typedef uint8_t pattern_t;
struct wdString;
struct wdStringCmp;
using wdStringArray = vector<wdString>;
using wdStringSet = set<wdString, wdStringCmp>;

struct wdString {
public:
    union {
        char str[8];
        uint64_t data64;
    } data;
    wdString(string w) {
        assert(w.length() == 5);
        data.data64 = 0;
        strncpy(data.str, w.c_str(), 6);
    }
    bool eql(const wdString &rhs) const { return data.data64 == rhs.data.data64; }
    pattern_t compare(const wdString &real_answer) const;
};

pattern_t wdString::compare(const wdString &real_answer) const {
    pattern_t ret = 0;
    static const uint8_t weights[] = {1, 3, 9, 27, 81};
    pattern_t pos[] = {0, 1, 2, 3, 4};
    // make a copy of data
    char buf1[8], buf2[8];
    *((uint64_t *)buf1) = data.data64;
    *((uint64_t *)buf2) = real_answer.data.data64;
    // pass 1(green)
    int nlen = 5;
    for(int i = 0; i < nlen; i++) {
        if(buf1[i] == buf2[i]) {
            ret += weights[pos[i]] * 2;
            // move the last one to this place
            buf1[i] = buf1[nlen - 1];
            buf2[i] = buf2[nlen - 1];
            buf1[nlen - 1] = buf2[nlen - 1] = '\0';
            pos[i] = pos[nlen - 1];
            nlen--;
            i--;
        }
    }
    // pass 2(yellow)
    for(int i = 0; i < nlen; i++) {
        if(strchr(buf2, buf1[i]) != NULL) {
            ret += weights[pos[i]];
        }
    }
    // remaining is zero
    return ret;
}

ostream &operator<<(ostream &os, const wdString &wd) {
    return os << wd.data.str;
}

struct wdStringCmp {
    bool operator()(const wdString &s1, const wdString &s2) const {
        return strcmp(s1.data.str, s2.data.str) < 0;
    }
};

// deserialize pattern
std::string get_pattern_string(pattern_t pattern)
{
    thread_local char pat[6] = {0};
    pat[5] = '\0';
    for(int i = 0; i < 5; i++) {
        int cur = pattern % 3;
        if(cur == 0) pat[i] = 'n';
        else if(cur == 1) pat[i] = 'p';
        else pat[i] = 'c';
        pattern /= 3;
    }
    return string(pat);
}

pattern_t from_pattern_string(string pattern)
{
    pattern_t ret = 0;
    pattern_t weights[] = {1, 3, 9, 27, 81};
    for(int i = 0; i < 5; i++) {
        if(pattern[i] == 'n') {
            // do nothing
        } else if(pattern[i] == 'p') {
            ret += weights[i];
        } else if(pattern[i] == 'c') {
            ret += 2 * weights[i];
        } else {
            assert(false);
            return 255;
        }
    }
    return ret;
}

void print_string_with_pattern(const wdString &w, pattern_t pat)
{
    cout << "Your guess: ";
    thread_local char buf[64];
    char *p = buf;
    for(int i = 0; i < 5; i++) {
        uint8_t cp = pat % 3;
        if(cp == 0) {
            p += sprintf(p, "\e[40m%c\e[0m ", w.data.str[i]);
        } else if(cp == 1) {
            p += sprintf(p, "\e[43m%c\e[0m ", w.data.str[i]);
        } else {
            p += sprintf(p, "\e[42m%c\e[0m ", w.data.str[i]);
        }
    }
    cout << buf << endl;
}

float calc_entropy(wdString guess, const wdStringSet &word_list)
{
    assert(word_list.size() > 0);
    thread_local int16_t cnt[243] = {0};
    memset(cnt, 0, sizeof(cnt));
    for(const wdString &w: word_list) {
        pattern_t p = guess.compare(w);
        cnt[p]++;
    }
    float exp = 0;
    float total = float(word_list.size());
    for(int i = 0; i < 243; i++) if(cnt[i]) {
        exp += log2f(total / cnt[i]) * cnt[i];
    }
    exp /= total;
    assert(!isnan(exp));
    return exp;
}


wdStringArray possible_answers;
wdStringArray possible_words;
wdStringSet possible_answers_set;

vector<pair<wdString, float>> get_topn(const wdStringSet &valid, int n)
{
    typedef pair<wdString, float> pr;
    static const vector<pr> initial = {
        { wdString("soare"), 5.885 },
        { wdString("roate"), 5.885 },
        { wdString("raise"), 5.878 },
        { wdString("reast"), 5.868 },
        { wdString("raile"), 5.865 },
        { wdString("slate"), 5.856 },
        { wdString("salet"), 5.836 },
        { wdString("crate"), 5.835 },
        { wdString("irate"), 5.833 },
        { wdString("trace"), 5.830 },
    };
    assert(valid.size() > 0);
    vector<pr> ans;
    if(valid.size() > 2000) {
        ans = initial;
    } else {
        ans.reserve(valid.size() + possible_words.size() - possible_answers.size());
        // first all valid strings
        for(const wdString &w: valid) {
            ans.push_back({w, calc_entropy(w, valid)});
        }
        // all words, except for those answers!
        for(const wdString &w: possible_words) if(valid.find(w) == valid.cend()) {
            ans.push_back({w, calc_entropy(w, valid)});
        }
        assert(ans.size() > 0);
        // do stable sort so strings in valid set get to be picked first
        stable_sort(ans.begin(), ans.end(), [&](const pr &a, const pr &b) {
            return a.second > b.second;
        });
    }

    return vector<pr>(ans.begin(), ans.begin() + n);
}

vector<pair<pattern_t, wdStringSet>> get_topn_patterns_and_words(wdString guess, const wdStringSet &valid, int n=20)
{
    assert(n > 0);
    thread_local vector<wdStringSet> stat(243);
    thread_local vector<pattern_t> ident(243);
    vector<pair<pattern_t, wdStringSet>> ret;
    for(int i = 0; i < 243; i++) ident[i] = i;
    for(wdStringSet &s: stat) s.clear();

    for(const wdString &w: valid) {
        stat[guess.compare(w)].insert(w);
    }
    partial_sort(ident.begin(), ident.begin() + n, ident.end(), [&](pattern_t a, pattern_t b) {
        return stat[a].size() > stat[b].size();
    });
    ret.reserve(n);
    for(int i = 0; i < n; i++) if(!stat[ident[i]].empty()) {
        ret.push_back({ ident[i], stat[ident[i]] });
    }
    return ret;
}

// two layer searching is proved to be useless.
wdString do_guess(const wdStringSet &valid, bool output=false) {
    typedef pair<wdString, float> pr;
    vector<pr> ans;

    ans = get_topn(valid, 10);

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

    return ans.front().first;
}

// read all files & init all data structures
void init() {
    std::string buf;
    possible_answers.reserve(2500);
    possible_words.reserve(13000);

    ifstream f_answers("possible_answers.txt");
    while(f_answers >> buf) {
        possible_answers.push_back(buf);
    }
    f_answers.close();

    ifstream f_words("possible_words.txt");
    while(f_words >> buf) {
        possible_words.push_back(buf);
    }
    f_words.close();

    for(const wdString &w: possible_answers) {
        possible_answers_set.insert(w);
    }
}

void benchmark_thread(wdStringArray w_list, vector<int> &distribution, mutex &lk)
{
    wdStringSet word_left;
    vector<int> my_dis(7, 0);
    cout << hex << this_thread::get_id() << dec << " got " << w_list.size() << " jobs" << endl;
    for(size_t i = 0; i < w_list.size(); i++) {
        wdString choice = w_list[i];
        int j;
        word_left = possible_answers_set;
        wdStringArray guesses;
        for(j = 0; j < 10; j++) {
            // a word from word list
            wdString user_input = do_guess(word_left);
            if(user_input.eql(choice)) {
                break;
            }
            pattern_t pat = user_input.compare(choice);
            wdStringSet new_list;
            for(const wdString &w: word_left) {
                if(user_input.compare(w) == pat) {
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
            my_dis[6] += 1;
        } else {
            my_dis[j] += 1;
        }
        if(i > 0 && i % 10 == 0) {
            cout << ".";
            cout.flush();
        }
    }
    {
        lock_guard<mutex> guard(lk);
        for(int i = 0 ; i < 7; i++) {
            distribution[i] += my_dis[i];
        }
    }
}

void benchmark() {
    vector<int> distribution(7, 0);
    int stat = 0;
    vector<thread> threads;
    mutex lk;
    #ifdef DEBUG
    const int THREAD_COUNT = 1;
    #else
    const int THREAD_COUNT = std::thread::hardware_concurrency();
    #endif
    if(THREAD_COUNT > 1) {
        for(int ti = 0; ti < THREAD_COUNT; ti++) {
            int start = possible_answers.size() * ti / THREAD_COUNT;
            int end = possible_answers.size() * (ti + 1) / THREAD_COUNT;
            threads.push_back(thread(benchmark_thread,
                wdStringArray(possible_answers.begin() + start, possible_answers.begin() + end), ref(distribution), ref(lk)));
        }

        for(thread &t: threads) {
            t.join();
        }
    } else {
        benchmark_thread(possible_answers, distribution, lk);
    }
    
    cout << endl;
    cout << "Distribution:\n";
    for(int i = 0; i < 6; i++) {
        cout << (i + 1) << '\t' << distribution[i] << endl;
        stat += distribution[i] * (i + 1);
    }
    cout << ">6\t" << distribution[6] << endl;
    stat += distribution[6] * 7;
    cout << "Average =\t" << (float(stat) / possible_answers.size()) << endl;
}

void cheat()
{
    wdStringSet left = possible_answers_set;
    while(left.size() > 0) {
        string buf;
        wdString user_input("*****");
        pattern_t pat;

        {
            int i = 0;
            cout << left.size() << " possible words left" << endl;
            for(const wdString &w: left) {
                cout << w << '\t';
                if(i++ == 16) break;
            }
            if(left.size() > 16) cout << "... and " << (left.size() - 16) << " more";
            cout << endl;
        }
        do_guess(left, true);
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

        print_string_with_pattern(user_input, pat);

        // filter words
        wdStringSet new_left;
        for(const wdString &w: left) {
            if(user_input.compare(w) == pat) {
                new_left.insert(w);
            }
        }
        left = new_left;
    }
    if(left.size() == 0) {
        cout << "No possibility left." << endl;
    }
}

void testCompare(string s1, string s2, string pat)
{
    pattern_t actual = wdString(s1).compare(wdString(s2));
    if(actual != from_pattern_string(pat)) {
        cerr << "Expecting " << pat << endl;
        cerr << "Actual " << get_pattern_string(actual) << endl;
        assert(false);
    }
}

int main(int argc, char *const argv[]) {
    int option;
    void (*action)() = cheat;

    while((option = getopt(argc, argv, "hbc")) != -1) {
        switch(option) {
            case 'h':
                cout << "Usage: " << argv[0] << " <options>" << endl;
                cout << usage_prompt << endl;
                exit(0);
            case 'b':
                // do benchmark
                action = benchmark;
                break;
            case 'c':
            default:
                // cheat
                break;
        }
    }

    init();

    testCompare("taint", "about", "npnnc");
    testCompare("slate", "gaint", "nnppn");

    action();
    return 0;
}
