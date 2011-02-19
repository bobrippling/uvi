
static void nc_up()
{
	static char init = 0;

	if(!init){
		initscr();
		noecho();
		cbreak();
#if NC_RAW
		raw(); /* use raw() to intercept ^C, ^Z */
#endif

		/* halfdelay() for main-loop style timeout */

		nonl();
		intrflush(stdscr, FALSE);
		keypad(stdscr, TRUE);

		/*ESCDELAY = 25; * duh **/

		pady = padx = 0;

		init = 1;
	}

	refresh();
}

static void gui_vstatus(const char *s, va_list l)
{
	if(strchr(s, '\n') || pfunc_wantconfimation)
		/*
		 * print a bunch of lines,
		 * and tell main() to wait for input from the user
		 */
		move(pfunc_wantconfimation++, 0);
	else
		move(MAX_Y, 0);

	clrtoeol();
#if VIEW_COLOUR
	if(global_settings.colour)
		coloron(COLOR_RED);
#endif
	vwprintw(stdscr, s, l);
#if VIEW_COLOUR
	if(global_settings.colour)
		coloroff(COLOR_RED);
#endif
}

void gui_status(const char *s, ...)
{
	va_list l;
	va_start(l, s);
	vgui_status(s, l);
	va_end(l);
}

static int nc_getch()
{
	int c;

#if NC_RAW
get:
#endif
	c = getch();
	switch(c){
#if NC_RAW
		case C_CTRL_C:
			sigh(SIGINT);
			break;

		case C_CTRL_Z:
			nc_down();
			raise(SIGSTOP);
			nc_up();
			goto get;
#endif

		/* TODO: CTRL_AND('v') */

		case 0:
		case -1:
		case C_EOF:
			return CTRL_AND('d');

		case C_DEL:
		case C_BACKSPACE:
		case C_ASCIIDEL:
			return '\b';

		case C_NEWLINE:
			return '\n';

		case C_TAB:
			return '\t';
	}

	return c;
}

