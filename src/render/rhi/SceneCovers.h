// Owns GUI-thread cover decoding, bounded archive leases and immutable atlas publications.
// The visual host supplies selection and projected demand; no window or renderer is borrowed.

#pragma once
#include <QObject>
#include <QImage>
#include <QJsonObject>
#include <QRect>
#include <memory>

namespace yalahoo {
class SelectionController;
struct SceneFrame;
struct ArchiveInstance;
struct CoverSource;
struct CoverPixels;
}

namespace yalahoo::rhi {
struct Input;
struct AtlasLease;
class SceneCovers final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SceneCovers)
public:
    explicit SceneCovers(QObject *parent = nullptr);
    ~SceneCovers() override;
    void setSelection(SelectionController *selection);
    // Update immediately on scene publication, before queued decoder completions.
    void observeFrame(const SceneFrame &frame);
    void setDemand(double pixels);
    void setSuspended(bool suspended);
    void prepare(Input &input, const Input &previous);
    void applyImage(Input &input) const;
    bool prepared() const;
    bool poolMode() const;
    quint64 revision() const;
    QImage image() const;
    QJsonObject statistics() const;
    QJsonObject cacheStatistics() const;
Q_SIGNALS:
    void changed();

private:
    struct Private;
    std::unique_ptr<Private> d;
};
}
