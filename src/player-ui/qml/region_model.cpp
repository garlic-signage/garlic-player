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
#include "region_model.hpp"

namespace player
{
    int RegionModel::rowCount(const QModelIndex &parent) const
    {
        return parent.isValid() ? 0 : count();
    }

    QVariant RegionModel::data(const QModelIndex &index, int role) const
    {
        if (!index.isValid() || index.row() >= count())
            return {};
        const Entry &entry = regions[static_cast<std::size_t>(index.row())];
        switch (role)
        {
            case LeftRole: return entry.definition.left;
            case TopRole: return entry.definition.top;
            case WidthRole: return entry.definition.width;
            case HeightRole: return entry.definition.height;
            case NameRole:
                return QString::fromStdString(entry.definition.region_name.empty() ? entry.definition.id
                                                                                   : entry.definition.region_name);
            case ColorRole: return QString::fromStdString(entry.definition.background_color);
            case ImageRole: return entry.image;
            case RepeatRole: return QString::fromStdString(entry.definition.background_repeat).toLower();
            case MediaRole: return QVariant::fromValue(static_cast<QObject *>(entry.media));
            default: return {};
        }
    }

    QHash<int, QByteArray> RegionModel::roleNames() const
    {
        return {{LeftRole, "left"},   {TopRole, "top"},     {WidthRole, "width"},   {HeightRole, "height"},
                {NameRole, "name"},   {ColorRole, "color"}, {ImageRole, "image"},   {RepeatRole, "repeat"},
                {MediaRole, "media"}};
    }

    void RegionModel::reset(const std::vector<garlic::core::smil::Region> &next)
    {
        beginResetModel();
        for (Entry &entry : regions)
            entry.media->deleteLater(); // QML may still hold it until the delegates are gone
        regions.clear();
        for (const garlic::core::smil::Region &definition : next)
            regions.push_back({definition, {}, new MediaModel(this)});
        endResetModel();
    }

    void RegionModel::setBackgroundImage(int row, const QString &url)
    {
        regions[static_cast<std::size_t>(row)].image = url;
        Q_EMIT dataChanged(index(row), index(row), {ImageRole});
    }
}
