#!/bin/bash
# if you need enter password when exece make_kernel_and_debug_run.sh
# please editer sudoers `sudo visudo` add make_kernel_and_debug_run.sh in
# writelist. /etc/sudoers.d/[your_file]
CURRENT_SESSION=$(tmux display-message -p '#S')
SESSIONS=$(tmux list-sessions -F '#S')

session_name='qemu_session'
if [ "$CURRENT_SESSION" = $session_name ]; then
        echo "current session name is $CURRENT_SESSION !!" 
        echo "switch to NEW SESSION !!"
        echo "run ./scripts/debug.sh AGAIN!!"
        tmux new-session -d  # make sure at least 2 sessions
        exit 1
        # echo "this session name is $CURRENT_SESSION"
        # tmux switch-client -t new_tmp_session  # switch to next session
        # tmux kill-session -t $session_name
fi

if tmux has-session -t $session_name 2>/dev/null; then
        tmux kill-session -t $session_name
fi

tmux new-session -d -s $session_name
tmux rename-window -t qemu_session:1 'QEMU-monitor'
tmux send-keys -t qemu_session:1 'sudo ./scripts/debug/make_kernel_and_debug_run.sh' C-m
tmux split-window -v -t qemu_session:1
tmux send-keys -t qemu_session:1 './scripts/debug/gdb_qemu.sh' C-m

# switch to qemu session 
tmux switch-client -t qemu_session
