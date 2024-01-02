/*
 *	NMH's Simple C Compiler, 2011,2012,2014
 *	Preprocessor
 */

#include "defs.h"
#include "data.h"
#include "decl.h"
#include "prec.h"

struct MacroBuf {
	char params[NAMELEN+1];
	char args[TEXTLEN+1];
};

static struct MacroBuf Macexp[MAXFNARGS];

static int p_primary();

static int append(int k, char *buf, char *s, int maxlen, int quote) {
	int n;
	char *p;
	p = buf + k;
	n = 0;
	if (quote && n + k < maxlen) {
		p[n] = '"'; n++;
	}

	while (n + k < maxlen - 1 && *s != '\0') {
		if (quote) {
			if (*s == '\\') {
				p[n] = '\\'; n++;
				p[n] = '\\'; n++;
			} else if (*s == '"') {
				p[n] = '\\'; n++;
				p[n] = '"'; n++;
			} else {
				p[n] = *s;
				n++;
			}
		} else {	
				p[n] = *s;
				n++;
		}
		s++;
	}
	if (quote && n + k < maxlen) {
		p[n] = '"'; n++;
	}
	p[n] = '\0';
	if (k + n >= maxlen) 
		fatal("buffer overflow in macro expansion");
	return k + n;
}

static int replace(int na, int k, char *buf, int maxlen) {
	int i;
	int q = 0;
	char *tok;
	tok = buf + k;
	if (*tok == '#') {
		tok++;
		q = 1;
	}
	for (i = 0; i < na; i++) {
		if (!strcmp(tok, Macexp[i].params)) 
		{
			buf[k] = '\0';
			return append(k, buf, Macexp[i].args, maxlen, q);
		}
	}
	return strlen(buf);
}

static char *expandbody(char *s, int na) {
	int c, ni, i;
	char *buf;
	buf = Macbuf + (Mp * (TEXTLEN+1));
	buf[0] = '\0';
	ni = 0;
	i = 0;
	if (O_preponly) {
		fprintf(Outfile, "\n: %s\n", s); 
	}
	while (*s && i < TEXTLEN) {
		if (*s == '"' || *s == '\'') {
			c = *s;
			if (ni < i) {
				buf[i] = '\0';
				i = replace(na, ni, buf, TEXTLEN);
			}
			ni = i + 1;	
			buf[i] = *s;
			s++; i++;
			while (*s && *s != c && i < TEXTLEN) {
				if (*s == '\\') {
					buf[i] = *s;
					s++; i++;
				}
				buf[i] = *s;
				s++; i++;
			}
			if (*s == c && i < TEXTLEN) {
				buf[i] = *s;
				s++; i++;
			}
			ni = i;
			continue;
		}
		if (s[0] == '#' && s[1] == '#') {
			buf[i] = '\0';
			i = replace(na, ni, buf, TEXTLEN);
			ni = i;
			s += 2;
			continue;
		}
		if (!(isalpha(*s) || isdigit(*s) || '_' == *s)) {
			buf[i] = '\0';
			if (ni < i) {
				i = replace(na, ni, buf, TEXTLEN);
			}
			if (*s == '#') 
				ni = i;
			else 	
				ni = i + 1;
		}
		buf[i] = *s;
		i++; s++;
	}
	buf[i] = '\0';
	if (ni < i) {
		replace(na, ni, buf, TEXTLEN);
	}
	if (O_preponly) {
		fprintf(Outfile, "\nXPND: %s\n", buf); 
	}
	return buf;
}

static int getargs(void) {
	int l, na, k;
	Token = scanraw();
	if (Token != LPAREN) fatal("missing '(' after macro name");
	l = 1;
	na = 0;
	k = 0;
	Macexp[na].args[k] = '\0';
	//Token = scanraw();
	while (l > 0  && Token != XEOF && na < MAXFNARGS - 1) {
		Token = scanraw();
		if (Token == RPAREN) l--;
		if (Token == LPAREN) l++;
		if (Token == COMMA && l == 1) {
			na++;
			k = 0;
			Macexp[na].args[k] = '\0';
		} else if (l > 0) {
			k = append(k, Macexp[na].args, Text, TEXTLEN, 0);
		} else {
			if (k != 0) na++;
		}
	}
	Macexp[na].args[k] = '\0';
	if (O_preponly) {
		fprintf(Outfile, "\nr: %d %s\n", na, Macexp[0].args); 
	}
	if (l != 0) fatal("Missing ')' after macro call");
	return na;
}

static int getparams(char **ps) {
	int np, k;
	char *s;
	s = *ps;
	np = 0;
	k = 0;
	Macexp[np].params[k] = '\0';
	s++;
	while ('\0' != *s && np < MAXFNARGS - 1) {
		if (')' == *s) {
			s++;
			break;
		}
		if (',' == *s) {
			np++;
			k = 0;
			Macexp[np].params[k] = '\0';
		} else {
			if (k >= NAMELEN) 
				fatal("parameter name too long in macro");

			if (!isspace(*s)) {
				Macexp[np].params[k] = *s;
				k++;
				Macexp[np].params[k] = '\0';
			}
		}
		s++;
	}
	if (k != 0) np++;
	*ps = s;
	return np;
}

static char *expandmac(char *s) {
	int na, np;
	na = getargs();
	np = getparams(&s);
	if (na != np) fatal("wrong number of arguments to macro");
	return expandbody(s, na);
}

void playmac(int y) {
	char *s;
	int obase;
	int i;
	char *p;
	
	s = Mtext[y];
	if (Mp >= MAXNMAC) fatal("too many nested macros");
	if ('(' == s[0]) {
		s = expandmac(s);
	}
	if (Putback) {
		Macc[Mp] = next();
	} else {
		Macc[Mp] = ' ';
	}
	Macy[Mp] = y;
	Macp[Mp++] = s;
}

int getln(char *buf, int max) {
	int	k;

	if (fgets(buf, max, Infile) == NULL) return 0;
	k = strlen(buf);
	if (k && '\n' == buf[k-1]) buf[--k] = 0;
	if (k && '\r' == buf[k-1]) buf[--k] = 0;
	return k;
}

static void defmac(void) {
	char	name[NAMELEN+1];
	char	buf[TEXTLEN+1], *p;
	int	y;
	int 	l;
	char    *co;
	char    *ci;

	Token = scanraw();
	if (Token != IDENT)
		error("identifier expected after '#define': %s", Text);
	copyname(name, Text);
	buf[0] = ' ';
	buf[1] = 0;
	if ('(' == Putback) {
		putback(' ');
		buf[0] = '(';
	}
	l = 0;
	if ('\n' == Putback)
		buf[0] = 0;
	else 
		l = getln(buf+1, TEXTLEN-2)+1;

	while (l > 0 && l < TEXTLEN-2 && buf[l-1] == '\\') {
		l--;
		buf[l] = 0;
		Line++;
		l += getln(buf+l, TEXTLEN-1-l);
	}
	p = buf;
	if (' ' == *p) {
		for (; isspace(*(p+1)); p++)
			;
	}
	co = p; /* FIXME don't strip comments if in string literal */
	while (*co) {
		if (*co == '/') {
			co++;
			if (*co == '/') {
				*(co-1) = '\0';
				break;
			} else if (*co == '*') {
				*(co-1) = '\0';
				co++;
				while (*co && *co != '*' && *(co+1) != '/') {
					co++;
				}
				if (*co == '*') {
					ci = co;
					while (co[2]) {
						*co = co[2];
						co++;
					}
					co = ci;
				}
			}
			if (!*co) break;
		}
		co++;
	}	
	if ((y = findmac(name)) != 0) {
		if (strcmp(Mtext[y], p)) /* FIXME: should be p not buf??? */
			error("macro redefinition: %s", name);
	}
	else {
		addglob(name, 0, TMACRO, 0, 0, 0, globname(p), 0);
	}
	Line++;
}

static void undef(void) {
	char	name[NAMELEN+1];
	int	y;

	Token = scanraw();
	copyname(name, Text);
	if (IDENT != Token)
		error("identifier expected after '#undef': %s", Text);
	if ((y = findmac(name)) != 0)
		Names[y] = "#undef'd";
}

static void printline()
{
	fprintf(Outfile, "\n# %d \"%s\"\n", Line, File);
}

void pretop(void)
{
	if (Newline) {
		fprintf(Outfile, "\n");
		Newline = 0;
	}
	while (Spaces >= 8) {
		Spaces -= 8;
		fprintf(Outfile, "\t", Text);
	}
	while (Spaces > 0) {
		Spaces--;
		fprintf(Outfile, " ", Text);
	}
	fprintf(Outfile, "%s ", Text);
        Token = scan();
}

static void include(void) {
	char	file[TEXTLEN+1], path[TEXTLEN+1];
	int	c, k;
	FILE	*inc, *oinfile;
	char	*ofile;
	int	oc, oline;
	char	*p;
	char	*pi;

	if ((c = skip()) == '<')
		c = '>';
	k = getln(file, TEXTLEN-strlen(SCCDIR)-9);
	Line++;
	while (k > 0 && file[k-1] != c) {
		k--;
	}
	if (!k || file[k-1] != c)
		error("missing delimiter in '#include'", NULL);
	if (k) file[k-1] = 0;
	if (c == '"') {
		strcpy(path, file);
		if ((inc = fopen(path, "r")) == NULL) {
			strcpy(path, File);
			p = path + strlen(path);
			while (p > path) {
				p--;
				if (*p == '/') {
					p[1] = '\0';
					break;
				}
			}
			strcat(path, file);
		} else 
			fclose(inc);
	} else {
		pi = Includes;
		inc = NULL;
		if (pi[0]) {
			do {
				strcpy(path, pi);
				strcat(path, "/");
				strcat(path, file);
				pi = pi + strlen(pi) + 1;
			} while (((inc = fopen(path, "r")) == NULL) && pi[0]); 
		}
		if (inc) {
			fclose(inc);
		} else {
			strcpy(path, SCCDIR);
			strcat(path, "/include/");
			strcat(path, file);
		}
	}
	if ((inc = fopen(path, "r")) == NULL)
		error("cannot open include file: %s", path);
	else {
		Inclev++;
		oc = next();
		oline = Line;
		ofile = File;
		oinfile = Infile;
		Line = 1;
		putback('\n');
		File = path;
		Infile = inc;
		if (O_preponly) {
			printline();
			Token = scan();
			while (XEOF != Token)
				pretop();
		} else {
			Token = scan();
			while (XEOF != Token)
				top();
		}
		Line = oline;
		File = ofile;
		Infile = oinfile;
		fclose(inc);
		putback(oc);
		Inclev--;
	}
}

static void ifdef(int expect) {
	char	name[NAMELEN+1];

	if (Isp >= MAXIFDEF)
		fatal("too many nested '#ifdef's");
	Token = scanraw();
	copyname(name, Text);
	if (IDENT != Token)
		error("identifier expected in '#ifdef'", NULL);
	if (frozen(1))
		Ifdefstk[Isp++] = P_IFNDEF;
	else if ((findmac(name) != 0) == expect)
		Ifdefstk[Isp++] = P_IFDEF;
	else
		Ifdefstk[Isp++] = P_IFNDEF;
}

static int p_opprec(int tok)
{
	switch (tok) {
	case LOGOR:
		return 5;
	case LOGAND:
		return 6;
	case EQUAL:
	case NOTEQ:
		return 10;
	case GREATER:
	case GTEQ:
	case LESS:
	case LTEQ:
		return 11;
	}
	return -1;
}

static int p_rightass(int tok)
{
	switch (tok) {
	case NOTEQ:
		return 1;
	}
	return 0;
}

/*
 * https://en.wikipedia.org/wiki/Operator-precedence_parser
 */
static int p_exprlist(int lhs, int minprec)
{
	int op, rhs, opprec;
	while (p_opprec(Token) >= minprec) {
		op = Token;
		opprec = p_opprec(Token);
		Token = scanraw();
		rhs = p_primary();
		while (p_opprec(Token) > opprec ||
				(p_opprec(Token) == opprec && 
				  p_rightass(Token))) 
		{
			rhs = p_exprlist(rhs, opprec + 
				((p_opprec(Token) > opprec) ? 1 : 0));
		}
		switch (op) {
		case LOGOR: lhs = lhs || rhs; break;
		case LOGAND: lhs = lhs && rhs; break;
		case EQUAL: lhs = lhs == rhs; break;
		case NOTEQ: lhs = lhs != rhs; break;
		case GREATER: lhs = lhs > rhs; break;
		case GTEQ: lhs = lhs >= rhs; break;
		case LESS: lhs = lhs < rhs; break;
		case LTEQ: lhs = lhs <= rhs; break;
		default:
			error("'#if' unknown '%s'", Text);
		}
	}
	return lhs;
}

static int p_primary()
{
	int r;
	int y;

	r = 0;
	switch (Token) {
	case XMARK:
		Token = scanraw();
		return !p_primary();
	case LPAREN:
		Token = scanraw();
		r = p_exprlist(p_primary(), 0);
		if (Token != RPAREN) {
			error("'#if defined' missing ')'...", NULL);
			return 0;
		}
		Token = scanraw();
		return r;
	case INTLIT:
		r = Value;
		Token = scanraw();
		return r;
	case IDENT:
		if (!strcmp("defined", Text)){
			Token = scanraw();
			if (Token == IDENT) {
				if (findmac(Text) != 0) {
					Token = scanraw();
					return 1;
				}
				Token = scanraw();
				return 0;
			}
			if (Token == LPAREN) {
				Token = scanraw();
				if (Token == IDENT) {

					if (findmac(Text) != 0) {
						r = 1;
					}
					Token = scanraw();
					if (Token == RPAREN) {
						Token = scanraw();
						return r;
					}
					error("'#if defined' "
							"missing ')'", NULL);
				}
			} 
			error("'#if defined' macro name missing", NULL);
		} else {
			if ((y = findmac(Text)) != 0) {
				Token = scanraw();
				return atoi(Mtext[y]);
			} else {
				Token = scanraw();
				return 0;
			}
		}
		break;
	case XEOF:
	case XEOL:
		error("'#if' expression is incomplete", NULL);
		return 0;
	default:
		error("'#if' unknown '%s'", Text);		
		Token = scanraw();
		return 0;
	}	
	return r;
}

static int p_expr()
{
	Token = scanraw();
	return p_exprlist(p_primary(), 0);
}

static void p_if(int incstk) {
	int 	k, l;
	if (incstk) {
		if (Isp >= MAXIFDEF)
			fatal("too many nested '#if's");
		Ifdefstk[Isp++] = P_IFNDEF;
	}
	if (frozen(2))
		Ifdefstk[Isp-1] = P_ELIFNOT;
	else if (p_expr()) 
		Ifdefstk[Isp-1] = P_IFDEF;
	else
		Ifdefstk[Isp-1] = P_IFNDEF;
}

static void p_elif(void) {
	if (!Isp)
		error("'#elif' without matching '#if'", NULL);
	else if (frozen(2))
		;
	else if (P_IFDEF == Ifdefstk[Isp-1])
		Ifdefstk[Isp-1] = P_ELIFNOT;
	else if (P_IFNDEF == Ifdefstk[Isp-1])
		p_if(0);
	else if (P_ELIFNOT == Ifdefstk[Isp-1])
		;
	else
		error("'#elif' without matching '#if'...", NULL);
}

static void p_else(void) {
	if (!Isp)
		error("'#else' without matching '#ifdef'", NULL);
	else if (frozen(2))
		;
	else if (P_IFDEF == Ifdefstk[Isp-1])
		Ifdefstk[Isp-1] = P_ELSENOT;
	else if (P_IFNDEF == Ifdefstk[Isp-1])
		Ifdefstk[Isp-1] = P_ELSE;
	else if (P_ELIFNOT == Ifdefstk[Isp-1])
		;
	else
		error("'#else' without matching '#ifdef'...", NULL);
		
}

static void endif(void) {
	if (!Isp)
		error("'#endif' without matching '#if'", NULL);
	else
		Isp--;
}

static void pperror(void) {
	char	buf[TEXTLEN+1];

	if ('\n' == Putback)
		buf[0] = 0;
	else
		getln(buf, TEXTLEN-1);
	error("#error: %s", buf);
	exit(1);
}

static char FNbuf[TEXTLEN];

static void setline(void) {
	char	buf[TEXTLEN+1], *p, *q;

	if ('\n' == Putback)
		buf[0] = 0;
	else
		getln(buf, TEXTLEN-1);
	Line = atoi(buf) - 1;
	if ((p = strchr(buf, '"')) != NULL) {
		p++;
		if ((q = strchr(p, '"')) != NULL) {
			*q = 0;
			File = strcpy(FNbuf, p);
		}
	}
}

static void junkln(void) {
	while (!feof(Infile) && fgetc(Infile) != '\n')
		;
	Line++;
}

int frozen(int depth) {
	int r;
	r = Isp >= depth &&
		(P_IFNDEF == Ifdefstk[Isp-depth] ||
		P_ELSENOT == Ifdefstk[Isp-depth] ||
		P_ELIFNOT == Ifdefstk[Isp-depth]);
	return r;
}

void preproc(void) {
	putback('#');
	Token = scanraw();
	if (	frozen(1) &&
		P_IFDEF != Token && P_IFNDEF != Token &&
		P_ELSE != Token && P_ENDIF != Token &&
		P_IF != Token && P_ELIF != Token
	) {
		junkln();
		return;
	}
	switch (Token) {
	case P_DEFINE:	defmac(); break;
	case P_UNDEF:	undef(); break;
	case P_INCLUDE:	include(); break;
	case P_IFDEF:	ifdef(1); break;
	case P_IFNDEF:	ifdef(0); break;
	case P_ELSE:	p_else(); break;
	case P_ENDIF:	endif(); break;
	case P_IF:	p_if(1); break;
	case P_ELIF:	p_elif(); break;
	case P_ERROR:	pperror(); break;
	case P_LINE:	setline(); break;
	case P_PRAGMA:	junkln(); break;
	default:	junkln(); break;
			break;
	}
}
