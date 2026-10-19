#!/usr/bin/env python3
"""Writes the isolated media library the Media Library room reads in tests (Content/MediaLibrary).

    tools/gen_media_library.py        # needs ffmpeg with the flac encoder

Music/ holds three short tagged tones (two artists, two albums, two genres) and an .m3u playlist of
two of them; Pictures/ holds two albums of two pictures copied from the museum's own textures. The
room never reads a visitor's own folders on its own: tools/media_library_check.sh runs the museum
with HOME and XDG_CONFIG_HOME pointing at a copy of this tree (user-dirs.dirs names Music/Pictures
there), so CNA's MediaLibrary sees only these files. All audio is generated here; the pictures are
the repository's textures.
"""

import pathlib
import shutil
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / "Content" / "MediaLibrary"

# file, title, artist, album, genre, track, frequency (Hz), milliseconds
TRACKS = (
    ("01-sunrise.flac", "Sunrise", "Museum Quartet", "Opening Hours", "Ambient", 1, 440.0, 1200),
    ("02-closing-time.flac", "Closing Time", "Museum Quartet", "Opening Hours", "Ambient", 2, 523.25, 900),
    ("03-the-archive.flac", "The Archive", "Curator", "Basement Tapes", "Electronic", 1, 329.63, 700),
)
PICTURES = (
    ("Exhibits", "brick.png"),
    ("Exhibits", "checker.png"),
    ("Floors", "carpet.png"),
    ("Floors", "clouds.png"),
)


def main() -> int:
    if OUT.exists():
        shutil.rmtree(OUT)
    music = OUT / "Music"
    music.mkdir(parents=True)
    for name, title, artist, album, genre, track, frequency, ms in TRACKS:
        subprocess.run(
            ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi",
             "-i", f"sine=frequency={frequency}:sample_rate=22050:duration={ms / 1000.0}",
             "-ac", "1", "-c:a", "flac", "-fflags", "+bitexact", "-flags:a", "+bitexact",
             "-metadata", f"title={title}", "-metadata", f"artist={artist}", "-metadata", f"album={album}",
             "-metadata", f"genre={genre}", "-metadata", f"track={track}", str(music / name)],
            check=True,
        )
    (music / "Favourites.m3u").write_text("#EXTM3U\n01-sunrise.flac\n03-the-archive.flac\n", encoding="utf-8")
    for album, texture in PICTURES:
        folder = OUT / "Pictures" / album
        folder.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / "Content" / "Textures" / texture, folder / texture)
    config = OUT / ".config"
    config.mkdir()
    # Read by SDL's XDG lookup (SDL_GetUserFolder) once HOME points at this tree.
    (config / "user-dirs.dirs").write_text('XDG_MUSIC_DIR="$HOME/Music"\nXDG_PICTURES_DIR="$HOME/Pictures"\n', encoding="utf-8")
    total = sum(path.stat().st_size for path in OUT.rglob("*") if path.is_file())
    print(f"wrote {OUT.relative_to(ROOT)}: {len(TRACKS)} songs, 1 playlist, {len(PICTURES)} pictures, {total // 1024} KB")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
