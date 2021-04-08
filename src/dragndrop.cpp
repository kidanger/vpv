#include <vector>
#include <string>
#include <memory>
#include <utility>

#include "layout.hpp"
#include "Window.hpp"
#include "Sequence.hpp"
#include "config.hpp"
#include "globals.hpp"
#include "ImageCollection.hpp"
#include "collection_expression.hpp"
#include "dragndrop.hpp"

static std::vector<std::string> dropping;

void handleDragDropEvent(const std::string& str, bool isfile)
{
    if (str.empty()) {  // last event of the serie
        if (dropping.size() == 0) return;
        auto colormap = !gColormaps.empty() ? gColormaps.back() : newColormap();
        auto player = !gPlayers.empty() ? gPlayers.back() : newPlayer();
        auto view = !gViews.empty() ? gViews.back() : newView();
        auto seq = newSequence(std::move(colormap), std::move(player), std::move(view));

        std::string files;
        for (const auto& s : dropping) {
            files += s + SEQUENCE_SEPARATOR;
        }
        *(files.end()-strlen(SEQUENCE_SEPARATOR)) = 0;
        auto filenames = buildFilenamesFromExpression(files);
        auto col = buildImageCollectionFromFilenames(filenames);
        seq->setImageCollection(col, "<from drag & drop>");

        std::shared_ptr<Window> win;
        if (gWindows.empty()) {
            win = newWindow();
            gShowHelp = false;
        } else if (gWindows[0]->sequences.empty()) {
            win = gWindows[0];
        } else {
            win = newWindow();
        }
        win->sequences.push_back(std::move(seq));
        relayout();
        dropping.clear();
    } else {
        dropping.push_back(str);
    }
}
