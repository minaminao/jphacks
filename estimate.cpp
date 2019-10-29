#include <vector>
#include <array>
#include <fstream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <numeric>
#include <iostream>

#define rep(i, n) for (int i = 0; i < n; ++i)

using namespace std;

typedef vector<vector<string>> csv;

void debug(vector<string> v)
{
    for (auto &x : v)
    {
        std::cout << x << ",";
    }
    std::cout << endl;
}

void debug(vector<pair<vector<string>, float>> v)
{
    int cnt = 0;
    for (auto &x : v)
    {
        cnt++;
        if (cnt > 10)
            return;
        for (auto &e : x.first)
        {
            std::cout << e << ",";
        }
        std::cout << x.second << endl;
    }
    std::cout << endl;
}

void debug(vector<pair<string, float>> v)
{
    int cnt = 0;
    for (auto &x : v)
    {
        cnt++;
        if (cnt > 10)
            return;
        std::cout << x.first << x.second << endl;
    }
    std::cout << endl;
}

csv readCsv(string filename, char sep = ',')
{
    csv result;
    ifstream stream(filename);
    string line;
    while (getline(stream, line))
    {
        vector<string> csv_line;
        istringstream s(line);
        string field;
        while (getline(s, field, sep))
        {
            csv_line.push_back(field);
        }
        result.push_back(csv_line);
    }
    return result;
}

vector<array<string, 2>> readNgramCsv(string filename, char sep = ',')
{
    vector<array<string, 2>> result;
    ifstream stream(filename);
    string line;
    while (getline(stream, line))
    {
        auto pos = line.find_last_of("\t");
        result.push_back({line.substr(0, pos), line.substr(pos + 1)});
    }
    return result;
}

template <typename Out>
void split(const std::string &s, char delim, Out result)
{
    std::istringstream iss(s);
    std::string item;
    while (std::getline(iss, item, delim))
    {
        *result++ = item;
    }
}

std::vector<std::string> split(const std::string &s, char delim)
{
    std::vector<std::string> elems;
    split(s, delim, std::back_inserter(elems));
    return elems;
}

int main()
{
    // read words
    std::cout << "start" << endl;
    csv raw_words = readCsv("data/words.csv");
    std::cout << "read words fin" << endl;
    unordered_map<string, vector<string>> vowel_to_wordlist;
    unordered_map<string, string> word_to_vowel;

    for (auto &line : raw_words)
    {
        string &vowel = line[5];
        string &word = line[1];
        word_to_vowel[word] = vowel;
        if (vowel_to_wordlist.find(vowel) == vowel_to_wordlist.end())
            vowel_to_wordlist[vowel] = {};
        vowel_to_wordlist[vowel].push_back(word);
    }

    std::cout << "st" << endl;
    // read ngram-list
    int N = 4;
    typedef unordered_map<string, vector<pair<string, float>>> VowelNgramDict;
    vector<VowelNgramDict> ngram_list(N);

    rep(n, N)
    {
        std::cout << n << endl;
        vector<array<string, 2>> raw_ngram = readNgramCsv("data/nwc2010-ngrams/word/over999/" + to_string(n + 1) + "gms/" + to_string(n + 1) + "gm-0000", '\t');
        ngram_list[n] = [&]() {
            VowelNgramDict ngram;
            ngram.reserve(raw_ngram.size());
            std::cout << "1/2" << endl;
            for (auto &line : raw_ngram)
            {
                size_t sp = line[0].find_last_of(" ");
                string last_word = n == 0 ? line[0] : line[0].substr(sp + 1);
                string last_vowel = word_to_vowel[last_word];
                string ngram_key = n == 0 ? last_vowel : line[0].substr(0, sp) + " " + last_vowel;
                auto ngram_value = make_pair(last_word, stof(line[1]));
                if (ngram.find(ngram_key) == ngram.end())
                {
                    ngram[ngram_key] = {ngram_value};
                }
                else
                {
                    ngram[ngram_key].push_back(ngram_value);
                }
            }
            std::cout << "2/2" << endl;
            for (auto &[key, word_list] : ngram)
            {
                unsigned long long sum = accumulate(word_list.begin(), word_list.end(), 0, [](unsigned long long pre, pair<string, float> &w) {
                    return pre + w.second;
                });
                for (auto &w : word_list)
                {
                    w.second /= sum;
                }
            }
            return ngram;
        }();
    }

    std::cout << "read fin" << endl;

    // search
    int WORD_MAX = 1000;
    auto pakupaku = [&](vector<string> in_vowel_list) {
        int W = in_vowel_list.size();
        vector<pair<vector<string>, float>> candidates = {pair<vector<string>, float>({}, 1.0)};
        vector<pair<vector<string>, float>> next_candidates;

        int min_cand_index = 0;
        float min_cand_value = -1;
        int cand_cnt = 0;

        rep(i, W)
        {
            std::cout << i + 1 << "文字目" << endl;
            string vowel = in_vowel_list[i];
            auto wl = vowel_to_wordlist.find(vowel);
            if (wl == vowel_to_wordlist.end())
                return vector<string>(); // 例外
            for (auto &candidate : candidates)
            {
                // ngramを超えたらsearch_wordsの右端Nこだけで検索
                size_t n_num = i + 1 > N ? N : i + 1;
                float zoom = 1.0;
                vector<pair<string, float>> word_list = [&](){
                    while(true){
                        auto search_words = vector<string>(candidate.first.begin() + ((i + 1) - n_num), candidate.first.end());
                        string key;
                        for (auto &word : search_words)
                        {
                            key += word + " ";
                        }
                        key += vowel;
                        auto& word_list = ngram_list[n_num - 1][key];
                        if(word_list.empty()){
                            n_num -= 1;
                            zoom *= 0.1;
                            continue;
                        }else{
                            return word_list;
                        }
                    }
                }();
                for (auto &word : word_list)
                {
                    auto next_cand_words = candidate.first;
                    next_cand_words.push_back(word.first);
                    if(candidate.second * word.second * zoom < 0){
                        cout << candidate.second << "," << word.second << "," << zoom << endl;
                        exit(1);
                    }
                    next_candidates.push_back(make_pair(move(next_cand_words), candidate.second * word.second * zoom));
                }
            }
            candidates = move(next_candidates);
            next_candidates = {};

            std::cout << "length:" << candidates.size() << endl;
            sort(candidates.begin(), candidates.end(), [](auto& a, auto& b) {
                return a.second > b.second;
            });
            candidates.resize(min(candidates.size(), 10000ul));
            debug(candidates);
        }

        std::cout << "search fin" << endl;

        sort(candidates.begin(), candidates.end(), [](auto& a, auto& b) {
            return a.second > b.second;
        });
        std::cout << candidates[0].second << endl;
        return candidates[0].first;
    };

    //auto in_sentence_list = {"oo", "a", "ii", "eni", "eu", "e"}
    while (true)
    {
        string input;
        vector<string> in_sentence_list; // = {"aia", "a", "aui", "au"};
        getline(cin, input);
        std::cout << input << endl;
        char c = ' ';
        in_sentence_list = split(input, c);
        debug(in_sentence_list);
        auto result = pakupaku(in_sentence_list);
        debug(result);
    }
}
