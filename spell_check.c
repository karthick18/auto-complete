#include "auto_complete.h"
#include <ctype.h>

#undef MIN
#define MIN(x,y) ( (x) < (y) ? (x) : (y) )
#define __MIN(a,b,c) MIN(a, MIN(b, c))

#undef MAX
#define MAX(x, y) ( (x) > (y) ? (x) : (y) )

#define MAX_CHARS ((int)('z' - 'a') + 1)

struct similar_sound_map
{
    const char *alphabets;
} similar_sound_map[] = {
    {"aeiou"},
    {"ck"},
    {"gj"},
    {"sz"},
    {"td"},
    {NULL},
};

/*
 * A spell check specific completion cache.
 */
struct spell_check_cache
{
#define SPELL_CHECK_CACHE_AGE_MAX (64)
    struct list_head *completion;
    char c; /*char*/
    int age; /*age of the cache*/
};

/*
 * Right now its a one to one cache for each of the alphabets.
 */
static struct spell_check_cache spell_check_cache[MAX_CHARS];

void spell_check_cache_init(void)
{
    register int i;
    for(i = 0; i < MAX_CHARS; ++i)
    {
        spell_check_cache[i].completion = NULL;
        spell_check_cache[i].c = (char)('a' + i);
        spell_check_cache[i].age = SPELL_CHECK_CACHE_AGE_MAX; 
    }
}

static void list_destroy_cb(struct word_list *wl)
{
    if(wl->word) free(wl->word);
    free(wl);
}

static __inline__ void spell_check_cache_add(char c, struct list_head *completion)
{
    struct list_head *last_completion;
    int age = SPELL_CHECK_CACHE_AGE_MAX;
    if(completion) age <<= 1;
    if(isupper(c)) c = tolower(c);
    c -= 'a';
    assert(c < MAX_CHARS);
    assert(spell_check_cache[(int)c].c == c + 'a');
    last_completion = spell_check_cache[(int)c].completion;
    spell_check_cache[(int)c].completion = completion;
    spell_check_cache[(int)c].age = age;
    if(last_completion)
        list_destroy(last_completion, struct word_list, list, list_destroy_cb);
}

static __inline__ struct list_head *spell_check_cache_find(char c)
{
    struct list_head *entry;
    if(isupper(c)) c = tolower(c);
    c -= 'a';
    assert(c < MAX_CHARS);
    assert(spell_check_cache[(int)c].c == c + 'a');
    if( (entry = spell_check_cache[(int)c].completion) )
        spell_check_cache[(int)c].age = SPELL_CHECK_CACHE_AGE_MAX << 1; /*for the cache age scan*/
    return entry;
}

void spell_check_cache_del(char c)
{
    spell_check_cache_add(c, NULL); /*invalidate the cache entry*/
}

static void spell_check_cache_age(void)
{
    register int i;
    for(i = 0; i < MAX_CHARS; ++i)
    {
        spell_check_cache[i].age >>= 1;
        if(!spell_check_cache[i].age && spell_check_cache[i].completion)
            spell_check_cache_add((char)('a' + i), NULL); /*invalidate the cache entry*/
    }
}

static char *completion_map_get(char c)
{
    register int i;
    char *wordset = NULL;
    for(i = 0; (wordset = (char*)similar_sound_map[i].alphabets); ++i)
        if(strchr(wordset, c))
            return strdup(wordset);

    wordset = malloc(2);
    assert(wordset);
    wordset[0] = tolower(c);
    wordset[1] = 0;
    return wordset;
}

/*
 * Check if the auto completion word is already a super/sub-set of the existing word in the wordlist.
 * Take 70% of the src/dst string for a subset match.
 */
static int is_contained_word(struct spell_check_word *wordlist, int num_words, const char *word)
{
#define MIN_LEN (0x7)
    register int i;
    int srclen = strlen(word);
    if(srclen < MIN_LEN)
        return 0;
    for(i = 0; i < num_words; ++i)
    {
        int len = strlen(wordlist[i].word);
        int cmp = 0;
        /*
         * Atleast min. bytes for a plural/extension match.
         */
        if(len < MIN_LEN) continue;
        if(len < srclen)
        {
            len = MAX(1, len-2);
            cmp = strncmp(word, wordlist[i].word, len);
        }
        else
        {
            len = MAX(1, srclen - 2);
            cmp = strncmp(wordlist[i].word, word, len);
        }
        if(!cmp)
        {
            do_log("Skipping word [%s] coz of [%s]\n", word, wordlist[i].word);
            return 1;
        }
    }
    return 0;
#undef MIN_LEN
}

/*
 * We take 25% of the length of the string starting from the beginning and fetch the auto completion list
 * for each of the characters 
 */

int compute_levenshtein(const char *s, const char *d)
{
    int *distance_matrix = NULL;
    int min_distance = 0;
    register int i, j;
    int n = strlen(s);
    int m = strlen(d);
    if(!n) return m;
    if(!m) return n;

    distance_matrix = calloc((n+1) * (m+1), sizeof(*distance_matrix));
    assert(distance_matrix);
    for(i = 0; i <= n; ++i)
        distance_matrix[i*(m+1)] = i;

    for(j = 0; j <= m; ++j)
        distance_matrix[j] = j;

    for(i = 1; i <= n; ++i)
    {
        char s1 = s[i-1];
        for(j = 1; j <= m ; ++j)
        {
            char d1 = d[j-1];
            int cost = 0;
            if(s1 != d1) cost = 1;
            distance_matrix[i*(m+1) + j] = 
                __MIN(1 + distance_matrix[(i-1)*(m+1) + j],
                      1 + distance_matrix[i*(m+1) + (j-1)],
                      cost + distance_matrix[(i-1)*(m+1) + (j-1)]);
        }
    }
    min_distance = distance_matrix[n*(m+1) + m];
    free(distance_matrix);
    return min_distance;
}

static int levenshtein_cmp(const void *a, const void *b)
{
    return ((struct spell_check_word*)a)->weight - ((struct spell_check_word*)b)->weight;
}

int spell_check(struct auto_complete_node *auto_complete, const char *w, 
                struct spell_check_word *wordlist, int *num_words)
{
    int l,al_match;
    int scantill = 0;
    int reset = 0, realloc_flag = 0;
    register int i;
    int count = 0;
    int word_limit = 0;
    char insert_map[0xff+1] = {0};

    if(!auto_complete || !w ) return -1;

    if(num_words && !(word_limit = *num_words))
        return 0;

    if(num_words)
        *num_words = 0;

    if(!w || (l = strlen(w)) < 3)
    {
        do_log("[%s] too short for spell check as min. length is [%d]. Skipping correction\n", w, l);
        return 0;
    }
    
    if(!wordlist)
    {
        word_limit = (~0U >> 1); /*infinite*/
        realloc_flag = 1;
    }

    al_match = l*6/10; /* for auto-complete matched hits, take 60% of the string len to match*/
    scantill = MAX(2, l/4) ; /*take 1/4th of the string to compute levenshtein*/
    for(i = 0; i < scantill; ++i)
    {
        struct list *iter;
        struct list_head *completion = NULL;
        char c = 0;
        char *wordset = completion_map_get(w[i]); /*get the completion map for a char*/
        char *s = wordset;
        assert(wordset != NULL);
        while((c=*s++))
        {
            if(!(completion = spell_check_cache_find(c)))
            {
                char word[2] = {0};
                completion = calloc(1, sizeof(*completion));
                assert(completion != NULL);
                list_init(completion);
                word[0] = c;
                word[1] = 0;
                complete_word(auto_complete, word, completion);
                /*
                 * add the completion history to the cache.
                 */
                spell_check_cache_add(c, completion);
            }
            if(insert_map[(int)c])
                continue;
            ++insert_map[(int)c];
            for(iter = completion->head; iter; iter = iter->next)
            {
                struct word_list *wl = LIST_ENTRY(iter, struct word_list, list);
                char *acw = wl->word;
                int val;
                l = MIN(count, word_limit);
                if(wordlist && is_contained_word(wordlist, l, acw))
                    continue;
                val = compute_levenshtein(w, acw); /*compute the levenshtein distance*/
                if(!val)  /* if direct hit or match then break */
                {
                    do_log("[%s] already spelled correctly\n", w);
                    reset = 1;
                    goto out_age;
                }
                if(val <= scantill 
                    /*|| 
                   (val = 2,
                   al_match > 6 && !strncmp(w, acw, al_match)
                   )*/ 
                   ) /*if its within limit or matches percent. of the string, add*/
                {
                    if(count < word_limit)
                    {
                        if(realloc_flag)
                        {
                            if(!(count & 7))
                            {
                                wordlist = realloc(wordlist, (count+8)*sizeof(*wordlist));
                                assert(wordlist != NULL);
                                memset(wordlist + count, 0, sizeof(*wordlist) * 8);
                            }
                        }
                        wordlist[count].word = acw;
                        wordlist[count].weight = val;
                    }
                    ++count;
                }
            }
        }
    }

    if(num_words)
        *num_words = count;

    l = MIN(count, word_limit);
    
    qsort(wordlist, l, sizeof(*wordlist), levenshtein_cmp);

    if(!realloc_flag)
        goto out_age;

    do_log("[%d] spell check suggestions found for word [%s]. Displaying [%d]\n", count, w, l);

    for(i = 0; i < l; ++i)
    {
        if(wordlist[i].weight > 0)
            do_log("Spell check suggestion [%d] - [%s]\n", i+1, wordlist[i].word);
    }

    if(realloc_flag)
        free(wordlist);
    
    out_age:
    spell_check_cache_age();

    return 0;
}
        
