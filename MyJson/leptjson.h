#ifndef LEPTJSON_H__
#define LEPTJSON_H__

#include<assert.h>

typedef enum{ // json value types
    LEPT_NULL, LEPT_FALSE, LEPT_TRUE, LEPT_NUMBER, LEPT_STRING, LEPT_ARRAY, LEPT_OBJECT
} lept_type;

enum{ // error types
    LEPT_PARSE_OK = 0,
    LEPT_PARSE_EXPECT_VALUE,
    LEPT_PARSE_INVALID_VALUE,
    LEPT_PARSE_ROOT_NOT_SINGULAR,
    LEPT_PARSE_NUMBER_TOO_BIG,
    LEPT_PARSE_MISS_QUOTATION_MARK,
    LEPT_PARSE_INVALID_STRING_ESCAPE,
    LEPT_PARSE_INVALID_STRING_CHAR,
    LEPT_PARSE_INVALID_UNICODE_HEX,
    LEPT_PARSE_INVALID_UNICODE_SURROGATE,
    LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET,
    LEPT_PARSE_MISS_KEY,
    LEPT_PARSE_MISS_COLON,
    LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET
};

enum{ // stringify error enum
    LEPT_STRINGIFY_OK = 0
};

#define LEPT_KEY_NOT_EXIST ((size_t)-1)

// store json structure
typedef struct lept_value lept_value;
typedef struct lept_member lept_member;

struct lept_value{
    union{
        double num;
        struct{ lept_member* m; size_t size; } obj; // obj也是可以包含多个元素的
        struct{ char* s; size_t len;} str;
        struct{ lept_value* e; size_t size; } arr; //size: number of elements; lept_value: head of array
    } u;
    lept_type type;
};

struct lept_member{
    char* key; size_t keylen; // key name and key length
    lept_value val; // value
};

#define lept_init(v) do{ (v)->type = LEPT_NULL; } while(0)

int lept_parse(lept_value* v, const char* json);

//做的是一样的事，就是为了对外api的完整
#define lept_set_null(v) lept_free(v)

// const明确语义: 从v中取值，不改变v指向的内容
lept_type lept_get_type(const lept_value* v); 

int lept_get_bool(const lept_value* v);
void lept_set_bool(lept_value* v, int b);

double lept_get_number(const lept_value* v);
void lept_set_number(lept_value* v, double n);

const char* lept_get_string(const lept_value* v);
size_t lept_get_string_length(const lept_value* v);
void lept_set_string(lept_value* v, const char* s, size_t len);

size_t lept_get_array_size(const lept_value* v);
lept_value* lept_get_array_element(const lept_value* v, size_t index); // get element through index(随机存取)

size_t lept_find_object_index(const lept_value* v, const char* key, size_t klen);
lept_value* lept_find_object_value(const lept_value* v, const char* key, size_t klen);
size_t lept_get_object_size(const lept_value* v);
const char* lept_get_object_key(const lept_value* v, size_t index);
size_t lept_get_object_key_length(const lept_value* v, size_t index);
lept_value* lept_get_object_value(const lept_value* v, size_t index);

int lept_is_equal(const lept_value* lhs, const lept_value* rhs);
void lept_copy(lept_value* dst, const lept_value* src);
void lept_move(lept_value* dst, lept_value* src);
void lept_swap(lept_value* lhs, lept_value* rhs);
void lept_free(lept_value* v);
char* lept_stringify(const lept_value* v, size_t* length);

#endif