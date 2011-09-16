#define MIN(a, b) ((a) > (b) ? (b) : (a))
int read_make(const char *line, char *buf, int buflen, int *l, int *c)
{
	const char *start = strchr(line, ':');
	if(start && sscanf(start, ":%d:%d:", l, c) == 2){
		snprintf(buf, MIN(start - line + 1, buflen), "%s", line);
		return 1;
	}
	return 0;
}
#undef MIN

void cmd_mak(int argc, char **argv, int force, struct range *rng)
{
	struct list *mak, *iter;
	char fname[32] = "/tmp/uvi_mak_XXXXXX";
	char *cmd;
	char *tee;
	int fd;
	int ret;
	int markch = '0';
	int added_bufs = 0;

	fd = mkstemp(fname);

	if(fd == -1){
		gui_status(GUI_ERR, "mkstemp: %s", strerror(errno));
		return;
	}

	if(argc == 1){
		cmd = ustrdup("make");
	}else{
		char *args = argv_to_str(argc - 1, argv + 1);
		cmd = ustrprintf("make %s", args);
		free(args);
	}

	tee = ustrprintf("%s 2>&1 | tee %s", cmd, fname);

	ret = shellout(tee, NULL);
	remove(fname);

	mak = list_from_fd(fd, NULL);
	close(fd);
	unlink(fname);

	for(iter = mak; iter && iter->data; iter = iter->next){
		char buf[256];
		int l, c;
		if(read_make(iter->data, buf, sizeof buf, &l, &c)){
			int i;

			l--;
			c--;

			i = mark_find(l, c);
			if('0' <= i && i <= markch)
				continue; /* duplicate */

			i = buffers_count();
			buffers_add(buf);
			if(i != buffers_count())
				added_bufs = 1;
			mark_set(markch++, l, c);
			if(markch > '9')
				break;
		}
	}

	gui_status(GUI_NONE, ret ? "make failed" : "make finished, %d marks ('0 to '%c)%s",
			markch - '0', markch - 1, added_bufs ? " (added buffers)":"");

	list_free(mak, free);
	free(cmd);
	free(tee);
}
