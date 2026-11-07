#include "aiws/gui/search_widget.hpp"
#include "aiws/gui/search_result_model.hpp"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableView>
#include <QVBoxLayout>
#include <QModelIndex>

namespace aiws::gui
{

    SearchWidget::SearchWidget(QWidget *parent) : QWidget(parent)
    {
        // TODO: Construct and lay out the Query, Search Results, and Selected Result
        // areas; configure the table/model; connect local events; establish initial state.
        query_edit_ = new QLineEdit(this);
        query_edit_->setPlaceholderText("Enter search query");

        result_count_spin_ = new QSpinBox(this);
        result_count_spin_->setValue(3);
        result_count_spin_->setRange(1, 100);

        search_button_ = new QPushButton("Search", this);
        search_button_->setEnabled(false);

        result_model_ = new SearchResultModel(this);
        results_view_ = new QTableView(this);
        results_view_->setModel(result_model_);
        results_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
        results_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        results_view_->horizontalHeader()->setStretchLastSection(true);

        selected_result_text_ = new QPlainTextEdit(this);
        selected_result_text_->setPlaceholderText("Select a result to inspect its details");
        selected_result_text_->setReadOnly(true);

        view_source_button_ = new QPushButton("View Source", this);

        auto *query_label = new QLabel("Query:", this);
        QFont query_font = query_label->font();
        query_font.setBold(true);
        query_label->setFont(query_font);

        auto *results_label = new QLabel("Results:", this);
        QFont results_font = results_label->font();
        results_font.setBold(true);
        results_label->setFont(results_font);

        auto *query_layout = new QHBoxLayout;
        query_layout->addWidget(query_label);
        query_layout->addWidget(query_edit_);
        query_layout->addWidget(results_label);
        query_layout->addWidget(result_count_spin_);
        query_layout->addWidget(search_button_);

        auto *selected_result_label = new QLabel("Selected Result:", this);
        QFont selected_font = selected_result_label->font();
        selected_font.setBold(true);
        selected_result_label->setFont(selected_font);

        auto *selected_layout = new QHBoxLayout;
        selected_layout->addWidget(selected_result_label);
        selected_layout->addStretch();
        selected_layout->addWidget(view_source_button_);

        auto *main_layout = new QVBoxLayout(this);
        main_layout->addLayout(query_layout);
        main_layout->addWidget(results_view_);
        main_layout->addLayout(selected_layout);
        main_layout->addWidget(selected_result_text_);

        connect(query_edit_, &QLineEdit::textChanged, this, &SearchWidget::query_changed);

        connect(results_view_->selectionModel(), &QItemSelectionModel::selectionChanged, this, &SearchWidget::show_selected_result);

        connect(search_button_, &QPushButton::clicked, this, &SearchWidget::request_search);

        connect(view_source_button_, &QPushButton::clicked, this, &SearchWidget::request_source_document);
    }

    QString SearchWidget::query() const
    {
        return query_edit_->text();
    }

    int SearchWidget::result_count() const
    {
        return result_count_spin_->value();
    }

    bool SearchWidget::has_query() const
    {
        return !query().trimmed().isEmpty();
    }

    void SearchWidget::set_search_enabled(bool enabled)
    {
        search_button_->setEnabled(enabled && has_query());
    }

    void SearchWidget::set_results(std::vector<SearchResult> results)
    {
        result_model_->set_results(results);

        results_view_->clearSelection();
        results_view_->setCurrentIndex(QModelIndex());

        selected_result_text_->clear();
        view_source_button_->setEnabled(false);
    }

    void SearchWidget::clear_results()
    {
        result_model_->clear();

        results_view_->clearSelection();
        results_view_->setCurrentIndex(QModelIndex());

        selected_result_text_->clear();
        view_source_button_->setEnabled(false);
    }

    void SearchWidget::reset()
    {
        query_edit_->clear();
        result_count_spin_->setValue(3);

        clear_results();
        set_search_enabled(false);
    }

    void SearchWidget::request_search()
    {
        if (!has_query() || !search_button_->isEnabled())
        {
            return;
        }

        emit search_requested(query(), result_count());
    }

    void SearchWidget::show_selected_result()
    {
        const QModelIndex index = results_view_->currentIndex();

        if (!index.isValid())
        {
            selected_result_text_->clear();
            view_source_button_->setEnabled(false);
            return;
        }

        const SearchResult *result = result_model_->result_at(index.row());

        if (result == nullptr)
        {
            return;
        }

        QString info;
        info += QString("Document: %1\n")
                    .arg(QString::fromStdString(result->document_id));
        info += QString("Chunk: %1\n")
                    .arg(static_cast<qulonglong>(result->chunk_sequence));
        info += QString("Score: %1\n\n")
                    .arg(result->score, 0, 'g', 12);
        info += "Text:\n";
        info += QString::fromStdString(result->text);

        selected_result_text_->setPlainText(info);
        view_source_button_->setEnabled(true);

        const QString chunk_id =
            QString::fromStdString(result->document_id) +
            "#" +
            QString::number(static_cast<qulonglong>(result->chunk_sequence));

        emit selected_chunk_changed(chunk_id);
    }

    void SearchWidget::request_source_document()
    {
        const QModelIndex index = results_view_->currentIndex();

        if (!index.isValid())
        {
            return;
        }

        const SearchResult *result = result_model_->result_at(index.row());

        if (result == nullptr)
        {
            return;
        }

        emit source_document_requested(
            QString::fromStdString(result->document_id));
    }

} // namespace aiws::gui