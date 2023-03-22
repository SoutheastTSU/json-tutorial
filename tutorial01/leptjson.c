#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <stdlib.h>  /* NULL */

// assert *(c->json) == ch, and c->json move to next place
#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)
// do{...} while(0): 把要执行的语句放到花括号里，可以避免宏定义展开引发的各种错误

typedef struct {
    const char* json;
}lept_context;

static void lept_parse_whitespace(lept_context* c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;  // the first non-whitespace charactor
}

static int lept_parse_null(lept_context* c, lept_value* v) {
    EXPECT(c, 'n'); // 保证c->json指针指向的字符为'n', 同时c->json往后移一位
    if (c->json[0] != 'u' || c->json[1] != 'l' || c->json[2] != 'l')
        return LEPT_PARSE_INVALID_VALUE;
    c->json += 3; //成功读入'null'
    v->type = LEPT_NULL;
    return LEPT_PARSE_OK;
}

static int lept_parse_true(lept_context* c, lept_value* v) {
    EXPECT(c, 't');
    // what if c->json[2] causes index out of range? -> this would not happen because '\0' will be read and terminate the conditioning
    // const char* 的最后一位必然是'\0'，因此在读到'\0'时必定会触发不等于给定字符的条件，由于后面都是'||'运算，所以后面的语句都不会被执行（因为执行了也不影响结果）
    if(c->json[0] != 'r' || c->json[1] != 'u' || c->json[2] != 'e') {
        return LEPT_PARSE_INVALID_VALUE;
    }
    c->json += 3;
    v->type = LEPT_TRUE;
    return LEPT_PARSE_OK;
}

static int lept_parse_false(lept_context* c, lept_value* v) {
    EXPECT(c, 'f');
    if(c->json[0] != 'a' || c->json[1] != 'l' || c->json[2] != 's' || c->json[3] != 'e'){
        return LEPT_PARSE_INVALID_VALUE;
    }
    c->json += 4;
    v->type = LEPT_FALSE;
    return LEPT_PARSE_OK;
}

static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 'n':  return lept_parse_null(c, v);    // after whitespace came 'n', try parse null
        case 't':  return lept_parse_true(c, v);
        case 'f':  return lept_parse_false(c, v); 
        case '\0': return LEPT_PARSE_EXPECT_VALUE;  // nothing but whitespaces, need value
        default:   return LEPT_PARSE_INVALID_VALUE; // invalid value
    }
}

int lept_parse(lept_value* v, const char* json) {
    // valid struct of json-test: (assume only null type) "ws null ws" 
    lept_context c;
    assert(v != NULL);
    c.json = json;
    v->type = LEPT_NULL;
    lept_parse_whitespace(&c); // eliminate ws at the front
    int status = lept_parse_value(&c, v);
    if(status == LEPT_PARSE_OK){
        lept_parse_whitespace(&c); // eliminate ws after value
        if(*c.json == '\0'){ // end of string
            return status;
        }
        else{ // something that is not whitespace before end of string
            return LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    else return status; // parse 'null'
}

lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}
