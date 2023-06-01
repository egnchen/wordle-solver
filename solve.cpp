#include "wordle.h"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace std;

const char *usage_prompt = "Options:\n\
\t-b: Run benchmark\n\
\t-c: Run interactive cheating(default)\n\
\t-h: Show this message\n\
\n\
Help on interactive cheating:\n\
c represent correct\n\
n represents null/non-existent\n\
p represents exist but position not right\n\
For example, \"ppcnn\" means the first two have wrong positions, the middle one is correct and the rest aren't find anywhere in this word.\
";

// two layer searching is proved to be useless.
wdString do_guess(const wdStringSet &valid, bool output = false) {
    typedef pair<wdString, float> pr;
    vector<pr> ans;

    ans = get_topn(valid, 10);

    if (output) {
        cout << "Top 10 recommended guesses:" << endl;
        for (int i = 0; i < 10; i++) {
            const auto &p = ans[i];
            if (valid.find(p.first) != valid.end()) {
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
    while (f_answers >> buf) {
        possible_answers.push_back(buf);
    }
    f_answers.close();

    ifstream f_words("possible_words.txt");
    while (f_words >> buf) {
        possible_words.push_back(buf);
    }
    f_words.close();

    for (const wdString &w : possible_answers) {
        possible_answers_set.insert(w);
    }
}

void benchmark_thread(wdStringArray w_list, vector<int> &distribution,
                                            mutex &lk) {
    wdStringSet word_left;
    vector<int> my_dis(7, 0);
    cout << hex << this_thread::get_id() << dec << " got " << w_list.size()
             << " jobs" << endl;
    for (size_t i = 0; i < w_list.size(); i++) {
        wdString choice = w_list[i];
        int j;
        word_left = possible_answers_set;
        wdStringArray guesses;
        for (j = 0; j < 10; j++) {
            // a word from word list
            wdString user_input = do_guess(word_left);
            if (user_input.eql(choice)) {
                break;
            }
            pattern_t pat = user_input.compare(choice);
            wdStringSet new_list;
            for (const wdString &w : word_left) {
                if (user_input.compare(w) == pat) {
                    new_list.insert(w);
                }
            }
            word_left = new_list;
            guesses.push_back(user_input);
        }
        // too long
        if (j > 6) {
            cout << "Choices:\t";
            for (const wdString &w : guesses)
                cout << w << '\t';
            cout << endl;
            my_dis[6] += 1;
        } else {
            my_dis[j] += 1;
        }
        if (i > 0 && i % 10 == 0) {
            cout << ".";
            cout.flush();
        }
    }
    {
        lock_guard<mutex> guard(lk);
        for (int i = 0; i < 7; i++) {
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
    if (THREAD_COUNT > 1) {
        for (int ti = 0; ti < THREAD_COUNT; ti++) {
            int start = possible_answers.size() * ti / THREAD_COUNT;
            int end = possible_answers.size() * (ti + 1) / THREAD_COUNT;
            threads.push_back(thread(
                benchmark_thread, wdStringArray(possible_answers.begin() + start, possible_answers.begin() + end),
                ref(distribution), ref(lk)));
        }

        for (thread &t : threads) {
            t.join();
        }
    } else {
        benchmark_thread(possible_answers, distribution, lk);
    }

    cout << endl;
    cout << "Distribution:\n";
    for (int i = 0; i < 6; i++) {
        cout << (i + 1) << '\t' << distribution[i] << endl;
        stat += distribution[i] * (i + 1);
    }
    cout << ">6\t" << distribution[6] << endl;
    stat += distribution[6] * 7;
    cout << "Average =\t" << (float(stat) / possible_answers.size()) << endl;
}

void print_word_list(const wdStringSet &word_list, int nr_print = 16) {
    int i = 0;
    for (const wdString &w : word_list) {
        cout << w << '\t';
        if (i++ == 16)
            break;
    }
    if (word_list.size() > nr_print) {
        cout << "... and " << (word_list.size() - nr_print) << " more";
    }
    cout << endl;
}

void cheat() {
    wdStringSet left = possible_answers_set;
    while (left.size() > 0) {
        string buf;
        wdString user_input("*****");
        pattern_t pat;
        print_word_list(left); 
        do_guess(left, true);
        do {
            cout << "Your input: ";
            cin >> buf;
        } while (buf.size() != 5);
        user_input = wdString(buf);

        do {
            cout << "Returned pattern: ";
            cin >> buf;
            pat = from_pattern_string(buf);
        } while (pat == 255);

        print_string_with_pattern(user_input, pat);

        // filter words
        wdStringSet new_left;
        for (const wdString &w : left) {
            if (user_input.compare(w) == pat) {
                new_left.insert(w);
            }
        }
        left = new_left;
    }
    if (left.size() == 0) {
        cout << "No possibility left." << endl;
    }
}

void test_compare(string s1, string s2, string pat) {
    pattern_t actual = wdString(s1).compare(wdString(s2));
    if (actual != from_pattern_string(pat)) {
        cerr << "Expecting " << pat << endl;
        cerr << "Actual " << get_pattern_string(actual) << endl;
        assert(false);
    }
}

int main(int argc, char *const argv[]) {
    int option;
    void (*action)() = cheat;

    while ((option = getopt(argc, argv, "hbc")) != -1) {
        switch (option) {
        case 'b':
            // do benchmark
            action = benchmark;
            break;
        case 'c':
            // check
            break;
        case 'h':
        default:
            cout << "Usage: " << argv[0] << " <options>" << endl;
            cout << usage_prompt << endl;
            exit(0);
        }
    }

    init();

    test_compare("taint", "about", "npnnc");
    test_compare("slate", "gaint", "nnppn");
    test_compare("dwell", "elegy", "nncpn");

    action();
    return 0;
}
