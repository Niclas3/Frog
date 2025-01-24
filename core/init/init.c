// first user process
// used by threads.c:
void init(void)
{
        while (1) {
        }

        /* uint_32 ret_pid = fork(); */
        /* if (ret_pid) { */
        /*     uint_32 ppid = getpid(); */
        /*     #<{(| printf("init pid is %d\n", getpid()); |)}># */
        /*     while (1) */
        /*         ; */
        /* } else { */
        /*     uint_32 cpid = getpid(); */
        /*     #<{(| printf("child pid is %d, ret id is %d\n", getpid(),
         * ret_pid); |)}># */
        /*     while (1) */
        /*         ; */
        /* } */
}
