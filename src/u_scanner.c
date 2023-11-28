// Copyright (c) 2019, Fernando Carmona Varo  <ferkiwi@gmail.com>
// Copyright (c) 2010, Braden "Blzut3" Obrzut <admin@maniacsvault.net>
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//    * Neither the name of the <organization> nor the
//      names of its contributors may be used to endorse or promote products
//      derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>

#include "doomtype.h"
#include "i_system.h"
#include "m_misc2.h"
#include "u_scanner.h"

static const char* U_TokenNames[TK_NumSpecialTokens] =
{
  "Identifier", // case insensitive identifier, beginning with a letter and may contain [a-z0-9_]
  "String Constant",
  "Integer Constant",
  "Float Constant",
  "Boolean Constant",
  "Logical And",
  "Logical Or",
  "Equals",
  "Not Equals",
  "Greater Than or Equals",
  "Less Than or Equals",
  "Left Shift",
  "Right Shift"
};

static void U_CheckForWhitespace(u_scanner_t* scanner);
static void U_ExpandState(u_scanner_t* scanner);
static void U_Unescape(char *str);
static void U_SetString(char **ptr, const char *start, int length);

u_scanner_t U_ScanOpen(const char* data, int length, const char* name)
{
  u_scanner_t scanner = {0};
  scanner.lineStart = scanner.logicalPosition = scanner.scanPos = scanner.tokenLinePosition = 0;
  scanner.line = scanner.tokenLine = 1;
  scanner.needNext = true;
  scanner.string = NULL;
  scanner.nextState.string = NULL;
  scanner.name = name;

  if(length == -1)
    length = strlen(data);
  scanner.length = length;
  scanner.data = (char*) malloc(sizeof(char)*length);
  memcpy(scanner.data, data, length);

  U_CheckForWhitespace(&scanner);

  return scanner;
}

void U_ScanClose(u_scanner_t* scanner)
{
  if (scanner->nextState.string != NULL)
    free(scanner->nextState.string);
  if(scanner->data != NULL)
    free(scanner->data);
}

static void U_IncrementLine(u_scanner_t* scanner)
{
  scanner->line++;
  scanner->lineStart = scanner->scanPos;
}

static void U_CheckForWhitespace(u_scanner_t* scanner)
{
  int comment = 0; // 1 = till next new line, 2 = till end block
  while(scanner->scanPos < scanner->length)
  {
    char cur = scanner->data[scanner->scanPos];
    char next = scanner->scanPos+1 < scanner->length ? scanner->data[scanner->scanPos+1] : 0;
    if(comment == 2)
    {
      if(cur != '*' || next != '/')
      {
        if(cur == '\n' || cur == '\r')
        {
          scanner->scanPos++;

          // Do a quick check for Windows style new line
          if(cur == '\r' && next == '\n')
            scanner->scanPos++;
          U_IncrementLine(scanner);
        }
        else
          scanner->scanPos++;
      }
      else
      {
        comment = 0;
        scanner->scanPos += 2;
      }
      continue;
    }

    if(cur == ' ' || cur == '\t' || cur == 0)
      scanner->scanPos++;
    else if(cur == '\n' || cur == '\r')
    {
      scanner->scanPos++;
      if(comment == 1)
        comment = 0;

      // Do a quick check for Windows style new line
      if(cur == '\r' && next == '\n')
        scanner->scanPos++;
      U_IncrementLine(scanner);
    }
    else if(cur == '/' && comment == 0)
    {
      switch(next)
      {
        case '/':
          comment = 1;
          break;
        case '*':
          comment = 2;
          break;
        default:
          return;
      }
      scanner->scanPos += 2;
    }
    else
    {
      if(comment == 0)
        return;
      else
        scanner->scanPos++;
    }
  }
}

boolean U_CheckToken(u_scanner_t* s, char token)
{
  if(s->needNext)
  {
    if(!U_GetNextToken(s, false))
      return false;
  }

  // An int can also be a float.
  if((s->nextState).token == token || ((s->nextState).token == TK_IntConst && s->token == TK_FloatConst))
  {
    s->needNext = true;
    U_ExpandState(s);
    return true;
  }
  s->needNext = false;
  return false;
}

static void U_ExpandState(u_scanner_t* s)
{
  s->logicalPosition = s->scanPos;
  U_CheckForWhitespace(s);

  U_SetString(&(s->string), s->nextState.string, -1);
  s->number = s->nextState.number;
  s->decimal = s->nextState.decimal;
  s->sc_boolean = s->nextState.sc_boolean;
  s->token = s->nextState.token;
  s->tokenLine = s->nextState.tokenLine;
  s->tokenLinePosition = s->nextState.tokenLinePosition;
}

static void U_SaveState(u_scanner_t* s, u_scanner_t savedstate)
{
  // This saves the entire parser state except for the data pointer.
  if (savedstate.string != NULL) free(savedstate.string);
  if (savedstate.nextState.string != NULL) free(savedstate.nextState.string);

  memcpy(&savedstate, s, sizeof(*s));
  savedstate.string = strdup(s->string);
  savedstate.nextState.string = strdup(s->nextState.string);
  savedstate.data = NULL;
}

static void U_RestoreState(u_scanner_t* s, u_scanner_t savedstate)
{
  if (savedstate.data == NULL)
  {
    char *saveddata = s->data;
    U_SaveState(&savedstate, *s);
    s->data = saveddata;
  }
}

boolean U_GetString(u_scanner_t* scanner)
{
  unsigned int start;
  char cur;
  u_parserstate_t* nextState = &scanner->nextState;

  if(!scanner->needNext)
  {
    scanner->needNext = true;
    U_ExpandState(scanner);
    return true;
  }

  nextState->tokenLine = scanner->line;
  nextState->tokenLinePosition = scanner->scanPos - scanner->lineStart;
  nextState->token = TK_NoToken;
  if(scanner->scanPos >= scanner->length)
  {
    U_ExpandState(scanner);
    return false;
  }

  start = scanner->scanPos;
  cur   = scanner->data[scanner->scanPos++];

  while(scanner->scanPos < scanner->length)
  {
    cur = scanner->data[scanner->scanPos];

    if(cur == ' ' || cur == '\t' || cur == '\n' || cur == '\r' || cur == 0)
      break;
    else
      scanner->scanPos++;
  }

  U_SetString(&(nextState->string), scanner->data + start, scanner->scanPos - start);
  U_ExpandState(scanner);
  return true;
}

boolean U_GetNextToken(u_scanner_t* scanner, boolean   expandState)
{
  unsigned int start;
  unsigned int end;
  char cur;
  int integerBase            = 10;
  boolean floatHasDecimal    = false;
  boolean floatHasExponent   = false;
  boolean stringFinished     = false; // Strings are the only things that can have 0 length tokens.
  u_parserstate_t* nextState = &scanner->nextState;

  if(!scanner->needNext)
  {
    scanner->needNext = true;
    if(expandState)
      U_ExpandState(scanner);
    return true;
  }

  nextState->tokenLine = scanner->line;
  nextState->tokenLinePosition = scanner->scanPos - scanner->lineStart;
  nextState->token = TK_NoToken;
  if(scanner->scanPos >= scanner->length)
  {
    if(expandState)
      U_ExpandState(scanner);
    return false;
  }

  start = scanner->scanPos;
  end   = scanner->scanPos;
  cur   = scanner->data[scanner->scanPos++];

  // Determine by first character
  if(cur == '_' || (cur >= 'A' && cur <= 'Z') || (cur >= 'a' && cur <= 'z'))
    nextState->token = TK_Identifier;
  else if(cur >= '0' && cur <= '9')
  {
    if(cur == '0')
      integerBase = 8;
    nextState->token = TK_IntConst;
  }
  else if(cur == '.')
  {
    floatHasDecimal = true;
    nextState->token = TK_FloatConst;
  }
  else if(cur == '"')
  {
    end = ++start; // Move the start up one character so we don't have to trim it later.
    nextState->token = TK_StringConst;
  }
  else
  {
    end = scanner->scanPos;
    nextState->token = cur;

    // Now check for operator tokens
    if(scanner->scanPos < scanner->length)
    {
      char next = scanner->data[scanner->scanPos];
      if(cur == '&' && next == '&')
        nextState->token = TK_AndAnd;
      else if(cur == '|' && next == '|')
        nextState->token = TK_OrOr;
      else if(cur == '<' && next == '<')
        nextState->token = TK_ShiftLeft;
      else if(cur == '>' && next == '>')
        nextState->token = TK_ShiftRight;
      //else if(cur == '#' && next == '#')
      //  nextState.token = TK_MacroConcat;
      else if(next == '=')
      {
        switch(cur)
        {
          case '=':
            nextState->token = TK_EqEq;
            break;
          case '!':
            nextState->token = TK_NotEq;
            break;
          case '>':
            nextState->token = TK_GtrEq;
            break;
          case '<':
            nextState->token = TK_LessEq;
            break;
          default:
            break;
        }
      }
      if(nextState->token != cur)
      {
        scanner->scanPos++;
        end = scanner->scanPos;
      }
    }
  }

  if(start == end)
  {
    while(scanner->scanPos < scanner->length)
    {
      cur = scanner->data[scanner->scanPos];
      switch(nextState->token)
      {
        default:
          break;
        case TK_Identifier:
          if(cur != '_' && (cur < 'A' || cur > 'Z') && (cur < 'a' || cur > 'z') && (cur < '0' || cur > '9'))
            end = scanner->scanPos;
          break;
        case TK_IntConst:
          if(cur == '.' || (scanner->scanPos-1 != start && cur == 'e'))
            nextState->token = TK_FloatConst;
          else if((cur == 'x' || cur == 'X') && scanner->scanPos-1 == start)
          {
            integerBase = 16;
            break;
          }
          else
          {
            switch(integerBase)
            {
              default:
                if(cur < '0' || cur > '9')
                  end = scanner->scanPos;
                break;
              case 8:
                if(cur < '0' || cur > '7')
                  end = scanner->scanPos;
                break;
              case 16:
                if((cur < '0' || cur > '9') && (cur < 'A' || cur > 'F') && (cur < 'a' || cur > 'f'))
                  end = scanner->scanPos;
                break;
            }
            break;
          }
        case TK_FloatConst:
          if(cur < '0' || cur > '9')
          {
            if(!floatHasDecimal && cur == '.')
            {
              floatHasDecimal = true;
              break;
            }
            else if(!floatHasExponent && cur == 'e')
            {
              floatHasDecimal = true;
              floatHasExponent = true;
              if(scanner->scanPos+1 < scanner->length)
              {
                char next = scanner->data[scanner->scanPos+1];
                if((next < '0' || next > '9') && next != '+' && next != '-')
                  end = scanner->scanPos;
                else
                  scanner->scanPos++;
              }
              break;
            }
            end = scanner->scanPos;
          }
          break;
        case TK_StringConst:
          if(cur == '"')
          {
            stringFinished = true;
            end = scanner->scanPos;
            scanner->scanPos++;
          }
          else if(cur == '\\')
            scanner->scanPos++; // Will add two since the loop automatically adds one
          break;
      }
      if(start == end && !stringFinished)
        scanner->scanPos++;
      else
        break;
    }
    // If we reached end of input while reading, set it as the end of token
    if(scanner->scanPos == scanner->length && start == end)
      end = scanner->length;
  }

  if(end-start > 0 || stringFinished)
  {
    U_SetString(&(nextState->string), scanner->data+start, end-start);
    if(nextState->token == TK_FloatConst)
    {
      nextState->decimal = atof(nextState->string);
      nextState->number = (int) (nextState->decimal);
      nextState->sc_boolean = (nextState->number != 0);
    }
    else if(nextState->token == TK_IntConst)
    {
      nextState->number = strtol(nextState->string, NULL, integerBase);
      nextState->decimal = nextState->number;
      nextState->sc_boolean = (nextState->number != 0);
    }
    else if(nextState->token == TK_Identifier)
    {
      // Identifiers should be case insensitive.
      char *p = nextState->string;
      while (*p)
      {
        *p = tolower(*p);
        p++;
      }
      // Check for a boolean constant.
      if(strcmp(nextState->string, "true") == 0)
      {
        nextState->token = TK_BoolConst;
        nextState->sc_boolean = true;
      }
      else if (strcmp(nextState->string, "false") == 0)
      {
        nextState->token = TK_BoolConst;
        nextState->sc_boolean = false;
      }
    }
    else if(nextState->token == TK_StringConst)
    {
      U_Unescape(nextState->string);
    }
    if(expandState)
      U_ExpandState(scanner);
    return true;
  }
  nextState->token = TK_NoToken;
  if(expandState)
    U_ExpandState(scanner);
  return false;
}

// Skips all Tokens in current line and parses the first token on the next
// line.
boolean U_GetNextLineToken(u_scanner_t* scanner)
{
  unsigned int line = scanner->line;
  boolean   retval = false;

  do retval = U_GetNextToken(scanner, true);
  while (retval && scanner->line == line);

  return retval;
}

void U_ErrorToken(u_scanner_t* s, int token)
{
  if (token < TK_NumSpecialTokens && s->token >= TK_Identifier && s->token < TK_NumSpecialTokens)
    U_Error(s, "Expected %s but got %s '%s' instead.", U_TokenNames[token], U_TokenNames[(int)s->token], s->string);
  else if (token < TK_NumSpecialTokens && s->token >= TK_NumSpecialTokens)
    U_Error(s, "Expected %s but got '%c' instead.", U_TokenNames[token], s->token);
  else if (token < TK_NumSpecialTokens && s->token == TK_NoToken)
     U_Error(s, "Expected %s", U_TokenNames[token]);
  else if (token >= TK_NumSpecialTokens && s->token >= TK_Identifier && s->token < TK_NumSpecialTokens)
    U_Error(s, "Expected '%c' but got %s '%s' instead.", token, U_TokenNames[(int)s->token], s->string);
  else
    U_Error(s, "Expected '%c' but got '%c' instead.", token, s->token);
}

void U_ErrorString(u_scanner_t* s, const char *mustget)
{
  if (s->token < TK_NumSpecialTokens)
    U_Error(s, "Expected '%s' but got %s '%s' instead.", mustget, U_TokenNames[(int)s->token], s->string);
  else
    U_Error(s, "Expected '%s' but got '%c' instead.", mustget, s->token);
}

void PRINTF_ATTR(2, 0) U_Error(u_scanner_t* s, const char *msg, ...)
{
  char buffer[1024];
  va_list ap;
  va_start(ap, msg);
  M_vsnprintf(buffer, 1024, msg, ap);
  va_end(ap);
  I_Error("%s:%d:%d:%s", s->name, s->tokenLine, s->tokenLinePosition, buffer);
}

boolean U_MustGetToken(u_scanner_t* s, char token)
{
  if(!U_CheckToken(s, token))
  {
    U_ExpandState(s);
    U_ErrorToken(s, token);
    return false;
  }
  return true;
}

boolean U_MustGetIdentifier(u_scanner_t* s, const char *ident)
{
  if (!U_CheckToken(s, TK_Identifier) || strcasecmp(s->string, ident))
  {
    U_ErrorString(s, ident);
    return false;
  }
  return true;
}

void U_Unget(u_scanner_t* s)
{
  s->needNext = false;
}

// Convenience helpers that parse an entire number including a leading minus or
// plus sign
static boolean U_ScanInteger(u_scanner_t* s)
{
  boolean   neg = false;
  if (!U_GetNextToken(s, true))
  {
    return false;
  }
  if (s->token == '-')
  {
   if (!U_GetNextToken(s, true))
    {
      return false;
    }
    neg = true;
  }
  else if (s->token == '+')
  {
   if (!U_GetNextToken(s, true))
   {
     return false;
   }
  }
  if (s->token != TK_IntConst)
  {
    return false;
  }
  if (neg)
  {
    s->number = -(s->number);
    s->decimal = -(s->decimal);
  }
  return true;
}

static boolean U_ScanFloat(u_scanner_t* s)
{
  boolean   neg = false;
  if (!U_GetNextToken(s, true))
  {
    return false;
  }
  if (s->token == '-')
  {
    if (!U_GetNextToken(s, true))
    {
      return false;
    }
    neg = true;
  }
  else if (s->token == '+')
  {
    if (!U_GetNextToken(s, true))
    {
      return false;
    }
  }
  if (s->token != TK_IntConst && s->token != TK_FloatConst)
  {
    return false;
  }
  if (neg)
  {
    s->number = -(s->number);
    s->decimal = -(s->decimal);
  }
  return true;
}

boolean U_CheckInteger(u_scanner_t* s)
{
  boolean   res;
  u_scanner_t savedstate = {0};
  U_SaveState(s, savedstate);
  res = U_ScanInteger(s);
  if (!res)
     U_RestoreState(s, savedstate);
  return res;
}

boolean U_CheckFloat(u_scanner_t* s)
{
  boolean   res;
  u_scanner_t savedstate = {0};
  U_SaveState(s, savedstate);
  res = U_ScanFloat(s);
  if (!res)
     U_RestoreState(s, savedstate);
  return res;
}

boolean U_MustGetInteger(u_scanner_t* s)
{
  if (!U_ScanInteger(s))
  {
    U_ErrorToken(s, TK_IntConst);
    return false;
  }
  return true;
}

boolean U_MustGetFloat(u_scanner_t* s)
{
  if (!U_ScanFloat(s))
  {
    U_ErrorToken(s, TK_FloatConst);
    return false;
  }
  return true;
}


boolean U_HasTokensLeft(u_scanner_t* s)
{
  return (s->scanPos < s->length);
}

// This is taken from ZDoom's strbin function which can do a lot more than just
// unescaping backslashes and quotation marks.
void U_Unescape(char *str)
{
  char *p = str, c;
  int i;

  while ((c = *p++)) {
    if (c != '\\') {
      *str++ = c;
    }
    else {
      switch (*p) {
      case 'a':
        *str++ = '\a';
        break;
      case 'b':
        *str++ = '\b';
        break;
      case 'f':
        *str++ = '\f';
        break;
      case 'n':
        *str++ = '\n';
        break;
      case 't':
        *str++ = '\t';
        break;
      case 'r':
        *str++ = '\r';
        break;
      case 'v':
        *str++ = '\v';
        break;
      case '?':
        *str++ = '\?';
        break;
      case '\n':
        break;
      case 'x':
      case 'X':
        c = 0;
        for (i = 0; i < 2; i++)
        {
          p++;
          if (*p >= '0' && *p <= '9')
            c = (c << 4) + *p - '0';
          else if (*p >= 'a' && *p <= 'f')
            c = (c << 4) + 10 + *p - 'a';
          else if (*p >= 'A' && *p <= 'F')
            c = (c << 4) + 10 + *p - 'A';
          else
          {
            p--;
            break;
          }
        }
        *str++ = c;
        break;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
        c = *p - '0';
        for (i = 0; i < 2; i++)
        {
          p++;
          if (*p >= '0' && *p <= '7')
            c = (c << 3) + *p - '0';
          else
          {
            p--;
            break;
          }
        }
        *str++ = c;
        break;
      default:
        *str++ = *p;
        break;
      }
      p++;
    }
  }
  *str = 0;
}

static void U_SetString(char **ptr, const char *start, int length)
{
  if (length == -1)
    length = strlen(start);
  if (*ptr != NULL) free(*ptr);
  *ptr = (char*)malloc(length + 1);
  memcpy(*ptr, start, length);
  (*ptr)[length] = '\0';
}
