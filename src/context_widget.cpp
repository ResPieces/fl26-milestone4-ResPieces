#include "aiws/gui/context_widget.hpp"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

namespace aiws::gui
{

    ContextWidget::ContextWidget(QWidget *parent) : QWidget(parent)
    {
        context_budget_slider_ = new QSlider(Qt::Horizontal, this);
        context_budget_slider_->setRange(50, 1000);
        context_budget_slider_->setValue(300);
        context_budget_slider_->setSingleStep(50);
        context_budget_slider_->setPageStep(50);

        budget_value_label_ = new QLabel("300", this);
        build_context_button_ = new QPushButton("Build Context", this);

        context_text_ = new QPlainTextEdit(this);
        context_text_->setPlaceholderText("Built context will appear here.");

        auto *Budget_Label = new QLabel("Token Budget:", this);

        auto *H_Layout_1 = new QHBoxLayout;
        H_Layout_1->addWidget(Budget_Label);
        H_Layout_1->addWidget(context_budget_slider_);
        H_Layout_1->addWidget(budget_value_label_);
        H_Layout_1->addWidget(build_context_button_);

        auto *Context_Label = new QLabel("Context", this);
        QFont ContextFont = Context_Label->font();
        ContextFont.setBold(true);
        Context_Label->setFont(ContextFont);

        auto *main_layout = new QVBoxLayout;
        main_layout->addWidget(Context_Label);
        main_layout->addLayout(H_Layout_1);
        main_layout->addWidget(context_text_);

        setLayout(main_layout);

        connect(build_context_button_, &QPushButton::clicked, this, &ContextWidget::request_context);

        connect(context_budget_slider_, &QSlider::valueChanged, this, &ContextWidget::budget_changed);
    }

    int ContextWidget::token_budget() const
    {
        return context_budget_slider_->value();
    }

    void ContextWidget::set_build_context_enabled(bool enabled)
    {
        build_context_button_->setEnabled(enabled);
    }

    void ContextWidget::set_context(const std::vector<ContextItem> &items)
    {
        QString output;

        for (const auto &item : items)
        {
            output += QString("Document: %1\n").arg(QString::fromStdString(item.document_id));
            output += QString("Chunk: %1\n").arg(QString::fromStdString(item.chunk_id));
            output += QString("Tokens: %1\n").arg(static_cast<qulonglong>(item.token_count));
            output += QString("Score: %1\n").arg(item.score, 0, 'g', 12);
            output += QString("Truncated: %1\n").arg(item.truncated ? "Yes" : "No");
            output += QString("Text:\n%1\n\n").arg(QString::fromStdString(item.text));
        }

        context_text_->setPlainText(output);
    }

    void ContextWidget::clear_context()
    {
        context_text_->clear();
    }

    void ContextWidget::reset()
    {
        clear_context();
        context_budget_slider_->setValue(300);
        budget_value_label_->setText(QString::number(300));

        set_build_context_enabled(false);
    }

    void ContextWidget::request_context()
    {
        if (!build_context_button_->isEnabled())
            return;

        emit build_context_requested(token_budget());
    }

    void ContextWidget::budget_changed(int value)
    {
        budget_value_label_->setText(QString::number(value));
    }

} // namespace aiws::gui
