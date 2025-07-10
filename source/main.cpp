#include <3ds.h>
#include <dirent.h>
#include <vector>
#include <string>
#include <stdio.h>
#include <archive.h>
#include <archive_entry.h>
#include <cstring>
#include <tinyxml2.h>

using namespace std;
using namespace tinyxml2;

// Check if the /ebooks/ directory exists on the SD card
bool DirExists() {
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
    consoleClear();
    printf("Select an ePub:\n\n");
    for (size_t i = 0; i < files.size(); i++) {
        if ((int)i == selectedIndex) {
            printf(" > %s\n", files[i].c_str());
        }
        else {
            printf("   %s\n", files[i].c_str());
        }
    }
}

// Recursively extract readable text from XML nodes
void extractText(XMLNode* node, string& output) {
    if (!node) return;

    XMLElement* elem = node->ToElement();
    if (elem) {
        string tag = elem->Name();
        if (tag == "script" || tag == "style") return; // Skip non-text content
    }

    if (node->ToText()) {
        output += node->ToText()->Value();
        output += " ";
    }

    for (XMLNode* child = node->FirstChild(); child; child = child->NextSibling()) {
        extractText(child, output);
    }
}

// Read and display content from .xhtml/.html files in EPUB
void readEPUBContent(const char* epubPath) {
    struct archive* a = archive_read_new();
    archive_read_support_format_zip(a);
    archive_read_support_filter_all(a);

    if (archive_read_open_filename(a, epubPath, 10240) != ARCHIVE_OK) {
        printf("Failed to open EPUB: %s\n", archive_error_string(a));
        archive_read_free(a);
        return;
    }

    struct archive_entry* entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char* name = archive_entry_pathname(entry);

        if (strstr(name, ".xhtml") || strstr(name, ".html")) {
            char buffer[8192];
            string fileContent;
            ssize_t size;
            while ((size = archive_read_data(a, buffer, sizeof(buffer))) > 0) {
                fileContent.append(buffer, size);
            }

            XMLDocument doc;
            if (doc.Parse(fileContent.c_str()) == XML_SUCCESS) {
                XMLElement* body = doc.FirstChildElement("html");
                if (body) body = body->FirstChildElement("body");

                string extractedText;
                extractText(body, extractedText);
                consoleClear();
                printf("=== %s ===\n\n", name);
                printf("%s\n", extractedText.c_str());
            } else {
                printf("Failed to parse: %s\n", name);
            }

            printf("\nPress B to return.\n");
            break;  // Only show one file for now
        } else {
            archive_read_data_skip(a);
        }
    }

    archive_read_free(a);
}

int main(int argc, char** argv) {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    if (!DirExists()) {
        consoleClear();
        printf("The path 'sdmc:/ebooks' was not found.\nEnsure that you have created a folder named 'ebooks' in the ROOT OF YOUR SD CARD.");
        printf("\nPress any key to exit.\n");

        while (aptMainLoop()) {
            hidScanInput();
            if (hidKeysDown()) break; // End the program once the user presses buttons
            gspWaitForVBlank();
        }

        gfxExit();
        return 0;
    }

    vector<string> ePubList = searchEPub("sdmc:/ebooks");
    if (ePubList.empty()) {
        consoleClear();
        printf("No .epub files were found.\n");
        printf("\nPress Start to exit.\n");
        while (aptMainLoop()) {
            hidScanInput();
            if (hidKeysDown() & KEY_START) break;
            gspWaitForVBlank();
        }
        gfxExit();
        return 0;
    }

    int selected = 0;
    drawList(ePubList, selected);
    printf("\nUse D-Pad UP/DOWN. A to select. Start to exit.\n");

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_START) break;

        if (kDown & (KEY_DOWN | KEY_CPAD_DOWN)) {
            selected = (selected + 1) % ePubList.size(); // Wrap around
            drawList(ePubList, selected);
            printf("\nUse D-Pad UP/DOWN. A to select. Start to exit.\n");
        }
        if (kDown & (KEY_UP | KEY_CPAD_UP)) {
            selected = (selected - 1 + ePubList.size()) % ePubList.size(); // Safe wrap-around
            drawList(ePubList, selected);
            printf("\nUse D-Pad UP/DOWN. A to select. Start to exit.\n");
        }
        if (kDown & KEY_A) {
            string path = "sdmc:/ebooks/" + ePubList[selected];
            consoleClear();
            readEPUBContent(path.c_str());
            while (aptMainLoop()) {
                hidScanInput();
                if (hidKeysDown() & KEY_B) {
                    drawList(ePubList, selected);
                    printf("\nUse D-Pad UP/DOWN. A to select. Start to exit.\n");
                    break;
                }
                gspWaitForVBlank();
            }
        }
    }

    gfxExit();
    return 0;
}
