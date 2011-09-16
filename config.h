#ifndef CONFIG_H
#define CONFIG_H

#if 0
#define COMMENT_COLOUR COLOR_BLUE
#define COMMENT_ATTRIB A_BOLD

#define QUOTE_COLOUR   COLOR_GREEN
#define QUOTE_ATTRIB   A_NORMAL

static const syntax syntaxs[] = {
	{ "#",  "\n",   COMMENT_COLOUR, COMMENT_ATTRIB },
	{ "/*", "*/",   COMMENT_COLOUR, COMMENT_ATTRIB },
	{ "//",  "\n",  COMMENT_COLOUR, COMMENT_ATTRIB },

	{ "\"",  "\"",  QUOTE_COLOUR  , QUOTE_ATTRIB   },
	{ "\'",  "'",   QUOTE_COLOUR  , QUOTE_ATTRIB   },
};

#define KEYWORD_COLOUR COLOR_YELLOW

static const char *keywords[] = {
	"TODO",
	"FIXME"
};
#endif

/* include :mak */
#define BLOAT

#endif
