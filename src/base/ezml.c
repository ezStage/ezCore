#include <ezCore/ezCore.h>

/*#################################################################################*/

#define WORD_ERROR		0x7f
#define WORD_EOF		0x0
#define WORD_INT		0x1
#define WORD_FLOAT		0x2
#define WORD_STRING		0x3
#define WORD_FOURCC		0x4
#define WORD_KEY		0x5
#define WORD_KEY_NONE	0x6

#define WORD_EQUAL		0xa	/*等号*/
#define WORD_COMMA		0xb	/*逗号*/
#define WORD_DOT		0xc	/*.*/

#define WORD_BRACE_0	0x11
#define WORD_BRACE_1	0x12
#define WORD_SQUARE_0	0x13
#define WORD_SQUARE_1	0x14

#define STR_MEM_N	26
typedef struct {
	char *str;
	intptr_t n;
	double f;
	ezFourCC fourcc; /*@xxxx*/
	int32_t str_n;
	int32_t str_mem_n;
	char str_mem[STR_MEM_N];
} Parse_word;

#define PARSE_BUF_SIZE	2000
typedef struct {
	char *p; //当前解析位置
	char *end; //当前buf最后有效数据的后一位
	int fd; //input
	int32_t line_i; //当前行号
	ezBool eof;
	ezBool err;
	char buf[1];
} Parse_ctx;
static Parse_ctx * parse_file_new(const char *file);
static Parse_ctx * parse_string_new(const char *str, int len);

static void _parse_ctx_destroy(void *p)
{
	Parse_ctx *ctx = (Parse_ctx *)p;
	if(ctx->fd != -1) {
		close(ctx->fd);
	}
	return;
}

static ezGC_type _PARSE_CTX_TYPE = {
	.destroy_func = _parse_ctx_destroy,
};

static Parse_ctx * parse_file_new(const char *file)
{
	int n;
	int fd;
	fd = open(file, O_RDONLY);
	if(fd == -1) {
		ezLog_err("open file %s error\n", file);
		ezLog_errno("open\n");
		return NULL;
	}
	Parse_ctx *ret = ezAlloc(sizeof(Parse_ctx)+PARSE_BUF_SIZE, &_PARSE_CTX_TYPE, FALSE);
	ret->fd = fd;
	ret->line_i = 1;
	ret->err = 0;
	ret->eof = FALSE;
	n = ez_read(fd, ret->buf, PARSE_BUF_SIZE);
	if(n <= 0) {
		n = 0;
		ret->eof = TRUE;
	}
	ret->p = ret->buf;
	ret->end = ret->buf+n;
	ret->buf[n] = '\0'; /*一定要有''\0'结束*/

	//UTF-8 texts may start with a 3-byte "BOM"
	if(!memcmp(ret->p, "\xef\xbb\xbf", 3)) {
		ret->p += 3;
	}
	return ret;
}

static Parse_ctx * parse_string_new(const char *str, int len)
{
	if(str == NULL) return NULL;
	if(len == -1) len = strlen(str);
	if(len <= 0) return NULL;
	Parse_ctx *ret = ezAlloc(sizeof(Parse_ctx)+len, &_PARSE_CTX_TYPE, FALSE);
	ret->fd = -1;
	ret->line_i = 1;
	ret->p = ret->buf;
	ret->end = ret->buf+len;
	ret->eof = TRUE;
	ret->err = 0;
	memcpy(ret->buf, str, len);
	ret->buf[len] = '\0'; /*一定要有''\0'结束*/

	//UTF-8 texts may start with a 3-byte "BOM"
	if(!memcmp(ret->p, "\xef\xbb\xbf", 3)) {
		ret->p += 3;
	}
	return ret;
}

static ezBool _read_buf(Parse_ctx*ctx) //返回是否读入新数据
{
	int n;
	if(ctx->eof) return FALSE;
	n = ez_read(ctx->fd, ctx->buf, PARSE_BUF_SIZE);
	if(n > 0) {
		ctx->p = ctx->buf;
		ctx->end = ctx->buf+n;
		ctx->buf[n] = '\0';
		return TRUE;
	}
	ctx->eof = TRUE;
	return FALSE;
}

/*===================================================*/
#define C_IS_BLANK(c) ((c) > '\0' && (c) <= ' ') /* 0 < c <= 32 */
#define C_IS_NEWLINE(c) ((c)=='\n' || (c)=='\r')

#define C_IS_LABEL_HEAD(c) (((c)>='A' && (c)<='Z')||((c)>='a' && (c)<='z')||((c)=='_'))
#define C_IS_LABEL(c) (((c)>='A' && (c)<='Z')||((c)>='a' && (c)<='z')||((c)=='_')||((c)>='0' && (c)<='9'))

static int16_t _get_escape_char(char *p, char **pend)
{
	int16_t ret = -1;
	switch(*p++)
	{
	case 'a': ret='\a'; break;
	case 'n': ret='\n'; break;
	case 't': ret='\t'; break;
	case 'b': ret='\b'; break;
	case 'r': ret='\r'; break;
	case 'f': ret='\f'; break;
	case 'v': ret='\v'; break;
	case '\\': ret='\\'; break;
	case '\'': ret='\''; break;
	case '\"': ret='\"'; break;
	case '0': ret=0; break;
	case 'x': {
		uint8_t c = *p++;
		if((c >= '0')&&(c <= '9')) ret = c - '0';
		else if((c >= 'A')&&(c <= 'F')) ret = c - 'A' + 10;
		else if((c >= 'a')&&(c <= 'f')) ret = c - 'a' + 10;
		else break;

		c = *p++; ret <<= 4;
		if((c >= '0')&&(c <= '9')) ret += c - '0';
		else if((c >= 'A')&&(c <= 'F')) ret += c - 'A' + 10;
		else if((c >= 'a')&&(c <= 'f')) ret += c - 'a' + 10;
		else ret = -1;
		}
		break;
	}
	*pend = (ret != -1)? p : p-1;
	return ret;
}

static uint16_t _parse_digit(char *p, char **pend, Parse_word *word)
{
	char *end;
	word->n = strtol(p, &end, 0);
	if((*end == '.')||(*end == 'e')||(*end == 'E')) {
		word->f = strtod(p, &end);
		*pend = end;
		if(p == end) return WORD_ERROR;
		return WORD_FLOAT;
	}
	*pend = end;
	if(p == end) return WORD_ERROR;
	return WORD_INT;
}

static uint16_t _parse_fourcc(char *p, char **pend, Parse_word *word)
{
	char *e = p;
	char c, c1=0,c2=0,c3=0,c4=0;
	c = *++e;
	if(C_IS_LABEL(c)) {
		c1 = c;
		c = *++e;
		if(C_IS_LABEL(c)) {
			c2 = c;
			c = *++e;
			if(C_IS_LABEL(c)) {
				c3 = c;
				c = *++e;
				if(C_IS_LABEL(c)) {c4 = c; c=*++e;}
			}
		}
	}
	if(C_IS_LABEL(c)) {
		*pend = p; /*这是出错情况, 退回到@*/
		return WORD_ERROR;
	}
	word->fourcc = EZ_FOURCC(c1,c2,c3,c4);
	return WORD_FOURCC;
}

/*===================================================*/
/*除非后面没有数据了(ctx->eof), 保证剩余字符至少有len个,
len应该小于PARSE_BUF_SIZE*/
/*注意因为语法设计上没有回溯, 所以把buf剩下的内容拷贝到前面时,
不用保留前面已经遍历过的字节!*/
static char * _check_word_buf(char *p, int len, Parse_ctx*ctx)
{
	int n = ctx->end - p;
	if(!ctx->eof && n<len)
	{/*没有eof, ctx->buf有效*/
		memmove(ctx->buf, p, n);
		p = ctx->buf;
		ctx->p = p;
		ctx->end = p+n;
		n = ez_read(ctx->fd, ctx->end, PARSE_BUF_SIZE-n);
		if(n > 0) {
			ctx->end += n;
		} else {
			ctx->eof = TRUE;
		}
		*ctx->end = '\0';
	}
	return p;
}

/*c是 \n 或 \r 中的一个, p是后一个字符位置*/
static char * _skip_one_newline(char c, char *p, Parse_ctx *ctx)
{
	++ ctx->line_i;
	p = _check_word_buf(p, 2, ctx);
	while(C_IS_NEWLINE(*p) && (*p != c)) ++p;
	return p;
}

/*===================================================*/

static inline void _word_init(Parse_word *wd)
{
	wd->str = wd->str_mem;
	wd->str_mem_n = STR_MEM_N;
}

static inline void _word_reset(Parse_word *wd)
{
	if(wd->str_mem_n > STR_MEM_N) {
		free(wd->str);
		wd->str = wd->str_mem;
		wd->str_mem_n = STR_MEM_N;
	}
	wd->str_n = 0;
	wd->fourcc = 0;
}

static void _word_add_str(Parse_word *wd, char c)
{
	int m = wd->str_mem_n;
	if(wd->str_n >= m)
	{
		char *p;
		if(m <= STR_MEM_N) {
			m = (STR_MEM_N<<4); /*(STR_MEM_N*16)*/
			p = malloc(m);
		} else {
			m <<= 1;
			p = realloc(wd->str, m);
		}
		if(p == NULL) {
			ezLog_err("ezml parse add str: no memory\n");
			return;
		}
		wd->str = p;
		wd->str_mem_n = m;
	}
	wd->str[wd->str_n++] = c;
	return;
}

uint16_t parse_get_word(Parse_ctx*ctx, Parse_word *wd)
{
	uint16_t type, status; /*1:单引号，2:双引号，3:key label串，4:行注释*/
	char c, *p;

	_word_reset(wd);

	if(ctx->err) return WORD_ERROR;

	type = WORD_EOF;
	status = 0;

	p = ctx->p;
	do {
		if(p == ctx->end) {
			if(!_read_buf(ctx)) {
				if(status == 4) type = WORD_STRING;
				break;
			}
			p = ctx->p;
		}
		c = *p++;
		switch(status)
		{
		case 0:
			if(C_IS_BLANK(c)) continue;
/*---------------------------------------------------*/
			switch(c)
			{
			case '\'': //字符串1
				status = 1;
				break;
			case '\"': //字符串2
				status = 2;
				break;
			case '#': //行注释
				status = 4;
				break;
			case '^':
				type = WORD_KEY_NONE;
				break;
			case '{':
				type = WORD_BRACE_0;
				break;
			case '}':
				type = WORD_BRACE_1; /*dict没有FOUR*/
				break;
			case '[':
				type = WORD_SQUARE_0;
				break;
			case ']':
				type = WORD_SQUARE_1;
				break;
			case '@':
				p = _check_word_buf(p-1, 5, ctx);
				type = _parse_fourcc(p, &p, wd);
				break;
			case ',':
				type = WORD_COMMA;
				break;
			case '=':
				type = WORD_EQUAL;
				break;
			case '.':
				type = WORD_DOT;
				break;
			default:
				if(C_IS_LABEL_HEAD(c)) {
					_word_add_str(wd, c);
					status = 3; //key Label串
					break;
				}
				if(((c>='0' && c<='9'))||(c=='+')||(c=='-')) { //数字
					p = _check_word_buf(p-1, 128, ctx); //足够大的数字了
					type = _parse_digit(p, &p, wd);
					break;
				}
				if(C_IS_NEWLINE(c)) {
					p = _skip_one_newline(c, p, ctx);
					break;
				}
				type = WORD_ERROR;
				break;
			}
/*---------------------------------------------------*/
			break;
		case 1: //字符串1中
		case 2: //字符串2中
			if(C_IS_NEWLINE(c)) {
				p = _skip_one_newline(c, p, ctx);
				_word_add_str(wd, '\n');
				break;
			}
			if(c == '\'' && status == 1) {
				type = WORD_KEY;
				break;
			}
			if(c == '\"' && status == 2) {
				type = WORD_STRING;
				break;
			}
			if(c == '\\') {
				p = _check_word_buf(p, 4, ctx);
				int16_t esc = _get_escape_char(p, &p);
				if(esc == -1) {
					type = WORD_ERROR;
					break;
				}
				c = (char)esc;
			}
			_word_add_str(wd, c);
			break;
		case 3: //key中
			if(!C_IS_LABEL(c)) {
				--p;
				type = WORD_KEY;
				break;
			}
			_word_add_str(wd, c);
			break;
		case 4: //行注释中
			if(C_IS_NEWLINE(c)) {
				p = _skip_one_newline(c, p, ctx);
				status = 0;
			}
			break;
		}
	} while(type == WORD_EOF); //还没有识别结果
	ctx->p = p;

	if(type == WORD_ERROR)
		ctx->err = TRUE;

	return type;
}

void parse_print(Parse_ctx*ctx)
{
	char *e = ctx->p;
	while(e < ctx->end) {
		if(C_IS_NEWLINE(*e)) break;
		++e;
	}
	ezLog_print("\n line %d: %.*s\n", ctx->line_i, (int)(e-ctx->p), ctx->p);
}

ezBool parse_check_label(const char *str, intptr_t len)
{
	char c;
	const char *e;
	if(str == NULL || len <= 0) return FALSE;
	e = str+len;
	c = *str++;
	if(!C_IS_LABEL_HEAD(c)) return FALSE;
	while(str<e) {
		c = *str++;
		if(!C_IS_LABEL(c)) return FALSE;
	}
	return TRUE;
}

/*===================================*/

void parse_dump_word(uint16_t wt, Parse_word *wd)
{
	switch(wt)
	{
	case WORD_ERROR:
		ezLog_print("<WORD_ERROR>\n");
		break;
	case WORD_EOF:
		ezLog_print("<WORD_EOF>\n");
		break;
	case WORD_INT:
		ezLog_print("%zd\n", wd->n);
		break;
	case WORD_FLOAT:
		ezLog_print("%f\n", wd->f);
		break;
	case WORD_STRING:
		ezLog_print("\"%.*s\"\n", wd->str_n, wd->str);
		break;
	case WORD_KEY:
		ezLog_print("%.*s", wd->str_n, wd->str);
		break;
	case WORD_KEY_NONE:
		ezLog_print("^");
		break;
	case WORD_BRACE_0:
		ezLog_print("{\n");
		break;
	case WORD_BRACE_1:
		ezLog_print("}\n");
		break;
	case WORD_SQUARE_0:
		ezLog_print("[");
		break;
	case WORD_SQUARE_1:
		ezLog_print("]\n");
		break;
	case WORD_FOURCC:
		ezLog_print("@%.4s\n", (char *)&(wd->fourcc));
		break;
	case WORD_EQUAL:
		ezLog_print("=");
		break;
	case WORD_DOT:
		ezLog_print(".");
		break;
	case WORD_COMMA:
		ezLog_print(",\n");
		break;
	default:
		ezLog_err("word type\n");
		break;
	}
	fflush(stdout);
}

#if 0
void parse_dump_ctx(Parse_ctx *ctx)
{
	uint16_t wt;
	Parse_word wd;
	while(1)
	{
		wt = parse_get_word(ctx, &wd);
		parse_dump_word(wt, &wd);
		if(wt == WORD_EOF) break;
		if(wt == WORD_ERROR) {
			parse_print(ctx);
			break;
		}
	}
	return;
}
#endif

static ezDict * _node_get_dict(ezDictNode *tn)
{
	ezDict *sub;
	sub = ezDictNode_get_dict(tn, FALSE);
	if(sub != NULL) return sub;

	sub = ezDict_create();
	if(sub == NULL) return NULL;
	if(ezDictNode_set_dict(tn, sub) == EZ_SUCCESS)
		return sub;
	return NULL;
}

/*
{ }中不能嵌套出现[ ]
[key] 表示相对于起始dict(绝对寻址)创建子dict,
并且后面的内容都在该子dict中创建, 除非再次遇到[ ].

first[level] 表示当前赋值开始时的dict,
新创建的cur_node总是从first开始, 因为有多级key,
所以cur_node不一定是first的直接子节点.

每次赋值结束后, 新的cur_node再从first开始.
{ }结束当前first, 回到上一层first
*/

#define S_BEGIN		0x01 /*开始, 前面清空*/
#define S_KEY		0x02 /*前面是key或^*/
#define S_DOT		0x08 /*前面是.*/
#define S_EQUAL		0x10 /*前面是=*/
#define S_VALUE		0x20 /*前面是value*/

#define LEVEL_MAX	1000
static ezResult _parse_ezml(ezDict *top, Parse_ctx *ctx)
{
	uint8_t status=S_BEGIN;
	ezBool in_square=FALSE; /* 是否在 [ ] 中*/
	int16_t level=0; /* { } 的层次 */

	ezDict * tmp_dict;
	ezDictNode *cur_dn=NULL;

	uint16_t wt;
	Parse_word wd;
	_word_init(&wd);

	ezDict *firsts[LEVEL_MAX+1];
	firsts[0] = top; //level = 0

	while(1)
	{
		wt = parse_get_word(ctx, &wd);
		switch(wt)
		{
		case WORD_SQUARE_0:
			if(level > 0) break; //{}里面不能嵌套[], 即[]只能在第0层
			if(in_square) break;
			if(status & (S_DOT|S_EQUAL)) break;
			in_square = TRUE;
			status = S_BEGIN;
			firsts[0] = top; //[]里面定位总是从top开始(绝对定位), 这时level为0
			cur_dn = NULL;
			continue;

		case WORD_SQUARE_1:
			if(!in_square) break;
			if(status & (S_DOT|S_EQUAL|S_VALUE)) break;
			if(status == S_KEY) {
				tmp_dict = _node_get_dict(cur_dn);
				if(tmp_dict == NULL) break;
				firsts[0] = tmp_dict; //这时level为0
			} //else first[0] 不变
			in_square = FALSE;
			cur_dn = NULL;
			status = S_BEGIN;
			continue;

		case WORD_KEY:
		case WORD_KEY_NONE:
			if(status & (S_KEY|S_EQUAL)) break;
			if(status == S_DOT) { //前面有cur_dn
				ezDict * tmp_d = _node_get_dict(cur_dn);
				if(tmp_d == NULL) break;
				if(wt == WORD_KEY)
					cur_dn = ezDict_get_key_node(tmp_d, wd.str, wd.str_n, TRUE);
				else /*WORD_KEY_NONE*/
					cur_dn = ezDict_add_nokey_node(tmp_d);
			} else { //从first开始
				if(wt == WORD_KEY)
					cur_dn = ezDict_get_key_node(firsts[level], wd.str, wd.str_n, TRUE);
				else /*WORD_KEY_NONE*/
					cur_dn = ezDict_add_nokey_node(firsts[level]);
			}
			if(cur_dn == NULL) break;
			status = S_KEY;
			continue;

		case WORD_DOT:
			if(status != S_KEY) break;
			status = S_DOT;
			continue;

		case WORD_EQUAL:
			if(in_square) break;
			if(status != S_KEY) break;
			status = S_EQUAL;
			continue;

		case WORD_INT:
		case WORD_FLOAT:
		case WORD_STRING:
			if(in_square) break;
			if(status & (S_DOT|S_KEY)) break;
			if(status & (S_BEGIN|S_VALUE)) {
				cur_dn = ezDict_add_nokey_node(firsts[level]);
			}
			if(cur_dn == NULL) break;
			if(wt == WORD_INT) {
				if(ezDictNode_set_int(cur_dn, wd.n) != EZ_SUCCESS) break;
			} else if(wt == WORD_FLOAT) {
				if(ezDictNode_set_float(cur_dn, wd.f) != EZ_SUCCESS) break;
			} else {
				if(ezDictNode_set_str(cur_dn, wd.str, wd.str_n) != EZ_SUCCESS) break;
			}
			status = S_VALUE;
			continue;

		case WORD_BRACE_0:
			if(in_square) break;
			if(level >= LEVEL_MAX) {
				ezLog_err("ezml { } level %d is too many\n", level);
				break;
			}
			if(status & (S_DOT|S_KEY)) break;
			if(status & (S_BEGIN|S_VALUE)) {
				cur_dn = ezDict_add_nokey_node(firsts[level]);
			}
			tmp_dict = _node_get_dict(cur_dn);
			if(tmp_dict == NULL) break;
			firsts[++level] = tmp_dict;
			cur_dn = NULL;
			status = S_BEGIN;
			continue;
		case WORD_BRACE_1:
			if(in_square) break;
			if(level < 1) break;
			cur_dn = ezDict_get_parent_node(firsts[level--]);
			if(cur_dn == NULL) break;
			status = S_VALUE;
			continue;

		case WORD_FOURCC:
			if(in_square) break;
			if(!(status & (S_VALUE|S_KEY))) break;
			ezDictNode_set_fourcc(cur_dn, wd.fourcc);
			cur_dn = NULL;
			status = S_BEGIN; /*结束赋值*/
			continue;

		case WORD_COMMA:
			if(in_square) break;
			cur_dn = NULL;
			status = S_BEGIN; /*任何状态下结束赋值*/
			continue;

		case WORD_EOF:
		case WORD_ERROR:
		default:
			break;
		}
		break;
	}

	ezResult ret = EZ_SUCCESS;

	if(wt != WORD_EOF)
	{
		ezLog_err("ezml illegl:\n");
		if(wt != WORD_ERROR)
			parse_dump_word(wt, &wd);
		parse_print(ctx);

		ret = EZ_FAIL;
	} else {
		if(level > 0) ret = EZ_FAIL;
		if(in_square) ret = EZ_FAIL;
		if(status & (S_DOT|S_EQUAL)) ret = EZ_FAIL;
		if(ret != EZ_SUCCESS) ezLog_err("ezml not end\n");
	}

	_word_reset(&wd);
	return ret;
}

ezDict *ezML_load_file(ezDict *dict, const char *file)
{
	if(dict == NULL) {
		dict = ezDict_create();
		if(dict == NULL) return NULL;
	}
	Parse_ctx *ctx = parse_file_new(file);
	if(ctx) {
		_parse_ezml(dict, ctx);
		ezDestroy(ctx);
	}
	return dict;
}

ezDict *ezML_load_string(ezDict *dict, const char *buf, int len)
{
	if(dict == NULL) {
		dict = ezDict_create();
		if(dict == NULL) return NULL;
	}
	Parse_ctx *ctx = parse_string_new(buf, len);
	if(ctx) {
		_parse_ezml(dict, ctx);
		ezDestroy(ctx);
	}
	return dict;
}

/*============================================================*/

/*可打印字符*/
#define C_IS_PRINT(c) ((c) > 0x20 && (c) < 0x7f) /* 32 < c < 127 */

static const char * _output_label(const char *str, intptr_t len)
{
	char *ret;
	ret = ezAlloc(len+1, NULL, FALSE);
	memcpy(ret, str, len);
	ret[len] = '\0';
	return ret;
}

static const char * _output_string(const char *str, intptr_t len, char yinhao)
{
	char c, *p, *ret;
	const char *e;
	int n;
	if(str == NULL || len <= 0) {
		ret = ezAlloc(3, NULL, FALSE);
		ret[0] = yinhao;
		ret[1] = yinhao;
		ret[2] = '\0';
		return ret;
	}

	e = str+len;
	p = (char *)str;
	for(n=2; p<e; ++p)
	{
		c = *p;
		switch(c) {
		case '\a':
		case '\t':
		case '\b':
		case '\f':
		case '\v':
		case '\\':
		case '\'':
		case '\"':
		case '\0':
			n += 2;
			break;
		case '\n':
		case '\r':
			n += 1;
		default:
			n += C_IS_PRINT(c)? 1 : 4;
			break;
		}
	}

	ret = ezAlloc(n+1, NULL, FALSE);
	p = ret;
	*p++ = yinhao;
	for(n=2; str<e; ++str)
	{
		c = *str;
		switch(c) {
		case '\a': c='a'; break;
		case '\t': c='t'; break;
		case '\b': c='b'; break;
		case '\f': c='f'; break;
		case '\v': c='v'; break;
		case '\\': c='\\'; break;
		case '\'': c='\''; break;
		case '\"': c='\"'; break;
		case '\0': c='0'; break;
		case '\n':
		case '\r':
			*p++ = c;
			continue;
		default:
			if(!C_IS_PRINT(c)) {
				*p++ = '\\';
				*p++ = 'x';

				char c2;
				c2 = '0'+((c>>4)&0x0f);
				if(c2 > '9') c2 += ('a' - '9' -1);
				*p++ = c2;

				c2 = '0'+(c & 0x0f);
				if(c2 > '9') c2 += ('a' - '9' -1);
				*p++ = c2;
			} else {
				*p++ = c;
			}
			continue;
		}
		*p++ = '\\';
		*p++ = c;
	}
	*p++ = yinhao;
	*p++ = '\0';
	return ret;
}

static void _write_keys(FILE *out, ezVector *keys)
{
	int i, n;
	char *key;
	fprintf(out, "\n[");
	n = ezVector_get_count(keys);
	for(i=0; i<n;) {
		key = ezVector_get(keys, i);
		fprintf(out, "%s", key);
		if(++i < n) fprintf(out, ".");
	}
	fprintf(out, "]\n");
}

static void _write_dict(FILE *out, ezDict *dict, ezVector *keys)
{
	int i, type;
	intptr_t len;
	ezFourCC four;
	const char *name;
	ezDictNode *tnode;

	for(i=0; (tnode=ezDict_get_index_node(dict, i)); ++i)
	{
		type = ezDictNode_get_type(tnode);
		if(type == EZ_DICT_NODE_DICT) continue;

		name = ezDictNode_get_key(tnode, &len);
		four = ezDictNode_get_fourcc(tnode);

		if(name)
		{
			if(parse_check_label(name, len)) {
				fprintf(out, "%s", name);
			} else {
				name = _output_string(name, len, '\'');
				fprintf(out, "%s", name);
			}
			if(type == EZ_DICT_NODE_NONE) {
				if(four) fprintf(out, "@%.4s", (char *)&four);
				fprintf(out, "\n");
				continue;
			}
			fprintf(out, "=");
		}
		else if(type == EZ_DICT_NODE_NONE)
		{	//no key, no type
			if(four) fprintf(out, "^@%.4s\n", (char *)&four);
			else fprintf(out, "^,\n");
			continue;
		}

		switch(type)
		{
		case EZ_DICT_NODE_STR:
			name = ezDictNode_get_str(tnode, &len);
			name = _output_string(name, len, '\"');
			fprintf(out, "%s", name);
			break;
		case EZ_DICT_NODE_INT:
			fprintf(out, "%zd", ezDictNode_get_int(tnode));
			break;
		case EZ_DICT_NODE_FLOAT:
			fprintf(out, "%f", ezDictNode_get_float(tnode));
			break;
		default:
			ezLog_err("#<error type %d>\n", type);
			break;
		}

		if(four) fprintf(out, "@%.4s", (char *)&four);
		fprintf(out, "\n");
	}

	for(i=0; (tnode=ezDict_get_index_node(dict, i)); ++i)
	{
		type = ezDictNode_get_type(tnode);
		if(type != EZ_DICT_NODE_DICT) continue;

		name = ezDictNode_get_key(tnode, &len);
		if(name == NULL)
			name = _output_label("^", 1);
		else if(parse_check_label(name, len))
			name = _output_label(name, len);
		else
			name = _output_string(name, len, '\'');

		ezVector_insert(keys, -1, (void *)name);

		_write_keys(out, keys);
		_write_dict(out, ezDictNode_get_dict(tnode, FALSE), keys);

		ezVector_take_out(keys, -1);
	}
	return;
}

void ezML_store_file(ezDict *dict, const char *file)
{
	if(dict != NULL)
	{
		FILE *out = fopen(file, "w");
		if(out == NULL) {
			ezLog_errno("fopen %s write\n", file);
			return;
		}

		uintptr_t gc_point = ezGCPoint_insert();
		{
			ezVector *keys = ezVector_create(NULL);
			_write_dict(out, dict, keys);
		}
		ezGCPoint_clean(gc_point);

		fflush(out);
		fclose(out);
	}
	return;
}

