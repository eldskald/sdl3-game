#include <SDL3/SDL.h>
#include <data-structs/dynarr.h>
#include <data-structs/hashmap.h>
#include <data-structs/list.h>
#include <services/json.h>
#include <services/servicedefs.h>

typedef enum {
    elemarr,
    elemobj,
} elemtype;



static char* double_str_cap(char* str, size_t* cap) {
    char* new_str = SDL_calloc(*cap << 1, sizeof(char));
    SDL_memcpy(new_str, str, *cap);
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

static void free_obj(hashmap* obj) {
    for (node* i = obj->indexes.head; i; i = i->next) {
        JSON_free(obj->values[i->val]);
    }
    clear_hashmap(obj);
}

static void free_arr(dynarr* arr) {
    for (size_t i = 0; i < arr->len; i++) {
        JSON_free(arr->at[i]);
    }
    clear_dynarr(arr);
}

static void
error_obj(hashmap* obj, char* val, list* recursions, char* message) {
    free_obj(obj);
    SDL_free(val);
    clear_list(recursions);
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
}

static void error_arr(dynarr* arr, char* val, list* recursions, char* message) {
    free_arr(arr);
    SDL_free(val);
    clear_list(recursions);
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
}



jsondata* JSON_parse(const char* string) {
    bool parsing = false;
    jsontype parsing_type = obj;
    bool is_parsing_str = false;

    // Reset with reset_val()
    char* val = SDL_calloc(PARSED_STR_STARTING_SIZE + 1, sizeof(char));
    size_t val_size = PARSED_STR_STARTING_SIZE;
    size_t val_i = 0;

    dynarr arrval = (dynarr){0};
    hashmap objval = (hashmap){0};

    bool parsing_num_found_dot = false;
    int parsing_str_found_dquotes = 0;
    bool parsing_arr_ended = false;
    bool parsing_obj_key = false;
    bool parsing_obj_val = false;
    char* parsing_obj_curr_key = NULL;
    bool parsing_obj_ended = false;

    list recursions = (list){0};

    for (const char* p = string; *p; p++) {
        char curr = *p;

        // Ignore whitespaces when not parsing strings
        if ((curr == ' ' || curr == '\t' || curr == '\n') && !is_parsing_str &&
            !parsing_obj_key)
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
            case '"':
                parsing_type = str;
                is_parsing_str = true;
                continue;
            case '[':
                init_dynarr(&arrval);
                parsing_type = arr;
                continue;
            case '{':
                init_hashmap(&objval);
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
                    return NULL;
                }
                parsing_obj_key = true;
                continue;
            default:
                SDL_free(val);
                clear_list(&recursions);
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "JSON ERROR: invalid first char: %c",
                             curr);
                return NULL;
            }
        }

        // When parsing a number
        if (parsing_type == num) {
            if (curr == '.' && parsing_num_found_dot) {
                SDL_free(val);
                clear_list(&recursions);
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "JSON ERROR: invalid number");
                return NULL;
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
                return NULL;
            }

            switch (curr) {
            case ',':
                if (is_parsing_str) {
                    val[val_i] = curr;
                    val_i++;
                    break;
                }
                if (recursions.head) {
                    val[val_i] = curr;
                    val_i++;
                    break;
                }
                jsondata* elem = JSON_parse(val);
                if (!elem) {
                    error_arr(&arrval,
                              val,
                              &recursions,
                              "JSON ERROR: invalid value in array");
                    return NULL;
                }
                push_to_dynarr(elem, &arrval);
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
                push_to_list(elemobj, &recursions);
                val[val_i] = curr;
                val_i++;
                break;
            case '[':
                if (is_parsing_str) {
                    val[val_i] = curr;
                    val_i++;
                    break;
                }
                push_to_list(elemarr, &recursions);
                val[val_i] = curr;
                val_i++;
                break;
            case '}':
                if (is_parsing_str) {
                    val[val_i] = curr;
                    val_i++;
                    break;
                }
                if (recursions.head->val == elemobj) {
                    pop_from_list(&recursions);
                    val[val_i] = curr;
                    val_i++;
                    break;
                } else {
                    error_arr(
                        &arrval, val, &recursions, "JSON ERROR: invalid array");
                    return NULL;
                }
                break;
            case ']':
                if (is_parsing_str) {
                    val[val_i] = curr;
                    val_i++;
                    break;
                }
                if (!recursions.head) {
                    jsondata* elem = JSON_parse(val);
                    if (!elem) {
                        error_arr(&arrval,
                                  val,
                                  &recursions,
                                  "JSON ERROR: invalid value in array");
                        return NULL;
                    }
                    push_to_dynarr(elem, &arrval);
                    SDL_free(val);
                    parsing_arr_ended = true;
                    break;
                } else if (recursions.head->val == elemarr) {
                    pop_from_list(&recursions);
                    val[val_i] = curr;
                    val_i++;
                    break;
                } else if (recursions.head->val == elemobj) {
                    error_arr(
                        &arrval, val, &recursions, "JSON ERROR: invalid array");
                    return NULL;
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
                return NULL;
            }

            switch (curr) {
            case '"':
                // Parsing key, quotation marks right after the first one
                if (parsing_obj_key && val_i == 0) {
                    error_obj(&objval,
                              val,
                              &recursions,
                              "JSON ERROR: invalid key in object");
                    return NULL;

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
                        return NULL;
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
                if (recursions.head) {
                    val[val_i] = curr;
                    val_i++;
                    break;
                }
                jsondata* elem = JSON_parse(val);
                if (!elem) {
                    error_obj(&objval,
                              val,
                              &recursions,
                              "JSON ERROR: invalid element in object");
                    return NULL;
                };
                set_on_hashmap(elem, parsing_obj_curr_key, &objval);
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
                    return NULL;
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
                push_to_list(elemobj, &recursions);
                val[val_i] = curr;
                val_i++;
                break;
            case '[':
                if (is_parsing_str || parsing_obj_key) {
                    val[val_i] = curr;
                    val_i++;
                    break;
                }
                push_to_list(elemarr, &recursions);
                val[val_i] = curr;
                val_i++;
                break;
            case '}':
                if (is_parsing_str || parsing_obj_key) {
                    val[val_i] = curr;
                    val_i++;
                    break;
                }
                if (!recursions.head) {
                    jsondata* elem = JSON_parse(val);
                    if (!elem) {
                        error_obj(&objval,
                                  val,
                                  &recursions,
                                  "JSON ERROR: invalid element in object");
                        return NULL;
                    };
                    set_on_hashmap(elem, parsing_obj_curr_key, &objval);
                    SDL_free(parsing_obj_curr_key);
                    SDL_free(val);
                    parsing_obj_ended = true;
                    parsing_obj_val = false;
                    break;
                } else if (recursions.head->val == elemobj) {
                    pop_from_list(&recursions);
                    val[val_i] = curr;
                    val_i++;
                    break;
                } else if (recursions.head->val == elemarr) {
                    error_obj(&objval,
                              val,
                              &recursions,
                              "JSON ERROR: invalid element in object");
                    return NULL;
                }
                break;
            case ']':
                if (is_parsing_str || parsing_obj_key) {
                    val[val_i] = curr;
                    val_i++;
                    break;
                }
                if (!recursions.head || recursions.head->val == elemobj) {
                    error_obj(&objval,
                              val,
                              &recursions,
                              "JSON ERROR: invalid element in object");
                    return NULL;
                } else if (recursions.head->val == elemarr) {
                    pop_from_list(&recursions);
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

    jsondata* res = SDL_malloc(sizeof(jsondata));
    res->type = parsing_type;
    res->num = 0;
    res->str = NULL;
    res->arr = arrval;
    res->obj = objval;

    if (parsing_type == num) {
        res->num = SDL_strtod(val, NULL);
        SDL_free(val);
    }

    if (parsing_type == str) {
        if (val[val_i - 1] != '"' ||
            (val[val_i - 1] == '"' && parsing_str_found_dquotes > 1)) {
            SDL_free(val);
            clear_list(&recursions);
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "JSON ERROR: invalid string ");
            return NULL;
        }
        val[val_i - 1] = '\0';
        res->str = val;
    }

    clear_list(&recursions);
    return res;
}



void JSON_free(jsondata* value) {
    if (value->str) SDL_free(value->str);
    if (value->arr.cap) free_arr(&value->arr);
    if (value->obj.cap) free_obj(&value->obj);
    SDL_free(value);
}
