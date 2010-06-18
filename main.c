#include "auto_complete.h"
#include <getopt.h>
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

static void usage(void) __attribute__((noreturn)); /*volatile void cannot be static*/
static char *progname = "unknown";
static void usage(void)
{
    fprintf(stderr, "%s -f <wordlistfile> -v | verbose  -s <spell_check_test_file> -a <auto_complete_test_file> \n",
            progname);
    exit(127);
}
                 
int main(int argc, char **argv)
{
    char dict_filename[0xff+1] = {0};
    char spell_check_filename[0xff+1] = {0};
    char auto_complete_filename[0xff+1] = {0};
    int c;
    char *prog;
    opterr = 0;

    progname = argv[0];
    if( (prog = strrchr(argv[0], '/') ) )
        progname = prog+1;

    while( (c = getopt(argc, argv, "vf:s:a:h")) != EOF )
    {
        switch(c)
        {
        case 'v':
            auto_complete_opts.verbose = 1;
            break;
            
        case 'f':
            strncpy(dict_filename, optarg, sizeof(dict_filename)-1);
            break;
            
        case 's':
            strncpy(spell_check_filename, optarg, sizeof(spell_check_filename)-1);
            break;

        case 'a':
            strncpy(auto_complete_filename, optarg, sizeof(auto_complete_filename)-1);
            break;

        case 'h':
        case '?':
        default:
            usage();
        }
    }

    if(optind != argc)
        usage();

    if(!dict_filename[0]) /*must have word input file*/
        usage();
    
    if(init_auto_complete(&auto_complete, dict_filename) < 0)
        fprintf(stderr, "Error initializing auto complete context with file [%s]\n", dict_filename);

    if(auto_complete_filename[0])
    {
        /*
         * Get the completion list for each of the words loaded from the filename as a test drive
         */
        LIST_DECLARE(wordlist);
        struct list *l;
        if(read_words(NULL, &wordlist, auto_complete_filename, NULL, NULL) < 0 )
        {
            fprintf(stderr, "Unable to read auto-complete list from [%s]\n", auto_complete_filename);
            goto spellcheck;
        }
        do_log("Auto completion test on [%d] words in the list\n", wordlist.nodes);
        while((l = wordlist.head))
        {
            LIST_DECLARE(completion_list);
            struct word_list *wl = LIST_ENTRY(l, struct word_list, list);
            list_del(l, &wordlist);
            do_log("Finding word [%s]\n", wl->word);
            if(!complete_word(&auto_complete, wl->word, &completion_list))
            {
                list_dump(&completion_list, struct word_list, list, word_list_display_cb);
                list_destroy(&completion_list, struct word_list, list, word_list_destroy_cb);
            }
            free(wl->word);
            free(wl);
        }
    }

    spellcheck:
    if(spell_check_filename[0])
    {
        LIST_DECLARE(wordlist);
        struct list *l;
        if(read_words(NULL, &wordlist, spell_check_filename, NULL, NULL) < 0)
        {
            fprintf(stderr, "Error reading spellcheck wordlist from [%s]\n", spell_check_filename);
            return -1;
        }
        while((l = wordlist.head))
        {
            int word_limit = 10;
            int num_words = word_limit;
            register int i;
            struct spell_check_word *spell_check_list;
            struct word_list *wl = LIST_ENTRY(l, struct word_list, list);
            const char *word = wl->word;
            list_del(l, &wordlist);
            free(wl);
            spell_check_list = calloc(word_limit, sizeof(*spell_check_list));
            assert(spell_check_list != NULL);
            spell_check(&auto_complete, word, spell_check_list, &num_words);
            fprintf(stdout, "\n[%d] Spell check corrections for word [%s] \n",
                    num_words, word);
            if(num_words > word_limit) num_words = word_limit;
            for(i = 0; i < num_words; ++i)
            {
                fprintf(stdout, "Spell check suggestion [%d], word [%s], weight [%d]\n",
                        i+1, spell_check_list[i].word, spell_check_list[i].weight);
            }
            free(spell_check_list);
            free((void*)word);
        }
    }

    return 0;
}
