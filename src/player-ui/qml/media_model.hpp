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
#ifndef PLAYER_UI_QML_MEDIA_MODEL_HPP
#define PLAYER_UI_QML_MEDIA_MODEL_HPP

#include <QAbstractListModel>
#include <QString>

#include <vector>

namespace player
{
    /// One medium shown in one region; plain values, so QML holds no pointer into the core.
    struct ShownMedia
    {
        int     instance = 0;
        QString kind;   // image, video, audio, web, brush
        QString source; // URL for QML: file:///…, http://…
        QString fit;    // of the medium, else of the region; lower case
        QString align;  // mediaAlign, lower case
        double  volume  = 1;
        QString color;  // brush
        QString label;  // title or src, for the debug dialog
        bool    primary = true; // only the first region of an instance reports end and errors
        bool    paused  = false;
        int     generation = 0; // RepeatMedia counts it up: QML plays again from the beginning
    };

    /**
     * The media of one region, the newest last (QML draws it on top). Only the topmost visible medium is shown, like
     * the stack of the widget player; audio has no picture and does not cover anything.
     */
    class MediaModel final : public QAbstractListModel
    {
            Q_OBJECT

        public:
            enum Role
            {
                InstanceRole = Qt::UserRole + 1,
                KindRole,
                SourceRole,
                FitRole,
                AlignRole,
                VolumeRole,
                ColorRole,
                PrimaryRole,
                PausedRole,
                GenerationRole,
                OnTopRole
            };

            using QAbstractListModel::QAbstractListModel;

            int                    rowCount(const QModelIndex &parent = {}) const override;
            QVariant               data(const QModelIndex &index, int role) const override;
            QHash<int, QByteArray> roleNames() const override;

            void add(ShownMedia media);
            void remove(int instance);
            void setPaused(int instance, bool paused);
            void raise(int instance); // a resumed medium comes to the top again
            void repeat(int instance);
            void clear();

            const std::vector<ShownMedia> &media() const noexcept { return shown; }

        private:
            std::vector<ShownMedia> shown;

            int  find(int instance) const;
            void updateOnTop();
    };
}

#endif // PLAYER_UI_QML_MEDIA_MODEL_HPP
