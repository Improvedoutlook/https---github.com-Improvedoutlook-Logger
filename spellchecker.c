#include "spellchecker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define INITIAL_DICT_CAPACITY 10000
#define INITIAL_MISSPELLED_CAPACITY 100

// Case-insensitive comparator for qsort
// Used for sorting both main and user dictionaries
static int DictionaryComparator(const void *a, const void *b) {
    const char *s1 = *(const char * const *)a;
    const char *s2 = *(const char * const *)b;
    
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

// Levenshtein distance calculation
static int LevenshteinDistance(const char *s1, const char *s2) {
    int len1 = strlen(s1);
    int len2 = strlen(s2);
    
    if (len1 == 0) return len2;
    if (len2 == 0) return len1;
    
    int *d = (int *)malloc((len2 + 1) * sizeof(int));
    if (!d) return -1;
    
    for (int i = 0; i <= len2; i++) {
        d[i] = i;
    }
    
    for (int i = 1; i <= len1; i++) {
        int prev_diag = i - 1;
        d[0] = i;
        
        for (int j = 1; j <= len2; j++) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            int temp = d[j];
            d[j] = (d[j] + 1 < d[j - 1] + 1) ? (d[j] + 1) : (d[j - 1] + 1);
            d[j] = (d[j] < prev_diag + cost) ? d[j] : (prev_diag + cost);
            prev_diag = temp;
        }
    }
    
    int result = d[len2];
    free(d);
    return result;
}

// Case-insensitive string comparison
static int strcasecmp_custom(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

// Binary search for dictionary lookup
static BOOL BinarySearchDictionary(Dictionary *dict, const char *word) {
    int left = 0, right = dict->count - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        int cmp = strcasecmp_custom(dict->words[mid], word);
        if (cmp == 0) return TRUE;
        if (cmp < 0) left = mid + 1;
        else right = mid - 1;
    }
    return FALSE;
}

// Create spell checker instance
SpellChecker* SpellChecker_Create(void) {
    SpellChecker *sc = (SpellChecker *)malloc(sizeof(SpellChecker));
    if (!sc) return NULL;
    
    memset(sc, 0, sizeof(SpellChecker));
    sc->enabled = TRUE;
    sc->suggestionsEnabled = TRUE;
    
    // Initialize dictionaries
    sc->mainDictionary.capacity = INITIAL_DICT_CAPACITY;
    sc->mainDictionary.words = (char **)malloc(INITIAL_DICT_CAPACITY * sizeof(char *));
    
    sc->userDictionary.capacity = 1000;
    sc->userDictionary.words = (char **)malloc(1000 * sizeof(char *));
    
    sc->ignoredWords.capacity = 100;
    sc->ignoredWords.words = (char **)malloc(100 * sizeof(char *));
    
    sc->misspelled.capacity = INITIAL_MISSPELLED_CAPACITY;
    sc->misspelled.words = (MisspelledWord *)malloc(INITIAL_MISSPELLED_CAPACITY * sizeof(MisspelledWord));
    
    if (!sc->mainDictionary.words || !sc->userDictionary.words || !sc->ignoredWords.words || !sc->misspelled.words) {
        SpellChecker_Destroy(sc);
        return NULL;
    }
    
    return sc;
}

// Destroy spell checker instance
void SpellChecker_Destroy(SpellChecker *sc) {
    if (!sc) return;
    
    for (int i = 0; i < sc->mainDictionary.count; i++) {
        free(sc->mainDictionary.words[i]);
    }
    free(sc->mainDictionary.words);
    
    for (int i = 0; i < sc->userDictionary.count; i++) {
        free(sc->userDictionary.words[i]);
    }
    free(sc->userDictionary.words);
    
    for (int i = 0; i < sc->ignoredWords.count; i++) {
        free(sc->ignoredWords.words[i]);
    }
    free(sc->ignoredWords.words);
    
    free(sc->misspelled.words);
    free(sc);
}

// Load dictionary from file
BOOL SpellChecker_LoadDictionary(SpellChecker *sc, const char *filePath) {
    if (!sc || !filePath) return FALSE;
    
    FILE *file = fopen(filePath, "r");
    if (!file) {
        return FALSE;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Remove trailing whitespace
        int len = strlen(line);
        while (len > 0 && isspace((unsigned char)line[len - 1])) {
            line[--len] = '\0';
        }
        
        if (len == 0 || line[0] == '#') continue; // Skip empty lines and comments
        
        // Check if we need to expand capacity
        if (sc->mainDictionary.count >= sc->mainDictionary.capacity) {
            sc->mainDictionary.capacity *= 2;
            char **newWords = (char **)realloc(sc->mainDictionary.words, 
                                              sc->mainDictionary.capacity * sizeof(char *));
            if (!newWords) {
                fclose(file);
                return FALSE;
            }
            sc->mainDictionary.words = newWords;
        }
        
        // Add word to dictionary
        sc->mainDictionary.words[sc->mainDictionary.count] = (char *)malloc(len + 1);
        if (!sc->mainDictionary.words[sc->mainDictionary.count]) {
            fclose(file);
            return FALSE;
        }
        strcpy(sc->mainDictionary.words[sc->mainDictionary.count], line);
        sc->mainDictionary.count++;
    }
    
    fclose(file);
    
    // Sort the main dictionary for binary search
    if (sc->mainDictionary.count > 0) {
        qsort(sc->mainDictionary.words, sc->mainDictionary.count, sizeof(char *), DictionaryComparator);
    }
    
    return sc->mainDictionary.count > 0;
}

// Load user dictionary from file
BOOL SpellChecker_LoadUserDictionary(SpellChecker *sc, const char *filePath) {
    if (!sc || !filePath) return FALSE;
    
    FILE *file = fopen(filePath, "r");
    if (!file) {
        return TRUE; // Not an error if user dict doesn't exist yet
    }
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        int len = strlen(line);
        while (len > 0 && isspace((unsigned char)line[len - 1])) {
            line[--len] = '\0';
        }
        
        if (len == 0) continue;
        
        if (sc->userDictionary.count >= sc->userDictionary.capacity) {
            sc->userDictionary.capacity *= 2;
            char **newWords = (char **)realloc(sc->userDictionary.words,
                                              sc->userDictionary.capacity * sizeof(char *));
            if (!newWords) {
                fclose(file);
                return FALSE;
            }
            sc->userDictionary.words = newWords;
        }
        
        sc->userDictionary.words[sc->userDictionary.count] = (char *)malloc(len + 1);
        if (!sc->userDictionary.words[sc->userDictionary.count]) {
            fclose(file);
            return FALSE;
        }
        strcpy(sc->userDictionary.words[sc->userDictionary.count], line);
        sc->userDictionary.count++;
    }
    
    fclose(file);
    
    // Sort the user dictionary for binary search
    if (sc->userDictionary.count > 0) {
        qsort(sc->userDictionary.words, sc->userDictionary.count, sizeof(char *), DictionaryComparator);
    }
    
    return TRUE;
}

// Check if a word is correct
BOOL SpellChecker_IsWordCorrect(SpellChecker *sc, const char *word) {
    if (!sc || !word || strlen(word) == 0) return TRUE;
    
    // Check ignore list first (ignored words are treated as correct)
    if (BinarySearchDictionary(&sc->ignoredWords, word)) return TRUE;
    
    // Check main dictionary
    if (BinarySearchDictionary(&sc->mainDictionary, word)) return TRUE;
    
    // Check user dictionary
    if (BinarySearchDictionary(&sc->userDictionary, word)) return TRUE;
    
    return FALSE;
}

// Extract words from text and check spelling
void SpellChecker_Check(SpellChecker *sc, const char *text) {
    if (!sc || !sc->enabled) {
        if (sc) sc->misspelled.count = 0;
        return;
    }
    
    // Reset misspelled list at start of every pass
    sc->misspelled.count = 0;
    
    // Detect empty or whitespace-only text and reset state
    if (!text || text[0] == '\0') {
        sc->misspelled.count = 0;
        return;
    }
    
    // Check if text is only whitespace
    const char *checkPtr = text;
    BOOL onlyWhitespace = TRUE;
    while (*checkPtr) {
        if (!isspace((unsigned char)*checkPtr)) {
            onlyWhitespace = FALSE;
            break;
        }
        checkPtr++;
    }
    
    if (onlyWhitespace) {
        sc->misspelled.count = 0;
        return;
    }
    
    const char *ptr = text;
    DWORD pos = 0;
    
    while (*ptr) {
        // Skip non-alphabetic characters
        while (*ptr && !isalpha((unsigned char)*ptr)) {
            ptr++;
            pos++;
        }
        
        if (!*ptr) break;
        
        // Extract word
        DWORD wordStart = pos;
        char word[256] = {0};
        int wordLen = 0;
        
        while (*ptr && isalpha((unsigned char)*ptr) && wordLen < sizeof(word) - 1) {
            word[wordLen++] = *ptr;
            ptr++;
            pos++;
        }
        word[wordLen] = '\0';
        
        // Check spelling
        if (!SpellChecker_IsWordCorrect(sc, word)) {
            // Add to misspelled list
            if (sc->misspelled.count >= sc->misspelled.capacity) {
                sc->misspelled.capacity *= 2;
                MisspelledWord *newWords = (MisspelledWord *)realloc(sc->misspelled.words,
                                                                     sc->misspelled.capacity * sizeof(MisspelledWord));
                if (!newWords) return;
                sc->misspelled.words = newWords;
            }
            
            sc->misspelled.words[sc->misspelled.count].startPos = wordStart;
            sc->misspelled.words[sc->misspelled.count].endPos = pos;
            strcpy(sc->misspelled.words[sc->misspelled.count].word, word);
            sc->misspelled.count++;
        }
    }
}

// Get suggestions for a misspelled word
char** SpellChecker_GetSuggestions(SpellChecker *sc, const char *word, int *count) {
    if (!sc || !word || !count) return NULL;
    
    *count = 0;
    
    typedef struct {
        char *word;
        int distance;
    } Suggestion;
    
    Suggestion *suggestions = (Suggestion *)malloc(10 * sizeof(Suggestion));
    if (!suggestions) return NULL;
    
    int suggestCount = 0;
    int maxDistance = 2; // Only suggest words within edit distance of 2
    
    // Check main dictionary
    for (int i = 0; i < sc->mainDictionary.count && suggestCount < 10; i++) {
        int dist = LevenshteinDistance(word, sc->mainDictionary.words[i]);
        if (dist > 0 && dist <= maxDistance) {
            suggestions[suggestCount].word = sc->mainDictionary.words[i];
            suggestions[suggestCount].distance = dist;
            suggestCount++;
        }
    }
    
    // Sort by distance
    for (int i = 0; i < suggestCount - 1; i++) {
        for (int j = i + 1; j < suggestCount; j++) {
            if (suggestions[j].distance < suggestions[i].distance) {
                Suggestion temp = suggestions[i];
                suggestions[i] = suggestions[j];
                suggestions[j] = temp;
            }
        }
    }
    
    // Limit to top 5 suggestions
    if (suggestCount > 5) suggestCount = 5;
    
    // Convert to result array
    char **result = (char **)malloc((suggestCount + 1) * sizeof(char *));
    if (!result) {
        free(suggestions);
        return NULL;
    }
    
    for (int i = 0; i < suggestCount; i++) {
        int len = strlen(suggestions[i].word);
        result[i] = (char *)malloc(len + 1);
        if (!result[i]) {
            for (int j = 0; j < i; j++) free(result[j]);
            free(result);
            free(suggestions);
            return NULL;
        }
        strcpy(result[i], suggestions[i].word);
    }
    result[suggestCount] = NULL;
    
    free(suggestions);
    *count = suggestCount;
    return result;
}

// Free suggestions array
void SpellChecker_FreeSuggestions(char **suggestions, int count) {
    if (!suggestions) return;
    for (int i = 0; i < count; i++) {
        free(suggestions[i]);
    }
    free(suggestions);
}

// Get misspelled words
MisspelledWordList* SpellChecker_GetMisspelledWords(SpellChecker *sc) {
    return sc ? &sc->misspelled : NULL;
}

// Check if position is misspelled
BOOL SpellChecker_IsMisspelledAtPosition(SpellChecker *sc, DWORD pos, char *outWord, int outWordLen) {
    if (!sc) return FALSE;
    
    for (int i = 0; i < sc->misspelled.count; i++) {
        if (pos >= sc->misspelled.words[i].startPos && pos < sc->misspelled.words[i].endPos) {
            if (outWord && outWordLen > 0) {
                strncpy(outWord, sc->misspelled.words[i].word, outWordLen - 1);
                outWord[outWordLen - 1] = '\0';
            }
            return TRUE;
        }
    }
    
    return FALSE;
}

// Add word to user dictionary
void SpellChecker_AddToUserDictionary(SpellChecker *sc, const char *word) {
    if (!sc || !word) return;
    
    // Check if already in user dictionary
    if (BinarySearchDictionary(&sc->userDictionary, word)) return;
    
    if (sc->userDictionary.count >= sc->userDictionary.capacity) {
        sc->userDictionary.capacity *= 2;
        char **newWords = (char **)realloc(sc->userDictionary.words,
                                          sc->userDictionary.capacity * sizeof(char *));
        if (!newWords) return;
        sc->userDictionary.words = newWords;
    }
    
    int len = strlen(word);
    sc->userDictionary.words[sc->userDictionary.count] = (char *)malloc(len + 1);
    if (!sc->userDictionary.words[sc->userDictionary.count]) return;
    
    strcpy(sc->userDictionary.words[sc->userDictionary.count], word);
    sc->userDictionary.count++;
    
    // Re-sort the user dictionary to maintain sorted order for binary search
    if (sc->userDictionary.count > 0) {
        qsort(sc->userDictionary.words, sc->userDictionary.count, sizeof(char *), DictionaryComparator);
    }
}

// Save user dictionary to file
void SpellChecker_SaveUserDictionary(SpellChecker *sc, const char *filePath) {
    if (!sc || !filePath) return;
    
    FILE *file = fopen(filePath, "w");
    if (!file) return;
    
    // Ensure sorted before saving
    if (sc->userDictionary.count > 0) {
        qsort(sc->userDictionary.words, sc->userDictionary.count, sizeof(char *), DictionaryComparator);
    }
    
    for (int i = 0; i < sc->userDictionary.count; i++) {
        fprintf(file, "%s\n", sc->userDictionary.words[i]);
    }
    
    fclose(file);
}

// Add word to ignore list (session-only, not persisted)
void SpellChecker_AddToIgnoreList(SpellChecker *sc, const char *word) {
    if (!sc || !word) return;
    
    // Check if already in ignore list
    if (BinarySearchDictionary(&sc->ignoredWords, word)) return;
    
    if (sc->ignoredWords.count >= sc->ignoredWords.capacity) {
        sc->ignoredWords.capacity *= 2;
        char **newWords = (char **)realloc(sc->ignoredWords.words,
                                          sc->ignoredWords.capacity * sizeof(char *));
        if (!newWords) return;
        sc->ignoredWords.words = newWords;
    }
    
    int len = strlen(word);
    sc->ignoredWords.words[sc->ignoredWords.count] = (char *)malloc(len + 1);
    if (!sc->ignoredWords.words[sc->ignoredWords.count]) return;
    
    strcpy(sc->ignoredWords.words[sc->ignoredWords.count], word);
    sc->ignoredWords.count++;
    
    // Re-sort the ignore list to maintain sorted order for binary search
    if (sc->ignoredWords.count > 0) {
        qsort(sc->ignoredWords.words, sc->ignoredWords.count, sizeof(char *), DictionaryComparator);
    }
}

// Clear all ignored words (useful for starting a new session)
void SpellChecker_ClearIgnoreList(SpellChecker *sc) {
    if (!sc) return;
    
    for (int i = 0; i < sc->ignoredWords.count; i++) {
        free(sc->ignoredWords.words[i]);
    }
    sc->ignoredWords.count = 0;
}

