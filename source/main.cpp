#include <3ds.h>
#include <dirent.h>
#include <vector>
#include <string>
#include <stdio.h>
#include <archive.h>
#include <archive_entry.h>
#include <cstring>
#include <tinyxml2.h>
#include <cctype>
#include <algorithm>
#include <sstream>
#include <map>

using namespace std;
using namespace tinyxml2;


int maxFileSize = 96 * 1048576; // Allocate 96 MB as maximum file size //

// Make pageSize a global, non-constant variable so it can be modified.
size_t pageSize = 400; // Initial characters per page.

// Define min/max for pageSize
const size_t minPageSize = 200; // Minimum characters per page
const size_t maxPageSize = 2000; // Maximum characters per page (adjust as needed based on screen fit)
const size_t pageSizeStep = 50; // How much to increment/decrement by

enum Colour {
    RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE, DEFAULT, INVALID
};

// Return input string as lowercase
string toLower(const string& input) {
    string output = input;
    std::transform(output.begin(), output.end(), output.begin(), [](unsigned char c){ return std::tolower(c);});
    return output;
}

Colour getColourFromString(const string& colourName) {
    string c = toLower(colourName);
    if (c == "red") return RED;
    if (c == "green") return GREEN;
    if (c == "yellow") return YELLOW;
    if (c == "blue") return BLUE;
    if (c == "magenta") return MAGENTA;
    if (c == "cyan") return CYAN;
    if (c == "white") return WHITE;
    if (c == "default") return DEFAULT;
    return INVALID;
}

string printColouredText(const string& input, const string& colour) {
    Colour selectedColour = getColourFromString(colour);
    string colourCode;

    switch (selectedColour) {
        case RED: colourCode = "\x1b[31m"; break;
        case GREEN: colourCode = "\x1b[32m"; break;
        case YELLOW: colourCode = "\x1b[33m"; break;
        case BLUE: colourCode = "\x1b[34m"; break;
        case MAGENTA: colourCode = "\x1b[35m"; break;
        case CYAN: colourCode = "\x1b[36m"; break;
        case WHITE: colourCode = "\x1b[37m"; break;
        case DEFAULT:
        case INVALID: colourCode = "\x1b[0m"; break;
    }
    return colourCode + input + "\x1b[0m";
}

// Check if the /ebooks/ directory exists on the SD card
bool DirExists() {
    DIR* dir = opendir("sdmc:/ebooks");
    if (dir) {
        closedir(dir);
        return true;
    }
    return false;
}

// Helper function for string replacement
void replace_all(std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    size_t start_pos = 0;
    while((start_pos = s.find(from, start_pos)) != std::string::npos) {
        s.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Advance past the replaced text
    }
}

// Map for common HTML entities and direct problematic UTF-8 character replacements
std::map<std::string, char> htmlEntities = {
    {"&apos;", '\''}, {"&#39;", '\''},
    {"&quot;", '\"'}, {"&#34;", '\"'},
    {"&amp;", '&'},
    {"&lt;", '<'},
    {"&gt;", '>'},
    
    // Smart quotes (often directly in UTF-8 or as entities)
    {"&lsquo;", '\''}, {"&#x2018;", '\''}, // Left single quote
    {"&rsquo;", '\''}, {"&#x2019;", '\''}, // Right single quote (apostrophe)
    {"&ldquo;", '\"'}, {"&#x201C;", '\"'}, // Left double quote
    {"&rdquo;", '\"'}, {"&#x201D;", '\"'}, // Right double quote

    // Dashes
    {"&mdash;", '-'}, {"&#x2014;", '-'}, // Em dash
    {"&ndash;", '-'}, {"&#x2013;", '-'}, // En dash

    // Ellipsis
    {"&hellip;", '.'}, {"&#x2026;", '.'}, // Horizontal ellipsis

    {"&nbsp;", ' '},  // Non-breaking space
    {"&#xa0;", ' '},
};

// Function to replace HTML entities and common problematic UTF-8 sequences with ASCII characters
std::string decodeHtmlEntities(const std::string& input) {
    std::string output = input;

    // First, handle named and numeric HTML entities
    for (const auto& pair : htmlEntities) {
        size_t pos = output.find(pair.first);
        while (pos != std::string::npos) {
            output.replace(pos, pair.first.length(), 1, pair.second);
            pos = output.find(pair.first, pos + 1);
        }
    }

    // Second, iterate through the string to replace known problematic multi-byte UTF-8 sequences
    // that might not be HTML entities but are directly encoded.
    // These specific byte sequences are common for typographic characters in UTF-8.
    replace_all(output, "\xE2\x80\x99", "'"); // U+2019 RIGHT SINGLE QUOTATION MARK (smart apostrophe)
    replace_all(output, "\xE2\x80\x98", "'"); // U+2018 LEFT SINGLE QUOTATION MARK
    replace_all(output, "\xE2\x80\x9C", "\""); // U+201C LEFT DOUBLE QUOTATION MARK
    replace_all(output, "\xE2\x80\x9D", "\""); // U+201D RIGHT DOUBLE QUOTATION MARK
    replace_all(output, "\xE2\x80\xA6", "..."); // U+2026 HORIZONTAL ELLIPSIS
    replace_all(output, "\xE2\x80\x93", "-"); // U+2013 EN DASH
    replace_all(output, "\xE2\x80\x94", "-"); // U+2014 EM DASH
    // Add other problematic UTF-8 sequences here if you identify more.

    // Final cleanup: Replace any remaining non-ASCII printable characters with a space.
    // This is a robust fallback for characters that are not explicitly handled above
    // and might cause display issues on the 3DS console.
    std::string cleanedOutput;
    for (char c : output) {
        // Keep standard ASCII printable characters (32-126) and common whitespace
        if ((static_cast<unsigned char>(c) >= 32 && static_cast<unsigned char>(c) <= 126) ||
            c == '\n' || c == '\r' || c == '\t') {
            cleanedOutput += c;
        } else {
            cleanedOutput += ' '; // Replace problematic characters with a space
        }
    }
    output = cleanedOutput;

    return output;
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
    // Add explicit buffer management after clearing the console
    gfxFlushBuffers();
    gfxSwapBuffers();

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

vector<string> paginateFile(const string& fullText, size_t pageSize) {
    vector<string> pages;
    size_t pos = 0;

    while (pos < fullText.size()) {
        size_t end = min(pageSize, fullText.size() - pos);
        pages.push_back(fullText.substr(pos, end));
        pos += end;
    }
    return pages;
}

// Recursively extract readable text from XML nodes
void extractText(XMLNode* node, string& output) {
    if (!node) return;

    XMLElement* elem = node->ToElement();
    string tag = "";

    if (elem) {
        tag = toLower(elem->Name());

        // Skip unwanted tags
        if (tag == "script" || tag == "style") return;

        // Add a newline before certain block tags, ensuring no double newlines
        if (tag == "p" || tag == "div" || tag == "blockquote" ||
            tag == "h1" || tag == "h2" || tag == "h3" || tag == "h4" || tag == "h5" || tag == "h6") {
            if (!output.empty() && output.back() != '\n') {
                output += "\n";
            }
        }
        if (tag == "br") { // <br> always means a newline
            output += "\n";
        }

        // Discard italic/bold formatting: no special characters added for 'em' or 'i' tags.
        // The text content within these tags will still be extracted.
    }

    if (node->ToText()) {
        string textValue = node->ToText()->Value();
        // Trim leading/trailing whitespace from text nodes to avoid excessive spaces
        size_t first = textValue.find_first_not_of(" \t\n\r");
        size_t last = textValue.find_last_not_of(" \t\n\r");
        if (string::npos == first) { // Empty or all whitespace
            textValue = "";
        } else {
            textValue = textValue.substr(first, (last - first + 1));
        }

        if (!textValue.empty()) {
            // Add a space before text if the previous char wasn't a space or newline
            if (!output.empty() && output.back() != ' ' && output.back() != '\n') {
                output += " ";
            }
            output += textValue;
        }
    }

    for (XMLNode* child = node->FirstChild(); child; child = child->NextSibling()) {
        extractText(child, output);
    }

    // Add newline after block tags to separate paragraphs, ensuring no double newlines
    if (elem) {
        if (tag == "p" || tag == "div" || tag == "blockquote" ||
            tag == "h1" || tag == "h2" || tag == "h3" || tag == "h4" || tag == "h5" || tag == "h6") {
            if (!output.empty() && output.back() != '\n') {
                 output += "\n";
            }
        }
        // No closing characters for italic/bold.
    }
}



void displayPage(const vector<string>& pages, int currentPage) {
    consoleClear();
    // Add explicit buffer management after clearing the console
    gfxFlushBuffers();
    gfxSwapBuffers();

    if (currentPage >= 0 && currentPage < (int)pages.size()) {
        printf("%s\n\n", pages[currentPage].c_str());
        printf("Page %d of %d\n", currentPage + 1, (int)pages.size());
        printf("L: Prev | R: Next | B: Back\n");
    }
    else {
        printf("%s", printColouredText("Invalid Page Number!", "red").c_str());
    }
}

bool willExceedRAM(struct archive_entry* entry) {
    // Just going to assume that we always pass a valid file //
    int64_t epubSize = archive_entry_size(entry);
    if (epubSize > maxFileSize) {
        return true;
    }
    else {
        return false;
    }

}

vector<string> splitLines(const string& text) {
    vector<string> lines;
    std::stringstream ss(text);
    string line;
    while (std::getline(ss, line)) {
        lines.push_back(line);
    }
    return lines;
}

string wordWrap(const string& input, size_t maxWidth) {
    string wrappedText;
    size_t currentPos = 0;
    size_t len = input.length();

    while (currentPos < len) {
        // Find the end of the next segment, respecting existing newlines
        size_t nextNewline = input.find('\n', currentPos);
        size_t endOfSegment = (nextNewline == string::npos) ? len : nextNewline;
        
        // Process the segment before the next newline
        string segment = input.substr(currentPos, endOfSegment - currentPos);
        size_t segmentPos = 0;
        size_t segmentLen = segment.length();

        while (segmentPos < segmentLen) {
            size_t endPos = min(segmentPos + maxWidth, segmentLen);
            
            if (endPos < segmentLen) {
                size_t wrapPos = segment.rfind(' ', endPos);
                if (wrapPos != string::npos && wrapPos > segmentPos) {
                    wrappedText += segment.substr(segmentPos, wrapPos - segmentPos);
                    wrappedText += "\n";
                    segmentPos = wrapPos + 1; // Move past the space
                } else {
                    // No suitable space found, wrap at maxWidth
                    wrappedText += segment.substr(segmentPos, maxWidth);
                    wrappedText += "\n";
                    segmentPos += maxWidth;
                }
            } else {
                wrappedText += segment.substr(segmentPos);
                segmentPos = segmentLen;
            }
        }
        
        // If there was a newline, add it to the output and move past it
        if (nextNewline != string::npos) {
            wrappedText += "\n";
            currentPos = nextNewline + 1;
        } else {
            currentPos = len;
        }
    }

    return wrappedText;
}

vector<string> getChapterList(const char* epubPath) {
    vector<string> chapters;
    struct archive* a = archive_read_new();
    archive_read_support_format_zip(a);

    if (archive_read_open_filename(a, epubPath, 10240) != ARCHIVE_OK) {
        archive_read_free(a);
        return chapters; // Return empty vector on failure
    }

    struct archive_entry* entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char* name = archive_entry_pathname(entry);
        if (strstr(name, ".xhtml") || strstr(name, ".html")) {
            chapters.push_back(name);
        }
        archive_read_data_skip(a);
    }

    archive_read_free(a);
    return chapters;
}

void readAndDisplayChapter(const char* epubPath, const char* chapterPath) {
    struct archive* a = archive_read_new();
    archive_read_support_format_zip(a);

    if (archive_read_open_filename(a, epubPath, 10240) != ARCHIVE_OK) {
        printf("Failed to open EPUB: %s\n", archive_error_string(a));
        archive_read_free(a);
        return;
    }

    struct archive_entry* entry;
    string fullText;
    bool chapterFound = false;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        // Find the specific chapter file we want to read
        if (strcmp(archive_entry_pathname(entry), chapterPath) == 0) {
        chapterFound = true;
        char buffer[1024];
        string fileContent;
        ssize_t size;

        while ((size = archive_read_data(a, buffer, sizeof(buffer))) > 0) {
            fileContent.append(buffer, size);
        }

        // Apply HTML entity decoding and problematic UTF-8 character conversion here
        fileContent = decodeHtmlEntities(fileContent);

        XMLDocument doc;
        if (doc.Parse(fileContent.c_str()) == XML_SUCCESS) {
            XMLElement* body = doc.FirstChildElement("html");
            if (body) body = body->FirstChildElement("body");
            if (body) extractText(body, fullText);
        }
        break;
    } else {
        archive_read_data_skip(a);
    }
    }
    archive_read_free(a);

    if (!chapterFound) {
        printf("Could not find chapter: %s\n", chapterPath);
        return;
    }

    fullText = wordWrap(fullText, 50); // Word wrap to 50 characters per line
    vector<string> pages = paginateFile(fullText, pageSize); // Use the global pageSize
    int currentPage = 0;
    
    // This display loop remains the same
    displayPage(pages, currentPage);
    while (aptMainLoop()) {
        hidScanInput();
        u32 kdown = hidKeysDown();

        if (kdown & KEY_B) break;
        if ((kdown & KEY_L) && currentPage > 0) {
            currentPage--;
            displayPage(pages, currentPage);
        }
        if ((kdown & KEY_R) && currentPage < (int)pages.size() - 1) {
            currentPage++;
            displayPage(pages, currentPage);
        }
        gspWaitForVBlank();
    }
}

void displayChapterMenu(const char* epubPath) {
    vector<string> chapterList = getChapterList(epubPath);

    if (chapterList.empty()) {
        consoleClear();
        // Add explicit buffer management after clearing the console
        gfxFlushBuffers();
        gfxSwapBuffers();

        printf("No chapters (.xhtml files) found in this EPUB.\n");
        printf("\nPress B to return.\n");
        while (aptMainLoop()) {
            hidScanInput();
            if (hidKeysDown() & KEY_B) break;
            gspWaitForVBlank();
        }
        return;
    }

    int selected = 0;

    while (aptMainLoop()) {
        drawList(chapterList, selected); // drawList calls consoleClear, flush, swap
        printf("\nSelect a chapter:\n");
        printf("Use D-Pad UP/DOWN. A to select. B to go back.\n");

        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_B) break; // Go back to main book list

        if (kDown & (KEY_DOWN | KEY_CPAD_DOWN)) {
            selected = (selected + 1) % chapterList.size();
        }
        if (kDown & (KEY_UP | KEY_CPAD_UP)) {
            selected = (selected - 1 + chapterList.size()) % chapterList.size();
        }
        if (kDown & KEY_A) {
            string chapterPath = chapterList[selected];
            consoleClear(); // Clear before displaying chapter
            // Add explicit buffer management after clearing the console
            gfxFlushBuffers();
            gfxSwapBuffers();

            // Call the function to read and display only the selected chapter
            readAndDisplayChapter(epubPath, chapterPath.c_str()); 

        }

        gspWaitForVBlank();
        // consoleClear(); // This was here before, but drawList already clears at start of loop
    }
}

// Function to display the settings menu
void displaySettingsMenu() {
    consoleClear();
    int selectedSetting = 0;
    const int numSettings = 2; // Page Size, Text Color
    string settingsOptions[numSettings] = {"Page Size", "Text Color"};

    while (aptMainLoop()) {
        consoleClear();
        // Add explicit buffer management after clearing the console
        // This might help ensure the clear is fully rendered before new text is drawn
        gfxFlushBuffers();
        gfxSwapBuffers();

        printf("--- Settings ---\n\n");

        // Display current values for settings
        if (selectedSetting == 0) {
            printf(" > Page Size: %zu (D-Pad L/R: %zu, L/R buttons: %d)\n", pageSize, pageSizeStep, 100);
        } else {
            printf("   Page Size: %zu\n", pageSize);
        }
        if (selectedSetting == 1) {
            printf(" > Text Color: DEFAULT (Not implemented yet)\n");
        } else {
            printf("   Text Color: DEFAULT\n");
        }
        
        printf("\n\nUse D-Pad UP/DOWN. A to select.\n");
        printf("D-Pad L/R for fine adjustment, L/R buttons for large adjustment.\n"); // Updated instructions
        printf("B to go back.\n");

        hidScanInput();
        u32 kDown = hidKeysDown(); // For single key presses
        // u32 kHeld = hidKeysHeld(); // For continuous adjustment, not used in this specific implementation

        if (kDown & KEY_B) break; // Exit settings menu

        if (kDown & (KEY_DOWN | KEY_CPAD_DOWN)) {
            selectedSetting = (selectedSetting + 1) % numSettings;
        }
        if (kDown & (KEY_UP | KEY_CPAD_UP)) {
            selectedSetting = (selectedSetting - 1 + numSettings) % numSettings;
        }

        // Handle L/R input for the selected setting
        if (selectedSetting == 0) { // Page Size setting
            // D-Pad Left / C-Pad Left for fine decrement
            if (kDown & (KEY_DLEFT | KEY_CPAD_LEFT)) {
                if (pageSize > minPageSize) {
                    pageSize = max(minPageSize, pageSize - pageSizeStep);
                }
            }
            // L button for large decrement
            else if (kDown & KEY_L) { // Use else if to prioritize D-Pad if both are pressed
                if (pageSize > minPageSize) {
                    pageSize = max(minPageSize, pageSize - 100); // Larger step
                }
            }
            
            // D-Pad Right / C-Pad Right for fine increment
            if (kDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) {
                if (pageSize < maxPageSize) {
                    pageSize = min(maxPageSize, pageSize + pageSizeStep);
                }
            }
            // R button for large increment
            else if (kDown & KEY_R) { // Use else if to prioritize D-Pad if both are pressed
                if (pageSize < maxPageSize) {
                    pageSize = min(maxPageSize, pageSize + 100); // Larger step
                }
            }
        }
        gspWaitForVBlank();
    }
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
    drawList(ePubList, selected); // This already contains consoleClear(), flush, swap
    printf("\nUse D-Pad UP/DOWN. A to select. Start to exit. SELECT for Settings.\n"); // Updated instruction

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_START) break;

        if (kDown & (KEY_DOWN | KEY_CPAD_DOWN)) {
            selected = (selected + 1) % ePubList.size(); // Wrap around
            drawList(ePubList, selected); // drawList calls consoleClear, flush, swap
            printf("\nUse D-Pad UP/DOWN. A to select. Start to exit. SELECT for Settings.\n");
        }
        if (kDown & (KEY_UP | KEY_CPAD_UP)) {
            selected = (selected - 1 + ePubList.size()) % ePubList.size(); // Safe wrap-around
            drawList(ePubList, selected); // drawList calls consoleClear, flush, swap
            printf("\nUse D-Pad UP/DOWN. A to select. Start to exit. SELECT for Settings.\n");
        }
        if (kDown & KEY_A) {
            string path = "sdmc:/ebooks/" + ePubList[selected];
            consoleClear(); // Clear before displaying chapter
            // Add explicit buffer management after clearing the console
            gfxFlushBuffers();
            gfxSwapBuffers();

            // Call the chapter menu function
            displayChapterMenu(path.c_str()); 
            // After returning from chapter menu, redraw the main ePub list
            drawList(ePubList, selected); // drawList calls consoleClear, flush, swap
            printf("\nUse D-Pad UP/DOWN. A to select. Start to exit. SELECT for Settings.\n");

        }
        // Call the settings page when SELECT is pressed
        if (kDown & KEY_SELECT) {
            consoleClear(); // Clear the main menu before entering settings
            // Add explicit buffer management after clearing the console
            gfxFlushBuffers();
            gfxSwapBuffers();

            displaySettingsMenu();
            // After returning from settings, redraw the main ePub list
            drawList(ePubList, selected); // drawList calls consoleClear, flush, swap
            printf("\nUse D-Pad UP/DOWN. A to select. Start to exit. SELECT for Settings.\n");
            continue; 
        }
    }

    gfxExit();
    return 0;
}