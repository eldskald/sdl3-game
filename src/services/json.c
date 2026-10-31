#include "services/json.h"
#include "data-structs/dynarr.h"
#include "data-structs/hashmap.h"
#include "services/defs.h"
#include <SDL3/SDL.h>

typedef enum {
    elemarr,
    elemobj,
} elemtype;



static char* double_str_cap(char* str, size_t* cap) {
    char* new_str = SDL_calloc(*cap << 1, sizeof(char));
    SDL_memcpy(new_str, str, *cap * sizeof(char));
    SDL_free(str);
    *cap = *cap << 1;
    return new_str;
}

static void reset_val(char** val, size_t* i, size_t* cap) {
    SDL_free(*val);
    *val = SDL_calloc(PARSED_STR_STARTING_SIZE, sizeof(char));
    *cap = PARSED_STR_STARTING_SIZE;
    *i = 0;
}

// The innermost open array or object on `recursions`.
static elemtype top(dynarr* recursions) {
    return *(elemtype*)dynarr_at(recursions->len - 1, recursions);
}

static void free_value(jsondata* value);

static void free_obj(hashmap* obj) {
    for (size_t i = 0; i < obj->len; i++)
        free_value(hashmap_value_at(i, obj));
    deinit_hashmap(obj);
}

static void free_arr(dynarr* arr) {
    for (size_t i = 0; i < arr->len; i++)
        free_value(JSON_at(i, arr));
    deinit_dynarr(arr);
}

// Frees what `value` holds, but not `value` itself.
static void free_value(jsondata* value) {
    if (value->type == str) SDL_free(value->str);
    if (value->type == arr) free_arr(&value->arr);
    if (value->type == obj) free_obj(&value->obj);
}

static void
error_obj(hashmap* obj, char* val, dynarr* recursions, char* message) {
    free_obj(obj);
    SDL_free(val);
    deinit_dynarr(recursions);
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
}

static void
error_arr(dynarr* arr, char* val, dynarr* recursions, char* message) {
    free_arr(arr);
    SDL_free(val);
    deinit_dynarr(recursions);
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
}



// Parses `string` into `res`. Returns false on errors, leaving nothing on
// `res` to be freed.
static bool parse(const char* string, jsondata* res) {
    bool parsing = false;
    jsontype parsing_type = obj;
    bool is_parsing_str = false;

    // Reset with reset_val()
    char* val = SDL_calloc(PARSED_STR_STARTING_SIZE + 1, sizeof(char));
    size_t val_size = PARSED_STR_STARTING_SIZE;
    size_t val_i = 0;

    dynarr arrval = (dynarr){0};
    hashmap objval = (hashmap){0};

    bool parsed_true = false;
    bool parsing_num_found_dot = false;
    int parsing_str_found_dquotes = 0;
    bool parsing_arr_ended = false;
    bool parsing_obj_key = false;
    bool parsing_obj_val = false;
    char* parsing_obj_curr_key = NULL;
    bool parsing_obj_ended = false;

    // Stack of the elemtypes of the arrays and objects open inside the value
    dynarr recursions = (dynarr){0};
    init_dynarr_with(&recursions, sizeof(elemtype), 0);

    for (const char* p = string; *p; p++) {
        char curr = *p;

        // Ignore whitespaces when not parsing strings or numbers
        if ((curr == ' ' || curr == '\t' || curr == '\n') && !is_parsing_str &&
            !parsing_obj_key && parsing_type != num)
            continue;

        // When parsing the first digit, figure out what the data type is.
        if (!parsing) {
            parsing = true;
            if ((curr >= '0' && curr <= '9') || curr == '-' || curr == '.') {
                parsing_type = num;
                val[val_i] = curr;
                val_i++;
                if (curr == '.') parsing_num_found_dot = true;
                continue;
            }
            switch (curr) {
            case 't':
                // has to be 'true', check for next chars
                p++;
                if (*p == 'r') {
                    p++;
                    if (*p == 'u') {
                        p++;
                        if (*p == 'e') {
                            parsing_type = bln;
                            parsed_true = true;
                            SDL_free(val);
                            continue;
                        }
                    }
                }
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "JSON ERROR: invalid entry");
                SDL_free(val);
                return false;
            case 'f':
                // has to be 'false', check for next chars
                p++;
                if (*p == 'a') {
                    p++;
                    if (*p == 'l') {
                        p++;
                        if (*p == 's') {
                            p++;
                            if (*p == 'e') {
                                parsing_type = bln;
                                SDL_free(val);
                                continue;
                            }
                        }
                    }
                }
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "JSON ERROR: invalid entry");
                SDL_free(val);
                return false;
            case '"':
                parsing_type = str;
                is_parsing_str = true;
                continue;
            case '[':
                init_dynarr(&arrval, sizeof(jsondata));
                parsing_type = arr;
                continue;
            case '{':
                init_hashmap(&objval, sizeof(jsondata));
                parsing_type = obj;
                // First char after { must be "
                p++;
                while (*p == ' ' || *p == '\t' || *p == '\n')
                    p++;
                if (*p != '"') {
                    error_obj(&objval,
                              val,
                              &recursions,
                              "JSON ERROR: first char in object must be "
                              "quotation mark");
                    return false;
                }
                parsing_obj_key = true;
                continue;
            default:
                SDL_free(val);
                deinit_dynarr(&recursions);
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "JSON ERROR: invalid first char: %c",
                             curr);
                return false;
            }
        }

        // When parsing a number
        if (parsing_type == num) {
            if (curr == '.' && parsing_num_found_dot) {
                SDL_free(val);
                deinit_dynarr(&recursions);
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "JSON ERROR: invalid number");
                return false;
            }
            if (curr == '.' && !parsing_num_found_dot) {
                val[val_i] = curr;
                val_i++;
                parsing_num_found_dot = true;
            }
            if (curr >= '0' && curr <= '9') {
                val[val_i] = curr;
                val_i++;
            }
        }

        // When parsing a string
        if (parsing_type == str) {
            val[val_i] = curr;
            val_i++;
            if (curr == '"') parsing_str_found_dquotes++;
        }

        // When parsing an array
        if (parsing_type == arr) {
            if (parsing_arr_ended) {
                error_arr(
                    &arrval, val, &recursions, "JSON ERROR: invalid array");
                return false;
            }

            switch (curr) {
            case ',':
                if (is_parsing_str) {
                    val[val_i] = curr;
                    val_i++;
                    break;
                }
                if (recursions.len) {
                    val[val_i] = curr;
                    val_i++;
                    break;
                }
                jsondata elem = (jsondata){0};
                if (!parse(val, &elem)) {
                    error_arr(&arrval,
                              val,
                              &recursions,
                              "JSON ERROR: invalid value in array");
                    return false;
                }
                push_to_dynarr(&elem, &arrval);
                reset_val(&val, &val_i, &val_size);
                break;
            case '"':
                is_parsing_str = !is_parsing_str;
                val[val_i] = curr;
                val_i++;
                break;
            case '{':
                if (is_parsing_str) {
                    val[val_i] = curr;
                    val_i++;
                    break;
                }
                push_to_dynarr(&(elemtype){elemobj}, &recursions);
                val[val_i] = curr;
                val_i++;
                break;
            case '[':
                if (is_parsing_str) {
                    val[val_i] = curr;
                    val_i++;
                    break;
                }
                push_to_dynarr(&(elemtype){elemarr}, &recursions);
                val[val_i] = curr;
                val_i++;
                break;
            case '}':
                if (is_parsing_str) {
                    val[val_i] = curr;
                    val_i++;
                    break;
                }
                if (top(&recursions) == elemobj) {
                    remove_at_from_dynarr(recursions.len - 1, &recursions);
                    val[val_i] = curr;
                    val_i++;
                    break;
                } else {
                    error_arr(
                        &arrval, val, &recursions, "JSON ERROR: invalid array");
                    return false;
                }
                break;
            case ']':
                if (is_parsing_str) {
                    val[val_i] = curr;
                    val_i++;
                    break;
                }
                if (!recursions.len) {
                    jsondata elem = (jsondata){0};
                    if (!parse(val, &elem)) {
                        error_arr(&arrval,
                                  val,
                                  &recursions,
                                  "JSON ERROR: invalid value in array");
                        return false;
                    }
                    push_to_dynarr(&elem, &arrval);
                    SDL_free(val);
                    parsing_arr_ended = true;
                    break;
                } else if (top(&recursions) == elemarr) {
                    remove_at_from_dynarr(recursions.len - 1, &recursions);
                    val[val_i] = curr;
                    val_i++;
                    break;
                } else if (top(&recursions) == elemobj) {
                    error_arr(
                        &arrval, val, &recursions, "JSON ERROR: invalid array");
                    return false;
                }
                break;
            default:
                val[val_i] = curr;
                val_i++;
                break;
            }
        }

        // When parsing an object
        if (parsing_type == obj) {
            if (parsing_obj_ended) {
                error_obj(
                    &objval, val, &recursions, "JSON ERROR: invalid object");
                return false;
            }

            switch (curr) {
            case '"':
                // Parsing key, quotation marks right after the first one
                if (parsing_obj_key && val_i == 0) {
                    error_obj(&objval,
                              val,
                              &recursions,
                              "JSON ERROR: invalid key in object");
                    return false;

                    // Parsing key, quotation marks after non empty string
                } else if (parsing_obj_key && val_i > 0) {

                    // Checking if next char is colon
                    p++;
                    while (*p == ' ' || *p == '\t' || *p == '\n')
                        p++;
                    if (*p != ':') {
                        error_obj(
                            &objval,
                            val,
                            &recursions,
                            "JSON ERROR: missing colon after key in object");
                        return false;
                    }

                    // If everything is good, setup for parsing value
                    parsing_obj_curr_key =
                        SDL_calloc(KEY_MAX_SIZE + 1, sizeof(char));
                    SDL_strlcpy(parsing_obj_curr_key, val, KEY_MAX_SIZE);
                    parsing_obj_key = false;
                    parsing_obj_val = true;
                    reset_val(&val, &val_i, &val_size);
                    break;

                    // Parsing value
                } else if (parsing_obj_val) {
                    is_parsing_str = !is_parsing_str;
                    val[val_i] = curr;
                    val_i++;
                    break;
                }
                break;
            case ',':
                if (is_parsing_str || parsing_obj_key) {
                    val[val_i] = curr;
                    val_i++;
                    break;
                }
                if (recursions.len) {
                    val[val_i] = curr;
                    val_i++;
                    break;
                }
                jsondata elem = (jsondata){0};
                if (!parse(val, &elem)) {
                    error_obj(&objval,
                              val,
                              &recursions,
                              "JSON ERROR: invalid element in object");
                    return false;
                };
                set_on_hashmap(&elem, parsing_obj_curr_key, &objval);
                SDL_free(parsing_obj_curr_key);
                reset_val(&val, &val_i, &val_size);

                // Checking if next char is "
                p++;
                while (*p == ' ' || *p == '\t' || *p == '\n')
                    p++;
                if (*p != '"') {
                    error_obj(&objval,
                              val,
                              &recursions,
                              "JSON ERROR: missing quotation mark after comma "
                              "in object");
                    return false;
                }

                parsing_obj_key = true;
                parsing_obj_val = false;
                break;
            case '{':
                if (is_parsing_str || parsing_obj_key) {
                    val[val_i] = curr;
                    val_i++;
                    break;
                }
                push_to_dynarr(&(elemtype){elemobj}, &recursions);
                val[val_i] = curr;
                val_i++;
                break;
            case '[':
                if (is_parsing_str || parsing_obj_key) {
                    val[val_i] = curr;
                    val_i++;
                    break;
                }
                push_to_dynarr(&(elemtype){elemarr}, &recursions);
                val[val_i] = curr;
                val_i++;
                break;
            case '}':
                if (is_parsing_str || parsing_obj_key) {
                    val[val_i] = curr;
                    val_i++;
                    break;
                }
                if (!recursions.len) {
                    jsondata elem = (jsondata){0};
                    if (!parse(val, &elem)) {
                        error_obj(&objval,
                                  val,
                                  &recursions,
                                  "JSON ERROR: invalid element in object");
                        return false;
                    };
                    set_on_hashmap(&elem, parsing_obj_curr_key, &objval);
                    SDL_free(parsing_obj_curr_key);
                    SDL_free(val);
                    parsing_obj_ended = true;
                    parsing_obj_val = false;
                    break;
                } else if (top(&recursions) == elemobj) {
                    remove_at_from_dynarr(recursions.len - 1, &recursions);
                    val[val_i] = curr;
                    val_i++;
                    break;
                } else if (top(&recursions) == elemarr) {
                    error_obj(&objval,
                              val,
                              &recursions,
                              "JSON ERROR: invalid element in object");
                    return false;
                }
                break;
            case ']':
                if (is_parsing_str || parsing_obj_key) {
                    val[val_i] = curr;
                    val_i++;
                    break;
                }
                if (!recursions.len || top(&recursions) == elemobj) {
                    error_obj(&objval,
                              val,
                              &recursions,
                              "JSON ERROR: invalid element in object");
                    return false;
                } else if (top(&recursions) == elemarr) {
                    remove_at_from_dynarr(recursions.len - 1, &recursions);
                    val[val_i] = curr;
                    val_i++;
                    break;
                }
                break;
            default:
                val[val_i] = curr;
                val_i++;
                break;
            }
        }

        if (val_i == val_size - 1) val = double_str_cap(val, &val_size);
    }

    *res = (jsondata){.type = parsing_type};
    if (parsing_type == arr) res->arr = arrval;
    if (parsing_type == obj) res->obj = objval;
    if (parsing_type == bln) res->bln = parsed_true;

    if (parsing_type == num) {
        res->num = SDL_strtod(val, NULL);
        SDL_free(val);
    }

    if (parsing_type == str) {
        if (val[val_i - 1] != '"' ||
            (val[val_i - 1] == '"' && parsing_str_found_dquotes > 1)) {
            SDL_free(val);
            deinit_dynarr(&recursions);
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "JSON ERROR: invalid string ");
            return false;
        }
        val[val_i - 1] = '\0';
        res->str = val;
    }

    deinit_dynarr(&recursions);
    return true;
}



jsondata* JSON_parse(const char* string) {
    jsondata* res = SDL_malloc(sizeof(jsondata));
    if (parse(string, res)) return res;
    SDL_free(res);
    return NULL;
}

void JSON_free(jsondata* value) {
    free_value(value);
    SDL_free(value);
}
