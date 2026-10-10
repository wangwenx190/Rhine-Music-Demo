#pragma once
#include "LibrarySession.h"
#include <QtQml/qqmlregistration.h>

struct LibrarySessionQml {
    Q_GADGET
    QML_FOREIGN(yalahoo::LibrarySession)
    QML_NAMED_ELEMENT(LibrarySession)
    QML_UNCREATABLE("Injected by the native application")
};
struct AlbumListModelQml {
    Q_GADGET
    QML_FOREIGN(yalahoo::AlbumListModel)
    QML_NAMED_ELEMENT(AlbumListModel)
    QML_UNCREATABLE("Owned by LibrarySession")
};
struct SearchListModelQml {
    Q_GADGET
    QML_FOREIGN(yalahoo::SearchListModel)
    QML_NAMED_ELEMENT(SearchListModel)
    QML_UNCREATABLE("Owned by LibrarySession")
};
