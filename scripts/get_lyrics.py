#!/usr/bin/env python3
import sys
import requests
from bs4 import BeautifulSoup

def get_lyrics(url):
    headers = {
        "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                      "AppleWebKit/537.36 (KHTML, like Gecko) "
                      "Chrome/115.0.0.0 Safari/537.36"
    }

    response = requests.get(url, headers=headers)
    html = response.text

    soup = BeautifulSoup(html, "html.parser")
    containers = soup.find_all("div", attrs={"data-lyrics-container": "true"})

    if not containers:
        return "Lyrics not found."

    lines = []
    start = False

    for c in containers:
        for text in c.stripped_strings:
            stripped = text.strip()
            if not stripped:
                continue

            # Delete [Verse 1], [Chorus] and similar
            if stripped.startswith("[") and stripped.endswith("]"):
                start = True  # skip first block
                continue

            if start:
                lines.append(stripped)

    return "\n".join(lines)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: get_lyrics.py <url>")
        sys.exit(1)

    url = sys.argv[1]
    lyrics = get_lyrics(url)
    print(lyrics)
