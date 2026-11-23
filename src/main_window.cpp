#include "aiws/gui/main_window.hpp"

#include "aiws/gui/context_widget.hpp"
#include "aiws/gui/search_widget.hpp"
#include "aiws/gui/term_inspector_widget.hpp"
#include "aiws/gui/workspace_widget.hpp"

#include <QHBoxLayout>
#include <QCloseEvent>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>

#include <iostream>

#include <chrono>

namespace aiws::gui
{

    // Base Corpus State
    class CorpusState
    {
    public:
        virtual ~CorpusState() = default;
        virtual void buildRequested(MainWindow &) = 0;
        virtual void workspaceChanged(MainWindow &) = 0;
        virtual void reset(MainWindow &) = 0;
        virtual void buildFinished(MainWindow &, int, BuildResult) = 0;
    };

    // Corpus: Invalid, Build: Not In Progress
    class IdleCorpusState final : public CorpusState
    {
    public:
        void buildRequested(MainWindow &window) override
        {
            window.request_build_from_state();
        }
        void workspaceChanged(MainWindow &window) override
        {
            window.invalidate_corpus();
        }
        void reset(MainWindow &window) override
        {
            window.reset_from_state();
        }
        void buildFinished(MainWindow &window, int id, BuildResult) override
        {
            window.discard_build_from_state(id);
        }
    };

    // Corpus: Valid, Build: Not In Progress
    class ReadyCorpusState final : public CorpusState
    {
    public:
        void buildRequested(MainWindow &window) override
        {
            window.request_build_from_state();
        }
        void workspaceChanged(MainWindow &window) override
        {
            window.workspace_changed_from_state();
        }
        void reset(MainWindow &window) override
        {
            window.reset_from_state();
        }
        void buildFinished(MainWindow &, int, BuildResult) override {}
    };

    // Corpus: Invalid, Build: In Progress
    class BuildingCorpusState final : public CorpusState
    {
    public:
        void buildRequested(MainWindow &window) override
        {
            window.request_build_from_state();
        }
        void workspaceChanged(MainWindow &window) override
        {
            window.workspace_changed_from_state();
        }
        void reset(MainWindow &window) override
        {
            window.reset_from_state();
        }
        void buildFinished(MainWindow &window, int id, BuildResult result) override
        {
            window.build_finished_from_state(id, std::move(result));
        }
    };

    // Corpus: Invalid, Build: In Progress, Obsolete :(
    class StaleBuildingCorpusState final : public CorpusState
    {
    public:
        void buildRequested(MainWindow &window) override
        {
            window.request_build_from_state();
        }
        void workspaceChanged(MainWindow &window) override
        {
            window.cancel_active_build();
        }
        void reset(MainWindow &window) override
        {
            window.reset_from_state();
        }
        void buildFinished(MainWindow &window, int id, BuildResult) override
        {
            if (id == window.current_build_id_)
                window.discard_build_from_state(id);
        }
    };

    MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
    {
        setWindowTitle("AI Workspace");
        corpus_state_ = std::make_unique<IdleCorpusState>();

        auto *title = new QWidget(this);
        title->setStyleSheet(
            "background-color: #037ffc;");

        auto *title_layout = new QHBoxLayout(title);

        auto *title_label = new QLabel("AI Workspace", title);
        title_label->setStyleSheet(
            "color: white;"
            "font-size: 26px;"
            "font-weight: bold;");

        title_layout->addWidget(title_label);
        title_layout->addStretch();

        // Create the main widgets.
        workspace_widget_ = new WorkspaceWidget(this);
        search_widget_ = new SearchWidget(this);
        term_inspector_widget_ = new TermInspectorWidget(this);
        context_widget_ = new ContextWidget(this);

        reset_button_ = new QPushButton("Reset", this);
        reset_button_->setStyleSheet(
            "QPushButton {"
            "    background-color: #fc9d03;" // Green background
            "    color: white;"              // White text
            "    border-radius: 5px;"
            "    padding: 5px;"
            "}");

        exit_button_ = new QPushButton("Exit", this);
        exit_button_->setStyleSheet(
            "QPushButton {"
            "    background-color: #911600;" // Green background
            "    color: white;"              // White text
            "    border-radius: 5px;"
            "    padding: 5px;"
            "}");
        status_label_ = new QLabel("Ready", this);
        status_label_->setObjectName("currentStatus");

        auto *hlayout = new QHBoxLayout;
        hlayout->addWidget(term_inspector_widget_);
        hlayout->addWidget(context_widget_);

        auto *status = new QLabel("Current Status", this);
        QFont statusFont = status->font();
        statusFont.setBold(true);
        status->setFont(statusFont);

        auto *bottomlayout = new QHBoxLayout;
        bottomlayout->addWidget(status);
        bottomlayout->addWidget(status_label_);
        bottomlayout->addWidget(reset_button_);
        bottomlayout->addWidget(exit_button_);

        auto *main_layout = new QVBoxLayout;
        main_layout->addWidget(title);
        main_layout->addWidget(workspace_widget_);
        main_layout->addWidget(search_widget_);
        main_layout->addLayout(hlayout);
        main_layout->addLayout(bottomlayout);

        auto *central_widget = new QWidget(this);
        central_widget->setLayout(main_layout);
        setCentralWidget(central_widget);

        resize(900, 900);

        // M4 Changes
        corpus_building_ = false;

        // Connections
        connect(workspace_widget_,
                &WorkspaceWidget::add_document_requested,
                this,
                &MainWindow::add_document);

        connect(workspace_widget_,
                &WorkspaceWidget::build_corpus_requested,
                this,
                &MainWindow::build_corpus);

        connect(workspace_widget_,
                &WorkspaceWidget::document_selected,
                this,
                &MainWindow::document_selected);

        connect(search_widget_,
                &SearchWidget::search_requested,
                this,
                &MainWindow::search_requested);

        connect(search_widget_,
                &SearchWidget::query_changed,
                this,
                &MainWindow::query_changed);

        connect(search_widget_,
                &SearchWidget::source_document_requested,
                this,
                &MainWindow::source_document_requested);

        connect(search_widget_,
                &SearchWidget::selected_chunk_changed,
                term_inspector_widget_,
                &TermInspectorWidget::set_selected_chunk);

        connect(context_widget_,
                &ContextWidget::build_context_requested,
                this,
                &MainWindow::build_context_requested);

        connect(term_inspector_widget_,
                &TermInspectorWidget::term_inspection_requested,
                this,
                &MainWindow::term_inspection_requested);

        connect(reset_button_,
                &QPushButton::clicked,
                this,
                &MainWindow::reset_application);

        connect(exit_button_,
                &QPushButton::clicked,
                this,
                &MainWindow::exit_application);

        update_action_states();

        build_thread_ = std::thread(&MainWindow::build_worker, this);
    }

    MainWindow::~MainWindow()
    {
        stop_build();
    }

    // Handles state transitions
    void MainWindow::set_corpus_state(std::unique_ptr<CorpusState> state)
    {
        corpus_state_ = std::move(state);
    }

    // Starts or replaces a build, cancels old work, and enters the Building corpus state.
    void MainWindow::request_build_from_state()
    {
        // When we build a state, we note that we are actively building and that the
        //      new build will invalidate the old corpus.
        corpus_building_ = true;
        corpus_current_ = false;
        workspace_changed_ = false;

        // Update the status label and buttons available to the user.
        status_label_->setText("Building Corpus...");
        update_action_states();

        const int id = ++next_build_id_;
        current_build_id_ = id;

        // Create a new promise/future pair for this build, add that and the ID to
        //      an unordered map for build futures. Set up this way in case the user
        //      presses the build button multiple times. This allows for the safe
        //      replacement of builds as the program runs.
        auto current_promise = std::make_shared<std::promise<BuildResult>>();
        build_futures_.emplace(id, current_promise->get_future());

        auto cancellation = std::make_shared<std::atomic<bool>>(false);

        std::unique_ptr<BuildRequest> replaced_request;

        // Here we modify shared data between the GUI/worker thread, so we have to lock
        //      the mutex.
        {
            std::lock_guard<std::mutex> lock(build_mutex_);

            // If there is already an active cancellation (pointer exists), we make
            //      sure our atomic bool notes the cancellation.
            if (active_cancellation_)
            {
                active_cancellation_->store(true);
            }

            // If there is already a build pending (pointer exists), cancel it and
            //       retain ownership in replaced_request until the old
            //       std::future entry is removed.
            if (pending_build_)
            {
                pending_build_->cancellation->store(true);
                replaced_request = std::move(pending_build_);
            }

            // Create our new build request
            pending_build_ = std::make_unique<BuildRequest>(
                BuildRequest{
                    current_build_id_,
                    workspace_,
                    current_promise,
                    build_delay_ms_,
                    cancellation});

            // Track the new build's cancellation flag
            active_cancellation_ = cancellation;
        }

        // Get rid of the future belonging to the obsolete pending request.
        if (replaced_request)
            build_futures_.erase(replaced_request->id);

        // Notify the condition variable
        build_condition_.notify_one();

        // Transition to the Building State
        set_corpus_state(std::make_unique<BuildingCorpusState>());
    }

    void MainWindow::build_finished_from_state(int id, BuildResult result)
    {

        if (id != current_build_id_)
        {
            discard_build_from_state(id);
            return;
        }

        corpus_building_ = false;

        if (workspace_changed_)
        {
            discard_build_from_state(id);
            return;
        }

        if (result.error)
        {
            // Report error
            corpus_current_ = false;
            invalidate_corpus();
            set_corpus_state(std::make_unique<IdleCorpusState>());
            return;
        }

        processing_core_ = std::move(*result.core);
        corpus_current_ = true;

        status_label_->setText("Corpus Built Successfully");
        workspace_widget_->set_corpus_status(
            QString("Corpus: Ready - %1 Chunks")
                .arg(processing_core_.chunk_count()));

        update_action_states();
        set_corpus_state(std::make_unique<ReadyCorpusState>());
    }

    // Manages changes to the current state, if a change occurs, the state is now stale :(
    void MainWindow::workspace_changed_from_state()
    {
        workspace_changed_ = true;
        cancel_active_build();
        invalidate_corpus();
        set_corpus_state(std::make_unique<StaleBuildingCorpusState>());
    }

    void MainWindow::discard_build_from_state(int id)
    {
        auto future_it = build_futures_.find(id);
        if (future_it != build_futures_.end())
            build_futures_.erase(future_it);

        if (id != current_build_id_)
            return;

        corpus_building_ = false;
        corpus_current_ = false;

        status_label_->setText("Corpus Build Discarded");
        invalidate_corpus();
        set_corpus_state(std::make_unique<IdleCorpusState>());
    }

    // Resets the state and returns to Idle.
    void MainWindow::reset_from_state()
    {
        cancel_active_build();

        ++current_build_id_;
        corpus_building_ = false;
        workspace_changed_ = true;
        corpus_current_ = false;

        invalidate_corpus();
        set_corpus_state(std::make_unique<IdleCorpusState>());
    }

    // Closes the window after the user chooses to
    void MainWindow::closeEvent(QCloseEvent *event)
    {
        stop_build();
        QMainWindow::closeEvent(event);
    }

    // Stops the current build (go figure)
    void MainWindow::stop_build()
    {

        // Lock the mutex since it accesses shared data
        {
            std::lock_guard<std::mutex> lock(build_mutex_);

            // Set atomic bool to true to indicate the worker thread is stopping.
            stopping_build_worker_ = true;

            // If there is already an active cancellation (pointer exists), we make
            //      sure our atomic bool notes the cancellation.
            if (active_cancellation_)
            {
                active_cancellation_->store(true);
            }

            // If there is already a pending build (pointer exists), we make sure it's
            //       atomic bool notes the cancellation.
            if (pending_build_)
            {
                pending_build_->cancellation->store(true);
            }
        }

        build_condition_.notify_all();
        if (build_thread_.joinable())
            build_thread_.join();
    }

    void MainWindow::cancel_active_build()
    {
        {
            std::lock_guard<std::mutex> lock(build_mutex_);
            if (active_cancellation_)
                active_cancellation_->store(true);
            if (pending_build_)
                pending_build_->cancellation->store(true);
        }
        build_condition_.notify_all();
    }

    void MainWindow::set_exit_callback(ExitCallback callback) noexcept
    {
        exit_callback_ = callback;
    }

    void MainWindow::add_document()
    {
        QString filePath = QFileDialog::getOpenFileName(
            this,
            "Add Document",
            QString(),
            "Text Files (*.txt)");

        if (filePath.isEmpty())
            return;

        QFile file(filePath);

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            qDebug() << "Failed to open:" << file.errorString();
            return;
        }

        corpus_state_->workspaceChanged(*this);

        QFileInfo fileInfo(filePath);
        QString fileName = fileInfo.fileName();

        QTextStream in(&file);
        QString fileContents = in.readAll();

        Document newDoc = Document(filePath.toStdString(), fileName.toStdString(), fileContents.toStdString());

        workspace_.add_document(newDoc);

        workspace_widget_->add_document(fileName);

        status_label_->setText("Document added; rebuild corpus");

        invalidate_corpus();
        update_action_states();
    }

    void MainWindow::build_corpus()
    {
        corpus_state_->buildRequested(*this);
    }

    void MainWindow::build_worker()
    {
        while (true)
        {
            std::unique_ptr<BuildRequest> request;
            {
                std::unique_lock<std::mutex> lock(build_mutex_);
                build_condition_.wait(lock, [this]
                                      { return stopping_build_worker_ || pending_build_ != nullptr; });
                if (stopping_build_worker_)
                    return;
                request = std::move(pending_build_);
            }

            emit build_worker_started(request->id);

            BuildResult result;

            try
            {
                if (request->delay_ms > 0)
                {
                    for (std::size_t i = 0;
                         i < request->workspace.documents().size() &&
                         !request->cancellation->load();
                         i++)
                    {
                        std::unique_lock<std::mutex> lock(build_mutex_);
                        build_condition_.wait_for(
                            lock,
                            std::chrono::milliseconds(request->delay_ms),
                            [this, &request]
                            {
                                return request->cancellation->load() || stopping_build_worker_;
                            });
                        if (request->cancellation->load() || stopping_build_worker_)
                        {
                            break;
                        }
                    }
                }

                if (!request->cancellation->load())
                {
                    result.core = std::make_unique<ProcessingCore>();
                    result.core->rebuild(request->workspace, request->cancellation.get());
                }
            }
            catch (...)
            {
                result.error = std::current_exception();
            }

            request->promise->set_value(std::move(result));

            bool should_notify = false;
            {
                std::lock_guard<std::mutex> lock(build_mutex_);
                should_notify = !stopping_build_worker_;
            }
            if (should_notify)
            {
                QMetaObject::invokeMethod(
                    this,
                    "corpus_build_finished",
                    Qt::QueuedConnection,
                    Q_ARG(int, request->id));
            }
        }
    }

    void MainWindow::document_selected(int row)
    {
        const auto &documents = workspace_.documents();

        if (row < 0 || static_cast<std::size_t>(row) >= documents.size())
        {
            workspace_widget_->clear_selected_document();
            update_action_states();
            return;
        }

        const auto &document = documents[row];

        workspace_widget_->show_selected_document(
            QString::fromStdString(document.title()),
            QString::fromStdString(document.text()));

        update_action_states();
    }

    void MainWindow::search_requested(const QString &query, int k)
    {
        if (query.trimmed().isEmpty())
        {
            search_widget_->clear_results();
            return;
        }

        if (!corpus_current_)
        {
            search_widget_->clear_results();
            return;
        }

        auto results = processing_core_.search(query.toStdString(), k);
        search_widget_->set_results(results);
        status_label_->setText(QString("Search Completed - %1 result(s).").arg(results.size()));
        update_action_states();
    }

    void MainWindow::query_changed(const QString &query)
    {
        // This should always be true because search_widget properly handles query changes, but I didn't know if I could
        // remove the parameter since that came default with the instructions.
        if (query == search_widget_->query())
        {
            term_inspector_widget_->clear_statistics();
            update_action_states();
        }
    }

    void MainWindow::build_context_requested(int token_budget)
    {
        if (!corpus_current_ || !search_widget_->has_query())
            return;

        auto items = processing_core_.build_context(
            search_widget_->query().toStdString(),
            search_widget_->result_count(),
            static_cast<std::size_t>(token_budget));

        context_widget_->clear_context();

        context_widget_->set_context(items);

        status_label_->setText(QString("Context built - %1 item(s).").arg(items.size()));
    }

    void MainWindow::source_document_requested(const QString &document_id)
    {
        term_inspector_widget_->clear_statistics();
        const auto &documents = workspace_.documents();

        for (size_t i = 0; i < documents.size(); ++i)
        {
            if (documents[i].id() == document_id.toStdString())
            {
                workspace_widget_->select_document(i);
                break;
            }
        }

        status_label_->setText("Source document selected");
    }

    void MainWindow::term_inspection_requested(const QString &term,
                                               const QString &chunk_id)
    {
        if (!corpus_current_ || term.trimmed().isEmpty())
        {
            term_inspector_widget_->clear_statistics();
            return;
        }

        int docFreq = processing_core_.document_frequency(term.toStdString());

        bool hasTerm_Freq = false;
        int termFreq = 1;
        try
        {
            termFreq = processing_core_.term_frequency(term.toStdString(), chunk_id.toStdString());
            hasTerm_Freq = true;
        }
        catch (const std::invalid_argument &e)
        {
            hasTerm_Freq = false;
        }
        term_inspector_widget_->set_statistics(docFreq, hasTerm_Freq, termFreq);

        status_label_->setText("Term Inspected");

        update_action_states();
    }

    void MainWindow::reset_application()
    {
        std::vector<Resettable *> myWidgets = {context_widget_, term_inspector_widget_, search_widget_, workspace_widget_};
        for (auto *widget : myWidgets)
        {
            widget->reset();
        }

        workspace_.clear();
        workspace_changed_ = true;
        cancel_active_build();

        invalidate_corpus();

        update_action_states();

        status_label_->setText("Workspace Reset");
    }

    void MainWindow::exit_application()
    {
        if (exit_callback_ != nullptr)
        {
            exit_callback_(this);
        }
    }

    void MainWindow::update_action_states()
    {
        bool has_documents = !workspace_.documents().empty();
        bool can_process_query = corpus_current_ && search_widget_->has_query();

        workspace_widget_->set_build_corpus_enabled(has_documents);
        term_inspector_widget_->set_inspection_enabled(corpus_current_);
        search_widget_->set_search_enabled(can_process_query);
        context_widget_->set_build_context_enabled(can_process_query);
    }

    void MainWindow::invalidate_corpus()
    {
        workspace_widget_->set_corpus_status("Corpus: Not Built");
        workspace_widget_->set_build_corpus_enabled(false);
        corpus_current_ = false;

        clear_processing_outputs();
        update_action_states();
    }

    void MainWindow::clear_processing_outputs()
    {
        workspace_widget_->clear_selected_document();
        search_widget_->reset();
        term_inspector_widget_->reset();
        context_widget_->reset();
    }

    bool MainWindow::add_document_file(const QString &path)
    {

        if (path.isEmpty())
            return false;

        QFile file(path);

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            qDebug() << "Failed to open:" << file.errorString();
            return false;
        }

        workspace_changed_ = true;
        cancel_active_build();

        QFileInfo fileInfo(path);
        QString fileName = fileInfo.fileName();

        QTextStream in(&file);
        QString fileContents = in.readAll();

        Document newDoc = Document(path.toStdString(), fileName.toStdString(), fileContents.toStdString());

        workspace_.add_document(newDoc);

        workspace_widget_->add_document(fileName);
        invalidate_corpus();
        update_action_states();
        return true;
    }

    void MainWindow::set_build_delay_ms(int ms_per_document)
    {
        build_delay_ms_ = ms_per_document;
    }

    void MainWindow::corpus_build_finished(int id)
    {
        auto future_it = build_futures_.find(id);
        if (future_it == build_futures_.end())
            return;

        BuildResult result = future_it->second.get();
        build_futures_.erase(future_it);

        corpus_state_->buildFinished(*this, id, std::move(result));
    }

} // namespace aiws::gui
