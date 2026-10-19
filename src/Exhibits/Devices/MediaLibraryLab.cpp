// The media library (Microsoft.Xna.Framework.Media.MediaLibrary): the songs, albums, artists, genres and
// playlists of the user's Music folder and the picture albums of their Pictures folder, as CNA indexes them.
//
// MediaLibrary reads the visitor's own folders, and SavePicture writes into them, so nothing happens until a
// manual button is pressed (the smoke sweep never presses it). tools/devices_check.sh runs the museum with
// HOME and XDG_CONFIG_HOME pointing at a copy of Content/MediaLibrary, so the check sees only fixture files and
// compares the exact counts and names this room logs under [museum][medialib].
#include <algorithm>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/ScreenExhibit.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Media/MediaLibrary.hpp"
#include "System/IO/MemoryStream.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace Microsoft::Xna::Framework::Media;

namespace {

std::string Seconds(const System::TimeSpan& t)
{
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f s", t.getTotalSecondsProperty());
    return buf;
}

class MediaLibraryLab final : public ScreenExhibit {
public:
    MediaLibraryLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("MediaLibrary", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
        p.AddManualButton("Read my media library (Music and Pictures)", [this] { Read(); });
        p.AddManualButton("SavePicture: save this room's swatch", [this] { Save(); });
        p.AddButton("Dispose the library", [this] { DisposeLibrary(); });
        p.AddReadout("Songs / albums / artists / genres", [this] { return counts_; });
        p.AddReadout("Playlists", [this] { return playlists_; });
        p.AddReadout("Pictures / picture albums", [this] { return pictures_; });
        p.AddReadout("Last result", [this] { return last_; });
        AddInfoSign(b, "The media library",
                    {"MediaLibrary indexes the Music and Pictures folders: songs carry their album, artist, genre and track number from the file's tags;",
                     "playlists come from .m3u files; picture albums are folders. It reads your own folders, so it opens only when you press the button."});
    }

    void OnLeave() override { DisposeLibrary(); }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        const auto text = [&](const std::string& t, float x, float y, Color c, float s = 0.7f) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        text("The Media Library", 24, 12, Kit::kGold, 1.0f);
        if (library_ == nullptr) {
            text("Press \"Read my media library\" on the panel: the library reads your own Music and Pictures folders.", 30, 80, Kit::kText, 0.68f);
            text("Last result: " + last_, 30, 120, Kit::kDim, 0.68f);
        } else {
            float y = 70.0f;
            for (const std::string& line : lines_) {
                text(line, 30, y, Kit::kText, 0.6f);
                y += 32.0f;
                if (y > 470.0f) break;
            }
            if (thumbnail_ != nullptr) sb.Draw(*thumbnail_, Rectangle(760, 300, 160, 160), Color::White);
        }
        sb.End();
    }

private:
    void Read()
    {
        try {
            DisposeLibrary();
            library_ = std::make_unique<MediaLibrary>();
            SongCollection* songs = library_->getSongsProperty();
            AlbumCollection* albums = library_->getAlbumsProperty();
            ArtistCollection* artists = library_->getArtistsProperty();
            GenreCollection* genres = library_->getGenresProperty();
            counts_ = std::to_string(songs->getCountProperty()) + " / " + std::to_string(albums->getCountProperty()) + " / " + std::to_string(artists->getCountProperty()) + " / " +
                      std::to_string(genres->getCountProperty());
            lines_.clear();
            lines_.push_back("Source: " + library_->getMediaSourceProperty()->getNameProperty() + "    songs / albums / artists / genres: " + counts_);
            for (int i = 0; i < songs->getCountProperty(); ++i) {
                const Song* s = (*songs)[i];
                const std::string line = "#" + std::to_string(s->getTrackNumberProperty()) + " " + s->getNameProperty() + " - " + (s->getArtistProperty() ? s->getArtistProperty()->getNameProperty() : "?") +
                                         ", " + (s->getAlbumProperty() ? s->getAlbumProperty()->getNameProperty() : "?") + ", " + (s->getGenreProperty() ? s->getGenreProperty()->getNameProperty() : "?") + ", " +
                                         Seconds(s->getDurationProperty());
                lines_.push_back(line);
                Museum::Log("medialib", "song %s", line.c_str());
            }
            for (int i = 0; i < albums->getCountProperty(); ++i) {
                const Album* a = (*albums)[i];
                Museum::Log("medialib", "album %s by %s: %d song(s), %s, art %s", a->getNameProperty().c_str(), a->getArtistProperty() ? a->getArtistProperty()->getNameProperty().c_str() : "?",
                            static_cast<int>(a->getSongsProperty()->getCountProperty()), Seconds(a->getDurationProperty()).c_str(), a->getHasArtProperty() ? "yes" : "no");
            }
            for (int i = 0; i < artists->getCountProperty(); ++i) {
                const Artist* a = (*artists)[i];
                Museum::Log("medialib", "artist %s: %d album(s), %d song(s)", a->getNameProperty().c_str(), static_cast<int>(a->getAlbumsProperty()->getCountProperty()), static_cast<int>(a->getSongsProperty()->getCountProperty()));
            }
            for (int i = 0; i < genres->getCountProperty(); ++i) {
                const Genre* g = (*genres)[i];
                Museum::Log("medialib", "genre %s: %d song(s)", g->getNameProperty().c_str(), static_cast<int>(g->getSongsProperty()->getCountProperty()));
            }
            PlaylistCollection* lists = library_->getPlaylistsProperty();
            playlists_ = std::to_string(lists->getCountProperty());
            for (int i = 0; i < lists->getCountProperty(); ++i) {
                const Playlist* pl = (*lists)[i];
                std::string names;
                for (int k = 0; k < pl->getSongsProperty()->getCountProperty(); ++k) names += (k ? ", " : "") + (*pl->getSongsProperty())[k]->getNameProperty();
                const std::string line = "playlist " + pl->getNameProperty() + ": " + names + " (" + Seconds(pl->getDurationProperty()) + ")";
                playlists_ += (i ? "; " : ": ") + pl->getNameProperty();
                lines_.push_back(line);
                Museum::Log("medialib", "%s", line.c_str());
            }
            ReadPictures();
            last_ = "read";
            Museum::Log("medialib", "library: %s songs/albums/artists/genres, %s playlist(s), %s", counts_.c_str(), std::to_string(lists->getCountProperty()).c_str(), pictures_.c_str());
        } catch (const std::exception& e) {
            library_.reset();
            last_ = std::string("MediaLibrary refused: ") + e.what();
            Museum::Log("medialib", "%s", last_.c_str());
        }
    }

    void ReadPictures()
    {
        PictureCollection* all = library_->getPicturesProperty();
        PictureAlbum* root = library_->getRootPictureAlbumProperty();
        int albumCount = 0;
        if (root != nullptr) {
            PictureAlbumCollection* children = root->getAlbumsProperty();
            albumCount = children->getCountProperty();
            for (int i = 0; i < albumCount; ++i) {
                const PictureAlbum* album = (*children)[i];
                std::string names;
                for (int k = 0; k < album->getPicturesProperty()->getCountProperty(); ++k) {
                    const Picture* pic = (*album->getPicturesProperty())[k];
                    names += (k ? ", " : "") + pic->getNameProperty() + " " + std::to_string(pic->getWidthProperty()) + "x" + std::to_string(pic->getHeightProperty());
                }
                const std::string line = "picture album " + album->getNameProperty() + " (parent " + (album->getParentProperty() ? album->getParentProperty()->getNameProperty() : "none") + "): " + names;
                lines_.push_back(line);
                Museum::Log("medialib", "%s", line.c_str());
            }
        }
        pictures_ = std::to_string(all->getCountProperty()) + " / " + std::to_string(albumCount);
        if (all->getCountProperty() > 0) {
            // GetThumbnail hands over a stream the caller owns.
            std::unique_ptr<System::IO::Stream> stream((*all)[0]->GetThumbnail());
            if (stream != nullptr) thumbnail_ = std::make_unique<Texture2D>(Texture2D::FromStream(*S().device, *stream));
        }
    }

    void Save()
    {
        if (library_ == nullptr) Read();
        if (library_ == nullptr) return;
        try {
            // A 2 x 2 BMP written here, so the saved bytes are known exactly.
            static const std::vector<uint8_t> bmp = {
                'B', 'M', 70, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0, 40, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 1, 0, 24, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 255, 0, 255, 0, 0, 0, 255, 0, 0, 255, 255, 255, 0, 0};
            Picture* saved = library_->SavePicture("museum-swatch", bmp);
            const int savedCount = library_->getSavedPicturesProperty()->getCountProperty();
            last_ = std::string("SavePicture: ") + (saved != nullptr ? saved->getNameProperty() + " " + std::to_string(saved->getWidthProperty()) + "x" + std::to_string(saved->getHeightProperty()) : "null") +
                    ", SavedPictures " + std::to_string(savedCount);
        } catch (const std::exception& e) {
            last_ = std::string("SavePicture refused: ") + e.what();
        }
        Museum::Log("medialib", "%s", last_.c_str());
    }

    void DisposeLibrary()
    {
        thumbnail_.reset();
        if (library_ == nullptr) return;
        library_->Dispose();
        Museum::Log("medialib", "disposed: IsDisposed %s", library_->getIsDisposedProperty() ? "true" : "false");
        library_.reset();
    }

    std::unique_ptr<MediaLibrary> library_;
    std::unique_ptr<Texture2D> thumbnail_;
    std::vector<std::string> lines_;
    std::string counts_ = "-", playlists_ = "-", pictures_ = "-", last_ = "-";
};

}  // namespace

std::unique_ptr<Exhibit> MakeMediaLibraryLab(const RoomDef& room, Services& services)
{
    return std::make_unique<MediaLibraryLab>(room, services);
}
