#include "services/json.h"
#include "data-structs/hashmap.h"
#include "tests.h"
#include "tests/json.h"
#include <SDL3/SDL.h>
#include <stdio.h>

void test_json(void) {
    printf(YELLOW "\nTesting JSON parser...\n" RESET);

    jsondata* test = JSON_parse("42.175");
    expect("Parsing 42.175...",
           2,
           (bool[]){test->type == num, test->num == 42.175});
    JSON_free(test);

    test = JSON_parse("true");
    expect(
        "Parsing true...", 2, (bool[]){test->type == bln, test->bln == true});
    JSON_free(test);

    test = JSON_parse("false");
    expect(
        "Parsing false...", 2, (bool[]){test->type == bln, test->bln == false});
    JSON_free(test);

    test = JSON_parse("-33.5");
    expect(
        "Parsing -33.5...", 2, (bool[]){test->type == num, test->num == -33.5});
    JSON_free(test);

    test = JSON_parse(".2");
    expect("Parsing .2...", 2, (bool[]){test->type == num, test->num == 0.2});
    JSON_free(test);

    test = JSON_parse("02");
    expect("Parsing 02...", 2, (bool[]){test->type == num, test->num == 2});
    JSON_free(test);

    test = JSON_parse("\"test\"");
    expect("Parsing \"test\"...",
           2,
           (bool[]){test->type == str, SDL_strcmp(test->str, "test") == 0});
    JSON_free(test);

    test = JSON_parse("\"test with, space, and commas\"");
    expect(
        "Parsing \"test with, space, and commas\"...",
        2,
        (bool[]){test->type == str,
                 SDL_strcmp(test->str, "test with, space, and commas") == 0});
    JSON_free(test);

    test = JSON_parse("\"[24, 17]\"");
    expect("Parsing \"[24, 17]\"...",
           2,
           (bool[]){test->type == str, SDL_strcmp(test->str, "[24, 17]") == 0});
    JSON_free(test);

    test = JSON_parse("[24, 17]");
    expect("Parsing [24, 17]...",
           5,
           (bool[]){
               test->type == arr,
               JSON_at(0, &test->arr)->type == num,
               JSON_at(0, &test->arr)->num == 24,
               JSON_at(1, &test->arr)->type == num,
               JSON_at(1, &test->arr)->num == 17,
           });
    JSON_free(test);

    test = JSON_parse("[24.3, \"array\"]");
    expect("Parsing [24.3, \"array\"]...",
           5,
           (bool[]){test->type == arr,
                    JSON_at(0, &test->arr)->type == num,
                    JSON_at(0, &test->arr)->num == 24.3,
                    JSON_at(1, &test->arr)->type == str,
                    SDL_strcmp(JSON_at(1, &test->arr)->str, "array") == 0});
    JSON_free(test);

    test = JSON_parse("[[0, 1], [0, 2], \"recursion\"]");
    expect("Parsing [[0, 1], [0, 2], \"recursion\"]...",
           13,
           (bool[]){test->type == arr,
                    JSON_at(0, &test->arr)->type == arr,
                    JSON_at(0, &JSON_at(0, &test->arr)->arr)->type == num,
                    JSON_at(0, &JSON_at(0, &test->arr)->arr)->num == 0,
                    JSON_at(1, &JSON_at(0, &test->arr)->arr)->type == num,
                    JSON_at(1, &JSON_at(0, &test->arr)->arr)->num == 1,
                    JSON_at(1, &test->arr)->type == arr,
                    JSON_at(0, &JSON_at(1, &test->arr)->arr)->type == num,
                    JSON_at(0, &JSON_at(1, &test->arr)->arr)->num == 0,
                    JSON_at(1, &JSON_at(1, &test->arr)->arr)->type == num,
                    JSON_at(1, &JSON_at(1, &test->arr)->arr)->num == 2,
                    JSON_at(2, &test->arr)->type == str,
                    SDL_strcmp(JSON_at(2, &test->arr)->str, "recursion") == 0});
    JSON_free(test);

    test = JSON_parse("{\"test\":42,\"key\":true}");
    expect("Parsing {\"test\":42,\"key\":true}...",
           5,
           (bool[]){test->type == obj,
                    JSON_get("test", test)->type == num,
                    JSON_get("test", test)->num == 42,
                    JSON_get("key", test)->type == bln,
                    JSON_get("key", test)->bln == true});
    JSON_free(test);

    test = JSON_parse("{\"array\": [33, .5], \"object\": {\"key\": \"val\"}}");
    jsondata* arrv = JSON_get("array", test);
    jsondata* objv = JSON_get("object", test);
    expect("Parsing {\"array\": [33, .5], \"object\": {\"key\": \"val\"}}...",
           9,
           (bool[]){test->type == obj,
                    arrv->type == arr,
                    JSON_at(0, &arrv->arr)->type == num,
                    JSON_at(0, &arrv->arr)->num == 33,
                    JSON_at(1, &arrv->arr)->type == num,
                    JSON_at(1, &arrv->arr)->num == 0.5,
                    objv->type == obj,
                    JSON_get("key", objv)->type == str,
                    SDL_strcmp(JSON_get("key", objv)->str, "val") == 0});
    JSON_free(test);

    test = JSON_parse(
        "[{\"id\":17,\"pos\":[33,-12]},{\"id\":04,\"pos\":[-.5,12]}]");
    jsondata* v0 = JSON_at(0, &test->arr);
    jsondata* id0 = JSON_get("id", v0);
    jsondata* pos0 = JSON_get("pos", v0);
    jsondata* v1 = JSON_at(1, &test->arr);
    jsondata* id1 = JSON_get("id", v1);
    jsondata* pos1 = JSON_get("pos", v1);
    expect("Parsing "
           "[{\"id\":17,\"pos\":[33,-12]},{\"id\":04,\"pos\":[-.5,12]}]...",
           17,
           (bool[]){
               test->type == arr,
               v0->type == obj,
               id0->type == num,
               id0->num == 17,
               pos0->type == arr,
               JSON_at(0, &pos0->arr)->type == num,
               JSON_at(0, &pos0->arr)->num == 33,
               JSON_at(1, &pos0->arr)->type == num,
               JSON_at(1, &pos0->arr)->num == -12,
               v1->type == obj,
               id1->type == num,
               id1->num == 4,
               pos1->type == arr,
               JSON_at(0, &pos1->arr)->type == num,
               JSON_at(0, &pos1->arr)->num == -0.5,
               JSON_at(1, &pos1->arr)->type == num,
               JSON_at(1, &pos1->arr)->num == 12,
           });
    JSON_free(test);
}
