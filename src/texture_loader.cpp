#define STB_IMAGE_IMPLEMENTATION

#include "texture_loader.h"
#include "stb_image.h"

#include <GL/gl.h>
#include <iostream>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/id3v2framefactory.h>
#include <vector>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

bool extractCoverFromMP3(const std::string& mp3Path, std::string& outImagePath) {
    TagLib::MPEG::File file(mp3Path.c_str());
    if (!file.isValid()) return false;

    TagLib::ID3v2::Tag* tag = file.ID3v2Tag();
    if (!tag) return false;

    auto frames = tag->frameListMap()["APIC"];
    if (frames.isEmpty()) return false;

    TagLib::ID3v2::AttachedPictureFrame* picFrame = static_cast<TagLib::ID3v2::AttachedPictureFrame*>(frames.front());

    const auto& data = picFrame->picture();
    fs::path rootPath(PROJECT_ROOT_DIR);
    fs::path tempDir = rootPath / "temp";
    fs::path tempFile = tempDir / "cover_from_tag.jpg";

    // Убеждаемся что директория существует
    if (!fs::exists(tempDir)) {
        fs::create_directories(tempDir);
    }

    std::string outPath = tempFile.string();
    std::ofstream img(outPath, std::ios::binary);
    img.write(data.data(), data.size());
    img.close();

    outImagePath = outPath;
    return true;
}

GLuint loadTexture(const char* filename) {
    int width, height, channels;

    // Loading image
    unsigned char* data = stbi_load(filename, &width, &height, &channels, STBI_rgb_alpha);

    // Creating an OpenGL Texture
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Setting filtering parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Loading an image into a texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    // Clear
    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);

    return textureID;
}

