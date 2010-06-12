#ifndef CONFIG_H
#define CONFIG_H

#define PROG_NAME "uvi"
#define DEFAULT_TAB_STOP 2
#define VARS_MAX_TABSTOP 8
#define VIEW_COLOUR      1


#if VIEW_COLOUR

/*
 * valid colours:
 *  COLOR_BLACK
 *  COLOR_GREEN
 *  COLOR_WHITE
 *  COLOR_RED
 *  COLOR_CYAN
 *  COLOR_MAGENTA
 *  COLOR_BLUE
 *  COLOR_YELLOW
 *
 * Attributes:
 *  A_NORMAL
 *  A_STANDOUT
 *  A_UNDERLINE
 *  A_REVERSE
 *  A_BLINK
 *  A_DIM
 *  A_BOLD
 *  A_PROTECT
 *  A_INVIS
 *  A_ALTCHARSET
 *  A_CHARTEXT
 */

#define COMMENT_COLOUR COLOR_BLUE
#define COMMENT_ATTRIB A_BOLD

#define QUOTE_COLOUR COLOR_GREEN
#define QUOTE_ATTRIB A_STANDOUT

#define SYNTAXES struct \
	{ \
		const char *const start, *const end; \
		const short slen, elen; \
		const int colour, attrib; \
	} const syntax[] = { \
		{ "#",  "\n",   1, 1, COMMENT_COLOUR, COMMENT_ATTRIB }, \
		{ "/*", "*/",   2, 2, COMMENT_COLOUR, COMMENT_ATTRIB }, \
		{ "//",  "\n",  2, 1, COMMENT_COLOUR, COMMENT_ATTRIB }, \
\
		{ "\"",  "\"",  1, 1, QUOTE_COLOUR  , QUOTE_ATTRIB   }, \
		{ "\'",  "'",   1, 1, QUOTE_COLOUR  , QUOTE_ATTRIB   }, \
\
/*	{ "*",   "*",   1, 1,              0, A_BOLD         }, \
		{ "_",   "_",   1, 1,              0, A_UNDERLINE    },*/ \
	}
#endif


struct settings
{
	char tabstop, showtabs;
#if VIEW_COLOUR
	char colour;
#endif
};

#endif
