#include "auto_complete.h"
#include <ctype.h>

#undef min
#define min(a, b) ( (a) < (b) ? (a) : (b) )
#define MIN_SPELL_CHECK_SUGGESTIONS (10)

static struct auto_complete_node auto_complete;

static void word_list_destroy_cb(struct word_list *wl)
{
    if(!wl) return;
    if(wl->word) free(wl->word);
    free(wl);
}

static void word_list_display_cb(struct word_list *wl, int index)
{
    if(!wl || !wl->word) return;
    if(index && !(index & 3))
        printf("\n");
    printf("%s ", wl->word);
}

/*
 * We take 30% of the length of the string starting from the beginning and fetch the auto completion list
 * for each of the characters 
 */
struct levenshtein_word
{
    char *word;
    int val; /*levenshtein value*/
};

static int levenshtein_cmp(const void *a, const void *b)
{
    return ((struct levenshtein_word*)a)->val - ((struct levenshtein_word*)b)->val;
}

static int spell_check(struct auto_complete_node *auto_complete, const char *w)
{
    struct list_head *completion_array = NULL;
    struct levenshtein_word *levenshtein_array = NULL; /*a sorted list based on levenshtein indexes*/
    int l,al_match;
    int scantill = 0;
    int reset = 0;
    register int i;
    int c = 0;
    if(!w || (l = strlen(w)) < 3)
    {
        printf("[%s] too short for spell check as min. length is [%d]. Skipping correction\n", w, l);
        return 0;
    }
    al_match = l*6/10; /* for auto-complete matched hits, take 60% of the string len to match*/
    scantill = l/4 ; /*take 1/4th of the string to compute levenshtein*/
    if(scantill < 2) scantill = 2;
    completion_array = calloc(scantill, sizeof(*completion_array));
    assert(completion_array != NULL);
    for(i = 0; i < scantill; ++i)
    {
        struct list *head;
        char word[2] = {w[i], 0};
        list_init(completion_array+i);
        complete_word(auto_complete, word, completion_array+i);
        while((head = completion_array[i].head))
        {
            struct word_list *wl = LIST_ENTRY(head, struct word_list, list);
            char *acw = wl->word;
            int val;
            list_del(&wl->list, &completion_array[i]);
            free(wl);
            val = compute_levenshtein(w, acw); /*compute the levenshtein distance*/
            if(!val)  /* if direct hit or match then break */
            {
                printf("[%s] already spelled correctly\n", w);
                reset = 1;
                goto out_free;
            }
            if(val <= scantill || 
               (val = 2,
                !strncmp(w, acw, al_match))
               ) /*if its within limit or matches percent. of the string, add*/
            {
                if(!(c & 7))
                {
                    levenshtein_array = realloc(levenshtein_array, (c+8)*sizeof(*levenshtein_array));
                    assert(levenshtein_array !=  NULL);
                    memset(levenshtein_array + c, 0, sizeof(*levenshtein_array) * 8);
                }
                levenshtein_array[c].word = acw;
                levenshtein_array[c++].val = val;
            }
        }
    }
    qsort(levenshtein_array, c, sizeof(*levenshtein_array), levenshtein_cmp);
    printf("[%d] spell check suggestions found for word [%s]. Displaying [%d]\n", c, w, 
           l = min(c, MIN_SPELL_CHECK_SUGGESTIONS));
    out_free:
    for(i = 0; i < l; ++i)
    {
        if(!reset && levenshtein_array[i].val > 0)
        {
            printf("Spell check suggestion [%d] - [%s]\n", i+1, levenshtein_array[i].word);
        }
        free(levenshtein_array[i].word);
    }
    free(levenshtein_array);
    return 0;
}
        

int main(int argc, char **argv)
{
    if(argc == 1)
    {
        printf("Specify the word list file as input to do auto completion or spell check on the remaining arguments\n");
        exit(0);
    }
    init_auto_complete(&auto_complete);
    if(argc > 1)
        read_words_file(&auto_complete, argv[1], NULL, NULL);
#ifndef SPELL_CHECKER
    {
        struct list *l, *temp;
        struct list *d = NULL;
        printf("Finding [%d] words in the list\n", auto_complete.list.nodes);
        l = auto_complete.list.head;
        auto_complete.list.head = NULL;
        //    dup_list(l, d, struct word_list, list);
        while(l)
        {
            LIST_DECLARE(completion_list);
            struct word_list *wl = LIST_ENTRY(l, struct word_list, list);
            temp = l->next;
            printf("Finding word [%s]\n", wl->word);
            assert(complete_word(&auto_complete, wl->word, &completion_list) == 0);
            list_dump(&completion_list, struct word_list, list, word_list_display_cb);
            list_destroy(&completion_list, struct word_list, list, word_list_destroy_cb);
            free(wl->word);
            free(wl);
            l = temp;
        }

        if(argc > 2)
        {
            register int i;
            for(i = 2; i < argc; ++i)
            {
                LIST_DECLARE(completion_list);
                printf("\nSearching completion for word [%s]\n", argv[i]);
                complete_word(&auto_complete, argv[i], &completion_list);
                if(completion_list.nodes > 0 )
                {
                    list_dump(&completion_list, struct word_list, list, word_list_display_cb);
                    list_destroy(&completion_list, struct word_list, list, word_list_destroy_cb);
                }
            }
        }
    }
#else
    if(argc > 2)
    {
        register int i;
        for(i = 2; i < argc; ++i)
        {
            printf("Spell check on word [%s]\n", argv[i]);
            spell_check(&auto_complete, argv[i]);
        }
    }
#endif

    return 0;
}
