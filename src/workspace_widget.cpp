#include "aiws/gui/workspace_widget.hpp"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>

namespace aiws::gui
{

    WorkspaceWidget::WorkspaceWidget(QWidget *parent) : QWidget(parent)
    {
        // TODO: Construct and lay out the Workspace and Selected Document areas,
        // connect local widget events, and establish initial presentation state.
        document_list_ = new QListWidget(this);
        add_document_button_ = new QPushButton("Add Document", this);
        build_corpus_button_ = new QPushButton("Build Corpus", this);
        corpus_status_label_ = new QLabel("Corpus: Not built", this);
        document_title_label_ = new QLabel("", this);

        document_text_ = new QPlainTextEdit(this);
        document_text_->setReadOnly(true);

        // ~-~-~-~-~-~-~-~-~-
        //    Layout stuff
        // ~-~-~-~-~-~-~-~-~-

        // Left side of Workspace
        auto *workspace_left_top = new QHBoxLayout;
        workspace_left_top->addWidget(new QLabel("Workspace", this));
        workspace_left_top->addWidget(add_document_button_);
        workspace_left_top->addWidget(build_corpus_button_);

        auto *workspace_left = new QVBoxLayout;
        workspace_left->addLayout(workspace_left_top);
        workspace_left->addWidget(document_list_);
        workspace_left->addWidget(corpus_status_label_);

        // Right Side of Workspace
        auto *workspace_right_top = new QHBoxLayout;
        workspace_right_top->addWidget(new QLabel("Selected Document", this));
        workspace_right_top->addWidget(document_title_label_);

        auto *workspace_right = new QVBoxLayout;
        workspace_right->addLayout(workspace_right_top);
        workspace_right->addWidget(document_text_);

        auto *main_layout = new QHBoxLayout(this);
        main_layout->addLayout(workspace_left);
        main_layout->addLayout(workspace_right);

        build_corpus_button_->setEnabled(false);

        connect(add_document_button_, &QPushButton::clicked,
                this, &WorkspaceWidget::add_document_requested);

        connect(build_corpus_button_, &QPushButton::clicked,
                this, &WorkspaceWidget::build_corpus_requested);

        connect(document_list_, &QListWidget::currentRowChanged,
                this, &WorkspaceWidget::document_selected);
    }

    void WorkspaceWidget::add_document(const QString &title)
    {
        document_list_->addItem(title);
    }

    void WorkspaceWidget::select_document(int row)
    {
        if (row < 0 || row >= document_list_->count())
        {
            document_list_->setCurrentRow(-1);
            return;
        }

        document_list_->setCurrentRow(row);
    }

    void WorkspaceWidget::show_selected_document(const QString &title,
                                                 const QString &text)
    {
        document_title_label_->setText(title);
        document_text_->setPlainText(text);
    }

    void WorkspaceWidget::clear_selected_document()
    {
        document_title_label_->setText("");
        document_text_->clear();
    }

    void WorkspaceWidget::set_corpus_status(const QString &text)
    {
        corpus_status_label_->setText(text);
    }

    void WorkspaceWidget::set_build_corpus_enabled(bool enabled)
    {
        build_corpus_button_->setEnabled(enabled);
    }

    void WorkspaceWidget::reset()
    {
        document_list_->clear();
        clear_selected_document();
        set_corpus_status("Corpus: Not built");
        set_build_corpus_enabled(false);
    }

} // namespace aiws::gui
