#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h> // maps malloc and free functions to their debug versions, track allocation and deallocation
#include<stdlib.h>
#include<string.h>
#include<math.h>
#include<errno.h>
#include "leptjson.h"



#ifndef LEPT_PARSE_STACK_INIT_SIZE // init capacity of stack
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

#define EXPECT(c,ch) do{assert(*(c->json++) == (ch));}while(0)
#define ISDIGIT(c) ((c)>='0' && (c)<='9')
#define ISDIGIT1TO9(c) ((c)>='1' && (c)<='9')
#define ISHEX(c) ( ((c)>='0' && (c)<='9') || ((c)>='A' && (c)<='F') || ((c)>='a' && (c)<='f'))
// PUTC(c, ch): push ch in c->stack，(char*)把void*转为字符指针类型
#define PUTC(c, ch) do{*(char *)lept_context_push(c, sizeof(char)) = (ch);}while(0)
#define STRING_ERROR(ret) do{c->top = head; return ret;}while(0)

typedef struct{
    const char* json;
    char* stack;
    size_t size, top; // size存储当前capacity，top存储当前栈实际占用大小
}lept_context; //这玩意是解析的时候用来缓冲的，不是对外的api

static void* lept_context_push(lept_context* c, size_t size){ //size是要放入的char的数量
    void* ret;
    if(size + c->top >= c->size){ // need dynamicly expand space
        if(c->size == 0)c->size = LEPT_PARSE_STACK_INIT_SIZE;
        while(size + c->top > c->size){
            c->size *= 1.5;
        }
        c->stack = (char*)realloc(c->stack, c->size); // 目前处理的是字符串
    }
    ret = c->stack + c->top; // ret return the pointer to push new values in
    c->top += size;
    return ret;
}

static void* lept_context_pop(lept_context* c, size_t size){
    assert(size <= c->top); // 保证弹出的size不大于当前的实际容量
    c->top -= size; // 提前将top移至弹出后的位置
    return c->stack + c->top; // 返回开始弹出的指针
}

static void lept_parse_whitespace(lept_context* c){
    const char* p = c->json;
    while(*p == ' '||*p=='\t'||*p=='\n'||*p=='\r'){
        p++;
    }
    c->json = p;
}

static int lept_parse_true(lept_context* c, lept_value* v){
    EXPECT(c, 't');
    v->type = LEPT_NULL;
    if(c->json[0]!='r'||c->json[1]!='u'||c->json[2]!='e'){
        return LEPT_PARSE_INVALID_VALUE;
    }
    c->json += 3;
    v->type = LEPT_TRUE;
    return LEPT_PARSE_OK;
}

static int lept_parse_false(lept_context* c, lept_value* v){
    EXPECT(c, 'f');
    v->type = LEPT_NULL;
    if(c->json[0]!='a'||c->json[1]!='l'||c->json[2]!='s'||c->json[3]!='e'){
        return LEPT_PARSE_INVALID_VALUE;
    }
    c->json += 4;
    v->type = LEPT_FALSE;
    return LEPT_PARSE_OK;
}

static int lept_parse_null(lept_context* c, lept_value* v){
    EXPECT(c, 'n');
    v->type = LEPT_NULL;
    if(c->json[0]!='u'||c->json[1]!='l'||c->json[2]!='l'){
        return LEPT_PARSE_INVALID_VALUE;
    }
    c->json += 3; // 'null' is parsed successfully
    v->type = LEPT_NULL;
    return LEPT_PARSE_OK;
}

static int lept_parse_number(lept_context* c, lept_value* v){
    const char* p = c->json;
    v->type = LEPT_NULL;
    // 需要筛选不符合JSON规范的格式
    if(*p == '-')p++;
    if(*p == '0')p++;
    else{
        if(!ISDIGIT1TO9(*p)) return LEPT_PARSE_INVALID_VALUE;
        for(p++;ISDIGIT(*p);p++); //第一个初始化语句'p++'只在初始执行一次
    }
    if(*p == '.'){
        p++;
        if(!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE; //小数点后必须有一位数字
        for(p++;ISDIGIT(*p);p++); //skip digits
    }
    if(*p == 'e' || *p=='E'){
        p++;
        if(*p=='+'||*p=='-')p++;
        if(!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE; //e后面跟一位数字
        for(p++;ISDIGIT(*p);p++);
    }
    errno = 0;
    v->u.num = strtod(c->json, NULL); //不需要记录end，因为上面校验的时候已经确定p为最终位置
    if(errno == ERANGE && (v->u.num == -HUGE_VAL || v->u.num == HUGE_VAL)) return LEPT_PARSE_NUMBER_TOO_BIG;
    c->json = p; //从c->json到p都已经校验过了，所以不需要额外的end指针
    v->type = LEPT_NUMBER;
    return LEPT_PARSE_OK;
}

static char* lept_parse_hex4(const char* p, unsigned* u){
    char* tmp = p;
    char* ptr;
    if(ISHEX(*p) && ISHEX(*(p+1)) && ISHEX(*(p+2)) && ISHEX(*(p+3)) && !ISHEX(*(p+4))){
        *u = strtol(p, &ptr, 16);
        return ptr;
    }
    else return NULL;
}

void lept_encode_utf8(lept_context* c, unsigned u){
    if(u <= 0x7F){
        PUTC(c, u & 0xFF);
    }
    else if(u <= 0x7FF){
        PUTC(c, 0xC0 | ((u >> 6) & 0xFF)); //这里0xFF是因为已经知道u<=0x7FF，右移6位即<0x1F,所以不用额外写成'& 0x1F'
        PUTC(c, 0x80 | (u        & 0x3F));
    }
    else if(u <= 0xFFFF){
        PUTC(c, 0xE0 | ((u >> 12) & 0xFF));
        PUTC(c, 0x80 | ((u >> 6)) & 0x3F);
        PUTC(c, 0x80 | (u         & 0x3F));
    }
    else if(u <= 0x10FFFF){
        PUTC(c, 0xF0 | ((u >> 18) & 0xFF));
        PUTC(c, 0x80 | ((u >> 12) & 0x3F));
        PUTC(c, 0x80 | ((u >> 6)) & 0x3F);
        PUTC(c, 0x80 | (u         & 0x3F));
    }
}

static int lept_parse_string_raw(lept_context* c, char** pstr, size_t* plen) {
    // parse string from c->json and pass head pointer of string to *str, pass string length to *len
    unsigned u, u2;
    size_t head = c->top; // 记录初始top位
    size_t len; // 字符串结束时，记录字符串长度（单位是字节）
    const char* p;
    EXPECT(c, '\"'); //保证是字符串
    p = c->json;
    for(;;){
        char ch = *p++;
        switch(ch){
            case '\"': // 字符串结束了，把缓冲堆栈里存储的字符串弹出来，设置到v中存储
                len = c->top - head;
                *plen = len;
                *pstr = (char*)lept_context_pop(c, len);
                c->json = p; //更新c中的json指针，后续继续读取其他元素
                return LEPT_PARSE_OK;
            case '\\': 
                switch(*p++) {
                    case 'u':
                        if(!(p=lept_parse_hex4(p, &u))) //读取第一个码点，合法则返回读完码点后的指针位置，非法则返回null
                            STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                        if(u >= 0xD800 && u <= 0xDBFF){ //u是高代理，后面应该有一个低代理
                            if(*p++ != '\\') STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            if(*p++ != 'u') STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            if(!(p=lept_parse_hex4(p, &u2))) //读取第二个码点
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                            if(u2 > 0xDFFF || u2 < 0xDC00)
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            u = ((u - 0xD800) << 10) + (u2 - 0xDC00) + 0x10000; //高代理低代理联合，计算出utf-8码点
                        }
                        lept_encode_utf8(c, u);
                        break;
                    case '\"': PUTC(c, '\"'); break;
                    case '\\': PUTC(c, '\\'); break;
                    case '/': PUTC(c, '/'); break;
                    case 'n': PUTC(c, '\n'); break;
                    case 'b': PUTC(c, '\b'); break;
                    case 'f': PUTC(c, '\f'); break;
                    case 'r': PUTC(c, '\r'); break;
                    case 't': PUTC(c, '\t'); break;
                    default: // invalid 转义字符
                        STRING_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE);
                }
                break; // case后面如果没有return，要记得break掉，否则会运行后面的case的语句
            case '\0': //还没读到右引号就结束了
                STRING_ERROR(LEPT_PARSE_MISS_QUOTATION_MARK);
            default: 
                if((unsigned char)ch < 0x20) { // 得到ch的unsigned char值，与0x20比较，过小的CHAR是非法的
                    STRING_ERROR(LEPT_PARSE_INVALID_STRING_CHAR);
                }
                PUTC(c, ch);
        }
    }
}

static int lept_parse_string(lept_context* c, lept_value* v){
    int ret;
    size_t len = 0;
    char* str;
    if( (ret = lept_parse_string_raw(c, &str, &len)) == LEPT_PARSE_OK ) // parse_string_raw获得字符串的长度和值信息
        lept_set_string(v, str, len); //用已有的set_string函数将已知值(起始指针)和长度的字符串置入v中
    return ret;
}

// forward declare: parse_array引用了parse_value, parse_value引用了parse_array
// 所以必须在定义parse_array前声明parse_value(或者定义parse_value前声明parse_array)
static int lept_parse_value(lept_context* c, lept_value* v);

static int lept_parse_array(lept_context* c, lept_value* v){
    size_t size = 0;
    int ret;
    EXPECT(c, '['); // start of array
    lept_parse_whitespace(c); // 空格
    if(*c->json == ']'){
        c->json++;
        v->type = LEPT_ARRAY;
        v->u.arr.size = 0;
        v->u.arr.e = NULL;
        return LEPT_PARSE_OK;
    }
    for(;;){
        lept_value e;
        lept_init(&e);
        if( ret = lept_parse_value(c, &e)){
            // ret == 0 为 LEPT_PARSE_OK，不为0代表出错，直接返回错误
            break;
        }
        // 解析完一个元素，把e的内容暂存进c
        memcpy(lept_context_push(c, sizeof(lept_value)), &e, sizeof(lept_value)); // copy e to temp stack in c->stack
        size++; // 数组元素数量 += 1
        lept_parse_whitespace(c); // whitespace before comma/bracket
        if(*c->json == ','){
            c->json++; // next element
            lept_parse_whitespace(c); // whitespace after comma
        }
        else if(*c->json == ']'){ //当前数组解析完毕，把c中暂存的内容弹出，memcpy到v
            c->json++;
            v->type = LEPT_ARRAY;
            v->u.arr.size = size;
            size *= sizeof(lept_value); // from num of elements -> size of memory
            v->u.arr.e = (lept_value*)malloc(size);
            memcpy(v->u.arr.e, lept_context_pop(c, size), size);
            return LEPT_PARSE_OK;
            // 这个分支是顺利完成解析的分支，已经将c中的内容正常全部取出交给v了
        }
        else{
            ret = LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
            break;
        }
    }
    // 对于所有没有顺利完成解析的分支，pop from c and free array elements that have already been pushed
    for(int i=0; i<size; i++){
        lept_free( (lept_value*)lept_context_pop(c, sizeof(lept_value)) );
    }
    return ret;
}

static int lept_parse_object(lept_context* c, lept_value* v){
    size_t size = 0;
    int ret;
    EXPECT(c, '{'); // start of array
    lept_parse_whitespace(c); // 空格
    if(*c->json == '}'){
        c->json++;
        v->type = LEPT_OBJECT;
        v->u.obj.size = 0;
        v->u.obj.m = NULL;
        return LEPT_PARSE_OK;
    }
    for(;;){
        lept_member m; // declare and init member
        m.key = NULL;
        m.keylen = 0;
        m.val = NULL;
        lept_value val; // declare and init value of key-value pair in member
        lept_init(&val);
        if(*c->json != '\"'){ // start of key
            ret = LEPT_PARSE_MISS_KEY;
            break;
        }
        char* tmp_key;
        if( ret = lept_parse_string_raw(c, &tmp_key, &m.keylen)){ // parse key
            // ret == 0 为 LEPT_PARSE_OK，不为0代表出错，直接返回错误
            break;
        }
        // 开辟一块内存存放key，指针交给m.key
        // 为什么不直接m.key = &val? 因为val是for循环内的局部变量，创建在栈上，进入下一个循环之后就会失效
        m.key = (char*)malloc(m.keylen + 1);
        memcpy(m.key, tmp_key, m.keylen);
        m.key[m.keylen] = '\0';

        lept_parse_whitespace(c); //ws before and after ':'
        if(*c->json++ != ':'){
            ret = LEPT_PARSE_MISS_COLON;
            break;
        }
        lept_parse_whitespace(c);

        if( (ret = lept_parse_value(c, &val)) ){
            free(m.key);
            lept_free(&val);
            break;
        }
        // 开辟一块内存存放val，指针交给m.val, 因为val是局部变量
        m.val = (lept_value*)malloc(sizeof(lept_value));
        memcpy(m.val, &val, sizeof(lept_value)); 
        // 解析一个member, 暂存进c
        memcpy(lept_context_push(c, sizeof(lept_member)), &m, sizeof(lept_member)); // copy e to temp stack in c->stack
        size++; // 数组元素数量 += 1
        lept_parse_whitespace(c); // whitespace before comma/bracket
        if(*c->json == ','){
            c->json++; // next element
            lept_parse_whitespace(c); // whitespace after comma
        }
        else if(*c->json == '}'){ //当前数组解析完毕，把c中暂存的内容弹出，memcpy到v
            c->json++;
            v->type = LEPT_OBJECT;
            v->u.obj.size = size;
            size *= sizeof(lept_member); // from num of elements -> size of memory
            v->u.obj.m = (lept_member*)malloc(size);
            memcpy(v->u.obj.m, lept_context_pop(c, size), size);
            return LEPT_PARSE_OK;
            // 这个分支是顺利完成解析的分支，已经将c中的内容正常全部取出交给v了
        }
        else{
            ret = LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET;
            break;
        }
    }
    // 对于所有没有顺利完成解析的分支，pop from c and free array elements that have already been pushed
    for(int i=0; i<size; i++){
        lept_free( ( (lept_member*)lept_context_pop(c, sizeof(lept_member)) )->val );
    }
    return ret;
}

static int lept_parse_value(lept_context* c, lept_value* v){
    switch(*c->json){
        case 'n': return lept_parse_null(c, v);
        case 'f': return lept_parse_false(c, v);
        case 't': return lept_parse_true(c, v);
        case '\"': return lept_parse_string(c, v);
        case '[': return lept_parse_array(c, v);
        case '{': return lept_parse_object(c, v);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
        default: return lept_parse_number(c, v);
    }
}

int lept_parse(lept_value* v, const char* json){ // v: store json value; json: json to be read;
    int ret = -1;
    lept_context c; // lept_context: store JSONs to be unpacked
    assert(v!=NULL);
    
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    
    lept_init(v); // initialize
    lept_parse_whitespace(&c);
    if((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK){ // 赋值的优先级低于条件判断，所以用括号
        lept_parse_whitespace(&c);
        if(*(c.json) != '\0'){
            v->type = LEPT_NULL;
            return LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    c.size = 0;
    c.top = 0;

    assert(c.top == 0); // make sure stack is poped empty and top指针正确归零
    free(c.stack);
    return ret;
}

lept_type lept_get_type(const lept_value* v){
    assert(v != NULL);
    return v->type;
}

double lept_get_number(const lept_value* v){
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->u.num;
}

void lept_set_number(lept_value* v, double n){
    assert(v!=NULL);
    v->u.num = n;
    v->type = LEPT_NUMBER;
}

int lept_get_bool(const lept_value* v){
    assert(v != NULL && (v->type == LEPT_TRUE || v->type == LEPT_FALSE));
    return v->type == LEPT_FALSE ? 0 : 1;
}

void lept_set_bool(lept_value* v, int b){
    assert(v != NULL);
    v->type = b==0 ? LEPT_FALSE : LEPT_TRUE;
}

const char* lept_get_string(const lept_value* v){
    assert(v!=NULL && v->type == LEPT_STRING);
    return v->u.str.s;
}

size_t lept_get_string_length(const lept_value* v){
    assert(v!=NULL && v->type == LEPT_STRING);
    return v->u.str.len;
}

void lept_set_string(lept_value* v, const char* s, size_t len){
    assert(v!=NULL && (s!=NULL || len==0)); // len为0是可以接受的
    lept_free(v); // 先释放原本指向的内容，然后再把字符串复制进去
    v->u.str.len = len;
    v->u.str.s = (char*)malloc(len+1); // 额外一位给'\0'
    memcpy(v->u.str.s, s, len); // 用memcpy把字符串拷贝进lept_value* v，而不是赋值（s是函数的输入，搞不好就在外面被free掉了）
    v->u.str.s[len] = '\0'; // 手动添加结束标志
    v->type = LEPT_STRING;
}

size_t lept_get_array_size(const lept_value* v){
    assert(v != NULL && v->type == LEPT_ARRAY);
    return v->u.arr.size;
}

lept_value* lept_get_array_element(const lept_value* v, size_t index){ // get element through index(随机存取)
    assert(v != NULL && v->type == LEPT_ARRAY);
    assert(index < v->u.arr.size); //假设给定的都是合法的下标
    return &(v->u.arr.e[index]); //v->u.a.e是数组头指针，用index访问其中第index个元素指向的内容
}

size_t lept_get_object_size(const lept_value* v){
    assert(v != NULL && v->type == LEPT_OBJECT);
    return v->u.obj.size;
}

const char* lept_get_object_key(const lept_value* v, size_t index){
    assert(v != NULL && v->type == LEPT_OBJECT && index < v->u.obj.size);
    return v->u.obj.m[index].key;
}

size_t lept_get_object_key_length(const lept_value* v, size_t index){
    assert(v != NULL && v->type == LEPT_OBJECT && index < v->u.obj.size);
    return v->u.obj.m[index].keylen;
}

lept_value* lept_get_object_value(const lept_value* v, size_t index){
    assert(v != NULL && v->type == LEPT_OBJECT && index < v->u.obj.size);
    return v->u.obj.m[index].val;
}

void lept_free(lept_value* v){ //这个free函数是要被各种lept_set_xx使用的
    assert(v!=NULL);
    if(v->type == LEPT_STRING){ //如果是字符串，就释放字符串内容
        v->u.str.len = 0;
        free(v->u.str.s);
    }
    // if(v->type == LEPT_ARRAY){ // 由于数组可能存在嵌套结构，对ARRAY的释放不能简单写成这样
    //     v->u.a.size = 0;
    //     free(v->u.a.e);
    // }
    if(v->type == LEPT_ARRAY){
        for(int i=0;i<v->u.arr.size;i++){
            lept_free( &(v->u.arr.e[i]) ); //给定一个lept_value*，对其中的内容进行递归的lept_free，也就是说free掉的是&(v->u.a.e[i])->u.a.e(套娃)
        }
        free(v->u.arr.e); //&(v->u.a.e[0])不就是v->u.a.e, 不会重复free吗？注意, 上面的lept_free()和这里的free()做的不是一件事
        // 上面的free函数释放的是以v->u.a.e开头的一块存放lept_value的chunk，这些内存中可能嵌套存在(v->u.a.e[i]).u.a.e或者.u.s.s等需要释放的内容
        // 所以在free(v->u.a.e)之前，需要先递归地调用lept_free()把chunk内的嵌套结构都释放完，最后再释放v->u.a.e本身
        v->u.arr.size = 0;
    }
    if(v->type == LEPT_OBJECT){
        for(int i=0;i<v->u.obj.size;i++){
            lept_free( v->u.obj.m[i].val ); // 递归地free掉lept_value类型的value
            free(v->u.obj.m[i].key); // free掉key字符串
            v->u.obj.m[i].keylen = 0;
        }
    }
    v->type = LEPT_NULL;
}