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
			char *fname;
			int i;

			if(l > 0) l--;
			if(c > 0) c--;

			i = mark_find(l, c);
			if('0' <= i && i <= markch)
				continue; /* duplicate */

			i = buffers_count();

			if(!strncmp(buf, "In file included from ", 22))
				fname = buf + 22;
			else
				fname = buf;
			buffers_add(fname);

			if(i != buffers_count())
				added_bufs++;
			mark_set(markch++, l, c);
			if(markch > '9')
				break;
		}
	}

	if(ret){
		gui_status(GUI_NONE, "make failed");
	}else{
		char buf[256];

		if(markch > '0'){
			snprintf(buf, sizeof buf, " ('0 to '%c)", markch - 1);
			if(added_bufs)
				snprintf(buf + strlen(buf), sizeof buf - strlen(buf), " (added %d buffer%s)",
					added_bufs, added_bufs == 1 ? "":"s");
		}else{
			*buf = '\0';
		}

		gui_status(GUI_NONE, "make finished, %d marks%s", markch - '0', buf);
	}

	list_free(mak, free);
	free(cmd);
	free(tee);
}
