/***************************************************************
*      scanner routine for Mini C language                    *
***************************************************************/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "Scanner.h"

extern FILE *sourceFile;                       // miniC source program

int currentLine = 1;
int currentCol = 0;
int superLetter(char ch);
int superLetterOrDigit(char ch);
int getNumber(char firstCharacter);
int hexValue(char ch);
void lexicalError(int n);


char *tokenName[] = {
	"!",        "!=",      "%",       "%=",     "%ident",   "%number",
	/* 0          1           2         3          4          5        */
	"&&",       "(",       ")",       "*",      "*=",       "+",
	/* 6          7           8         9         10         11        */
	"++",       "+=",      ",",       "-",      "--",	    "-=",
	/* 12         13         14        15         16         17        */
	"/",        "/=",      ";",       "<",      "<=",       "=",
	/* 18         19         20        21         22         23        */
	"==",       ">",       ">=",      "[",      "]",        "eof",
	/* 24         25         26        27         28         29        */
	//   ...........    word symbols ................................. //
	/* 30         31         32        33         34         35        */
	"const",    "else",     "if",      "int",     "return",  "void",
	/* 36         37         38        39                              */
	"while",    "{",        "||",       "}",
	/* 추가 */
    "char",     "double",   "for",      "do",       "goto",
    "switch",   "case",     "break",    "default",
    ":",
    "%charLit", "%stringLit", "%doubleLit",
    "%docComment"
};

char *keyword[NO_KEYWORD] = {
	"const",  "else",    "if",    "int",    "return",  "void",    "while",
	/* 추가 */
    "char",   "double",  "for",   "do",     "goto",    "switch",  "case",
    "break",  "default"
};

enum tsymbol tnum[NO_KEYWORD] = {
	tconst,    telse,     tif,     tint,     treturn,   tvoid,     twhile,
	/* 추가 */
    tchar,    tdouble,   tfor,    tdo,      tgoto,     tswitch,   tcase,
    tbreak,   tdefault
};

int getNextChar() {
    int c = fgetc(sourceFile);    
    if (c == '\n') {
        currentLine++;
        currentCol = 0;
    } else if (c != EOF) {
        currentCol++;
    }
    return c;
}

void ungetNextChar(int ch) {
    if (ch == EOF) return;
    ungetc(ch, sourceFile);    
    if (ch == '\n') {
        currentLine--;
    } else {
        currentCol--;
    }
}

/* 소수점 이후를 읽어 double 값을 완성한다.
 * 호출 시점: '.' 문자는 이미 소비됨. 다음 입력은 소수부 첫 숫자.
 * 슬라이드 4.2.3 상태 B → C → (D → E) 구간에 대응.
 */
double collectFractionAndExponent(double intPart) {
    double fracPart = 0.0;
    double divisor = 10.0;
    char ch = getNextChar();
    
    /* 소수부 (B → C → ε) */
    while (isdigit(ch)) {
        fracPart += (ch - '0') / divisor;
        divisor *= 10.0;
        ch = getNextChar();
    }
    
    double value = intPart + fracPart;
    
    /* 지수부 (C → D → E/F/G → E → ε) */
    if (ch == 'e' || ch == 'E') {
        int expSign = 1;
        int expVal = 0;
        ch = getNextChar();
        if (ch == '+')      { ch = getNextChar(); }
        else if (ch == '-') { expSign = -1; ch = getNextChar(); }
        while (isdigit(ch)) {
            expVal = 10 * expVal + (ch - '0');
            ch = getNextChar();
        }
        double mult = 1.0;
        for (int i = 0; i < expVal; i++) mult *= 10.0;
        value = (expSign > 0) ? value * mult : value / mult;
    }
    
    ungetNextChar(ch);
    return value;
}

struct tokenType scanner()
{
	struct tokenType token;
	int i, index;
	char ch, id[ID_LENGTH];
	int tokenLine, tokenCol;        // 추가

	token.number = tnull;

	do {
		while (isspace(ch = getNextChar()));	// state 1: skip blanks
		
		tokenLine = currentLine;    // 추가: 토큰 시작 위치 기록
        tokenCol = currentCol;      // 추가
		
		if (superLetter(ch)) { // identifier or keyword
			i = 0;
			do {
				if (i < ID_LENGTH) id[i++] = ch;
				ch = getNextChar();
			} while (superLetterOrDigit(ch));
			if (i >= ID_LENGTH) lexicalError(1);
			id[i] = '\0';
			ungetNextChar(ch);  //  retract
									 // find the identifier in the keyword table
			for (index = 0; index < NO_KEYWORD; index++)
				if (!strcmp(id, keyword[index])) break;
			if (index < NO_KEYWORD)    // found, keyword exit
				token.number = tnum[index];
			else {                     // not found, identifier exit
				token.number = tident;
				strcpy(token.value.id, id);
			}
		}  // end of identifier or keyword
		else if (isdigit(ch)) {  // number
			int intPart = getNumber(ch);
			char peekCh = getNextChar();
			if (peekCh == '.') {                            // 5.5 또는 5.
				token.number = tdoubleLit;
				token.value.dnum = collectFractionAndExponent((double)intPart);
			} else {                                        // 정수
				ungetNextChar(peekCh);
				token.number = tnumber;
				token.value.num = intPart;
    		}
		}
		else switch (ch) {  // special character
		case '\'': {    // character literal start
    		ch = getNextChar();
    		char charValue;
    
    		if (ch == '\\') {          // escape sequence
        		ch = getNextChar();
        		switch (ch) {
					case 'n':  charValue = '\n'; break;
					case 't':  charValue = '\t'; break;
					case 'r':  charValue = '\r'; break;
					case '0':  charValue = '\0'; break;
					case '\\': charValue = '\\'; break;
					case '\'': charValue = '\''; break;
					case '"':  charValue = '"';  break;
					default:   charValue = ch;   break;  // unknown escape
				}
			} else {
				charValue = ch;
			}
    
			ch = getNextChar();          // expect closing '
			if (ch != '\'') {
				lexicalError(5);          // unclosed character literal
				ungetNextChar(ch);
			}
			
			token.number = tcharLit;
			token.value.ch = charValue;
			break;
		}
		case '"': {    // string literal start
			int strIdx = 0;
			char strBuf[256];
			
			ch = getNextChar();
			while (ch != '"' && ch != EOF) {
				char appendCh;
				
				if (ch == '\\') {          // escape sequence
					ch = getNextChar();
					switch (ch) {
						case 'n':  appendCh = '\n'; break;
						case 't':  appendCh = '\t'; break;
						case 'r':  appendCh = '\r'; break;
						case '0':  appendCh = '\0'; break;
						case '\\': appendCh = '\\'; break;
						case '\'': appendCh = '\''; break;
						case '"':  appendCh = '"';  break;
						default:   appendCh = ch;   break;
					}
				} else {
					appendCh = ch;
				}
				
				if (strIdx < 255) strBuf[strIdx++] = appendCh;
				ch = getNextChar();
			}
			
			if (ch == EOF) {
				lexicalError(6);          // unclosed string literal
			}
			
			strBuf[strIdx] = '\0';
			token.number = tstringLit;
			strcpy(token.value.str, strBuf);
			break;
		}
		case '.': {    // double literal 숏폼: .5
			char peekCh = getNextChar();
			if (isdigit(peekCh)) {
				ungetNextChar(peekCh);                      // helper가 다시 읽음
				token.number = tdoubleLit;
				token.value.dnum = collectFractionAndExponent(0.0);
			} else {
				ungetNextChar(peekCh);
				lexicalError(4);                            // 잘못된 문자
			}
			break;
		}
		case '/':
			ch = getNextChar();
			if (ch == '*') {
				char ch2 = getNextChar();
				if (ch2 == '*') {
					// doc block comment: /** ... */
					char ch3 = getNextChar();
					if (ch3 == '/') {               // empty doc comment: /**/
						printf("Documented Comments ------> \n");
					} else {
						char docBuf[1024];
						int docIdx = 0;
						docBuf[docIdx++] = ch3;
						char prev = ch3;
						char cur;
						while ((cur = getNextChar()) != EOF) {
							if (prev == '*' && cur == '/') {
								docIdx--;            // remove trailing *
								break;
							}
							if (docIdx < 1023) docBuf[docIdx++] = cur;
							prev = cur;
						}
						docBuf[docIdx] = '\0';
						printf("Documented Comments ------> %s\n", docBuf);
					}
				} else {
					// regular block comment: original logic
					ch = ch2;
					do {
						while (ch != '*') ch = getNextChar();
						ch = getNextChar();
					} while (ch != '/');
				}
			}
			else if (ch == '/') {
				char ch2 = getNextChar();
				if (ch2 == '/') {                    /* /// 라인 문서 주석 */
					char docBuf[1024];
					int docIdx = 0;
					char c = getNextChar();
					while (c != '\n' && c != EOF) {
						if (docIdx < 1023) docBuf[docIdx++] = c;
						c = getNextChar();
					}
					docBuf[docIdx] = '\0';
					printf("Documented Comments ------> %s\n", docBuf);
				} else {
					/* 일반 // 라인 주석 — ch2가 이미 첫 글자 */
					while (ch2 != '\n' && ch2 != EOF) ch2 = getNextChar();
				}
			}
			else if (ch == '=')  token.number = tdivAssign;
			else {
				token.number = tdiv;
				ungetNextChar(ch);
			}
			break;
		case '!':
			ch = getNextChar();
			if (ch == '=')  token.number = tnotequ;
			else {
				token.number = tnot;
				ungetNextChar(ch); // retract
			}
			break;
		case '%':
			ch = getNextChar();
			if (ch == '=') {
				token.number = tremAssign;
			}
			else {
				token.number = tremainder;
				ungetNextChar(ch);
			}
			break;
		case '&':
			ch = getNextChar();
			if (ch == '&')  token.number = tand;
			else {
				lexicalError(2);
				ungetNextChar(ch);  // retract
			}
			break;
		case '*':
			ch = getNextChar();
			if (ch == '=')  token.number = tmulAssign;
			else {
				token.number = tmul;
				ungetNextChar(ch);  // retract
			}
			break;
		case '+':
			ch = getNextChar();
			if (ch == '+')  token.number = tinc;
			else if (ch == '=') token.number = taddAssign;
			else {
				token.number = tplus;
				ungetNextChar(ch);  // retract
			}
			break;
		case '-':
			ch = getNextChar();
			if (ch == '-')  token.number = tdec;
			else if (ch == '=') token.number = tsubAssign;
			else {
				token.number = tminus;
				ungetNextChar(ch);  // retract
			}
			break;
		case '<':
			ch = getNextChar();
			if (ch == '=') token.number = tlesse;
			else {
				token.number = tless;
				ungetNextChar(ch);  // retract
			}
			break;
		case '=':
			ch = getNextChar();
			if (ch == '=')  token.number = tequal;
			else {
				token.number = tassign;
				ungetNextChar(ch);  // retract
			}
			break;
		case '>':
			ch = getNextChar();
			if (ch == '=') token.number = tgreate;
			else {
				token.number = tgreat;
				ungetNextChar(ch);  // retract
			}
			break;
		case '|':
			ch = getNextChar();
			if (ch == '|')  token.number = tor;
			else {
				lexicalError(3);
				ungetNextChar(ch);  // retract
			}
			break;
		case '(': token.number = tlparen;         break;
		case ')': token.number = trparen;         break;
		case ',': token.number = tcomma;          break;
		case ';': token.number = tsemicolon;      break;
		case ':': token.number = tcolon;     	  break;   // ← 추가
		case '[': token.number = tlbracket;       break;
		case ']': token.number = trbracket;       break;
		case '{': token.number = tlbrace;         break;
		case '}': token.number = trbrace;         break;
		case EOF: token.number = teof;            break;
		default: {
			printf("Current character : %c", ch);
			lexicalError(4);
			break;
		}

		} // switch end
	} while (token.number == tnull);

	/* 추가: 토큰 위치 정보 부착 */
    token.lineNo = tokenLine;
    token.colNo = tokenCol;
    strcpy(token.fileName, sourceFileName);

	return token;
} // end of scanner

void lexicalError(int n)
{
	printf(" *** Lexical Error : ");
	switch (n) {
	case 1: printf("an identifier length must be less than 12.\n");
		break;
	case 2: printf("next character must be &\n");
		break;
	case 3: printf("next character must be |\n");
		break;
	case 4: printf("invalid character\n");
		break;
	case 5: printf("unclosed character literal\n"); 
		break;   // 추가
	case 6: printf("unclosed string literal\n"); 
		break;   // 추가
	}
}

int superLetter(char ch)
{
	if (isalpha(ch) || ch == '_') return 1;
	else return 0;
}

int superLetterOrDigit(char ch)
{
	if (isalnum(ch) || ch == '_') return 1;
	else return 0;
}

int getNumber(char firstCharacter)
{
	int num = 0;
	int value;
	char ch;

	if (firstCharacter == '0') {
		ch = getNextChar();
		if ((ch == 'X') || (ch == 'x')) {		// hexa decimal
			while ((value = hexValue(ch = getNextChar())) != -1)
				num = 16 * num + value;
		}
		else if ((ch >= '0') && (ch <= '7'))	// octal
			do {
				num = 8 * num + (int)(ch - '0');
				ch = getNextChar();
			} while ((ch >= '0') && (ch <= '7'));
		else num = 0;						// zero
	}
	else {									// decimal
		ch = firstCharacter;
		do {
			num = 10 * num + (int)(ch - '0');
			ch = getNextChar();
		} while (isdigit(ch));
	}
	ungetNextChar(ch);  /*  retract  */
	return num;
}

int hexValue(char ch)
{
	switch (ch) {
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		return (ch - '0');
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		return (ch - 'A' + 10);
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
		return (ch - 'a' + 10);
	default: return -1;
	}
}

void printToken(tokenType token)
{
    if (token.number == tident) {
        printf("Token -----> %s (%d, %s, %s, %d, %d)\n",
            token.value.id, token.number, token.value.id,
            token.fileName, token.lineNo, token.colNo);
    }
    else if (token.number == tnumber) {
        printf("Token -----> %d (%d, %d, %s, %d, %d)\n",
            token.value.num, token.number, token.value.num,
            token.fileName, token.lineNo, token.colNo);
    }
	else if (token.number == tcharLit) {                       // 추가
		printf("Token -----> '%c' (%d, %c, %s, %d, %d)\n",
			token.value.ch, token.number, token.value.ch,
			token.fileName, token.lineNo, token.colNo);
	}
	else if (token.number == tstringLit) {                     // 추가
		printf("Token -----> \"%s\" (%d, %s, %s, %d, %d)\n",
			token.value.str, token.number, token.value.str,
			token.fileName, token.lineNo, token.colNo);
	}
	else if (token.number == tdoubleLit) {					  // 추가
		printf("Token -----> %g (%d, %g, %s, %d, %d)\n",
			token.value.dnum, token.number, token.value.dnum,
			token.fileName, token.lineNo, token.colNo);
	}
    else {
        printf("Token -----> %s (%d, 0, %s, %d, %d)\n",
            tokenName[token.number], token.number,
            token.fileName, token.lineNo, token.colNo);
    }
}