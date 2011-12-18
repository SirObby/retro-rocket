/*
 * Copyright (c) 2006, Adam Dunkels
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#define DEBUG 0

#if DEBUG
#define DEBUG_PRINTF(...)  kprintf(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...)
#endif

#include "../include/kernel.h"
#include "../include/string.h"
#include "../include/ubasic.h"
#include "../include/tokenizer.h"
#include "../include/memcpy.h"
#include "../include/kprintf.h"

#define MAX_NUMLEN 10

struct keyword_token {
  char *keyword;
  int token;
};

static const struct keyword_token keywords[] = {
  {"let", TOKENIZER_LET},
  {"print", TOKENIZER_PRINT},
  {"if", TOKENIZER_IF},
  {"then", TOKENIZER_THEN},
  {"else", TOKENIZER_ELSE},
  {"for", TOKENIZER_FOR},
  {"to", TOKENIZER_TO},
  {"next", TOKENIZER_NEXT},
  {"goto", TOKENIZER_GOTO},
  {"gosub", TOKENIZER_GOSUB},
  {"return", TOKENIZER_RETURN},
  {"call", TOKENIZER_CALL},
  {"end", TOKENIZER_END},
  {NULL, TOKENIZER_ERROR}
};

/*---------------------------------------------------------------------------*/
static int singlechar(struct ubasic_ctx* ctx)
{
  if(*ctx->ptr == '\n') {
    return TOKENIZER_CR;
  } else if(*ctx->ptr == ',') {
    return TOKENIZER_COMMA;
  } else if(*ctx->ptr == ';') {
    return TOKENIZER_SEMICOLON;
  } else if(*ctx->ptr == '+') {
    return TOKENIZER_PLUS;
  } else if(*ctx->ptr == '-') {
    return TOKENIZER_MINUS;
  } else if(*ctx->ptr == '&') {
    return TOKENIZER_AND;
  } else if(*ctx->ptr == '|') {
    return TOKENIZER_OR;
  } else if(*ctx->ptr == '*') {
    return TOKENIZER_ASTR;
  } else if(*ctx->ptr == '/') {
    return TOKENIZER_SLASH;
  } else if(*ctx->ptr == '%') {
    return TOKENIZER_MOD;
  } else if(*ctx->ptr == '(') {
    return TOKENIZER_LEFTPAREN;
  } else if(*ctx->ptr == ')') {
    return TOKENIZER_RIGHTPAREN;
  } else if(*ctx->ptr == '<') {
    return TOKENIZER_LT;
  } else if(*ctx->ptr == '>') {
    return TOKENIZER_GT;
  } else if(*ctx->ptr == '=') {
    return TOKENIZER_EQ;
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
static int get_next_token(struct ubasic_ctx* ctx)
{
  struct keyword_token const *kt;
  int i;

  DEBUG_PRINTF("get_next_token(): '%s'\n", ctx->ptr);

  if(*ctx->ptr == 0) {
    return TOKENIZER_ENDOFINPUT;
  }
  
  if(isdigit(*ctx->ptr)) {
    for(i = 0; i < MAX_NUMLEN; ++i) {
      if(!isdigit(ctx->ptr[i])) {
	if(i > 0) {
	  ctx->nextptr = ctx->ptr + i;
	  return TOKENIZER_NUMBER;
	} else {
	  DEBUG_PRINTF("get_next_token: error due to too short number\n");
	  return TOKENIZER_ERROR;
	}
      }
      if(!isdigit(ctx->ptr[i])) {
	DEBUG_PRINTF("get_next_token: error due to malformed number\n");
	return TOKENIZER_ERROR;
      }
    }
    DEBUG_PRINTF("get_next_token: error due to too long number\n");
    return TOKENIZER_ERROR;
  } else if(singlechar(ctx)) {
    ctx->nextptr = ctx->ptr + 1;
    return singlechar(ctx);
  } else if(*ctx->ptr == '"') {
    ctx->nextptr = ctx->ptr;
    do {
      ++ctx->nextptr;
    } while(*ctx->nextptr != '"');
    ++ctx->nextptr;
    return TOKENIZER_STRING;
  } else {
    for(kt = keywords; kt->keyword != NULL; ++kt) {
      if(strncmp(ctx->ptr, kt->keyword, strlen(kt->keyword)) == 0) {
	ctx->nextptr = ctx->ptr + strlen(kt->keyword);
	return kt->token;
      }
    }
  }

  if ((*ctx->ptr >= 'a' && *ctx->ptr <= 'z') || (*ctx->ptr >= 'A' && *ctx->ptr <= 'Z')) {
    ctx->nextptr = ctx->ptr;
    while ((*ctx->nextptr >= 'a' && *ctx->nextptr <= 'z') || (*ctx->nextptr >= 'A' && *ctx->nextptr <= 'Z'))
	    ctx->nextptr++;
    //ctx->nextptr++;
    //kprintf("Variable. nextptr = %08x ptr = %08x\n", ctx->nextptr, ctx->ptr);
    return TOKENIZER_VARIABLE;
  }

  
  return TOKENIZER_ERROR;
}
/*---------------------------------------------------------------------------*/
void
tokenizer_init(const char *program, struct ubasic_ctx* ctx)
{
  ctx->ptr = program;
  ctx->current_token = get_next_token(ctx);
}
/*---------------------------------------------------------------------------*/
int tokenizer_token(struct ubasic_ctx* ctx)
{
  return ctx->current_token;
}
/*---------------------------------------------------------------------------*/
void tokenizer_next(struct ubasic_ctx* ctx)
{

  if(tokenizer_finished(ctx)) {
    return;
  }

  DEBUG_PRINTF("tokenizer_next: %p\n", ctx->nextptr);
  ctx->ptr = ctx->nextptr;
  while(*ctx->ptr == ' ') {
    ++ctx->ptr;
  }
  ctx->current_token = get_next_token(ctx);
  DEBUG_PRINTF("tokenizer_next: '%s' %d\n", ctx->ptr, ctx->current_token);
  return;
}
/*---------------------------------------------------------------------------*/
int tokenizer_num(struct ubasic_ctx* ctx)
{
  return atoi(ctx->ptr);
}
/*---------------------------------------------------------------------------*/
void tokenizer_string(char *dest, int len, struct ubasic_ctx* ctx)
{
  char *string_end;
  int string_len;
  
  if(tokenizer_token(ctx) != TOKENIZER_STRING) {
    return;
  }
  string_end = strchr(ctx->ptr + 1, '"');
  if(string_end == NULL) {
    return;
  }
  string_len = string_end - ctx->ptr - 1;
  if(len < string_len) {
    string_len = len;
  }
  memcpy(dest, ctx->ptr + 1, string_len);
  dest[string_len] = 0;
}
/*---------------------------------------------------------------------------*/
void tokenizer_error_print(struct ubasic_ctx* ctx)
{
  kprintf("%s\n", ctx->ptr);
}
/*---------------------------------------------------------------------------*/
int tokenizer_finished(struct ubasic_ctx* ctx)
{
  return *ctx->ptr == 0 || ctx->current_token == TOKENIZER_ENDOFINPUT;
}
/*---------------------------------------------------------------------------*/
const char* tokenizer_variable_name(struct ubasic_ctx* ctx)
{
  //return *ctx->ptr - 'a';
  static char varname[MAX_VARNAME];
  int count = 0;
  //kprintf("Ptr: %08x; *ctx->ptr=%d\n", ctx->ptr, *ctx->ptr);
  while (((*ctx->ptr >= 'a' && *ctx->ptr <= 'z') || (*ctx->ptr >= 'A' && *ctx->ptr <= 'Z')) && count < MAX_VARNAME)
  {
	//kprintf("Add: %c\n", ctx->ptr);
	varname[count++] = *(ctx->ptr++);
  }
  varname[count] = 0;
  //kprintf("variable name: %s\n", varname);
  return varname;
}
/*---------------------------------------------------------------------------*/
