import itertools
import pickle
import pandas as pd
from IPython import embed
from IPython.terminal.embed import InteractiveShellEmbed

ishowta_f = open("data/ishowta.pickle", "rb")
df_ishowta = pd.read_pickle(ishowta_f, compression=None)

vowel_to_wordlist = {}

for line in df_ishowta.values:
    word, freq, _, _, vowel = line
    if not vowel in vowel_to_wordlist:
        vowel_to_wordlist[vowel] = []
    vowel_to_wordlist[vowel].append(word)

for k in vowel_to_wordlist:
    vowel_to_wordlist[k].sort(reverse=True)
    vowel_to_wordlist[k] = vowel_to_wordlist[k]


def v_to_wl(vowel):
    if vowel in vowel_to_wordlist:
        return vowel_to_wordlist[vowel]
    return [""]


df_gms = {}
sentence_to_freq = {}

N = 7

for i in range(1, N + 1):
    df_gms[i] = pd.read_csv(f'data/nwc2010-ngrams/word/over999/{i}gms/{i}gm-0000.xz', sep='\t', header=None)
    # df_gms[i] = pd.read_csv(f'data/nwc2010-ngrams/word/over999/{i}gms/{i}gm-0000', sep='\t', header=None)

    sentence_to_freq[i] = {k: v for k, v in zip(df_gms[i][0], df_gms[i][1])}
print("READ CSV")

def pakupaku(in_sentence_list):
    W = len(in_sentence_list)

    can_words = [v_to_wl(vowel) for vowel in in_sentence_list] # e.g. [["え", "ぜ"]]

    all_candidates = [ [(sentence_to_freq[1][s] if s in sentence_to_freq[1] else 0, s) for s in words] for words in can_words]
    for i in range(W):
        all_candidates[i].sort(reverse=True)
    
    out_sentence_list = ["" for i in range(W)]

    for i in range(W):
        print(out_sentence_list)
        k = 0
        if out_sentence_list[i] == "":
            out_sentence_list[i] = can_words[i][0]
            candidates = [(sentence_to_freq[1][s] if s in sentence_to_freq[1] else 0, s) for s in can_words[i]]
            k = 1
        else:
            candidate_sentence = ""
            for j in range(i, W):
                if out_sentence_list[j] == "":
                    break
                k += 1
                candidate_sentence += out_sentence_list[j] + " "
            candidate_sentence = candidate_sentence[-1]
            candidates = [(0, candidate_sentence)]

        for n in range(2, N + 1):
            if i + n - 1 >= W:
                break
            p = itertools.product(candidates, all_candidates[i + n - 1])

            new_candidates = []

            for t in p:
                search_word = ""
                for x in t:
                    search_word += x[1] + " "
                search_word = search_word[:-1]

                if not search_word in sentence_to_freq[n]:
                    continue

                new_candidates.append(
                    (sentence_to_freq[n][search_word], search_word))
            
            new_candidates.sort(reverse=True)
            print(i, n, new_candidates[:5])

            if len(new_candidates) > 0:
                out_sentence_list[i:i+n] = new_candidates[0][1].split(" ")


            candidates = new_candidates

    print("RESULT:", "".join(out_sentence_list))
    print()


in_sentence_list = ["oo", "a", "ii", "eni", "eu", "e"]
pakupaku(in_sentence_list)

ipshell = InteractiveShellEmbed()

print('Usage: pakupaku(["oniia", "eai"])')
print()
ipshell()
