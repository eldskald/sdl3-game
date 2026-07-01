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
               ((jsondata*)test->arr.at[0])->type == num,
               ((jsondata*)test->arr.at[0])->num == 24,
               ((jsondata*)test->arr.at[1])->type == num,
               ((jsondata*)test->arr.at[1])->num == 17,
           });
    JSON_free(test);

    test = JSON_parse("[24.3, \"array\"]");
    expect(
        "Parsing [24.3, \"array\"]...",
        5,
        (bool[]){test->type == arr,
                 ((jsondata*)test->arr.at[0])->type == num,
                 ((jsondata*)test->arr.at[0])->num == 24.3,
                 ((jsondata*)test->arr.at[1])->type == str,
                 SDL_strcmp(((jsondata*)test->arr.at[1])->str, "array") == 0});
    JSON_free(test);

    test = JSON_parse("[[0, 1], [0, 2], \"recursion\"]");
    expect(
        "Parsing [[0, 1], [0, 2], \"recursion\"]...",
        13,
        (bool[]){
            test->type == arr,
            ((jsondata*)test->arr.at[0])->type == arr,
            ((jsondata*)((jsondata*)test->arr.at[0])->arr.at[0])->type == num,
            ((jsondata*)((jsondata*)test->arr.at[0])->arr.at[0])->num == 0,
            ((jsondata*)((jsondata*)test->arr.at[0])->arr.at[1])->type == num,
            ((jsondata*)((jsondata*)test->arr.at[0])->arr.at[1])->num == 1,
            ((jsondata*)test->arr.at[1])->type == arr,
            ((jsondata*)((jsondata*)test->arr.at[1])->arr.at[0])->type == num,
            ((jsondata*)((jsondata*)test->arr.at[1])->arr.at[0])->num == 0,
            ((jsondata*)((jsondata*)test->arr.at[1])->arr.at[1])->type == num,
            ((jsondata*)((jsondata*)test->arr.at[1])->arr.at[1])->num == 2,
            ((jsondata*)test->arr.at[2])->type == str,
            SDL_strcmp(((jsondata*)test->arr.at[2])->str, "recursion") == 0});
    JSON_free(test);

    test = JSON_parse("{\"test\":42,\"key\":true}");
    expect("Parsing {\"test\":42,\"key\":true}...",
           5,
           (bool[]){
               test->type == obj,
               ((jsondata*)get_from_hashmap("test", &test->obj))->type == num,
               ((jsondata*)get_from_hashmap("test", &test->obj))->num == 42,
               ((jsondata*)get_from_hashmap("key", &test->obj))->type == bln,
               ((jsondata*)get_from_hashmap("key", &test->obj))->bln == true});
    JSON_free(test);

    test = JSON_parse("{\"array\": [33, .5], \"object\": {\"key\": \"val\"}}");
    jsondata* arrv = (jsondata*)get_from_hashmap("array", &test->obj);
    jsondata* objv = (jsondata*)get_from_hashmap("object", &test->obj);
    expect("Parsing {\"array\": [33, .5], \"object\": {\"key\": \"val\"}}...",
           9,
           (bool[]){
               test->type == obj,
               arrv->type == arr,
               ((jsondata*)arrv->arr.at[0])->type == num,
               ((jsondata*)arrv->arr.at[0])->num == 33,
               ((jsondata*)arrv->arr.at[1])->type == num,
               ((jsondata*)arrv->arr.at[1])->num == 0.5,
               objv->type == obj,
               ((jsondata*)get_from_hashmap("key", &objv->obj))->type == str,
               SDL_strcmp(((jsondata*)get_from_hashmap("key", &objv->obj))->str,
                          "val") == 0});
    JSON_free(test);

    test = JSON_parse(
        "[{\"id\":17,\"pos\":[33,-12]},{\"id\":04,\"pos\":[-.5,12]}]");
    jsondata* v0 = (jsondata*)test->arr.at[0];
    jsondata* id0 = (jsondata*)get_from_hashmap("id", &v0->obj);
    jsondata* pos0 = (jsondata*)get_from_hashmap("pos", &v0->obj);
    jsondata* v1 = (jsondata*)test->arr.at[1];
    jsondata* id1 = (jsondata*)get_from_hashmap("id", &v1->obj);
    jsondata* pos1 = (jsondata*)get_from_hashmap("pos", &v1->obj);
    expect("Parsing "
           "[{\"id\":17,\"pos\":[33,-12]},{\"id\":04,\"pos\":[-.5,12]}]...",
           17,
           (bool[]){
               test->type == arr,
               v0->type == obj,
               id0->type == num,
               id0->num == 17,
               pos0->type == arr,
               ((jsondata*)pos0->arr.at[0])->type == num,
               ((jsondata*)pos0->arr.at[0])->num == 33,
               ((jsondata*)pos0->arr.at[1])->type == num,
               ((jsondata*)pos0->arr.at[1])->num == -12,
               v1->type == obj,
               id1->type == num,
               id1->num == 4,
               pos1->type == arr,
               ((jsondata*)pos1->arr.at[0])->type == num,
               ((jsondata*)pos1->arr.at[0])->num == -0.5,
               ((jsondata*)pos1->arr.at[1])->type == num,
               ((jsondata*)pos1->arr.at[1])->num == 12,
           });
    JSON_free(test);
}
