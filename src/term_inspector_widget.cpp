#include "aiws/gui/term_inspector_widget.hpp"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace aiws::gui
{

    TermInspectorWidget::TermInspectorWidget(QWidget *parent) : QWidget(parent)
    {
        // TODO: Construct and lay out the Term Inspector, connect local events,
        // and establish initial state.

        term_edit_ = new QLineEdit(this);
        term_edit_->setPlaceholderText("Enter a Term");

        inspect_button_ = new QPushButton("Inspect", this);
        document_frequency_value_ = new QLabel("-", this);
        chunk_frequency_value_ = new QLabel("-", this);
        selected_chunk_id_.clear();

        auto *Term_Label = new QLabel("Term:", this);

        auto *H_Layout_1 = new QHBoxLayout;
        H_Layout_1->addWidget(Term_Label);
        H_Layout_1->addWidget(term_edit_);
        H_Layout_1->addWidget(inspect_button_);

        auto *docFreqLabel = new QLabel("Document Frequency:", this);
        auto *selectedChunkTFLabel = new QLabel("Selected Chunk TF:", this);

        auto *H_Layout_2 = new QHBoxLayout;
        H_Layout_2->addWidget(docFreqLabel);
        H_Layout_2->addWidget(document_frequency_value_);
        H_Layout_2->addWidget(selectedChunkTFLabel);
        H_Layout_2->addWidget(chunk_frequency_value_);

        auto *Term_Inspector_Label = new QLabel("Term Inspector", this);
        QFont TermInspectorFont = Term_Inspector_Label->font();
        TermInspectorFont.setBold(true);
        Term_Inspector_Label->setFont(TermInspectorFont);

        auto *main_layout = new QVBoxLayout;
        main_layout->addWidget(Term_Inspector_Label);
        main_layout->addLayout(H_Layout_1);
        main_layout->addLayout(H_Layout_2);

        setLayout(main_layout);

        connect(inspect_button_, &QPushButton::clicked, this, &TermInspectorWidget::request_inspection);
    }

    void TermInspectorWidget::set_inspection_enabled(bool enabled)
    {
        inspect_button_->setEnabled(enabled);
    }

    void TermInspectorWidget::set_selected_chunk(const QString &chunk_id)
    {
        clear_statistics();
        selected_chunk_id_ = chunk_id;
    }

    void TermInspectorWidget::set_statistics(std::size_t document_frequency,
                                             bool has_chunk_frequency,
                                             std::size_t chunk_frequency)
    {
        document_frequency_value_->setText(
            QString::number(static_cast<qulonglong>(document_frequency)));

        if (has_chunk_frequency)
        {
            chunk_frequency_value_->setText(
                QString::number(static_cast<qulonglong>(chunk_frequency)));
        }
        else
        {
            chunk_frequency_value_->setText("-");
        }
    }

    void TermInspectorWidget::clear_statistics()
    {
        document_frequency_value_->setText("-");
        chunk_frequency_value_->setText("-");
    }

    void TermInspectorWidget::reset()
    {
        term_edit_->clear();
        selected_chunk_id_.clear();
        clear_statistics();
        set_inspection_enabled(false);
    }

    void TermInspectorWidget::request_inspection()
    {
        const QString term = term_edit_->text().trimmed();

        if (term.isEmpty() || !inspect_button_->isEnabled())
        {
            return;
        }

        emit term_inspection_requested(term, selected_chunk_id_);
    }

} // namespace aiws::gui
