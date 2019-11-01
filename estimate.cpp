#include <vector>
#include <array>
#include <fstream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <numeric>
#include <iostream>

#include <boost/algorithm/string.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

#include "library/pprint/include/pprint.hpp"

#define DEBUG

#define rep(i, n) for (int i = 0; i < n; ++i)
using namespace std;
using boost::split;
using boost::algorithm::join;
using csv = vector<vector<string>>;
pprint::PrettyPrinter printer;

csv readCsv(string filename, char sep = ',') {
    csv result;
    ifstream stream(filename);
    string line;
    while (getline(stream, line)) {
        vector<string> csv_line;
        istringstream s(line);
        string field;
        while (getline(s, field, sep)) {
            csv_line.push_back(field);
        }
        result.push_back(csv_line);
    }
    return result;
}

// fast
vector<array<string, 2>> readNgramCsv(string filename, char sep = ',') {
    vector<array<string, 2>> result;
    ifstream stream(filename);
    string line;
    while (getline(stream, line)) {
        auto pos = line.find_last_of("\t");
        result.push_back({line.substr(0, pos), line.substr(pos + 1)});
    }
    return result;
}

int main() {
    printer.compact(true);

    printer.print("setup (1/2)");

    // read words
    csv raw_word_to_vowel = readCsv("data/words.csv");

    // make vowel <-> wordlist map
    unordered_map<string, vector<string>> vowel_to_wordlist;
    unordered_map<string, string> word_to_vowel;
    for (auto &line : raw_word_to_vowel) {
        string &vowel = line[5];
        string &word = line[1];
        word_to_vowel[word] = vowel;
        if (vowel_to_wordlist.find(vowel) == vowel_to_wordlist.end()) {
            vowel_to_wordlist[vowel] = {word};
        } else {
            vowel_to_wordlist[vowel].push_back(word);
        }
    }

    printer.print("setup (2/2)");

    // read ngram-list
    // key: n-1個の単語+n個目の単語の母音
    // value: [(n個目の単語, n個目の単語の頻度/sum)]
    int N = 4;
    typedef unordered_map<string, vector<pair<string, float>>> VowelNgramDict;
    vector<VowelNgramDict> ngram_list(N);
    // 遅いのでシリアライズする
    string ngram_filename = "data/ngram.dat";
    if (ifstream(ngram_filename).is_open()) {
        ifstream ifs("data/ngram.dat");
        boost::archive::binary_iarchive ia(ifs);
        ia >> ngram_list;
    } else {
        rep(n, N) {
            printer.print("read", n + 1, "gram");
            vector<array<string, 2>> raw_ngram = readNgramCsv("data/nwc2010-ngrams/word/over999/" + to_string(n + 1) + "gms/" + to_string(n + 1) + "gm-0000", '\t');
            ngram_list[n] = [&]() {
                VowelNgramDict ngram;
                ngram.reserve(raw_ngram.size());
                // make ngram map
                for (auto &line : raw_ngram) {
                    size_t sp_pos = line[0].find_last_of(" ");
                    string last_word = n == 0 ? line[0] : line[0].substr(sp_pos + 1);
                    string last_vowel = word_to_vowel[last_word];
                    string ngram_key = n == 0 ? last_vowel : line[0].substr(0, sp_pos) + " " + last_vowel;
                    auto ngram_value = make_pair(last_word, stof(line[1]));
                    if (ngram.find(ngram_key) == ngram.end()) {
                        ngram[ngram_key] = {ngram_value};
                    } else {
                        ngram[ngram_key].push_back(ngram_value);
                    }
                }
                // calc sum and rewrite
                for (auto &[key, word_list] : ngram) {
                    unsigned long long sum = accumulate(word_list.begin(), word_list.end(), 0, [](unsigned long long pre, pair<string, float> &w) {
                        return pre + w.second;
                    });
                    for (auto &w : word_list) {
                        w.second /= sum;
                    }
                }
                return ngram;
            }();
        }

        ofstream ofs("data/ngram.dat");
        boost::archive::binary_oarchive oa(ofs);
        oa << ngram_list;
    }

    printer.print("setup fin");

    // search
    unsigned long BEAM_LENGTH = 10000;
    auto pakupaku = [&](vector<string> in_vowel_list) {
        int W = in_vowel_list.size();
        vector<pair<vector<string>, float>> candidates = {pair<vector<string>, float>({}, 1.0)};  // i文字目におけるそれっぽい候補列
        vector<pair<vector<string>, float>> next_candidates;

        rep(n, W) {
            printer.print(n + 1, "文字目");
            string vowel = in_vowel_list[n];
            auto wl = vowel_to_wordlist.find(vowel);
            if (wl == vowel_to_wordlist.end())
                return string("");  // 母音に当てはまる単語が無かったので諦める
            // 全ての候補に対して次の単語の候補を探してnext_candidatesに入れる
            for (auto &candidate : candidates) {
                size_t n_num = n + 1 > N ? N : n + 1;  // 検索するNgramのN
                float c_nlength = 1.0;                 // 評価値のn_numによる減衰値
                vector<pair<string, float>> word_list = [&]() {
                    while (true) {
                        auto search_words = vector<string>(candidate.first.begin() + ((n + 1) - n_num), candidate.first.end());
                        string key;
                        for (auto &word : search_words) {
                            key += word + " ";
                        }
                        key += vowel;
                        auto &word_list = ngram_list[n_num - 1][key];
                        if (word_list.empty()) {
                            // 現在最長のn_numで無ければn_numを減らしていく
                            n_num -= 1;
                            // 減らす分評価値も下げる
                            c_nlength *= 0.1;
                            continue;
                        } else {
                            return word_list;
                        }
                    }
                }();
                // next_candidatesに追加
                for (auto &word : word_list) {
                    auto next_cand_words = candidate.first;
                    next_cand_words.push_back(word.first);
                    float eval_value = candidate.second * word.second * c_nlength;  // n-1文字目までの評価値×n文字目の評価値×減衰値
                    next_candidates.push_back(make_pair(move(next_cand_words), eval_value));
                }
            }
            candidates = move(next_candidates);
            next_candidates = {};

#ifdef DEBUG
            printer.print("length:", candidates.size());
#endif
            // ビーム幅まで減らす
            std::sort(candidates.begin(), candidates.end(), [](auto &a, auto &b) { return a.second > b.second; });
            candidates.resize(min(candidates.size(), BEAM_LENGTH));
#ifdef DEBUG
            printer.print(vector(candidates.begin(), candidates.begin() + 10));
#endif
        }

        // 最も良さそうなのだけ返す
        std::sort(candidates.begin(), candidates.end(), [](auto &a, auto &b) { return a.second > b.second; });
        return join(candidates[0].first, "");
    };

    while (true) {
        string input;
        vector<string> in_sentence_list;  // = {"oo", "a", "ii", "eni", "eu", "e"}
        printer.print_inline("Input any vowel sentence: ");
        getline(cin, input);
        split(in_sentence_list, input, boost::is_any_of(" "));

        auto result = pakupaku(in_sentence_list);
        printer.print("Result: ", result);
    }
}
