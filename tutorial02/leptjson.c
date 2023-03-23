#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <stdlib.h>  /* NULL, strtod() */
#include <math.h>
#include <errno.h>

#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)

typedef struct {
    const char* json;
}lept_context;

static void lept_parse_whitespace(lept_context* c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}

static int lept_parse_literal(const char* literal, lept_type type, lept_context* c, lept_value* v) {
    EXPECT(c, *literal);
    literal++; // in EXPECT(C, *literal) c->json performed a '++' operation
    while(*literal != '\0'){
        if(*c->json != *literal){
            return LEPT_PARSE_INVALID_VALUE;
        }
        literal++;
        c->json++;
    }
    v->type = type;
    return LEPT_PARSE_OK;
}

// static int lept_parse_true(lept_context* c, lept_value* v) {
//     EXPECT(c, 't');
//     if (c->json[0] != 'r' || c->json[1] != 'u' || c->json[2] != 'e')
//         return LEPT_PARSE_INVALID_VALUE;
//     c->json += 3;
//     v->type = LEPT_TRUE;
//     return LEPT_PARSE_OK;
// }

// static int lept_parse_false(lept_context* c, lept_value* v) {
//     EXPECT(c, 'f');
//     if (c->json[0] != 'a' || c->json[1] != 'l' || c->json[2] != 's' || c->json[3] != 'e')
//         return LEPT_PARSE_INVALID_VALUE;
//     c->json += 4;
//     v->type = LEPT_FALSE;
//     return LEPT_PARSE_OK;
// }

// static int lept_parse_null(lept_context* c, lept_value* v) {
//     EXPECT(c, 'n');
//     if (c->json[0] != 'u' || c->json[1] != 'l' || c->json[2] != 'l')
//         return LEPT_PARSE_INVALID_VALUE;
//     c->json += 3;
//     v->type = LEPT_NULL;
//     return LEPT_PARSE_OK;
// }

#define LEPT_IS_DIGIT(c)    (*(c)>='0' && *(c)<='9')
#define LEPT_TO_NEXT_NONDIGIT(c) do{ assert(LEPT_IS_DIGIT(c)==0);\
                                 c++;\
                                 if(LEPT_IS_DIGIT(c)==0)return LEPT_PARSE_INVALID_VALUE;\
                                 while(LEPT_IS_DIGIT(c)==1){c++;} }while(0)

static int lept_parse_number(lept_context* c, lept_value* v) {
    char* end;
    /* \TODO validate number */
    // 在哪些情况下是invalid_value错误，哪些情况下是root_not_singular?
    // strtod()对于'.123' '00123' '123.'都是能输出结果的，但是JSON规则下'.123'和'123.'都是非法，'00123'只能读入0，后面的属于多余输入
    const char* tmp = c->json;
    if(*tmp != '-' && !LEPT_IS_DIGIT(tmp)){
        return LEPT_PARSE_INVALID_VALUE;
    }
    if(*tmp=='-') tmp++;
    if(LEPT_IS_DIGIT(tmp)==0)return LEPT_PARSE_INVALID_VALUE;
    if(*tmp == '0' && *(tmp+1)!='.' && *(tmp+1)!='\0'){ // json rule: start with 0: either follow with a '.' or '\0'...
        // 前导0是不合法的，比如'0123'，按照json规则应该是只读入第一个0，视作number==0的lept_value, 但是strtod()会无视前导0直接读完123
        // 所以碰到前导0，提前返回对应错误退出
        return LEPT_PARSE_ROOT_NOT_SINGULAR; // otherwise strtod() will read only '0', and rest of json regarded as 'root not singular'.
    }
    while(LEPT_IS_DIGIT(tmp)){tmp++;}
    // ENCOUNTER '.' OR 'e'/'E' OR '\0'
    // if '.', check if there is digit after '.'
    if(*tmp=='.'){ // read digits after first '.'
        // tmp++;
        if(!LEPT_IS_DIGIT(tmp+1)) return LEPT_PARSE_INVALID_VALUE; // at least a digit after '.'
        //注意用宏替代函数的可能问题：首先++,--肯定不能用作宏函数的输入，然后输入如果带有+1,-1这种计算，加括号以尽可能避免运算符优先级的问题
    }
    // // ENCOUNTER 'E'/'e'
    // if(*tmp=='E' || *tmp=='e'){
    //     if(*(tmp+1)=='+' || *(tmp+1)=='-')tmp++; //skip 正负符号 for one time
    //     LEPT_TO_NEXT_NONDIGIT(tmp); // read digits after 'e'
    //     if(*tmp!='.' && *tmp!='\0')return LEPT_PARSE_ROOT_NOT_SINGULAR;
    //     if(*tmp=='.'){
    //         LEPT_TO_NEXT_NONDIGIT(tmp);
    //     }
    // }

    double d = strtod(c->json, &end);
    // strtod: 将第一个形参（const char*, 字符串）里的数字读成double并返回，第二个形参(char** 类型)接收剩余部分的首字符指针
    if((d == HUGE_VAL || d == -HUGE_VAL) && errno==ERANGE)
        return LEPT_PARSE_NUMBER_TOO_BIG;
    v->n = d;
    if (c->json == end) // no number read
        return LEPT_PARSE_INVALID_VALUE;
    c->json = end;
    v->type = LEPT_NUMBER;
    return LEPT_PARSE_OK;
}

static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 't':  return lept_parse_literal("true", LEPT_TRUE, c, v);
        case 'f':  return lept_parse_literal("false", LEPT_FALSE, c, v);
        case 'n':  return lept_parse_literal("null", LEPT_NULL, c, v);
        default:   return lept_parse_number(c, v);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
    }
}

int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    int ret;
    assert(v != NULL);
    c.json = json;
    v->type = LEPT_NULL;
    lept_parse_whitespace(&c);
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    return ret;
}

lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

double lept_get_number(const lept_value* v) {
    // can get number if and only if v->type == LEPT_NUMBER
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->n;
}
