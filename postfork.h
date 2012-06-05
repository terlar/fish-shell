/** \file postfork.h

	Functions that we may safely call after fork(), of which there are very few. In particular we cannot allocate memory, since we're insane enough to call fork from a multithreaded process.
*/

#ifndef FISH_POSTFORK_H
#define FISH_POSTFORK_H

#include <wchar.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <list>
#include <spawn.h>

#include "config.h"
#include "common.h"
#include "util.h"
#include "proc.h"
#include "wutil.h"
#include "io.h"

/**
   This function should be called by both the parent process and the
   child right after fork() has been called. If job control is
   enabled, the child is put in the jobs group, and if the child is
   also in the foreground, it is also given control of the
   terminal. When called in the parent process, this function may
   fail, since the child might have already finished and called
   exit. The parent process may safely ignore the exit status of this
   call.

   Returns 0 on sucess, -1 on failiure. 
*/
int set_child_group( job_t *j, process_t *p, int print_errors );

/**
   Initialize a new child process. This should be called right away
   after forking in the child process. If job control is enabled for
   this job, the process is put in the process group of the job, all
   signal handlers are reset, signals are unblocked (this function may
   only be called inside the exec function, which blocks all signals),
   and all IO redirections and other file descriptor actions are
   performed.

   \param j the job to set up the IO for
   \param p the child process to set up

   \return 0 on sucess, -1 on failiure. When this function returns,
   signals are always unblocked. On failiure, signal handlers, io
   redirections and process group of the process is undefined.
*/
int setup_child_process( job_t *j, process_t *p );

/* Call fork(), optionally waiting until we are no longer multithreaded. If the forked child doesn't do anything that could allocate memory, take a lock, etc. (like call exec), then it's not necessary to wait for threads to die. If the forked child may do those things, it should wait for threads to die.
*/
pid_t execute_fork(bool wait_for_threads_to_die);

/** This class represents a list of things to do to a child process.
    This can either be 'executed' when using fork, or can be turned into a posix_spawnattr to pass to posix_spawn.
*/
class fork_actions_t {
    private:
        
    /** Whether we should set the parent group ID (and what to set it to) */
    bool should_set_parent_group_id;
    int desired_parent_group_id;
    
    /** Files to close */
    std::vector<int> files_to_close;
    
    /** A list of files to open, and the corresponding fd */
    struct fork_action_open_file_t {
        std::string path;
        int mode;
        int fd;
    };
    std::vector<fork_action_open_file_t> files_to_open;
    
    /** A list of file descriptors to re-map */
    struct fork_action_remap_fd_t {
        int from;
        int to;
    };
    std::vector<fork_action_remap_fd_t> files_to_remap;
    
    /* Whether to reset signal handlers */
    bool reset_signal_handlers;
    
    /* Whether to reset the sigmask */
    bool reset_sigmask;
    
    
    public:
    /* Constructor defaults everything to false. */
    fork_actions_t();
    
    /** Setup for a child process */
    void setup_for_child_process(job_t *j, process_t *p);
    
    /* Initializes and fills in a posix_spawnattr_t; on success, the caller should destroy it via posix_spawnattr_destroy */
    bool make_spawnattr(posix_spawnattr_t *result) const;
    
    /* Initializes and fills in a posix_spawn_file_actions_t; on success, the caller should destroy it via posix_spawnattr_destroy */
    bool make_file_actions(posix_spawn_file_actions_t *result) const;
};

#endif
