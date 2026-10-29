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
#ifndef PLAYER_UI_QML_REGION_MODEL_HPP
#define PLAYER_UI_QML_REGION_MODEL_HPP

#include "media_model.hpp"

#include "smil/document.hpp"

#include <QAbstractListModel>

#include <vector>

namespace player
{
    /// The regions of the current layout, sorted by z-index (QML draws a later one above), each with its media.
    class RegionModel final : public QAbstractListModel
    {
            Q_OBJECT

        public:
            enum Role
            {
                LeftRole = Qt::UserRole + 1, // 0..1 of the window
                TopRole,
                WidthRole,
                HeightRole,
                NameRole,
                ColorRole,
                ImageRole, // URL of the background image, empty if none or not there yet
                RepeatRole,
                MediaRole // the MediaModel of the region
            };

            using QAbstractListModel::QAbstractListModel;

            int                    rowCount(const QModelIndex &parent = {}) const override;
            QVariant               data(const QModelIndex &index, int role) const override;
            QHash<int, QByteArray> roleNames() const override;

            /// A new layout: the regions anew, their media are gone already (StopMedia come first).
            void reset(const std::vector<garlic::core::smil::Region> &regions);
            void setBackgroundImage(int row, const QString &url);

            int                               count() const noexcept { return static_cast<int>(regions.size()); }
            const garlic::core::smil::Region &definition(int row) const { return regions[static_cast<std::size_t>(row)].definition; }
            MediaModel                       *media(int row) const { return regions[static_cast<std::size_t>(row)].media; }

        private:
            struct Entry
            {
                garlic::core::smil::Region definition;
                QString                    image;
                MediaModel                *media = nullptr; // child of this model
            };
            std::vector<Entry> regions;
    };
}

#endif // PLAYER_UI_QML_REGION_MODEL_HPP
