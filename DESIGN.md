# M4 Decision Record

(1) threads and ownership, including the statement for constraint 5;
    * GUI Thread - Owns the Main Window, associated widgets, and the workspace/processing core
    * Rebuild Thread - Owns a copy of the workspace to rebuild the corpus
    * Constraint 5 - cancel_build_, a handoff between threads with future.get() in MainWindow.cpp, and
                        in the destructor for MainWindow


(2) how a finished build is judged valid;
    A finished build is judged valid if:
        a. There were no changes to the workspace while the build was processing (new documents/rebuilds)
        b. No errors occured during the rebuild
    If both of those conditions are met the rebuild is deemed valid and used going forward.


(3) changes since the checkpoint;
    * No work was done for the checkpoint so here is my answer to the original questions:
        1. GUI Thread - Owns the Main Window, associated widgets, and the workspace/processing core
            Rebuild Thread - Owns a copy of the workspace to rebuild the corpus
        2. The states of the corpus itself are built/not built. The wider application has more states
            but for this milestone, the corpus' build status is all that matters. The corpus initializes
            as unbuilt until initiated by the user upon pressing the "build" button. When the user presses
            the button, it creates another thread to build the corpus. Upon successful completion the
            corpus is validated, and remains so until;
                a. A new document is added
                b. The corpus is reset
            Immediately upon either of those actions occuring, the corpus is invalidated and a rebuild is
            required.
        3.  R1 after M3 was threatened by the fact that processing occured on the GUI thread, which would
            have hindered responsiveness. To ensure that the GUI remains responsive, I create a
            promise/future pair which allows me to rebuild from a snapshot on a separate thread, while
            taking note in the GUI thread that a rebuild is occuring. Because the rebuild is managed on
            that separate thread, it keeps the GUI thread free to continue responding to user inputs.
            To ensure R2 is maintained, I have a boolean in place that tracks any changes that would
            impact corpus validity when the code is running. For example, once I start building a corpus
            I set a boolean workspace_changed_ to false. If at any point during the build, I add a 
            document or reset, that boolean becomes true. Then after my corpus is built, I check to see if
            that variable is still false. If it is, I know that the corpus I've just built is still valid,
            and I display it to the user. If it is true, then the corpus was changed and must be rebuilt,
            which I tell the user.
        4.  Main Window owns the processing core as a member function. Once a build begins, we use a new 
            core, which is owned by BuildResult::core. Once the build finishes successfully, that core
            is moved to/owned by Main Window.
        5.  I originally looked into QThread, and began working with it. It was a very intuitive system
            and made implementation, however, it was unfortunately out of scope, and so while I liked it,
            I was unable to proceed with it.


(4) CR-1 impact, meaning which functions changed and why the rest did not;
    * I was not able to implement CR-1. There was little instructional material
        about Unix sockets or resources provided to address them. With no prior
        experience I struggled to formulate a solution that I could understand
        without relying substantially on AI.

(5) where each fault switch is and which test catches it;
    * Both fault switches lie in my MainWindow::corpus_build_finished() function.
    * AIWS_FAULT_ACCEPT_STALE bypasses my check for whether or not the workspace was changed. This is
        validated using base_build_test and reset_while_building.
    * AIWS_FAULT_LOSE_FAILURE ignores my error reporting and is validated in report_rebuild_failure()


(6) AI-use disclosure, required but ungraded.
    * AI used to assist in the creation of test cases and debugging of multithreaded work
        in main_window.cpp as well as providing pointers for solving issues with the autograder.