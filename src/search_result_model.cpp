#include "aiws/gui/search_result_model.hpp"

namespace aiws::gui
{

    SearchResultModel::SearchResultModel(QObject *parent)
        : QAbstractTableModel(parent)
    {
    }

    int SearchResultModel::rowCount(const QModelIndex &parent) const
    {
        if (parent.isValid())
        {
            return 0;
        }

        return results_.size();
    }

    int SearchResultModel::columnCount(const QModelIndex &parent) const
    {

        if (parent.isValid())
        {
            return 0;
        }

        return 5;
    }

    QVariant SearchResultModel::data(const QModelIndex &index, int role) const
    {

        if (role != Qt::DisplayRole)
        {
            return {};
        }

        if (!index.isValid() || index.row() < 0 || index.row() >= results_.size() || index.column() < 0 || index.column() >= columnCount())
        {
            return {};
        }

        const SearchResult &result = results_[index.row()];
        int column = index.column();

        if (column == 0)
        {
            return index.row() + 1;
        }
        else if (column == 1)
        {
            return QString::fromStdString(result.document_id);
        }
        else if (column == 2)
        {
            return static_cast<qlonglong>(result.chunk_sequence);
        }
        else if (column == 3)
        {
            return result.score;
        }
        else if (column == 4)
        {
            return QString::fromStdString(result.text);
        }
        else
        {
            return {};
        }
    }

    QVariant SearchResultModel::headerData(int section,
                                           Qt::Orientation orientation,
                                           int role) const
    {

        if (role != Qt::DisplayRole)
        {
            return {};
        }

        if (orientation == Qt::Horizontal)
        {
            if (section == 0)
            {
                return QString("Rank");
            }
            else if (section == 1)
            {
                return QString("Document");
            }
            else if (section == 2)
            {
                return QString("Chunk");
            }
            else if (section == 3)
            {
                return QString("Score");
            }
            else if (section == 4)
            {
                return QString("Text");
            }
        }

        if (orientation == Qt::Vertical && section >= 0 && section < results_.size())
        {
            return section + 1;
        }

        return {};
    }

    void SearchResultModel::set_results(std::vector<SearchResult> results)
    {
        beginResetModel();
        results_ = std::move(results);
        endResetModel();
    }

    void SearchResultModel::clear()
    {
        beginResetModel();
        results_.clear();
        endResetModel();
    }

    const SearchResult *SearchResultModel::result_at(int row) const noexcept
    {
        if (row < 0 || row >= results_.size())
        {
            return nullptr;
        }

        return &results_[row];
    }

} // namespace aiws::gui
