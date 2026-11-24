# M5 Decision Record

1. M4 fixes: anything in your M4 that did not work and how you fixed it, or "none".
    * I'm going to state "none" here, not because my M4 submission was perfect, but
        because I couldn't diagnose issues based on the Autograder feedback from M4.
        As far as I can tell this solution should meet all of the requirements, but
        it's still failing the same autograder tests from the last Milestone and I
        was unable to figure out why.

2. Threads and shared data: each thread, and the statement required by constraint 4.
    * There are two threads in this solution, the GUI thread and the build_worker thread.
    * In response to constraint 4:
        - pending_build_: The GUI thread modifies it; worker thread removes it. (protected by build_mutex_)
        - active_cancellation_: The GUI thread updates it and the worker thread does not directly access it. (protected by build_mutex_)
        - stopping_build_worker_: The GUI thread sets it during when stopping or canceling and the worker thread reads it. (protected by build_mutex_ and is an atomic bool)
        - BuildRequest::cancellation: Belongs to each build request and is shared by the two threads. (It's an atomic bool)
        - build_condition_: The worker thread waits on it and the GUI thread notifies it. (Used with build_mutex_)
        - build_futures_: GUI thread uses this one itself, and it contains build ID's/associated futures. (protected by build_mutex_)
        - BuildRequest::promise: The worker thread sets its value. The GUI thread gest it with build_futures_. Promise/future is synched by std::promise/std::future, and the unordered map access is protected by build_mutex_.

3. States: each state, what it means, and which events change it, as a table or a diagram.
    * There are four states; idle, stale, ready, and building. As best I can type it up, here is the relation:
         State      |               Meaning               |         Transition causes
    -------------------------------------------------------------------------------------------
          Idle      | Invalid Corpus w/ no Active Build   | To Building -> Build requested
         Stale      | Active build w/ changed workspace   | To Building -> Build requested
                    |                                     | To Idle -> Discards a current build
                    |                                     |         -> Workspace Reset
        Building    | A build is active w/ invalid corpus | To Stale -> Workspace Changed
                    |                                     | To Ready -> Build successful/current
                    |                                     | To Idle -> Build failed/discarded
                    |                                     |         -> Build reset
         Ready      | Valid Corpus w/ no Active build     | To Building -> Build requested
                    |                                     | To Stale -> Workspace Changed 
                    |                                     |                 w/ active build
                    |                                     | To Idle -> Workspace Reset

4. One event sequence that threatens R1 to R4, traced step by step through your functions on
both threads.
    * Let's say you start a build using build_corpus() and request_build_from_state(). This method creates one queued BuildRequest, sets pending_build_, and notifies build_condition_. The existing build worker thread in build_worker() then wakes, takes pending_build_ under build_mutex_, and begins rebuilding the request’s copied workspace snapshot on that thread. If we then add another document in the GUI thread using add_document, corpus_state_->workspaceChanged() calls workspace_changed_from_state(), which marks workspace_changed_, calls cancel_active_build(), cancels the active build, invalidates the corpus, and enters StaleBuildingCorpusState. build_worker() observes the cancellation through request->cancellation and stops wihout rebuilding or publishing an old corpus. It then sets the result promise and invokes corpus_build_finished() on the GUI thread. Returning to that GUI thread, build_finished_from_state() sees workspace_changed_ and calls discard_build_from_state(), preventing the result from becoming current. Because the worker checks cancellation periodically, a later build does not wait for the obsolete build to finish. Eventually, once all work is done, exit calls closeEvent() and stop_build(). stop_build() sets stopping_build_worker_, cancels the request, and wakes build_condition_ before joining build_thread_. The worker returns promptly and is joined, satisfying clean shutdown.

5. Your answers to the three decisions in Section 4, one or two sentences each, naming the
functions that carry them out.
    3.1: The user can request to rebuild while already building, which calls request_build_from_state(). That cancels the current build and any other pending requests and makes our pending_build_ the new request.
    3.2: With the CR-1 instructions being posted so late I still didn't get the chance to properly implement remote adds. :(
    3.3: The minimum shared interface needs to be the pending_build_ and build_condition to hand off work to the worker thread, the stopping_build_worker_ flag for shutdowns, the promise/future pairs for BuildResults, and the build_futures_ map to tie build requests to their correct ID. The GUI adds requests, leading to the worker being woken up/canceling builds, and having to report data/identification information. If we tried to go any smaller it just would not work.

6. Where each fault switch is and which test catches it.
    * AIWS_FAULT_NO_CANCEL: Changes MainWindow::build_worker() so a build that has already started only uses shutdown cancellation and no longer works for replacements. It is covered by the replacement_build_does_not_wait_for_canceled_build() test.
    * AIWS_FAULT_SILENT_SHUTDOWN: Makes MainWindow::stop_build() skip build_condition_.notify_all() while a build is active. It is covered by the destroying_window_during_build_joins_worker() test.

7. AI-use disclosure, required but ungraded.
    * Inquiries about structure implementation (how I settled on states/transitions), proofreading certain comments/text entries, and debugging as I was writing code as well as after receiving autograder feedback