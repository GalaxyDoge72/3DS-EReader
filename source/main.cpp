#include <3ds.h>
#include <dirent.h>
#include <vector>
#include <string>
#include <stdio.h>

using namespace std;

// Check if the /ebooks/ directory exists on the SD card
bool epubDirExists() {
    DIR* dir = opendir("sdmc:/ebooks");
    if (dir) {
        closedir(dir);
        return true;
    }
    return false;
}

// Search for ePub books in the /ebooks/ directory
vector<string> searchEPub(const string& epubPath) {
    vector<string> foundFiles;
    DIR* dir = opendir(epubPath.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            string name = entry->d_name;
            if (name.size() > 5 && name.substr(name.size() - 5) == ".epub") {
                foundFiles.push_back(name);
            }
        }
        closedir(dir);
    }
    return foundFiles;
}

void drawList(const vector<string>& files, int selectedIndex) {
    
}

int main(int argc, char** argv) {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    if (epubDirExists()) {
        vector<string> epubs = searchEPub("sdmc:/ebooks");
        if (!epubs.empty()) {
            printf("Found ePub files:\n");
            for (const auto& file : epubs) {
                printf(" - %s\n", file.c_str());
            }
        } else {
            printf("No ePub files found in /ebooks/.\n");
        }
    } else {
        printf("/ebooks directory not found.\n");
    }

    printf("\nPress START to exit.\n");
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
