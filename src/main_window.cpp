#include "aiws/gui/main_window.hpp"

#include "aiws/gui/context_widget.hpp"
#include "aiws/gui/search_widget.hpp"
#include "aiws/gui/term_inspector_widget.hpp"
#include "aiws/gui/workspace_widget.hpp"

#include <QHBoxLayout>
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

namespace aiws::gui
{

    MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
    {
        setWindowTitle("AI Workspace");

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
        workspace_changed_ = false;

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
    }

    MainWindow::~MainWindow()
    {
        if (build_thread_.joinable())
            build_thread_.join();
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

        QFileInfo fileInfo(filePath);
        QString fileName = fileInfo.fileName();

        QTextStream in(&file);
        QString fileContents = in.readAll();

        Document newDoc = Document(filePath.toStdString(), fileName.toStdString(), fileContents.toStdString());

        workspace_.add_document(newDoc);

        workspace_widget_->add_document(fileName);

        status_label_->setText("Document added; rebuild corpus");

        workspace_changed_ = true;

        invalidate_corpus();
        update_action_states();
    }

    void MainWindow::build_corpus()
    {
        if (corpus_building_)
            return;

        corpus_building_ = true;
        corpus_current_ = false;

        status_label_->setText("Building Corpus...");
        update_action_states();

        workspace_changed_ = false;

        Workspace tempWorkSpace = workspace_;

        auto promise = std::make_shared<std::promise<BuildResult>>();

        build_future_ = promise->get_future();

        // std::cout << "Creating Thread" << std::endl;

        build_thread_ = std::thread([this, tempWorkSpace = std::move(tempWorkSpace), promise, delay_ms = build_delay_ms_]() mutable
                                    {
            BuildResult result;

            try
            {
                if (delay_ms > 0)
                {
                    for (std::size_t i = 0;
                        i < tempWorkSpace.documents().size();
                        i++)
                    {
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(delay_ms));
                    }
                }
                result.core = std::make_unique<ProcessingCore>();

                result.core->rebuild(tempWorkSpace);
            }
            catch (...)
            {
                result.error = std::current_exception();
            }

            // Transfer the result to the future.
            promise->set_value(std::move(result));

            // Notify the GUI thread that the build has finished.
            QMetaObject::invokeMethod(
                this,
                "corpus_build_finished",
                Qt::QueuedConnection); });
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
        term_inspector_widget_->clear_statistics();
        update_action_states();
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

        for (int i = 0; i < documents.size(); ++i)
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
        // std::cout << "Widgets Reset" << std::endl;

        workspace_.clear();
        // std::cout << "Workspace Cleared" << std::endl;

        invalidate_corpus();
        // std::cout << "Corpus Invalidated" << std::endl;

        update_action_states();
        // std::cout << "Action States Updated" << std::endl;

        workspace_changed_ = true;

        status_label_->setText("Workspace Reset");
        // std::cout << "Done" << std::endl;
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

        workspace_widget_->set_build_corpus_enabled(has_documents);
        term_inspector_widget_->set_inspection_enabled(corpus_current_);

        if (search_widget_->has_query())
        {
            search_widget_->set_search_enabled(true);

            if (corpus_current_)
            {
                context_widget_->set_build_context_enabled(true);
            }
        }
        else
        {
            search_widget_->set_search_enabled(false);
            context_widget_->set_build_context_enabled(false);
        }
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

    // M4 Functions:

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

        QTextStream in(&file);
        QString fileContents = in.readAll();

        add_document_contents(path, fileContents);
        return true;
    }

    void MainWindow::add_remote_document(const QString &path, const QString &contents)
    {
        add_document_contents(path, contents);
    }

    void MainWindow::add_document_contents(const QString &path, const QString &contents)
    {
        if (path.isEmpty())
            return;

        QFileInfo fileInfo(path);
        const QString fileName = fileInfo.fileName();
        Document newDoc = Document(path.toStdString(), fileName.toStdString(), contents.toStdString());

        workspace_.add_document(newDoc);

        workspace_widget_->add_document(fileName);
        workspace_changed_ = true;

        invalidate_corpus();
        update_action_states();
    }

    void MainWindow::set_build_delay_ms(int ms_per_document)
    {
        build_delay_ms_ = ms_per_document;
    }

    void MainWindow::corpus_build_finished()
    {
        // std::cout << "In build finished" << std::endl;

        if (build_thread_.joinable())
            build_thread_.join();

        BuildResult result = build_future_.get();

        corpus_building_ = false;

// Check whether the workspace changed during the build.
#ifndef AIWS_FAULT_ACCEPT_STALE
        if (workspace_changed_)
        {
            status_label_->setText("Corpus Build Discarded");
            corpus_current_ = false;

            update_action_states();
            return;
        }
#endif

        // Handle build failures.
        if (result.error)
        {
#ifndef AIWS_FAULT_LOSE_FAILURE
            try
            {
                std::rethrow_exception(result.error);
            }
            catch (const std::exception &e)
            {
                status_label_->setText(
                    QString("Corpus Build Failed: %1").arg(e.what()));
            }
            catch (...)
            {
                status_label_->setText("Corpus Build Failed");
            }

            corpus_current_ = false;
            update_action_states();
#endif
            return;
        }

        // Accept the completed corpus.
        processing_core_ = std::move(*result.core);

        corpus_current_ = true;

        status_label_->setText("Corpus Built Successfully");

        QString message = QString("Corpus: Ready - %1 Chunks")
                              .arg(processing_core_.chunk_count());

        workspace_widget_->set_corpus_status(message);

        update_action_states();
    }

} // namespace aiws::gui
