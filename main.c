#include "auto_complete.h"

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

int main(int argc, char **argv)
{
    const char *wordlist[] = { "damn", "is", "damned", "disgusting", "array", "fuck", NULL };
    const char **w = wordlist;
    const char *p;
    struct list *l, *temp;
    struct list *d = NULL;
    init_auto_complete(&auto_complete);
    while( (p = *w++) )
    {
        if(!add_word(&auto_complete, p))
            link_word(strdup(p), &auto_complete.list, 0);
    }
    if(argc > 1)
        read_words_file(&auto_complete, argv[1], NULL, NULL);
    else link_word(strdup("is"), &auto_complete.list, 0);
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
    return 0;
}
