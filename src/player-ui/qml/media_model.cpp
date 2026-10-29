/*
 garlic-player: SMIL Player for Digital Signage
 Copyright (C) 2026 Nikolaos Sagiadinos <garlic@saghiadinos.de>
 This file is part of the GarlicSignage source code

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License, version 3,
 as published by the Free Software Foundation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "media_model.hpp"

namespace player
{
    int MediaModel::rowCount(const QModelIndex &parent) const
    {
        return parent.isValid() ? 0 : static_cast<int>(shown.size());
    }

    QVariant MediaModel::data(const QModelIndex &index, int role) const
    {
        if (!index.isValid() || index.row() >= rowCount())
            return {};
        const ShownMedia &media = shown[static_cast<std::size_t>(index.row())];
        switch (role)
        {
            case InstanceRole: return media.instance;
            case KindRole: return media.kind;
            case SourceRole: return media.source;
            case FitRole: return media.fit;
            case AlignRole: return media.align;
            case VolumeRole: return media.volume;
            case ColorRole: return media.color;
            case PrimaryRole: return media.primary;
            case PausedRole: return media.paused;
            case GenerationRole: return media.generation;
            case OnTopRole:
            {
                // the last one with a picture; audio lies above without covering anything
                for (auto it = shown.rbegin(); it != shown.rend(); ++it)
                    if (it->kind != "audio")
                        return it->instance == media.instance;
                return false;
            }
            default: return {};
        }
    }

    QHash<int, QByteArray> MediaModel::roleNames() const
    {
        return {{InstanceRole, "instance"}, {KindRole, "kind"},       {SourceRole, "source"},
                {FitRole, "fit"},           {AlignRole, "align"},     {VolumeRole, "volume"},
                {ColorRole, "color"},       {PrimaryRole, "primary"}, {PausedRole, "paused"},
                {GenerationRole, "generation"}, {OnTopRole, "onTop"}};
    }

    void MediaModel::add(ShownMedia media)
    {
        int row = rowCount();
        beginInsertRows({}, row, row);
        shown.push_back(std::move(media));
        endInsertRows();
        updateOnTop();
    }

    void MediaModel::remove(int instance)
    {
        int row = find(instance);
        if (row < 0)
            return;
        beginRemoveRows({}, row, row);
        shown.erase(shown.begin() + row);
        endRemoveRows();
        updateOnTop();
    }

    void MediaModel::setPaused(int instance, bool paused)
    {
        int row = find(instance);
        if (row < 0)
            return;
        shown[static_cast<std::size_t>(row)].paused = paused;
        Q_EMIT dataChanged(index(row), index(row), {PausedRole});
    }

    void MediaModel::raise(int instance)
    {
        int row  = find(instance);
        int last = rowCount() - 1;
        if (row < 0 || row == last)
            return;
        beginMoveRows({}, row, row, {}, last + 1);
        ShownMedia media = std::move(shown[static_cast<std::size_t>(row)]);
        shown.erase(shown.begin() + row);
        shown.push_back(std::move(media));
        endMoveRows();
        updateOnTop();
    }

    void MediaModel::repeat(int instance)
    {
        int row = find(instance);
        if (row < 0)
            return;
        ++shown[static_cast<std::size_t>(row)].generation;
        Q_EMIT dataChanged(index(row), index(row), {GenerationRole});
    }

    void MediaModel::clear()
    {
        beginResetModel();
        shown.clear();
        endResetModel();
    }

    int MediaModel::find(int instance) const
    {
        for (std::size_t i = 0; i < shown.size(); ++i)
            if (shown[i].instance == instance)
                return static_cast<int>(i);
        return -1;
    }

    void MediaModel::updateOnTop()
    {
        if (!shown.empty())
            Q_EMIT dataChanged(index(0), index(rowCount() - 1), {OnTopRole});
    }
}
