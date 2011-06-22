void cmd_mak(int argc, char **argv, int force, struct range *rng)
{
	struct list *mak;
	char fname[32] = "/tmp/uvi_mak_XXXXXX";
	char *cmd;
	char *tee;
	int fd;
	int ret;

	fd = mkstemp(fname);

	if(fd == -1){
		gui_status(GUI_ERR, "mkstemp: %s", strerror(errno));
		return;
	}

	cmd = argv_to_str(argc - 1, argv + 1);
	tee = ustrprintf("%s | tee %s", cmd, fname);

	ret = shellout(tee, NULL);
	remove(fname);

	gui_status(GUI_NONE, ret ? "make failed" : "make success");

	mak = list_from_fd(fd, NULL);

	/* TODO */

	free(cmd);
	free(tee);
}
