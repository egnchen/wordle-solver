#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
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

#include <gperftools/profiler.h>

using namespace std;

// this is the wordle game solver inspired by 3b1b
// watch this: https://www.youtube.com/watch?v=v68zYyaEmEA
typedef uint8_t pattern_t;
struct wdString;
struct wdStringCmp;
using wdStringArray = vector<wdString>;
using wdStringSet = set<wdString, wdStringCmp>;

struct wdString {
public:
    char data[8];
    uint32_t mask;
    wdString(string w) {
        assert(w.length() == 5);
        strncpy(data, w.c_str(), 6);
        mask = 0;
        for(int i = 0; i < 5; i++) mask |= 1 << (data[i] - 'a');
    }
    bool eql(const wdString &rhs) const { return strcmp(data, rhs.data) == 0; }
    pattern_t compare(const wdString &real_answer) const;
};

inline pattern_t wdString::compare(const wdString &real_answer) const {
    pattern_t ret = 0;
    for(int i = 0; i < 5; i++) {
        ret *= 3;
        if(data[i] == real_answer.data[i]) {
            // nothing
        } else if(real_answer.mask & (1 << data[i])) {
            ret += 1;
        } else {
            ret += 2;
        }
    }
    return ret;
}

ostream &operator<<(ostream &os, const wdString &wd) {
    return os << wd.data;
}

struct wdStringCmp {
    bool operator()(const wdString &s1, const wdString &s2) const {
        return strcmp(s1.data, s2.data) < 0;
    }
};

// deserialize pattern
std::string get_pattern_string(pattern_t pattern)
{
    thread_local char pat[6] = {0};
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

#define USE_TWO_LAYER_GUESS
// do guess with two-layer search
wdString do_guess(const wdStringSet &valid, bool output=false) {
    typedef pair<wdString, float> pr;
    static const vector<pr> initial = {
        { wdString("slate"), 10.304 },
        { wdString("trace"), 10.272 },
        { wdString("crate"), 10.267 },
        { wdString("least"), 10.248 },
        { wdString("stale"), 10.241 },
        { wdString("stare"), 10.212 },
        { wdString("grate"), 10.186 },
        { wdString("react"), 10.180 },
        { wdString("saner"), 10.179 },
        { wdString("snare"), 10.133 },
    };
    
    vector<pr> ans;

    #ifndef USE_TWO_LAYER_GUESS

    return get_topn(valid, 1)[0].first;

    #else

    if(valid.size() > 2000) {
        // use initial guess directly
        ans = initial;
    } else {
        // do two-layer guess
        ans = get_topn(valid, 10);
        for(auto &pr: ans) {
            auto stat = get_topn_patterns_and_words(pr.first, valid, 50);
            for(const auto &p: stat) {
                const wdStringSet &new_valid = p.second;
                auto layer2_choice = get_topn(new_valid, 1);
                pr.second += float(new_valid.size()) * layer2_choice.front().second / valid.size();
            }
        }

        // sort again
        sort(ans.begin(), ans.end(), [](const pair<wdString, float> &a, const pair<wdString, float> &b) {
            return a.second > b.second;
        });
    
    }
        
    #endif

    if(output) {
        cout << "Top 10 recommended guesses:" << endl;
        for(int i = 0; i < 10; i++) {
            const auto &p = ans[i];
            if(possible_answers_set.find(p.first) != possible_answers_set.end()) {
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
    const int THREAD_COUNT = std::thread::hardware_concurrency();
    // const int THREAD_COUNT = 1;
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
        wdString user_input("");
        pattern_t pat;

        cout << left.size() << " possible words left" << endl;
        for(wdString w: left) cout << w << '\t';
        cout << endl;
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

        // filter words
        wdStringSet new_left;
        for(wdString w: left) {
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

int main(void) {
    init();
    ProfilerStart("test.prof");
    // maybe you want to do benchmark
    benchmark();
    ProfilerStop();
    // or maybe you just want to cheat
    // cheat();
    return 0;
}
